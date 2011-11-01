/*
 * libwat: Wireless AT commands library
 *
 * Written by David Yat Sin <dyatsin@sangoma.com>
 *
 * Copyright (C) 2011, Sangoma Technologies.
 * All Rights Reserved.
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
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

#ifndef __WAT_SCHED_H__
#define __WAT_SCHED_H__

#define WAT_MICROSECONDS_PER_SECOND 1000000

typedef struct wat_sched wat_sched_t;
typedef void (*wat_sched_callback_t)(void *data);
typedef uint64_t wat_timer_id_t;

/*! \brief Create a new scheduling context */
WAT_DECLARE(wat_status_t) wat_sched_create(wat_sched_t **sched, const char *name);

/*! \brief Run the schedule to find timers that are expired and run its callbacks */
WAT_DECLARE(wat_status_t) wat_sched_run(wat_sched_t *sched);

/*! 
 * \brief Schedule a new timer 
 * \param sched The scheduling context (required)
 * \param name Timer name, typically unique but is not required to be unique, any null terminated string is fine (required)
 * \param callback The callback to call upon timer expiration (required)
 * \param data Optional data to pass to the callback
 * \param timer Timer id pointer to store the id of the newly created timer. It can be null
 *              if you do not need to know the id, but you need this if you want to be able 
 *              to cancel the timer with wat_sched_cancel_timer
 */
WAT_DECLARE(wat_status_t) wat_sched_timer(wat_sched_t *sched, const char *name,
		int ms, wat_sched_callback_t callback, void *data, wat_timer_id_t *timer);

/*! 
 * \brief Cancel the timer
 * \param sched The scheduling context (required)
 * \param timer The timer to cancel (required)
 */
WAT_DECLARE(wat_status_t) wat_sched_cancel_timer(wat_sched_t *sched, wat_timer_id_t timer);

/*! \brief Destroy the context and all of the scheduled timers in it */
WAT_DECLARE(wat_status_t) wat_sched_destroy(wat_sched_t **sched);

/*! 
 * \brief Calculate the time to the next timer and return it 
 * \param sched The sched context
 * \param timeto The pointer to store the next timer time in milliseconds
 */
WAT_DECLARE(wat_status_t) wat_sched_get_time_to_next_timer(const wat_sched_t *sched, int32_t *timeto);


/*! \brief Global initialization, called just once */
WAT_DECLARE(wat_status_t) wat_sched_global_init(void);

#endif

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
