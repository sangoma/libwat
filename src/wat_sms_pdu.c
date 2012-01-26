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


/* From www.dreamfabric.com/sms/default_alphabet.html */
static struct default_alphabet_val {
	uint8_t first_byte;
	uint8_t second_byte;
	wchar_t wchar;	/* Corresponding ISO-8859-I value */
} default_alphabet_vals[] = {
	{ 0x00, 0x00, 0x00000040 }, // COMMERCIAL AT
	{ 0x01, 0x00, 0x000000A3 }, // POUND SIGN
	{ 0x02, 0x00, 0x00000024 }, // DOLLAR SIGN
	{ 0x03, 0x00, 0x000000A5 }, // YEN SIGN
	{ 0x04, 0x00, 0x000000E8 }, // LATIN SMALL LETTER E WITH GRAVE
	{ 0x05, 0x00, 0x000000E9 }, // LATIN SMALL LETTER E WITH ACUTE
	{ 0x06, 0x00, 0x000000F9 }, // LATIN SMALL LETTER U WITH GRAVE
	{ 0x07, 0x00, 0x000000EC }, // LATIN SMALL LETTER I WITH GRAVE
	{ 0x08, 0x00, 0x000000F2 }, // LATIN SMALL LETTER O WITH GRAVE
	{ 0x09, 0x00, 0x000000C7 }, // LATIN CAPITAL LETTER C WITH CEDILLA
	{ 0x0A, 0x00, 0x0000000A }, // LINE FEED
	{ 0x0B, 0x00, 0x000000D8 }, // LATIN CAPITAL LETTER O WITH STROKE
	{ 0x0C, 0x00, 0x000000F8 }, // LATIN SMALL LETTER O WITH STROKE
	{ 0x0D, 0x00, 0x0000000D }, // CARRIAGE RETURN
	{ 0x0E, 0x00, 0x000000C5 }, // LATIN CAPITAL LETTER A WITH RING ABOVE
	{ 0x0F, 0x00, 0x000000E5 }, // LATIN SMALL LETTER A WITH RING ABOVE
	{ 0x10, 0x00, 0x00000394 }, // GREEK CAPITAL LETTER DELTA
	{ 0x11, 0x00, 0x0000005F }, // LOW LINE
	{ 0x12, 0x00, 0x000003A6 }, // GREEK CAPITAL LETTER PHI
	{ 0x13, 0x00, 0x00000393 }, // GREEK CAPITAL LETTER GAMMA
	{ 0x14, 0x00, 0x0000039B }, // GREEK CAPITAL LETTER LAMBDA
	{ 0x15, 0x00, 0x000003A9 }, // GREEK CAPITAL LETTER OMEGA
	{ 0x16, 0x00, 0x000003A0 }, // GREEK CAPITAL LETTER PI
	{ 0x17, 0x00, 0x000003A8 }, // GREEK CAPITAL LETTER PSI
	{ 0x18, 0x00, 0x000003A3 }, // GREEK CAPITAL LETTER SIGMA
	{ 0x19, 0x00, 0x00000398 }, // GREEK CAPITAL LETTER THETA
	{ 0x1A, 0x00, 0x0000039E }, // GREEK CAPITAL LETTER XI
	{ 0x1B, 0x0A, 0x0000000C }, // FORM FEED
	{ 0x1B, 0x14, 0x0000005E }, // CIRCUMFLEX ACCENT
	{ 0x1B, 0x28, 0x0000007B }, // LEFT CURLY BRACKET
	{ 0x1B, 0x29, 0x0000007D }, // RIGHT CURLY BRACKET
	{ 0x1B, 0x2F, 0x0000005C }, // REVERSE SOLIDUS (BACKSLASH)
	{ 0x1B, 0x3C, 0x0000005B }, // LEFT SQUARE BRACKET
	{ 0x1B, 0x3D, 0x0000007E }, // TILDE
	{ 0x1B, 0x3E, 0x0000005D }, // RIGHT SQUARE BRACKET
	{ 0x1B, 0x40, 0x0000007C }, // VERTICAL LINE
	{ 0x1B, 0x65, 0x000020AC }, // EURO SIGN
	{ 0x1C, 0x00, 0x000000C6 }, // LATIN CAPITAL LETTER AE
	{ 0x1D, 0x00, 0x000000E6 }, // LATIN SMALL LETTER AE
	{ 0x1E, 0x00, 0x000000DF }, // LATIN SMALL LETTER SHARP S (German)
	{ 0x1F, 0x00, 0x000000C9 }, // LATIN CAPITAL LETTER E WITH ACUTE
	{ 0x20, 0x00, 0x00000020 }, // SPACE
	{ 0x21, 0x00, 0x00000021 }, // EXCLAMATION MARK
	{ 0x22, 0x00, 0x00000022 }, // QUOTATION MARK
	{ 0x23, 0x00, 0x00000023 }, // NUMBER SIGN
	{ 0x24, 0x00, 0x000000A4 }, // CURRENCY SIGN
	{ 0x25, 0x00, 0x00000025 }, // PERCENT SIGN
	{ 0x26, 0x00, 0x00000026 }, // AMPERSAND
	{ 0x27, 0x00, 0x00000027 }, // APOSTROPHE
	{ 0x28, 0x00, 0x00000028 }, // LEFT PARENTHESIS
	{ 0x29, 0x00, 0x00000029 }, // RIGHT PARENTHESIS
	{ 0x2A, 0x00, 0x0000002A }, // ASTERISK
	{ 0x2B, 0x00, 0x0000002B }, // PLUS SIGN
	{ 0x2C, 0x00, 0x0000002C }, // COMMA
	{ 0x2D, 0x00, 0x0000002D }, // HYPHEN-MINUS
	{ 0x2E, 0x00, 0x0000002E }, // FULL STOP
	{ 0x2F, 0x00, 0x0000002F }, // SOLIDUS (SLASH)
	{ 0x30, 0x00, 0x00000030 }, // DIGIT ZERO
	{ 0x31, 0x00, 0x00000031 }, // DIGIT ONE
	{ 0x32, 0x00, 0x00000032 }, // DIGIT TWO
	{ 0x33, 0x00, 0x00000033 }, // DIGIT THREE
	{ 0x34, 0x00, 0x00000034 }, // DIGIT FOUR
	{ 0x35, 0x00, 0x00000035 }, // DIGIT FIVE
	{ 0x36, 0x00, 0x00000036 }, // DIGIT SIX
	{ 0x37, 0x00, 0x00000037 }, // DIGIT SEVEN
	{ 0x38, 0x00, 0x00000038 }, // DIGIT EIGHT
	{ 0x39, 0x00, 0x00000039 }, // DIGIT NINE
	{ 0x3A, 0x00, 0x0000003A }, // COLON
	{ 0x3B, 0x00, 0x0000003B }, // SEMICOLON
	{ 0x3C, 0x00, 0x0000003C }, // LESS-THAN SIGN
	{ 0x3D, 0x00, 0x0000003D }, // EQUALS SIGN
	{ 0x3E, 0x00, 0x0000003E }, // GREATER-THAN SIGN
	{ 0x3F, 0x00, 0x0000003F }, // QUESTION MARK
	{ 0x40, 0x00, 0x000000A1 }, // INVERTED EXCLAMATION MARK
	{ 0x41, 0x00, 0x00000041 }, // LATIN CAPITAL LETTER A
	{ 0x42, 0x00, 0x00000042 }, // LATIN CAPITAL LETTER B
	{ 0x43, 0x00, 0x00000043 }, // LATIN CAPITAL LETTER C
	{ 0x44, 0x00, 0x00000044 }, // LATIN CAPITAL LETTER D
	{ 0x45, 0x00, 0x00000045 }, // LATIN CAPITAL LETTER E
	{ 0x46, 0x00, 0x00000046 }, // LATIN CAPITAL LETTER F
	{ 0x47, 0x00, 0x00000047 }, // LATIN CAPITAL LETTER G
	{ 0x48, 0x00, 0x00000048 }, // LATIN CAPITAL LETTER H
	{ 0x49, 0x00, 0x00000049 }, // LATIN CAPITAL LETTER I
	{ 0x4A, 0x00, 0x0000004A }, // LATIN CAPITAL LETTER J
	{ 0x4B, 0x00, 0x0000004B }, // LATIN CAPITAL LETTER K
	{ 0x4C, 0x00, 0x0000004C }, // LATIN CAPITAL LETTER L
	{ 0x4D, 0x00, 0x0000004D }, // LATIN CAPITAL LETTER M
	{ 0x4E, 0x00, 0x0000004E }, // LATIN CAPITAL LETTER N
	{ 0x4F, 0x00, 0x0000004F }, // LATIN CAPITAL LETTER O
	{ 0x50, 0x00, 0x00000050 }, // LATIN CAPITAL LETTER P
	{ 0x51, 0x00, 0x00000051 }, // LATIN CAPITAL LETTER Q
	{ 0x52, 0x00, 0x00000052 }, // LATIN CAPITAL LETTER R
	{ 0x53, 0x00, 0x00000053 }, // LATIN CAPITAL LETTER S
	{ 0x54, 0x00, 0x00000054 }, // LATIN CAPITAL LETTER T
	{ 0x55, 0x00, 0x00000055 }, // LATIN CAPITAL LETTER U
	{ 0x56, 0x00, 0x00000056 }, // LATIN CAPITAL LETTER V
	{ 0x57, 0x00, 0x00000057 }, // LATIN CAPITAL LETTER W
	{ 0x58, 0x00, 0x00000058 }, // LATIN CAPITAL LETTER X
	{ 0x59, 0x00, 0x00000059 }, // LATIN CAPITAL LETTER Y
	{ 0x5A, 0x00, 0x0000005A }, // LATIN CAPITAL LETTER Z
	{ 0x5B, 0x00, 0x000000C4 }, // LATIN CAPITAL LETTER A WITH DIAERESIS
	{ 0x5C, 0x00, 0x000000D6 }, // LATIN CAPITAL LETTER O WITH DIAERESIS
	{ 0x5D, 0x00, 0x000000D1 }, // LATIN CAPITAL LETTER N WITH TILDE
	{ 0x5E, 0x00, 0x000000DC }, // LATIN CAPITAL LETTER U WITH DIAERESIS
	{ 0x5F, 0x00, 0x000000A7 }, // SECTION SIGN
	{ 0x60, 0x00, 0x000000BF }, // INVERTED QUESTION MARK
	{ 0x61, 0x00, 0x00000061 }, // LATIN SMALL LETTER A
	{ 0x62, 0x00, 0x00000062 }, // LATIN SMALL LETTER B
	{ 0x63, 0x00, 0x00000063 }, // LATIN SMALL LETTER C
	{ 0x64, 0x00, 0x00000064 }, // LATIN SMALL LETTER D
	{ 0x65, 0x00, 0x00000065 }, // LATIN SMALL LETTER E
	{ 0x66, 0x00, 0x00000066 }, // LATIN SMALL LETTER F
	{ 0x67, 0x00, 0x00000067 }, // LATIN SMALL LETTER G
	{ 0x68, 0x00, 0x00000068 }, // LATIN SMALL LETTER H
	{ 0x69, 0x00, 0x00000069 }, // LATIN SMALL LETTER I
	{ 0x6A, 0x00, 0x0000006A }, // LATIN SMALL LETTER J
	{ 0x6B, 0x00, 0x0000006B }, // LATIN SMALL LETTER K
	{ 0x6C, 0x00, 0x0000006C }, // LATIN SMALL LETTER L
	{ 0x6D, 0x00, 0x0000006D }, // LATIN SMALL LETTER M
	{ 0x6E, 0x00, 0x0000006E }, // LATIN SMALL LETTER N
	{ 0x6F, 0x00, 0x0000006F }, // LATIN SMALL LETTER O
	{ 0x70, 0x00, 0x00000070 }, // LATIN SMALL LETTER P
	{ 0x71, 0x00, 0x00000071 }, // LATIN SMALL LETTER Q
	{ 0x72, 0x00, 0x00000072 }, // LATIN SMALL LETTER R
	{ 0x73, 0x00, 0x00000073 }, // LATIN SMALL LETTER S
	{ 0x74, 0x00, 0x00000074 }, // LATIN SMALL LETTER T
	{ 0x75, 0x00, 0x00000075 }, // LATIN SMALL LETTER U
	{ 0x76, 0x00, 0x00000076 }, // LATIN SMALL LETTER V
	{ 0x77, 0x00, 0x00000077 }, // LATIN SMALL LETTER W
	{ 0x78, 0x00, 0x00000078 }, // LATIN SMALL LETTER X
	{ 0x79, 0x00, 0x00000079 }, // LATIN SMALL LETTER Y
	{ 0x7A, 0x00, 0x0000007A }, // LATIN SMALL LETTER Z
	{ 0x7B, 0x00, 0x000000E4 }, // LATIN SMALL LETTER A WITH DIAERESIS
	{ 0x7C, 0x00, 0x000000F6 }, // LATIN SMALL LETTER O WITH DIAERESIS
	{ 0x7D, 0x00, 0x000000F1 }, // LATIN SMALL LETTER N WITH TILDE
	{ 0x7E, 0x00, 0x000000FC }, // LATIN SMALL LETTER U WITH DIAERESIS
	{ 0x7F, 0x00, 0x000000E0 }, // LATIN SMALL LETTER A WITH GRAVE
};

