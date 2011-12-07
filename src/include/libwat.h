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
#ifndef _LIBWAT_H
#define _LIBWAT_H


#include <stdlib.h>
#include <stdint.h>

#include "wat_declare.h"

/* Debugging */
#define WAT_DEBUG_UART_RAW		(1 << 0) /* Show raw uart reads */
#define WAT_DEBUG_UART_DUMP		(1 << 1) /* Show uart commands */
#define WAT_DEBUG_CALL_STATE	(1 << 2) /* Debug call states */
#define WAT_DEBUG_AT_PARSE		(1 << 3) /* Debug how AT commands are parsed */
#define WAT_DEBUG_AT_HANDLE		(1 << 4) /* Debug how AT commands are scheduled/processed */
#define WAT_DEBUG_SMS_DECODE	(1 << 5) /* Debug how PDU is decoded */

/*ENUMS & Defines ******************************************************************/

#define WAT_MAX_SPANS		32
#define WAT_MAX_NUMBER_SZ	32 /* TODO: Find real max sizes based on specs */
#define WAT_MAX_NAME_SZ		24 /* TODO: Find real max sizes based on specs */
#define WAT_MAX_SMS_SZ		1024 /* TODO: Find real max sizes based on specs */
#define WAT_MAX_CMD_SZ		2048 /* TODO: Find real max sizes based on specs */
#define WAT_MAX_TYPE_SZ		12
#define WAT_MAX_OPERATOR_SZ	32	/* TODO: Find real max sizes based on specs */

#define WAT_MAX_CALLS_PER_SPAN			16
#define WAT_MAX_SMSS_PER_SPAN			16
#define WAT_MAX_ERROR_SZ				40

#define WAT_MIN_DTMF_DURATION_MS 100
typedef size_t wat_size_t;

/*! \brief Codec modes enabled in a given span 
 * warning: Since telit GSM module is the first implemented
 * we made this definitions match the telit spec. There is
 * also a list of codec string names (WAT_CODEC_NAMES) that must be
 * kept in order with this list, do not change the order of
 * codecs here unless you really know what you're doing
 * */
typedef enum {
	WAT_CODEC_ALL    = 0,        /*! All modes enabled */
	WAT_CODEC_FR     = (1 << 0), /*! Full rate */
	WAT_CODEC_EFR    = (1 << 1), /*! Enhanced full rate */
	WAT_CODEC_HR     = (1 << 2), /*! Half rate */
	WAT_CODEC_AMR_FR = (1 << 3), /*! AMR full rate */
	WAT_CODEC_AMR_HR = (1 << 4), /*! AMR half rate */
} wat_codec_t;

/*! Valid names you can use as wat_encode_codec() list of codecs */
#define WAT_CODEC_NAMES "All", "Full-Rate", "Enhanced-Full-Rate", "Half-Rate", "AMR-Full-Rate", "AMR-Half-Rate"

typedef enum {
	WAT_SIGSTATUS_DOWN,
	WAT_SIGSTATUS_UP,
} wat_sigstatus_t;

typedef enum {
	WAT_ALARM_NONE,
	WAT_ALARM_NO_SIGNAL,
	WAT_ALARM_LO_SIGNAL,
	WAT_ALARM_INVALID,
} wat_alarm_t;

#define WAT_ALARM_STRINGS "Alarm Cleared", "No Signal", "Lo Signal", "Invalid"

WAT_STR2ENUM_P(wat_str2wat_alarm, wat_alarm2str, wat_alarm_t);


typedef enum {
	WAT_SMS_PDU,
	WAT_SMS_TXT,
} wat_sms_type_t;

typedef enum {
	WAT_SMS_CAUSE_QUEUE_FULL,
	WAT_SMS_CAUSE_MODE_NOT_SUPPORTED,
	WAT_SMS_CAUSE_NO_RESPONSE,
	WAT_SMS_CAUSE_NO_NETWORK,
	WAT_SMS_CAUSE_NETWORK_REFUSE,
	WAT_SMS_CAUSE_UNKNOWN,
} wat_sms_cause_t;

#define WAT_SMS_CAUSE_STRINGS "Queue full", "Mode not supported", "No response", "No network",  "Network Refused", "Unknown"

