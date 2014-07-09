/*
 * libwat: Wireless AT commands library
 *
 * Jasper van der Neut - Stulen <jasper@speakup.nl>
 * Copyright (C) 2013, SpeakUp BV
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
#include "motorola.h"

wat_status_t motorola_start(wat_span_t *span);
wat_status_t motorola_restart(wat_span_t *span);
wat_status_t motorola_shutdown(wat_span_t *span);
wat_status_t motorola_set_codec(wat_span_t *span, wat_codec_t codec_mask);
wat_status_t motorola_wait_sim(wat_span_t *span);


static wat_module_t motorola_interface = {
	.start = motorola_start,
	.restart = motorola_restart,
	.shutdown = motorola_shutdown,
	.set_codec = motorola_set_codec,
	.wait_sim = motorola_wait_sim,
	.model =  0,
	.name = "motorola",
	.flags = WAT_MODFLAG_NONE,
};

wat_status_t motorola_init(wat_span_t *span)
{
	return wat_module_register(span, &motorola_interface);
}

wat_status_t motorola_start(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Starting Motorola module\n");

	/* Enable notifications for incoming SMS */
	wat_cmd_enqueue(span, "AT+CNMI=0,2,2", wat_response_cnmi, NULL, span->config.timeout_command);

	return WAT_SUCCESS;
}

wat_status_t motorola_restart(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Restarting Motorola module\n");
	return WAT_FAIL;
}

wat_status_t motorola_shutdown(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_DEBUG, "Stopping Motorola module\n");
	return WAT_FAIL;
}

wat_status_t motorola_set_codec(wat_span_t *span, wat_codec_t codec_mask)
{
	/* Motorola docs suggest the AT+MVC command for codec preference.
	 * The Junghanns QuadGSM doesn't recognize this command, so NoOp for now */
	wat_log_span(span, WAT_LOG_DEBUG, "Setting codec preferences unsupported\n");

	return WAT_SUCCESS;
}

wat_status_t motorola_wait_sim(wat_span_t *span)
{
	wat_log_span(span, WAT_LOG_INFO, "Waiting for SIM acccess...\n");

	/* Copied from bristuff libgsmat */
	wat_cmd_enqueue(span, "AT+CMEE=2", NULL, NULL, span->config.timeout_command);
	wat_cmd_enqueue(span, "AT+MADIGITAL=1", NULL, NULL, span->config.timeout_command);

	/* Motorola dev guide states this command is necessary for full operation */
	wat_cmd_enqueue(span, "AT+CPIN?", wat_response_cpin, NULL, span->config.timeout_command);

	/* FIXME: Only pin 0000 (no pin) supported at this moment */
	wat_cmd_enqueue(span, "AT+CPIN=\"0000\"", NULL, NULL, span->config.timeout_command);

	if (span->state < WAT_SPAN_STATE_POST_START) {
		wat_span_set_state(span, WAT_SPAN_STATE_POST_START);
        }

	return WAT_SUCCESS;
}

