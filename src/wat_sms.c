/*
 * libwat: Wireless AT commands library
 *
 * David Yat Sin <dyatsin@sangoma.com>
 * Copyright (C) 2011, Sangoma Technologies.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contributors:
 *
 */

#include <iconv.h>
#include <wchar.h>
#include <errno.h>

#include "libwat.h"
#include "wat_internal.h"
#include "wat_sms_pdu.h"
#include "base64/base64.h"


static uint8_t bit(uint8_t byte, uint8_t bitpos);
static uint8_t hexstr_to_val(char *string);
static uint8_t decstr_to_val(char *string);

static int wat_decode_sms_pdu_dcs_data(wat_sms_pdu_dcs_t *dcs, uint8_t dcs_val);
static int wat_decode_sms_pdu_sender(wat_number_t *sender, char *data);
static int wat_decode_sms_pdu_smsc(wat_number_t *smsc, char *data);
static int wat_decode_sms_pdu_scts(wat_timestamp_t *ts, char *data);
static int wat_decode_sms_pdu_message_ascii(char *message, wat_size_t max_len, char *data, wat_size_t len, uint8_t padding);
static int wat_decode_sms_text_scts(wat_timestamp_t *ts, char *string);

wat_status_t wat_sms_encode_pdu(wat_span_t *span, wat_sms_t *sms);

wat_status_t wat_decode_sms_content(char *raw_data, wat_size_t *raw_data_len, wat_sms_content_t *content);
wat_status_t wat_decode_sms_content_encoding_base64(char *raw_content, wat_size_t *raw_content_len, wat_sms_content_t *content);

wat_status_t wat_decode_encoding_base64(char *raw_content, wat_size_t *raw_content_len, const char *data, wat_size_t data_len);

static int octet_to_septet(int octet)
{
	return ((octet * 8) / 7) + (((octet * 8) % 7) ? 1 : 0);
}

static int septet_to_octet(int septet)
{
	return (septet * 7) / 8 + (((septet * 7) % 8) ? 1 : 0);
}

wat_status_t wat_span_sms_create(wat_span_t *span, wat_sms_t **insms, uint8_t sms_id, wat_direction_t dir)
{
	wat_sms_t *sms = NULL;
	sms = wat_calloc(1, sizeof(*sms));
	wat_assert_return(sms, WAT_FAIL, "Could not allocate memory for new sms\n");

	if (g_debug & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "[id:%d]Created new sms p:%p\n", sms_id, sms);
	}

	sms->span = span;
	sms->id = sms_id;
	sms->dir = dir;

	*insms = sms;

	return WAT_SUCCESS;
}

void wat_span_sms_destroy(wat_sms_t **insms)
{
	wat_sms_t *sms;
	wat_span_t *span;
	wat_assert_return_void(insms, "Sms was null");
	wat_assert_return_void(*insms, "Sms was null");
	wat_assert_return_void((*insms)->span, "Sms had no span");
	
	sms = *insms;
	*insms = NULL;
	span = sms->span;	

	if (g_debug & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Destroyed sms with id:%d p:%p\n", sms->id, sms);
	}

	wat_safe_free(sms);
	return;
}

