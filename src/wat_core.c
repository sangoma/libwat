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

#include <stdarg.h>

#include "libwat.h"
#include "wat_internal.h"

extern wat_event_handler_t event_handlers[];

static wat_status_t wat_span_perform_start(wat_span_t *span);
static wat_status_t wat_span_perform_post_start(wat_span_t *span);
static wat_status_t wat_span_perform_stop(wat_span_t *span);

WAT_RESPONSE_FUNC(wat_response_post_start_complete);
WAT_SCHEDULED_FUNC(wat_scheduled_wait_sim);

/* Check for pending commands, and execute command if module is not cmd_busy */
void wat_span_run_events(wat_span_t *span)
{
	wat_event_t *event = NULL;

	while (1) {
		event = wat_queue_dequeue(span->event_queue);
		if (!event) {
			break;
		}
		wat_event_process(span, event);
		wat_safe_free(event);
	}
	
	return;
}

void wat_span_run_sched(wat_span_t *span)
{
	wat_sched_run(span->sched);
	return;
}

/* Check for pending commands, and execute command if module is not cmd_busy */
void wat_span_run_cmds(wat_span_t *span)
{
	wat_cmd_t *cmd;

	/* Once we are in SMS mode, we have to finish transmitting that SMS before
	writing any other commands */
	if (span->sms_write) {
		if (wat_sms_send_body(span->outbound_sms) == WAT_BREAK) {
			return;
		}
	}

	if (!span->cmd_busy) {
		/* Check if there are any commands waiting to be transmitted */
		cmd = wat_queue_dequeue(span->cmd_queue);
		if (cmd) {
			if (cmd->cmd == NULL) {
				/* This is a dummy command, just call the callback function */
				wat_log_span(span, WAT_LOG_DEBUG, "Dequeuing dummy command %p\n", cmd->cb);
				cmd->cb(span, NULL, WAT_SUCCESS, cmd->obj, NULL);
				wat_safe_free(cmd);
				return;
			}
			span->cmd = cmd;
			span->cmd_busy = 1;

			if (g_debug & WAT_DEBUG_AT_HANDLE) {
				char mydata[WAT_MAX_CMD_SZ];
				wat_log_span(span, WAT_LOG_DEBUG, "Dequeuing command %s\n", format_at_data(mydata, span->cmd->cmd, strlen(span->cmd->cmd)));
			}

			wat_write_command(span);
			wat_sched_timer(span->sched, "command timeout", span->config.timeout_command, wat_cmd_timeout, (void*) span, &span->timeouts[WAT_TIMEOUT_CMD]);
		}
	}

	/* Check if there are any commands that we received */
	wat_cmd_process(span);

	return;
}


void wat_span_run_smss(wat_span_t *span)
{	
	if (!span->outbound_sms) {
		wat_sms_t *sms = NULL;
		sms = wat_queue_dequeue(span->sms_queue);
		if (sms) {
			span->outbound_sms = sms;
			wat_sms_set_state(sms, WAT_SMS_STATE_START);
		}
	}
	return;
}


wat_iterator_t *wat_get_iterator(wat_iterator_type_t type, wat_iterator_t *iter)
{
	int allocated = 0;
	if (iter) {
		if (iter->type != type) {
			wat_log(WAT_LOG_ERROR, "Cannot switch iterator types\n");
			return NULL;
		}
		allocated = iter->allocated;
		memset(iter, 0, sizeof(*iter));
		iter->type = type;
		iter->allocated = allocated;
		return iter;
	}

	iter = wat_calloc(1, sizeof(*iter));
	if (!iter) {
		return NULL;
	}
	iter->type = type;
	iter->allocated = 1;
	return iter;
}

wat_iterator_t *wat_span_get_call_iterator(const wat_span_t *span, wat_iterator_t *iter)
{
	if (!(iter = wat_get_iterator(WAT_ITERATOR_CALLS, iter))) {
		return NULL;
	}	
	iter->index = 1;
	while(iter->index < wat_array_len(span->calls)) {
		/* Could have empty pointers in the middle of array, so find the next
		one that's not empty */
		if (span->calls[iter->index]) {
			break;
		}
		iter->index++;
	}

	if (!span->calls[iter->index]) {
		wat_safe_free(iter);
		return NULL;
	}

	iter->span = span;
	return iter;
}

