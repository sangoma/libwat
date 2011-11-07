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

#ifndef _LIBWAT_H
#define _LIBWAT_H


#include <stdlib.h>
#include <stdint.h>

#include "wat_declare.h"

#if 0
#define WAT_FUNC_DEBUG 1
#endif
/* Debugging */
#define WAT_DEBUG_UART_RAW		(1 << 0) /* Show raw uart reads */
#define WAT_DEBUG_UART_DUMP		(1 << 1) /* Show uart commands */
#define WAT_DEBUG_CALL_STATE	(1 << 2) /* Debug call states */
#define WAT_DEBUG_AT_PARSE		(1 << 3) /* Debug how AT commands are parsed */
#define WAT_DEBUG_AT_HANDLE		(1 << 4) /* Debug how AT commands are scheduled/processed */

/*ENUMS & Defines ******************************************************************/

#define WAT_MAX_SPANS		32
#define WAT_MAX_NUMBER_SZ	32 /* DAVIDY TODO: Find real max sizes based on specs */
#define WAT_MAX_NAME_SZ		24 /* DAVIDY TODO: Find real max sizes based on specs */
#define WAT_MAX_SMS_SZ		1024 /* DAVIDY TODO: Find real max sizes based on specs */
#define WAT_MAX_CMD_SZ		512 /* DAVIDY TODO: Find real max sizes based on specs */

#define WAT_MAX_CALLS_PER_SPAN			16
#define WAT_MAX_SMSS_PER_SPAN			16

typedef size_t wat_size_t;

typedef enum {
	WAT_SIGSTATUS_DOWN,
	WAT_SIGSTATUS_UP,
} wat_sigstatus_t;

typedef enum {
	WAT_ALARM_NO_SIGNAL,
	WAT_ALARM_NO_SIM,
} wat_alarm_t;

typedef enum {
	WAT_SMS_TXT,
	WAT_SMS_PDU,
} wat_sms_type_t;

typedef enum {
	WAT_MODULE_TELIT,
	WAT_MODULE_INVALID,
} wat_moduletype_t;

#define WAT_MODULETYPE_STRINGS "telit", "invalid"
WAT_STR2ENUM_P(wat_str2wat_moduletype, wat_moduletype2str, wat_moduletype_t);

typedef enum {
	WAT_NET_NOT_REGISTERED = 0,				/* Initial state */
	WAT_NET_REGISTERED_HOME,				/* Registered to home network */
	WAT_NET_NOT_REGISTERED_SEARCHING,		/* Not Registered, searching for an operator */
	WAT_NET_REGISTRATION_DENIED,			/* Registration denied */
	WAT_NET_UNKNOWN,						/* Unknown */
	WAT_NET_REGISTERED_ROAMING,				/* Registered, roaming */
	WAT_NET_INVALID,
} wat_net_stat_t;

#define WAT_NET_STAT_STRINGS "Not Registered", "Registered Home", "Not Registered, Searching", "Registration Denied", "Unknown", "Registered Roaming", "Invalid"

WAT_STR2ENUM_P(wat_str2wat_net_stat, wat_net_stat2str, wat_net_stat_t);

typedef enum {
	WAT_NUMBER_TYPE_UNKNOWN,
	WAT_NUMBER_TYPE_INTERNATIONAL,
	WAT_NUMBER_TYPE_INVALID,
} wat_number_type_t;

#define WAT_NUMBER_TYPE_STRINGS "unknown", "international" ,"invalid"

WAT_STR2ENUM_P(wat_str2wat_number_type, wat_number_type2str, wat_number_type_t);

typedef enum {
	WAT_NUMBER_PLAN_UNKNOWN,
	WAT_NUMBER_PLAN_ISDN,
	WAT_NUMBER_PLAN_INVALID,
} wat_number_plan_t;

#define WAT_NUMBER_PLAN_STRINGS "unknown", "ISDN" ,"invalid"

