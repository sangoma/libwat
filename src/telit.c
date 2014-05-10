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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "libwat.h"
#include "wat_internal.h"
#include "telit.h"

#define TELIT_GC864 0
#define TELIT_HE910 1

wat_status_t telit_start(wat_span_t *span);
wat_status_t telit_restart(wat_span_t *span);
wat_status_t telit_shutdown(wat_span_t *span);
wat_status_t telit_wait_sim(wat_span_t *span);
wat_status_t telit_set_codec(wat_span_t *span, wat_codec_t codec_mask);

WAT_RESPONSE_FUNC(wat_response_atz);
WAT_RESPONSE_FUNC(wat_response_ate);
WAT_RESPONSE_FUNC(wat_response_selint);
WAT_RESPONSE_FUNC(wat_response_smsmode);
WAT_RESPONSE_FUNC(wat_response_regmode);
WAT_RESPONSE_FUNC(wat_response_dvi);
WAT_RESPONSE_FUNC(wat_response_shssd);
WAT_RESPONSE_FUNC(wat_response_codecinfo);
WAT_RESPONSE_FUNC(wat_response_set_codec);
WAT_RESPONSE_FUNC(wat_response_qss);
WAT_NOTIFY_FUNC(wat_notify_qss);

static wat_module_t telit_gc864_interface = {
	.start = telit_start,
	.restart = telit_restart,
	.shutdown = telit_shutdown,
	.set_codec = telit_set_codec,
	.wait_sim = telit_wait_sim,
	.model = TELIT_GC864,
	.name = "Telit GC864",
};

static wat_module_t telit_he910_interface = {
	.start = telit_start,
	.restart = telit_restart,
	.shutdown = telit_shutdown,
	.set_codec = telit_set_codec,
	.wait_sim = telit_wait_sim,
	.model = TELIT_HE910,
	.name = "Telit HE910",
};

wat_status_t telit_gc864_init(wat_span_t *span)
{
	return wat_module_register(span, &telit_gc864_interface);
}

wat_status_t telit_he910_init(wat_span_t *span)
{
	return wat_module_register(span, &telit_he910_interface);
}

WAT_NOTIFY_FUNC(wat_notify_codec_info)
{
	unsigned count;
	char *cmdtokens[10];
	int consumed_tokens = 0;
	
	WAT_NOTIFY_FUNC_DBG_START

	wat_match_prefix(tokens[0], "#CODECINFO: ");

	count = wat_cmd_entry_tokenize(tokens[0], cmdtokens, wat_array_len(cmdtokens));

	if (count < 0) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to parse #CODECINFO event '%s'\n", tokens[0]);
		consumed_tokens = 1;
	} else {
		wat_log_span(span, WAT_LOG_DEBUG, "Codec in use: %s\n", tokens[0]);
		consumed_tokens = 1;
	}

	wat_free_tokens(cmdtokens);
	return consumed_tokens;
}