wat_iterator_t *wat_span_get_notify_iterator(const wat_span_t *span, wat_iterator_t *iter)
{
	if (!(iter = wat_get_iterator(WAT_ITERATOR_NOTIFYS, iter))) {
		return NULL;
	}
	iter->index = 1;

	if (!span->notifys[iter->index]) {
		/* If the first element in the array is NULL, there are no elements in the array, return NULL */
		wat_safe_free(iter);
		return NULL;
	}
	iter->span = span;
	return iter;
}

wat_iterator_t *wat_iterator_next(wat_iterator_t *iter)
{
	wat_assert_return(iter && iter->type, NULL, "Invalid iterator\n");

	switch (iter->type) {		
		case WAT_ITERATOR_CALLS:
			wat_assert_return(iter->index, NULL, "calls iterator index cannot be zero!\n");
			while (iter->index < wat_array_len(iter->span->calls)) {
				iter->index++;
				if (iter->span->calls[iter->index]) {
					return iter;
				}				
			}
			return NULL;
		case WAT_ITERATOR_NOTIFYS:
			wat_assert_return(iter->index, NULL, "notify iterator index cannot be zero!\n");
			if (iter->index == iter->span->notify_count) {
				return NULL;
			}
			iter->index++;
			if (!iter->span->notifys[iter->index]) {
				return NULL;
			}
			return iter;	
		default:
			break;
	}

	wat_assert_return(0, NULL, "Unknown iterator type\n");
	return NULL;
}

void *wat_iterator_current(wat_iterator_t *iter)
{
	wat_assert_return(iter && iter->type, NULL, "Invalid iterator\n");

	switch (iter->type) {
		case WAT_ITERATOR_CALLS:
			wat_assert_return(iter->index, NULL, "calls iterator index cannot be zero!\n");
			wat_assert_return(iter->index <= wat_array_len(iter->span->calls), NULL, "channel iterator index bigger than calls size!\n");
			return iter->span->calls[iter->index];
		case WAT_ITERATOR_NOTIFYS:
			wat_assert_return(iter->index, NULL, "notify iterator index cannot be zero!\n");
			wat_assert_return(iter->index <= iter->span->notify_count, NULL, "channel iterator index bigger than notify count!\n");
			return iter->span->notifys[iter->index];
		default:
			break;
	}

	wat_assert_return(0, NULL, "Unknown iterator type\n");
	return NULL;
}

wat_status_t wat_iterator_free(wat_iterator_t *iter)
{
	/* it's valid to pass a NULL iterator, do not return failure  */
	if (!iter) {
		return WAT_SUCCESS;
	}

	if (!iter->allocated) {
		memset(iter, 0, sizeof(*iter));
		return WAT_SUCCESS;
	}

	wat_assert_return(iter->type, WAT_FAIL, "Cannot free invalid iterator\n");
	wat_safe_free(iter);

	return WAT_SUCCESS;
}

wat_status_t wat_span_update_alarm_status(wat_span_t *span, wat_alarm_t new_alarm)
{
	if (new_alarm != span->alarm) {
		span->alarm = new_alarm;
		if (g_interface.wat_span_sts) {
			wat_span_status_t sts_event;

			memset(&sts_event, 0, sizeof(sts_event));
			sts_event.type = WAT_SPAN_STS_ALARM;
			sts_event.sts.alarm = span->alarm;
			g_interface.wat_span_sts(span->id, &sts_event);
		}
	}
	return WAT_SUCCESS;
}

wat_status_t wat_span_update_sig_status(wat_span_t *span, wat_bool_t up)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Signalling status changed to %s\n", up ? "Up": "Down");

	span->sigstatus = up ? WAT_SIGSTATUS_UP: WAT_SIGSTATUS_DOWN;

	if (span->state == WAT_SPAN_STATE_RUNNING) {
		if (g_interface.wat_span_sts) {
			wat_span_status_t sts_event;

			memset(&sts_event, 0, sizeof(sts_event));
			sts_event.type = WAT_SPAN_STS_SIGSTATUS;
			sts_event.sts.sigstatus = span->sigstatus;
			g_interface.wat_span_sts(span->id, &sts_event);
		}
	}

	if (span->sigstatus == WAT_SIGSTATUS_UP) {
		/* Get the Operator Name */
		wat_cmd_enqueue(span, "AT+COPS?", wat_response_cops, NULL);

		/* Own Number */
		wat_cmd_enqueue(span, "AT+CNUM", wat_response_cnum, NULL);
	}

	return WAT_SUCCESS;
}

