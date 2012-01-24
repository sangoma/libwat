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


#ifndef _TELIT_H
#define _TELIT_H

wat_status_t telit_init(wat_span_t *span);
WAT_NOTIFY_FUNC(wat_notify_codec_info);

typedef enum {
	WAT_TELIT_SIM_NOT_INSERTED = 0,
	WAT_TELIT_SIM_INSERTED = 1,
	WAT_TELIT_SIM_INSERTED_PIN_UNLOCKED = 2,
	WAT_TELIT_SIM_INSERTED_READY = 3,
	WAT_TELIT_SIM_INVALID = 0xFF, 
} wat_telit_sim_status_t;

#define WAT_TELIT_SIM_STATUS_STRINGS "Not-Inserted", "Inserted", "Inserted-PIN-Unlocked", "Inserted-Ready"

WAT_STR2ENUM_P(wat_str2wat_telit_sim_status, wat_telit_sim_status2str, wat_telit_sim_status_t);

#endif /* _TELIT_H */
