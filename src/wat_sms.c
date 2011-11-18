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
#include "pduconv.h"

static uint8_t hexstr_to_val(char *string);
static uint8_t decstr_to_val(char *string);
static int wat_decode_sms_pdu_sender(wat_number_t *sender, char *data);
static int wat_decode_sms_pdu_smsc(wat_number_t *smsc, char *data);
static int wat_decode_sms_pdu_timestamp(wat_sms_pdu_timestamp_t *ts, char *data);
static int wat_decode_sms_pdu_message_7_bit(char *message, char *data, wat_size_t len, uint8_t pad_len, uint8_t carry_over);


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

			if (wat_queue_enqueue(span->sms_queue, sms) != WAT_SUCCESS) {
				wat_log_span(span, WAT_LOG_DEBUG, "[sms:%d] SMS queue full\n", sms->id);
				sms->cause = WAT_SMS_CAUSE_QUEUE_FULL;
				wat_sms_set_state(sms, WAT_SMS_STATE_COMPLETE);
			}
			break;
		case WAT_SMS_STATE_START:
			/* See if we need to adjust the sms mode */
			span->outbound_sms = sms;
			if (span->sms_mode != sms->type) {
				if (sms->type == WAT_SMS_TXT) {
					wat_cmd_enqueue(span, "AT+CMGF=1", wat_response_cmgf, sms);
				} else {
					wat_cmd_enqueue(span, "AT+CMGF=0", wat_response_cmgf, sms);
				}
			} else {
				wat_sms_set_state(sms, WAT_SMS_STATE_SEND_HEADER);
			}
			break;
		case WAT_SMS_STATE_SEND_HEADER:
			{
				if (sms->type == WAT_SMS_PDU) {
					/* TODO: PDU not implemented */
				} else {
					char cmd[40];
					memset(cmd, 0, sizeof(cmd));

					/* TODO set the TON/NPI as well */
					sprintf(cmd, "AT+CMGS=\"%s\"", sms->called_num.digits);
					wat_cmd_enqueue(span, cmd, wat_response_cmgs_start, sms);
				}
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

	while (sms->wrote < sms->len) {
		memset(command, 0, sizeof(command));
		len = sms->len - sms->wrote;
		if (len <= 0) {
			break;
		}

		if (len > sizeof(command)) {
			len = sizeof(command);
		}
		
		memcpy(command, &sms->message[sms->wrote], len);
		/* We need to add an extra 2 bytes because dahdi expects a CRC */
		len_wrote = wat_span_write(span, command, len + 2) - 2;

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

static int wat_decode_sms_pdu_semi_octets(char *string, char *data, wat_size_t len)
{
	int i;
	int ret = 0;
	char *p = string;

	for (i = 0; i < (len*2); i+=2) {
		*p++ = data[i+1];
		if (data[i] != 'F') {
			*p++ = data[i];
		}
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

static int wat_decode_sms_pdu_timestamp(wat_sms_pdu_timestamp_t *ts, char *data)
{
	int i;
	if (g_debug & WAT_DEBUG_PDU_DECODE) {
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

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  Year:%d Month:%d Day:%d Hr:%d Min:%d Sec:%d Timezone:%d\n",
																	ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second, ts->timezone);
	}
	
	/* TP-SCTS always takes 14 bytes */
	return 14;
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

static int wat_decode_sms_pdu_message_7_bit(char *message, char *data, wat_size_t len, uint8_t pad_len, uint8_t carry_over)
{
	int i;
	uint8_t data_pdu[WAT_MAX_SMS_SZ];

	len = 10;
	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding PDU Message (len:%d)[%s]\n", len, data);
	}

	memset(data_pdu, 0, sizeof(data_pdu));

	for (i = 0; i < len - 1; i++) {
		data_pdu[i] = hexstr_to_val(&data[i*2]);
	}

	memset(message, 0, WAT_MAX_SMS_SZ);
	/* 7-bit encoding ---> 8-bit encoding */

	if (((len*7)%8)) {
		pdu_to_ascii(data_pdu, ((len*7)/8) + 1, message);
	} else {
		pdu_to_ascii(data_pdu, ((len*7)/8), message);
	}

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Message:%s\n", message);
	}
	return len;
}

static int wat_decode_sms_pdu_smsc(wat_number_t *smsc, char *data)
{
	/* www.dreamfabric.com/sms/type_of_address.html */
	int len;

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding SMSC information [%s]\n", data);
	}

	len = hexstr_to_val(&data[0]);

	wat_decode_type_of_address(hexstr_to_val(&data[2]), &smsc->type, &smsc->plan);

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  len:%d type:%d plan:%d\n", len, smsc->type, smsc->plan);
	}
	
	/* For the smsc, the 'length' parameter specifies how many octets in the sms->number */
	/* Actual length of number is sms_event.pdu.smsc.len-1 because we have to substract 'toa' */
	wat_decode_sms_pdu_semi_octets(smsc->digits, &data[4], len - 1);

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
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

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding Sender information [%s]\n", data);
	}
	
	len = hexstr_to_val(&data[0]);

	wat_decode_type_of_address(hexstr_to_val(&data[2]), &sender->type, &sender->plan);

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  len:%d type:%d plan:%d\n", len, sender->type, sender->plan);
	}

	/* For the sender, the 'length' parameter specifies how many characters are in the sender->number:
	   2 characters per octet, there is a trailing F is there is an odd number of characters */
	data_len = (len / 2) + (len % 2);

	wat_decode_sms_pdu_semi_octets(sender->digits, (char *)&data[4], data_len);
	
	if (g_debug & WAT_DEBUG_PDU_DECODE) {
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
	uint8_t padding = 0;
	uint8_t carry_over = 0;

	/* From www.dreamfabric.com/sms */
	/* GSM 03.38 */
	
	memset(&sms_event, 0, sizeof(sms_event));

	ret = wat_decode_sms_pdu_smsc(&sms_event.pdu.smsc, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMSC from SMS PDU data [%s]\n", &data[i]);
		return WAT_FAIL;
	}
	i += ret;

	ret = wat_decode_sms_pdu_deliver(&sms_event.pdu.sms_deliver, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMS-DELIVER from SMS PDU data [%s]\n", &data[i]);
		/* TODO print SMS PDU here */
		return WAT_FAIL;
	}
	i += ret;

	ret = wat_decode_sms_pdu_sender(&sms_event.pdu.sender, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMS-SENDER from SMS PDU data [%s]\n", &data[i]);
		/* TODO print SMS PDU here */
		return WAT_FAIL;
	}
	i += ret;

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding TP-PID TP-DCS [%s]\n", &data[i]);
	}

	sms_event.pdu.tp_pid = hexstr_to_val(&data[i]);
	i += 2;
	sms_event.pdu.tp_dcs = hexstr_to_val(&data[i]);
	i += 2;

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  TP-PID:%d TP-DCS:%d\n", sms_event.pdu.tp_pid, sms_event.pdu.tp_dcs);
	}

	ret = wat_decode_sms_pdu_timestamp(&sms_event.pdu.tp_scts, &data[i]);
	if (ret <= 0) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to decode SMS-SCTS from SMS PDU data [%s]\n", &data[i]);
		/* TODO print SMS PDU here */
		return WAT_FAIL;
	}

	i+= ret;

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding TP-UDL [%s]\n", &data[i]);
	}
	sms_event.pdu.tp_udl = hexstr_to_val(&data[i]);
	i += 2;

	if (g_debug & WAT_DEBUG_PDU_DECODE) {
		/* User data length */
		wat_log(WAT_LOG_DEBUG, "TP-UDL:%d\n", sms_event.pdu.tp_udl);
	}

	if (sms_event.pdu.sms_deliver.tp_udhi) {
		if (g_debug & WAT_DEBUG_PDU_DECODE) {
			wat_log(WAT_LOG_DEBUG, "Decoding TP-UDHL [%s]\n", &data[i]);
		}
		sms_event.pdu.tp_udhl = hexstr_to_val(&data[i]);
		i += 2;

		sms_event.pdu.iei = hexstr_to_val(&data[i]);
		i += 2;

		sms_event.pdu.iedl = hexstr_to_val(&data[i]);
		i += 2;

		sms_event.pdu.refnr = hexstr_to_val(&data[i]);
		i += 2;

		sms_event.pdu.total = hexstr_to_val(&data[i]);
		i += 2;

		sms_event.pdu.seq = hexstr_to_val(&data[i]);
		i += 2;

		padding = 1;
		carry_over = 1;
		if (g_debug & WAT_DEBUG_PDU_DECODE) {
			/* User data length */
			wat_log(WAT_LOG_DEBUG, "TP-UDHL:%d IEI:%d IEDL:%d Ref nr:%d Total:%d Seq:%d\n",
					sms_event.pdu.tp_udhl, sms_event.pdu.iei, sms_event.pdu.iedl, sms_event.pdu.refnr, sms_event.pdu.total, sms_event.pdu.seq);
		}
		
	}

	switch (sms_event.pdu.tp_dcs) {
		/* See www.dreamfabric.com/sms/dcs.html for different Data Coding Schemes */
		case 0:
			/* Default Aplhabet, phase 2 */
			sms_event.len = wat_decode_sms_pdu_message_7_bit(sms_event.message, &data[i], sms_event.pdu.tp_udl, sms_event.pdu.seq, sms_event.pdu.seq);
			break;
		default:
			wat_log(WAT_LOG_ERROR, "Dont' know how to decode incoming SMS message with coding scheme:0x%x\n", sms_event.pdu.tp_dcs);
			break;
	}

	return WAT_SUCCESS;
}