wat_status_t telit_start(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Starting %s module\n", span->module.name);

	/* Section 2.1 of Telit AT Commands reference Guide recommends these options to be enabled */
	wat_cmd_enqueue(span, "AT#SELINT=2", wat_response_selint, NULL, span->config.timeout_command);

	wat_cmd_enqueue(span, "AT#SMSMODE=1", wat_response_smsmode, NULL, span->config.timeout_command);

	/* From Telit AT commands reference guide, page 105: Set AT#REGMODE=1
	 * makes CREG behavior more formal */
	wat_cmd_enqueue(span, "AT#REGMODE=1", NULL, NULL, span->config.timeout_command);

	if (span->module.model == TELIT_HE910) {
		wat_cmd_enqueue(span, "AT#DVI=1,2,0", wat_response_dvi, NULL, span->config.timeout_command);
		wat_cmd_enqueue(span, "AT#DVIEXT=1,0,0,1,0", NULL, NULL, span->config.timeout_command);
	} else if (span->module.model == TELIT_GC864) {
		wat_cmd_enqueue(span, "AT#DVI=1,1,0", wat_response_dvi, NULL, span->config.timeout_command);
		/* I guess we want full CPU power */
		wat_cmd_enqueue(span, "AT#CPUMODE=1", NULL, NULL, span->config.timeout_command);
	} else {
		wat_log_span(span, WAT_LOG_ERROR, "Invalid telit module %s (%d)\n", span->module.name, span->module.model);
		return WAT_FAIL;
	}

	/* Enable Echo cancellation */
	wat_cmd_enqueue(span, "AT#SHFEC=1", NULL, NULL, span->config.timeout_command);
	wat_cmd_enqueue(span, "AT#SHSEC=1", NULL, NULL, span->config.timeout_command);

	/* Disable Sidetone as it sounds like echo on calls with long delay (e.g SIP calls) */
	wat_cmd_enqueue(span, "AT#SHSSD=0", wat_response_shssd, NULL, span->config.timeout_command);

	/* Enable codec notifications 
	 * (format = 1 is text, mode 2 is short mode to get notifications only including the codec in use) */
	wat_cmd_enqueue(span, "AT#CODECINFO=1,2", wat_response_codecinfo, NULL, span->config.timeout_command);
	wat_cmd_register(span, "#CODECINFO", wat_notify_codec_info);
	
	/* Make sure the DIALMODE is set to 0 to receive an OK code as soon as possible
	 * the option of using DIALMODE=2 is tempting as provides progress status 
	 * notifications (DIALING, RINGING, CONNECTED, RELEASED, DISCONNECTED), but the modem
	 * will not accept any further commands in the meantime, which is not convenient */
	wat_cmd_enqueue(span, "AT#DIALMODE=0", NULL, NULL, span->config.timeout_command);

	/* Enable automatic Band selection */
	wat_cmd_enqueue(span, "AT+COPS=0", NULL, NULL, span->config.timeout_command);

	switch (span->config.band) {
		case WAT_BAND_900_1800:
			wat_cmd_enqueue(span, "AT#BND=0", NULL, NULL, span->config.timeout_command);
			break;
		case WAT_BAND_900_1900:
			wat_cmd_enqueue(span, "AT#BND=1", NULL, NULL, span->config.timeout_command);
			break;
		case WAT_BAND_850_1800:
			wat_cmd_enqueue(span, "AT#BND=2", NULL, NULL, span->config.timeout_command);
			break;
		case WAT_BAND_850_1900:
			wat_cmd_enqueue(span, "AT#BND=3", NULL, NULL, span->config.timeout_command);
			break;
		default:
			wat_log_span(span, WAT_LOG_CRIT, "Unsupported band value:%d\n", span->config.band);
		case WAT_BAND_AUTO:
			wat_cmd_enqueue(span, "AT#AUTOBND=2", NULL, NULL, span->config.timeout_command);
			break;
	}

	return WAT_SUCCESS;
}

wat_status_t telit_restart(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Restarting Telit module\n");
	return WAT_FAIL;
}

wat_status_t telit_shutdown(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Stopping Telit module\n");
	return WAT_FAIL;
}

wat_status_t telit_set_codec(wat_span_t *span, wat_codec_t codec_mask)
{
	/* since telit is the first module ever written we got to choose the codec mask to 
	 * match their spec and we can bypass mapping from wat codec values to telit values */
	char codec_cmd[WAT_MAX_CMD_SZ];
	snprintf(codec_cmd, sizeof(codec_cmd), "AT#CODEC=%d", codec_mask);
	wat_cmd_enqueue(span, codec_cmd, wat_response_set_codec, NULL, span->config.timeout_command);
	return WAT_SUCCESS;
}

/*
 * It seems sometimes the telit module may never come to state 'sim ready' and just
 * stay in 'sim inserted', even though we clearly can place/receive calls and send/receive 
 * sms, we consider any state other than 'sim not inserted' enough now to proceed with initialization
 */
#define WAT_TELIT_SIM_IS_READY(sim_status) (sim_status != WAT_TELIT_SIM_NOT_INSERTED && sim_status != WAT_TELIT_SIM_INVALID)

wat_status_t telit_wait_sim(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_INFO, "Waiting for SIM acccess...\n");
	wat_cmd_register(span, "#QSS", wat_notify_qss);
	wat_cmd_enqueue(span, "AT#QSS=2", wat_response_qss, NULL, span->config.timeout_command);
	wat_cmd_enqueue(span, "AT#QSS?", wat_response_qss, NULL, span->config.timeout_command);
	return WAT_SUCCESS;
}

