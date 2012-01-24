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

#include <iconv.h>
#include <wchar.h>
#include <errno.h>

#include "libwat.h"
#include "wat_internal.h"
#include "wat_sms_pdu.h"

static int wat_encode_sms_pdu_semi_octets(char *data, char *string, wat_size_t len);
static int wat_decode_sms_pdu_semi_octets(char *string, char *data, wat_size_t len);

static uint8_t lo_bits(uint8_t byte, uint8_t numbits)
{
	switch(numbits) {
		case 0:
			return byte & 0x00;
		case 1:
			return byte & 0x01;
		case 2:
			return byte & 0x03;
		case 3:
			return byte & 0x07;
		case 4:
			return byte & 0x0F;
		case 5:
			return byte & 0x1F;
		case 6:
			return byte & 0x3F;
		case 7:
			return byte & 0x7F;
	}
	return 0;
}

static uint8_t hi_bits(uint8_t byte, uint8_t numbits)
{
	return byte >> (8 - numbits);
}

static int wat_decode_sms_pdu_semi_octets(char *string, char *data, wat_size_t len)
{
	int i;
	int ret = 0;
	char *p = string;

	for (i = 0; i < (len*2); i+=2) {
		*p++ = data[i+1];
		ret++;
		if (data[i] != 'F') {
			*p++ = data[i];
			ret++;
		}
	}
	return ret;
}

static int wat_encode_sms_pdu_semi_octets(char *data, char *string, wat_size_t len)
{
	int i;
	int ret = 0;
	char *p = data;
	uint8_t odd = len % 2;
	
	for (i = 0; i < (len - odd); i+=2) {
		char lo_nibble[2];
		char hi_nibble[2];

		lo_nibble[0] = string[i];
		lo_nibble[1] = '\0';

		hi_nibble[0] = string[i+1];
		hi_nibble[1] = '\0';
		
		*p++ = (atoi(hi_nibble) << 4)|(atoi(lo_nibble));
		ret++;
	}
	
	if (odd) {
		*p++ = 0xF0 | string[i];
		ret++;
	}

	return ret;
}


wat_status_t wat_encode_sms_pdu_smsc(wat_number_t *smsc, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	char *data = *outdata;
	char *digits = smsc->digits;
	unsigned digits_len = 0;
	unsigned len = 0;

	if (digits[0] == '+') {
		digits++;
	}
	
	/* Length SMSC information */
	data[len] = (1 + (strlen(digits) + 1) / 2);
	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG, "SMSC Address-Length:0x%02x\n", 0xFF & data[len]);
	}
	len++;
;

	/* SMSC Type-of-Address */
	data[len] = 0x80 | ((smsc->type & 0x7) << 4) | (smsc->plan & 0xF);

	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG,  "SMSC Type-Of-Address:0x%02x\n", 0xFF & data[len]);
	}
	len++;

	digits_len = wat_encode_sms_pdu_semi_octets(&data[len], digits, strlen(digits));

	*outdata = &data[len + digits_len];
	*outdata_len = *outdata_len + len + digits_len;

	return WAT_SUCCESS;
}

wat_status_t wat_encode_sms_pdu_to(wat_number_t *to, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	char *data = *outdata;
	char *digits = to->digits;
	unsigned digits_len = 0;
	unsigned len = 0;

	if (digits[0] == '+') {
		digits++;
	}

	/* Address-Length. Length of phone number */
	data[len] = strlen(to->digits);
	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG, "To Address-Length:0x%02x\n", 0xFF & data[len]);
	}
	len++;

	/* Destination Type-of-Address */
	data[len] = 0x80 | ((to->type & 0x07) << 4) | (to->plan & 0xF);
	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG,  "To Type-Of-Address:0x%02x\n", 0xFF & data[len]);
	}
	len++;

	digits_len = wat_encode_sms_pdu_semi_octets(&data[len], digits, strlen(digits));
	
	*outdata = &data[len + digits_len];
	*outdata_len = *outdata_len + len + digits_len;

	return WAT_SUCCESS;
}
	

wat_status_t wat_encode_sms_pdu_submit(wat_sms_pdu_submit_t *submit, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	char *data = *outdata;

	*data = submit->tp_rp << 7;
	*data |= (submit->tp_udhi & 0x1) << 6;
	*data |= (submit->tp_srr & 0x1) << 5;
	*data |= (submit->vp.type & 0x3) << 3;
	*data |= (submit->tp_rd & 0x1) << 1;
	*data |= 0x01; /* mti = SMS-SUBMIT */


	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG, "SMS-SUBMIT:0x%02x\n", *data);
	}

	*outdata = *outdata + 1;
	*outdata_len = *outdata_len + 1;
	return WAT_SUCCESS;
}

wat_status_t wat_encode_sms_pdu_message_ref(uint8_t refnr, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	/* TP-Message-Reference */
	**outdata = refnr;

	*outdata = *outdata + 1;
	*outdata_len = *outdata_len + 1;

	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG, "PDU: TP-Message-Reference:0x%02x\n", refnr);
	}
	return WAT_SUCCESS;
}

