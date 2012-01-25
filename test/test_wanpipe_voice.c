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
#define IO_SIZE 3000

typedef struct _wat_span {
	sng_fd_t dev;
	sng_fd_t bchan_dev;
	sangoma_wait_obj_t *waitable;
	sangoma_wait_obj_t *bchan_waitable;
	unsigned char wat_span_id;
	wat_span_config_t wat_span_config;
	uint8_t answer_call:1; /* Incoming call pending */
	uint8_t hangup_call:1;	 /* Hangup call pending */
	uint8_t release_call:1;	 /* Release call pending */
	uint8_t make_call:1;	 /* Send an outbound call */
	uint8_t answered:1; /* call was answered */
	uint16_t wat_call_id; /* Call ID of call if there is a call active */
	FILE *outfile;
	FILE *infile;
} gsm_span_t;

int end = 0;
int g_make_call = 0;
char g_called_number[255];
int g_hangup_call = 0;
gsm_span_t gsm_span;
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

	res = sangoma_writemsg(gsm_span.dev, &tx_hdr, sizeof(tx_hdr), buffer, len, 0);
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
					gsm_span.make_call = 1;
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

	gsm_span.wat_call_id = call_id;

	if (g_hangup_call) {
		/* Hangup the incoming call */
		gsm_span.hangup_call = 1;
	} else {
		/* Answer the incoming call */
		gsm_span.answer_call = 1;
	}
		
	return;
}

void on_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status)
{
	if (status->type == WAT_CON_STATUS_TYPE_ANSWER) {
		gsm_span.answered = 1;
	}
}

