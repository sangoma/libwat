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

typedef struct {
	unsigned id;
	unsigned dir;
	unsigned stat;
} clcc_entry_t;

typedef enum {
	WAT_TERM_OK,
	WAT_TERM_CONNECT,
	WAT_TERM_BUSY,
	WAT_TERM_ERR,
	WAT_TERM_NO_DIALTONE,
	WAT_TERM_NO_ANSWER,
	WAT_TERM_NO_CARRIER,
	WAT_TERM_CMS_ERR,
	WAT_TERM_CME_ERR,
	WAT_TERM_EXT_ERR,
	WAT_TERM_SMS,
} wat_term_t;

struct terminator {
	char *terminator;
	wat_bool_t success;
	wat_term_t term_type;
};

static struct terminator terminators[] = {
	{ "OK", WAT_TRUE, WAT_TERM_OK },
	{ "CONNECT", WAT_TRUE, WAT_TERM_CONNECT },
	{ "BUSY", WAT_FALSE, WAT_TERM_BUSY },
	{ "ERROR", WAT_FALSE, WAT_TERM_ERR },
	{ "NO DIALTONE", WAT_FALSE, WAT_TERM_NO_DIALTONE },
	{ "NO ANSWER", WAT_FALSE, WAT_TERM_NO_ANSWER },
	{ "NO CARRIER", WAT_FALSE, WAT_TERM_NO_CARRIER },
	{ "+CMS ERROR:", WAT_FALSE, WAT_TERM_CMS_ERR },
	{ "+CME ERROR:", WAT_FALSE, WAT_TERM_CME_ERR },
	{ "+EXT ERROR:", WAT_FALSE, WAT_TERM_EXT_ERR },
	{ ">", WAT_TRUE, WAT_TERM_SMS }
};

struct error_code {
	uint32_t code;
	char *string;
};

/* GSM Equipment related codes */
static struct error_code cme_codes[] = {
	{ 0, "Phone failure" },
	{ 1, "No connection to phone" },
	{ 2, "phone-adaptor link reserved" },
	{ 3, "operation not allowed" },
	{ 4, "operation not supported" },
	{ 5, "PH-SIM PIN required" },
	{ 10, "SIM not inserted" },
	{ 11, "SIM PIN required" },
	{ 12, "SIM PUK required" },
	{ 13, "SIM failure" },
	{ 14, "SIM busy" },
	{ 15, "SIM wrong" },
	{ 16, "incorrect password" },
	{ 17, "SIM PIN2 required" },
	{ 18, "SIM PUK2 required" },
	{ 20, "memory full" },
	{ 21, "invalid index" },
	{ 22, "not found" },
	{ 23, "memory failure" },
	{ 24, "text string too long" },
	{ 25, "invalid characters in text string" },
	{ 26, "dial string too long" },
	{ 27, "invalid characters in dial string" },
	{ 30, "no network service" },
	{ 31, "network time-out" },
	{ 32, "network not allowed - emergency calls only" },
	{ 40, "network personalization PIN required" },
	{ 41, "network personalization PUK required" },
	{ 42, "network subset personalization PIN required" },
	{ 43, "network subset personalization PUK required" },
	{ 44, "service provider personalization PIN required" },
	{ 45, "service provider personalization PUK required" },
	{ 46, "corporate personalization PIN required" },
	{ 47, "corporate personalization PUK required" },
	{ 100, "unknown" }, /* General purpose error */
	{ 103, "Illegal MS" },
	{ 106, "Illegal ME" },
	{ 107, "GPRS service not allowed" },
	{ 111, "PLMN not allowed" },
	{ 112, "Location area not allowed" },
	{ 113, "Roaming not allowed in this location area" },
	{ 132, "service option not supported" },
	{ 133, "requested service option not subscribed" },
	{ 134, "service option temporarily out of order" },
	{ 148, "unspecified GPRS error" },
	{ 149, "PDP authentication failure" },
	{ 150, "invalid mobile class" },
	{ 257, "Call barred" },
	{ 258, "Phone is busy" },
	{ 259, "User abort" },
	{ 260, "Invalid dial string" },
	{ 262, "SIM blocked"},
	{ 263, "Invalid block"},
	{ 300, "ME failure" },
	{ 301, "SMS service of ME reserved" },
	{ 302, "Operation not allowed" },
	{ 303, "Operation not supported" },
	{ 304, "invalid PDU mode parameter" },
	{ 305, "invalid text mode parameter" },
	{ 310, "SIM not inserted" },
	{ 311, "SIM PIN required" },
	{ 312, "PH-SIM PIN required" },
	{ 313, "SIM failure" },
	{ 314, "SIM busy" },
	{ 315, "SIM wrong" },
	{ 316, "SIM PUK required" },
	{ 317, "SIM PIN2 required" },
	{ 318, "SIM PUK2 required" },
	{ 320, "memory failure" },
	{ 321, "invalid memory index" },
	{ 322, "memory full" },
	{ 330, "SMSC address unknown" },
	{ 331, "no network service" },
	{ 332, "network time-out"},
	{ 400, "generic undocummented error" },
	{ 401, "wrong state" },
	{ 402, "wrong mode" },
	{ 403, "context already activated" },
	{ 404, "stack already active" },
	{ 405, "activation failed" },
	{ 406, "context not opened" },
	{ 407, "cannot setup socket" },
	{ 408, "cannot resolve DN" },
	{ 409, "time-out in opening socket" },
	{ 410, "cannot open socket" },
	{ 411, "remote disconnected or time-out" },
	{ 412, "connection failed" },
	{ 413, "tx error" },
	{ 414, "already listening" },
	{ 500, "unknown error"},
	{ 772, "SIM powered down"},
	{ -1, "invalid"},
};