WAT_STR2ENUM_P(wat_str2wat_number_plan, wat_number_plan2str, wat_number_plan_t);

typedef enum {
	WAT_NUMBER_VALIDITY_VALID,			/* CLI Number is valid */
	WAT_NUMBER_VALIDITY_WITHELD,		/* CLI has been withheld by originator */
	WAT_NUMBER_VALIDITY_UNAVAILABLE,	/* CLI unavailable due to interworking problems or limitation of originating network */
	WAT_NUMBER_VALIDITY_INVALID,
} wat_number_validity_t;

#define WAT_NUMBER_VALIDITY_STRINGS "valid", "witheld" ,"unavailable", "invalid"

WAT_STR2ENUM_P(wat_str2wat_number_validity, wat_number_validity2str, wat_number_validity_t);

typedef struct {
	char digits [WAT_MAX_NUMBER_SZ];
	wat_number_type_t type;
	wat_number_plan_t plan;
	uint8_t validity;
} wat_number_t;

typedef enum {
	WAT_CALL_TYPE_VOICE,
	WAT_CALL_TYPE_DATA,
	WAT_CALL_TYPE_FAX,
	WAT_CALL_TYPE_INVALID,
} wat_call_type_t;

#define WAT_CALL_TYPE_STRINGS "voice", "data", "fax", "invalid"
WAT_STR2ENUM_P(wat_str2wat_call_type, wat_call_type2str, wat_call_type_t);

typedef enum {
	WAT_CALL_SUB_REAL,		/* Regular call */
	WAT_CALL_SUB_CALLWAIT,	/* Call Waiting */
	WAT_CALL_SUB_THREEWAY,	/* Three-way call */
	WAT_CALL_SUB_INVALID, 
} wat_call_sub_t;

#define WAT_CALL_SUB_STRINGS "real", "call waiting", "three-way", "invalid"
WAT_STR2ENUM_P(wat_str2wat_call_sub, wat_call_sub2str, wat_call_sub_t);

typedef enum {
	WAT_CALL_HANGUP_CAUSE_NORMAL,
	WAT_CALL_HANGUP_CAUSE_INVALID,
} wat_call_hangup_cause_t;

#define WAT_CALL_HANGUP_CAUSE_STRINGS "normal", "invalid"
WAT_STR2ENUM_P(wat_str2wat_call_hangup_cause, wat_call_hangup_cause2str, wat_call_hangup_cause_t);

typedef struct {
	wat_net_stat_t stat;
	uint8_t lac;	/* Local Area Code for the currently registered on cell */
	uint8_t ci;		/* Cell Id for currently registered on cell */

	uint8_t rssi;
	uint8_t ber;
} wat_net_info_t;

typedef enum {
	WAT_LOG_CRIT,
	WAT_LOG_ERROR,
	WAT_LOG_WARNING,
	WAT_LOG_INFO,
	WAT_LOG_NOTICE,
	WAT_LOG_DEBUG,
} wat_loglevel_t;

/* Structures  *********************************************************************/
typedef struct _wat_con_event {
	wat_call_type_t type;
	wat_call_sub_t	sub;
	wat_number_t called_num;
	wat_number_t calling_num;
	char calling_name[WAT_MAX_NAME_SZ];
} wat_con_event_t;

typedef struct _wat_sms_event {
	
	wat_sms_type_t type;				/* PDU or Plain Text */
	uint32_t len;				/* Length of message */
	char message[WAT_MAX_SMS_SZ];	/* Message */
} wat_sms_event_t;

typedef struct _wat_rel_event {
	uint32_t cause;
} wat_rel_event_t;

typedef enum {
	WAT_STATUS_REASON_CALL_ID_INUSE = 1,
	WAT_STATUS_REASON_NO_MEM,
} wat_cmd_status_reason_t;

