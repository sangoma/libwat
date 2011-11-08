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
#include <stdarg.h>

#include "libwat.h"
#include "wat_internal.h"
#include "telit.h"

typedef enum {
	/*
		0 - active
		1 - held
		2 - dialing (MO call)
		3 - alerting (MO call)
		4 - incoming (MT call)
		5 - waiting	 (MT call)
	*/
	WAT_CLCC_STAT_ACTIVE,
	WAT_CLCC_STAT_HELD,
	WAT_CLCC_STAT_DIALING,
	WAT_CLCC_STAT_ALERTING,
	WAT_CLCC_STAT_INCOMING,
	WAT_CLCC_STAT_WAITING,
	WAT_CLCC_STAT_INVALID,
} wat_clcc_stat_t;

#define WAT_CLCC_STAT_STRINGS "active", "held", "dialing", "alerting", "incoming", "waiting"
WAT_STR2ENUM_P(wat_str2wat_clcc_stat, wat_clcc_stat2str, wat_clcc_stat_t);

WAT_ENUM_NAMES(WAT_CLCC_STAT_NAMES, WAT_CLCC_STAT_STRINGS)
WAT_STR2ENUM(wat_str2wat_clcc_stat, wat_clcc_stat2str, wat_clcc_stat_t, WAT_CLCC_STAT_NAMES, WAT_CLCC_STAT_INVALID)

typedef enum {
	WAT_CSQ_BER_0,
	WAT_CSQ_BER_1,
	WAT_CSQ_BER_2,
	WAT_CSQ_BER_3,
	WAT_CSQ_BER_4,
	WAT_CSQ_BER_5,
	WAT_CSQ_BER_6,
	WAT_CSQ_BER_7,
	WAT_CSQ_BER_NOT_DETECTABLE,
} wat_csq_ber_t;

#define WAT_CSQ_BER_STRINGS "less than 0.2%", "0.2 to 0.4%", "0.4 to 0.8%", "0.8 to 1.6%", "1.6 to 3.2%", "3.2 to 6.4%", "6.4 to 12.8%", "more than 12.8%", "not detectable"
WAT_STR2ENUM_P(wat_str2wat_csq_ber, wat_csq_ber2str, wat_csq_ber_t);

WAT_ENUM_NAMES(WAT_CSQ_BER_NAMES, WAT_CSQ_BER_STRINGS)
WAT_STR2ENUM(wat_str2wat_csq_ber, wat_csq_ber2str, wat_csq_ber_t, WAT_CSQ_BER_NAMES, WAT_CSQ_BER_NOT_DETECTABLE)

typedef struct {
	unsigned id;
	unsigned dir;
	unsigned stat;
} clcc_entry_t;

char *wat_decode_csq_rssi(char *in, unsigned rssi);
static int wat_cmd_entry_tokenize(char *entry, char *tokens[]);


static int wat_cmd_entry_tokenize(char *entry, char *tokens[])
{
	int token_count = 0;
	char *p = NULL;

	p = strtok(entry, ",");
	while (p != NULL) {
		tokens[token_count++] = wat_strdup(p);
		p = strtok(NULL, ",");
	}
	return token_count;
}

/* Get Module Manufacturer Name */
WAT_RESPONSE_FUNC(wat_response_cgmm)
{
	WAT_RESPONSE_FUNC_DBG_START	
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module manufacturer name\n");
		WAT_FUNC_DBG_END
		return;
	}

	strncpy(span->manufacturer_name, tokens[0], sizeof(span->manufacturer_name));
	WAT_FUNC_DBG_END
	return;
}

/* Get Module Manufacturer Identification */
WAT_RESPONSE_FUNC(wat_response_cgmi)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module manufacturer id\n");
		WAT_FUNC_DBG_END
		return;
	}

	strncpy(span->manufacturer_id, tokens[0], sizeof(span->manufacturer_id));
	WAT_FUNC_DBG_END
	return;
}