void on_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event)
{
	fprintf(stdout, "s%d: Call hangup (id:%d) cause:%d\n", span_id, call_id, rel_event->cause);
	gsm_span.release_call = 1;
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
	char *devstr = NULL;
	char *playfile_str = NULL;
	unsigned next;
	int x;
	int res;
	int count = 0;
	sangoma_wait_obj_t *waitable_objs[2];
	uint32_t input_flags[2];
	uint32_t output_flags[2];
	sangoma_status_t sangstatus;
	wanpipe_api_t tdm_api;
	int spanno = 0;
	int channo = 0;
	int bchan = 0;
	int i = 0;
	int bchan_mtu = 0;

	unsigned char iobuf[IO_SIZE];

	if (argc < 2) {
		fprintf(stderr, "Usage:\n"
				"-dev <sXcY> - D-channel wanpipe device\n"
				"-make_call [number-to-dial] - Place a call in the provided span\n"
				"-hangup_call - Hangup any calls in the provided span\n"
				"-playfile - Provide a file to play when the call is answered\n");
		exit(1);
	}

#define INC_ARG(arg_i) \
	arg_i++; \
	if (arg_i >= argc) { \
		fprintf(stderr, "No option value was specified\n"); \
		exit(1); \
	} 

	for (i = 1; i < argc; i++) {
		if (!strcasecmp(argv[i], "-dev")) {
			INC_ARG(i);
			devstr = argv[i];
			x = sscanf(devstr, "s%dc%d", &spanno, &channo);
			if (x != 2) {
				fprintf(stderr, "Invalid span/channel string provided (for span 1 chan 1 you must provide string s1c1)\n");
				exit(1);
			}
			if (channo < 2 || (channo % 2)) {
				fprintf(stderr, "Invalid d-channel device %s (channel must be even number >= 2)\n", devstr);
				exit(1);
			}
		} else if (!strcasecmp(argv[i], "-make_call")) {
			INC_ARG(i);
			g_make_call = 1;
			snprintf(g_called_number, sizeof(g_called_number)-1, "%s", argv[i]);
		} else if (!strcasecmp(argv[i], "-hangup_call")) {
			g_hangup_call = 1;
		} else if (!strcasecmp(argv[i], "-playfile")) {
			INC_ARG(i);
			playfile_str = argv[i];
		} else {
			fprintf(stderr, "Invalid option %s\n", argv[i]);
			exit(1);
		}
	}

	if (!devstr) {
		fprintf(stderr, "-dev is a mandatory option\n");
		exit(1);
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
	memset(&gsm_span, 0, sizeof(gsm_span));

	gsm_span.wat_span_id = 1;
	gsm_span.wat_span_config.moduletype = WAT_MODULE_TELIT;
	gsm_span.wat_span_config.timeout_cid_num = 10;

	fprintf(stdout, "Opening device %s\n", devstr);
	gsm_span.dev = sangoma_open_tdmapi_span_chan(spanno, channo);
	if (gsm_span.dev < 0) {
		fprintf(stderr, "Unable to open %s: %s\n", devstr, strerror(errno));
		exit(1);
	}

	sangstatus = sangoma_wait_obj_create(&gsm_span.waitable, gsm_span.dev, SANGOMA_DEVICE_WAIT_OBJ_SIG);
	if (sangstatus != SANG_STATUS_SUCCESS) {
		fprintf(stderr, "Unable to create waitable object for channel %s: %s\n", devstr, strerror(errno));
		exit(1);
	}

	bchan = channo - 1;
	gsm_span.bchan_dev = sangoma_open_tdmapi_span_chan(spanno, bchan);
	if (gsm_span.bchan_dev <= 0) {
		fprintf(stderr, "Unable to open b-channel s%dc%d: %s\n", spanno, bchan, strerror(errno));
		exit(1);
	}

	bchan_mtu = sangoma_tdm_get_usr_mtu_mru(gsm_span.bchan_dev, &tdm_api);
	if (bchan_mtu <= 0) {
		fprintf(stderr, "Unable to retrieve b-channel s%dc%d MTU: %s\n", spanno, bchan, strerror(errno));
		exit(1);
	}

	sangstatus = sangoma_wait_obj_create(&gsm_span.bchan_waitable, gsm_span.bchan_dev, SANGOMA_DEVICE_WAIT_OBJ_SIG);
	if (sangstatus != SANG_STATUS_SUCCESS) {
		fprintf(stderr, "Unable to create waitable object for channel s%dc%d: %s\n", spanno, bchan, strerror(errno));
		exit(1);
	}
	
	fprintf(stdout, "Configuring span\n");
	if (wat_span_config(gsm_span.wat_span_id, &(gsm_span.wat_span_config))) {
		fprintf(stderr, "Failed to configure span!!\n");
		return -1;
	}

	fprintf(stdout, "Starting span\n");
	if (wat_span_start(gsm_span.wat_span_id)) {
		fprintf(stderr, "Failed to start span!!\n");
		return -1;
	}

	fprintf(stdout, "Running \n");
	while(!end) {
		int numchans = 0;
		count++;
		if (!(count % 1000)) {
			fprintf(stdout, ".\n");
		}

		wat_span_run(gsm_span.wat_span_id);
		next = wat_span_schedule_next(gsm_span.wat_span_id);
		if (next > POLL_INTERVAL) {
			next = POLL_INTERVAL;
		}

		waitable_objs[0] = gsm_span.waitable;
		input_flags[0] = SANG_WAIT_OBJ_HAS_INPUT;
		output_flags[0]= 0;
		numchans = 1;

		input_flags[1] = 0;
		output_flags[1] = 0;

		if (gsm_span.answered) {

			if (!gsm_span.outfile) {
				char fname[255];
				snprintf(fname, sizeof(fname), "s%dc%d-output.raw", spanno, bchan);
				gsm_span.outfile = fopen(fname, "w");
			}

			if (playfile_str && !gsm_span.infile) {
				gsm_span.infile = fopen(playfile_str, "r");
				if (!gsm_span.infile) {
					fprintf(stderr, "Failed to open input file %s\n", playfile_str);
					break;
				}
			}

			if (gsm_span.infile || gsm_span.outfile) {
				waitable_objs[1] = gsm_span.bchan_waitable;
				output_flags[1] = 0;
				numchans++;
				if (gsm_span.infile) {
					input_flags[1] = SANG_WAIT_OBJ_HAS_OUTPUT;
				}
				if (gsm_span.outfile) {
					input_flags[1] |= SANG_WAIT_OBJ_HAS_INPUT;
				}
			}
		}

		sangstatus = sangoma_waitfor_many(waitable_objs, input_flags, output_flags, numchans, next);
		if (sangstatus == SANG_STATUS_APIPOLL_TIMEOUT) {
			/* Timeout - do nothing */
		} else if (sangstatus != SANG_STATUS_SUCCESS) {
			fprintf(stdout, "Failed to poll d-channel\n");
			break;
		} else {
			if (output_flags[0] & SANG_WAIT_OBJ_HAS_INPUT) {
				wp_tdm_api_rx_hdr_t rx_hdr;
				memset(&rx_hdr, 0, sizeof(rx_hdr));
				res = sangoma_readmsg(gsm_span.dev, &rx_hdr, sizeof(rx_hdr), iobuf, 1024, 0);
				if (res < 0) {
					fprintf(stdout, "Failed to read d-channel\n");
					break;
				}
				/* fprintf(stdout, "Read [%s] len:%d\n", iobuf, res); */
				wat_span_process_read(gsm_span.wat_span_id, iobuf, res);
			}

			if (gsm_span.outfile && (output_flags[1] & SANG_WAIT_OBJ_HAS_INPUT)) {
				wp_tdm_api_rx_hdr_t rx_hdr;
				memset(&rx_hdr, 0, sizeof(rx_hdr));
				res = sangoma_readmsg(gsm_span.bchan_dev, &rx_hdr, sizeof(rx_hdr), iobuf, 1024, 0);
				if (res < 0) {
					fprintf(stdout, "Failed to read from b-channel: %s\n", strerror(errno));
					break;
				}
				fwrite(iobuf, 1, res, gsm_span.outfile);
			}

			if (gsm_span.infile && (output_flags[1] & SANG_WAIT_OBJ_HAS_OUTPUT)) {
				wp_tdm_api_tx_hdr_t tx_hdr;
				int len = 0;

				memset(&tx_hdr, 0, sizeof(tx_hdr));

				len = fread(iobuf, 1, bchan_mtu, gsm_span.infile);
				if (len <= 0) {
					fclose(gsm_span.infile);
					gsm_span.infile = NULL;
				} else {
					#define gsm_swap16(sample) (((sample >> 8) & 0x00FF) | ((sample << 8) & 0xFF00))
					int16_t *tx_linear = NULL;
					tx_linear = (int16_t *)iobuf;
					for (i = 0; i < len/2; i++) {
						tx_linear[i] = gsm_swap16(tx_linear[i]);
					}
					res = sangoma_writemsg(gsm_span.bchan_dev, &tx_hdr, sizeof(tx_hdr), iobuf, len, 0);
					if (res != len) {
						fprintf(stdout, "Failed to write to b-channel device (res:%d len:%d): %s\n", res, len, strerror(errno));
						break;
					}
				}
			}
		}

		if (gsm_span.answer_call) {
			gsm_span.answer_call = 0;
			wat_con_cfm(gsm_span.wat_span_id, gsm_span.wat_call_id);
		}

		if (gsm_span.hangup_call) {
			gsm_span.hangup_call = 0;
			wat_rel_req(gsm_span.wat_span_id, gsm_span.wat_call_id);
		}

		if (gsm_span.release_call) {
			gsm_span.release_call = 0;
			wat_rel_cfm(gsm_span.wat_span_id, gsm_span.wat_call_id);
		}

		if (gsm_span.make_call) {
			wat_con_event_t con_event;
			
			gsm_span.make_call = 0;
			memset(&con_event, 0, sizeof(con_event));
			
			sprintf(con_event.called_num.digits, g_called_number);
			gsm_span.wat_call_id = (g_outbound_call_id++) | 0x8;
			printf("Dialing number %s\n", g_called_number);
			wat_con_req(gsm_span.wat_span_id, gsm_span.wat_call_id, &con_event);
		}
	}

	fprintf(stdout, "Exiting...\n");

	if (wat_span_stop(gsm_span.wat_span_id)) {
		fprintf(stderr, "Failed to stop span!!\n");
		return -1;
	}
	close(gsm_span.dev);
	close(gsm_span.bchan_dev);
	return 0;
}