/* GSM Network related codes */
/* From wwww.smssolutions.net/tutorials/gsm/gsmerrorcodes */
static struct error_code cms_codes[] = {
	{ 1, "Unassigned number" },
	{ 8, "Operator determined barring" },
	{ 10, "Call bared" },
	{ 21, "Shor message transfer rejected" },
	{ 27, "Destination out of service" },
	{ 28, "Unenditified subscriber" },
	{ 29, "Facility rejected" },
	{ 30, "Unknown subscriber" },
	{ 38, "Network out of order" },
	{ 41, "Temporary failure" },
	{ 42, "Congestion" },
	{ 47, "Resources unavailable" },
	{ 50, "Requested facility not subscribed" },
	{ 69, "Requested facility not implemented" },
	{ 81, "Invalid short message transfer reference value" },
	{ 95, "Invalid message unspecified" },
	{ 96, "Invalid mandatory information" },
	{ 97, "Message type non existent or not implemented" },
	{ 98, "Message not compatible with short message protocol" },
	{ 99, "Information element non-existent or not implemented" },
	{ 111, "Protocol error, unspecified"},
	{ 127, "Internetworking, unspecified"},
	{ 128, "Telematic internetworking not supported"},
	{ 129, "Short message type 0 not supported"}, 
	{ 130, "Cannot replace short message"},
	{ 143, "Unspecified TP-PID error"},
	{ 144, "Data code scheme not supported"},
	{ 145, "Message class not supported"},
	{ 159, "Unspecified TP-DCS error"},
	{ 160, "Command cannot be actioned"},
	{ 161, "Command unsupported"},
	{ 175, "Unspecified TP-Command error"},
	{ 176, "TPDU not supported"},
	{ 192, "SC busy"},
	{ 193, "No SC subscription"},
	{ 194, "SC System failure"},
	{ 195, "Invalid SME address"},
	{ 196, "Destination SME barred"},
	{ 197, "SM Rejected-Duplicate SM"},
	{ 198, "TP-VPF not supported"},
	{ 199, "TP-VP not supported"},
	{ 208, "D0 SIM SMS Storage full"},
	{ 209, "No SMS Storage capability in SIM"},
	{ 210, "Error in MS"},
	{ 211, "Memory capacity exceeded"},
	{ 212, "SIM application toolkit busy"},
	{ 213, "SIM data download error"},
	{ 255, "Unspecified error cause"},
	{ 300, "ME Failure"},
	{ 301, "SMS service of ME reserved"},
	{ 302, "Operation not allowed"},
	{ 303, "Operation not supported"},
	{ 304, "Invalid PDU mode parameter"},
	{ 305, "Invalid Text mode parameter"},
	{ 310, "SIM not inserted"},
	{ 311, "SIM PIN required"},
	{ 312, "PH-SIM PIN required"},
	{ 313, "SIM failure"},
	{ 314, "SIM busy"},
	{ 315, "SIM wrong"},
	{ 316, "SIM PUK required"},
	{ 317, "SIM PIN2 required"},
	{ 318, "SIM PUK2 required"},
	{ 320, "Memory failure"},
	{ 321, "Invalid memory index"},
	{ 322, "Memory full"},
	{ 330, "SMSC address unknown"},
	{ 331, "No network service"},
	{ 332, "Network timeout"},
	{ 340, "No +CNMA expected"},
	{ 500, "Unknown error"},
	{ 512, "User abort"},
	{ 513, "Unable to store"},
	{ 514, "Invalid status"},
	{ 515, "Device busy or Invalid Character in string"},
	{ 516, "Invalid length"},
	{ 517, "Invalid character in PDU"},
	{ 518, "Invalid parameter"},
	{ 519, "Invalid length or character"},
	{ 520, "Invalid character in text"},
	{ 521, "Timer expired"},
	{ 522, "Operation temporary not allowed"},
	{ 532, "SIM not ready"},
	{ 534, "Cell Broadcast error unknown"},
	{ 535, "Protocol stack busy"},
	{ 538, "Invalid parameter"},
	{ -1, "invalid"},
};