/* Get Module Revision Identification */
WAT_RESPONSE_FUNC(wat_response_cgmr)
{
	unsigned start = 0;
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module revision identification\n");
		WAT_FUNC_DBG_END
		return;
	}

	if (!strncmp(tokens[0], "Revision:", 9)) {
		start = 6;
	}

	strncpy(span->revision_id, &((tokens[0])[start]), sizeof(span->revision_id));
	WAT_FUNC_DBG_END
	return;
}

/* Get Module Serial Number */
WAT_RESPONSE_FUNC(wat_response_cgsn)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module serial number\n");
		WAT_FUNC_DBG_END
		return;
	}

	strncpy(span->serial_number, tokens[0], sizeof(span->serial_number));
	WAT_FUNC_DBG_END
	return;
}

/* Get Module IMSI */
WAT_RESPONSE_FUNC(wat_response_cimi)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module International Subscriber Identify\n");
		WAT_FUNC_DBG_END
		return;
	}

	strncpy(span->imsi, tokens[0], sizeof(span->imsi));
	WAT_FUNC_DBG_END
	return;
}

/* Enable Calling Line Presentation  */
WAT_RESPONSE_FUNC(wat_response_clip)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		span->clip = WAT_FALSE;
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable Calling Line Presentation\n");
		WAT_FUNC_DBG_END
		return;
	}

	span->clip = WAT_TRUE;
	WAT_FUNC_DBG_END
	return;
}

/* Network Registration Report */
WAT_RESPONSE_FUNC(wat_response_creg)
{
	char *cmdtokens[10];
	unsigned mode = 0;
 	unsigned stat = 0;
	unsigned lac = 0;
	unsigned ci = 0;
	
	WAT_RESPONSE_FUNC_DBG_START
	
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain Network Registration Report\n");
		WAT_FUNC_DBG_END
		return;
	}

	if (!strncmp(tokens[0], "+CREG: ", 7)) {
		int len = strlen(&(tokens[0])[7]);
		memmove(tokens[0], &(tokens[0])[7], len);
		memset(&tokens[0][len], 0, strlen(&tokens[0][len]));
	}
	
	memset(cmdtokens, 0, sizeof(cmdtokens));
	switch(wat_cmd_entry_tokenize(tokens[0], cmdtokens)) {
		case 4: /* Format: <mode>, <stat>[,<Lac>, <Ci>] */
			lac = atoi(cmdtokens[2]);
			ci = atoi(cmdtokens[3]);
			span->net_info.lac = lac;
			span->net_info.ci = ci;
			/* Fall-through */
		case 2: /* Format: <mode>, <stat> */
			mode = atoi(cmdtokens[0]);
			stat = atoi(cmdtokens[1]);
			wat_span_update_net_status(span, stat);
			break;	
		default:
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CREG Response %s\n", tokens[0]);
	}
	
	wat_free_tokens(cmdtokens);	
	
	WAT_FUNC_DBG_END
	return;
}

/* New Message Indications To Terminal Equipment */
WAT_RESPONSE_FUNC(wat_response_cnmi)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable New Messages Indications to TE\n");
		WAT_FUNC_DBG_END
		return;
	}
	WAT_FUNC_DBG_END
	return;
}

/* Set Operator Selection */
WAT_RESPONSE_FUNC(wat_response_cops)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable Operator Selection\n");
		WAT_FUNC_DBG_END
		return;
	}
	WAT_FUNC_DBG_END
	return;
}

WAT_RESPONSE_FUNC(wat_response_cnum)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain own number\n");
		WAT_FUNC_DBG_END
		return;
	}
	/* Format +CNUM: <number>, <type> */
	/* E.g +CNUM: "TELEPHONE","+16473380980",145,7,4 */

	if (!tokens[1]) {
		/* If this is a single token response,
		   then Subscriber Number is not available
		   on this SIM card
		*/
		sprintf(span->subscriber_number, "Not available");
		WAT_FUNC_DBG_END
		return;
	}

	if (!strncmp(tokens[0], "+CNUM: ", 6)) {
		int len = strlen(&(tokens[0])[6]);
		memmove(tokens[0], &(tokens[0])[6], len);
		memset(&tokens[0][len], 0, strlen(&tokens[0][len]));
	}	

	/* TODO: Do a complete parsing of the parameters */

	strncpy(span->subscriber_number, tokens[0], sizeof(span->subscriber_number));
	WAT_FUNC_DBG_END
	return;
}

