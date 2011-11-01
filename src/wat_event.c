/*
 * libwat: Wireless AT commands library
 *
 * Written by David Yat Sin <dyatsin@sangoma.com>
 *
 * Copyright (C) 2011, Sangoma Technologies.
 * All Rights Reserved.
 */

/*
 * Please do not directly contact any of the maintainers
 * of this project for assistance; the project provides a web
 * site, mailing lists and IRC channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 * In addition, when this program is distributed with Asterisk in
 * any form that would qualify as a 'combined work' or as a
 * 'derivative work' (but not mere aggregation), you can redistribute
 * and/or modify the combination under the terms of the license
 * provided with that copy of Asterisk, instead of the license
 * terms granted here.
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
		cmd_status.success = 0;

		if (status == WAT_EBUSY) {
			wat_log(WAT_LOG_CRIT, "s%d:[id:%d]Call with this ID already exists\n", span->id, event->call_id);
			cmd_status.reason = WAT_STATUS_REASON_CALL_ID_INUSE;
		} else {
			wat_log(WAT_LOG_CRIT, "s%d:[id:%d] Failed to allocate new call\n", span->id, event->call_id);
			cmd_status.reason = WAT_STATUS_REASON_NO_MEM;
		}
				
		if (g_interface.wat_rel_cfm) {
			g_interface.wat_rel_cfm(span->id, call->id, &cmd_status);
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
