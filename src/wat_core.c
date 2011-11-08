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
#include "telit.h"

extern wat_event_handler_t event_handlers[];

static int g_tokens = 0;

struct terminator_t {
	char *terminator;
	wat_bool_t success;
};

static struct terminator_t terminators[] = {
	{ "OK", WAT_TRUE },
	{ "CONNECT", WAT_TRUE },
	{ "BUSY", WAT_FALSE },
	{ "ERROR", WAT_FALSE },
	{ "NO DIALTONE", WAT_FALSE },
	{ "NO ANSWER", WAT_FALSE },
	{ "NO CARRIER", WAT_FALSE },
	{ "+CMS ERROR:", WAT_FALSE },
	{ "+CME ERROR:", WAT_FALSE },
	{ "+EXT ERROR:", WAT_FALSE }
};

/* TODO switch these functions to static */
wat_status_t wat_cmd_process(wat_span_t *span);
wat_status_t wat_tokenize_line(char *tokens[], char *line, wat_size_t len, wat_size_t *consumed);
wat_status_t wat_cmd_handle_notify(wat_span_t *span, char *tokens[]);
wat_status_t wat_cmd_handle_response(wat_span_t *span, char *tokens[], wat_bool_t success);
wat_status_t wat_check_terminator(const char* token, wat_bool_t *success);

/* Check for pending commands, and execute command if module is not busy */
void wat_span_run_events(wat_span_t *span)
{
	wat_event_t *event = NULL;

	while (1) {
		event = wat_event_dequeue(span);
		if (!event) {
			break;
		}
		wat_event_process(span, event);
		wat_safe_free(event);
	}
	
	return;
}

wat_status_t wat_event_enqueue(wat_span_t *span, wat_event_t *in_event)
{
	wat_event_t *event = wat_calloc(1, sizeof(*event));
	
	wat_assert_return(event, WAT_ENOMEM, "Failed to allocated memory for new event\n");

	memcpy(event, in_event, sizeof(*event));
	if (wat_queue_enqueue(span->event_queue, event) != WAT_SUCCESS) {
		wat_assert("Failed to enqueue new event\n");
		return WAT_FAIL;
	}
	return WAT_SUCCESS;
}

wat_event_t *wat_event_dequeue(wat_span_t *span)
{
	return wat_queue_dequeue(span->event_queue);
}

wat_status_t wat_event_process(wat_span_t *span, wat_event_t *event)
{
	int i = 0;
	wat_log_span(span, WAT_LOG_DEBUG, "Processing event \"%s\"\n", wat_event2str(event->id));
	
	while(event_handlers[i].func != NULL) {
		if (event_handlers[i].event_id == event->id) {
			event_handlers[i].func(span, event);
			goto done;
		}
		i++;
	}
	wat_log_span(span, WAT_LOG_ERROR, "No handler for event \"%s\"\n", wat_event2str(event->id));
	return WAT_FAIL;
done:
	return WAT_SUCCESS;
}

static wat_status_t wat_span_set_state(wat_span_t *span, wat_span_state_t new_state)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Changing state from %s to %s\n", wat_span_state2str(span->state), wat_span_state2str(new_state));

	span->state = new_state;
	return WAT_SUCCESS;
}

/* Check for pending commands, and execute command if module is not busy */
void wat_span_run_cmds(wat_span_t *span)
{
	wat_cmd_t *cmd;

	if (!span->cmd_busy) {
		/* Check if there are any commands waiting to be transmitted */
		cmd = wat_queue_dequeue(span->cmd_queue);
		if (cmd) {
			char command[WAT_MAX_CMD_SZ];

			span->cmd = cmd;
			span->cmd_busy = 1;

			if (g_debug & WAT_DEBUG_UART_DUMP) {
				wat_log_span(span, WAT_LOG_DEBUG, "[TX AT] %s\n", span->cmd->cmd);
			}

			sprintf(command, "%s\r\n ", span->cmd->cmd);

			if (wat_cmd_write(span, command) != WAT_SUCCESS) {
				wat_log_span(span, WAT_LOG_DEBUG, "Failed to write to span\n");
			}
		}
	}

	/* Check if there are any commands that we received */
	wat_cmd_process(span);

	return;
}

