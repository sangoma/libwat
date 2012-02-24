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
#define WAT_DEBUG_UART_RAW			(1 << 0) /* Show raw uart reads */
#define WAT_DEBUG_UART_DUMP			(1 << 1) /* Show uart commands */
#define WAT_DEBUG_CALL_STATE		(1 << 2) /* Debug call states */
#define WAT_DEBUG_SPAN_STATE		(1 << 3) /* Debug call states */
#define WAT_DEBUG_AT_PARSE			(1 << 4) /* Debug how AT commands are parsed */
#define WAT_DEBUG_AT_HANDLE			(1 << 5) /* Debug how AT commands are scheduled/processed */
#define WAT_DEBUG_SMS_DECODE		(1 << 6) /* Debug how PDU is decoded */
#define WAT_DEBUG_SMS_ENCODE		(1 << 7) /* Debug how PDU is encoded */

//#define WAT_FUNC_DEBUG 1

/*ENUMS & Defines ******************************************************************/

#define WAT_MAX_SPANS		32
#define WAT_MAX_NUMBER_SZ	32 /* TODO: Find real max sizes based on specs */
#define WAT_MAX_NAME_SZ		24 /* TODO: Find real max sizes based on specs */
#define WAT_MAX_SMS_SZ		160
#define WAT_MAX_CMD_SZ		4000 /* TODO: Find real max sizes based on specs */
#define WAT_MAX_TYPE_SZ		12
#define WAT_MAX_OPERATOR_SZ	32	/* TODO: Find real max sizes based on specs */

#define WAT_MAX_CALLS_PER_SPAN			16
#define WAT_MAX_SMSS_PER_SPAN			64
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
	WAT_ALARM_SIM_ACCESS_FAIL,
	WAT_ALARM_INVALID,
} wat_alarm_t;

#define WAT_ALARM_STRINGS "Alarm Cleared", "No Signal", "Lo Signal", "SIM access failure", "Invalid"
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
	char model[32];
 	char manufacturer[32];
	char revision[32];
	char serial[32];
} wat_chip_info_t;

