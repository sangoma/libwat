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
#ifndef _TESTWAT_UTILS_H
#define _TESTWAT_UTILS_H

void *on_malloc(size_t size);
void *on_calloc(size_t nmemb, size_t size);
void on_free(void *ptr);
void on_log(unsigned char loglevel, char *fmt, ...);
void on_log_span(unsigned char span_id, unsigned char loglevel, char *fmt, ...);
void on_assert(char *message);


#endif /* _TESTWAT_UTILS_H */
