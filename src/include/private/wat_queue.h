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