wat_status_t _wat_sms_set_state(const char *func, int line, wat_sms_t *sms, wat_sms_state_t new_state)
{
	wat_span_t *span = sms->span;

	/* TODO: Implement state table for allowable state changes */
	if (g_debug & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "[sms:%d] SMS State change from %s to %s (%s:%d)\n", sms->id, wat_sms_state2str(sms->state), wat_sms_state2str(new_state), func, line);
	}

	sms->state = new_state;

	switch(sms->state) {
		case WAT_SMS_STATE_QUEUED:
			if (span->sigstatus != WAT_SIGSTATUS_UP) {
				wat_log_span(span, WAT_LOG_WARNING, "[sms:%d] Cannot send SMS when network is down\n", sms->id);

				sms->cause = WAT_SMS_CAUSE_NO_NETWORK;
				wat_sms_set_state(sms, WAT_SMS_STATE_COMPLETE);
				break;
			}

			if (sms->sms_event.type == WAT_SMS_PDU) {
				wat_log(WAT_LOG_DEBUG, "Sending SMS in PDU mode\n");

				wat_sms_encode_pdu(span, sms);
			} else {
				wat_log(WAT_LOG_DEBUG, "Sending SMS in TXT mode\n");

				memcpy(&sms->body, sms->sms_event.content.data, sizeof(sms->sms_event.content.data));
				sms->body_len = sms->sms_event.content.len;
			}

			if (wat_queue_enqueue(span->sms_queue, sms) != WAT_SUCCESS) {
				wat_log_span(span, WAT_LOG_WARNING, "[sms:%d] SMS queue full\n", sms->id);
				sms->cause = WAT_SMS_CAUSE_QUEUE_FULL;
				wat_sms_set_state(sms, WAT_SMS_STATE_COMPLETE);
			}
			break;
		case WAT_SMS_STATE_START:
			span->outbound_sms = sms;
			if (sms->sms_event.type == WAT_SMS_TXT) {
				/* We need to adjust the sms mode */
				wat_cmd_enqueue(span, "AT+CMGF=1", wat_response_cmgf, sms);
			} else {
				wat_sms_set_state(sms, WAT_SMS_STATE_SEND_HEADER);
			}
			break;
		case WAT_SMS_STATE_SEND_HEADER:
			{
				char cmd[40];
				memset(cmd, 0, sizeof(cmd));
				if (sms->sms_event.type == WAT_SMS_PDU) {
					sprintf(cmd, "AT+CMGS=%zd", sms->pdu_len);
				} else {
					/* TODO set the TON/NPI as well */
					sprintf(cmd, "AT+CMGS=\"%s\"", sms->sms_event.to.digits);
				}
				wat_cmd_enqueue(span, cmd, NULL, NULL);
			}
			break;
		case WAT_SMS_STATE_SEND_BODY:
			wat_sms_send_body(sms);
			break;
		case WAT_SMS_STATE_SEND_TERMINATOR:
			{
				char cmd[4];
				sprintf(cmd, "%c\r\n", 0x1a);
				wat_cmd_send(span, cmd, wat_response_cmgs_end, sms);
			}
			break;
		case WAT_SMS_STATE_COMPLETE:
			{
				wat_sms_status_t sms_status;

				if (sms->sms_event.type == WAT_SMS_TXT) {
					/* Switch the GSM module back to PDU mode */
					wat_cmd_enqueue(span, "AT+CMGF=0", NULL, NULL);
				}
				
				memset(&sms_status, 0, sizeof(sms_status));
				if (!sms->cause) {
					sms_status.success = WAT_TRUE;
				} else {
					sms_status.success = WAT_FALSE;
					sms_status.cause = sms->cause;
					sms_status.error = sms->error;
				}
				
				if (g_interface.wat_sms_sts) {
					g_interface.wat_sms_sts(span->id, sms->id, &sms_status);
				}
				wat_span_sms_destroy(&sms);
			}
			break;
		default:
			wat_log(WAT_LOG_CRIT, "Unhandled state change\n");
			break;
	}
	
	return WAT_SUCCESS;
}

wat_status_t wat_sms_send_body(wat_sms_t *sms)
{	
	int len;
	int len_wrote;	
	char command[WAT_MAX_CMD_SZ];
	wat_span_t *span = sms->span;

	span->sms_write = 1;

	while (sms->wrote < sms->body_len) {
		memset(command, 0, sizeof(command));
		len = sms->body_len - sms->wrote;
		if (len <= 0) {
			break;
		}

		if (len > sizeof(command)) {
			len = sizeof(command);
		}
		
		memcpy(command, &sms->body[sms->wrote], len);
		len_wrote = wat_span_write(span, command, len);

		sms->wrote += len_wrote;

		if (len_wrote <= 0) {
			/* Some lower level queue is full  */
			wat_log_span(span, WAT_LOG_ERROR, "Failed to write AT command, sms send fail\n");
		}
	}
	span->sms_write = 0;
	wat_sms_set_state(sms, WAT_SMS_STATE_SEND_TERMINATOR);
	return WAT_SUCCESS;
}