wat_status_t wat_span_update_net_status(wat_span_t *span, unsigned stat)
{
	switch (stat) {
		case WAT_NET_NOT_REGISTERED:
		case WAT_NET_REGISTERED_HOME:
		case WAT_NET_NOT_REGISTERED_SEARCHING:
		case WAT_NET_REGISTRATION_DENIED:
		case WAT_NET_UNKNOWN:
		case WAT_NET_REGISTERED_ROAMING:
			break;
		default:
			wat_log_span(span, WAT_LOG_CRIT, "Invalid network status:%s\n", stat);
			return WAT_FAIL;
	}

	if (span->net_info.stat != stat) {
		wat_log_span(span, WAT_LOG_NOTICE, "Network status changed to \"%s\"\n", wat_net_stat2str(stat));

		if (wat_sig_status_up(span->net_info.stat) != wat_sig_status_up(stat)) {
			wat_span_update_sig_status(span, wat_sig_status_up(stat));
		}
		span->net_info.stat = stat;
	}
	return WAT_SUCCESS;
}

wat_bool_t wat_sig_status_up(wat_net_stat_t stat)
{
	switch(stat) {
		case WAT_NET_NOT_REGISTERED:
		case WAT_NET_NOT_REGISTERED_SEARCHING:
		case WAT_NET_REGISTRATION_DENIED:
		case WAT_NET_UNKNOWN:
			return WAT_FALSE;
		case WAT_NET_REGISTERED_HOME:
		case WAT_NET_REGISTERED_ROAMING:
			return WAT_TRUE;
		case WAT_NET_INVALID:
			wat_log(WAT_LOG_CRIT, "Invalid network status\n");
			return WAT_FALSE;
	}
	/* Should never reach here */
	return WAT_FALSE;
}

char *wat_string_clean(char *string)
{
	if (string[0] == '\"') {
		int len = strlen(string);
		memmove(string, &string[1], len - 1);
		string[len - 1]='\0';
	}
	if (string[strlen(string) - 1] == '\"') {
		string[strlen(string) - 1] = '\0';
	}
	return string;
}

void wat_decode_type_of_address(uint8_t octet, wat_number_type_t *type, wat_number_plan_t *plan)
{
	if (type) {
		*type = (octet > 4) & 0x07;
	}

	if (plan) {
		/* Numbering plan has non-consecutive values */
		switch(octet & 0x0F) {
			case 0:
				*plan = WAT_NUMBER_PLAN_UNKNOWN;
				break;
			case 1:
				*plan = WAT_NUMBER_PLAN_ISDN;
				break;
			case 3:
				*plan = WAT_NUMBER_PLAN_DATA;
				break;
			case 4:
				*plan = WAT_NUMBER_PLAN_TELEX;
				break;
			case 8:
				*plan = WAT_NUMBER_PLAN_NATIONAL;
				break;
			case 9:
				*plan = WAT_NUMBER_PLAN_PRIVATE;
				break;
			case 10:
				*plan = WAT_NUMBER_PLAN_ERMES; 
				break;
			case 15:
				*plan = WAT_NUMBER_PLAN_RESERVED;
				break;
			default:
				*plan = WAT_NUMBER_PLAN_INVALID;
				break;
		}
	}
	return;
}