WAT_ENUM_NAMES(WAT_TELIT_SIM_STATUS_NAMES, WAT_TELIT_SIM_STATUS_STRINGS)
WAT_STR2ENUM(wat_str2wat_telit_sim_status, wat_telit_sim_status2str, wat_telit_sim_status_t, WAT_TELIT_SIM_STATUS_NAMES, WAT_TELIT_SIM_INVALID)

WAT_NOTIFY_FUNC(wat_notify_qss)
{
	int rc = 1;
	char *cmdtokens[4];
	int sim_status = 0;

	WAT_NOTIFY_FUNC_DBG_START

	/* Format #QSS: 3 */
	wat_match_prefix(tokens[0], "#QSS: ");

	switch (wat_cmd_entry_tokenize(tokens[0], cmdtokens, wat_array_len(cmdtokens))) {
		case 1:
			sim_status = atoi(cmdtokens[0]);
			wat_log_span(span, WAT_LOG_INFO, "SIM access status changed to '%s' (%d)\n", wat_telit_sim_status2str(sim_status), sim_status);
			if (WAT_TELIT_SIM_IS_READY(sim_status)) {
				if (span->state < WAT_SPAN_STATE_POST_START) {
					wat_span_set_state(span, WAT_SPAN_STATE_POST_START);
				}
			}
			break;
		case 2:
			/* This is not a notify, but a response */
			rc = 0;
			break;
		default:
			wat_log(WAT_LOG_ERROR, "Failed to parse #QSS %s\n", tokens[0]);
			break;
	}

	wat_free_tokens(cmdtokens);

	WAT_FUNC_DBG_END
	return rc;
}

WAT_RESPONSE_FUNC(wat_response_qss)
{
	char *cmdtokens[4];
	int sim_status = 0;
	int parameters = 0;
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to get SIM status\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	/* Format #QSS: 2,3 */
	wat_match_prefix(tokens[0], "#QSS: ");
	if (!tokens[1]) {
		/* This is a response to AT#QSS = 2 (enabling Unsollicited QSS events)*/
		WAT_FUNC_DBG_END
		return 1;
	}

	parameters = wat_cmd_entry_tokenize(tokens[0], cmdtokens, wat_array_len(cmdtokens));
	switch (parameters) {
		case 2:
			sim_status = atoi(cmdtokens[1]);
			wat_log_span(span, WAT_LOG_INFO, "SIM status is '%s' (%d)\n", wat_telit_sim_status2str(sim_status), sim_status);
			if (WAT_TELIT_SIM_IS_READY(sim_status)) {
				if (span->state < WAT_SPAN_STATE_POST_START) {
					wat_span_set_state(span, WAT_SPAN_STATE_POST_START);
				}
			}
			break;
		default:
			wat_log(WAT_LOG_ERROR, "Failed to parse #QSS %s, expecting 2 parameters but got %d\n",
					tokens[0], parameters);
			break;
	}
	
	wat_free_tokens(cmdtokens);

	WAT_FUNC_DBG_END
	return 2;
}

WAT_RESPONSE_FUNC(wat_response_selint)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable interface type\n");
	}
	WAT_FUNC_DBG_END
	return 1;
}

WAT_RESPONSE_FUNC(wat_response_smsmode)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable sms mode\n");
	}
	WAT_FUNC_DBG_END
	return 1;
}

WAT_RESPONSE_FUNC(wat_response_regmode)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable reg mode\n");
	}
	WAT_FUNC_DBG_END
	return 1;
}

WAT_RESPONSE_FUNC(wat_response_dvi)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable Digital Voice Interface\n");
		WAT_FUNC_DBG_END
		return 1;
	}
	WAT_FUNC_DBG_END
	return 1;
}

WAT_RESPONSE_FUNC(wat_response_shssd)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to disable Sidetone\n");
	}
	WAT_FUNC_DBG_END
	return 1;
}


WAT_RESPONSE_FUNC(wat_response_codecinfo)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable codec notifications\n");
	}
	WAT_FUNC_DBG_END
	return 1;
}

WAT_RESPONSE_FUNC(wat_response_set_codec)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to set codec preferences!\n");
	}
	WAT_FUNC_DBG_END
	return 1;
}


