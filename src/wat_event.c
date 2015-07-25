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

WAT_EVENT_FUNC(wat_event_con_req);
WAT_EVENT_FUNC(wat_event_con_cfm);
WAT_EVENT_FUNC(wat_event_rel_req);
WAT_EVENT_FUNC(wat_event_rel_cfm);
WAT_EVENT_FUNC(wat_event_sms_req);

wat_event_handler_t event_handlers[] =  {
	{WAT_EVENT_CON_REQ, wat_event_con_req},
	{WAT_EVENT_CON_CFM, wat_event_con_cfm},
	{WAT_EVENT_REL_REQ, wat_event_rel_req},
	{WAT_EVENT_REL_CFM, wat_event_rel_cfm},
	{WAT_EVENT_SMS_REQ, wat_event_sms_req},
	/* Should be last handler in the list */
	{WAT_EVENT_INVALID, NULL},
};

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

WAT_EVENT_FUNC(wat_event_con_req)
{
	wat_status_t status;
	wat_call_t *call = NULL;
	WAT_SPAN_FUNC_DBG_START

	status = wat_span_call_create(span, &call, event->call_id, WAT_DIRECTION_OUTGOING);
	if (status != WAT_SUCCESS) {
		if (g_interface.wat_rel_cfm) {
			g_interface.wat_rel_cfm(span->id, event->call_id);
		}
		return;
	}

	memcpy(&call->called_num, &event->data.con_event.called_num, sizeof(call->called_num));

	wat_call_set_state(call, WAT_CALL_STATE_DIALING);

	WAT_FUNC_DBG_END
	return;
}

WAT_EVENT_FUNC(wat_event_con_cfm)
{
	wat_call_t *call = NULL;
	WAT_SPAN_FUNC_DBG_START

	call = span->calls[event->call_id];
	if (!call) {
		wat_log_span(span, WAT_LOG_CRIT, "[id:%d]Failed to find call\n", event->call_id);
		WAT_FUNC_DBG_END
		return;
	}

	wat_call_set_state(call, WAT_CALL_STATE_ANSWERED);

	WAT_FUNC_DBG_END
	return;
}

WAT_EVENT_FUNC(wat_event_rel_req)
{
	wat_call_t *call = NULL;
	WAT_SPAN_FUNC_DBG_START

	call = span->calls[event->call_id];
	if (!call) {
		wat_log_span(span, WAT_LOG_CRIT, "[id:%d]Failed to find call\n", event->call_id);
		WAT_FUNC_DBG_END
		return;
	}

	if (call->state >= WAT_CALL_STATE_HANGUP) {
		wat_log_span(span, WAT_LOG_DEBUG, "[id:%d] Call was already in hangup procedure\n", event->call_id);
		WAT_FUNC_DBG_END
		return; 
	}

	wat_call_set_state(call, WAT_CALL_STATE_HANGUP);

	WAT_FUNC_DBG_END
	return;
}


WAT_EVENT_FUNC(wat_event_rel_cfm)
{
	wat_call_t *call = NULL;
	WAT_SPAN_FUNC_DBG_START

	call = span->calls[event->call_id];
	if (!call) {
		wat_log_span(span, WAT_LOG_CRIT, "[id:%d]Failed to find call\n", event->call_id);
		WAT_FUNC_DBG_END
		return;
	}

	wat_call_set_state(call, WAT_CALL_STATE_TERMINATING_CMPL);

	WAT_FUNC_DBG_END
	return;
}

WAT_EVENT_FUNC(wat_event_sms_req)
{	
	wat_status_t status;
	wat_sms_t *sms = NULL;
	WAT_SPAN_FUNC_DBG_START

	status = wat_span_sms_create(span, &sms, event->sms_id, WAT_DIRECTION_OUTGOING);
	if (status != WAT_SUCCESS) {
		wat_sms_status_t sms_status;
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create new SMS\n");

		memset(&sms_status, 0, sizeof(sms_status));
		sms_status.success = WAT_FALSE;

		if (g_interface.wat_sms_sts) {
			g_interface.wat_sms_sts(span->id, event->sms_id, &sms_status);
		}
		return;
	}

	memcpy(&sms->sms_event, &event->data.sms_event, sizeof(sms->sms_event));

	wat_sms_set_state(sms, WAT_SMS_STATE_QUEUED);

	WAT_FUNC_DBG_END
	return;
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