/* TODO: Fill in ext error codes */
static struct error_code ext_codes[] = {
	{ -1, "invalid" },
};

static wat_status_t wat_tokenize_line(char *tokens[], char *line, wat_size_t len, wat_size_t *consumed);
static int wat_cmd_handle_notify(wat_span_t *span, char *tokens[]);
static int wat_cmd_handle_response(wat_span_t *span, char *tokens[], wat_bool_t success, char *error);
static wat_status_t wat_match_terminator(const char* token, wat_bool_t *success, char **error);
static void wat_remove_prefix(char *string, const char *prefix);

static void wat_remove_prefix(char *string, const char *prefix)
{
	int prefix_len = strlen(prefix);
	if (!strncmp(string, prefix, prefix_len)) {
		int len = strlen(&string[prefix_len]);
		memmove(string, &string[prefix_len], len);
		memset(&string[len], 0, strlen(&string[len]));
	}
}

static char *wat_strerror(int error, struct error_code error_table[])
{
	int i = 0;

	while (error_table[i].code != -1) {
		if (error_table[i].code == error) {
			return error_table[i].string;
		}
		i++;
	}
	return "invalid";
}

wat_status_t wat_cmd_enqueue(wat_span_t *span, const char *incommand, wat_cmd_response_func *cb, void *obj)
{
	wat_cmd_t *cmd;
	wat_assert_return(span->cmd_queue, WAT_FAIL, "No command queue!\n");

	if (!strlen(incommand)) {
		wat_log_span(span, WAT_LOG_DEBUG, "Invalid cmd to enqueue \"%s\"\n", incommand);
		return WAT_FAIL;
	}

	if (g_debug & WAT_DEBUG_AT_HANDLE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Enqueued command \"%s\"\n\n", incommand);
	}

	/* Add a \r to finish the command */
	cmd = wat_calloc(1, sizeof(*cmd));
	wat_assert_return(cmd, WAT_FAIL, "Failed to alloc new command\n");
	
	cmd->cb = cb;
	cmd->obj = obj;
	cmd->cmd = wat_strdup(incommand);
	wat_queue_enqueue(span->cmd_queue, cmd);
	return WAT_SUCCESS;
}

static wat_status_t wat_match_terminator(const char* token, wat_bool_t *success, char **error)
{
	int i;

	for (i = 0; i < sizeof(terminators)/sizeof(terminators[0]); i++) {
		if (!strncmp(terminators[i].terminator, token, strlen(terminators[i].terminator))) {
			*success = terminators[i].success;
			switch(terminators[i].term_type) {
				case WAT_TERM_CMS_ERR:					
					*error = wat_strerror(atoi(&token[strlen(terminators[i].terminator) + 1]), cms_codes);
					break;
				case WAT_TERM_CME_ERR:
					*error = wat_strerror(atoi(&token[strlen(terminators[i].terminator) + 1]), cme_codes);
					break;
				case WAT_TERM_EXT_ERR:
					*error = wat_strerror(atoi(&token[strlen(terminators[i].terminator) + 1]), ext_codes);
					break;
				default:
					break;
			}
			return WAT_SUCCESS;
		}
	}
	return WAT_FAIL;
}

#if 0
typedef enum {
	WAT_CMD_PARSE_STATE_INIT,
	WAT_CMD_PARSE_STATE_HAS_TOKEN, /* We have a token that is not a terminator */
	
} wat_cmd_parse_state_t;
#endif

