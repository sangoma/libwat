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
#include <sys/time.h>
#include "wat_internal.h"

#ifdef __WINDOWS__
struct wat_timezone {
    int tz_minuteswest;         /* minutes W of Greenwich */
    int tz_dsttime;             /* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct wat_timezone *tz)
{
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    static int tzflag;
    if (NULL != tv) {
        GetSystemTimeAsFileTime(&ft);
        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;

        /*converting file time to unix epoch */
        tmpres /= 10;           /*convert into microseconds */
        tmpres -= DELTA_EPOCH_IN_MICROSECS;
        tv->tv_sec = (long) (tmpres / 1000000UL);
        tv->tv_usec = (long) (tmpres % 1000000UL);
    }
    if (NULL != tz) {
        if (!tzflag) {
            _tzset();
            tzflag++;
        }
        tz->tz_minuteswest = _timezone / 60;
        tz->tz_dsttime = _daylight;
    }
    return 0;
}
#endif /* __WINDOWS__ */

typedef struct wat_timer wat_timer_t;

struct wat_sched {
	char name[80];
	wat_timer_id_t currid;
	wat_mutex_t *mutex;
	wat_timer_t *timers;
	int freerun;
	wat_sched_t *next;
	wat_sched_t *prev;
};

struct wat_timer {
	char name[80];
	wat_timer_id_t id;
	struct timeval time;
	void *usrdata;
	wat_sched_callback_t callback;
	wat_timer_t *next;
	wat_timer_t *prev;
};

WAT_DECLARE(wat_status_t) wat_sched_create(wat_sched_t **sched, const char *name)
{
	wat_sched_t *newsched = NULL;

	wat_assert_return(sched != NULL, WAT_EINVAL, "invalid pointer\n");
	wat_assert_return(name != NULL, WAT_EINVAL, "invalid sched name\n");

	*sched = NULL;

	newsched = wat_calloc(1, sizeof(*newsched));
	if (!newsched) {
		return WAT_ENOMEM;
	}

	if (wat_mutex_create(&newsched->mutex) != WAT_SUCCESS) {
		goto failed;
	}

	strncpy(newsched->name, name, sizeof(newsched->name)-1);
	newsched->currid = 1;

	*sched = newsched;
	wat_log(WAT_LOG_DEBUG, "Created schedule %s\n", name);
	return WAT_SUCCESS;

failed:
	wat_log(WAT_LOG_CRIT, "Failed to create schedule\n");

	if (newsched) {
		if (newsched->mutex) {
			wat_mutex_destroy(&newsched->mutex);
		}
		wat_safe_free(newsched);
	}
	return WAT_FAIL;
}

WAT_DECLARE(wat_status_t) wat_sched_run(wat_sched_t *sched)
{
	wat_status_t status = WAT_FAIL;
	wat_timer_t *runtimer;
	wat_timer_t *timer;
	wat_sched_callback_t callback;
	int ms = 0;
	int rc = -1;
	void *data;
	struct timeval now;

	wat_assert_return(sched != NULL, WAT_EINVAL, "sched is null!\n");

tryagain:

	wat_mutex_lock(sched->mutex);

	rc = gettimeofday(&now, NULL);
	if (rc == -1) {
		wat_log(WAT_LOG_ERROR, "Failed to retrieve time of day\n");
		goto done;
	}

	timer = sched->timers;
	while (timer) {
		runtimer = timer;
		timer = runtimer->next;

		ms = ((runtimer->time.tv_sec - now.tv_sec) * 1000) +
		     ((runtimer->time.tv_usec - now.tv_usec) / 1000);

		if (ms <= 0) {

			if (runtimer == sched->timers) {
				sched->timers = runtimer->next;
				if (sched->timers) {
					sched->timers->prev = NULL;
				}
			}

			callback = runtimer->callback;
			data = runtimer->usrdata;
			if (runtimer->next) {
				runtimer->next->prev = runtimer->prev;
			}
			if (runtimer->prev) {
				runtimer->prev->next = runtimer->next;
			}

			runtimer->id = 0;
			wat_safe_free(runtimer);

			/* avoid deadlocks by releasing the sched lock before triggering callbacks */
			wat_mutex_unlock(sched->mutex);

			callback(data);
			/* after calling a callback we must start the scanning again since the
			 * callback or some other thread may have added or cancelled timers to 
			 * the linked list */
			goto tryagain;
		}
	}

	status = WAT_SUCCESS;

done:

	wat_mutex_unlock(sched->mutex);
#ifdef __WINDOWS__
	UNREFERENCED_PARAMETER(sched);
#endif

	return status;
}

