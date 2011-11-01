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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "libwat.h"

void *on_calloc(size_t nmemb, size_t size);
void on_free(void *ptr);
void on_log(unsigned char loglevel, char *fmt, ...);
void on_assert(char *message);

void on_assert(char *message)
{
	fprintf(stderr, "ASSERT!!!! %s\n", message);
	abort();
	return;
}

void *on_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void on_free(void *ptr)
{
	free(ptr);
	return;
}

void on_log(unsigned char loglevel, char *fmt, ...)
{
	char *data;
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vasprintf(&data, fmt, ap);
	if (ret == -1) {
		return;
	}

	fprintf(stdout, "libwat[%s]:%s",
								(loglevel==WAT_LOG_CRIT)? "CRIT":
								(loglevel==WAT_LOG_ERROR)? "ERROR":
								(loglevel==WAT_LOG_WARNING)? "WARNING":
								(loglevel==WAT_LOG_INFO)? "INFO":
								(loglevel==WAT_LOG_NOTICE)? "NOTICE":
										(loglevel==WAT_LOG_DEBUG)? "DEBUG": "UNKNOWN", data);

	if (data) free(data);
	return;
}

wat_interface_t test_interface = {
	.wat_log = on_log,
	.wat_calloc = on_calloc,
	.wat_free = on_free,
	.wat_assert = on_assert,
};