wat_status_t wat_cmd_process(wat_span_t *span)
{
	char data[WAT_BUFFER_SZ];	
	unsigned i = 0;
	wat_size_t len = 0;	

	if (wat_buffer_peep(span->buffer, data, &len) == WAT_SUCCESS) {
		wat_size_t consumed;
		char *tokens[WAT_TOKENS_SZ];
		int tokens_consumed = 0;
		int tokens_unused = 0;
		wat_bool_t success = WAT_FALSE;
		wat_status_t status =- WAT_FAIL;

		memset(tokens, 0, sizeof(tokens));

		if (g_debug & WAT_DEBUG_UART_DUMP) {
			char mydata[WAT_MAX_CMD_SZ];
			wat_log_span(span, WAT_LOG_DEBUG, "[RX AT] %s (len:%d)\n", format_at_data(mydata, data, len), len);
		}

		status = wat_tokenize_line(tokens, (char*)data, len, &consumed);
		if (status == WAT_SUCCESS) {
			for (i = 0; tokens[i]; i++) {
				char *error = NULL;

				status = wat_match_terminator(tokens[i], &success, &error);
				if (status == WAT_SUCCESS) {
					if (!tokens_unused && !success && !span->cmd_busy) {
						/* This could be a hangup from the remote side, schedule a CLCC to find out which call hung-up */
						wat_cmd_enqueue(span, "AT+CLCC", wat_response_clcc, NULL);
						tokens_consumed++;
					}
					
					tokens_consumed += wat_cmd_handle_response(span, &tokens[i-tokens_unused], success, error);
					tokens_unused = 0;
				} else {
					if (!tokens[i+1]) {
						tokens_consumed += wat_cmd_handle_notify(span, &tokens[i-tokens_unused]);
					} else {
						tokens_unused++;
					}
				}
			}
			wat_free_tokens(tokens);
		}
		if (tokens_consumed) {
			/* If we handled this token, remove it from the buffer */

			wat_buffer_flush(span->buffer, consumed);
		}
	}

	return WAT_SUCCESS;
}

static int wat_cmd_handle_response(wat_span_t *span, char *tokens[], wat_bool_t success, char *error)
{
	int tokens_consumed = 0;
	wat_cmd_t *cmd;
	
	wat_assert_return(span->cmd, WAT_FAIL, "We did not have a command pending\n");
	
	cmd = span->cmd;
	if (g_debug & WAT_DEBUG_AT_HANDLE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Handling response for cmd:%s\n", cmd->cmd);
	}
	
	if (cmd->cb) {
		tokens_consumed = cmd->cb(span, tokens, success, cmd->obj, error);
	} else {
		tokens_consumed = 1;
	}

	span->cmd = NULL;

	wat_safe_free(cmd->cmd);
	wat_safe_free(cmd);
	span->cmd_busy = 0;
	wat_log_span(span, WAT_LOG_DEBUG, "Response consumed %d tokens\n", tokens_consumed);
	return tokens_consumed;
}

static int wat_cmd_handle_notify(wat_span_t *span, char *tokens[])
{	
	int i;
	int tokens_consumed = 0;

	/* For notifications, the first token contains the AT command prefix */
	if (g_debug & WAT_DEBUG_AT_HANDLE) {
		wat_log_span(span, WAT_LOG_DEBUG, "Handling notify for cmd:%s\n", tokens[0]);
	}

	for (i = 0; i < sizeof(span->notifys)/sizeof(span->notifys[0]); i++) {
		if (span->notifys[i]) {
			wat_notify_t *notify = span->notifys[i];
			if (!strncasecmp(notify->prefix, tokens[0], strlen(notify->prefix))) {
				tokens_consumed = notify->func(span, tokens);
				goto done;
			}
		}
	}

	/* This is not an error, sometimes sometimes we have an incomplete response
	(terminator not received yet), and we think its a notify  */
	wat_log_span(span, WAT_LOG_DEBUG, "No handler for unsollicited notify \"%s\"\n", tokens[0]);
done:
	wat_log_span(span, WAT_LOG_DEBUG, "Notify consumed %d tokens\n", tokens_consumed);
	return tokens_consumed;
}

