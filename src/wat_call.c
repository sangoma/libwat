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

wat_status_t wat_span_call_create(wat_span_t *span, wat_call_t **incall, uint8_t call_id, wat_direction_t dir)
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

	if (span->config.debug_mask & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "[id:%d]Created new call p:%p\n", id, call);
	}

	span->calls[id] = call;
	call->span = span;
	call->id = id;
	call->dir = dir;
	call->state = WAT_CALL_STATE_INIT;
	*incall = call;

	return WAT_SUCCESS;
}

void wat_span_call_destroy(wat_call_t **incall)
{
	wat_call_t *call = NULL;
	wat_span_t *span = NULL;
	wat_assert_return_void(incall, "Call was null");
	wat_assert_return_void(*incall, "Call was null");
	wat_assert_return_void((*incall)->span, "Call had no span");
	
	call = *incall;
	*incall = NULL;
	span = call->span;

	/* The call we're about to destroy could be related to scheduled timers in its span, find them if any and cancel them */
	wat_sched_cancel_timers_by_data(span->sched, call);

	if (!span->calls[call->id]) {
		wat_log_span(span, WAT_LOG_CRIT, "Could not find call to destroy inside span (id:%d)\n", call->id);
	} else {
		span->calls[call->id] = NULL;
	}

	if (span->config.debug_mask & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Destroyed call with id:%d p:%p\n", call->id, call);
	}

	wat_safe_free(call);
	return;
}

wat_call_t *wat_span_get_call_by_state(wat_span_t *span, wat_call_state_t state)
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

wat_status_t _wat_call_set_state(const char *func, int line, wat_call_t *call, wat_call_state_t new_state)
{
	wat_span_t *span = call->span;

	/* TODO: Implement state table for allowable state changes */
	if (span->config.debug_mask & WAT_DEBUG_CALL_STATE) {
		wat_log_span(span, WAT_LOG_DEBUG, "[id:%d] Call State change from %s to %s\n", call->id, wat_call_state2str(call->state), wat_call_state2str(new_state), func, line);
	}
	call->state = new_state;

	switch(call->state) {
		case WAT_CALL_STATE_DIALING:
		{
			if (call->dir == WAT_DIRECTION_INCOMING) {
				/* schedule a CLCC, we may or may not get a CLIP right after CRING */
				wat_sched_timer(span->sched, "clip_timeout", span->config.timeout_cid_num, wat_scheduled_clcc, (void*) call, &span->timeouts[WAT_TIMEOUT_CLIP]);
			} else {
				char cmd[40];
				memset(cmd, 0, sizeof(cmd));

				sprintf(cmd, "ATD%s; ", call->called_num.digits);
				
				wat_cmd_enqueue(span, cmd, wat_response_atd, call, 15000);

				wat_sched_timer(span->sched, "progress_monitor", span->config.progress_poll_interval, wat_scheduled_clcc, (void*) call, &span->timeouts[WAT_PROGRESS_MONITOR]);
			}
		}
		break;
		case WAT_CALL_STATE_DIALED:
		{
			if (call->dir == WAT_DIRECTION_INCOMING) {
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
			if (call->dir == WAT_DIRECTION_INCOMING) {
				wat_cmd_enqueue(span, "ATA", wat_response_ata, call, 30000);
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
			wat_cmd_enqueue(span, "ATH", wat_response_ath, call, 30000);
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

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

/******************************************************************************/
