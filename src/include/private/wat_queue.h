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

#ifndef _WAT_QUEUE_H
#define _WAT_QUEUE_H


typedef struct wat_queue wat_queue_t;

wat_status_t wat_queue_create(wat_queue_t **outqueue, wat_size_t capacity);
wat_status_t wat_queue_destroy(wat_queue_t **inqueue);
wat_status_t wat_queue_enqueue(wat_queue_t *queue, void *obj);
void *wat_queue_dequeue(wat_queue_t *queue);
wat_bool_t wat_queue_empty(wat_queue_t *queue);


#endif /* _WAT_QUEUE_H */

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

