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

wat_event_handler_t event_handlers[] =  {
	{WAT_EVENT_CON_REQ, wat_event_con_req},
	{WAT_EVENT_CON_CFM, wat_event_con_cfm},
	{WAT_EVENT_REL_REQ, wat_event_rel_req},
	{WAT_EVENT_REL_CFM, wat_event_rel_cfm},	
	/* Should be last handler in the list */
	{WAT_EVENT_INVALID, NULL},
};

WAT_EVENT_FUNC(wat_event_con_req)
{
	wat_status_t status;
	wat_call_t *call = NULL;
	WAT_SPAN_FUNC_DBG_START

	status = wat_span_call_create(span, &call, event->call_id);
	if (status != WAT_SUCCESS) {
		wat_cmd_status_t cmd_status;

		memset(&cmd_status, 0, sizeof(cmd_status));
		cmd_status.success = WAT_FALSE;

		if (g_interface.wat_rel_cfm) {
			g_interface.wat_rel_cfm(span->id, call->id);
		}
		return;
	}

	call->dir = WAT_CALL_DIRECTION_OUTGOING;
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
		wat_log(WAT_LOG_CRIT, "s%d:[id:%d]Failed to find call\n", span->id, event->call_id);
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
		wat_log(WAT_LOG_CRIT, "s%d:[id:%d]Failed to find call\n", span->id, event->call_id);
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
		wat_log(WAT_LOG_CRIT, "s%d:[id:%d]Failed to find call\n", span->id, event->call_id);
		WAT_FUNC_DBG_END
		return;
	}

	wat_call_set_state(call, WAT_CALL_STATE_TERMINATING_CMPL);

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
