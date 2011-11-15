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

#if 1
//uint32_t	g_debug = WAT_DEBUG_UART_RAW | WAT_DEBUG_UART_DUMP | WAT_DEBUG_AT_PARSE;
//uint32_t	g_debug = WAT_DEBUG_UART_DUMP | WAT_DEBUG_AT_PARSE;
//uint32_t	g_debug = WAT_DEBUG_UART_DUMP | WAT_DEBUG_AT_HANDLE;
uint32_t	g_debug = WAT_DEBUG_AT_HANDLE;

//uint32_t	g_debug = WAT_DEBUG_UART_DUMP | WAT_DEBUG_AT_PARSE | WAT_DEBUG_CALL_STATE | WAT_DEBUG_AT_HANDLE;
#else
uint32_t	g_debug = 0;
#endif

wat_interface_t g_interface;
wat_span_t g_spans[WAT_MAX_SPANS];

WAT_ENUM_NAMES(WAT_MODULETYPE_NAMES, WAT_MODULETYPE_STRINGS)
WAT_STR2ENUM(wat_str2wat_moduletype, wat_moduletype2str, wat_moduletype_t, WAT_MODULETYPE_NAMES, WAT_MODULE_INVALID)

WAT_ENUM_NAMES(WAT_EVENT_NAMES, WAT_EVENT_STRINGS)
WAT_STR2ENUM(wat_str2wat_event, wat_event2str, wat_event_id_t, WAT_EVENT_NAMES, WAT_EVENT_INVALID)

WAT_ENUM_NAMES(WAT_NET_STAT_NAMES, WAT_NET_STAT_STRINGS)
WAT_STR2ENUM(wat_str2wat_net_stat, wat_net_stat2str, wat_net_stat_t, WAT_NET_STAT_NAMES, WAT_NET_INVALID)

WAT_ENUM_NAMES(WAT_CSQ_BER_NAMES, WAT_CSQ_BER_STRINGS)
WAT_STR2ENUM(wat_str2wat_csq_ber, wat_csq_ber2str, wat_csq_ber_t, WAT_CSQ_BER_NAMES, WAT_CSQ_BER_NOT_DETECTABLE)

WAT_ENUM_NAMES(WAT_CALL_TYPE_NAMES, WAT_CALL_TYPE_STRINGS)
WAT_STR2ENUM(wat_str2wat_call_type, wat_call_type2str, wat_call_type_t, WAT_CALL_TYPE_NAMES, WAT_CALL_TYPE_INVALID)

WAT_ENUM_NAMES(WAT_CALL_SUB_NAMES, WAT_CALL_SUB_STRINGS)
WAT_STR2ENUM(wat_str2wat_call_sub, wat_call_sub2str, wat_call_sub_t, WAT_CALL_SUB_NAMES, WAT_CALL_SUB_INVALID)

WAT_ENUM_NAMES(WAT_NUMBER_TYPE_NAMES, WAT_NUMBER_TYPE_STRINGS)
WAT_STR2ENUM(wat_str2wat_number_type, wat_number_type2str, wat_number_type_t, WAT_NUMBER_TYPE_NAMES, WAT_NUMBER_TYPE_INVALID)

WAT_ENUM_NAMES(WAT_NUMBER_PLAN_NAMES, WAT_NUMBER_PLAN_STRINGS)
WAT_STR2ENUM(wat_str2wat_number_plan, wat_number_plan2str, wat_number_plan_t, WAT_NUMBER_PLAN_NAMES, WAT_NUMBER_PLAN_INVALID)

WAT_ENUM_NAMES(WAT_NUMBER_VALIDITY_NAMES, WAT_NUMBER_VALIDITY_STRINGS)
WAT_STR2ENUM(wat_str2wat_number_validity, wat_number_validity2str, wat_number_validity_t, WAT_NUMBER_VALIDITY_NAMES, WAT_NUMBER_VALIDITY_INVALID)

