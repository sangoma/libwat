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

#ifndef _WAT_SMS_PDU_H
#define _WAT_SMS_PDU_H

#include "wat_internal.h"

void print_buffer(wat_loglevel_t loglevel, char *data, wat_size_t data_len, char *message);


/* ENCODING FUNCTIONS */
wat_status_t wat_encode_sms_pdu_message_7bit(wchar_t *indata, wat_size_t indata_size, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size, uint8_t padding);

wat_status_t wat_encode_sms_pdu_message_ucs2(char *indata, wat_size_t indata_size, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);

wat_status_t wat_encode_sms_pdu_smsc(wat_number_t *smsc, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);
wat_status_t wat_encode_sms_pdu_to(wat_number_t *to, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);
wat_status_t wat_encode_sms_pdu_submit(wat_sms_pdu_submit_t *submit, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);
wat_status_t wat_encode_sms_pdu_message_ref(uint8_t refnr, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);
wat_status_t wat_encode_sms_pdu_pid(uint8_t pid, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);
wat_status_t wat_encode_sms_pdu_dcs(wat_sms_pdu_dcs_t *dcs, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);
wat_status_t wat_encode_sms_pdu_vp(wat_sms_pdu_vp_t *vp, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);
wat_status_t wat_encode_sms_pdu_udh(wat_sms_pdu_udh_t *udh, char **outdata, wat_size_t *outdata_len, wat_size_t outdata_size);

wat_status_t wat_verify_default_alphabet(char *content_data);
#if 0
wat_status_t wat_verify_ascii(uint8_t *data, wat_size_t len);
#endif
wat_status_t wat_convert_ascii(char *raw_content, wat_size_t *raw_content_len);

/* DECODING FUNCTIONS */
wat_status_t wat_decode_sms_pdu_deliver(wat_sms_pdu_deliver_t *deliver, char **data, wat_size_t max_len);

wat_status_t wat_decode_sms_pdu_smsc(wat_number_t *smsc, char **data, wat_size_t size);
wat_status_t wat_decode_sms_pdu_from(wat_number_t *from, char **data, wat_size_t size);
wat_status_t wat_decode_sms_pdu_dcs(wat_sms_pdu_dcs_t *dcs, char **data, wat_size_t size);
wat_status_t wat_decode_sms_pdu_pid(uint8_t *pid, char **indata, wat_size_t size);
wat_status_t wat_decode_sms_pdu_scts(wat_timestamp_t *ts, char **data, wat_size_t size);
wat_status_t wat_decode_sms_pdu_udl(uint8_t *udl, char **indata, wat_size_t size);
wat_status_t wat_decode_sms_pdu_udh(wat_sms_pdu_udh_t *udh, char **indata, wat_size_t size);
wat_status_t wat_decode_sms_pdu_message_7bit(char *outdata, wat_size_t *outdata_len, wat_size_t outdata_size, wat_size_t message_len, int padding, char **data, wat_size_t size);
wat_status_t wat_decode_sms_pdu_message_ucs2(char *outdata, wat_size_t *outdata_len, wat_size_t outdata_size, wat_size_t message_len, char **indata, wat_size_t size);
#endif /* _WAT_SMS_PDU_H */