WAT_STR2ENUM_P(wat_str2wat_sms_cause, wat_sms_cause2str, wat_sms_cause_t);

typedef enum {
	WAT_MODULE_TELIT,
	WAT_MODULE_INVALID,
} wat_moduletype_t;

#define WAT_MODULETYPE_STRINGS "telit", "invalid"
WAT_STR2ENUM_P(wat_str2wat_moduletype, wat_moduletype2str, wat_moduletype_t);

typedef enum {
	WAT_NUMBER_TYPE_UNKNOWN,
	WAT_NUMBER_TYPE_INTERNATIONAL,
	WAT_NUMBER_TYPE_NATIONAL,
	WAT_NUMBER_TYPE_NETWORK_SPECIFIC,
	WAT_NUMBER_TYPE_SUBSCRIBER,
	WAT_NUMBER_TYPE_ALPHANUMERIC, /* Coded according to GSM TS 03.38 7-bit default alphabet */
	WAT_NUMBER_TYPE_ABBREVIATED,
	WAT_NUMBER_TYPE_RESERVED,
	WAT_NUMBER_TYPE_INVALID,
} wat_number_type_t;

#define WAT_NUMBER_TYPE_STRINGS "unknown", "international" , "national", "network specific", "subscriber", "alphanumeric", "abbreviated", "reserved", "invalid"

WAT_STR2ENUM_P(wat_str2wat_number_type, wat_number_type2str, wat_number_type_t);

typedef enum {
	WAT_NUMBER_PLAN_UNKNOWN,
	WAT_NUMBER_PLAN_ISDN,
	WAT_NUMBER_PLAN_DATA,
	WAT_NUMBER_PLAN_TELEX,
	WAT_NUMBER_PLAN_NATIONAL,
	WAT_NUMBER_PLAN_PRIVATE,
	WAT_NUMBER_PLAN_ERMES,	/* ETSI DE/PS 3 01-3 */
	WAT_NUMBER_PLAN_RESERVED,
	WAT_NUMBER_PLAN_INVALID,
} wat_number_plan_t;

#define WAT_NUMBER_PLAN_STRINGS "unknown", "ISDN", "data", "telex", "national", "private", "ermes", "reserved", "invalid"

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
	WAT_LOG_CRIT,
	WAT_LOG_ERROR,
	WAT_LOG_WARNING,
	WAT_LOG_INFO,
	WAT_LOG_NOTICE,
	WAT_LOG_DEBUG,
} wat_loglevel_t;

/* Structures  *********************************************************************/

typedef struct _wat_chip_info {
	char manufacturer_name[32];
 	char manufacturer_id[32];
	char revision[32];
	char serial[32];
} wat_chip_info_t;

typedef struct _wat_sim_info {
	wat_number_t subscriber;
	char subscriber_type[WAT_MAX_TYPE_SZ];
	char imsi[32];
} wat_sim_info_t;

typedef enum {
	WAT_NET_NOT_REGISTERED = 0,             /* Initial state */
	WAT_NET_REGISTERED_HOME,                /* Registered to home network */
	WAT_NET_NOT_REGISTERED_SEARCHING,       /* Not Registered, searching for an operator */
	WAT_NET_REGISTRATION_DENIED,            /* Registration denied */
	WAT_NET_UNKNOWN,                        /* Unknown */
	WAT_NET_REGISTERED_ROAMING,             /* Registered, roaming */
	WAT_NET_INVALID,
} wat_net_stat_t;

#define WAT_NET_STAT_STRINGS "Not Registered", "Registered Home", "Not Registered, Searching", "Registration Denied", "Unknown", "Registered Roaming", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_net_stat, wat_net_stat2str, wat_net_stat_t);