static int wat_decode_sms_text_scts(wat_timestamp_t *ts, char *string)
{
	char *sctstokens[3];

	/* format: 11/11/23,14:42:17+00 */
	
	if (wat_cmd_entry_tokenize(wat_string_clean(string), sctstokens, wat_array_len(sctstokens)) < 2) {
		wat_log(WAT_LOG_ERROR, "Failed to parse SCTS [%s]\n", string);
	} else {
		if (sscanf(sctstokens[0], "%d/%d/%d", &ts->year, &ts->month ,&ts->day) == 3) {
			if (g_debug & WAT_DEBUG_SMS_DECODE) {
				wat_log(WAT_LOG_DEBUG, "SMS-SCTS: year:%d month:%d day:%d\n", ts->year, ts->month ,ts->day);
			}
		} else {
			wat_log(WAT_LOG_ERROR, "Failed to parse date from SCTS [%s]\n", sctstokens[0]);
		}
		if (sscanf(sctstokens[1], "%d:%d:%d+%d", &ts->hour, &ts->minute ,&ts->second, &ts->timezone) == 4) {
			if (g_debug & WAT_DEBUG_SMS_DECODE) {
				wat_log(WAT_LOG_DEBUG, "SMS-SCTS: hour:%d minute:%d second:%d tz:%d\n", ts->hour, ts->minute ,ts->second, ts->timezone);
			}
		} else {
			wat_log(WAT_LOG_ERROR, "Failed to parse time from SCTS [%s]\n", sctstokens[1]);
		}
	}

	wat_free_tokens(sctstokens);

	return 0;
}

wat_status_t wat_handle_incoming_sms_text(wat_span_t *span, char *from, char *scts, char *content)
{
	wat_sms_event_t sms_event;
	
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding Text Message From:[%s] SCTS:[%s] message:[%s]\n", from, scts, content);
	}
	
	memset(&sms_event, 0, sizeof(sms_event));

	wat_decode_sms_text_scts(&sms_event.scts, scts);
	strncpy(sms_event.content.data, content, sizeof(sms_event.content.data));

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "SMS Content:%s\n", sms_event.content.data);
	}	

	if (g_interface.wat_sms_ind) {
		g_interface.wat_sms_ind(span->id, &sms_event);
	}

	return WAT_SUCCESS;
}

static int wat_decode_sms_pdu_deliver(wat_sms_pdu_deliver_t *deliver, char *data)
{
	uint8_t octet;

	wat_log(WAT_LOG_DEBUG, "Decoding SMS-DELIVER [%s]\n", data);
	octet = hexstr_to_val(data);
	
	deliver->tp_mti = octet & 0x03;
	deliver->tp_mms = (octet > 2) & 0x01;
	deliver->tp_sri = (octet > 4) & 0x01;
	deliver->tp_udhi = (octet > 5) & 0x01;
	deliver->tp_rp = (octet > 6) & 0x01;

	wat_log(WAT_LOG_DEBUG, "  TP-RP:%d TP-UDHI:%d TP-SRI:%d TP-MMS:%d TP-MTI:%d\n",
								deliver->tp_rp, deliver->tp_udhi, deliver->tp_sri, deliver->tp_mms, deliver->tp_mti);

	/* SMS-DELIVER always takes 2 chars */
	return 2;
}

static int wat_decode_sms_pdu_scts(wat_timestamp_t *ts, char *data)
{
	int i;
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding Time Stamp information [%s]\n", data);
	}

	for (i = 0; i <= 6; i++) {
		uint8_t val = decstr_to_val(&data[i*2]);
		uint8_t true_val = (val / 10) + ((val % 10) * 10);
		switch(i) {
			case 0:
				ts->year = true_val;
				break;
			case 1:
				ts->month = true_val;
				break;
			case 2:
				ts->day = true_val;
				break;
			case 3:
				ts->hour = true_val;
				break;
			case 4:
				ts->minute = true_val;
				break;
			case 5:
				ts->second = true_val;
				break;
			case 6:
				ts->timezone = true_val;
				break;
		}
	}

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  Year:%d Month:%d Day:%d Hr:%d Min:%d Sec:%d Timezone:%d\n",
																	ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second, ts->timezone);
	}
	
	/* TP-SCTS always takes 14 bytes */
	return 14;
}

