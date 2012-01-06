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

#include "libwat.h"
#include "wat_internal.h"

static uint8_t bit(uint8_t byte, uint8_t bitpos);
static uint8_t hexstr_to_val(char *string);
static uint8_t decstr_to_val(char *string);
static int wat_decode_sms_pdu_dcs_data(wat_sms_pdu_dcs_t *dcs, uint8_t dcs_val);
static int wat_decode_sms_pdu_sender(wat_number_t *sender, char *data);
static int wat_decode_sms_pdu_smsc(wat_number_t *smsc, char *data);
static int wat_decode_sms_pdu_scts(wat_timestamp_t *ts, char *data);
static int wat_decode_sms_pdu_message_7_bit(char *message, wat_size_t max_len, char *data, wat_size_t len, uint8_t padding);
static int wat_encode_sms_pdu_message_7_bit(uint8_t *message, wat_size_t max_len, char *data, wat_size_t len, uint8_t padding);

static int wat_decode_sms_text_scts(wat_timestamp_t *ts, char *string);

static int wat_sms_encode_pdu(wat_span_t *span, wat_sms_t *sms);

static int wat_encode_sms_pdu_semi_octets(char *data, char *string, wat_size_t len);
static int wat_decode_sms_pdu_semi_octets(char *string, char *data, wat_size_t len);


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
				wat_log_span(span, WAT_LOG_DEBUG, "[sms:%d] Cannot send SMS when network is down\n", sms->id);

				sms->cause = WAT_SMS_CAUSE_NO_NETWORK;
				wat_sms_set_state(sms, WAT_SMS_STATE_COMPLETE);
				break;
			}

			if (sms->sms_event.type == WAT_SMS_PDU) {
				wat_log(WAT_LOG_DEBUG, "Sending SMS in PDU mode\n");

				wat_sms_encode_pdu(span, sms);
			} else {
				wat_log(WAT_LOG_DEBUG, "Sending SMS in TXT mode\n");

				memcpy(&sms->body, sms->sms_event.content, sizeof(sms->sms_event.content));
				sms->body_len = sms->sms_event.content_len;
			}

			if (wat_queue_enqueue(span->sms_queue, sms) != WAT_SUCCESS) {
				wat_log_span(span, WAT_LOG_DEBUG, "[sms:%d] SMS queue full\n", sms->id);
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
					sprintf(cmd, "AT+CMGS=%d", sms->pdu_len);
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
				wat_cmd_enqueue(span, cmd, wat_response_cmgs_end, sms);
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
		/* We need to add an extra 2 bytes because dahdi expects a CRC */
		//len_wrote = wat_span_write(span, command, len + 2) - 2;
		len_wrote = wat_span_write(span, command, len);

		sms->wrote += len_wrote;

		if (len_wrote <= 0) {
			/* Some lower level queue is full, return, and we will try to transmit again when we get called again */
			return WAT_BREAK;
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
	
	memset(sctstokens, 0, sizeof(sctstokens));

	if (wat_cmd_entry_tokenize(wat_string_clean(string), sctstokens) < 2) {
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
	strncpy(sms_event.content, content, sizeof(sms_event.content));

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "SMS Content:%s\n", sms_event.content);
	}	

	if (g_interface.wat_sms_ind) {
		g_interface.wat_sms_ind(span->id, &sms_event);
	}

	return WAT_SUCCESS;
}

static int wat_decode_sms_pdu_semi_octets(char *string, char *data, wat_size_t len)
{
	int i;
	int ret = 0;
	char *p = string;

	for (i = 0; i < (len*2); i+=2) {
		*p++ = data[i+1];
		ret++;
		if (data[i] != 'F') {
			*p++ = data[i];
			ret++;
		}
	}
	return ret;
}