static wat_status_t wat_span_perform_start(wat_span_t *span)
{
	memset(span->calls, 0, sizeof(span->calls));
	memset(span->notifys, 0, sizeof(span->notifys));
	memset(&span->net_info, 0, sizeof(span->net_info));
	
	if (wat_queue_create(&span->event_queue, WAT_EVENT_QUEUE_SZ) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create queue\n");
		return WAT_FAIL;
	}

	if (wat_queue_create(&span->cmd_queue, WAT_CMD_QUEUE_SZ) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create queue\n");
		return WAT_FAIL;
	}

	if (wat_queue_create(&span->sms_queue, WAT_MAX_SMSS_PER_SPAN) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create queue\n");
		return WAT_FAIL;
	}

	if (wat_buffer_create(&span->buffer, WAT_BUFFER_SZ) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create buffer\n");
		return WAT_FAIL;
	}

	if (wat_sched_create(&span->sched, "span_schedule") != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create scheduler\n");
		return WAT_FAIL;
	}

	wat_log_span(span, WAT_LOG_DEBUG, "Starting span\n");

	wat_cmd_register(span, "+CRING", wat_notify_cring);

	wat_cmd_register(span, "+CMT", wat_notify_cmt);

	wat_cmd_register(span, "+CLIP", wat_notify_clip);
	wat_cmd_register(span, "+CREG", wat_notify_creg);

#if 0
	wat_cmd_register(span, "+CDIP", wat_notify_cdip);
	wat_cmd_register(span, "+CNAP", wat_notify_cnap);
	wat_cmd_register(span, "+CCWA", wat_notify_ccwa);
#endif

	/* Module soft reset */
	wat_cmd_enqueue(span, "ATZ", wat_response_atz, NULL);

	/* Disable echo mode */
	wat_cmd_enqueue(span, "ATE0", wat_response_ate, NULL);

	wat_cmd_enqueue(span, "ATX4", NULL, NULL);

	/* Enable Mobile Equipment Error Reporting, numeric mode */
	wat_cmd_enqueue(span, "AT+CMEE=1", NULL, NULL);

	/* Enable extended format reporting */
	wat_cmd_enqueue(span, "AT+CRC=1", NULL, NULL);

	/* Enable New Message Indications To TE */
	wat_cmd_enqueue(span, "AT+CNMI=2,2", wat_response_cnmi, NULL);

	span->module.wait_sim(span);
	
	wat_sched_timer(span->sched, "wait_sim", span->config.timeout_wait_sim, wat_scheduled_wait_sim, (void *) span, &span->timeouts[WAT_TIMEOUT_WAIT_SIM]);
	return WAT_SUCCESS;
}

WAT_RESPONSE_FUNC(wat_response_post_start_complete)
{
	WAT_RESPONSE_FUNC_DBG_START
	wat_span_set_state(span, WAT_SPAN_STATE_RUNNING);
	WAT_FUNC_DBG_END
	return 0;
}

/* This function is executed once the SIM is Inserted and ready */
static wat_status_t wat_span_perform_post_start(wat_span_t *span)
{
	/* Enable Calling Line Presentation */
	wat_cmd_enqueue(span, "AT+CLIP=1", wat_response_clip, NULL);
	
	/* Set Operator mode */
	wat_cmd_enqueue(span, "AT+COPS=3,0", wat_response_cops, NULL);

	/* Set the Call Class to voice  */
	/* TODO: The FCLASS should be set before sending ATD command for each call */
	wat_cmd_enqueue(span, "AT+FCLASS=8", NULL, NULL);

	/* Call module specific start here */
	span->module.start(span);

	span->module.set_codec(span, span->config.codec_mask);

	/* Check the PIN status, this will also report if there is no SIM inserted */
	wat_cmd_enqueue(span, "AT+CPIN?", wat_response_cpin, NULL);

	/* Get some information about the chip */
	
	/* Get Module Model Identification */
	wat_cmd_enqueue(span, "AT+CGMM", wat_response_cgmm, NULL);

	/* Get Module Manufacturer Identification */
	wat_cmd_enqueue(span, "AT+CGMI", wat_response_cgmi, NULL);

	/* Get Module Revision Identification */
	wat_cmd_enqueue(span, "AT+CGMR", wat_response_cgmr, NULL);

	/* Get Module Serial Number */
	wat_cmd_enqueue(span, "AT+CGSN", wat_response_cgsn, NULL);

	/* Get Module IMSI */
	wat_cmd_enqueue(span, "AT+CIMI", wat_response_cimi, NULL);

	/* Signal Quality */
	wat_cmd_enqueue(span, "AT+CSQ", wat_response_csq, NULL);
	
	/* Enable Network Registration Unsolicited result code */
	wat_cmd_enqueue(span, "AT+CREG=1", NULL, NULL);

	/* Check Registration Status in case module is already registered */
	wat_cmd_enqueue(span, "AT+CREG?", wat_response_creg, NULL);

	wat_cmd_enqueue(span, NULL, wat_response_post_start_complete, NULL);

	wat_sched_timer(span->sched, "signal_monitor", span->config.signal_poll_interval, wat_scheduled_csq, (void*) span, NULL);
	return WAT_SUCCESS;
}