static int wat_decode_sms_pdu_dcs_data(wat_sms_pdu_dcs_t *dcs, uint8_t dcs_val)
{
	/* Based on Section 4 of GSM 03.38 */
	uint8_t dcs_grp = dcs_val >> 4;

	dcs->grp = WAT_SMS_PDU_DCS_GRP_INVALID;
	dcs->msg_class = WAT_SMS_PDU_DCS_MSG_CLASS_INVALID;
	dcs->ind_type = WAT_SMS_PDU_DCS_IND_TYPE_INVALID;
	dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_INVALID;
	
	if (!(dcs_grp & 0xFF00)) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_GEN;
	} else if (dcs_grp >= 0x4 && dcs_grp <= 0xB) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_RESERVED;
	} else if (dcs_grp == 0xC) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_MWI_DISCARD_MSG;
	} else if (dcs_grp == 0xD) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_1;
	} else if (dcs_grp == 0xE) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_2;
	} else if (dcs_grp == 0xF) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_DATA_CODING;
	} else {
		wat_log(WAT_LOG_ERROR, "Invalid SMS PDU coding group (val:%x)\n", dcs_grp);
	}

	if (!dcs_val) {
		/* Special case */
		dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_DEFAULT;
		return 0;
	}

	switch (dcs->grp) {
		case WAT_SMS_PDU_DCS_GRP_GEN:
			dcs->compressed = bit(dcs_val, 5);

			if (bit(dcs_val, 4)) {
				dcs->msg_class = dcs_val & 0x03;
			} else {
				dcs->msg_class = WAT_SMS_PDU_DCS_MSG_CLASS_INVALID;
			}

			dcs->alphabet = (dcs_val >> 2) & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_MWI_DISCARD_MSG:
		case WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_1:
		case WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_2:
			dcs->ind_active = (bit(dcs_val, 3));
			dcs->ind_type = dcs_val & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_DATA_CODING:
			if (bit(dcs_val, 2)) {
				dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_DEFAULT;
			} else {
				dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_8BIT;
			}

			dcs->msg_class = dcs_val & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_RESERVED:
			/* Custom coding grp - cannot decode */
		case WAT_SMS_PDU_DCS_GRP_INVALID:
			break;
	}
	return 0;
}


static uint8_t hexstr_to_val(char *string)
{
	char octet_string[3];
	int octet;

	memset(octet_string, 0, sizeof(octet_string));
	memcpy(octet_string, string, 2);
	sscanf(octet_string, "%x", &octet);
	return (uint8_t )octet;
}

static uint8_t decstr_to_val(char *string)
{
	char octet_string[3];
	int octet;

	memset(octet_string, 0, sizeof(octet_string));
	memcpy(octet_string, string, 2);
	sscanf(octet_string, "%d", &octet);
	return (uint8_t )octet;
}

static uint8_t bit(uint8_t byte, uint8_t bitpos)
{
	return (byte >> bitpos) & 0x01;
}


static int wat_decode_sms_pdu_message_ascii(char *message, wat_size_t max_len, char *data, wat_size_t len, uint8_t padding)
{
#if 0
	int i;
	int carry = 0;
	uint8_t byte, conv_byte;
	int message_len = 0;	
	
	memset(message, 0, max_len);
		
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding PDU Message (len:%d)[%s] pad:%d\n", len, data, padding);
	}

	i = 0;
	if (padding) {
		uint8_t j = i % 7;
		byte = hexstr_to_val(&data[(i++)*2]);
		conv_byte = hi_bits(byte, 7-j);
		message_len += sprintf(&message[message_len], "%c", conv_byte);
		padding = 1;
	}

	for (; message_len < len; i++) {
		uint8_t j = (i - padding) % 7;
		byte = hexstr_to_val(&data[(i)*2]);
				
		conv_byte = ((lo_bits(byte, (7-j))) << j) | carry;
		carry = hi_bits(byte, j+1);

		message_len += sprintf(&message[message_len], "%c", conv_byte);

		if (j == 6) {
			message_len += sprintf(&message[message_len], "%c", carry);
			carry = 0;
		}
	}
	message[message_len++] = '\0';

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Contents:%s (len:%d)\n", message, message_len);
	}
	return len;
