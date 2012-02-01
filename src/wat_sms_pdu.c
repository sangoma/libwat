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

static uint8_t bit(uint8_t byte, uint8_t bitpos);

/* From www.dreamfabric.com/sms/default_alphabet.html */
/* Matching ASCII values from www.developershome.com/sms/gsmAlphabet.asp */
static struct default_alphabet_val {
	uint8_t first_byte;
	uint8_t second_byte;
	wchar_t wchar;	/* Corresponding ISO-8859-I value */
	uint8_t ascii;	/* Corresponding ASCII value. 0xFF if not exist */
} default_alphabet_vals[] = {
	{ 0x00, 0x00, 0x00000040, 0x40 }, // COMMERCIAL AT
	{ 0x01, 0x00, 0x000000A3, 0x9C }, // POUND SIGN
	{ 0x02, 0x00, 0x00000024, 0x24 }, // DOLLAR SIGN
	{ 0x03, 0x00, 0x000000A5, 0x9D }, // YEN SIGN
	{ 0x04, 0x00, 0x000000E8, 0x8A }, // LATIN SMALL LETTER E WITH GRAVE
	{ 0x05, 0x00, 0x000000E9, 0x82 }, // LATIN SMALL LETTER E WITH ACUTE
	{ 0x06, 0x00, 0x000000F9, 0x97 }, // LATIN SMALL LETTER U WITH GRAVE
	{ 0x07, 0x00, 0x000000EC, 0x8D }, // LATIN SMALL LETTER I WITH GRAVE
	{ 0x08, 0x00, 0x000000F2, 0x95 }, // LATIN SMALL LETTER O WITH GRAVE
	{ 0x09, 0x00, 0x000000C7, 0x80 }, // LATIN CAPITAL LETTER C WITH CEDILLA
	{ 0x0A, 0x00, 0x0000000A, 0x0A }, // LINE FEED
	{ 0x0B, 0x00, 0x000000D8, 0xFF }, // LATIN CAPITAL LETTER O WITH STROKE
	{ 0x0C, 0x00, 0x000000F8, 0xFF }, // LATIN SMALL LETTER O WITH STROKE
	{ 0x0D, 0x00, 0x0000000D, 0x0D }, // CARRIAGE RETURN
	{ 0x0E, 0x00, 0x000000C5, 0x8F }, // LATIN CAPITAL LETTER A WITH RING ABOVE
	{ 0x0F, 0x00, 0x000000E5, 0x86 }, // LATIN SMALL LETTER A WITH RING ABOVE
	{ 0x10, 0x00, 0x00000394, 0xFF }, // GREEK CAPITAL LETTER DELTA
	{ 0x11, 0x00, 0x0000005F, 0x5F }, // UNDERSCORE
	{ 0x12, 0x00, 0x000003A6, 0xE8 }, // GREEK CAPITAL LETTER PHI
	{ 0x13, 0x00, 0x00000393, 0xE2 }, // GREEK CAPITAL LETTER GAMMA
	{ 0x14, 0x00, 0x0000039B, 0xFF }, // GREEK CAPITAL LETTER LAMBDA
	{ 0x15, 0x00, 0x000003A9, 0xEA }, // GREEK CAPITAL LETTER OMEGA
	{ 0x16, 0x00, 0x000003A0, 0xFF }, // GREEK CAPITAL LETTER PI
	{ 0x17, 0x00, 0x000003A8, 0xFF }, // GREEK CAPITAL LETTER PSI
	{ 0x18, 0x00, 0x000003A3, 0xFF }, // GREEK CAPITAL LETTER SIGMA
	{ 0x19, 0x00, 0x00000398, 0xFF }, // GREEK CAPITAL LETTER THETA
	{ 0x1A, 0x00, 0x0000039E, 0xF0 }, // GREEK CAPITAL LETTER XI
	{ 0x1B, 0x0A, 0x0000000C, 0x0C }, // FORM FEED
	{ 0x1B, 0x14, 0x0000005E, 0x5E }, // CIRCUMFLEX ACCENT
	{ 0x1B, 0x28, 0x0000007B, 0x7B }, // LEFT CURLY BRACKET
	{ 0x1B, 0x29, 0x0000007D, 0x7D }, // RIGHT CURLY BRACKET
	{ 0x1B, 0x2F, 0x0000005C, 0x5C }, // BACKSLASH
	{ 0x1B, 0x3C, 0x0000005B, 0x5B }, // LEFT SQUARE BRACKET
	{ 0x1B, 0x3D, 0x0000007E, 0x7E }, // TILDE
	{ 0x1B, 0x3E, 0x0000005D, 0x5D }, // RIGHT SQUARE BRACKET
	{ 0x1B, 0x40, 0x0000007C, 0x7C }, // VERTICAL BAR
	{ 0x1B, 0x65, 0x000020AC, 0xFF }, // EURO SIGN
	{ 0x1C, 0x00, 0x000000C6, 0x92 }, // LATIN CAPITAL LETTER AE
	{ 0x1D, 0x00, 0x000000E6, 0x91 }, // LATIN SMALL LETTER AE
	{ 0x1E, 0x00, 0x000000DF, 0xFF }, // SMALL LETER ESZETT
	{ 0x1F, 0x00, 0x000000C9, 0x90 }, // LATIN CAPITAL LETTER E WITH ACUTE
	{ 0x20, 0x00, 0x00000020, 0x20 }, // SPACE
	{ 0x21, 0x00, 0x00000021, 0x21 }, // EXCLAMATION MARK
	{ 0x22, 0x00, 0x00000022, 0x22 }, // QUOTATION MARK
	{ 0x23, 0x00, 0x00000023, 0x23 }, // NUMBER SIGN
	{ 0x24, 0x00, 0x000000A4, 0xFF }, // CURRENCY SIGN
	{ 0x25, 0x00, 0x00000025, 0x25 }, // PERCENT SIGN
	{ 0x26, 0x00, 0x00000026, 0x26 }, // AMPERSAND
	{ 0x27, 0x00, 0x00000027, 0x27 }, // APOSTROPHE
	{ 0x28, 0x00, 0x00000028, 0x28 }, // LEFT PARENTHESIS
	{ 0x29, 0x00, 0x00000029, 0x29 }, // RIGHT PARENTHESIS
	{ 0x2A, 0x00, 0x0000002A, 0x2A }, // ASTERISK
	{ 0x2B, 0x00, 0x0000002B, 0x2B }, // PLUS SIGN
	{ 0x2C, 0x00, 0x0000002C, 0x2C }, // COMMA
	{ 0x2D, 0x00, 0x0000002D, 0x2D }, // HYPHEN-MINUS
	{ 0x2E, 0x00, 0x0000002E, 0x2E }, // FULL STOP
	{ 0x2F, 0x00, 0x0000002F, 0x2F }, // SLASH
	{ 0x30, 0x00, 0x00000030, 0x30 }, // DIGIT ZERO
	{ 0x31, 0x00, 0x00000031, 0x31 }, // DIGIT ONE
	{ 0x32, 0x00, 0x00000032, 0x32 }, // DIGIT TWO
	{ 0x33, 0x00, 0x00000033, 0x33 }, // DIGIT THREE
	{ 0x34, 0x00, 0x00000034, 0x34 }, // DIGIT FOUR
	{ 0x35, 0x00, 0x00000035, 0x35 }, // DIGIT FIVE
	{ 0x36, 0x00, 0x00000036, 0x36 }, // DIGIT SIX
	{ 0x37, 0x00, 0x00000037, 0x37 }, // DIGIT SEVEN
	{ 0x38, 0x00, 0x00000038, 0x38 }, // DIGIT EIGHT
	{ 0x39, 0x00, 0x00000039, 0x39 }, // DIGIT NINE
	{ 0x3A, 0x00, 0x0000003A, 0x3A }, // COLON
	{ 0x3B, 0x00, 0x0000003B, 0x3B }, // SEMICOLON
	{ 0x3C, 0x00, 0x0000003C, 0x3C }, // LESS-THAN SIGN
	{ 0x3D, 0x00, 0x0000003D, 0x3D }, // EQUALS SIGN
	{ 0x3E, 0x00, 0x0000003E, 0x3E }, // GREATER-THAN SIGN
	{ 0x3F, 0x00, 0x0000003F, 0x3F }, // QUESTION MARK
	{ 0x40, 0x00, 0x000000A1, 0xFF }, // INVERTED EXCLAMATION MARK
	{ 0x41, 0x00, 0x00000041, 0x41 }, // LATIN CAPITAL LETTER A
	{ 0x42, 0x00, 0x00000042, 0x42 }, // LATIN CAPITAL LETTER B
	{ 0x43, 0x00, 0x00000043, 0x43 }, // LATIN CAPITAL LETTER C
	{ 0x44, 0x00, 0x00000044, 0x44 }, // LATIN CAPITAL LETTER D
	{ 0x45, 0x00, 0x00000045, 0x45 }, // LATIN CAPITAL LETTER E
	{ 0x46, 0x00, 0x00000046, 0x46 }, // LATIN CAPITAL LETTER F
	{ 0x47, 0x00, 0x00000047, 0x47 }, // LATIN CAPITAL LETTER G
	{ 0x48, 0x00, 0x00000048, 0x48 }, // LATIN CAPITAL LETTER H
	{ 0x49, 0x00, 0x00000049, 0x49 }, // LATIN CAPITAL LETTER I
	{ 0x4A, 0x00, 0x0000004A, 0x4A }, // LATIN CAPITAL LETTER J
	{ 0x4B, 0x00, 0x0000004B, 0x4B }, // LATIN CAPITAL LETTER K
	{ 0x4C, 0x00, 0x0000004C, 0x4C }, // LATIN CAPITAL LETTER L
	{ 0x4D, 0x00, 0x0000004D, 0x4D }, // LATIN CAPITAL LETTER M
	{ 0x4E, 0x00, 0x0000004E, 0x4E }, // LATIN CAPITAL LETTER N
	{ 0x4F, 0x00, 0x0000004F, 0x4F }, // LATIN CAPITAL LETTER O
	{ 0x50, 0x00, 0x00000050, 0x50 }, // LATIN CAPITAL LETTER P
	{ 0x51, 0x00, 0x00000051, 0x51 }, // LATIN CAPITAL LETTER Q
	{ 0x52, 0x00, 0x00000052, 0x52 }, // LATIN CAPITAL LETTER R
	{ 0x53, 0x00, 0x00000053, 0x53 }, // LATIN CAPITAL LETTER S
	{ 0x54, 0x00, 0x00000054, 0x54 }, // LATIN CAPITAL LETTER T
	{ 0x55, 0x00, 0x00000055, 0x55 }, // LATIN CAPITAL LETTER U
	{ 0x56, 0x00, 0x00000056, 0x56 }, // LATIN CAPITAL LETTER V
	{ 0x57, 0x00, 0x00000057, 0x57 }, // LATIN CAPITAL LETTER W
	{ 0x58, 0x00, 0x00000058, 0x58 }, // LATIN CAPITAL LETTER X
	{ 0x59, 0x00, 0x00000059, 0x59 }, // LATIN CAPITAL LETTER Y
	{ 0x5A, 0x00, 0x0000005A, 0x5A }, // LATIN CAPITAL LETTER Z
	{ 0x5B, 0x00, 0x000000C4, 0x8E }, // LATIN CAPITAL LETTER A WITH DIAERESIS
	{ 0x5C, 0x00, 0x000000D6, 0x99 }, // LATIN CAPITAL LETTER O WITH DIAERESIS
	{ 0x5D, 0x00, 0x000000D1, 0xA5 }, // LATIN CAPITAL LETTER N WITH TILDE
	{ 0x5E, 0x00, 0x000000DC, 0xFF }, // LATIN CAPITAL LETTER U WITH DIAERESIS
	{ 0x5F, 0x00, 0x000000A7, 0xFF }, // SECTION SIGN
	{ 0x60, 0x00, 0x000000BF, 0xFF }, // INVERTED QUESTION MARK
	{ 0x61, 0x00, 0x00000061, 0x61 }, // LATIN SMALL LETTER A
	{ 0x62, 0x00, 0x00000062, 0x62 }, // LATIN SMALL LETTER B
	{ 0x63, 0x00, 0x00000063, 0x63 }, // LATIN SMALL LETTER C
	{ 0x64, 0x00, 0x00000064, 0x64 }, // LATIN SMALL LETTER D
	{ 0x65, 0x00, 0x00000065, 0x65 }, // LATIN SMALL LETTER E
	{ 0x66, 0x00, 0x00000066, 0x66 }, // LATIN SMALL LETTER F
	{ 0x67, 0x00, 0x00000067, 0x67 }, // LATIN SMALL LETTER G
	{ 0x68, 0x00, 0x00000068, 0x68 }, // LATIN SMALL LETTER H
	{ 0x69, 0x00, 0x00000069, 0x69 }, // LATIN SMALL LETTER I
	{ 0x6A, 0x00, 0x0000006A, 0x6A }, // LATIN SMALL LETTER J
	{ 0x6B, 0x00, 0x0000006B, 0x6B }, // LATIN SMALL LETTER K
	{ 0x6C, 0x00, 0x0000006C, 0x6C }, // LATIN SMALL LETTER L
	{ 0x6D, 0x00, 0x0000006D, 0x6D }, // LATIN SMALL LETTER M
	{ 0x6E, 0x00, 0x0000006E, 0x6E }, // LATIN SMALL LETTER N
	{ 0x6F, 0x00, 0x0000006F, 0x6F }, // LATIN SMALL LETTER O
	{ 0x70, 0x00, 0x00000070, 0x70 }, // LATIN SMALL LETTER P
	{ 0x71, 0x00, 0x00000071, 0x71 }, // LATIN SMALL LETTER Q
	{ 0x72, 0x00, 0x00000072, 0x72 }, // LATIN SMALL LETTER R
	{ 0x73, 0x00, 0x00000073, 0x73 }, // LATIN SMALL LETTER S
	{ 0x74, 0x00, 0x00000074, 0x74 }, // LATIN SMALL LETTER T
	{ 0x75, 0x00, 0x00000075, 0x75 }, // LATIN SMALL LETTER U
	{ 0x76, 0x00, 0x00000076, 0x76 }, // LATIN SMALL LETTER V
	{ 0x77, 0x00, 0x00000077, 0x77 }, // LATIN SMALL LETTER W
	{ 0x78, 0x00, 0x00000078, 0x78 }, // LATIN SMALL LETTER X
	{ 0x79, 0x00, 0x00000079, 0x79 }, // LATIN SMALL LETTER Y
	{ 0x7A, 0x00, 0x0000007A, 0x7A }, // LATIN SMALL LETTER Z
	{ 0x7B, 0x00, 0x000000E4, 0x84 }, // LATIN SMALL LETTER A WITH DIAERESIS
	{ 0x7C, 0x00, 0x000000F6, 0x94 }, // LATIN SMALL LETTER O WITH DIAERESIS
	{ 0x7D, 0x00, 0x000000F1, 0xA4 }, // LATIN SMALL LETTER N WITH TILDE
	{ 0x7E, 0x00, 0x000000FC, 0x81 }, // LATIN SMALL LETTER U WITH DIAERESIS
	{ 0x7F, 0x00, 0x000000E0, 0x85 }, // LATIN SMALL LETTER A WITH GRAVE
};


