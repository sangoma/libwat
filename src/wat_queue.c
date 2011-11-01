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

#include "libwat.h"
#include "wat_internal.h"

struct wat_queue {
	wat_mutex_t *mutex;
	wat_size_t	capacity;
	wat_size_t	size;
	uint32_t rindex;
	uint32_t windex;
	void **elements;
};

wat_status_t wat_queue_create(wat_queue_t **outqueue, wat_size_t capacity)
{
	wat_queue_t *queue = NULL;

	wat_assert_return(outqueue, WAT_FAIL, "Queue double pointer is null\n");
	wat_assert_return(capacity > 0 , WAT_FAIL, "Queue capacity is not bigger than 0\n");

	*outqueue = NULL;
	queue = wat_calloc(1, sizeof(*queue));
	wat_assert_return(queue, WAT_FAIL, "Failed to alloc mem\n");

	queue->elements = wat_calloc(1, sizeof(void*)*capacity);
	if (!queue->elements) {
		goto failed;
	}
	queue->capacity = capacity;

	if (wat_mutex_create(&queue->mutex) != WAT_SUCCESS) {
		goto failed;
	}

	*outqueue = queue;
	return WAT_SUCCESS;

failed:
	if (queue) {
		wat_mutex_destroy(&queue->mutex);
		wat_safe_free(queue->elements);
		wat_safe_free(queue);
	}
	
	return WAT_FAIL;
}

wat_status_t wat_queue_enqueue(wat_queue_t *queue, void *obj)
{
	wat_status_t status = WAT_FAIL;
	
	wat_assert_return(queue, WAT_FAIL, "Queue is null\n");

	wat_mutex_lock(queue->mutex);

	if (queue->windex == queue->capacity) {
		/* try to see if we can wrap around */
		queue->windex = 0;
	}

	if (queue->size != 0 && queue->windex == queue->rindex) {
		wat_log(WAT_LOG_ERROR, "Failed to enqueue obj %p in queue %p, no more room! windex == rindex == %d!\n", obj, queue, queue->windex);
		goto done;
	}

	queue->elements[queue->windex++] = obj;
	queue->size++;
	status = WAT_SUCCESS;

done:
	wat_mutex_unlock(queue->mutex);

	return status;
}

wat_bool_t wat_queue_empty(wat_queue_t *queue)
{
	return (queue->size) ? WAT_FALSE : WAT_TRUE;
}

void *wat_queue_dequeue(wat_queue_t *queue)
{
	void *obj = NULL;

	wat_assert_return(queue, NULL, "Queue is null!");
	wat_mutex_lock(queue->mutex);

	if (queue->size == 0) {
		goto done;
	}

	obj = queue->elements[queue->rindex];
	queue->elements[queue->rindex++] = NULL;
	queue->size--;
	if (queue->rindex == queue->capacity) {
		queue->rindex = 0;
	}

done:
	wat_mutex_unlock(queue->mutex);

	return obj;
}

wat_status_t wat_queue_destroy(wat_queue_t **inqueue)
{
	wat_queue_t *queue = NULL;
	wat_assert_return(inqueue, WAT_FAIL, "Queue is null!\n");
	wat_assert_return(*inqueue, WAT_FAIL, "Queue is null!\n");

	queue = *inqueue;
	wat_mutex_destroy(&queue->mutex);
	wat_safe_free(queue->elements);
	wat_safe_free(queue);
	*inqueue = NULL;

	return WAT_SUCCESS;
}