WAT_ENUM_NAMES(WAT_CALL_STATE_NAMES, WAT_CALL_STATE_STRINGS)
WAT_STR2ENUM(wat_str2wat_call_state, wat_call_state2str, wat_call_state_t, WAT_CALL_STATE_NAMES, WAT_CALL_STATE_INVALID)

WAT_ENUM_NAMES(WAT_SMS_STATE_NAMES, WAT_SMS_STATE_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_state, wat_sms_state2str, wat_sms_state_t, WAT_SMS_STATE_NAMES, WAT_SMS_STATE_INVALID)

WAT_ENUM_NAMES(WAT_DIRECTION_NAMES, WAT_DIRECTION_STRINGS)
WAT_STR2ENUM(wat_str2wat_direction, wat_direction2str, wat_direction_t, WAT_DIRECTION_NAMES, WAT_DIRECTION_INVALID)

static wat_span_t *wat_get_span(uint8_t span_id);

WAT_DECLARE(void) wat_version(uint8_t *current, uint8_t *revision, uint8_t *age)
{
	*current = wat_VERSION_LT_CURRENT;
	*revision = wat_VERSION_LT_REVISION;
	*age = wat_VERSION_LT_AGE;
}


WAT_DECLARE(wat_status_t) wat_register(wat_interface_t *interface)
{
	memset(g_spans, 0, sizeof(g_spans));

	if (!interface->wat_log ||
		!interface->wat_malloc ||
		!interface->wat_calloc ||
		!interface->wat_free) {
		return WAT_FAIL;
	}

	
#if 0 /* Put warnings here instead */
	wat_assert_return(interface->wat_sigstatus_change, WAT_FAIL, "No wat_sigstatus_change callback\n");
	wat_assert_return(interface->wat_span_write, WAT_FAIL, "No wat_span_write callback\n");
	wat_assert_return(interface->wat_log, WAT_FAIL, "No wat_log callback\n");
	wat_assert_return(interface->wat_calloc, WAT_FAIL, "No wat_calloc callback\n");
	wat_assert_return(interface->wat_malloc, WAT_FAIL, "No wat_malloc callback\n");
	wat_assert_return(interface->wat_safe_free, WAT_FAIL, "No wat_safe_free callback\n");
	wat_assert_return(interface->wat_alarm, WAT_FAIL, "No wat_alarm callback\n");
	wat_assert_return(interface->wat_con_ind, WAT_FAIL, "No wat_con_ind callback\n");
	wat_assert_return(interface->wat_con_cfm, WAT_FAIL, "No wat_con_cfm callback\n");
	wat_assert_return(interface->wat_rel_ind, WAT_FAIL, "No wat_rel_ind callback\n");
	wat_assert_return(interface->wat_rel_cfm, WAT_FAIL, "No wat_rel_cfm callback\n");
	wat_assert_return(interface->wat_sms_ind, WAT_FAIL, "No wat_sms_ind callback\n");
	wat_assert_return(interface->wat_sms_cfm, WAT_FAIL, "No wat_sms_cfm callback\n");
	wat_assert_return(interface->wat_cmd_cfm, WAT_FAIL, "No wat_cmd_cfm callback\n");
#endif

	memcpy(&g_interface, interface, sizeof(*interface));

	wat_log(WAT_LOG_DEBUG, "General interface registered\n");
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_span_config(uint8_t span_id, wat_span_config_t *span_config)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");
	
	if (span->configured) {
		wat_log_span(span, WAT_LOG_ERROR, "Span was already configured\n");
		return WAT_FAIL;
	}

	switch(span_config->moduletype) {
		case WAT_MODULE_TELIT:
			if (telit_init(span) != WAT_SUCCESS) {
				goto failed;
			}
			break;
		default:
			wat_log_span(span, WAT_LOG_ERROR, "Invalid module type\n", span_config->moduletype);
			return WAT_EINVAL;
	}
	
	span->id = span_id;
	span->configured = 1;
	memcpy(&span->config, span_config, sizeof(*span_config));

	wat_log_span(span, WAT_LOG_DEBUG, "Configured span for %s module\n", wat_moduletype2str(span_config->moduletype));
	return WAT_SUCCESS;

failed:

	wat_log_span(span, WAT_LOG_ERROR, "Failed to configure span for %s module\n", span_id, wat_moduletype2str(span_config->moduletype));

	return WAT_FAIL;
}