static wat_status_t wat_tokenize_line(char *tokens[], char *line, wat_size_t len, wat_size_t *consumed)
{
	int i;
	int token_index = 0;
	uint8_t has_token = 0;
	unsigned token_start_index = 0;
	unsigned consumed_index = 0;

	char *token_str = NULL;
	char *p = NULL;

	for (i = 0; i < len; i++) {
		switch(line[i]) {
			case '\n':
				if (has_token) {
					/* This is the end of a token */
					has_token = 0;

					tokens[token_index++] = token_str;
				}
				break;
			case '\r':
				/* Ignore \r */
				break;
			case '>':
				{
					/* We are in SMS mode */

					/* Save previous token */
					if (has_token) {
						/* This is the end of a token */
						has_token = 0;

						tokens[token_index++] = token_str;
					}
					/* Create a new token */
					tokens[token_index++] = wat_strdup(">\0");
				}
				break;
			default:
				if (!has_token) {
					/* This is the start of a new token */
					has_token = 1;
					token_start_index = i;

					token_str = wat_calloc(1, WAT_MAX_CMD_SZ);
					wat_assert_return(token_str, WAT_FAIL, "Failed to allocate new token\n");

					p = token_str;
				}
				*(p++) = line[i];
				
		}
	}

	/* No more tokens left in buffer */
	if (token_index) {
		while (i <  len) {
			/* Remove remaining \r and \n" */
			if (line[i] != '\r' && line[i] != '\n') {
				break;
			}
			i++;
		}
		consumed_index = i;

		*consumed = consumed_index;

		if (g_debug & WAT_DEBUG_AT_PARSE) {
			wat_log(WAT_LOG_DEBUG, "Decoded tokens %d consumed:%u len:%u\n", token_index, *consumed, len);

			for (i = 0; i < token_index; i++) {
				wat_log(WAT_LOG_DEBUG, "  Token[%d]:%s\n", i, tokens[i]);
			}
		}
		return WAT_SUCCESS;
	}

	if (has_token) {
		/* We only got half a token, need to free this pointer */
		wat_safe_free(token_str);
	}

	return WAT_FAIL;
}

void wat_free_tokens(char *tokens[])
{
	unsigned i;
	for (i = 0; tokens[i]; i++) {
		wat_safe_free(tokens[i]);
	}
}

wat_status_t wat_cmd_register(wat_span_t *span, const char *prefix, wat_cmd_notify_func func)
{
	int i;
	wat_status_t status = WAT_FAIL;
	wat_notify_t *new_notify = NULL;
	wat_iterator_t *iter = NULL;
	wat_iterator_t *curr = NULL;

	/* Check if there is already a notify callback set for this prefix first */
	iter = wat_span_get_notify_iterator(span, iter);
	for (curr = iter; curr; curr = wat_iterator_next(curr)) {
		wat_notify_t *notify = wat_iterator_current(curr);
		if (!strcmp(notify->prefix, prefix)) {
			/* Overwrite existing notify */
			wat_log_span(span, WAT_LOG_INFO, "Already had a notifier for prefix %s\n", prefix);

			notify->func = func;
			status = WAT_SUCCESS;
		}
	}

	/* TODO: Create a function: wat_span_get_free_notify that returns a pointer to a
	free location */
	for (i = 1; i < sizeof(span->notifys)/sizeof(span->notifys[0]); i++) {
		if (!span->notifys[i]) {
			new_notify = wat_calloc(1, sizeof(*new_notify));
			wat_assert_return(new_notify, WAT_FAIL, "Failed to alloc memory\n");

			new_notify->prefix = wat_strdup(prefix);
			new_notify->func = func;
			
			span->notifys[i] = new_notify;
			span->notify_count++;
			status = WAT_SUCCESS;
			goto done;
		}
	}

	wat_log(WAT_LOG_CRIT, "Failed to register new notifier, no space left in notify list\n");

done:
			wat_iterator_free(iter);
	return status;
}

static int wat_cmd_entry_tokenize(char *entry, char *tokens[])
{
	int token_count = 0;
	char *p = NULL;

	if (entry[0] == ',') {
		/* If the first character is a ',' , this string begins with an empty token,
		 we still need to count it */
		tokens[token_count++] = wat_strdup("");
	}
	p = strtok(entry, ",");
	while (p != NULL) {
		tokens[token_count++] = wat_strdup(p);
		p = strtok(NULL, ",");
	}
	return token_count;
}

WAT_RESPONSE_FUNC(wat_response_atz)
{
	int tokens_consumed = 0;
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to reset module\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	if (!strncmp(tokens[0], "ATZ", 3)) {
		/* The chip had echo mode turned on, so the command was echo'ed back */
		++tokens_consumed;
	}
	
	WAT_FUNC_DBG_END
	return ++tokens_consumed;
}

WAT_RESPONSE_FUNC(wat_response_ate)
{
	int tokens_consumed = 0;
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to disable echo mode\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	if (!strncmp(tokens[0], "ATE", 3)) {
		/* The chip had echo mode turned on, so the command was echo'ed back */
		++tokens_consumed;
	}

	WAT_FUNC_DBG_END
	return ++tokens_consumed;
}