wat_status_t wat_cmd_enqueue(wat_span_t *span, const char *incommand, wat_cmd_response_func *cb, void *obj)
{
	wat_cmd_t *cmd;
	wat_assert_return(span->cmd_queue, WAT_FAIL, "No command queue!\n");

	if (!strlen(incommand)) {
		wat_log_span(span, WAT_LOG_DEBUG, "Invalid cmd to enqueue \"%s\"\n", incommand);
		return WAT_FAIL;
	}

	if (g_debug & WAT_DEBUG_AT_HANDLE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Enqueued command \"%s\"\n", incommand);
	}

	/* Add a \r to finish the command */
	
	cmd = wat_calloc(1, sizeof(*cmd));
	wat_assert_return(cmd, WAT_FAIL, "Failed to alloc new command\n");
	
	cmd->cb = cb;
	cmd->obj = obj;
	cmd->cmd = wat_strdup(incommand);
	wat_queue_enqueue(span->cmd_queue, cmd);
	return WAT_SUCCESS;
}

wat_status_t wat_check_terminator(const char* token, wat_bool_t *success)
{
	int i;

	for (i = 0; i < sizeof(terminators)/sizeof(terminators[0]); i++) {
		if (!strncmp(terminators[i].terminator, token, strlen(terminators[i].terminator))) {
			*success = terminators[i].success;
			return WAT_SUCCESS;
		}
	}
	return WAT_FAIL;
}

wat_status_t wat_cmd_process(wat_span_t *span)
{
	char data[WAT_BUFFER_SZ];
	unsigned i = 0;
	wat_size_t len = 0;
	wat_status_t status = WAT_FAIL;

	if (wat_buffer_peep(span->buffer, data, &len) == WAT_SUCCESS) {
		wat_size_t consumed;
		char *tokens[WAT_TOKENS_SZ];
		wat_bool_t success = WAT_FALSE;

		memset(tokens, 0, sizeof(tokens));

		if (g_debug & WAT_DEBUG_UART_DUMP) {
			char mydata[WAT_MAX_CMD_SZ];
			wat_log_span(span, WAT_LOG_DEBUG, "[RX AT] %s (len:%d)\n", format_at_data(mydata, data, len), len);
		}

		status = wat_tokenize_line(tokens, (char*)data, len, &consumed);
		if (status == WAT_SUCCESS) {
			for (i = 0; tokens[i]; i++) {
				wat_bool_t handled = WAT_FALSE;
				status = wat_check_terminator(tokens[i], &success);
				if (status == WAT_SUCCESS) {
					/* This is a single token response or a call hangup */
					if (span->cmd_busy) {
						wat_cmd_handle_response(span, &tokens[i], success);
						handled = WAT_TRUE;
					} else if (!success) {
						/* This is a hangup from the remote side */

						wat_cmd_enqueue(span, "AT+CLCC", wat_response_clcc, NULL);
						handled = WAT_TRUE;
					}
				} else if (tokens[i+1]) {
					/* There is one more token in the list, check if it is a terminator */
					if (wat_check_terminator(tokens[i+1], &success) == WAT_SUCCESS) {
						if (span->cmd_busy) {
							/* This is a two token response */
							wat_cmd_handle_response(span, &tokens[i], success);
							i++;
							handled = WAT_TRUE;
						}
					}
				}

				if (handled == WAT_FALSE) {
					/* We do not have a terminator */
					if (!strncmp(tokens[i], "+", 1) ||
						!strncmp(tokens[i], "#", 1)) {
						/* This could be an unsollicited notification */
						status = wat_cmd_handle_notify(span, &tokens[i]);
						if (status == WAT_BREAK) {
							/* Some responses contain the command prefix, if we do
							not have a notify handler, then this could be an
							incomplete response, so do not flush */
							continue;
						} else {
							handled = WAT_TRUE;
						}
					} else if (span->cmd_busy) {
						/* We do not have a full response, wait for the full response */
						continue;
					} else {
						char mydata[WAT_MAX_CMD_SZ];
						wat_log_span(span, WAT_LOG_DEBUG, "Failed to parse AT commands %s (len:%d)\n", format_at_data(mydata, data, len), len);
					}
				}
				if (handled == WAT_TRUE) {
					/* If we handled this token, remove it from the buffer */
					wat_buffer_flush(span->buffer, consumed);
				}
			} /* for (i = 0; tokens[i]; i++) */
#if 0
			if (1) {
			if (wat_buffer_peep(span->buffer, data, &len) == WAT_SUCCESS) {
				char mydata[WAT_MAX_CMD_SZ];
				wat_log_span(span, WAT_LOG_DEBUG, "peeped AFTER: [%s] len:%d\n", format_at_data(mydata, data, len), len);
			}
		}
#endif	
			wat_free_tokens(tokens);
		}
	}
	
	return WAT_SUCCESS;
}

