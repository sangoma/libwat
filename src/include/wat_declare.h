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


#ifndef __WAT_DECLARE_H__
#define __WAT_DECLARE_H__

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_XOPEN_SOURCE) && !defined(__FreeBSD__)
#define _XOPEN_SOURCE 600
#endif

#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 1
#endif
#ifndef HAVE_SYS_SOCKET_H
#define HAVE_SYS_SOCKET_H 1
#endif

#ifndef __WINDOWS__
#if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32) || defined(_WIN64)
#define __WINDOWS__
#endif
#endif

#ifdef _MSC_VER
#if defined(WAT_DECLARE_STATIC)
#define WAT_DECLARE(type)			type __stdcall
#define WAT_DECLARE_NONSTD(type)		type __cdecl
#define WAT_DECLARE_DATA
#elif defined(WAT_EXPORTS)
#define WAT_DECLARE(type)			__declspec(dllexport) type __stdcall
#define WAT_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define WAT_DECLARE_DATA				__declspec(dllexport)
#else
#define WAT_DECLARE(type)			__declspec(dllimport) type __stdcall
#define WAT_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define WAT_DECLARE_DATA				__declspec(dllimport)
#endif
#define WAT_DECLARE_INLINE(type)		extern __inline__ type /* why extern? see http://support.microsoft.com/kb/123768 */
#define EX_DECLARE_DATA				__declspec(dllexport)
#else
#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(HAVE_VISIBILITY)
#define WAT_DECLARE(type)		__attribute__((visibility("default"))) type
#define WAT_DECLARE_NONSTD(type)	__attribute__((visibility("default"))) type
#define WAT_DECLARE_DATA		__attribute__((visibility("default")))
#else
#define WAT_DECLARE(type)		type
#define WAT_DECLARE_NONSTD(type)	type
#define WAT_DECLARE_DATA
#endif
#define WAT_DECLARE_INLINE(type)		__inline__ type
#define EX_DECLARE_DATA
#endif

#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif
#if (_MSC_VER >= 1400)			/* VC8+ */
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif
#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif
#ifndef strncasecmp
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#undef HAVE_STRINGS_H
#undef HAVE_SYS_SOCKET_H
/* disable warning for zero length array in a struct */
/* this will cause errors on c99 and ansi compliant compilers and will need to be fixed in the wanpipe header files */
#pragma warning(disable:4706)
#pragma comment(lib, "Winmm")
#endif

#define WAT_ENUM_NAMES(_NAME, _STRINGS) static const char * _NAME [] = { _STRINGS , NULL };
#define WAT_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) WAT_DECLARE(_TYPE) _FUNC1 (const char *name); WAT_DECLARE(const char *) _FUNC2 (_TYPE type);
#define WAT_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)	\
	WAT_DECLARE(_TYPE) _FUNC1 (const char *name)			\
	{														\
		int i;												\
		_TYPE t = _MAX ;									\
															\
		for (i = 0; i < _MAX ; i++) {						\
			if (!strcasecmp(name, _STRINGS[i])) {			\
				t = (_TYPE) i;								\
				break;										\
			}												\
		}													\
															\
		return t;											\
	}														\
	WAT_DECLARE(const char *) _FUNC2 (_TYPE type)			\
	{														\
		if (type > _MAX) {									\
			type = _MAX;									\
		}													\
		return _STRINGS[(int)type];							\
	}														\

#ifdef __WINDOWS__
#include <stdio.h>
#include <windows.h>
#define WAT_INVALID_SOCKET INVALID_HANDLE_VALUE
typedef HANDLE wat_socket_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
#define WAT_O_BINARY O_BINARY
#define WAT_SIZE_FMT "Id"
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif /* _MSC_VER */
#else /* __WINDOWS__ */
#define WAT_O_BINARY 0
#define WAT_SIZE_FMT "zd"
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#endif

/*! \brief WAT APIs possible return codes */
typedef enum {
	WAT_SUCCESS, /*!< Success */
	WAT_FAIL, /*!< Failure, generic error return code when no more specific return code can be used */

	WAT_ENOMEM, /*!< Allocation failure */

	WAT_TIMEOUT, /*!< Operation timed out (ie: polling on a device)*/

	WAT_ENOSYS, /*!< Operation not implemented */

	WAT_BREAK,

	WAT_EINVAL, /*!< Invalid argument */
	WAT_EBUSY, /*!< Device busy */
} wat_status_t;

typedef enum {
	WAT_FALSE,
	WAT_TRUE,
} wat_bool_t;

#ifdef __cplusplus
} /* extern C */
#endif

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