WAT_RESPONSE_FUNC(wat_response_csq)
{
	unsigned rssi, ber;
	WAT_RESPONSE_FUNC_DBG_START

	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain Signal Strength\n");
		WAT_FUNC_DBG_END
		return;
	}

	if (!strncmp(tokens[0], "+CSQ: ", 6)) {
		int len = strlen(&(tokens[0])[6]);
		memmove(tokens[0], &(tokens[0])[6], len);
		memset(&tokens[0][len], 0, strlen(&tokens[0][len]));
	}
	
	if (sscanf(tokens[0], "%d,%d\n", &rssi, &ber) == 2) {
		char dest[30];
		span->net_info.rssi = rssi;
		span->net_info.ber = ber;
		wat_log_span(span, WAT_LOG_DEBUG, "Signal strength:%s (BER:%s)\n", wat_decode_csq_rssi(dest, rssi), wat_csq_ber2str(ber));
	} else {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CSQ %s\n", tokens[0]);
	}
	WAT_FUNC_DBG_END
	return;
}

WAT_RESPONSE_FUNC(wat_response_ata)
{
	wat_call_t *call = (wat_call_t*) obj;
	WAT_RESPONSE_FUNC_DBG_START

	if (success) {		
		wat_call_set_state(call, WAT_CALL_STATE_UP);
	} else {
		wat_log_span(span, WAT_LOG_INFO, "[id:%d] Failed to answer call\n", call->id);
		/* Schedule a CLCC to resync the call state */
		wat_cmd_enqueue(call->span, "AT+CLCC", wat_response_clcc, call);
	}
	
	WAT_FUNC_DBG_END
	return;
}

WAT_RESPONSE_FUNC(wat_response_ath)
{
	wat_call_t *call = (wat_call_t*) obj;
	WAT_RESPONSE_FUNC_DBG_START

	if (success) {
		wat_call_set_state(call, WAT_CALL_STATE_HANGUP_CMPL);
	} else {
		wat_log_span(span, WAT_LOG_ERROR, "[id:%d] Failed to hangup call\n", call->id);
		/* Schedule a CLCC to resync the call state */
		wat_cmd_enqueue(call->span, "AT+CLCC", wat_response_clcc, call);
	}
	
	WAT_FUNC_DBG_END
	return;
}

WAT_RESPONSE_FUNC(wat_response_atd)
{
	wat_call_t *call = (wat_call_t*) obj;
	WAT_RESPONSE_FUNC_DBG_START

	if (!success) {
		wat_log_span(span, WAT_LOG_ERROR, "[id:%d] Failed to make outbound call\n", call->id);
		/* Schedule a CLCC to resync the call state */
		wat_cmd_enqueue(call->span, "AT+CLCC", wat_response_clcc, call);
	}

	WAT_FUNC_DBG_END
	return;
}