wat_status_t wat_cmd_handle_response(wat_span_t *span, char *tokens[], wat_bool_t success)
{
	wat_cmd_t *cmd;
	
	wat_assert_return(span->cmd, WAT_FAIL, "We did not have a command pending\n");
	
	cmd = span->cmd;
	if (g_debug & WAT_DEBUG_AT_HANDLE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Handling response for cmd:%s\n", cmd->cmd);
	}
	
	if (cmd->cb) {
		cmd->cb(span, tokens, success, cmd->obj);
	}

	span->cmd = NULL;

	wat_safe_free(cmd->cmd);
	wat_safe_free(cmd);
	span->cmd_busy = 0;
	return WAT_SUCCESS;
}

wat_status_t wat_cmd_handle_notify(wat_span_t *span, char *tokens[])
{
	int i;
	/* For notifications, the first token contains the AT command prefix */
	if (g_debug & WAT_DEBUG_AT_HANDLE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Handling notify for cmd:%s\n", tokens[0]);
	}

	for (i = 0; i < sizeof(span->notifys)/sizeof(span->notifys[0]); i++) {
		if (span->notifys[i]) {
			wat_notify_t *notify = span->notifys[i];
			if (!strncasecmp(notify->prefix, tokens[0], strlen(notify->prefix))) {
				/* TODO: Take out the prefix from the first token */
				return notify->func(span, tokens);
			}
		}
	}

	/* This is not an error, as we could be waiting for the command terminator */
	wat_log_span(span, WAT_LOG_DEBUG, "No handler for unsollicited notify \"%s\"\n", tokens[0]);
	return WAT_BREAK;
}

wat_status_t wat_tokenize_line(char *tokens[], char *line, wat_size_t len, wat_size_t *consumed)
{
	int i;
	int token_index = 0;
	uint8_t has_token = 0;
	unsigned token_start_index = 0;
	unsigned consumed_index = 0;

	char *token_str = NULL;
	char *p = NULL;

	for (i = 0; i < len; i++) {
		switch(line[i]) {
			case '\n':
				if (has_token) {
					/* This is the end of a token */
					has_token = 0;

					tokens[token_index++] = token_str;
				}
				break;
			case '\r':
				/* Ignore \r */
				break;
			default:
				if (!has_token) {
					/* This is the start of a new token */
					has_token = 1;
					token_start_index = i;

					token_str = wat_calloc(1, 200);
					wat_assert_return(token_str, WAT_FAIL, "Failed to allocate new token\n");

					p = token_str;
				}
				*(p++) = line[i];
				
		}
	}

	/* No more tokens left in buffer */
	
	if (token_index) {
		while (i <  len) {
			/* Remove remaining \r and \n" */
			if (line[i] != '\r' && line[i] != '\n') {
				break;
			}
			i++;
		}
		consumed_index = i;

		*consumed = consumed_index;

		if (g_debug & WAT_DEBUG_AT_PARSE) {
			wat_log(WAT_LOG_DEBUG, "Decoded tokens %d consumed:%u len:%u\n", token_index, *consumed, len);

			for (i = 0; i < token_index; i++) {
				wat_log(WAT_LOG_DEBUG, "  Token[%d]:%s\n", i, tokens[i]);
			}
		}
		return WAT_SUCCESS;
	}

	if (has_token) {
		/* We only got half a token */
		wat_safe_free(token_str);
		g_tokens--;
	}

	return WAT_FAIL;
}

