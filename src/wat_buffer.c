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

#include <stdarg.h>

#include "libwat.h"
#include "wat_internal.h"


wat_status_t wat_buffer_create(wat_buffer_t **outbuffer, wat_size_t capacity)
{
	wat_buffer_t *buffer = NULL;

	wat_assert_return (outbuffer, WAT_FAIL, "Buffer double pointer is null\n");
	wat_assert_return (capacity > 0, WAT_FAIL, "Buffer capacity is not bigger than 0\n");

	*outbuffer = NULL;
	
	buffer = wat_calloc(1, sizeof(*buffer));
	if (!buffer) {
		return WAT_FAIL;
	}

	buffer->data = wat_calloc(1, (sizeof(void*)*capacity));
	if (!buffer->data) {
		goto failed;
	}

	if (wat_mutex_create(&buffer->mutex) != WAT_SUCCESS) {
		goto failed;
	}
	
	buffer->capacity = capacity;
	buffer->windex = 0;
	buffer->rindex = 0;
	buffer->size = 0;
	*outbuffer = buffer;
	return WAT_SUCCESS;
	
failed:
	if (buffer) {
		if (buffer->mutex) {
			wat_mutex_destroy(&buffer->mutex);
		}
		wat_safe_free(buffer->data);
		wat_safe_free(buffer);
	}	
	return WAT_FAIL;
}

wat_status_t wat_buffer_destroy(wat_buffer_t **inbuffer)
{
	wat_buffer_t *buffer = NULL;

	wat_assert_return(inbuffer != NULL, WAT_FAIL, "Buffer is null!\n");
	wat_assert_return(*inbuffer != NULL, WAT_FAIL, "Buffer is null!\n");

	buffer = *inbuffer;
	wat_mutex_destroy(&buffer->mutex);
	wat_safe_free(buffer->data);
	wat_safe_free(buffer);
	*inbuffer = NULL;
	
	return WAT_SUCCESS;
}


wat_status_t wat_buffer_enqueue(wat_buffer_t *buffer, void *data, wat_size_t len)
{
	uint8_t *buffer_data = (uint8_t*)buffer->data;
	uint8_t *in_data = data;

	wat_size_t write_before_wrap = 0;
	wat_size_t write_after_wrap = 0;
	
	wat_mutex_lock(buffer->mutex);
	if ((buffer->size + len) > buffer->capacity) {
		wat_mutex_unlock(buffer->mutex);
		wat_log(WAT_LOG_ERROR, "buffer is full\n");
		return WAT_FAIL;
	}

	/* Find out how many bytes we can write before wrap around */
	write_before_wrap = buffer->capacity - buffer->windex;
	if (write_before_wrap <= len) {
		/* See if we need there is more data to write */
		write_after_wrap = len - write_before_wrap;
	} else {
		/* There is no need to wrap around */
		write_before_wrap = len;
	}

	/* Write what we can before wrap around */
	memcpy(&buffer_data[buffer->windex], in_data, write_before_wrap);
	
	buffer->windex += write_before_wrap;

	if (buffer->windex == buffer->capacity) {
		/* Wrap around */
		buffer->windex = 0;
	}

	/* We still have more data to write */
	if (write_after_wrap) {
		memcpy(buffer_data, &in_data[write_before_wrap], write_after_wrap);
		buffer->windex = write_after_wrap;
	}
	
	buffer->size += len;

	buffer->new_data = 1;
	wat_mutex_unlock(buffer->mutex);

	return WAT_SUCCESS;
}

/* Caller should make sure that data has enough space to store full buffer */
/* Will return all data currently in buffer, without dequeueing */
wat_status_t wat_buffer_peep(wat_buffer_t *buffer, void *data, wat_size_t *len)
{
	uint8_t *buffer_data = (uint8_t*)buffer->data;
	uint8_t *out_data = data;
	wat_size_t read_before_wrap = 0;
	wat_size_t read_after_wrap = 0;

	wat_mutex_lock(buffer->mutex);
	buffer->new_data = 0;

	if (!buffer->size) {
		wat_mutex_unlock(buffer->mutex);
		return WAT_FAIL;
	}

	*len = buffer->size;
	
	if (buffer->rindex < buffer->windex) {
		/* There is no wrap around, just copy data, and go */
		memcpy(out_data, &buffer_data[buffer->rindex], buffer->size);
		wat_mutex_unlock(buffer->mutex);
		return WAT_SUCCESS;
	}

	/* Wrap around */
	read_before_wrap = buffer->capacity - buffer->rindex;
	memcpy(out_data, &buffer_data[buffer->rindex], read_before_wrap);

	read_after_wrap = buffer->size - read_before_wrap;

	if (read_after_wrap) {
		memcpy(&out_data[read_before_wrap], buffer_data, read_after_wrap);
	}
	
	wat_mutex_unlock(buffer->mutex);

	return WAT_SUCCESS;
}

wat_bool_t wat_buffer_new_data(wat_buffer_t *buffer)
{
	if (buffer->new_data) {
		return WAT_TRUE;
	}
	return WAT_FALSE;
}

wat_status_t wat_buffer_dequeue(wat_buffer_t *buffer, void *data, wat_size_t len)
{
	uint8_t *buffer_data = (uint8_t *)buffer->data;
	uint8_t *out_data = data;
	wat_size_t read_before_wrap = 0;
	wat_size_t read_after_wrap = 0;

	wat_mutex_lock(buffer->mutex);
	/* We cannot dequeue more that what we currently have */
	if (buffer->size < len) {
		wat_mutex_unlock(buffer->mutex);
		return WAT_FAIL;
	}

	read_before_wrap = buffer->capacity - buffer->rindex;
	if (read_before_wrap <= len) {
		read_after_wrap = len - read_before_wrap;
	} else {
		read_before_wrap = len;
	}
	
	memcpy(data, &buffer_data[buffer->rindex], read_before_wrap);
	buffer->rindex += read_before_wrap;

	if (buffer->rindex == buffer->capacity) {
		buffer->rindex = 0;
	}

	if (read_after_wrap) {
		memcpy(&out_data[read_before_wrap], buffer_data, read_after_wrap);
		buffer->rindex = read_after_wrap;
	}

	buffer->size -= len;
	
	wat_mutex_unlock(buffer->mutex);
	return WAT_SUCCESS;
}

wat_status_t wat_buffer_flush(wat_buffer_t *buffer, wat_size_t len)
{
	unsigned read_before_wrap;
	
	wat_mutex_lock(buffer->mutex);
	/* We cannot flush more that what we currently have */
	if (buffer->size < len) {
		wat_mutex_unlock(buffer->mutex);
		return WAT_FAIL;
	}

	read_before_wrap = buffer->capacity - buffer->rindex;
	if (read_before_wrap <= len) {
		buffer->rindex = len - read_before_wrap;
	} else {
		buffer->rindex += len;
	}
	buffer->size -= len;
	wat_mutex_unlock(buffer->mutex);
	return WAT_SUCCESS;
}

wat_status_t wat_buffer_reset(wat_buffer_t *buffer)
{
	wat_mutex_lock(buffer->mutex);
	buffer->size = 0;
	buffer->rindex = 0;
	buffer->windex = 0;
	wat_mutex_unlock(buffer->mutex);
	return WAT_SUCCESS;
}
