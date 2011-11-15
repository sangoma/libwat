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
			span->sms = sms;
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

static void wat_decode_sms_pdu_semi_octets(char *string, uint8_t *data, wat_size_t len)
{
	int i;
	char *p = string;
	
	for (i = 0; i < len; i++) {
		sprintf(p++, "%d", data[i] & 0x0F);
		if (((data[i] >> 4) & 0x0F) != 0x0F) {
			sprintf(p++, "%d", (data[i] >> 4) & 0x0F);
		}
	}
	return;
}

static void wat_decode_sms_pdu_deliver(wat_sms_pdu_deliver_t *deliver, uint8_t data)
{
	deliver->mti = data & 0x03;
	deliver->mms = (data > 2) & 0x01;
	deliver->sri = (data > 4) & 0x01;
	deliver->udhi = (data > 5) & 0x01;
	deliver->rp = (data > 6) & 0x01;
	return;
}

static void wat_decode_sms_pdu_timestamp(wat_sms_pdu_timestamp_t *ts, uint8_t *data)
{
	ts->year = SWAP_NIBBLE(data[0]);
	ts->month = SWAP_NIBBLE(data[1]);
	ts->day = SWAP_NIBBLE(data[2]);
	ts->hour = SWAP_NIBBLE(data[3]);
	ts->minute = SWAP_NIBBLE(data[4]);
	ts->second = SWAP_NIBBLE(data[5]);
	ts->timezone = SWAP_NIBBLE(data[6]);
}

static int wat_decode_sms_pdu_message(char *message, uint8_t *data, wat_size_t len)
{
	return 0;
}

wat_status_t wat_handle_incoming_sms_pdu(uint8_t *data, wat_size_t len)
{	
	
	wat_sms_event_t sms_event;
	int i = 0;
	
	/* From www.dreamfabric.com/sms */ 
	memset(&sms_event, 0, sizeof(sms_event));
#if 0
	sms_event.pdu.smsc.len = atoi(((char *)(data[i++] & 0xFF)));
	//sms_event.pdu.smsc.toa = atoi(&(char)data[i++]);
	wat_decode_sms_pdu_semi_octets(sms_event.pdu.smsc.number, &data[i++], sms_event.pdu.smsc.len-1);

	i += sms_event.pdu.smsc.len - 1;
	wat_decode_sms_pdu_deliver(&sms_event.pdu.deliver, data[i++]);

	sms_event.pdu.sender.len = data[i++];
	sms_event.pdu.sender.toa = data[i++];
	wat_decode_sms_pdu_semi_octets(sms_event.pdu.sender.number, &data[i++], sms_event.pdu.sender.len-1);

	i += sms_event.pdu.sender.len - 1;

	sms_event.pdu.pid = data[i++];
	sms_event.pdu.dcs = data[i++];

	wat_decode_sms_pdu_timestamp(&sms_event.pdu.timestamp, &data[i++]);

	i += 6;

	sms_event.len = wat_decode_sms_pdu_message(sms_event.message, &data[i+1], data[i]);
#endif
	return WAT_SUCCESS;
}