WAT_DECLARE(void) wat_free_tokens(char *tokens[])
{
	unsigned i;
	for (i = 0; tokens[i]; i++) {
		wat_safe_free(tokens[i]);
	}
}

wat_status_t wat_cmd_register(wat_span_t *span, const char *prefix, wat_cmd_notify_func func)
{
	int i;
	wat_status_t status = WAT_FAIL;
	wat_notify_t *new_notify = NULL;	
	wat_iterator_t *iter = NULL;
	wat_iterator_t *curr = NULL;

	/* Check if there is already a notify callback set for this prefix first */
	iter = wat_span_get_notify_iterator(span, iter);
	for (curr = iter; curr; curr = wat_iterator_next(curr)) {
		wat_notify_t *notify = wat_iterator_current(curr);
		if (!strcmp(notify->prefix, prefix)) {
			/* Overwrite existing notify */
			wat_log_span(span, WAT_LOG_INFO, "Already had a notifier for prefix %s\n", prefix);

			notify->func = func;
			status = WAT_SUCCESS;
		}
	}

	/* TODO: Create a function: wat_span_get_free_notify that returns a pointer to a
		free location */
	for (i = 1; i < sizeof(span->notifys)/sizeof(span->notifys[0]); i++) {
		if (!span->notifys[i]) {
			new_notify = wat_calloc(1, sizeof(*new_notify));
			wat_assert_return(new_notify, WAT_FAIL, "Failed to alloc memory\n");

			new_notify->prefix = wat_strdup(prefix);
			new_notify->func = func;
			
			span->notifys[i] = new_notify;
			span->notify_count++;
			status = WAT_SUCCESS;
			goto done;
		}
	}

	wat_log(WAT_LOG_CRIT, "Failed to register new notifier, no space left in notify list\n");

done:
	wat_iterator_free(iter);
	return status;
}



WAT_DECLARE(wat_iterator_t *) wat_get_iterator(wat_iterator_type_t type, wat_iterator_t *iter)
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

WAT_DECLARE(wat_iterator_t *) wat_span_get_call_iterator(const wat_span_t *span, wat_iterator_t *iter)
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

WAT_DECLARE(wat_iterator_t *) wat_span_get_sms_iterator(const wat_span_t *span, wat_iterator_t *iter)
{
	if (!(iter = wat_get_iterator(WAT_ITERATOR_SMSS, iter))) {
		return NULL;
	}
	iter->index = 1;
	
	while(iter->index <= sizeof(span->smss)/sizeof(span->smss[0])) {
		/* Could have empty pointers in the middle of array, so find the next
			one that's not empty */
		if (span->calls[iter->index]) {
			break;
		}
		iter->index++;
	}

	if (!span->smss[iter->index]) {
		wat_safe_free(iter);
		return NULL;
	}
	
	iter->span = span;
	return iter;
}

WAT_DECLARE(wat_iterator_t *) wat_span_get_notify_iterator(const wat_span_t *span, wat_iterator_t *iter)
{
	if (!(iter = wat_get_iterator(WAT_ITERATOR_NOTIFYS, iter))) {
		return NULL;
	}
	iter->index = 1;

	if (!span->notifys[iter->index]) {
		/* If the first element in the array is NULL, there are no elements
			in the array, return NULL */
		wat_safe_free(iter);
		return NULL;
	}
	iter->span = span;
	return iter;
}