typedef enum {
	WAT_PIN_READY,					/* ME is not pending for any password */
	WAT_PIN_SIM_PIN,				/* ME is waiting for SIM PIN to be given */
	WAT_PIN_SIM_PUK,				/* ME is waiting for SIM PUK to be given */
	WAT_PIN_PH_SIM_PIN,				/* ME is waiting for phone-to-sim card password to be given */
	WAT_PIN_PH_FSIM_PIN,			/* ME is waiting phone-to-very first SIM card password to be given */
	WAT_PIN_PH_FSIM_PUK,			/* ME is waiting for phone-to-very first SIM card unblocking password to be given */
	WAT_PIN_SIM_PIN2,				/* ME is waiting for SIM PIN 2 to be given */
	WAT_PIN_SIM_PUK2,				/* ME is waiting for SIM PUK 2 to be given */
	WAT_PIN_PH_NET_PIN,				/* ME is waiting network personalization password to be given */
	WAT_PIN_PH_NET_PUK, 			/* ME is waiting network personalization unblocking password to be given */
	WAT_PIN_PH_NETSUB_PIN,			/* ME is waiting network subset personalization password to be given */
	WAT_PIN_PH_NETSUB_PUK,			/* ME is waiting netowrk subset personalization unblocking password to be given */
	WAT_PIN_PH_SP_PIN,				/* ME is waiting service provider personalization password to be given */
	WAT_PIN_PH_SP_PUK,				/* ME is waiting service provider personalization unblocking password to be given */
	WAT_PIN_PH_CORP_PIN,			/* ME is waiting corporate personalization password to be given */
	WAT_PIN_PH_CORP_PUK,			/* ME is waiting corporate personalization unblocking password to be given */
	WAT_PIN_PH_MCL_PIN,  			/* ME is waiting for Multi Country Lock password to be given */
	WAT_PIN_INVALID,				/* Invalid Response */
} wat_pin_stat_t;

#define WAT_PIN_STAT_STRINGS "Ready", "SIM PIN required", "SIM PUK required", "PH-SIM PIN required", \
						"PH-First SIM PIN required", "PH-First SIM PUK required", \
						"SIM PIN 2 required", "SIM PUK 2 required", \
						"PH-NET PIN required", "PH-NET PUK required", \
						"PH-NETSUB PIN required", "PH-NETSUB PUK required", \
						"PH-SP PIN required", "PH-SP PUK required" \
						"PH-CORP PIN required", "PH-CORP PUK required" \
						"PH-MCL PIN required", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_pin_stat, wat_pin_stat2str, wat_pin_stat_t);

#define WAT_PIN_CHIP_STAT_STRINGS "READY", "SIM PIN", "SIM PUK", "PH-SIM PIN", \
						"PH-FSIM PIN", "PH-FSIM PUK", \
						"SIM PIN2", "SIM PUK2" \
						"PH-NET PIN", "PH-NET PUK", \
						"PH-NETSUB PIN", "PH-NETSUB PUK", \
						"PH-SP PIN", "PH-SP PUK" \
						"PH-CORP PIN", "PH-CORP PUK" \
						"PH-MCL PIN", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_chip_pin_stat, wat_chip_pin_stat2str, wat_pin_stat_t);

typedef struct {
	wat_net_stat_t stat;
	uint8_t lac;	/* Local Area Code for the currently registered on cell */
	uint8_t ci;		/* Cell Id for currently registered on cell */
	char operator_name[WAT_MAX_OPERATOR_SZ];
} wat_net_info_t;

typedef struct {
	uint8_t rssi;
	uint8_t ber;
} wat_sig_info_t;

typedef struct _wat_con_event {
	wat_call_type_t type;
	wat_call_sub_t	sub;
	wat_number_t called_num;
	wat_number_t calling_num;
	char calling_name[WAT_MAX_NAME_SZ];
} wat_con_event_t;

typedef struct _wat_sms_pdu_deliver {
	/* From  www.dreamfabric.com/sms/deliver_fo.html */
	uint8_t tp_rp:1; /* Reply Path */
	uint8_t tp_udhi:1; /* User data header indicator. 1 => User Data field starts with a header */
	uint8_t tp_sri:1; /* Status report indication. 1 => Status report is going to be returned to the SME */
	uint8_t tp_mms:1; /* More messages to send. 0 => There are more messages to send  */
	uint8_t tp_mti:2; /* Message type indicator. 0 => this PDU is an SMS-DELIVER */
} wat_sms_pdu_deliver_t;

typedef struct _wat_sms_pdu_timestamp {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int timezone;
} wat_timestamp_t;

