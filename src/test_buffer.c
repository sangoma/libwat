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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <dahdi/user.h>

#include "libwat.h"
#include "wat_internal.h"

extern wat_interface_t test_interface;

int main (int argc, char *argv[])
{	
	wat_buffer_t *mybuffer;
	wat_status_t test_status = WAT_SUCCESS;

	/* TODO: Find a way to have unit tests that do not need to register an interface */

	/* Need an interface for calloc/free/assert/log */
	if (wat_register(&test_interface)) {
		fprintf(stderr, "Failed to register WAT Library !!!\n");
		return -1;
	}
	fprintf(stdout, "Registered interface to WAT Library\n");

	if (wat_buffer_create(&mybuffer, 200) != WAT_SUCCESS){
		fprintf(stdout, "Failed to create new buffer!!\n");
		return -1;
	}

	fprintf(stdout, "Running test #1\n");
	/* Test that cannot enqueue something that's bigger than capacity */
	{
		wat_status_t status;
		uint8_t data[300];
		memset(data, 5, sizeof(data));

		status = wat_buffer_enqueue(mybuffer, data, sizeof(data));
		if (status == WAT_SUCCESS) {
			fprintf(stderr, "Error buffer should be full\n");
			test_status = WAT_FAIL;
			goto done;
		} else {
			fprintf(stdout, "test #1 OK\n");
		}
		
		wat_buffer_reset(mybuffer);
	}	

	fprintf(stdout, "Running test #2\n");
	/* Test that buffer reports error if we try to enqueue too much */
	{
		int i;
		for (i = 0; i < 5; i++) {
			wat_status_t status;
			uint8_t data[50];
			memset(data, 5, sizeof(data));
			
			status = wat_buffer_enqueue(mybuffer, data, sizeof(data));
			if (i < 4) {
				if (status != WAT_SUCCESS) {
					fprintf(stderr, "Error buffer should not be full yet\n");
					test_status = WAT_FAIL;
				} else {
					fprintf(stdout, "\t enqueue #%d OK\n", i);
				}
			} else {
				if (status == WAT_SUCCESS) {
					fprintf(stderr, "Error buffer should be full by now\n");
					test_status = WAT_FAIL;
				} else {
					fprintf(stdout, "\t enqueue #%d OK\n", i);
				}
			}
			
		}
		if (test_status == WAT_SUCCESS) {
			fprintf(stdout, "test #2 OK\n");
		} else {
			fprintf(stdout, "test #2 Failed\n");
			goto done;
		}
		wat_buffer_reset(mybuffer);
	}

	fprintf(stdout, "Running test #3\n");
	/* Test that buffer can wrap around  */
	{
		int i;
		uint8_t data[50];
		memset(data, 5, sizeof(data));
		for (i = 0; i < 5000; i++) {
			wat_buffer_enqueue(mybuffer, data, 5);
			wat_buffer_dequeue(mybuffer, data, 5);

			wat_buffer_enqueue(mybuffer, data, 50);
			wat_buffer_dequeue(mybuffer, data, 42);
			wat_buffer_enqueue(mybuffer, data, 20);
			wat_buffer_dequeue(mybuffer, data, 28);
		}
		/* Buffer should be empty here */
		if (wat_buffer_dequeue(mybuffer, data, 1) != WAT_FAIL) {
			fprintf(stdout, "test #3 Failed\n");
			goto done;
		}

		wat_buffer_reset(mybuffer);
		fprintf(stdout, "test #3 OK\n");
	}

	fprintf(stdout, "Running test #4\n\n\n");
	/* Test buffer consistency */
	{
		uint8_t data[200];
		uint32_t len = 0;
		memset(data, 0, sizeof(data));
		
		wat_buffer_enqueue(mybuffer, "<word1>", 7);
		wat_buffer_enqueue(mybuffer, "<word2>", 7);
		wat_buffer_enqueue(mybuffer, "<word3>", 7);
		wat_buffer_enqueue(mybuffer, "<word4>", 7);

		memset(data, 0, sizeof(data));
		wat_buffer_peep(mybuffer, data, &len);
		fprintf(stdout, "data peep: [%s] len:%d\n", data, len);
		fprintf(stdout, "You should have [<word1><word2><word3><word4>], len:28   ");
		if (!memcmp(data, "<word1><word2><word3><word4>", 28)) {
			fprintf(stdout, "OK\n");
		} else {
			fprintf(stdout, "FAILED\n");
			test_status = WAT_FAIL;
			goto done;
		}

		fprintf(stdout, "\n");
		memset(data, 0, sizeof(data));
		wat_buffer_dequeue(mybuffer, data, 12);
		fprintf(stdout, "data dequeued: [%s] ", data);
		if (!memcmp(data, "<word1><word", 12)) {
			fprintf(stdout, "OK\n");
		} else {
			fprintf(stdout, "FAILED\n");
			test_status = WAT_FAIL;
			goto done;
		}

		fprintf(stdout, "\n");
		memset(data, 0, sizeof(data));
		wat_buffer_dequeue(mybuffer, data, 11);
		fprintf(stdout, "data dequeued: [%s] ", data);
		if (!memcmp(data, "2><word3><w", 11)) {
			fprintf(stdout, "OK\n");
		} else {
			fprintf(stdout, "FAILED\n");
			test_status = WAT_FAIL;
			goto done;
		}

		fprintf(stdout, "\n");
		memset(data, 0, sizeof(data));
		wat_buffer_peep(mybuffer, data, &len);
		fprintf(stdout, "data peep: [%s] len:%d ", data, len);
		if (len != 5) {
			fprintf(stdout, "len should be 5 (len:%d)\n", len);
			goto done;
		}
		if (!memcmp(data, "ord4>", len)) {
			fprintf(stdout, "OK\n");
		} else {
			fprintf(stdout, "FAILED\n");
			test_status = WAT_FAIL;
			goto done;
		}

		wat_buffer_reset(mybuffer);
		fprintf(stdout, "test #4 OK\n");
	}

	fprintf(stdout, "Running test #5\n\n\n");
	/* Test Wrap around */
	{
		int i;
		uint8_t data[200];
		uint32_t len = 0;
		memset(data, 0, sizeof(data));

		for (i = 0; i < 30; i++) {
			wat_buffer_enqueue(mybuffer, "<word>", 6);
		}

		for (i = 0; i < 30; i++) {
			wat_buffer_dequeue(mybuffer, data, 6);
		}
		
		/* At this point, we have 20 bytes before wrap around */
		for (i = 0; i < 5; i++) {
			wat_buffer_enqueue(mybuffer, "<hello>", strlen("<hello>"));
		}

		memset(data, 0, sizeof(data));
		wat_buffer_peep(mybuffer, data, &len);
		if (len != (strlen("<hello>")*5)) {
			fprintf(stdout, "Invalid length!!");
			test_status = WAT_FAIL;
			goto done;
		}

		fprintf(stdout, "data peep: [%s] len:%d ", data, len);
		if (!memcmp(data, "<hello><hello><hello><hello><hello>", len)) {
			fprintf(stdout, "OK\n");
		} else {
			fprintf(stdout, "FAILED\n");
			test_status = WAT_FAIL;
			goto done;
		}

		memset(data, 0, sizeof(data));
		wat_buffer_dequeue(mybuffer, data, strlen("<hello>")*4);
		
		fprintf(stdout, "data dequeue: [%s] len:%d ", data, len);
		if (!memcmp(data, "<hello><hello><hello><hello>", strlen("<hello>")*4)) {
			fprintf(stdout, "OK\n");
		} else {
			fprintf(stdout, "FAILED\n");
			test_status = WAT_FAIL;
			goto done;
		}

		memset(data, 0, sizeof(data));
		wat_buffer_dequeue(mybuffer, data, strlen("<hello>"));

		fprintf(stdout, "data dequeue: [%s] len:%d ", data, strlen("<hello>"));
		if (!memcmp(data, "<hello>", strlen("<hello>"))) {
			fprintf(stdout, "OK\n");
		} else {
			fprintf(stdout, "FAILED\n");
			test_status = WAT_FAIL;
			goto done;
		}
	}

	if (wat_buffer_destroy(&mybuffer) != WAT_SUCCESS){
		fprintf(stdout, "Failed to destroy buffer!!\n");
		return -1;
	}

done:
	if (test_status == WAT_SUCCESS) {
		fprintf(stdout, "All tests passed\n");
	} else {
		fprintf(stdout, "Some tests failed\n");
	}
	fprintf(stdout, "Test complete\n");
	return 0;
}