wat_status_t wat_verify_default_alphabet(char *content_data)
{
	wat_bool_t matched;
	unsigned j;
	wchar_t *c;

	c = (wchar_t *)content_data;
			
	while (*c != L'\0') {
		matched = WAT_FALSE;
		for (j = 0; j < wat_array_len(default_alphabet_vals); j++) {
			if (default_alphabet_vals[j].wchar == *c) {
				matched = WAT_TRUE;
				break;
			}
		}
		if (matched == WAT_FALSE) {
			return WAT_FAIL;
		}
		c++;
	}
	return WAT_SUCCESS;
}

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

wat_status_t wat_encode_sms_pdu_message_ref(uint8_t tp_message_ref, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	/* TP-Message-Reference */
	**outdata = tp_message_ref;

	*outdata = *outdata + 1;
	*outdata_len = *outdata_len + 1;

	if (g_debug & WAT_DEBUG_SMS_ENCODE) {
		wat_log(WAT_LOG_DEBUG, "PDU: TP-Message-Reference:0x%02x\n", tp_message_ref);
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

wat_status_t wat_encode_sms_pdu_udh(wat_sms_pdu_udh_t *udh, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size)
{
	/* From en.wikipedia.org/wiki/Concatenated_SMS */	
	char *ptr = *outdata;

	/* Length of the User Data Header - we will fill the length at the end */
	ptr++;
	

	/* Information Element Identifier */
	*(ptr++) = WAT_SMS_PDU_UDH_IEI_CONCATENATED_SMS_8BIT;

	/* Length of the header, excluding the first two fields */
	*(ptr++) = 0x03;

	/* CSMS reference number, must be the same for all the SMS parts in the CSMS */
	*(ptr++) = udh->refnr;

	/* Total number of parts. The value shall remain constant for every short message in the CSMS */
	*(ptr++) = udh->total;

	/* Part number in the sequence. Starts at 1 and increments for every new message */
	*(ptr++) = udh->seq;

	/* Fill in the length */
	**outdata = ptr - *outdata - 1;

	*outdata_len += (**outdata) + 1;
	*outdata = ptr;
	
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

void write_septet(char *out, unsigned septet, uint8_t byte)
{
	int pos = ((septet + 1) * 7) / 8;
	int shift = septet % 8;

	if (pos > 0) {
		out[pos-1] |= (byte << (8 - shift)) & 0xFF;
		out[pos] |= byte >> shift;
	} else {
		out[pos] |= byte;
	}
}


wat_status_t wat_encode_sms_pdu_message_7bit(wchar_t *indata, wat_size_t indata_size, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size, uint8_t offset)
{
	unsigned septet;
	char *out;
	int i, j;
	wat_bool_t matched;

	septet = offset;
	out = *outdata;

	for (i = 0; i < (indata_size/4) ; i++) {
		matched = WAT_FALSE;
		for (j = 0; j < wat_array_len(default_alphabet_vals); j++) {
			if (default_alphabet_vals[j].wchar == indata[i]) {
				matched = WAT_TRUE;
				break;
			}
		}

		if (matched == WAT_TRUE) {			
			write_septet(out, septet++, default_alphabet_vals[j].first_byte);

			if (default_alphabet_vals[j].second_byte) {
				write_septet(out, septet++, default_alphabet_vals[j].second_byte);
			}
		} else {
			wat_log(WAT_LOG_ERROR, "Failed to translate char 0x%08X into GSM alphabet (index:%d len:%d)\n", indata[i], i, indata_size);
			return WAT_FAIL;
		}
	}
	*outdata_len = septet - offset;

	*outdata = out;
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
