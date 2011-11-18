/*
 * libwat: Wireless AT commands library
 *
 * Written by David Yat Sin <dyatsin@sangoma.com>
 *
 * Copyright (C) 2011, Sangoma Technologies.
 * All Rights Reserved.
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
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

#include <dahdi/user.h>

#include "libwat.h"
#include "test_utils.h"

#define POLL_INTERVAL 10 /* 10 millisecond */
#define BLOCK_SIZE 3000

typedef struct _wat_span {
	int fd;
	unsigned char wat_span_id;
	wat_span_config_t wat_span_config;
	int dchan_up;
	uint8_t send_sms:1;	 /* Send an outbound sms */
	uint16_t wat_sms_id; /* Call ID of call if there is a call active */
} gsm_span_t;


int end = 0;
gsm_span_t gsm_spans[32];
static int g_outbound_sms_id = 1;
static int g_message_count = 10;
static int g_message_sent = 0;
static int g_message_ack = 0;

void on_sigstatus_change(unsigned char span_id, wat_sigstatus_t sigstatus);
void on_alarm(unsigned char span_id, wat_alarm_t alarm);
int on_span_write(unsigned char span_id, void *buffer, unsigned len);

void on_con_ind(unsigned char span_id, uint8_t call_id, wat_con_event_t *con_event);
void on_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status);
void on_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event);
void on_rel_cfm(unsigned char span_id, uint8_t call_id);
void on_sms_ind(unsigned char span_id, wat_sms_event_t *sms_event);
void on_sms_sts(unsigned char span_id, uint8_t sms_id, wat_sms_status_t *status);
void on_cmd_sts(unsigned char span_id, wat_cmd_status_t *status);

int on_span_write(unsigned char span_id, void *buffer, unsigned len)
{
	int res;
	uint8_t writebuf[BLOCK_SIZE];

	memcpy(writebuf, buffer, len);

	/* fprintf(stdout, "Writing len:%d\n", len); */
	res = write(gsm_spans[0].fd, writebuf, len);
	if (res != len) {
		fprintf(stdout, "Failed to write to dahdi device (res:%u len:%u)\n", res, len);
	}
	return res;
}

void on_sigstatus_change(unsigned char span_id, wat_sigstatus_t sigstatus)
{
	fprintf(stdout, "span:%d Signalling status changed %d\n", span_id, sigstatus);
	if (sigstatus == WAT_SIGSTATUS_UP) {
		gsm_spans[0].send_sms = 1;
	}
	return;
}

void on_alarm(unsigned char span_id, wat_alarm_t alrm)
{
	fprintf(stdout, "span:%d Alarm received\n", span_id);
	return;
}

void on_con_ind(unsigned char span_id, uint8_t call_id, wat_con_event_t *con_event)
{
	fprintf(stdout, "s%d: Incoming call (id:%d) Calling Number:%s type:%d plan:%d\n", span_id, call_id, con_event->calling_num.digits, con_event->calling_num.type, con_event->calling_num.plan);

	return;
}

void on_con_sts(unsigned char span_id, uint8_t call_id, wat_con_status_t *status)
{
	return;
}

void on_rel_ind(unsigned char span_id, uint8_t call_id, wat_rel_event_t *rel_event)
{
	fprintf(stdout, "s%d: Call hangup (id:%d) cause:%d\n", span_id, call_id, rel_event->cause);
	
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
	fprintf(stdout, "s%d: [id:%d]SMS status %s cause:%d error:%s \n", span_id, sms_id, (status->success == WAT_TRUE) ? "success": "Fail", status->cause, status->error);

	if (++g_message_ack >= g_message_count) {
		end = 1;
	}
	return;
}

void on_cmd_sts(unsigned char span_id, wat_cmd_status_t *status)
{
	return;
}