#endif
	return 0;
}

static int wat_decode_sms_pdu_smsc(wat_number_t *smsc, char *data)
{
	/* TODO: move to wat_sms_pdu.c */
#if 0
	/* www.dreamfabric.com/sms/type_of_address.html */
	int len;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding SMSC information [%s]\n", data);
	}

	len = hexstr_to_val(&data[0]);

	wat_decode_type_of_address(hexstr_to_val(&data[2]), &smsc->type, &smsc->plan);

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  len:%d type:%d plan:%d\n", len, smsc->type, smsc->plan);
	}
	
	/* For the smsc, the 'length' parameter specifies how many octets in the sms->number */
	/* Actual length of number is sms_event.pdu.smsc.len-1 because we have to substract 'toa' */
	wat_decode_sms_pdu_semi_octets(smsc->digits, &data[4], len - 1);

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  number:%s\n", smsc->digits);
	}

	/* Need to add 2 for type-of-address */
	return len*2 + 2;
#endif
	return 0;
}

static int wat_decode_sms_pdu_sender(wat_number_t *sender, char *data)
{
	/* TODO: move to wat_sms_pdu.c */
#if 0
	/* www.developershome.com/sms/cmgrCommand3.asp */
	int len;
	int data_len;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding Sender information [%s]\n", data);
	}
	
	len = hexstr_to_val(&data[0]);

	wat_decode_type_of_address(hexstr_to_val(&data[2]), &sender->type, &sender->plan);

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  len:%d type:%d plan:%d\n", len, sender->type, sender->plan);
	}

	/* For the sender, the 'length' parameter specifies how many characters are in the sender->number:
	   2 characters per octet, there is a trailing F is there is an odd number of characters */
	data_len = (len / 2) + (len % 2);

	wat_decode_sms_pdu_semi_octets(sender->digits, (char *)&data[4], data_len);
	
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  number:%s\n", sender->digits);
	}

	/* Need to add 2 for type-of-address + 2 for len [+ 1 if odd numbered size] */
	return 4 + len + (len % 2);
#endif
	return 0;
}


