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
	while(iter->index <= sizeof(span->calls)/sizeof(span->calls[0])) {
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
			while (iter->index < sizeof(iter->span->calls)/sizeof(iter->span->calls[0])) {
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
			wat_assert_return(iter->index <= sizeof(iter->span->calls)/sizeof(iter->span->calls[0]), NULL, "channel iterator index bigger than calls size!\n");
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