static void write_septet(char *out, unsigned septet, uint8_t byte)
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

#if 0
static uint8_t hexstr_to_val(char *string)
{
	char octet_string[3];
	int octet;

	memset(octet_string, 0, sizeof(octet_string));
	memcpy(octet_string, string, 2);
	sscanf(octet_string, "%x", &octet);
	return (uint8_t )octet;
}

static uint8_t decstr_to_val(char *string)
{
	char octet_string[3];
	int octet;

	memset(octet_string, 0, sizeof(octet_string));
	memcpy(octet_string, string, 2);
	sscanf(octet_string, "%d", &octet);
	return (uint8_t )octet;
}
#endif

static uint8_t bit(uint8_t byte, uint8_t bitpos)
{
	return (byte >> bitpos) & 0x01;
}

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

#if 0
wat_status_t wat_verify_ascii(uint8_t *data, wat_size_t len)
{
	int i, j;
	
	for(i = 0; i < len; i++) {
		for (j = 0; j < wat_array_len(default_alphabet_vals); j++) {
			if (default_alphabet_vals[j].first_byte == data[i]) {
				if (default_alphabet_vals[j].ascii == 0xFF) {
					return WAT_FAIL;
				}
			}
		}
	}
	return WAT_SUCCESS;
}
#endif

