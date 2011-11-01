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


#ifndef _WAT_MUTEX_H
#define _WAT_MUTEX_H

#include "libwat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wat_mutex wat_mutex_t;

WAT_DECLARE(wat_status_t) wat_mutex_create(wat_mutex_t **mutex);
WAT_DECLARE(wat_status_t) wat_mutex_destroy(wat_mutex_t **mutex);

#define wat_mutex_lock(_x) _wat_mutex_lock(__FILE__, __LINE__, __FUNCTION__, _x)
WAT_DECLARE(wat_status_t) _wat_mutex_lock(const char *file, int line, const char *func, wat_mutex_t *mutex);

#define wat_mutex_unlock(_x) _wat_mutex_unlock(__FILE__, __LINE__, __FUNCTION__, _x)
WAT_DECLARE(wat_status_t) _wat_mutex_unlock(const char *file, int line, const char *func, wat_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif /* _WAT_MUTEX_H */

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

