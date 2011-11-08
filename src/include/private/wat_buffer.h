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

#ifndef _WAT_BUFFER_H
#define _WAT_BUFFER_H


typedef struct {
	unsigned rindex;
	unsigned windex;
	wat_size_t capacity;
	wat_size_t size;
	wat_mutex_t *mutex;
	void **data;
} wat_buffer_t;

wat_status_t wat_buffer_create(wat_buffer_t **buffer, wat_size_t capacity);
wat_status_t wat_buffer_destroy(wat_buffer_t **buffer);

wat_status_t wat_buffer_enqueue(wat_buffer_t *buffer, void *data, wat_size_t len);
wat_status_t wat_buffer_peep(wat_buffer_t *buffer, void *data, wat_size_t *len);
wat_status_t wat_buffer_dequeue(wat_buffer_t *buffer, void *data, wat_size_t len);
wat_status_t wat_buffer_flush(wat_buffer_t *buffer, wat_size_t len);
wat_status_t wat_buffer_reset(wat_buffer_t *buffer);

#endif /* _WAT_BUFFER_H */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

/******************************************************************************/