WAT_RESPONSE_FUNC(wat_response_clcc)
{
	int i;
	unsigned num_clcc_entries = 0;
	clcc_entry_t entries[10];
	wat_iterator_t *iter, *curr;
	
	WAT_NOTIFY_FUNC_DBG_START

	memset(entries, 0, sizeof(entries));

	/* Format :
		+CLCC:<id1>, <dir>, <stat>, <mode>, <mpty>, <number>, <type>, <alpha>,
		+CLCC:<id2>, <dir>, <stat>, <mode>, <mpty>, <number>, <type>, <alpha>,
		+CLCC:<id3>, <dir>, <stat>, <mode>, <mpty>, <number>, <type>, <alpha>

		<idn>: Call Identification Number
		<dir>: Call direction
					0 - mobile originated call
					1 - mobile terminated call
		<stat>: state of the call
					0 - active
					1 - held
					2 - dialing (MO call)
					3 - alerting (MO call)
					4 - incoming (MT call)
					5 - waiting	 (MT call)
		<mode>: call type
					0 - voice
					1 - data
					2 - fax
					9 - unknown
		<mpty>:	multiparty call flag
					0 - call is not one of multiparty
					1 - call is one of multiparty
		<number>: string type phone number in format specified by <type>
		<type>: type of phone number
					129 - national numbering scheme
					145 - international numbering scheme (contains the character "+")
		<alpha>: string type, alphanumeric representation of <number> corresponding to entry found in phonebook
	*/

	if (!strncmp(tokens[0], "+CLCC: ", 7)) {
		int len = strlen(&(tokens[0])[7]);
		memmove(tokens[0], &(tokens[0])[7], len);
		memset(&tokens[0][len], 0, strlen(&tokens[0][len]));
	}

	for (i = 0; strncmp(tokens[i], "OK", 2); i++) {
		unsigned id, dir, stat;
		char *cmdtokens[10];
		memset(cmdtokens, 0, sizeof(cmdtokens));

		if (wat_cmd_entry_tokenize(tokens[i], cmdtokens) < 8) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CLCC entry:%s\n", tokens[i]);
			wat_free_tokens(cmdtokens);
		}

		id = atoi(cmdtokens[0]);
		if (id <= 0) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse call ID from CLCC entry:%s\n", tokens[i]);
			WAT_FUNC_DBG_END
			return;
		}

		dir = atoi(cmdtokens[1]);
		if (dir < 0) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse call direction from CLCC entry:%s\n", tokens[i]);
			WAT_FUNC_DBG_END
			return;
		}

		stat = atoi(cmdtokens[2]);
		if (stat < 0) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse call state from CLCC entry:%s\n", tokens[i]);
			WAT_FUNC_DBG_END
			return;
		}

		wat_log_span(span, WAT_LOG_DEBUG, "CLCC entry (id:%d dir:%s stat:%s)\n",
													id,
													wat_call_direction2str(dir),
													wat_clcc_stat2str(stat));

		entries[num_clcc_entries].id = id;
		entries[num_clcc_entries].dir = dir;
		entries[num_clcc_entries].stat = stat;
		num_clcc_entries++;
		
		wat_free_tokens(cmdtokens);
	}

	iter = wat_span_get_call_iterator(span, NULL);
	if (!iter) {
		WAT_FUNC_DBG_END
		return;
	}

	for (curr = iter; curr; curr = wat_iterator_next(curr)) {
		wat_bool_t matched = WAT_FALSE;
		wat_call_t *call = wat_iterator_current(curr);
		
		switch (call->state) {
			case WAT_CALL_STATE_DIALING:
				if (call->dir == WAT_CALL_DIRECTION_INCOMING) {
					for (i = 0; i < num_clcc_entries; i++) {					
						if (entries[i].stat == 4) {
							/* Save the module ID for this call */
							call->modid = entries[i].id;

							wat_log_span(span, WAT_LOG_DEBUG, "[id:%d] module call (modid:%d)\n", call->id, call->modid);
							wat_call_set_state(call, WAT_CALL_STATE_DIALED);
							matched = WAT_TRUE;
						}
					}
				} else {
					for (i = 0; i < num_clcc_entries; i++) {
						switch(entries[i].stat) {
							case 2: /* Dialing */
							case 3: /* Alerting */
								/* Save the module ID for this call */
								call->modid = entries[i].id;

								wat_log_span(span, WAT_LOG_DEBUG, "[id:%d] module call (modid:%d)\n", call->id, call->modid);

								if (entries[i].stat == 2) {
									wat_call_set_state(call, WAT_CALL_STATE_DIALED);
								} else {
									wat_call_set_state(call, WAT_CALL_STATE_RINGING);
								}
								matched = WAT_TRUE;

								/* Keep monitoring the call to find out when the call is anwered */
								wat_sched_timer(span->sched, "progress_monitor", span->config.progress_poll_interval, wat_scheduled_clcc, (void*) call, &call->timeouts[WAT_PROGRESS_MONITOR]);
								break;
							
						}
					}
				}
				break;
			case WAT_CALL_STATE_DIALED:
				if (call->dir == WAT_CALL_DIRECTION_INCOMING) {

				} else {
					for (i = 0; i < num_clcc_entries; i++) {
						switch(entries[i].stat) {
							case 2: /* Dialing */
								matched = WAT_TRUE;
								/* Keep monitoring the call to find out when the call is anwered */
								wat_sched_timer(span->sched, "progress_monitor", span->config.progress_poll_interval, wat_scheduled_clcc, (void*) call, &call->timeouts[WAT_PROGRESS_MONITOR]);
								break;
							case 3: /* Alerting */
								wat_call_set_state(call, WAT_CALL_STATE_RINGING);
							
								matched = WAT_TRUE;
								/* Keep monitoring the call to find out when the call is anwered */
								wat_sched_timer(span->sched, "progress_monitor", span->config.progress_poll_interval, wat_scheduled_clcc, (void*) call, &call->timeouts[WAT_PROGRESS_MONITOR]);
								break;
							case 0:
								matched = WAT_TRUE;
								wat_call_set_state(call, WAT_CALL_STATE_ANSWERED);
								break;
						}
					}
				}
				break;
			case WAT_CALL_STATE_RINGING:
				for (i = 0; i < num_clcc_entries; i++) {
					switch(entries[i].stat) {
						case 3:
							matched = WAT_TRUE;
							/* Keep monitoring the call to find out when the call is anwered */
							wat_sched_timer(span->sched, "progress_monitor", span->config.progress_poll_interval, wat_scheduled_clcc, (void*) call, &call->timeouts[WAT_PROGRESS_MONITOR]);
							break;
						case 0:
							matched = WAT_TRUE;
							wat_call_set_state(call, WAT_CALL_STATE_ANSWERED);
							break;
					}
				}
				break;
			default:
				for (i = 0; i < num_clcc_entries; i++) {
					if (entries[i].id == call->modid) {
						wat_log_span(span, WAT_LOG_DEBUG, "[id:%d] Matched call in CLCC entry (modid:%d)\n", call->id, call->modid);
						matched = WAT_TRUE;
					}
				}
				break;
		}
		
		if (matched == WAT_FALSE) {
			if (g_debug & WAT_DEBUG_CALL_STATE) {
				wat_log_span(span, WAT_LOG_DEBUG, "[id:%d]No CLCC entries for call, hanging up\n", call->id);
			}
			wat_call_set_state(call, WAT_CALL_STATE_TERMINATING);
		}
	}

	wat_iterator_free(iter);

	WAT_FUNC_DBG_END
	return;
}


