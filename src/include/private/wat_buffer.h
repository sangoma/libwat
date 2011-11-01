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