WAT_DECLARE(wat_status_t) wat_span_unconfig(uint8_t span_id)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");
	
	if (span->configured) {
		wat_log_span(span, WAT_LOG_ERROR, "Span was not configured\n");
		return WAT_FAIL;
	}
	if (span->running) {
		wat_log_span(span, WAT_LOG_ERROR, "Cannot unconfig running span. Please stop span first\n");
		return WAT_FAIL;
	}

	memset(&g_spans[span_id], 0, sizeof(g_spans[0]));
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_span_start(uint8_t span_id)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");
	
	if (span->running) {
		wat_log_span(span, WAT_LOG_ERROR, "Span was already started\n");
		return WAT_FAIL;
	}

	span->running = 1;

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

	if (wat_queue_create(&span->sms_queue, WAT_SMS_QUEUE_SZ) != WAT_SUCCESS) {
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

	/* Enable Calling Line Presentation */
	wat_cmd_enqueue(span, "AT+CLIP=1", wat_response_clip, NULL);

	/* Enable New Message Indications To TE */
	wat_cmd_enqueue(span, "AT+CNMI=2,2", wat_response_cnmi, NULL);
	//wat_cmd_enqueue(span, "AT+CNMI=2,2,0,0,0", wat_response_cnmi, NULL);

	/* Set Operator mode */
	wat_cmd_enqueue(span, "AT+COPS=3,0", wat_response_cops, NULL);

	/* Call module specific start here */
	span->module.start(span);

	/* Get some information about the chip */
	
	/* Get Module Manufacturer Name */
	wat_cmd_enqueue(span, "AT+CGMM", wat_response_cgmm, NULL);

	/* Get Module Manufacturer Identification */
	wat_cmd_enqueue(span, "AT+CGMI", wat_response_cgmi, NULL);

	/* Get Module Revision Identification */
	wat_cmd_enqueue(span, "AT+CGMR", wat_response_cgmr, NULL);

	/* Get Module Serial Number */
	wat_cmd_enqueue(span, "AT+CGSN", wat_response_cgsn, NULL);

	/* Get Module IMSI */
	wat_cmd_enqueue(span, "AT+CIMI", wat_response_cimi, NULL);

	/* Own Number */
	wat_cmd_enqueue(span, "AT+CNUM", wat_response_cnum, NULL);

	/* Signal Quality */
	wat_cmd_enqueue(span, "AT+CSQ", wat_response_csq, NULL);

	/* Enable Network Registration Unsolicited result code */
	wat_cmd_enqueue(span, "AT+CREG=1", NULL, NULL);

	/* Check Registration Status in case module is already registered */
	wat_cmd_enqueue(span, "AT+CREG?", wat_response_creg, NULL);
	
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_span_stop(uint8_t span_id)
{
	wat_span_t *span;
	wat_iterator_t *iter = NULL;
	wat_iterator_t *curr = NULL;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");
	
	if (!span->running) {
		wat_log_span(span, WAT_LOG_ERROR, "Span was not running\n");
		return WAT_FAIL;
	}

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

	span->running = 0;
	return WAT_SUCCESS;
}

WAT_DECLARE(uint32_t) wat_span_schedule_next(uint8_t span_id)
{
	wat_span_t *span;
	int32_t timeto = -1;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	if (!span->running) {
		return -1;
	}

	if (!span->cmd_busy && wat_queue_empty(span->cmd_queue) == WAT_FALSE) {
		return 0;
	}

	if (wat_queue_empty(span->event_queue) == WAT_FALSE) {
		return 0;
	}

	if (wat_queue_empty(span->sms_queue) == WAT_FALSE) {
		return 0;
	}

	if (wat_sched_get_time_to_next_timer(span->sched, &timeto) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to get time to next event\n", span->id);
		timeto = -1;
	}
	return timeto;
}

WAT_DECLARE(void) wat_span_run(uint8_t span_id)
{	
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return_void(span, "Invalid span");

	/* Check if there are pending events requested by the user */
	wat_span_run_events(span);

	/* Check if there are pending commands received from the chip */
	wat_span_run_cmds(span);

	/* Check if there are any timeouts */
	wat_span_run_sched(span);

	/* Check if there are pending sms's requested by the user */
	wat_span_run_smss(span);
	return;
}

WAT_DECLARE(void) wat_span_process_read(uint8_t span_id, void *data, uint32_t len)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return_void(span, "Invalid span");

	if (g_debug & WAT_DEBUG_UART_RAW) {
		char mydata[WAT_MAX_CMD_SZ];
		wat_log_span(span, WAT_LOG_DEBUG, "[RX RAW] %s (len:%d)\n", format_at_data(mydata, data, len), len);
	}

	if (wat_buffer_enqueue(span->buffer, data, len) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enqueue\n");
	}
	return;
}