static void handle_sig(int sig)
{
	switch(sig) {
		case SIGUSR1:
			gsm_spans[0].send_sms = 1;
			break;
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
	struct pollfd fds[1];
	struct dahdi_params p;
	struct dahdi_bufferinfo bi;
	struct dahdi_spaninfo si;

	unsigned char inbuf[BLOCK_SIZE];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <DAHDI channel number> [message count]\n", argv[0]);
		exit(1);
	}

	x = atoi(argv[1]);
	if (x < 1) {
		fprintf(stderr, "Invalid channel number\n");
		exit(1);
	}

	if (argc > 2) {
		g_message_count = atoi(argv[2]);
	}

	if (g_message_count < 1) {
		g_message_count = 10;
	}

	signal(SIGINT, handle_sig);
	signal(SIGUSR1, handle_sig);
	
	memset(&gen_interface, 0, sizeof(gen_interface));
	
	gen_interface.wat_sigstatus_change = on_sigstatus_change;
	gen_interface.wat_span_write = on_span_write;
	gen_interface.wat_log = on_log;
	gen_interface.wat_log_span = on_log_span;
	gen_interface.wat_malloc = on_malloc;
	gen_interface.wat_calloc = on_calloc;
	gen_interface.wat_free = on_free;
	
	gen_interface.wat_alarm = on_alarm;
	gen_interface.wat_con_ind = on_con_ind;
	gen_interface.wat_con_sts = on_con_sts;
	gen_interface.wat_rel_ind = on_rel_ind;
	gen_interface.wat_rel_cfm = on_rel_cfm;
	gen_interface.wat_sms_ind = on_sms_ind;
	gen_interface.wat_sms_sts = on_sms_sts;
	gen_interface.wat_cmd_sts = on_cmd_sts;
	
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
	
	gsm_spans[0].fd = open("/dev/dahdi/channel", O_RDWR);
	if (gsm_spans[0].fd < 0) {
		fprintf(stderr, "Unable to open %s:%s\n", argv[1], strerror(errno));
		exit(1);
	}
	
	if (ioctl(gsm_spans[0].fd, DAHDI_SPECIFY, &x) == -1) {
		fprintf(stderr, "Unable to open D-channel %d(%s)\n", x, strerror(errno));
		exit(1);
	}

	if (ioctl(gsm_spans[0].fd, DAHDI_GET_PARAMS, &p) == -1) {
		fprintf(stderr, "Unable to get parameters for d-channel %d(%s)\n", x, strerror(errno));
		exit(1);
	}

	if ((p.sigtype != DAHDI_SIG_HDLCFCS) && (p.sigtype != DAHDI_SIG_HARDHDLC)) {
		close(gsm_spans[0].fd);
		fprintf(stderr, "D-channel %d is not in HDLC/FCS mode.\n", x);
		return -1;
	}

	memset(&si, 0, sizeof(si));
	res = ioctl(gsm_spans[0].fd, DAHDI_SPANSTAT, &si);
	if (res) {
		close(gsm_spans[0].fd);
		fprintf(stderr, "Unable to get span state for D-channel %d (%s)\n", x, strerror(errno));
	}

	if (!si.alarms) {
		gsm_spans[0].dchan_up = 1;
	} else {
		gsm_spans[0].dchan_up = 0;
	}

	memset(&bi, 0, sizeof(bi));
	bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.numbufs = 32;
	bi.bufsize = 1024;

	if (ioctl(gsm_spans[0].fd, DAHDI_SET_BUFINFO, &bi)) {
		fprintf(stderr, "Unable to set appropriate buffering on channel %d: %s\n", x, strerror(errno));
		close(gsm_spans[0].fd);
		return -1;
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

		fds[0].fd = gsm_spans[0].fd;
		fds[0].events = POLLIN | POLLPRI;
		fds[0].revents = 0;

		res = poll(fds, 1, POLL_INTERVAL);
		if (!res) {
			/* Timeout - do nothing */
		} else if (res < 0) {
			fprintf(stdout, "Failed to poll d-channel\n");
		} else {
			if (fds[0].revents & POLLPRI) {
				fprintf(stdout, "Event received on d-channel\n");
			}
			if (fds[0].revents & POLLIN) {
				res = read(gsm_spans[0].fd, inbuf, 1024);
				if (res < 0) {
					fprintf(stdout, "Failed to read d-channel\n");
					break;
				}
				/* fprintf(stdout, "Read [%s] len:%d\n", inbuf, res); */
				wat_span_process_read(gsm_spans[0].wat_span_id, inbuf, res);
			}

		}

		if (gsm_spans[0].send_sms) {
			gsm_spans[0].send_sms = 0;

			while (g_message_sent++ < g_message_count) {
				wat_sms_event_t sms_event;
				memset(&sms_event, 0, sizeof(sms_event));

				fprintf(stdout, "Sending SMS\n");
				
				
				sprintf(sms_event.called_num.digits, "6472671197");
				//sprintf(sms_event.called_num.digits, "6474024627");
				gsm_spans[0].wat_sms_id = (g_outbound_sms_id++) | 0x8;

				sprintf(sms_event.message, "This is a very long message \
											aaaaaaaaaaaaaaaaaaaaaaaaaa1 \
											bbbbbbbbbbbbbbbbbbbbbbbbbb2 \
											cccccccccccccccccccccccccc3 \
											dddddddddddddddddddddddddd4 \
											eeeeeeeeeeeeeeeeeeeeeeeeee5 \
											ffffffffffffffffffffffffff6 \
											gggggggggggggggggggggggggg7 \
											hhhhhhhhhhhhhhhhhhhhhhhhhh8 \
											iiiiiiiiiiiiiiiiiiiiiiiiii9 \
											jjjjjjjjjjjjjjjjjjjjjjjjjja");
				//sprintf(sms_event.message, "Hello");
				sms_event.len = strlen(sms_event.message);

				sms_event.type = WAT_SMS_TXT;

				wat_sms_req(gsm_spans[0].wat_span_id, gsm_spans[0].wat_sms_id, &sms_event);
			}
		}
	}

	fprintf(stdout, "Exiting...\n");

	if (wat_span_stop(gsm_spans[0].wat_span_id)) {
		fprintf(stderr, "Failed to stop span!!\n");
		return -1;
	}
	close(gsm_spans[0].fd);
	return 0;
}