static wat_status_t wat_span_perform_stop(wat_span_t *span)
{
	wat_iterator_t *iter = NULL;
	wat_iterator_t *curr = NULL;

	span->module.shutdown(span);

	wat_sched_destroy(&span->sched);
	wat_buffer_destroy(&span->buffer);
	wat_queue_destroy(&span->sms_queue);
	wat_queue_destroy(&span->event_queue);
	wat_queue_destroy(&span->cmd_queue);

	iter = wat_span_get_notify_iterator(span, iter);
	for (curr = iter; curr; curr = wat_iterator_next(curr)) {
		wat_notify_t *notify = wat_iterator_current(curr);
		wat_safe_free(notify->prefix);
		wat_safe_free(notify);
	}
	wat_iterator_free(iter);
	return WAT_SUCCESS;
}

wat_status_t _wat_span_set_state(const char *func, int line, wat_span_t *span, wat_span_state_t new_state)
{
	wat_status_t status = WAT_SUCCESS;

	/* TODO: Implement state table for allowable state changes */
	if (g_debug & WAT_DEBUG_SPAN_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "[id:%d] Span State change from %s to %s\n", span->id, wat_span_state2str(span->state), wat_span_state2str(new_state), func, line);
	}

	switch(new_state) {
		case WAT_SPAN_STATE_INIT:
			/* Initial state, do nothing */
			break;
		case WAT_SPAN_STATE_START:
			if (span->state >= WAT_SPAN_STATE_START) {
				wat_log(WAT_LOG_CRIT, "Span start was already performed\n");
				status = WAT_FAIL;
			} else {
				status = wat_span_perform_start(span);
			}
			break;
		case WAT_SPAN_STATE_POST_START:
			if (span->state >= WAT_SPAN_STATE_POST_START) {
				wat_log(WAT_LOG_CRIT, "Span post-start was already performed\n");
				status = WAT_FAIL;
			} else {
				wat_sched_cancel_timer(span->sched, span->timeouts[WAT_TIMEOUT_WAIT_SIM]);
				status = wat_span_perform_post_start(span);
			}
			break;
		case WAT_SPAN_STATE_RUNNING:
			{
				/* Notify the user that we are ready */
				if (g_interface.wat_span_sts) {
					wat_span_status_t sts_event;

					memset(&sts_event, 0, sizeof(sts_event));
					sts_event.type = WAT_SPAN_STS_READY;					
					g_interface.wat_span_sts(span->id, &sts_event);
				}

				if (g_interface.wat_span_sts) {
					/* We do not send STS_SIGSTATUS events to the user app until we are in running state, 
					   so send the first STS_SIGSTATUS event right when we finish initialization to set the
					   initial sigstatus for the user */

					wat_span_status_t sts_event;

					memset(&sts_event, 0, sizeof(sts_event));
					sts_event.type = WAT_SPAN_STS_SIGSTATUS;
					sts_event.sts.sigstatus = span->sigstatus;
					g_interface.wat_span_sts(span->id, &sts_event);
				}

				status = WAT_SUCCESS;
			}
			break;
		case WAT_SPAN_STATE_STOP:
			if (span->state < WAT_SPAN_STATE_START) {
				wat_log(WAT_LOG_CRIT, "Span was not started\n");
				status = WAT_FAIL;
			} else {
				status = wat_span_perform_stop(span);
			}
			break;
		case WAT_SPAN_STATE_SHUTDOWN:
			
			break;
		default:
			wat_log(WAT_LOG_CRIT, "Unhandled state change\n");
	}

	if (status == WAT_SUCCESS) {
		span->state = new_state;
	}
	return status;
}

WAT_SCHEDULED_FUNC(wat_scheduled_wait_sim)
{
	wat_span_t *span = (wat_span_t*) data;

	wat_log_span(span, WAT_LOG_ERROR, "SIM ready timeout\n");
	wat_span_update_alarm_status(span, WAT_ALARM_SIM_ACCESS_FAIL);
}