WAT_DECLARE(wat_status_t) wat_span_get_chip_info(uint8_t span_id,
					char *manufacturer_name, wat_size_t len_manufacturer_name,
					char *manufacturer_id, wat_size_t len_manufacturer_id,
					char *revision_id, wat_size_t len_revision_id,
					char *serial_number, wat_size_t len_serial_number,
					char *imsi, wat_size_t len_imsi,
					char *subscriber_number, wat_size_t len_subscriber_number)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	strncpy(manufacturer_name, span->manufacturer_name, len_manufacturer_name);
	strncpy(manufacturer_id, span->manufacturer_id, len_manufacturer_id);
	strncpy(revision_id, span->revision_id, len_revision_id);
	strncpy(serial_number, span->serial_number, len_serial_number);
	strncpy(imsi, span->imsi, len_imsi);
	strncpy(subscriber_number, span->subscriber_number, len_subscriber_number);

	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_span_get_netinfo(uint8_t span_id, char *net_info, wat_size_t len)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START
			
	if (!span->running) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	snprintf(net_info, len, "%s", wat_net_stat2str(span->net_info.stat));
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_span_get_signal_quality(uint8_t span_id, char *strength, wat_size_t len_strength, char *ber, wat_size_t len_ber)
{
	char dest[30];
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START

	if (!span->running) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	snprintf(ber, len_ber, wat_csq_ber2str(span->net_info.ber));
	snprintf(strength, len_strength, "%s", wat_decode_csq_rssi(dest, span->net_info.rssi));

	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_con_cfm(uint8_t span_id, uint8_t call_id)
{
	wat_span_t *span;
	wat_event_t event;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");
	
	WAT_SPAN_FUNC_DBG_START

	if(!call_id) {
		WAT_FUNC_DBG_END
		return WAT_EINVAL;
	}

	if (!span->running) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	memset(&event, 0, sizeof(event));
	
	event.id = WAT_EVENT_CON_CFM;
	event.call_id = call_id;

	wat_event_enqueue(span, &event);
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_con_req(uint8_t span_id, uint8_t call_id, wat_con_event_t *con_event)
{
	wat_span_t *span;
	wat_event_t event;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START

	if ((call_id < 8) || call_id >= WAT_MAX_CALLS_PER_SPAN) {
		wat_log_span(span, WAT_LOG_ERROR, "[id:%d]Invalid outbound call_id\n", call_id);
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	if(!call_id) {
		WAT_FUNC_DBG_END
		return WAT_EINVAL;
	}

	if (!span->running) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	memset(&event, 0, sizeof(event));
	
	event.id = WAT_EVENT_CON_REQ;
	event.call_id = call_id;

	memcpy(&event.data.con_event, con_event, sizeof(*con_event));
	wat_event_enqueue(span, &event);
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_rel_cfm(uint8_t span_id, uint8_t call_id)
{
	wat_span_t *span;
	wat_event_t event;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START

	if(!call_id) {
		WAT_FUNC_DBG_END
		return WAT_EINVAL;
	}

	if (!span->running) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	memset(&event, 0, sizeof(event));
	
	event.id = WAT_EVENT_REL_CFM;
	event.call_id = call_id;

	wat_event_enqueue(span, &event);
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_rel_req(uint8_t span_id, uint8_t call_id)
{
	wat_span_t *span;
	wat_event_t event;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START

	if(!call_id) {
		WAT_FUNC_DBG_END
		return WAT_EINVAL;
	}

	if (!span->running) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	memset(&event, 0, sizeof(event));
	
	event.id = WAT_EVENT_REL_REQ;
	event.call_id = call_id;

	wat_event_enqueue(span, &event);
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_sms_req(uint8_t span_id, uint8_t sms_id, wat_sms_event_t *sms_event)
{
	wat_span_t *span;
	wat_event_t event;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START

	if(!sms_id) {
		WAT_FUNC_DBG_END
		return WAT_EINVAL;
	}

	if (!span->running) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	memset(&event, 0, sizeof(event));
	
	event.id = WAT_EVENT_SMS_REQ;
	event.sms_id = sms_id;
	memcpy(&event.data.sms_event, sms_event, sizeof(*sms_event));

	wat_event_enqueue(span, &event);
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

wat_span_t *wat_get_span(uint8_t span_id)
{
	wat_span_t *span;
	if (!span_id || span_id >= WAT_MAX_SPANS) {
		return NULL;
	}
	span = &g_spans[span_id];
	if (!span) {
		return NULL;
	}
	return span;
}

int wat_span_write(wat_span_t *span, void *data, uint32_t len)
{
	int res;
	
	if (g_debug & WAT_DEBUG_UART_RAW) {		
		char mydata[WAT_MAX_CMD_SZ];
		char *cmd = (char *)data;
		wat_log_span(span, WAT_LOG_DEBUG, "[TX RAW] %s (len:%d)\n", format_at_data(mydata, cmd, strlen(cmd)), strlen(cmd));
	}

	res = g_interface.wat_span_write(span->id, data, len);
	if (res < len) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to write to span (wrote:%d len:%d)\n", res, len);
		return WAT_FAIL;
	}
	return res;
}

wat_status_t wat_module_register(wat_span_t *span, wat_module_t *module)
{
	memcpy(&span->module, module, sizeof(*module));
	return WAT_SUCCESS;
}

void *wat_malloc(wat_size_t size)
{
	wat_assert_return(g_interface.wat_malloc, NULL, "No callback for malloc specified\n");
	return g_interface.wat_malloc(size);
}

void *wat_calloc(wat_size_t nmemb, wat_size_t size)
{
	wat_assert_return(g_interface.wat_calloc, NULL, "No callback for calloc specified\n");
	return g_interface.wat_calloc(nmemb, size);
}

void wat_free(void *ptr)
{
	wat_assert_return(g_interface.wat_free, , "No callback for free specified\n");
	g_interface.wat_free(ptr);
}

char *wat_strdup(const char *str)
{
	wat_size_t len = strlen(str) + 1;
	void *new = wat_calloc(1, len);
	if (!new) {
		return NULL;
	}
	return (char *) memcpy(new, str, len);
}

wat_status_t wat_span_update_sig_status(wat_span_t *span, wat_bool_t up)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Signalling status changed to %s\n", up ? "Up": "Down");

	span->sigstatus = up ? WAT_SIGSTATUS_UP: WAT_SIGSTATUS_DOWN;

	if (g_interface.wat_sigstatus_change) {
		g_interface.wat_sigstatus_change(span->id, span->sigstatus);
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