typedef struct _wat_sms_event_pdu {
	wat_number_t smsc;
	wat_sms_pdu_deliver_t sms_deliver;
	wat_number_t sender;
	
	uint8_t tp_pid;		/* Protocol Identifier */
	uint8_t tp_dcs;		/* Daca coding scheme */
	uint8_t tp_udl;

	uint8_t tp_udhl;

	uint8_t iei;
	uint8_t iedl;
	uint8_t refnr;
	uint8_t total;
	uint8_t seq;

} wat_sms_event_pdu_t;

typedef struct _wat_sms_event {
	wat_number_t calling_num;
	wat_number_t called_num;
	wat_sms_type_t type;				/* PDU or Plain Text */
	uint32_t len;						/* Length of message */
	wat_timestamp_t scts;
	wat_sms_event_pdu_t pdu;
	char message[WAT_MAX_SMS_SZ];		/* Message */
} wat_sms_event_t;

typedef struct _wat_rel_event {
	uint32_t cause;
	const char *error;
} wat_rel_event_t;

typedef enum _wat_con_status_type {
	WAT_CON_STATUS_TYPE_RINGING = 1,
	WAT_CON_STATUS_TYPE_ANSWER,
} wat_con_status_type_t;

typedef struct _wat_con_status {
	wat_con_status_type_t type;
} wat_con_status_t;

typedef struct _wat_sms_status {
	wat_bool_t success;
	wat_sms_cause_t cause;
	const char *error;
} wat_sms_status_t;

typedef struct _wat_cmd_status {
	wat_bool_t success;
	const char *error;
} wat_cmd_status_t;

typedef struct _wat_span_config_t {
	wat_moduletype_t moduletype;	

	/* Timeouts */
	uint32_t timeout_cid_num; /* Timeout to wait for a CLIP */
	uint32_t timeout_command;	/* General timeout to for the chip to respond to a command */
	uint32_t progress_poll_interval; /* How often to check for call status on outbound call */
	uint32_t signal_poll_interval;	/* How often to check for signal quality */
	uint8_t	signal_threshold; /* If the signal strength drops lower than this value in -dBM, we will report an alarm */
	wat_codec_t codec_mask; /* Which codecs to advertise */
} wat_span_config_t;

typedef void (*wat_sigstatus_change_func_t)(uint8_t span_id, wat_sigstatus_t sigstatus);
typedef void (*wat_alarm_func_t)(uint8_t span_id, wat_alarm_t alarm);
typedef void (*wat_log_func_t)(uint8_t level, char *fmt, ...);
typedef void* (*wat_malloc_func_t)(size_t size);
typedef void* (*wat_calloc_func_t)(size_t nmemb, size_t size);	
typedef void (*wat_free_func_t)(void *ptr);
typedef void (*wat_log_span_func_t)(uint8_t span_id, uint8_t level, char *fmt, ...);
typedef void (*wat_assert_func_t)(char *message);
typedef void (*wat_con_ind_func_t)(uint8_t span_id, uint8_t call_id, wat_con_event_t *con_event);
typedef void (*wat_con_sts_func_t)(uint8_t span_id, uint8_t call_id, wat_con_status_t *con_status);
typedef void (*wat_rel_ind_func_t)(uint8_t span_id, uint8_t call_id, wat_rel_event_t *rel_event);
typedef void (*wat_rel_cfm_func_t)(uint8_t span_id, uint8_t call_id);
typedef void (*wat_sms_ind_func_t)(uint8_t span_id, wat_sms_event_t *sms_event);
typedef void (*wat_sms_sts_func_t)(uint8_t span_id, uint8_t sms_id, wat_sms_status_t *sms_status);
typedef void (*wat_cmd_sts_func_t)(uint8_t span_id, wat_cmd_status_t *status);
typedef int (*wat_span_write_func_t)(uint8_t span_id, void *data, uint32_t len);

typedef struct _wat_interface {
	/* Call-backs */
	wat_sigstatus_change_func_t wat_sigstatus_change;
	wat_alarm_func_t wat_alarm;

	/* Memory management */
	wat_malloc_func_t wat_malloc;
	wat_calloc_func_t wat_calloc;
	wat_free_func_t wat_free;

	/* Logging */
	wat_log_func_t wat_log;
	wat_log_span_func_t wat_log_span;

	/* Assert */
	wat_assert_func_t wat_assert;

	/* Events */
	wat_con_ind_func_t wat_con_ind;
 	wat_con_sts_func_t wat_con_sts;
	wat_rel_ind_func_t wat_rel_ind;
	wat_rel_cfm_func_t wat_rel_cfm;
	wat_sms_ind_func_t wat_sms_ind;
	wat_sms_sts_func_t wat_sms_sts;
	wat_span_write_func_t wat_span_write;
} wat_interface_t;