WAT_NOTIFY_FUNC(wat_notify_cring)
{
	wat_call_t *call = NULL;
	char *token = tokens[0];

	WAT_NOTIFY_FUNC_DBG_START

	if (!strncmp(tokens[0], "+CRING: ", 8)) {
		int len = strlen(&(tokens[0])[8]);
		memmove(tokens[0], &(tokens[0])[8], len);
		memset(&tokens[0][len], 0, strlen(&tokens[0][len]));
	}

	wat_log_span(span, WAT_LOG_DEBUG, "Incoming CRING:%s\n", token);

	/* TODO: We can receive CRING multiple times, check that we did not already create a call for this event */

	/* Assumption: We can only get one incoming call at a time */
	call = wat_span_get_call_by_state(span, WAT_CALL_STATE_DIALING);
	if (call) {
		/* We already allocated this call - do nothing */		
		WAT_FUNC_DBG_END
		return WAT_SUCCESS;
	}

	call = wat_span_get_call_by_state(span, WAT_CALL_STATE_DIALED);
	if (call) {
		/* We already allocated this call - do nothing */
		WAT_FUNC_DBG_END
		return WAT_SUCCESS;
	}

	/* Create new call */
	if (wat_span_call_create(span, &call, 0) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create new call\n");
		WAT_FUNC_DBG_END
		return WAT_SUCCESS;
	}
	
	call->dir	= WAT_CALL_DIRECTION_INCOMING;
	
	call->type = wat_str2wat_call_type(token);
	wat_log_span(span, WAT_LOG_DEBUG, "Call Type:%s(%d)\n", wat_call_type2str(call->type), call->type);

	wat_call_set_state(call, WAT_CALL_STATE_DIALING);
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

WAT_NOTIFY_FUNC(wat_notify_ring)
{
	WAT_NOTIFY_FUNC_DBG_START
	/* TODO: Implement me */
	WAT_FUNC_DBG_END
	return WAT_SUCCESS;
}

/* Calling Line Identification Presentation */
WAT_NOTIFY_FUNC(wat_notify_clip)
{
	char *cmdtokens[10];
	unsigned numtokens;
	wat_call_t *call = NULL;

	WAT_NOTIFY_FUNC_DBG_START

	if (!strncmp(tokens[0], "+CLIP: ", 7)) {
		int len = strlen(&(tokens[0])[7]);
		memmove(tokens[0], &(tokens[0])[7], len);
		memset(&tokens[0][len], 0, strlen(&tokens[0][len]));
	}

	wat_log_span(span, WAT_LOG_DEBUG, "Incoming CLIP:%s\n", tokens[0]);

	/* TODO: We can receive CLIP multiple times, check if this is not the first CLIP */

	/* Assumption: We can only get one incoming call at a time */
	call = wat_span_get_call_by_state(span, WAT_CALL_STATE_DIALED);
	if (call) {
		if (!wat_test_flag(call, WAT_CALL_FLAG_RCV_CLIP)) {
			/* We already processed a CLIP - do nothing */
			/* Too late, we already notified the user */
			wat_log_span(span, WAT_LOG_CRIT, "Received CLIP after CLIP timeout:%d\n", span->config.timeout_cid_num);
		}
		WAT_FUNC_DBG_END
		return WAT_SUCCESS;
	}

	call = wat_span_get_call_by_state(span, WAT_CALL_STATE_DIALING);
	if (!call) {
		wat_log_span(span, WAT_LOG_CRIT, "Received CLIP without CRING\n");
		WAT_FUNC_DBG_END
		return WAT_SUCCESS;
	}

	if (wat_test_flag(call, WAT_CALL_FLAG_RCV_CLIP)) {
		/* We already processed a CLIP - do nothing */
		WAT_FUNC_DBG_END
		return WAT_SUCCESS;
	}

	wat_set_flag(call, WAT_CALL_FLAG_RCV_CLIP);
	
	/* Format: +CLIP: <number>, <type>, "", <alpha>, <CLI_validity>
			<number>: String type phone number of format specified by <type>
			<type>: type of address octet in integer format
				128 - both the type of number and the numbering plan are unknown
				129 - unknown type of number and ISDN/Telephony numbering plan
				145 - international type of number and ISDN/Telephony numbering plan
						(contains the character "+")
			<CLI_validity>:
				0 - CLI validity
				1 - CLI has been witheld by originator
				2 - CLI is not available due to interworking problems or limitation of originating network.
	*/
	
	memset(cmdtokens, 0, sizeof(cmdtokens));	

	numtokens = wat_cmd_entry_tokenize(tokens[0], cmdtokens);

	if (numtokens < 1) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CLIP entry:%s\n", tokens[0]);
		wat_free_tokens(cmdtokens);
	}

	if (strlen(cmdtokens[0]) < 0) {
		wat_log_span(span, WAT_LOG_DEBUG, "Calling Number not available\n");
		goto done;
	}

	strncpy(call->calling_num.digits, cmdtokens[0], strlen(cmdtokens[0]));

	if (numtokens >= 1) {
		switch (atoi(cmdtokens[1])) {
			case 128:
				call->calling_num.type = WAT_NUMBER_TYPE_UNKNOWN;
				call->calling_num.plan = WAT_NUMBER_PLAN_UNKNOWN;
				break;
			case 129:
				call->calling_num.type = WAT_NUMBER_TYPE_UNKNOWN;
				call->calling_num.plan = WAT_NUMBER_PLAN_ISDN;
				break;
			case 145:
				call->calling_num.type = WAT_NUMBER_TYPE_INTERNATIONAL;
				call->calling_num.plan = WAT_NUMBER_PLAN_ISDN;
				break;
			case 0:
				/* Calling Number is not available */
				call->calling_num.type = WAT_NUMBER_TYPE_INVALID;
				call->calling_num.plan = WAT_NUMBER_PLAN_INVALID;
				break;
			default:
				wat_log_span(span, WAT_LOG_ERROR, "Invalid number type from CLIP:%s\n", tokens[0]);
				call->calling_num.type = WAT_NUMBER_TYPE_INVALID;
				call->calling_num.plan = WAT_NUMBER_PLAN_INVALID;
				break;
		}
	}

	if (numtokens >= 6) {
		switch (atoi(cmdtokens[5])) {
			case 0:
				call->calling_num.validity = WAT_NUMBER_VALIDITY_VALID;
				break;
			case 1:
				call->calling_num.validity = WAT_NUMBER_VALIDITY_WITHELD;
				break;
			case 2:
				call->calling_num.validity = WAT_NUMBER_VALIDITY_UNAVAILABLE;
				break;
			default:
				wat_log_span(span, WAT_LOG_ERROR, "Invalid number validity from CLIP:%s\n", tokens[0]);
				call->calling_num.validity = WAT_NUMBER_VALIDITY_INVALID;
				break;
		}
	}

	
	wat_log_span(span, WAT_LOG_DEBUG, "Calling Number:%s type:%s(%d) plan:%s(%d) validity:%s(%d)\n",
										call->calling_num.digits,
										wat_number_type2str(call->calling_num.type), call->calling_num.type,
										wat_number_plan2str(call->calling_num.plan), call->calling_num.plan,
										wat_number_validity2str(call->calling_num.validity), call->calling_num.validity);

done:	
	wat_free_tokens(cmdtokens);
	return WAT_SUCCESS;
}