/* Get Module Manufacturer Name */
WAT_RESPONSE_FUNC(wat_response_cgmm)
{
	WAT_RESPONSE_FUNC_DBG_START	
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module manufacturer name\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	strncpy(span->manufacturer_name, tokens[0], sizeof(span->manufacturer_name));
	WAT_FUNC_DBG_END
	return 2;
}

/* Get Module Manufacturer Identification */
WAT_RESPONSE_FUNC(wat_response_cgmi)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module manufacturer id\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	strncpy(span->manufacturer_id, tokens[0], sizeof(span->manufacturer_id));
	WAT_FUNC_DBG_END
	return 2;
}

/* Get Module Revision Identification */
WAT_RESPONSE_FUNC(wat_response_cgmr)
{
	unsigned start = 0;
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module revision identification\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	if (!strncmp(tokens[0], "Revision:", 9)) {
		start = 6;
	}

	strncpy(span->revision_id, &((tokens[0])[start]), sizeof(span->revision_id));
	WAT_FUNC_DBG_END
	return 2;
}

/* Get Module Serial Number */
WAT_RESPONSE_FUNC(wat_response_cgsn)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module serial number\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	strncpy(span->serial_number, tokens[0], sizeof(span->serial_number));
	WAT_FUNC_DBG_END
	return 2;
}

/* Get Module IMSI */
WAT_RESPONSE_FUNC(wat_response_cimi)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain module International Subscriber Identify\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	strncpy(span->imsi, tokens[0], sizeof(span->imsi));
	WAT_FUNC_DBG_END
	return 2;
}

/* Enable Calling Line Presentation  */
WAT_RESPONSE_FUNC(wat_response_clip)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		span->clip = WAT_FALSE;
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable Calling Line Presentation\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	span->clip = WAT_TRUE;
	WAT_FUNC_DBG_END
	return 1;
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
		return 1;
	}

	wat_remove_prefix(tokens[0], "+CREG: ");
	
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
	return 2;
}

/* New Message Indications To Terminal Equipment */
WAT_RESPONSE_FUNC(wat_response_cnmi)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable New Messages Indications to TE\n");
		WAT_FUNC_DBG_END
		return 1;
	}
	WAT_FUNC_DBG_END
	return 1;
}

/* Set Operator Selection */
WAT_RESPONSE_FUNC(wat_response_cops)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to enable Operator Selection\n");
		WAT_FUNC_DBG_END
		return 1;
	}
	WAT_FUNC_DBG_END
	return 1;
}

WAT_RESPONSE_FUNC(wat_response_cnum)
{
	WAT_RESPONSE_FUNC_DBG_START
	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain own number\n");
		WAT_FUNC_DBG_END
		return 1;
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
		return 1;
	}

	wat_remove_prefix(tokens[0], "+CNUM: ");

	/* TODO: Do a complete parsing of the parameters */

	strncpy(span->subscriber_number, tokens[0], sizeof(span->subscriber_number));
	WAT_FUNC_DBG_END
	return 2;
}

