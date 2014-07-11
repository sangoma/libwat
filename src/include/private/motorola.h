/*
 * libwat: Wireless AT commands library
 *
 * Jasper van der Neut - Stulen <jasper@speakup.nl>
 * Copyright (C) 2013, SpeakUp BV
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


#ifndef _MOTOROLA_H
#define _MOTOROLA_H

wat_status_t motorola_init(wat_span_t *span);

typedef enum {
	WAT_MOTOROLA_SIM_NOT_INSERTED = 0,
	WAT_MOTOROLA_SIM_INSERTED = 1,
	WAT_MOTOROLA_SIM_INSERTED_PIN_UNLOCKED = 2,
	WAT_MOTOROLA_SIM_INSERTED_READY = 3,
	WAT_MOTOROLA_SIM_INVALID = 0xFF,
} wat_motorola_sim_status_t;

#define WAT_MOTOROLA_SIM_STATUS_STRINGS "Not-Inserted", "Inserted", "Inserted-PIN-Unlocked", "Inserted-Ready"

#endif /* _MOTOROLA_H */