WAT_DECLARE(wat_iterator_t *) wat_iterator_next(wat_iterator_t *iter)
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
		case WAT_ITERATOR_SMSS:
			wat_assert_return(iter->index, NULL, "smss iterator index cannot be zero!\n");
			while (iter->index < sizeof(iter->span->smss)/sizeof(iter->span->smss[0])) {
				iter->index++;
				if (iter->span->smss[iter->index]) {
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

WAT_DECLARE(void *) wat_iterator_current(wat_iterator_t *iter)
{
	wat_assert_return(iter && iter->type, NULL, "Invalid iterator\n");

	switch (iter->type) {
		case WAT_ITERATOR_CALLS:
			wat_assert_return(iter->index, NULL, "calls iterator index cannot be zero!\n");
			wat_assert_return(iter->index <= sizeof(iter->span->calls)/sizeof(iter->span->calls[0]), NULL, "channel iterator index bigger than calls size!\n");
			return iter->span->calls[iter->index];
		case WAT_ITERATOR_SMSS:
			wat_assert_return(iter->index, NULL, "smss iterator index cannot be zero!\n");
			wat_assert_return(iter->index <= sizeof(iter->span->smss)/sizeof(iter->span->smss[0]), NULL, "channel iterator index bigger than sms size!\n");
			return iter->span->smss[iter->index];
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

WAT_DECLARE(wat_status_t) wat_iterator_free(wat_iterator_t *iter)
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

WAT_DECLARE(wat_status_t) wat_span_call_create(wat_span_t *span, wat_call_t **incall, uint8_t call_id)
{
	uint32_t id;
	wat_call_t *call = NULL;

	if (call_id) {
		if (span->calls[call_id]) {
			return WAT_EBUSY;
		}
		id = call_id;
	} else {
		id = span->last_call_id + 1;
		
		while (id != span->last_call_id) {
			if (span->calls[id] == NULL) {
				goto done;
			}
			if (++id == WAT_MAX_CALLS_PER_SPAN) {
				/* We skip id = 0, because id = 0 is considered invalid */
				id = 1;
			}
		}

		wat_log_span(span, WAT_LOG_CRIT, "Could not allocate a new call id\n");
		return WAT_FAIL;
	}
done:

	call = wat_calloc(1, sizeof(*call));
	wat_assert_return(call, WAT_FAIL, "Could not allocate memory for new call\n");

	if (g_debug & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "[id:%d]Created new call p:%p\n", id, call);
	}

	span->calls[id] = call;
	call->span = span;
	call->id = id;
	call->state = WAT_CALL_STATE_IDLE;
	*incall = call;

	return WAT_SUCCESS;
}

WAT_DECLARE(void) wat_span_call_destroy(wat_call_t **incall)
{
	wat_call_t *call;
	wat_span_t *span;
	wat_assert_return_void(incall, "Call was null");
	wat_assert_return_void(*incall, "Call was null");
	wat_assert_return_void((*incall)->span, "Call had no span");
	
	call = *incall;
	*incall = NULL;
	span = call->span;

	if (!span->calls[call->id]) {
		wat_log_span(span, WAT_LOG_CRIT, "Could not find call to destroy inside span (id:%d)\n", call->id);
	} else {
		span->calls[call->id] = NULL;
	}

	if (g_debug & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Destroyed call with id:%d p:%p\n", call->id, call);
	}

	wat_safe_free(call);
	return;
}

WAT_DECLARE(wat_call_t *) wat_span_get_call_by_state(wat_span_t *span, wat_call_state_t state)
{
	wat_call_t *call = NULL;
	wat_iterator_t *iter;
	wat_iterator_t *curr;

	iter = wat_span_get_call_iterator(span, NULL);

	for (curr = iter; curr; curr = wat_iterator_next(curr)) {
		if (((wat_call_t*)wat_iterator_current(curr))->state == state) {
			call = (wat_call_t*)wat_iterator_current(curr);
			break;
		}
	}

	wat_iterator_free(iter);
	return call;
}

WAT_DECLARE(wat_status_t) wat_call_set_state(wat_call_t *call, wat_call_state_t new_state)
{
	wat_span_t *span = call->span;

	/* TODO: Implement state table for allowable state changes */
	if (g_debug & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "[id:%d]State change from %s to %s\n", call->id, wat_call_state2str(call->state), wat_call_state2str(new_state));
	}
	call->state = new_state;

	switch(call->state) {
		case WAT_CALL_STATE_DIALING:
			{
				if (call->dir == WAT_CALL_DIRECTION_INCOMING) {
					/* schedule a CLCC, we may or may not get a CLIP right after CRING */
					wat_sched_timer(span->sched, "clip_timeout", span->config.timeout_cid_num, wat_scheduled_clcc, (void*) call, &call->timeouts[WAT_TIMEOUT_CLIP]);
				} else {
					char cmd[40];
					memset(cmd, 0, sizeof(cmd));

					sprintf(cmd, "ATD%s;", call->called_num.digits);
					wat_cmd_enqueue(span, cmd, wat_response_atd, call);
					wat_sched_timer(span->sched, "progress_monitor", span->config.progress_poll_interval, wat_scheduled_clcc, (void*) call, &call->timeouts[WAT_PROGRESS_MONITOR]);
				}
			}
			break;
		case WAT_CALL_STATE_DIALED:
			{
				if (call->dir == WAT_CALL_DIRECTION_INCOMING) {
					/* Notify the user of the call */
					wat_con_event_t con_event;
					
					memset(&con_event, 0, sizeof(con_event));

					con_event.type = call->type;
					con_event.sub = WAT_CALL_SUB_REAL;	/* hard coded for now */
					memcpy(&con_event.calling_num, &call->calling_num, sizeof(call->calling_num));

					if (g_interface.wat_con_ind) {
						g_interface.wat_con_ind(span->id, call->id, &con_event);
					}
				} else {
					/* Nothing to do */
				}
			}
			break;
		case WAT_CALL_STATE_RINGING:
			{
				wat_con_status_t con_status;
				memset(&con_status, 0, sizeof(con_status));

				con_status.type = WAT_CON_STATUS_TYPE_RINGING;

				if (g_interface.wat_con_sts) {
					g_interface.wat_con_sts(span->id, call->id, &con_status);
				}
			}
			break;
		case WAT_CALL_STATE_ANSWERED:
			if (call->dir == WAT_CALL_DIRECTION_INCOMING) {
				wat_cmd_enqueue(span, "ATA", wat_response_ata, call);
			} else {
				wat_con_status_t con_status;
				memset(&con_status, 0, sizeof(con_status));

				con_status.type = WAT_CON_STATUS_TYPE_ANSWER;

				if (g_interface.wat_con_sts) {
					g_interface.wat_con_sts(span->id, call->id, &con_status);
				}
				wat_call_set_state(call, WAT_CALL_STATE_UP);
			}
			break;
		case WAT_CALL_STATE_UP:
			/* Do nothing for now */
			break;
		case WAT_CALL_STATE_TERMINATING:
			{
				wat_rel_event_t rel_event;
				memset(&rel_event, 0, sizeof(rel_event));
				rel_event.cause = WAT_CALL_HANGUP_CAUSE_NORMAL;

				if (g_interface.wat_rel_ind) {
					g_interface.wat_rel_ind(span->id, call->id, &rel_event);
				}
			}
			break;
		case WAT_CALL_STATE_TERMINATING_CMPL:
			{
				wat_span_call_destroy(&call);
			}
			break;
		case WAT_CALL_STATE_HANGUP:
			{
				wat_cmd_enqueue(span, "ATH", wat_response_ath, call);
			}
			break;
		case WAT_CALL_STATE_HANGUP_CMPL:
			{
				wat_cmd_status_t cmd_status;
				memset(&cmd_status, 0, sizeof(cmd_status));
				
				if (g_interface.wat_rel_cfm) {
					g_interface.wat_rel_cfm(span->id, call->id);
				}
				wat_span_call_destroy(&call);
			}
			break;
		default:
			wat_log(WAT_LOG_CRIT, "Unhandled state change\n");
	}
	
	return WAT_SUCCESS;
}
