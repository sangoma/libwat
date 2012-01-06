/*
 * Test libwat with Sangoma Wanpipe API
 *
 * Moises Silva <moy@sangoma.com>
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
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <libsangoma.h>

#include "libwat.h"
#include "test_utils.h"

#define POLL_INTERVAL 10 /* 10 millisecond */
#define BLOCK_SIZE 3000

typedef struct _wat_span {
	sng_fd_t dev;
	sangoma_wait_obj_t *waitable;
	unsigned char wat_span_id;
	wat_span_config_t wat_span_config;
	uint8_t answer_call:1; /* Incoming call pending */
	uint8_t hangup_call:1;	 /* Hangup call pending */
	uint8_t release_call:1;	 /* Release call pending */
	uint8_t make_call:1;	 /* Send an outbound call */
	uint16_t wat_call_id; /* Call ID of call if there is a call active */
} gsm_span_t;

int end = 0;
int g_make_call = 0;
char g_called_number[255];
int g_hangup_call = 0;
gsm_span_t gsm_spans[32];
static int g_outbound_call_id = 1;

void on_span_status(unsigned char span_id, wat_span_status_t *status);
int on_span_write(unsigned char span_id, void *buffer, unsigned len);

void on_con_ind(unsigned char span_id, uint8_t call_id, wat_con_event_t *con_event);
void on_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status);
void on_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event);
void on_rel_cfm(unsigned char span_id, uint8_t call_id);
void on_sms_ind(unsigned char span_id, wat_sms_event_t *sms_event);
void on_sms_sts(unsigned char span_id, uint8_t sms_id, wat_sms_status_t *status);

int on_span_write(unsigned char span_id, void *buffer, unsigned len)
{
	int res;
	wp_tdm_api_tx_hdr_t tx_hdr;

	memset(&tx_hdr, 0, sizeof(tx_hdr));

	res = sangoma_writemsg(gsm_spans[0].dev, &tx_hdr, sizeof(tx_hdr), buffer, len, 0);
	if (res != len) {
		fprintf(stdout, "Failed to write to sangoma device (res:%u len:%u)\n", res, len);
	}
	return res;
}


void on_span_status(unsigned char span_id, wat_span_status_t *status)
{
	switch (status->type) {
		case WAT_SPAN_STS_READY:

			break;
		case WAT_SPAN_STS_SIGSTATUS:
			fprintf(stdout, "span:%d Signalling status changed %d\n", span_id, status->sts.sigstatus);

			if (status->sts.sigstatus == WAT_SIGSTATUS_UP) {
				if (g_make_call) {
					gsm_spans[0].make_call = 1;
				}
			}
			return;
			break;
		case WAT_SPAN_STS_SIM_INFO_READY:

			break;
		case WAT_SPAN_STS_ALARM:
			fprintf(stdout, "span:%d Alarm received\n", span_id);
			break;
		default:
			fprintf(stdout, "Unhandled span status");
	}
	return;
}

void on_con_ind(unsigned char span_id, uint8_t call_id, wat_con_event_t *con_event)
{
	fprintf(stdout, "s%d: Incoming call (id:%d) Calling Number:%s type:%d plan:%d\n", span_id, call_id, con_event->calling_num.digits, con_event->calling_num.type, con_event->calling_num.plan);

	gsm_spans[0].wat_call_id = call_id;

	if (g_hangup_call) {
		/* Hangup the incoming call */
		gsm_spans[0].hangup_call = 1;
	} else {
		/* Answer the incoming call */
		gsm_spans[0].answer_call = 1;
	}
		
	return;
}

void on_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status)
{
	return;
}

void on_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event)
{
	fprintf(stdout, "s%d: Call hangup (id:%d) cause:%d\n", span_id, call_id, rel_event->cause);
	gsm_spans[0].release_call = 1;
	return;
}

void on_rel_cfm(unsigned char span_id, uint8_t call_id)
{
	fprintf(stdout, "s%d: Call hangup complete (id:%d)\n", span_id, call_id);
	return;
}

void on_sms_ind(unsigned char span_id, wat_sms_event_t *sms_event)
{
	return;
}

void on_sms_sts(unsigned char span_id, uint8_t sms_id, wat_sms_status_t *status)
{
	return;
}

static void handle_sig(int sig)
{
	switch(sig) {
		case SIGINT:
			end = 1;
			fprintf(stdout, "SIGINT caught, exiting program\n");
			break;
		default:
			fprintf(stdout, "Unhandled signal (%d)\n", sig);
	}
	return;
}