WAT_NOTIFY_FUNC(wat_notify_creg)
{
	int stat;
	unsigned count;
	char *cmdtokens[3];
	wat_status_t status = WAT_FAIL;
	
	WAT_NOTIFY_FUNC_DBG_START

	if (!strncmp(tokens[0], "+CREG: ", 7)) {
		int len = strlen(&(tokens[0])[7]);
		memmove(tokens[0], &(tokens[0])[7], len);
		memset(&tokens[0][len], 0, strlen(&tokens[0][len]));
	}

	memset(cmdtokens, 0, sizeof(cmdtokens));
	count = wat_cmd_entry_tokenize(tokens[0], cmdtokens);

	if (count < 0) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CREG Response %s\n", tokens[0]);
		status = WAT_SUCCESS;
	} else if (count == 1) {
		stat = atoi(cmdtokens[0]);
		if (stat < 0) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CREG Response %s\n", tokens[0]);
			status = WAT_SUCCESS;
		} else {
			wat_span_update_net_status(span, stat);
		}
	} else {
		/* if count > 1, this is an unsollicited notification, but an response
			(and the terminator has not been received yet) return WAT_BREAK,
			so that we wait for a complete response */
		status = WAT_BREAK;
	}

	wat_free_tokens(cmdtokens);
	return status;
}

WAT_SCHEDULED_FUNC(wat_scheduled_clcc)
{
	wat_call_t *call = (wat_call_t *)data;
	wat_cmd_enqueue(call->span, "AT+CLCC", wat_response_clcc, call);
}

char *wat_decode_csq_rssi(char *in, unsigned rssi)
{
	switch (rssi) {
		case 0:
			return "(-113)dBm or less";
		case 1:
			return "(-111)dBm";
		case 31:
			return "(-51)dBm";
		case 99:
			return "not detectable";
		default:
			if (rssi >= 2 && rssi <= 30) {
				sprintf(in, "(-%d)dBm", 113-(2*rssi));
				return in;
			}
	}
	return "invalid";
}

char* format_at_data(char *dest, void *indata, wat_size_t len)
{
	int i;
	uint8_t *data = indata;
	char *p = dest;

	for (i = 0; i < len; i++) {
		switch(data[i]) {
			case '\r':
				sprintf(p, "\\r");
				p+=2;
				break;
			case '\n':
				sprintf(p, "\\n");
				p+=2;
				break;
			default:
				*p = data[i];
				p++;
		}
	}
	*p = '\0';
	return dest;
}