/* Functions  *********************************************************************/
/* TODO: add Doxygen headers */
WAT_DECLARE(void) wat_version(uint8_t *current, uint8_t *revision, uint8_t *age);
WAT_DECLARE(wat_status_t) wat_register(wat_interface_t *interface);
WAT_DECLARE(wat_status_t) wat_span_config(uint8_t span_id, wat_span_config_t *span_config);
WAT_DECLARE(wat_status_t) wat_span_unconfig(unsigned char span_id);
WAT_DECLARE(wat_status_t) wat_span_start(uint8_t span_id);
WAT_DECLARE(wat_status_t) wat_span_stop(uint8_t span_id);
WAT_DECLARE(void) wat_span_process_read(uint8_t span_id, void *data, uint32_t len);
WAT_DECLARE(uint32_t) wat_span_schedule_next(uint8_t span_id);
WAT_DECLARE(void) wat_span_run(uint8_t span_id);

WAT_DECLARE(const wat_chip_info_t*) wat_span_get_chip_info(uint8_t span_id);
WAT_DECLARE(const wat_sim_info_t*) wat_span_get_sim_info(uint8_t span_id);
WAT_DECLARE(const wat_net_info_t*) wat_span_get_net_info(uint8_t span_id);
WAT_DECLARE(const wat_sig_info_t*) wat_span_get_sig_info(uint8_t span_id);
WAT_DECLARE(const wat_pin_stat_t*) wat_span_get_pin_info(uint8_t span_id);
WAT_DECLARE(wat_alarm_t) wat_span_get_alarms(uint8_t span_id);
WAT_DECLARE(const char *) wat_span_get_last_error(uint8_t span_id);

WAT_DECLARE(char*) wat_decode_rssi(char *dest, unsigned rssi);
WAT_DECLARE(const char*) wat_decode_alarm(unsigned alarm);
WAT_DECLARE(const char *) wat_decode_ber(unsigned ber);
WAT_DECLARE(const char *) wat_decode_sms_cause(uint32_t cause);
WAT_DECLARE(const char *) wat_decode_pin_status(wat_pin_stat_t pin_status);

#define WAT_AT_CMD_RESPONSE_ARGS (uint8_t span_id, char *tokens[], wat_bool_t success, void *obj, char *error)
#define WAT_AT_CMD_RESPONSE_FUNC(name) static int (name)  WAT_AT_CMD_RESPONSE_ARGS
typedef int (*wat_at_cmd_response_func) WAT_AT_CMD_RESPONSE_ARGS;

WAT_DECLARE(wat_status_t) wat_con_cfm(uint8_t span_id, uint8_t call_id);
WAT_DECLARE(wat_status_t) wat_con_req(uint8_t span_id, uint8_t call_id, wat_con_event_t *con_event);
WAT_DECLARE(wat_status_t) wat_rel_req(uint8_t span_id, uint8_t call_id);
WAT_DECLARE(wat_status_t) wat_rel_cfm(uint8_t span_id, uint8_t call_id);
WAT_DECLARE(wat_status_t) wat_sms_req(uint8_t span_id, uint8_t sms_id, wat_sms_event_t *sms_event);
WAT_DECLARE(wat_status_t) wat_cmd_req(uint8_t span_id, const char *at_cmd, wat_at_cmd_response_func cb, void *obj);
WAT_DECLARE(wat_status_t) wat_send_dtmf(uint8_t span_id, uint8_t call_id, const char *dtmf, wat_at_cmd_response_func cb, void *obj);
WAT_DECLARE(wat_status_t) wat_span_set_dtmf_duration(uint8_t span_id, int duration_ms);
WAT_DECLARE(wat_status_t) wat_span_set_codec(uint8_t span_id, wat_codec_t codec_mask);
WAT_DECLARE(wat_codec_t) wat_encode_codec(const char *codec);

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


