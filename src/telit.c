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
//#include <math.h>
#include "libwat.h"
#include "wat_internal.h"
#include "telit.h"

wat_status_t telit_start(wat_span_t *span);
wat_status_t telit_restart(wat_span_t *span);
wat_status_t telit_shutdown(wat_span_t *span);

WAT_RESPONSE_FUNC(wat_response_atz);
WAT_RESPONSE_FUNC(wat_response_ate);
WAT_RESPONSE_FUNC(wat_response_selint);
WAT_RESPONSE_FUNC(wat_response_smsmode);
WAT_RESPONSE_FUNC(wat_response_regmode);
WAT_RESPONSE_FUNC(wat_response_dvi);
WAT_RESPONSE_FUNC(wat_response_shssd);
WAT_RESPONSE_FUNC(wat_response_codecinfo);

static wat_module_t telit_interface = {
	.start = telit_start,
	.restart = telit_restart,
	.shutdown = telit_shutdown,
};

wat_status_t telit_init(wat_span_t *span)
{
	return wat_module_register(span, &telit_interface);
}

WAT_NOTIFY_FUNC(wat_notify_codec_info)
{
	unsigned count;
	char *cmdtokens[10];
	int consumed_tokens = 0;
	
	WAT_NOTIFY_FUNC_DBG_START

	wat_match_prefix(tokens[0], "#CODECINFO: ");

	memset(cmdtokens, 0, sizeof(cmdtokens));
	count = wat_cmd_entry_tokenize(tokens[0], cmdtokens);

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
	wat_log_span(span, WAT_LOG_DEBUG, "Starting Telit module\n");

	/* Section 2.1 of Telit AT Commands reference Guide recommends these options to be enabled */
	wat_cmd_enqueue(span, "AT#SELINT=2", wat_response_selint, NULL);
	wat_cmd_enqueue(span, "AT#SMSMODE=1", wat_response_smsmode, NULL);

	/* From Telit AT commands reference guide, page 105: Set AT#REGMODE=1
	 * makes CREG behavior more formal */
	wat_cmd_enqueue(span, "AT#REGMODE=1", NULL, NULL);
	wat_cmd_enqueue(span, "AT#DVI=1,1,0", wat_response_dvi, NULL);

	/* Enable Echo cancellation */
	wat_cmd_enqueue(span, "AT#SHFEC=1", NULL, NULL);
	wat_cmd_enqueue(span, "AT#SHSEC=1", NULL, NULL);

	/* Disable Sidetone as it sounds like echo on calls with long delay (e.g SIP calls) */
	wat_cmd_enqueue(span, "AT#SHSSD=0", wat_response_shssd, NULL);

	/* Enable codec notifications 
	 * (format = 1 is text, mode 2 is short mode to get notifications only including the codec in use) */
	wat_cmd_enqueue(span, "AT#CODECINFO=1,2", wat_response_codecinfo, NULL);
	wat_cmd_register(span, "#CODECINFO", wat_notify_codec_info);

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