WAT_DECLARE(wat_status_t) wat_sched_timer(wat_sched_t *sched, const char *name,
		int ms, wat_sched_callback_t callback, void *data, wat_timer_id_t *timerid)
{
	wat_status_t status = WAT_FAIL;
	struct timeval now;
	int rc = 0;
	wat_timer_t *newtimer;

	wat_assert_return(sched != NULL, WAT_EINVAL, "sched is null!\n");
	wat_assert_return(name != NULL, WAT_EINVAL, "timer name is null!\n");
	wat_assert_return(callback != NULL, WAT_EINVAL, "sched callback is null!\n");
	wat_assert_return(ms > 0, WAT_EINVAL, "milliseconds must be bigger than 0!\n");

	if (timerid) {
		*timerid = 0;
	}

	rc = gettimeofday(&now, NULL);
	if (rc == -1) {
		wat_log(WAT_LOG_ERROR, "Failed to retrieve time of day\n");
		return WAT_FAIL;
	}

	wat_mutex_lock(sched->mutex);

	newtimer = wat_calloc(1, sizeof(*newtimer));
	if (!newtimer) {
		goto done;
	}
	newtimer->id = sched->currid;
	sched->currid++;
	if (!sched->currid) {
		wat_log(WAT_LOG_NOTICE, "Timer id wrap around for sched %s\n", sched->name);
		/* we do not want currid to be zero since is an invalid id 
		 * TODO: check that currid does not exists already in the context, it'd be insane
		 * though, having a timer to live all that time */
		sched->currid++;
	}

	strncpy(newtimer->name, name, sizeof(newtimer->name)-1);
	newtimer->callback = callback;
	newtimer->usrdata = data;

	newtimer->time.tv_sec = now.tv_sec + (ms / 1000);
	newtimer->time.tv_usec = now.tv_usec + (ms % 1000) * 1000;
	if (newtimer->time.tv_usec >= WAT_MICROSECONDS_PER_SECOND) {
		newtimer->time.tv_sec += 1;
		newtimer->time.tv_usec -= WAT_MICROSECONDS_PER_SECOND;
	}

	if (!sched->timers) {
		sched->timers = newtimer;
	}  else {
		newtimer->next = sched->timers;
		sched->timers->prev = newtimer;
		sched->timers = newtimer;
	}

	if (timerid) {
		*timerid = newtimer->id;
	}

	status = WAT_SUCCESS;
done:

	wat_mutex_unlock(sched->mutex);
#ifdef __WINDOWS__
	UNREFERENCED_PARAMETER(sched);
	UNREFERENCED_PARAMETER(name);
	UNREFERENCED_PARAMETER(ms);
	UNREFERENCED_PARAMETER(callback);
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(timerid);
#endif
	return status;
}

WAT_DECLARE(wat_status_t) wat_sched_get_time_to_next_timer(const wat_sched_t *sched, int32_t *timeto)
{
	wat_status_t status = WAT_FAIL;
	int res = -1;
	int ms = 0;
	struct timeval currtime;
	wat_timer_t *current = NULL;
	wat_timer_t *winner = NULL;
	
	/* forever by default */
	*timeto = -1;

	wat_mutex_lock(sched->mutex);

	res = gettimeofday(&currtime, NULL);
	if (-1 == res) {
		wat_log(WAT_LOG_ERROR, "Failed to get next event time\n");
		goto done;
	}
	status = WAT_SUCCESS;
	current = sched->timers;
	while (current) {
		/* if no winner, set this as winner */
		if (!winner) {
			winner = current;
		}
		current = current->next;
		/* if no more timers, return the winner */
		if (!current) {
			ms = (((winner->time.tv_sec - currtime.tv_sec) * 1000) + 
			     ((winner->time.tv_usec - currtime.tv_usec) / 1000));

			/* if the timer is expired already, return 0 to attend immediately */
			if (ms < 0) {
				*timeto = 0;
				break;
			}
			*timeto = ms;
			break;
		}

		/* if the winner timer is after the current timer, then we have a new winner */
		if (winner->time.tv_sec > current->time.tv_sec
		    || (winner->time.tv_sec == current->time.tv_sec &&
		       winner->time.tv_usec > current->time.tv_usec)) {
			winner = current;
		}
	}

done:
	wat_mutex_unlock(sched->mutex);
#ifdef __WINDOWS__
	UNREFERENCED_PARAMETER(timeto);
	UNREFERENCED_PARAMETER(sched);
#endif

	return status;
}

WAT_DECLARE(wat_status_t) wat_sched_cancel_timer(wat_sched_t *sched, wat_timer_id_t timerid)
{
	wat_status_t status = WAT_FAIL;
	wat_timer_t *timer;

	wat_assert_return(sched != NULL, WAT_EINVAL, "sched is null!\n");

	if (!timerid) {
		return WAT_SUCCESS;
	}

	wat_mutex_lock(sched->mutex);

	/* look for the timer and destroy it */
	for (timer = sched->timers; timer; timer = timer->next) {
		if (timer->id == timerid) {
			if (timer == sched->timers) {
				/* it's the head timer, put a new head */
				sched->timers = timer->next;
			}
			if (timer->prev) {
				timer->prev->next = timer->next;
			}
			if (timer->next) {
				timer->next->prev = timer->prev;
			}
			wat_safe_free(timer);
			status = WAT_SUCCESS;
			break;
		}
	}

	wat_mutex_unlock(sched->mutex);

	return status;
}

WAT_DECLARE(wat_status_t) wat_sched_destroy(wat_sched_t **insched)
{
	wat_sched_t *sched = NULL;
	wat_timer_t *timer;
	wat_timer_t *deltimer;
	wat_assert_return(insched != NULL, WAT_EINVAL, "sched is null!\n");
	wat_assert_return(*insched != NULL, WAT_EINVAL, "sched is null!\n");

	sched = *insched;

	/* if we have a previous member (then we were not head) set our previous next to our next */
	if (sched->prev) {
		sched->prev->next = sched->next;
	}
	/* if we have a next then set their prev to our prev (if we were head prev will be null and sched->next is already the new head) */
	if (sched->next) {
		sched->next->prev = sched->prev;
	}

	/* now grab the sched mutex */
	wat_mutex_lock(sched->mutex);

	timer = sched->timers;
	while (timer) {
		deltimer = timer;
		timer = timer->next;
		wat_safe_free(deltimer);
	}

	wat_log(WAT_LOG_DEBUG, "Destroying schedule %s\n", sched->name);

	wat_mutex_unlock(sched->mutex);

	wat_mutex_destroy(&sched->mutex);

	wat_safe_free(sched);

	*insched = NULL;
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
