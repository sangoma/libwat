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
//uint32_t	g_debug = WAT_DEBUG_AT_HANDLE | WAT_DEBUG_SMS_DECODE;
uint32_t	g_debug = WAT_DEBUG_UART_RAW | WAT_DEBUG_UART_DUMP | WAT_DEBUG_AT_PARSE | WAT_DEBUG_CALL_STATE | WAT_DEBUG_AT_HANDLE | WAT_DEBUG_SMS_DECODE | WAT_DEBUG_SMS_ENCODE;
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

WAT_ENUM_NAMES(WAT_PIN_STAT_NAMES, WAT_PIN_STAT_STRINGS)
WAT_STR2ENUM(wat_str2wat_pin_stat, wat_pin_stat2str, wat_pin_stat_t, WAT_PIN_STAT_NAMES, WAT_PIN_INVALID)

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

WAT_ENUM_NAMES(WAT_SPAN_STATE_NAMES, WAT_SPAN_STATE_STRINGS)
WAT_STR2ENUM(wat_str2wat_span_state, wat_span_state2str, wat_span_state_t, WAT_SPAN_STATE_NAMES, WAT_SPAN_STATE_INVALID)

WAT_ENUM_NAMES(WAT_SMS_CAUSE_NAMES, WAT_SMS_CAUSE_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_cause, wat_sms_cause2str, wat_sms_cause_t, WAT_SMS_CAUSE_NAMES, WAT_SMS_CAUSE_UNKNOWN)

WAT_ENUM_NAMES(WAT_SMS_PDU_MTI_NAMES, WAT_SMS_PDU_MTI_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_pdu_mti, wat_sms_pdu_mti2str, wat_sms_pdu_mti_t, WAT_SMS_PDU_MTI_NAMES, WAT_SMS_PDU_MTI_INVALID)

WAT_ENUM_NAMES(WAT_SMS_PDU_DCS_GRP_NAMES, WAT_SMS_PDU_DCS_GRP_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_pdu_dcs_grp, wat_sms_pdu_dcs_grp2str, wat_sms_pdu_dcs_grp_t, WAT_SMS_PDU_DCS_GRP_NAMES, WAT_SMS_PDU_DCS_GRP_INVALID)

WAT_ENUM_NAMES(WAT_SMS_PDU_DCS_MESSAGE_CLASS_NAMES, WAT_SMS_PDU_DCS_MESSAGE_CLASS_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_pdu_dcs_msg_cls, wat_sms_pdu_dcs_msg_cls2str, wat_sms_pdu_dcs_msg_cls_t, WAT_SMS_PDU_DCS_MESSAGE_CLASS_NAMES, WAT_SMS_PDU_DCS_MSG_CLASS_INVALID)

WAT_ENUM_NAMES(WAT_SMS_PDU_DCS_ALPHABET_NAMES, WAT_SMS_PDU_DCS_ALPHABET_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_pdu_dcs_alphabet, wat_sms_pdu_dcs_alphabet2str, wat_sms_pdu_dcs_alphabet_t, WAT_SMS_PDU_DCS_ALPHABET_NAMES, WAT_SMS_PDU_DCS_ALPHABET_INVALID)

WAT_ENUM_NAMES(WAT_SMS_PDU_VP_NAMES, WAT_SMS_PDU_VP_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_pdu_vp_type, wat_sms_pdu_vp_type2str, wat_sms_pdu_vp_type_t, WAT_SMS_PDU_VP_NAMES, WAT_SMS_PDU_VP_INVALID)

WAT_ENUM_NAMES(WAT_SMS_CONTENT_ENCODING_NAMES, WAT_SMS_CONTENT_ENCODING_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_content_encoding, wat_sms_content_encoding2str, wat_sms_content_encoding_t, WAT_SMS_CONTENT_ENCODING_NAMES, WAT_SMS_CONTENT_ENCODING_INVALID)

WAT_ENUM_NAMES(WAT_SMS_CONTENT_CHARSET_NAMES, WAT_SMS_CONTENT_CHARSET_STRINGS)
WAT_STR2ENUM(wat_str2wat_sms_content_charset, wat_sms_content_charset2str, wat_sms_content_charset_t, WAT_SMS_CONTENT_CHARSET_NAMES, WAT_SMS_CONTENT_CHARSET_INVALID)

WAT_ENUM_NAMES(WAT_DIRECTION_NAMES, WAT_DIRECTION_STRINGS)
WAT_STR2ENUM(wat_str2wat_direction, wat_direction2str, wat_direction_t, WAT_DIRECTION_NAMES, WAT_DIRECTION_INVALID)

WAT_ENUM_NAMES(WAT_ALARM_NAMES, WAT_ALARM_STRINGS)
WAT_STR2ENUM(wat_str2wat_alarm, wat_alarm2str, wat_alarm_t, WAT_ALARM_NAMES, WAT_ALARM_INVALID)

