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