wat_status_t wat_encode_sms_pdu_pid(uint8_t pid, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	/* TP-PID - Protocol Identifier */
	**outdata = pid;

	*outdata = *outdata + 1;
	*outdata_len = *outdata_len + 1;


	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG, "PDU: TP-PID:0x%02x\n", pid);
	}
	return WAT_SUCCESS;
}

wat_status_t wat_encode_sms_pdu_dcs(wat_sms_pdu_dcs_t *dcs, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	char *data = *outdata;

	*data = (dcs->compressed & 0x01) << 4;

	/* Bit 4 - Message class present or not */
	if (dcs->msg_class != WAT_SMS_PDU_DCS_MSG_CLASS_INVALID) {
		*data |= 0x10;
	}

	/* Bits 3 & 2 - Alphabet */
	*data |= (dcs->alphabet & 0x03) << 2;

	/* Bits 0 & 1 - Message class */
	*data |= (dcs->msg_class & 0x03);
	
	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG,  "TP-DCS:0x%02x\n", *data);
	}

	*outdata = *outdata + 1;
	*outdata_len = *outdata_len + 1;

	return WAT_SUCCESS;
}

wat_status_t wat_encode_sms_pdu_vp(wat_sms_pdu_vp_t *vp, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	/* TP-VP - Validity Period */
	/* www.dreamfabric.com/sms/vp.html */
	switch (vp->type) {
		case WAT_SMS_PDU_VP_NOT_PRESENT:
			break;
		case WAT_SMS_PDU_VP_RELATIVE:
			**outdata = vp->data.relative;
			*outdata = *outdata + 1;
			*outdata_len = *outdata_len + 1;
			break;
		case WAT_SMS_PDU_VP_ABSOLUTE:
		case WAT_SMS_PDU_VP_ENHANCED:		
			wat_log(WAT_LOG_ERROR, "Validity period type not supported\n");
			return WAT_FAIL;
		case WAT_SMS_PDU_VP_INVALID:
			wat_log(WAT_LOG_ERROR, "Invalid Validity Period\n");
			return WAT_FAIL;
	}
	return WAT_SUCCESS;
}

wat_status_t wat_encode_sms_pdu_message_ucs2(char *indata, wat_size_t indata_size, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	iconv_t cd;
	char *pindata = indata;
	char *content = NULL;
	char *pcontent = NULL;
	char *outdata_start = *outdata;

	wat_size_t data_len = wcslen((const wchar_t *)indata) * sizeof(wchar_t);
	wat_size_t len_avail = outdata_size;

	content = (*outdata) + 1;
	pcontent = content;

	cd = iconv_open("UCS-2BE", "WCHAR_T");
	wat_assert_return(cd >= 0, WAT_FAIL, "Failed to create object for wide character conversion\n");

	if (iconv(cd, &pindata, &data_len, &pcontent, &len_avail) < 0) {
		wat_log(WAT_LOG_ERROR, "Failed to convert into UCS2 (%s)", strerror(errno));
		iconv_close(cd);
		return WAT_FAIL;
	}

	*outdata_start = (outdata_size - len_avail);
	*outdata_len += ((*outdata_start) + 1);
	*outdata = *outdata + *outdata_len;	

	
	iconv_close(cd);
	return WAT_SUCCESS;
}

wat_status_t wat_encode_sms_pdu_message_7bit(wchar_t *indata, wat_size_t indata_size, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size, uint8_t padding)
{
	uint8_t carry;
	uint8_t byte;
	uint8_t next_byte;
	unsigned i;

	char *content = NULL;
	wchar_t *data = NULL;
	wat_size_t indata_size_char;


	indata_size_char = indata_size/4;
	content = (*outdata) + 1;

	data = wat_malloc(indata_size + 1);

	wat_assert_return(data, WAT_FAIL, "Failed to malloc");
	memcpy(data, indata, indata_size);

	for (i = 0; i < indata_size_char; i++) {
		uint8_t j = i % 8;
		if (j != 7) {
			carry = lo_bits(data[i+1], (j+1));

			next_byte = hi_bits(data[i+1], (7-j));
			data[i+1] = next_byte;

			byte = lo_bits(data[i], (7-j)) | carry << (7-j);

			*content = byte;
			content++;
		}
	}

	wat_safe_free(data);
	
	*content = '\0';
	**outdata = indata_size_char;

	/* Increase the length in septets */
	*outdata_len = *outdata_len + (indata_size_char * 7) / 8 + (((indata_size_char * 7) % 8) ? 1 : 0) + 1;

	*outdata = content;	

	return WAT_SUCCESS;
}

void print_buffer(wat_loglevel_t loglevel, char *data, wat_size_t data_len, char *message)
{
	int x;
	char str[5000];
	wat_size_t str_len = 0;

	for (x = 0; x < data_len; x++) {
		str_len += sprintf(&str[str_len], "0x%02X ", 0xFF & data[x]);
		if (x && !((x+1)%16)) {
			str_len += sprintf(&str[str_len], "\n");
		} else if (x && !((x+1)%8)) {
			str_len += sprintf(&str[str_len], "     ");
		}
	}
	wat_log(loglevel, "\n\n %s \n%s\n(len:%d)\n\n", message, str, data_len);
	return;
}