static int wat_encode_sms_pdu_semi_octets(char *data, char *string, wat_size_t len)
{	
	int i;
	int ret = 0;
	char *p = data;
	uint8_t odd = len % 2;
	
	for (i = 0; i < (len - odd); i+=2) {
		char lo_nibble[2];
		char hi_nibble[2];

		lo_nibble[0] = string[i];
		lo_nibble[1] = '\0';

		hi_nibble[0] = string[i+1];
		hi_nibble[1] = '\0';
		
		*p++ = (atoi(hi_nibble) << 4)|(atoi(lo_nibble));
		ret++;
	}
	
	if (odd) {
		*p++ = 0xF0 | string[i];
		ret++;
	}

	return ret;
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
	dcs->charset = WAT_SMS_PDU_DCS_CHARSET_INVALID;
	
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
		dcs->charset = WAT_SMS_PDU_DCS_CHARSET_7BIT;
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

			dcs->charset = (dcs_val >> 2) & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_MWI_DISCARD_MSG:
		case WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_1:
		case WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_2:
			dcs->ind_active = (bit(dcs_val, 3));
			dcs->ind_type = dcs_val & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_DATA_CODING:
			if (bit(dcs_val, 2)) {
				dcs->charset = WAT_SMS_PDU_DCS_CHARSET_7BIT;
			} else {
				dcs->charset = WAT_SMS_PDU_DCS_CHARSET_8BIT;
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

static uint8_t lo_bits(uint8_t byte, uint8_t numbits)
{
	switch(numbits) {
		case 0:
			return byte & 0x00;
		case 1:
			return byte & 0x01;
		case 2:
			return byte & 0x03;
		case 3:
			return byte & 0x07;
		case 4:
			return byte & 0x0F;
		case 5:
			return byte & 0x1F;
		case 6:
			return byte & 0x3F;
		case 7:
			return byte & 0x7F;
	}
	return 0;
}

static uint8_t hi_bits(uint8_t byte, uint8_t numbits)
{
	return byte >> (8 - numbits);
}

static int wat_encode_sms_pdu_submit(wat_span_t *span, char *data, wat_sms_pdu_submit_t *submit)
{
	uint8_t octet;

	octet = submit->tp_rp << 7;
	octet |= (submit->tp_udhi & 0x1) << 6;
	octet |= (submit->tp_srr & 0x1) << 5;
	octet |= (submit->tp_vpf & 0x3) << 3;
	octet |= (submit->tp_rd & 0x1) << 1;
	octet |= 0x01; /* mti = SMS-SUBMIT */


	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log_span(span, WAT_LOG_DEBUG, "SMS-SUBMIT:0x%02x\n", octet);
	}

	*data = octet;
	return 1;
}

static int wat_encode_sms_pdu_dcs(wat_span_t *span, char *data, wat_sms_pdu_dcs_t *dcs)
{
	uint8_t octet = 0;

	octet |= (dcs->compressed & 0x01) << 4;

	/* Bit 4 - Message class present or not */
	if (dcs->msg_class != WAT_SMS_PDU_DCS_MSG_CLASS_INVALID) {
		octet |= 0x10;
	}

	/* Bits 3 & 2 - Alphabet */
	octet |= (dcs->charset & 0x03) << 2;

	/* Bits 0 & 1 - Message class */
	octet |= (dcs->msg_class & 0x03);
	
	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log_span(span, WAT_LOG_DEBUG,  "TP-DCS:0x%02x\n", octet);
	}
		
	*data = octet;
	return 1;
}

static int wat_encode_sms_pdu_message_7_bit(uint8_t *message, wat_size_t max_len, char *indata, wat_size_t len, uint8_t padding)
{
	uint8_t carry;
	uint8_t byte;
	uint8_t next_byte;
	int i;
	uint8_t *p = &message[1];
	char *data = NULL;
	int message_len = 0;

	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG, "Encoding PDU Message (len:%d)[%s] pad:%d\n", len, data, padding);
	}
	data = wat_malloc(max_len);
	wat_assert_return(data, 0, "Failed to malloc");
	memcpy(data, indata, max_len);

	for (i = 0; i < len; i++) {
		uint8_t j = i % 8;
		if (j != 7) {
			carry = lo_bits(data[i+1], (j+1));

			next_byte = hi_bits(data[i+1], (7-j));
			data[i+1] = next_byte;

			byte = lo_bits(data[i], (7-j)) | carry << (7-j);

			*p = byte;
			p++;
			message_len++;
		}
	}
	*p = '\0';
	message[0] = len;

	wat_safe_free(data);

	/* Return the length in septets */
	return (len * 7) / 8 + (((len * 7) % 8) ? 1 : 0) + 1; 
}