WAT_ENUM_NAMES(WAT_BAND_NAMES, WAT_BAND_STRINGS)
WAT_STR2ENUM(wat_str2wat_band, wat_band2str, wat_band_t, WAT_BAND_NAMES, WAT_BAND_INVALID)

WAT_RESPONSE_FUNC(wat_user_cmd_response);
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
		!interface->wat_log_span ||
		!interface->wat_malloc ||
		!interface->wat_calloc ||
		!interface->wat_free ||
		!interface->wat_span_write) {
		return WAT_FAIL;
	}

	if (!interface->wat_span_sts) {
		wat_log(WAT_LOG_WARNING, "No wat_span_sts callback\n");
	}

	if (!interface->wat_con_ind) {
		wat_log(WAT_LOG_WARNING, "No wat_con_ind callback\n");
	}

	if (!interface->wat_rel_ind) {
		wat_log(WAT_LOG_WARNING, "No wat_rel_ind callback\n");
	}

	if (!interface->wat_rel_cfm) {
		wat_log(WAT_LOG_WARNING, "No wat_rel_cfm callback\n");
	}

	if (!interface->wat_sms_ind) {
		wat_log(WAT_LOG_WARNING, "No wat_sms_ind callback\n");
	}

	if (!interface->wat_sms_sts) {
		wat_log(WAT_LOG_WARNING, "No wat_sms_sts callback\n");
	}

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

	/* if the user does not know what a given timer should be, we provide defaults */
	if (!span->config.timeout_cid_num) {
		span->config.timeout_cid_num = WAT_DEFAULT_TIMEOUT_CID_NUM;
	}
	if (!span->config.timeout_command) {
		span->config.timeout_command = WAT_DEFAULT_TIMEOUT_COMMAND;
	}
	if (!span->config.timeout_wait_sim) {
		span->config.timeout_wait_sim = WAT_DEFAULT_TIMEOUT_WAIT_SIM;
	}
	
	if (!span->config.cmd_interval) {
		span->config.cmd_interval = WAT_DEFAULT_COMMAND_INTERVAL;
	}
	if (!span->config.progress_poll_interval) {
		span->config.progress_poll_interval = WAT_DEFAULT_PROGRESS_POLL_INTERVAL;
	}
	if (!span->config.signal_poll_interval) {
		span->config.signal_poll_interval = WAT_DEFAULT_SIGNAL_POLL_INTERVAL;
	}
	if (!span->config.signal_threshold) {
		span->config.signal_threshold = WAT_DEFAULT_SIGNAL_THRESHOLD;
	}

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

	if (span->state >= WAT_SPAN_STATE_START) {
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

	return wat_span_set_state(span, WAT_SPAN_STATE_START);
}

WAT_DECLARE(wat_status_t) wat_span_stop(uint8_t span_id)
{
	wat_span_t *span;
	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	return wat_span_set_state(span, WAT_SPAN_STATE_STOP);
}

WAT_DECLARE(uint32_t) wat_span_schedule_next(uint8_t span_id)
{
	wat_span_t *span;
	int32_t timeto = -1;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_FAIL, "Invalid span");

	if (span->state < WAT_SPAN_STATE_START) {
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

	if (span->state < WAT_SPAN_STATE_START) {
		wat_log(WAT_LOG_CRIT, "Span was not started\n");
		return;
	}

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

WAT_DECLARE(const wat_chip_info_t*) wat_span_get_chip_info(uint8_t span_id)
{
	wat_span_t *span;
	WAT_SPAN_FUNC_DBG_START

	span = wat_get_span(span_id);
	wat_assert_return(span, NULL, "Invalid span");

	WAT_FUNC_DBG_END
	return &span->chip_info;
}

WAT_DECLARE(const wat_sim_info_t*) wat_span_get_sim_info(uint8_t span_id)
{
	wat_span_t *span;
	
	span = wat_get_span(span_id);
	wat_assert_return(span, NULL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START
	WAT_FUNC_DBG_END
	return &span->sim_info;
}

WAT_DECLARE(const wat_net_info_t*) wat_span_get_net_info(uint8_t span_id)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, NULL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START
	WAT_FUNC_DBG_END
	return &span->net_info;
}

WAT_DECLARE(const wat_sig_info_t*) wat_span_get_sig_info(uint8_t span_id)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, NULL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START
	WAT_FUNC_DBG_END
	return &span->sig_info;
}

WAT_DECLARE(const wat_pin_stat_t*) wat_span_get_pin_info(uint8_t span_id)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, NULL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START
	WAT_FUNC_DBG_END
	return &span->pin_status;
}

WAT_DECLARE(wat_alarm_t) wat_span_get_alarms(uint8_t span_id)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, WAT_ALARM_NO_SIGNAL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START
	WAT_FUNC_DBG_END
	return span->alarm;
}

