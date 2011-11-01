/* 
 * Cross Platform Mutex abstraction
 * Copyright(C) 2007 Michael Jerris
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so.
 *
 * This work is provided under this license on an "as is" basis, without warranty of any kind,
 * either expressed or implied, including, without limitation, warranties that the covered code
 * is free of defects, merchantable, fit for a particular purpose or non-infringing. The entire
 * risk as to the quality and performance of the covered code is with you. Should any covered
 * code prove defective in any respect, you (not the initial developer or any other contributor)
 * assume the cost of any necessary servicing, repair or correction. This disclaimer of warranty
 * constitutes an essential part of this license. No use of any covered code is authorized hereunder
 * except under this disclaimer. 
 *
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 *
 */

#ifdef WIN32
#   if (_WIN32_WINNT < 0x0400)
#       error "Need to target at least Windows 95/WINNT 4.0 because TryEnterCriticalSection is needed"
#   endif
#   include <windows.h>
#endif
/*#define WAT_DEBUG_MUTEX 0*/

#define WAT_DEBUG_MUTEX 0


#include "libwat.h"
#include "wat_internal.h"
#include "wat_mutex.h"

#ifdef WIN32
#include <process.h>

#define WAT_THREAD_CALLING_CONVENTION __stdcall

struct wat_mutex {
	CRITICAL_SECTION mutex;
};

#else
#include <pthread.h>
#include <poll.h>

#include <errno.h>
#ifndef PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif /* PTHREAD_MUTEX_RECURSIVE */
				  
				 
#ifdef WAT_DEBUG_MUTEX
#define WAT_MUTEX_MAX_REENTRANCY 30
typedef struct wat_lock_entry {
	const char *file;
	const char *func;
	uint32_t line;
} wat_lock_entry_t;

typedef struct wat_lock_history {
	wat_lock_entry_t locked;
	wat_lock_entry_t unlocked;
} wat_lock_history_t;
#endif

struct wat_mutex {
	pthread_mutex_t mutex;
#ifdef WAT_DEBUG_MUTEX
	wat_lock_history_t lock_history[WAT_MUTEX_MAX_REENTRANCY];
	uint8_t reentrancy;
#endif
};

#endif

WAT_DECLARE(wat_status_t) wat_mutex_create(wat_mutex_t **mutex)
{
	wat_status_t status = WAT_FAIL;
	wat_mutex_t *check = NULL;

	check = (wat_mutex_t *)wat_calloc(1, sizeof(**mutex));
	if (!check)
		goto done;
#ifdef WIN32
	InitializeCriticalSection(&check->mutex);
#else
	if (pthread_mutex_init(&check->mutex, NULL))
		goto fail;

	goto success;

 fail:
	goto done;

 success:
#endif
	*mutex = check;
	status = WAT_SUCCESS;

 done:
	return status;
}

WAT_DECLARE(wat_status_t) wat_mutex_destroy(wat_mutex_t **mutex)
{
	wat_mutex_t *mp = *mutex;
	*mutex = NULL;
	if (!mp) {
		return WAT_FAIL;
	}
#ifdef WIN32
	DeleteCriticalSection(&mp->mutex);
#else
	if (pthread_mutex_destroy(&mp->mutex))
		return WAT_FAIL;
#endif
	wat_safe_free(mp);
	return WAT_SUCCESS;
}

#define ADD_LOCK_HISTORY(mutex, file, line, func) \
	{ \
		if ((mutex)->reentrancy < WAT_MUTEX_MAX_REENTRANCY) { \
			(mutex)->lock_history[mutex->reentrancy].locked.file = (file); \
			(mutex)->lock_history[mutex->reentrancy].locked.func = (func); \
			(mutex)->lock_history[mutex->reentrancy].locked.line = (line); \
			(mutex)->lock_history[mutex->reentrancy].unlocked.file = NULL; \
			(mutex)->lock_history[mutex->reentrancy].unlocked.func = NULL; \
			(mutex)->lock_history[mutex->reentrancy].unlocked.line = 0; \
			(mutex)->reentrancy++; \
			if ((mutex)->reentrancy == WAT_MUTEX_MAX_REENTRANCY) { \
				wat_log(WAT_LOG_ERROR, "Max reentrancy reached for mutex %p (%s:%s:%d)\n", (mutex), (file), (func), (line)); \
			} \
		} \
	}

WAT_DECLARE(wat_status_t) _wat_mutex_lock(const char *file, int line, const char *func, wat_mutex_t *mutex)
{
#ifdef WIN32
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	UNREFERENCED_PARAMETER(func);

	EnterCriticalSection(&mutex->mutex);
#else
	int err;
	if ((err = pthread_mutex_lock(&mutex->mutex))) {
		wat_log(WAT_LOG_ERROR, "Failed to lock mutex %d:%s\n", err, strerror(err));
		return WAT_FAIL;
	}
#endif
#ifdef WAT_DEBUG_MUTEX
	ADD_LOCK_HISTORY(mutex, file, line, func);
#endif
	return WAT_SUCCESS;
}

WAT_DECLARE(wat_status_t) _wat_mutex_unlock(const char *file, int line, const char *func, wat_mutex_t *mutex)
{
#ifdef WAT_DEBUG_MUTEX
	int i = 0;
	if (mutex->reentrancy == 0) {
		wat_log(WAT_LOG_ERROR, "Cannot unlock something that is not locked! (%s:%s:%d)\n", file, func, line);
		return WAT_FAIL;
	}
	i = mutex->reentrancy - 1;
	/* I think this is a fair assumption when debugging */
	if (func != mutex->lock_history[i].locked.func) {
		wat_log(WAT_LOG_WARNING, "Mutex %p was suspiciously locked at %s->%s:%d but unlocked (%s:%s:%d)\n",
				mutex, mutex->lock_history[i].locked.func, mutex->lock_history[i].locked.file, mutex->lock_history[i].locked.line,
				func, file, line);
	}
	mutex->lock_history[i].unlocked.file = file;
	mutex->lock_history[i].unlocked.line = line;
	mutex->lock_history[i].unlocked.func = func;
	mutex->reentrancy--;
#endif
#ifdef WIN32
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	UNREFERENCED_PARAMETER(func);

	LeaveCriticalSection(&mutex->mutex);
#else
	if (pthread_mutex_unlock(&mutex->mutex)) {
		wat_log(WAT_LOG_ERROR, "Failed to unlock mutex: %s\n", strerror(errno));
#ifdef WAT_DEBUG_MUTEX
		mutex->reentrancy++;
#endif
		return WAT_FAIL;
	}
#endif
	return WAT_SUCCESS;
}

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