static int wat_decode_sms_pdu_message_7_bit(char *message, wat_size_t max_len, char *data, wat_size_t len, uint8_t padding)
{
	int i;
	int carry = 0;
	uint8_t byte, conv_byte;
	int message_len = 0;

	memset(message, 0, 500);
		
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
}

static int wat_decode_sms_pdu_smsc(wat_number_t *smsc, char *data)
{
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
}

static int wat_decode_sms_pdu_sender(wat_number_t *sender, char *data)
{
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
		wat_log(WAT_LOG_DEBUG, "DCS - Grp:%s Alphabet:%s\n", wat_sms_pdu_dcs_grp2str(sms_event.pdu.dcs.grp), wat_sms_pdu_dcs_charset2str(sms_event.pdu.dcs.charset));
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
		sms_event.pdu.tp_udhl = hexstr_to_val(&data[i]); /* User data header length */
		i += 2;

		sms_event.pdu.iei = hexstr_to_val(&data[i]); /* Information Element Identifier */
		i += 2;

		sms_event.pdu.iedl = hexstr_to_val(&data[i]); /* Information Element Identifier Length */
		i += 2;

		sms_event.pdu.refnr = hexstr_to_val(&data[i]); /* Reference Number */
		i += 2;

		sms_event.pdu.total = hexstr_to_val(&data[i]); /* Total Number of parts (number of concatenated sms */
		i += 2;

		sms_event.pdu.seq = hexstr_to_val(&data[i]); /* Sequence */
		i += 2;

		content_len -= 8; /* TODO check if this should be: content_len - sms_event.pdu.tp_udhl */
		if (g_debug & WAT_DEBUG_SMS_DECODE) {
			/* User data length */
			wat_log_span(span, WAT_LOG_DEBUG, "TP-UDHL:%d IEI:%d IEDL:%d Ref nr:%d Total:%d Seq:%d\n",
					sms_event.pdu.tp_udhl, sms_event.pdu.iei, sms_event.pdu.iedl, sms_event.pdu.refnr, sms_event.pdu.total, sms_event.pdu.seq);
		}
		
	}

	switch (sms_event.pdu.dcs.charset) {
		/* See www.dreamfabric.com/sms/dcs.html for different Data Coding Schemes */
		case WAT_SMS_PDU_DCS_CHARSET_7BIT:
			/* Default Aplhabet, phase 2 */
			sms_event.content_len = wat_decode_sms_pdu_message_7_bit(sms_event.content, sizeof(sms_event.content), &data[i], content_len , sms_event.pdu.seq);
			break;
		case WAT_SMS_PDU_DCS_CHARSET_8BIT:
		case WAT_SMS_PDU_DCS_CHARSET_16BIT:
			sms_event.content_len = content_len;
			memcpy(sms_event.content, &data[i], content_len);
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

static int wat_sms_encode_pdu(wat_span_t *span, wat_sms_t *sms)
{
	uint8_t pdu_data[200];
	unsigned pdu_data_len = 0;
	int pdu_header_len = 0;
	wat_sms_event_t *sms_event = &sms->sms_event;
	int i;

	/* www.dreamfabric.com/sms/ */

	/* Fill in the SMSC */
	if (sms_event->pdu.smsc.digits[0] == '+') {
		memmove(&sms_event->pdu.smsc.digits[0], &sms_event->pdu.smsc.digits[1], sizeof(sms_event->pdu.smsc.digits));
	}
	
	/* Length SMSC information */
	pdu_data[pdu_data_len] = (1 + (strlen(sms_event->pdu.smsc.digits) + 1) / 2);
	pdu_data_len++;

	/* SMSC Type-of-Address */
	pdu_data[pdu_data_len] = 0x80; /* MSB always set to 1 */
	pdu_data[pdu_data_len] |= (sms_event->pdu.smsc.type & 0x7) << 4;
	pdu_data[pdu_data_len] |= (sms_event->pdu.smsc.plan & 0xF);
	pdu_data_len++;

	pdu_data_len += wat_encode_sms_pdu_semi_octets((char *)&pdu_data[pdu_data_len], sms_event->pdu.smsc.digits, strlen(sms_event->pdu.smsc.digits));

	pdu_header_len = pdu_data_len;

	/* SMS-SUBMIT */
	pdu_data_len += wat_encode_sms_pdu_submit(span, (char *)&pdu_data[pdu_data_len], &sms_event->pdu.sms.submit);

	/* TP-Message-Reference */
	pdu_data[pdu_data_len] = sms_event->pdu.refnr;
	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log_span(span, WAT_LOG_DEBUG, "PDU[%04d] TP-Message-Reference:0x%02x\n", pdu_data_len, pdu_data[pdu_data_len]);
	}
	pdu_data_len++;

	/* Address-Length. Length of phone number */
	pdu_data[pdu_data_len] = strlen(sms_event->to.digits);
	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log_span(span, WAT_LOG_DEBUG, "PDU[%04d] Address-Length:0x%02x\n", pdu_data_len, pdu_data[pdu_data_len]);
	}
	pdu_data_len++;

	/* Destination Type-of-Address */
	pdu_data[pdu_data_len] = 0x80; /* MSB always set to 1 */
	pdu_data[pdu_data_len] |= (sms_event->to.type & 0x7) << 4;
	pdu_data[pdu_data_len] |= (sms_event->to.plan & 0xF);

	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log_span(span, WAT_LOG_DEBUG,  "PDU[%04d] Type-Of-Address:0x%02x\n", pdu_data_len, pdu_data[pdu_data_len]);
	}
	pdu_data_len++;

	pdu_data_len += wat_encode_sms_pdu_semi_octets((char *)&pdu_data[pdu_data_len], sms_event->to.digits, strlen(sms_event->to.digits));
	

	/* TP-PID - Protocol Identifier */
	pdu_data[pdu_data_len] = 0x00;
	pdu_data_len++;

	/* TP-DCS - Data Coding Scheme */
	/* www.dreamfabric.com/sms/dcs.html */
	/* Bit 5 = compressed/uncompressed */
	pdu_data_len += wat_encode_sms_pdu_dcs(span, (char *)&pdu_data[pdu_data_len], &sms_event->pdu.dcs);
	
	/* TP-VP - Validity Period */
	/* www.dreamfabric.com/sms/vp.html */
	switch (sms_event->pdu.sms.submit.tp_vpf) {
		case WAT_SMS_PDU_VP_NOT_PRESENT:
			break;
		case WAT_SMS_PDU_VP_RELATIVE:
			pdu_data[pdu_data_len] = sms_event->pdu.sms.submit.vp_data.relative;
			pdu_data_len++;
			break;
		case WAT_SMS_PDU_VP_ABSOLUTE:
		case WAT_SMS_PDU_VP_ENHANCED:
			wat_log_span(span, WAT_LOG_ERROR, "Warning  Validity period type not supported\n");
			break;
	}	
	
	if (sms_event->pdu.dcs.charset == WAT_SMS_PDU_DCS_CHARSET_7BIT) {
		pdu_data_len += wat_encode_sms_pdu_message_7_bit(&pdu_data[pdu_data_len], sizeof(pdu_data) - pdu_data_len, sms_event->content, sms_event->content_len, 0);

	}
	
	sms->pdu_len = pdu_data_len - pdu_header_len;

	/*  Convert into string representation */
	for (i = 0; i < pdu_data_len; i++) {
		sms->body_len += sprintf((char*)&sms->body[sms->body_len], "%02x", pdu_data[i]);
	}
	return 0;
}