wat_status_t wat_handle_incoming_sms_pdu(wat_span_t *span, char *data, wat_size_t len)
{
	wat_sms_event_t sms_event;
	int ret;
	int i = 0;
	wat_size_t content_len = 0;

	/* From www.dreamfabric.com/sms */
	/* GSM 03.38 */

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Decoding SMS-PDU [%s] len:%d\n", data, len);
	}

	memset(&sms_event, 0, sizeof(sms_event));

	ret = wat_decode_sms_pdu_smsc(&sms_event.pdu.smsc, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMSC from SMS PDU data [%s]\n", &data[i]);
		return WAT_FAIL;
	}
	i += ret;

	ret = wat_decode_sms_pdu_deliver(&sms_event.pdu.sms.deliver, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMS-DELIVER from SMS PDU data [%s]\n", &data[i]);

		return WAT_FAIL;
	}
	i += ret;

	ret = wat_decode_sms_pdu_sender(&sms_event.from, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMS-SENDER from SMS PDU data [%s]\n", &data[i]);

		return WAT_FAIL;
	}
	i += ret;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Decoding TP-PID TP-DCS [%s]\n", &data[i]);
	}

	sms_event.pdu.tp_pid = hexstr_to_val(&data[i]);
	i += 2;
	sms_event.pdu.tp_dcs = hexstr_to_val(&data[i]);
	i += 2;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log_span(span, WAT_LOG_DEBUG, "  TP-PID:%d TP-DCS:%d\n", sms_event.pdu.tp_pid, sms_event.pdu.tp_dcs);
	}

	ret = wat_decode_sms_pdu_dcs_data(&sms_event.pdu.dcs, sms_event.pdu.tp_dcs);
	if (ret) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMS-DCS from SMS PDU data [%s]\n", &data[i]);

		return WAT_FAIL;
	}

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "DCS - Grp:%s Alphabet:%s\n", wat_sms_pdu_dcs_grp2str(sms_event.pdu.dcs.grp), wat_sms_pdu_dcs_alphabet2str(sms_event.pdu.dcs.alphabet));
	}

	ret = wat_decode_sms_pdu_scts(&sms_event.scts, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMS-SCTS from SMS PDU data [%s]\n", &data[i]);
		
		return WAT_FAIL;
	}

	i+= ret;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Decoding TP-UDL [%s]\n", &data[i]);
	}
	sms_event.pdu.tp_udl = hexstr_to_val(&data[i]);
	content_len = sms_event.pdu.tp_udl;
	i += 2;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		/* User data length */
		wat_log_span(span, WAT_LOG_DEBUG, "TP-UDL:%d\n", sms_event.pdu.tp_udl);
	}

	if (sms_event.pdu.sms.deliver.tp_udhi) {
		if (g_debug & WAT_DEBUG_SMS_DECODE) {
			wat_log_span(span, WAT_LOG_DEBUG, "Decoding TP-UDHL [%s]\n", &data[i]);
		}
		sms_event.pdu.udh.tp_udhl = hexstr_to_val(&data[i]); /* User data header length */
		i += 2;

		sms_event.pdu.udh.iei = hexstr_to_val(&data[i]); /* Information Element Identifier */
		i += 2;

		sms_event.pdu.udh.iedl = hexstr_to_val(&data[i]); /* Information Element Identifier Length */
		i += 2;

		sms_event.pdu.udh.refnr = hexstr_to_val(&data[i]); /* Reference Number */
		i += 2;

		sms_event.pdu.udh.total = hexstr_to_val(&data[i]); /* Total Number of parts (number of concatenated sms */
		i += 2;

		sms_event.pdu.udh.seq = hexstr_to_val(&data[i]); /* Sequence */
		i += 2;

		content_len -= 8; /* TODO check if this should be: content_len - sms_event.pdu.tp_udhl */
		if (g_debug & WAT_DEBUG_SMS_DECODE) {
			/* User data length */
			wat_log_span(span, WAT_LOG_DEBUG, "TP-UDHL:%d IEI:%d IEDL:%d Ref nr:%d Total:%d Seq:%d\n",
						 sms_event.pdu.udh.tp_udhl, sms_event.pdu.udh.iei, sms_event.pdu.udh.iedl, sms_event.pdu.udh.refnr, sms_event.pdu.udh.total, sms_event.pdu.udh.seq);
		}
		
	}

	switch (sms_event.pdu.dcs.alphabet) {
		/* See www.dreamfabric.com/sms/dcs.html for different Data Coding Schemes */
		case WAT_SMS_PDU_DCS_ALPHABET_DEFAULT:
			/* Default Aplhabet, phase 2 */
			sms_event.content.len = wat_decode_sms_pdu_message_ascii(sms_event.content.data, sizeof(sms_event.content.data), &data[i], content_len , sms_event.pdu.udh.seq);
			break;
		case WAT_SMS_PDU_DCS_ALPHABET_8BIT:
		case WAT_SMS_PDU_DCS_ALPHABET_UCS2:
			sms_event.content.len = content_len;
			memcpy(sms_event.content.data, &data[i], content_len);
			break;
		default:
			wat_log_span(span, WAT_LOG_ERROR, "Don't know how to decode incoming SMS message with coding scheme:0x%x\n", sms_event.pdu.tp_dcs);
			break;
	}

	if (g_interface.wat_sms_ind) {
		g_interface.wat_sms_ind(span->id, &sms_event);
	}

	return WAT_SUCCESS;
}