wat_status_t wat_convert_ascii(char *raw_content, wat_size_t *raw_content_len)
{
	wat_status_t status;
	int i, j;
	char *data = NULL;
	char *p;

	status = WAT_SUCCESS;

	data = wat_malloc(*raw_content_len);

	wat_assert_return(data, WAT_FAIL, "Failed to malloc");
	memset(data, 0, *raw_content_len);

	p = data;

	for(i = 0; i < (*raw_content_len) - 1; i++) {
		wat_bool_t matched = WAT_FALSE;

		for (j = 0; j < wat_array_len(default_alphabet_vals); j++) {
			if (default_alphabet_vals[j].first_byte == raw_content[i]) {
				if (default_alphabet_vals[j].second_byte) {
					if ((i + 1) < *raw_content_len &&
						default_alphabet_vals[j].second_byte == raw_content[i + 1]) {
						i++;
						matched = WAT_TRUE;
						break;
					}
				} else {
					matched = WAT_TRUE;
					break;
				}
			}
		}

		if (matched == WAT_FALSE) {
			status = WAT_FAIL;
			goto done;
		} else {
			if (default_alphabet_vals[j].ascii == 0xFF) {
				/* Cannot convert char to ascii */
				status = WAT_FAIL;
				goto done;
			}
			*p = default_alphabet_vals[j].ascii;
			p++;
		}
	}
	*p = '\0';

done:
	if (status == WAT_SUCCESS) {
		memcpy(raw_content, data, strlen(data));
	}
	wat_safe_free(data);
	return status;
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


static int wat_decode_sms_pdu_semi_octets(char *string, char *data, wat_size_t len)
{
	int i;
	char *p = string;

	for (i = 0; i < len; i++) {
		sprintf(p++, "%d", data[i] & 0x0F);
		if (((data[i] & 0xFF) >> 4) != 0x0f) {
			sprintf(p++, "%d", (data[i] >> 4) & 0x0F);
		}
	}
	return strlen(string);
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

/*-------------------------------------------------------------------------------------------------
   ENCODING FUNCTIONS
-------------------------------------------------------------------------------------------------*/

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


/*-------------------------------------------------------------------------------------------------
   DECODING FUNCTIONS
-------------------------------------------------------------------------------------------------*/

wat_status_t wat_decode_sms_pdu_message_ucs2(char *outdata, wat_size_t *outdata_len, wat_size_t outdata_size, wat_size_t inmessage_len, char **indata, wat_size_t size)
{
	iconv_t cd;
	char *pindata = *indata;
	char *poutdata = outdata;
	wat_size_t message_len = inmessage_len;
	
	wat_size_t len_avail = outdata_size;
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding message from UCS2 len:%d\n", message_len);
	}

	cd = iconv_open("UTF-8", "UCS-2BE");
	wat_assert_return(cd >= 0, WAT_FAIL, "Failed to create object for UCS2 conversion\n");

	if (iconv(cd, &pindata, &message_len, &poutdata, &len_avail) < 0) {
		wat_log(WAT_LOG_ERROR, "Failed to convert from UCS2 (%s)", strerror(errno));
		iconv_close(cd);
		return WAT_FAIL;
	}

	*outdata_len = outdata_size - len_avail;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		print_buffer(WAT_LOG_DEBUG, outdata, *outdata_len, "Contents:");
	}
	return WAT_SUCCESS;
}