WAT_DECLARE(const char *) wat_span_get_last_error(uint8_t span_id)
{
	wat_span_t *span;

	span = wat_get_span(span_id);
	wat_assert_return(span, NULL, "Invalid span");

	WAT_SPAN_FUNC_DBG_START
	
	if (strlen(span->last_error) > 0) {
		WAT_FUNC_DBG_END
		return span->last_error;
	}
	WAT_FUNC_DBG_END
	return NULL;
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

	if (span->state < WAT_SPAN_STATE_START) {
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

	if (span->state < WAT_SPAN_STATE_RUNNING) {
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

 if (span->state < WAT_SPAN_STATE_START) {
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

WAT_DECLARE(wat_status_t) wat_span_set_dtmf_duration(uint8_t span_id, int duration_ms)
{
	char duration_cmd[WAT_MAX_CMD_SZ];
	int duration = 0;
	wat_span_t *span = NULL;
	span = wat_get_span(span_id);
	
	if (!span || span->state < WAT_SPAN_STATE_START) {
		return WAT_EINVAL;
	}

	if (duration_ms < WAT_MIN_DTMF_DURATION_MS) {
		duration_ms = WAT_MIN_DTMF_DURATION_MS;
	}
	duration = duration_ms / 10;
	snprintf(duration_cmd, sizeof(duration_cmd), "AT+VTD=%d", duration);
	wat_cmd_enqueue(span, duration_cmd, NULL, NULL);
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) wat_send_dtmf(uint8_t span_id, uint8_t call_id, const char *dtmf, wat_at_cmd_response_func cb, void *obj)
{
	char dtmf_cmd[WAT_MAX_CMD_SZ];
	if (!dtmf) {
		return WAT_EINVAL;
	}
	snprintf(dtmf_cmd, sizeof(dtmf_cmd), "AT+VTS=\"%s\"", dtmf);
	return wat_cmd_req(span_id, dtmf_cmd, cb, obj);
}

WAT_DECLARE(wat_status_t) wat_span_set_codec(uint8_t span_id, wat_codec_t codec_mask)
{
	wat_span_t *span = NULL;
	span = wat_get_span(span_id);

	if (!span || span->state < WAT_SPAN_STATE_START) {
		wat_log_span(span, WAT_LOG_ERROR, "Invalid span (unknown or not running)\n");
		return WAT_EINVAL;
	}
	return span->module.set_codec(span, codec_mask);
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

 if (span->state < WAT_SPAN_STATE_START) {
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

	if (span->state < WAT_SPAN_STATE_RUNNING) {
		WAT_FUNC_DBG_END
		return WAT_FAIL;
	}

	if (sms_event->content.len  <= 0) {
		wat_log_span(span, WAT_LOG_ERROR, "[sms:%d]SMS length cannot be 0\n", sms_id);
		WAT_FUNC_DBG_END
		return WAT_EINVAL;
	}

	switch(sms_event->content.encoding) {
		case WAT_SMS_CONTENT_ENCODING_RAW:
			switch(sms_event->pdu.dcs.alphabet) {
				case WAT_SMS_PDU_DCS_ALPHABET_DEFAULT:
				case WAT_SMS_PDU_DCS_ALPHABET_8BIT:
					if (sms_event->content.len >= WAT_MAX_SMS_SZ) {
						wat_log_span(span, WAT_LOG_ERROR, "[sms:%d]SMS length has to be less than %d (len:%d)\n", sms_id, WAT_MAX_SMS_SZ, sms_event->content.len);
						WAT_FUNC_DBG_END
								return WAT_FAIL;
					}
					break;
				case WAT_SMS_PDU_DCS_ALPHABET_UCS2:
					if (sms_event->content.len > 2*WAT_MAX_SMS_SZ) {
						wat_log_span(span, WAT_LOG_ERROR, "[sms:%d]SMS length cannot be greater than %d (len:%d)\n", sms_id, 2*WAT_MAX_SMS_SZ, sms_event->content.len);
						WAT_FUNC_DBG_END
						return WAT_FAIL;
					}
					break;
				default:
					wat_log_span(span, WAT_LOG_ERROR, "[sms:%d]Unsupported character set (%d)\n", sms_id, sms_event->pdu.dcs.alphabet);
					WAT_FUNC_DBG_END
					return WAT_FAIL;
					break;
			}
			break; /* case WAT_SMS_CONTENT_ENCODING_RAW */
		case WAT_SMS_CONTENT_ENCODING_BASE64:
			/* TODO: find out how to compute max length for base 64 */
			break; /* case WAT_SMS_CONTENT_ENCODING_BASE64 */
		case WAT_SMS_CONTENT_ENCODING_HEX:
			wat_log_span(span, WAT_LOG_ERROR, "[sms:%d]Hex content encoding not implemented yet\n", sms_id);
			WAT_FUNC_DBG_END
			return WAT_FAIL;
		default:
			wat_log_span(span, WAT_LOG_ERROR, "[sms:%d]Unsupported content encoding %s(%d)\n", sms_id, wat_sms_content_encoding2str(sms_event->content.encoding), sms_event->content.encoding);
			WAT_FUNC_DBG_END
			return WAT_FAIL;
	}

	memset(&event, 0, sizeof(event));
	
	event.id = WAT_EVENT_SMS_REQ;
	event.sms_id = sms_id;

	sprintf(sms_event->to.digits, "%s", wat_string_clean(sms_event->to.digits));
	
	memcpy(&event.data.sms_event, sms_event, sizeof(*sms_event));

	wat_event_enqueue(span, &event);
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_RESPONSE_FUNC(wat_user_cmd_response)
{
	int processed_tokens = 0;
	wat_user_cmd_t *cmd = obj;
	if (cmd->cb) {
		processed_tokens = cmd->cb(span->id, tokens, success, cmd->obj, error);
	} else {
		processed_tokens = 1;
	}
	wat_safe_free(obj);
	return processed_tokens;
}

WAT_DECLARE(wat_status_t) wat_cmd_req(uint8_t span_id, const char *at_cmd, wat_at_cmd_response_func cb, void *obj)
{
	wat_user_cmd_t *user_cmd = NULL;
	wat_span_t *span = NULL;

	span = wat_get_span(span_id);

	wat_assert_return(span, WAT_FAIL, "Invalid span");

	user_cmd = wat_calloc(1, sizeof(*user_cmd));
	if (!user_cmd) {
		return WAT_ENOMEM;
	}
	user_cmd->cb = cb;
	user_cmd->obj = obj;
	return wat_cmd_enqueue(span, at_cmd, wat_user_cmd_response, user_cmd);
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
		wat_log_span(span, WAT_LOG_DEBUG, "[TX RAW] %s (len:%d)\n", format_at_data(mydata, cmd, len), len);
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

WAT_DECLARE(char*) wat_decode_rssi(char *dest, unsigned rssi)
{
	switch (rssi) {
		case 0:
			sprintf(dest, "(-113)dBm or less");
		case 1:
			sprintf(dest, "(-111)dBm");
		case 31:
			sprintf(dest, "(-51)dBm");
		case 99:
			sprintf(dest, "not detectable");
		default:
			if (rssi >= 2 && rssi <= 30) {
				sprintf(dest, "(-%d)dBm", 113-(2*rssi));
			} else {
				sprintf(dest, "invalid");
			}
	}
	return dest;
}

WAT_DECLARE(const char*) wat_decode_alarm(unsigned in_alarm)
{
	return wat_alarm2str(in_alarm);
}

WAT_DECLARE(const char *) wat_decode_ber(unsigned ber)
{
	return wat_csq_ber2str(ber);
}

WAT_DECLARE(const char *) wat_decode_sms_cause(uint32_t cause)
{
	return wat_sms_cause2str(cause);
}

WAT_DECLARE(const char *) wat_decode_pin_status(wat_pin_stat_t pin_status)
{
	return wat_pin_stat2str(pin_status);
}

WAT_DECLARE(const char*) wat_decode_sms_pdu_mti(unsigned mti)
{
	return wat_sms_pdu_mti2str(mti);
}

WAT_DECLARE(const char *) wat_decode_band(wat_band_t band)
{
	return wat_band2str(band);
}

WAT_DECLARE(wat_band_t) wat_encode_band(const char *band)
{
	return wat_str2wat_band(band);
}

WAT_DECLARE(const char*) wat_decode_timezone(char *dest, int timezone)
{
	int hours = timezone/4;
	int minutes = (timezone % 4)*15;
	sprintf(dest, "%s%02d%02d",
				(timezone & 0x80) ? "-":"+",
				hours, minutes);
	return dest;
}

static const char *wat_codec_names[] = { WAT_CODEC_NAMES };
WAT_DECLARE(wat_codec_t) wat_encode_codec(const char *codec)
{
	wat_codec_t codec_mask = 0;
	char *c = 0;
	int i = 0;
	if (!codec) {
		return codec_mask;
	}
	while (*codec) {
		c = strchr(codec, ',');
		if (c) {
			*c = '\0';
		}
		for (i = 1; i < wat_array_len(wat_codec_names); i++) {
			if (!strcasecmp(codec, wat_codec_names[i])) {
				codec_mask |= (1 << (i-1));
				break;
			}
		}
		if (i == wat_array_len(wat_codec_names)) {
			wat_log(WAT_LOG_WARNING, "Unrecognized codec %s\n", codec);
		}
		if (c) {
			codec = (++c);
		} else {
			break;
		}
	}
	return codec_mask;
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