wat_status_t wat_sms_encode_pdu(wat_span_t *span, wat_sms_t *sms)
{
	wat_status_t status;
	char pdu_data[1000];
	wat_size_t pdu_data_len;
	char raw_content[WAT_MAX_SMS_SZ*sizeof(wchar_t)];
	wat_size_t raw_content_len;
	wat_size_t pdu_header_len;	
	wat_size_t udh_len;
	wat_sms_event_t *sms_event;
	char *pdu_data_ptr;
	char *tp_udh_loc;
	unsigned i;	

	sms_event = &sms->sms_event;
	pdu_data_ptr = pdu_data;
	pdu_data_len = 0;
	udh_len = 0;

	/* www.dreamfabric.com/sms/ */

	if (!strlen(sms_event->pdu.smsc.digits)) {
		if (strlen(span->sim_info.smsc.digits)) {
			wat_log_span(span, WAT_LOG_DEBUG, "SMSC not specified, using %s\n", span->sim_info.smsc.digits);
			memcpy(&sms_event->pdu.smsc, &span->sim_info.smsc, sizeof(span->sim_info.smsc));
		} else {
			wat_log_span(span, WAT_LOG_ERROR, "SMSC information not available\n");
			WAT_FUNC_DBG_END
			return WAT_FAIL;
		}
	}

	status = wat_encode_sms_pdu_smsc(&sms_event->pdu.smsc, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);
	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode SMS-SMSC information\n");
		return status;
	}

	pdu_header_len = pdu_data_len;

	/* We need to include User Data Header if we want to send concatenated messages */
	if (!sms_event->pdu.sms.submit.tp_udhi && sms_event->pdu.udh.total > 1) {
		wat_log_span(span, WAT_LOG_DEBUG, "Including User Data Header due to contatenated messages\n");
		sms_event->pdu.sms.submit.tp_udhi = 1;
	}

	status = wat_encode_sms_pdu_submit(&sms_event->pdu.sms.submit, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);
	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode SMS-SUBMIT information\n");
		return status;
	}

	status = wat_encode_sms_pdu_message_ref(sms_event->pdu.tp_message_ref, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);

	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode SMS-Message Ref information\n", sizeof(pdu_data) - pdu_data_len);
		return status;
	}

	status = wat_encode_sms_pdu_to(&sms_event->to, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);
	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode SMS-Destination information\n", sizeof(pdu_data) - pdu_data_len);
		return status;
	}

	status = wat_encode_sms_pdu_pid(sms_event->pdu.tp_pid, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);
	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode SMS Protocol Identifier\n");
		return status;
	}

	/* Decode sms content before encoding the DCS so we can tell whether the contents would fit within default GSM alphabet */
	status = wat_decode_sms_content(raw_content, &raw_content_len, &sms_event->content);
	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to decode SMS content encoding\n");
		return status;
	}

	/* If we cannot convert contents into Default alphabet, we need to switch to UCS2 */
	if (sms_event->content.charset == WAT_SMS_CONTENT_CHARSET_UTF8 &&
		wat_verify_default_alphabet(raw_content) != WAT_SUCCESS) {

		wat_log_span(span, WAT_LOG_DEBUG, "Switching to UCS2 alphabet\n");
		sms_event->pdu.dcs.alphabet = WAT_SMS_PDU_DCS_ALPHABET_UCS2;
	}
	
	status = wat_encode_sms_pdu_dcs(&sms_event->pdu.dcs, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);
	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode SMS Data Coding Scheme\n");
		return status;
	}	

	status = wat_encode_sms_pdu_vp(&sms_event->pdu.sms.submit.vp, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);
	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode SMS Validity Period\n");
		return status;
	}

	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		print_buffer(WAT_LOG_DEBUG, pdu_data, pdu_data_len, "SMS PDU Header");
	}

	/* This is the location where we will store the user data header length */
	tp_udh_loc = pdu_data_ptr;

	if (sms_event->pdu.sms.submit.tp_udhi) {
		wat_size_t post_udl_data_len;
		
		pdu_data_ptr++;
		pdu_data_len++;
		post_udl_data_len = pdu_data_len;

		status = wat_encode_sms_pdu_udh(&sms_event->pdu.udh, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);

		if (status != WAT_SUCCESS) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to encode User Data Header\n");
			return status;
		}

		udh_len = pdu_data_len - post_udl_data_len;
	}

	switch (sms_event->pdu.dcs.alphabet) {
		case WAT_SMS_PDU_DCS_ALPHABET_DEFAULT:
			{
				char *tp_udh_loc_ptr = tp_udh_loc + 1;
				wat_size_t content_len = udh_len;

				/* Convert the length into septets */
				status = wat_encode_sms_pdu_message_7bit((wchar_t *)raw_content, raw_content_len, &tp_udh_loc_ptr, &content_len, sizeof(pdu_data) - pdu_data_len - udh_len, octet_to_septet(udh_len));

				*tp_udh_loc = octet_to_septet(udh_len) + content_len;

				pdu_data_len += septet_to_octet(content_len + octet_to_septet(udh_len)) - udh_len;
				if (!sms_event->pdu.sms.submit.tp_udhi) {
					pdu_data_len++;
				}
			}
			break;
		case WAT_SMS_PDU_DCS_ALPHABET_UCS2:
			status = wat_encode_sms_pdu_message_ucs2(raw_content, raw_content_len, &pdu_data_ptr, &pdu_data_len, sizeof(pdu_data) - pdu_data_len);
			break;
		default:
			wat_log_span(span, WAT_LOG_ERROR, "Unsupported alphabet (%d)\n", sms_event->pdu.dcs.alphabet);
			status = WAT_FAIL;
			break;
	}

	if (status != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to encode message contents into pdu\n");
		return WAT_FAIL;
	}

	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		print_buffer(WAT_LOG_DEBUG, pdu_data, pdu_data_len, "SMS PDU Before string encoding");
	}

	sms->pdu_len = pdu_data_len - pdu_header_len;

	/*  Convert into string representation */
	for (i = 0; i < pdu_data_len; i++) {
		sprintf((char *)&sms->body[i*2], "%02x", (0xFF) & pdu_data[i]);
	}
	sms->body_len = pdu_data_len*2;

	return WAT_SUCCESS;
}