typedef struct _wat_cmd_status {
	int success:1;					/* Set to 1 if command was successful */
	wat_cmd_status_reason_t reason;	/* Reason for failure */

	/* Command specific information */
	union {
	} info;
} wat_cmd_status_t;

typedef struct _wat_span_config_t {
	wat_moduletype_t moduletype;
	uint32_t signal_poll_interval;

	/* Timeouts */
	uint32_t timeout_cid_num; /* Timeout to wait for a CLIP */
} wat_span_config_t;

typedef struct _wat_interface {
	/* Call-backs */
	void (*wat_sigstatus_change)(uint8_t span_id, wat_sigstatus_t sigstatus);
	void (*wat_alarm)(uint8_t span_id, wat_alarm_t alarm);

	/* Memory management */
	void *(*wat_malloc)(size_t size);
	void *(*wat_calloc)(size_t nmemb, size_t size);	
	void (*wat_free)(void *ptr);

	/* Logging */
	void (*wat_log)(uint8_t level, char *fmt, ...);
	void (*wat_log_span)(uint8_t span_id, uint8_t level, char *fmt, ...);

	/* Assert */
	void (*wat_assert)(char *message);

	/* Events */
	void (*wat_con_ind)(uint8_t span_id, uint8_t call_id, wat_con_event_t *con_event);
	void (*wat_con_cfm)(uint8_t span_id, uint8_t call_id, wat_cmd_status_t *status);
	void (*wat_rel_ind)(uint8_t span_id, uint8_t call_id, wat_rel_event_t *rel_event);
	void (*wat_rel_cfm)(uint8_t span_id, uint8_t call_id, wat_cmd_status_t *status);
	void (*wat_sms_ind)(uint8_t span_id, uint8_t call_id, wat_sms_event_t *sms_event);
	void (*wat_sms_cfm)(uint8_t span_id, uint8_t sms_id, wat_cmd_status_t *status);
	void (*wat_cmd_cfm)(uint8_t span_id, wat_cmd_status_t *status);
	void (*wat_span_write)(uint8_t span_id, void *data, uint32_t len);
} wat_interface_t;

/* Functions  *********************************************************************/
/* DAVIDY: TODO: add Doxygen headers */
WAT_DECLARE(void) wat_version(uint8_t *current, uint8_t *revision, uint8_t *age);
WAT_DECLARE(wat_status_t) wat_register(wat_interface_t *interface);
WAT_DECLARE(wat_status_t) wat_span_config(uint8_t span_id, wat_span_config_t *span_config);
WAT_DECLARE(wat_status_t) wat_span_unconfig(unsigned char span_id);
WAT_DECLARE(wat_status_t) wat_span_start(uint8_t span_id);
WAT_DECLARE(wat_status_t) wat_span_stop(uint8_t span_id);
WAT_DECLARE(void) wat_span_process_read(uint8_t span_id, void *data, uint32_t len);
WAT_DECLARE(uint32_t) wat_span_schedule_next(uint8_t span_id);
WAT_DECLARE(void) wat_span_run(uint8_t span_id);

WAT_DECLARE(void) wat_span_get_chip_info(uint8_t span_id, char *manufacturer_name, char *manufacturer_id, char *revision_id, char *serial_number, char *imsi, char *subscriber_number);
WAT_DECLARE(wat_status_t) wat_span_get_netinfo(uint8_t span_id, wat_net_info_t *net_info);


WAT_DECLARE(wat_status_t) wat_con_cfm(uint8_t span_id, uint8_t call_id);
WAT_DECLARE(wat_status_t) wat_con_req(uint8_t span_id, uint8_t call_id, wat_con_event_t *con_event);
WAT_DECLARE(wat_status_t) wat_rel_req(uint8_t span_id, uint8_t call_id);
WAT_DECLARE(wat_status_t) wat_rel_cfm(uint8_t span_id, uint8_t call_id);

#endif /* _LIBWAT_H */

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