wat_status_t wat_decode_sms_pdu_message_7bit(char *outdata, wat_size_t *outdata_len, wat_size_t outdata_size, wat_size_t message_len, int padding, char **indata, wat_size_t size)
{
	int i;
	int carry;
	uint8_t byte, conv_byte;
	wat_size_t data_len;
	char *data;

	carry = 0;
	data_len = 0;
	data = *indata;
	
	memset(outdata, 0, outdata_size);

	i = 0;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Decoding message from 7-bit len:%d\n", message_len);
	}

	if (padding) {
		uint8_t j = i % 7;
		
		byte = *data;
		data++;
		
		conv_byte = hi_bits(byte, 7-j);
		data_len += sprintf(&outdata[data_len], "%c", conv_byte);
		padding = 1;
	}

	for (; data_len < message_len; i++) {
		uint8_t j = (i - padding) % 7;
		byte = *data;
		data++;

		conv_byte = ((lo_bits(byte, (7-j))) << j) | carry;
		carry = hi_bits(byte, j+1);

		data_len += sprintf(&outdata[data_len], "%c", conv_byte);

		if (j == 6) {
			data_len += sprintf(&outdata[data_len], "%c", carry);
			carry = 0;
		}
	}

	outdata[data_len++] = '\0';
	*outdata_len = data_len;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "Contents:%s (len:%d)\n", outdata, *outdata_len);
	}
	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_smsc(wat_number_t *smsc, char **indata, wat_size_t size)
{
	/* www.dreamfabric.com/sms/type_of_address.html */
	char *data;
	wat_size_t len;

	data = *indata;

	len = *data;
	data++;
	
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  SMSC len:%d\n", len);
	}

	wat_decode_type_of_address((*data & 0xFF), &smsc->type, &smsc->plan);
	data++;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  SMSC type:%d plan:%d\n", smsc->type, smsc->plan);
	}

	wat_decode_sms_pdu_semi_octets(smsc->digits, data, len - 1);

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  SMSC number:%s\n", smsc->digits);
	}

	*indata = data + (len - 1);
	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_deliver(wat_sms_pdu_deliver_t *deliver, char **indata, wat_size_t size)
{
	uint8_t octet;

	//octet = hexstr_to_val(*indata);
	octet = **indata;

	deliver->tp_mti = octet & 0x03;
	deliver->tp_mms = (octet > 2) & 0x01;
	deliver->tp_sri = (octet > 4) & 0x01;
	deliver->tp_udhi = (octet > 5) & 0x01;
	deliver->tp_rp = (octet > 6) & 0x01;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  SMS-DELIVER: TP-RP:%d TP-UDHI:%d TP-SRI:%d TP-MMS:%d TP-MTI:%d\n",
				deliver->tp_rp, deliver->tp_udhi, deliver->tp_sri, deliver->tp_mms, deliver->tp_mti);
	}

	*indata = *indata + 1;
	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_from(wat_number_t *from, char **indata, wat_size_t size)
{
	/* www.dreamfabric.com/sms/type_of_address.html */
	char *data;
	wat_size_t len;

	data = *indata;

	len = *data;
	data++;
	
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  From len:%d\n", len);
	}

	wat_decode_type_of_address((*data & 0xFF), &from->type, &from->plan);
	data++;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  From type:%d plan:%d\n", from->type, from->plan);
	}

	/* For the sender, the 'length' parameter specifies how many characters are in the sender->number:
	2 characters per octet, there is a trailing F is there is an odd number of characters */
	wat_decode_sms_pdu_semi_octets(from->digits, data, (len / 2) + (len % 2));


	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  From number:%s\n", from->digits);
	}

	*indata = data + (len / 2 + (len % 2));
	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_pid(uint8_t *pid, char **indata, wat_size_t size)
{
	*pid = **indata;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  TP-PID:0x%02x\n", *pid);
	}
	*indata = *indata + 1;

	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_dcs(wat_sms_pdu_dcs_t *dcs, char **indata, wat_size_t size)
{
	/* Based on Section 4 of GSM 03.38 */
	uint8_t octet = **indata;
	uint8_t dcs_grp = octet >> 4;

	*indata = *indata + 1;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  TP-DCS:0x%02X\n", octet);
	}

	/* Based on Section 4 of GSM 03.38 */
	dcs->grp = WAT_SMS_PDU_DCS_GRP_INVALID;
	dcs->msg_class = WAT_SMS_PDU_DCS_MSG_CLASS_INVALID;
	dcs->ind_type = WAT_SMS_PDU_DCS_IND_TYPE_INVALID;
	dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_INVALID;

	if (!(dcs_grp & 0xFF00)) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_GEN;
	} else if (dcs_grp >= 0x4 && dcs_grp <= 0xB) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_RESERVED;
	} else if (dcs_grp == 0xC) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_MWI_DISCARD_MSG;
	} else if (dcs_grp == 0xD) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_1;
	} else if (dcs_grp == 0xE) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_2;
	} else if (dcs_grp == 0xF) {
		dcs->grp = WAT_SMS_PDU_DCS_GRP_DATA_CODING;
	} else {
		wat_log(WAT_LOG_ERROR, "Invalid SMS PDU coding group (val:%x)\n", dcs_grp);
	}

	if (!octet) {
		/* Special case */
		dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_DEFAULT;
		if (g_debug & WAT_DEBUG_SMS_DECODE) {
			wat_log(WAT_LOG_DEBUG, "  DCS alphabet:%s\n", wat_sms_pdu_dcs_alphabet2str(dcs->alphabet));
		}
		return WAT_SUCCESS;
	}

	switch (dcs->grp) {
		case WAT_SMS_PDU_DCS_GRP_GEN:
			dcs->compressed = bit(octet, 5);

			if (bit(octet, 4)) {
				dcs->msg_class = octet & 0x03;
			} else {
				dcs->msg_class = WAT_SMS_PDU_DCS_MSG_CLASS_INVALID;
			}

			dcs->alphabet = (octet >> 2) & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_MWI_DISCARD_MSG:
		case WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_1:
		case WAT_SMS_PDU_DCS_GRP_MWI_STORE_MSG_2:
			dcs->ind_active = (bit(octet, 3));
			dcs->ind_type = octet & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_DATA_CODING:
			if (bit(octet, 2)) {
				dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_DEFAULT;
			} else {
				dcs->alphabet = WAT_SMS_PDU_DCS_ALPHABET_8BIT;
			}

			dcs->msg_class = octet & 0x03;
			break;
		case WAT_SMS_PDU_DCS_GRP_RESERVED:
			/* Custom coding grp - cannot decode */
		case WAT_SMS_PDU_DCS_GRP_INVALID:
			break;
	}
	
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  DCS alphabet:%s\n", wat_sms_pdu_dcs_alphabet2str(dcs->alphabet));
	}
	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_scts(wat_timestamp_t *ts, char **indata, wat_size_t size)
{
	char *data = *indata;
	int i;

	for (i = 0; i <= 6; i++) {
		uint8_t val = data[i];
		uint8_t true_val = ((val & 0xF0) >> 4) + ((val & 0x0F) * 10);

		switch(i) {
			case 0:
				ts->year = true_val;
				break;
			case 1:
				ts->month = true_val;
				break;
			case 2:
				ts->day = true_val;
				break;
			case 3:
				ts->hour = true_val;
				break;
			case 4:
				ts->minute = true_val;
				break;
			case 5:
				ts->second = true_val;
				break;
			case 6:
				ts->timezone = true_val;
				break;
		}
	}

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  Year:%d Month:%d Day:%d Hr:%d Min:%d Sec:%d Timezone:%d\n",
				ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second, ts->timezone);
	}

	/* TP-SCTS always takes 7 bytes */
	*indata += 7;

	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_udl(uint8_t *udl, char **indata, wat_size_t size)
{
	*udl = **indata;
	*indata = *indata + 1;
	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		wat_log(WAT_LOG_DEBUG, "  TP-UDL:%d\n", *udl);
	}
	return WAT_SUCCESS;
}

wat_status_t wat_decode_sms_pdu_udh(wat_sms_pdu_udh_t *udh, char **indata, wat_size_t size)
{
	char *data = *indata;
	
	udh->tp_udhl = *data; /* User data header length */
	data++;

	udh->iei = *data; /* Information Element Identifier */
	data++;

	udh->iedl = *data; /* Information Element Identifier Length */
	data++;

	udh->refnr = *data; /* Reference Number */
	data++;

	udh->total = *data; /* Total Number of parts (number of concatenated sms */
	data++;

	udh->seq = *data; /* Sequence */
	data++;

	if (g_debug & WAT_DEBUG_SMS_DECODE) {
		/* User data length */
		wat_log(WAT_LOG_DEBUG, "TP-UDHL:%d IEI:%d IEDL:%d Ref nr:%d Total:%d Seq:%d\n", udh->tp_udhl, udh->iei, udh->iedl, udh->refnr, udh->total, udh->seq);
	}

	*indata += udh->tp_udhl;
	return WAT_SUCCESS;
}