WAT_RESPONSE_FUNC(wat_response_csq)
{
	unsigned rssi, ber;
	WAT_RESPONSE_FUNC_DBG_START

	if (success != WAT_TRUE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to obtain Signal Strength\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	wat_remove_prefix(tokens[0], "+CSQ: ");
	
	if (sscanf(tokens[0], "%d,%d\n", &rssi, &ber) == 2) {
		char dest[30];
		span->net_info.rssi = rssi;
		span->net_info.ber = ber;
		wat_log_span(span, WAT_LOG_NOTICE, "Signal strength:%s (BER:%s)\n", wat_decode_csq_rssi(dest, rssi), wat_csq_ber2str(ber));
	} else {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CSQ %s\n", tokens[0]);
	}
	WAT_FUNC_DBG_END
	return 2;
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
	return 1;
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
	return 1;
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
	return 1;
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

	wat_remove_prefix(tokens[0], "+CLCC: ");

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
			return 1;
		}

		dir = atoi(cmdtokens[1]);
		if (dir < 0) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse call direction from CLCC entry:%s\n", tokens[i]);
			WAT_FUNC_DBG_END
			return 1;
		}

		stat = atoi(cmdtokens[2]);
		if (stat < 0) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse call state from CLCC entry:%s\n", tokens[i]);
			WAT_FUNC_DBG_END
			return 1;
		}

		wat_log_span(span, WAT_LOG_DEBUG, "CLCC entry (id:%d dir:%s stat:%s)\n",
													id,
													wat_direction2str(dir),
													wat_clcc_stat2str(stat));

		entries[num_clcc_entries].id = id;
		entries[num_clcc_entries].dir = dir;
		entries[num_clcc_entries].stat = stat;
		num_clcc_entries++;
		
		wat_free_tokens(cmdtokens);
	}

	iter = wat_span_get_call_iterator(span, NULL);
	if (!iter) {
		/* No calls active */
		if (num_clcc_entries) {
			wat_log_span(span, WAT_LOG_CRIT, "We have %d CLCC entries, but no active calls!!\n", num_clcc_entries);
		}
		WAT_FUNC_DBG_END
		return 1;
	}

	for (curr = iter; curr; curr = wat_iterator_next(curr)) {
		wat_bool_t matched = WAT_FALSE;
		wat_call_t *call = wat_iterator_current(curr);
		
		switch (call->state) {
			case WAT_CALL_STATE_DIALING:
				if (call->dir == WAT_DIRECTION_INCOMING) {
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
				if (call->dir == WAT_DIRECTION_INCOMING) {

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
	return 2;
}

WAT_RESPONSE_FUNC(wat_response_cmgf)
{
	wat_sms_t *sms;
	WAT_RESPONSE_FUNC_DBG_START

	sms = (wat_sms_t *)obj;

	if (success == WAT_FALSE) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to switch SMS mode\n");
		if (sms) {
			sms->cause = WAT_SMS_CAUSE_MODE_NOT_SUPPORTED;
			wat_sms_set_state(sms, WAT_SMS_STATE_COMPLETE);
		}
		WAT_FUNC_DBG_END
		return 1;
	}
	if (sms) {
		span->sms_mode = sms->type;
		wat_sms_set_state(sms, WAT_SMS_STATE_SEND_HEADER);
	}
	WAT_FUNC_DBG_END
	return 1;
}


WAT_RESPONSE_FUNC(wat_response_cmgs_start)
{
	wat_sms_t *sms;
	wat_sms_status_t sms_status;

	WAT_RESPONSE_FUNC_DBG_START
	sms = (wat_sms_t *)obj;

	if (!sms) {
		wat_log_span(span, WAT_LOG_CRIT, "Sent a SMS, but we lost pointer\n");
		WAT_FUNC_DBG_END
		return 1;
	}
	
	memset(&sms_status, 0, sizeof(sms_status));
	
	if (success == WAT_TRUE) {
		wat_sms_set_state(sms, WAT_SMS_STATE_SEND_BODY);
	} else {
		sms->cause = WAT_SMS_CAUSE_NO_RESPONSE;
		sms->error = error;
		wat_sms_set_state(sms, WAT_SMS_STATE_COMPLETE);
	}

	WAT_FUNC_DBG_END
	return 1;
}

WAT_RESPONSE_FUNC(wat_response_cmgs_end)
{
	wat_sms_t *sms;
	wat_sms_status_t sms_status;

	WAT_RESPONSE_FUNC_DBG_START
	sms = (wat_sms_t *)obj;

	if (!sms) {
		wat_log_span(span, WAT_LOG_CRIT, "Sent a SMS, but we lost pointer\n");
		WAT_FUNC_DBG_END
		return 1;
	}
	
	memset(&sms_status, 0, sizeof(sms_status));
	
	if (success != WAT_TRUE) {
		sms->cause = WAT_SMS_CAUSE_NETWORK_REJECT;
		sms->error = error;
	}
	span->outbound_sms = NULL;

	wat_sms_set_state(sms, WAT_SMS_STATE_COMPLETE);

	WAT_FUNC_DBG_END
	return 1;
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
		return 1;
	}

	/* Create new call */
	if (wat_span_call_create(span, &call, 0, WAT_DIRECTION_INCOMING) != WAT_SUCCESS) {
		wat_log_span(span, WAT_LOG_CRIT, "Failed to create new call\n");
		WAT_FUNC_DBG_END
		return 1;
	}
		
	call->type = wat_str2wat_call_type(token);
	wat_log_span(span, WAT_LOG_DEBUG, "Call Type:%s(%d)\n", wat_call_type2str(call->type), call->type);

	wat_call_set_state(call, WAT_CALL_STATE_DIALING);
	WAT_FUNC_DBG_END
	return 1;
}

/* Incoming SMS */
WAT_NOTIFY_FUNC(wat_notify_cmt)
{
	wat_size_t len;
	char *cmdtokens[4];
	unsigned numtokens;

	WAT_NOTIFY_FUNC_DBG_START
	/* Format +CMT <alpha>, <length> */
	/* token [1] has PDU data */

	wat_remove_prefix(tokens[0], "+CMT: ");

	memset(cmdtokens, 0, sizeof(cmdtokens));

	numtokens = wat_cmd_entry_tokenize(tokens[0], cmdtokens);

	if (numtokens < 2) {
		wat_log_span(span, WAT_LOG_WARNING, "Failed to parse incoming SMS Header %s (%d)\n", tokens[0], numtokens);
		goto done;
	}

	len = atoi(cmdtokens[1]);
	if (len <= 0) {
		wat_log_span(span, WAT_LOG_WARNING, "Invalid PDU len in SMS header %s\n", tokens[0]);
		goto done;
	}

	wat_log_span(span, WAT_LOG_DEBUG, "[sms] len:%d\n", len);

	/* PDU Mode:
		+CMT:<alpha>,<length>,\r\n<pdu>
		alpha:representation from phonebook
		length: pdu length
		pdu: pdu message

		Text Mode:
		+CMT:<oa>,<alpha><scts>[,<tooa>,<fo>,<pid>,<dcs>,<sca>,<tosca>,<length>]\r\n<data>
		oa: Originating Address
		alpha:representation from phonebook
		scts:arrival time of the message to the SC
		tooa,tosca: type of number
		fo: first octet
		pid: Protocol identifier
		dcs: Data Coding Scheme
		sca: Service Centre Address
		length: text length
	*/

	if (numtokens > 2) {
		/* Text mode */
		wat_log_span(span, WAT_LOG_ERROR, "Reception of Text mode SMS not implemented!!!\n", tokens[0]);
	} else {
		wat_handle_incoming_sms_pdu(span, tokens[1], len);
	}
	
done:
	wat_free_tokens(cmdtokens);
	WAT_FUNC_DBG_END
	return 2;
}

/* Calling Line Identification Presentation */
WAT_NOTIFY_FUNC(wat_notify_clip)
{
	char *cmdtokens[10];
	unsigned numtokens;
	wat_call_t *call = NULL;

	WAT_NOTIFY_FUNC_DBG_START

	wat_remove_prefix(tokens[0], "+CLIP: ");

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
		return 1;
	}

	call = wat_span_get_call_by_state(span, WAT_CALL_STATE_DIALING);
	if (!call) {
		wat_log_span(span, WAT_LOG_CRIT, "Received CLIP without CRING\n");
		WAT_FUNC_DBG_END
		return 1;
	}

	if (wat_test_flag(call, WAT_CALL_FLAG_RCV_CLIP)) {
		/* We already processed a CLIP - do nothing */
		WAT_FUNC_DBG_END
		return 1;
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
		wat_decode_type_of_address(atoi(cmdtokens[1]), &call->calling_num.type, &call->calling_num.plan);
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
	return 1;
}

WAT_NOTIFY_FUNC(wat_notify_creg)
{
	int stat;
	unsigned count;
	char *cmdtokens[3];
	int consumed_tokens = 0;
	
	WAT_NOTIFY_FUNC_DBG_START

	wat_remove_prefix(tokens[0], "+CREG: ");

	memset(cmdtokens, 0, sizeof(cmdtokens));
	count = wat_cmd_entry_tokenize(tokens[0], cmdtokens);

	if (count < 0) {
		wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CREG Response %s\n", tokens[0]);
		consumed_tokens = 1;
	} else if (count == 1) {
		stat = atoi(cmdtokens[0]);
		if (stat < 0) {
			wat_log_span(span, WAT_LOG_ERROR, "Failed to parse CREG Response %s\n", tokens[0]);
			consumed_tokens = 1;
		} else {
			wat_span_update_net_status(span, stat);
			consumed_tokens = 1;
		}
	} else {
		/* if count > 1, this is NOT an unsollicited notification, but an response
			(and the terminator has not been received yet) return 0, so we do not consume
			any tokens and wiat for a complete response */
		consumed_tokens = 0;
	}

	wat_free_tokens(cmdtokens);
	return consumed_tokens;
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
			case 0x1a:
				sprintf(p, "<sub>");
				p+=5;
				break;
			default:
				*p = data[i];
				p++;
		}
	}
	*p = '\0';
	return dest;
}