wat_status_t wat_decode_encoding_base64(char *raw_content, wat_size_t *raw_content_len, const char *data, wat_size_t data_len)
{	
	if (!base64_decode(data, data_len, raw_content, raw_content_len)) {
		wat_log(WAT_LOG_ERROR, "Failed to perform base64 decoding\n");
		return WAT_FAIL;
	}
	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_content(char *raw_data, wat_size_t *raw_data_len, wat_sms_content_t *content)
{
	char *data;
	char *data_ptr;
	wat_size_t data_len;	
	wat_size_t data_avail;
	wat_size_t data_left;
	iconv_t cd;
	wat_status_t status = WAT_SUCCESS;

	switch (content->encoding) {
		case WAT_SMS_CONTENT_ENCODING_RAW:
			/* No encoding used */
			data = content->data;
			data_len = content->len;
			break;
		case WAT_SMS_CONTENT_ENCODING_BASE64:
			data = wat_malloc(content->len);
			data_len = content->len;	
			memset(data, 0, content->len);

			wat_decode_encoding_base64(data, &data_len, content->data, content->len);
			break;
		case WAT_SMS_CONTENT_ENCODING_HEX:
			wat_log(WAT_LOG_ERROR, "Hex content encoding not supported yet!!\n");
			break;
		default:
			wat_log(WAT_LOG_ERROR, "Unsupported content encoding (%d)\n", content->encoding);
			status = WAT_FAIL;
			goto done;
	}

	data_ptr = data;
	data_avail = (WAT_MAX_SMS_SZ + 1) * sizeof(wchar_t);
	data_left = data_avail;

	switch (content->charset) {
		case WAT_SMS_CONTENT_CHARSET_ASCII:
			cd = iconv_open("WCHAR_T", "ASCII");
			break;
		case WAT_SMS_CONTENT_CHARSET_UTF8:
			cd = iconv_open("WCHAR_T", "UTF-8");
			break;
		default:
			wat_log(WAT_LOG_ERROR, "Unsupported content charset:%d\n", content->charset);
			status = WAT_FAIL;
			goto done;
	}

	if (cd < 0) {
		wat_log(WAT_LOG_CRIT, "Failed to create new converter\n");
		status = WAT_FAIL;
		goto done;
	}

	if (iconv(cd, &data_ptr, &data_len, &raw_data, &data_left) < 0) {
		wat_log(WAT_LOG_ERROR, "Failed to perform character conversion (charset:%d)\n", content->charset);
		return WAT_FAIL;
	}
	
	*((wchar_t *)raw_data) = L'\0';

	*raw_data_len = (data_avail - data_left);

done:
	if (cd >= 0) {
		iconv_close(cd);
	}

	if (content->encoding == WAT_SMS_CONTENT_ENCODING_BASE64) {
		wat_safe_free(data);
	}
	return status;
}