int main (int argc, char *argv[])
{
	wat_interface_t gen_interface;
	unsigned next;
	int x;
	int res;
	int count = 0;
	sangoma_wait_obj_t *waitable_objs[1];
	uint32_t input_flags[1];
	uint32_t output_flags[1];
	sangoma_status_t sangstatus;
	int spanno = 0;
	int channo = 0;

	unsigned char inbuf[BLOCK_SIZE];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <sXcY> [make_call, hangup_call] [number-to-dial]\n\
							make_call:   Make a call once the link is up\n\
							hangup_call: Hangup all incoming calls\n\
							default:     Answer all incoming call\n\n", argv[0]);
		exit(1);
	}

	x = sscanf(argv[1], "s%dc%d", &spanno, &channo);
	if (x != 2) {
		fprintf(stderr, "Invalid span/channel string provided (for span 1 chan 1 you must provide string s1c1)\n");
		exit(1);
	}

	if (argc > 2) {
		if (!strncasecmp(argv[2], "make_call", 9)) {
			if (argc < 4) {
				fprintf(stderr, "Please specify a number to dial\n");
				exit(1);
			}
			g_make_call = 1;
			snprintf(g_called_number, sizeof(g_called_number)-1, "%s", argv[3]);
		}
		if (!strncasecmp(argv[2], "hangup_call", 11)) {
			g_hangup_call = 1;
		}
	}

	signal(SIGINT, handle_sig);
	
	memset(&gen_interface, 0, sizeof(gen_interface));
	
	gen_interface.wat_span_sts = on_span_status;
	gen_interface.wat_span_write = on_span_write;
	gen_interface.wat_log = on_log;
	gen_interface.wat_log_span = on_log_span;
	gen_interface.wat_malloc = on_malloc;
	gen_interface.wat_calloc = on_calloc;
	gen_interface.wat_free = on_free;
	
	gen_interface.wat_con_ind = on_con_ind;
	gen_interface.wat_con_sts = on_con_sts;
	gen_interface.wat_rel_ind = on_rel_ind;
	gen_interface.wat_rel_cfm = on_rel_cfm;
	gen_interface.wat_sms_ind = on_sms_ind;
	gen_interface.wat_sms_sts = on_sms_sts;
	
	if (wat_register(&gen_interface)) {
		fprintf(stderr, "Failed to register WAT Library !!!\n");
		return -1;
	}
	fprintf(stdout, "Registered interface to WAT Library\n");

	/* Configure 1 span for Telit */
	memset(gsm_spans, 0, sizeof(gsm_spans));

	gsm_spans[0].wat_span_id = 1;
	gsm_spans[0].wat_span_config.moduletype = WAT_MODULE_TELIT;
	gsm_spans[0].wat_span_config.timeout_cid_num = 10;

	fprintf(stdout, "Opening device %s\n", argv[1]);

	gsm_spans[0].dev = sangoma_open_tdmapi_span_chan(spanno, channo);
	if (gsm_spans[0].dev < 0) {
		fprintf(stderr, "Unable to open %s:%s\n", argv[1], strerror(errno));
		exit(1);
	}

	sangstatus = sangoma_wait_obj_create(&gsm_spans[0].waitable, gsm_spans[0].dev, SANGOMA_DEVICE_WAIT_OBJ_SIG);
	if (sangstatus != SANG_STATUS_SUCCESS) {
		fprintf(stderr, "Unable to open %s:%s\n", argv[1], strerror(errno));
		exit(1);
	}
	
	fprintf(stdout, "Configuring span\n");
	if (wat_span_config(gsm_spans[0].wat_span_id, &(gsm_spans[0].wat_span_config))) {
		fprintf(stderr, "Failed to configure span!!\n");
		return -1;
	}

	fprintf(stdout, "Starting span\n");
	if (wat_span_start(gsm_spans[0].wat_span_id)) {
		fprintf(stderr, "Failed to start span!!\n");
		return -1;
	}

	fprintf(stdout, "Running \n");
	while(!end) {
		count++;
		if (!(count % 1000)) {
			fprintf(stdout, ".\n");
		}

		wat_span_run(gsm_spans[0].wat_span_id);
		next = wat_span_schedule_next(gsm_spans[0].wat_span_id);
		if (next > POLL_INTERVAL) {
			next = POLL_INTERVAL;
		}

		waitable_objs[0] = gsm_spans[0].waitable;
		input_flags[0] = SANG_WAIT_OBJ_HAS_INPUT;
		output_flags[0]= 0;

		sangstatus = sangoma_waitfor_many(waitable_objs, input_flags, output_flags, 1, next);
		if (sangstatus == SANG_STATUS_APIPOLL_TIMEOUT) {
			/* Timeout - do nothing */
		} else if (sangstatus != SANG_STATUS_SUCCESS) {
			fprintf(stdout, "Failed to poll d-channel\n");
		} else {
			if (output_flags[0] & SANG_WAIT_OBJ_HAS_INPUT) {
				wp_tdm_api_rx_hdr_t rx_hdr;
				memset(&rx_hdr, 0, sizeof(rx_hdr));
				res = sangoma_readmsg(gsm_spans[0].dev, &rx_hdr, sizeof(rx_hdr), inbuf, 1024, 0);
				if (res < 0) {
					fprintf(stdout, "Failed to read d-channel\n");
					break;
				}
				/* fprintf(stdout, "Read [%s] len:%d\n", inbuf, res); */
				wat_span_process_read(gsm_spans[0].wat_span_id, inbuf, res);
			}
		}

		if (gsm_spans[0].answer_call) {
			gsm_spans[0].answer_call = 0;
			wat_con_cfm(gsm_spans[0].wat_span_id, gsm_spans[0].wat_call_id);
		}

		if (gsm_spans[0].hangup_call) {
			gsm_spans[0].hangup_call = 0;
			wat_rel_req(gsm_spans[0].wat_span_id, gsm_spans[0].wat_call_id);
		}

		if (gsm_spans[0].release_call) {
			gsm_spans[0].release_call = 0;
			wat_rel_cfm(gsm_spans[0].wat_span_id, gsm_spans[0].wat_call_id);
		}

		if (gsm_spans[0].make_call) {
			wat_con_event_t con_event;
			
			gsm_spans[0].make_call = 0;
			memset(&con_event, 0, sizeof(con_event));
			
			sprintf(con_event.called_num.digits, g_called_number);
			gsm_spans[0].wat_call_id = (g_outbound_call_id++) | 0x8;
			printf("Dialing number %s\n", g_called_number);
			wat_con_req(gsm_spans[0].wat_span_id, gsm_spans[0].wat_call_id, &con_event);
		}
	}

	fprintf(stdout, "Exiting...\n");

	if (wat_span_stop(gsm_spans[0].wat_span_id)) {
		fprintf(stderr, "Failed to stop span!!\n");
		return -1;
	}
	close(gsm_spans[0].dev);
	return 0;
}