typedef struct _wat_sim_info {
	wat_number_t subscriber;
	char subscriber_type[WAT_MAX_TYPE_SZ];
	wat_number_t smsc; /* SMS Service Centre */
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

typedef enum {
	WAT_SMS_PDU_MTI_SMS_DELIVER,
	WAT_SMS_PDU_MTI_SMS_SUBMIT,
	WAT_SMS_PDU_MTI_INVALID,
} wat_sms_pdu_mti_t;

#define WAT_SMS_PDU_MTI_STRINGS "SMS-DELIVER", "SMS-SUBMIT", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_sms_pdu_mti, wat_sms_pdu_mti2str, wat_sms_pdu_mti_t);

/* Defined in GSM 03.38 */
typedef enum {
	WAT_SMS_PDU_DCS_GRP_GEN,				/* General */
	WAT_SMS_PDU_DCS_GRP_RESERVED,			/* Reserved coding groups */
	WAT_SMS_PDU_DCS_GRP_MWI_DISCARD_MSG,	/* Message Wating Indication Group: Discard Message */
	WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_1,	/* Message Waiting Indication Group: Store Message (type #1) */
	WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_2,	/* Message Waiting Indication Group: Store Message (type #1) */
	WAT_SMS_PDU_DCS_GRP_DATA_CODING,		/* Data coding/Message class */
	WAT_SMS_PDU_DCS_GRP_INVALID,
} wat_sms_pdu_dcs_grp_t;

#define WAT_SMS_PDU_DCS_GRP_STRINGS "General", "Reserved", "MWI-Discard Message", "MWI-Store Message", "MWI-Store Message", "Data coding", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_sms_pdu_dcs_grp, wat_sms_pdu_dcs_grp2str, wat_sms_pdu_dcs_grp_t);

typedef enum {
	WAT_SMS_PDU_DCS_MSG_CLASS_GENERAL,
	WAT_SMS_PDU_DCS_MSG_CLASS_ME_SPECIFIC,
	WAT_SMS_PDU_DCS_MSG_CLASS_SIM_SPECIFIC,
	WAT_SMS_PDU_DCS_MSG_CLASS_TE_SPECIFIC, /* See GSM TS 07.05 */
	WAT_SMS_PDU_DCS_MSG_CLASS_INVALID,
} wat_sms_pdu_dcs_msg_cls_t;

#define WAT_SMS_PDU_DCS_MESSAGE_CLASS_STRINGS "General", "ME-Specific", "SIM-Specific", "TE-Specific", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_sms_pdu_dcs_msg_cls, wat_sms_pdu_dcs_msg_cls2str, wat_sms_pdu_dcs_msg_cls_t);

typedef enum {
	WAT_SMS_PDU_DCS_IND_TYPE_VOICEMAIL_MSG_WAITING,
	WAT_SMS_PDU_DCS_IND_TYPE_FAX_MSG_WAITING,
	WAT_SMS_PDU_DCS_IND_TYPE_ELECTRONIC_MAIL_MSG_WAITING,
	WAT_SMS_PDU_DCS_IND_TYPE_OTHER_MSG_WAITING,
	WAT_SMS_PDU_DCS_IND_TYPE_INVALID,
} wat_sms_pdu_dcs_ind_type_t;

typedef enum {
	WAT_SMS_PDU_DCS_ALPHABET_DEFAULT,		/* ASCII */
	WAT_SMS_PDU_DCS_ALPHABET_8BIT,
	WAT_SMS_PDU_DCS_ALPHABET_UCS2,			/* 16 bit */
	WAT_SMS_PDU_DCS_ALPHABET_RESERVED,
	WAT_SMS_PDU_DCS_ALPHABET_INVALID,
} wat_sms_pdu_dcs_alphabet_t;

#define WAT_SMS_PDU_DCS_ALPHABET_STRINGS "default", "8-bit", "UCS2", "reserved", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_sms_pdu_dcs_alphabet, wat_sms_pdu_dcs_alphabet2str, wat_sms_pdu_dcs_alphabet_t);

typedef enum {
	WAT_SMS_PDU_VP_NOT_PRESENT,
	WAT_SMS_PDU_VP_ABSOLUTE,
	WAT_SMS_PDU_VP_RELATIVE,
	WAT_SMS_PDU_VP_ENHANCED,
	WAT_SMS_PDU_VP_INVALID,
} wat_sms_pdu_vp_type_t;
#define WAT_SMS_PDU_VP_STRINGS "not present", "absolute", "relative", "enhanced", "invalid"
WAT_STR2ENUM_P(wat_str2wat_sms_pdu_vp_type, wat_sms_pdu_vp_type2str, wat_sms_pdu_vp_type_t);

typedef struct _wat_sms_pdu_vp {
	wat_sms_pdu_vp_type_t type; /* Validity Period Format */
	union {
		uint8_t relative; /* Used when tp_vp == WAT_SMS_PDU_VP_RELATIVE see www.dreamfabric.com/sms/vp.html for description */
		/* WAT_SMS_PDU_VP_ABSOLUTE & WAT_SMS_PDU_VP_ENHANCED not implemented yet */
	} data;
} wat_sms_pdu_vp_t;

typedef struct _wat_sms_pdu_deliver {
	/* From  www.dreamfabric.com/sms/deliver_fo.html */
	uint8_t tp_rp:1; /* Reply Path */
	uint8_t tp_udhi:1; /* User data header indicator. 1 => User Data field starts with a header */
	uint8_t tp_sri:1; /* Status report indication. 1 => Status report is going to be returned to the SME */
	uint8_t tp_mms:1; /* More messages to send. 0 => There are more messages to send  */
	wat_sms_pdu_mti_t tp_mti; /* Message type indicator */
} wat_sms_pdu_deliver_t;

typedef struct _wat_sms_pdu_submit {
	/* From  www.dreamfabric.com/sms/submit_fo.html */

	uint8_t tp_rp:1; /* Reply Path */
	uint8_t tp_udhi:1; /* User data header indicator. 1 => User Data field starts with a header */
	uint8_t tp_srr:1; /* Status report request. 1 => Status report requested */
	uint8_t tp_rd:1; /* Reject duplicates */

	wat_sms_pdu_vp_t vp;
} wat_sms_pdu_submit_t;

typedef struct _wat_sms_pdu_dcs {
	wat_sms_pdu_dcs_grp_t grp;
	uint8_t compressed:1; /* Compression defined in GSM TS 03.42 */
	wat_sms_pdu_dcs_msg_cls_t msg_class; /* Message Class */
	uint8_t ind_active:1;	/* Set indication Active */
	wat_sms_pdu_dcs_ind_type_t ind_type;
	wat_sms_pdu_dcs_alphabet_t alphabet;	/* Note: alphabet parameter is ignored for outbound SMS, as libwat will switch to UCS2 if needed */
} wat_sms_pdu_dcs_t;

typedef struct _wat_sms_pdu_timestamp {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int timezone;
} wat_timestamp_t;

/* From 3GPP TS 03.40 V7.5.0, page 62 */
typedef enum {
	WAT_SMS_PDU_UDH_IEI_CONCATENATED_SMS_8BIT, /* Concatenated short messages, 8-bit reference number */
	WAT_SMS_PDU_UDH_IEI_SPECIAL_SMS_INDICATION, /* Special SMS Message Indication */
	WAT_SMS_PDU_UDH_IEI_RESERVED, /* Reserved */
	WAT_SMS_PDU_UDH_IEI_NOT_USED, /* Value not used to avoid misinterpretation as <LF> character */
	WAT_SMS_PDU_UDH_IEI_APPLICATION_PORT_8BIT, /* Application port addressing scheme, 8-bit address */
	WAT_SMS_PDU_UDH_IEI_APPLICATION_PORT_16BIT, /* Application port addressing scheme, 16-bit address */
	WAT_SMS_PDU_UDH_IEI_SMSC_CONTROL_PARAMETERS,	/* SMSC Control Parameters */
	WAT_SMS_PDU_UDH_IEI_UDH_SOURCE_INDICATOR, /* UDH source indicator */
	WAT_SMS_PDU_UDH_IEI_CONCATENATED_SMS_16BIT, /* Concatenated short messages, 16-bit reference number */
	WAT_SMS_PDU_UDH_IEI_WIRELESS_CONTROL_MESSAGE_PROTOCOL, /* Wireless Control Message Protocol */
	WAT_SMS_PDU_UDH_IEI_INVALID,
	/* 0A - 6F: Reserved for future use */
	/* 70 - 7F: SIM Toolkit Security Headers */
	/* 80 - 9F:	SME to SME specific use */
	/* A0 - BF: Reserved for future use */
 	/* C0 - DF: SC specific use */
 	/* E0 - FF: Reserved for future use */
} wat_sms_pdu_udh_iei_t;


typedef struct _wat_sms_pdu_udh {
	uint8_t tp_udhl;

	wat_sms_pdu_udh_iei_t iei;
	uint8_t iedl;
	uint8_t refnr;
	uint8_t total;
	uint8_t seq;
} wat_sms_pdu_udh_t;

typedef struct _wat_sms_event_pdu {
	wat_number_t smsc;
	union {
		wat_sms_pdu_deliver_t deliver;
		wat_sms_pdu_submit_t submit;
	} sms;
	
	uint8_t tp_pid;			/* Protocol Identifier */
	uint8_t tp_dcs;			/* Data coding scheme */
	uint8_t tp_message_ref;
	
	wat_sms_pdu_dcs_t dcs;	/* Values are derived from tp_dcs */
	uint8_t tp_udl;

	wat_sms_pdu_udh_t udh;	/* User Data Header */	
} wat_sms_event_pdu_t;

typedef enum _wat_sms_content_charset {
	WAT_SMS_CONTENT_CHARSET_ASCII,
	WAT_SMS_CONTENT_CHARSET_UTF8,
	WAT_SMS_CONTENT_CHARSET_INVALID
} wat_sms_content_charset_t;

#define WAT_SMS_CONTENT_CHARSET_STRINGS "ASCII", "UTF-8", "invalid"
WAT_STR2ENUM_P(wat_str2wat_sms_content_charset, wat_sms_content_charset2str, wat_sms_content_charset_t);

typedef enum _wat_sms_content_encoding {
	WAT_SMS_CONTENT_ENCODING_NONE,
	WAT_SMS_CONTENT_ENCODING_BASE64,
	WAT_SMS_CONTENT_ENCODING_HEX,	/* Not implement yet */
	WAT_SMS_CONTENT_ENCODING_INVALID,
} wat_sms_content_encoding_t;

#define WAT_SMS_CONTENT_ENCODING_STRINGS "none", "base64", "hex", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_sms_content_encoding, wat_sms_content_encoding2str, wat_sms_content_encoding_t);

typedef struct _wat_sms_content_t {
	wat_size_t len; 							/* Length of message */
	wat_sms_content_encoding_t encoding;		/* Encoding type (raw, base64, hex) */
	wat_sms_content_charset_t charset;			/* Character set (ascii, utf-8) */
	char data[2*WAT_MAX_SMS_SZ];					/* Message */
} wat_sms_content_t;

typedef struct _wat_sms_event {
	wat_number_t from;					/* Incoming SMS only */
	wat_number_t to;					/* Outgoing SMS only */
	wat_sms_type_t type;				/* PDU or Plain Text */
	wat_timestamp_t scts;				/* Incoming SMS only */
	wat_sms_event_pdu_t pdu;
	wat_sms_content_t content;
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

typedef enum {
	WAT_SPAN_STS_READY,			/* Span initialization is complete, we can now process external commands */
	WAT_SPAN_STS_SIGSTATUS,		/* Span signalling status changed */
	WAT_SPAN_STS_NETSTATUS, 	/* Span Network registration changed */
	WAT_SPAN_STS_ALARM,			/* Alarm is on or cleared */
	WAT_SPAN_STS_SIM_INFO_READY,	/* SIM information available */
} wat_span_status_type_t;

typedef struct _wat_span_status_t {
	wat_span_status_type_t type;

	union {
		wat_sigstatus_t sigstatus;
		wat_sim_info_t sim_info;
		wat_alarm_t alarm;
	} sts;
} wat_span_status_t;

typedef enum {
	WAT_BAND_AUTO,			/* Automatic Band Negotiation */
	WAT_BAND_900_1800,		/* GSM 900 MHz + DCS 1800 MHz */
	WAT_BAND_900_1900,		/* GSM 900 MHz + PCS 1900 MHz */
	WAT_BAND_850_1800,		/* GSM 850 MHz + DCS 1800 MHz */
	WAT_BAND_850_1900,		/* GSM 850 MHz + PCS 1900 MHz */
	WAT_BAND_INVALID,
} wat_band_t;

#define WAT_BAND_STRINGS "auto", "900-1800", "900-1900", "850-1800" , "850-1900", "Invalid"
WAT_STR2ENUM_P(wat_str2wat_band, wat_band2str, wat_band_t);

typedef struct _wat_span_config_t {
	wat_moduletype_t moduletype;	

	/* Timeouts */
	uint32_t timeout_cid_num; /* Timeout to wait for a CLIP */
	uint32_t timeout_command;	/* General timeout to for the chip to respond to a command */
	uint32_t timeout_wait_sim; /* Timeout to wait for SIM to respond */
	uint32_t cmd_interval;		/* Minimum amount of time between sending 2 commands to the chip */
	uint32_t progress_poll_interval; /* How often to check for call status on outbound call */
	uint32_t signal_poll_interval;	/* How often to check for signal quality */
	uint8_t	signal_threshold; /* If the signal strength drops lower than this value in -dBM, we will report an alarm */
	uint32_t call_release_delay; /* After a call is hangup, delay before sending Rel Cfm to user. Sometimes, if we try to 
									call right after a hangup, the call fails, it looks like a grace period is needed between
									releasing a call and making  a new one */
	
	wat_band_t band;			/* Band frequency to be used */
	wat_codec_t codec_mask; /* Which codecs to advertise */

	wat_sms_content_encoding_t incoming_sms_encoding; /* Encoding to use on received SMS when not in ASCII */
} wat_span_config_t;

typedef void (*wat_span_sts_func_t)(uint8_t span_id, wat_span_status_t *status);
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
	wat_span_sts_func_t wat_span_sts;

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
WAT_DECLARE(void) wat_set_debug(uint32_t debug_mask);
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
WAT_DECLARE(const char*) wat_decode_pdu_mti(unsigned mti);
WAT_DECLARE(const char*) wat_decode_pdu_dcs(char *dest, wat_sms_pdu_dcs_t *dcs);
WAT_DECLARE(const char *) wat_decode_band(wat_band_t band);

WAT_DECLARE(const char*) wat_decode_timezone(char *dest, int timezone);

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
WAT_DECLARE(wat_band_t) wat_encode_band(const char *band);

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


