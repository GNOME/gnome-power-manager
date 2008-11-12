/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <davidz@redhat.com>
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib.h>
#include <string.h>
#include "egg-debug.h"
#include "egg-string.h"

#include "dkp-enum.h"

/**
 * dkp_device_type_to_text:
 **/
const gchar *
dkp_device_type_to_text (DkpDeviceType type_enum)
{
	const gchar *type = NULL;
	switch (type_enum) {
	case DKP_DEVICE_TYPE_LINE_POWER:
		type = "line-power";
		break;
	case DKP_DEVICE_TYPE_BATTERY:
		type = "battery";
		break;
	case DKP_DEVICE_TYPE_UPS:
		type = "ups";
		break;
	case DKP_DEVICE_TYPE_MONITOR:
		type = "monitor";
		break;
	case DKP_DEVICE_TYPE_MOUSE:
		type = "mouse";
		break;
	case DKP_DEVICE_TYPE_KEYBOARD:
		type = "keyboard";
		break;
	case DKP_DEVICE_TYPE_PDA:
		type = "pda";
		break;
	case DKP_DEVICE_TYPE_PHONE:
		type = "phone";
		break;
	case DKP_DEVICE_TYPE_UNKNOWN:
		type = "unknown";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return type;
}

/**
 * dkp_device_type_from_text:
 **/
DkpDeviceType
dkp_device_type_from_text (const gchar *type)
{
	if (type == NULL)
		return DKP_DEVICE_TYPE_UNKNOWN;
	if (egg_strequal (type, "line-power"))
		return DKP_DEVICE_TYPE_LINE_POWER;
	if (egg_strequal (type, "battery"))
		return DKP_DEVICE_TYPE_BATTERY;
	if (egg_strequal (type, "ups"))
		return DKP_DEVICE_TYPE_UPS;
	if (egg_strequal (type, "monitor"))
		return DKP_DEVICE_TYPE_MONITOR;
	if (egg_strequal (type, "mouse"))
		return DKP_DEVICE_TYPE_MOUSE;
	if (egg_strequal (type, "keyboard"))
		return DKP_DEVICE_TYPE_KEYBOARD;
	if (egg_strequal (type, "pda"))
		return DKP_DEVICE_TYPE_PDA;
	if (egg_strequal (type, "phone"))
		return DKP_DEVICE_TYPE_PHONE;
	return DKP_DEVICE_TYPE_UNKNOWN;
}

/**
 * dkp_device_state_to_text:
 **/
const gchar *
dkp_device_state_to_text (DkpDeviceState state_enum)
{
	const gchar *state = NULL;
	switch (state_enum) {
	case DKP_DEVICE_STATE_CHARGING:
		state = "charging";
		break;
	case DKP_DEVICE_STATE_DISCHARGING:
		state = "discharging";
		break;
	case DKP_DEVICE_STATE_EMPTY:
		state = "empty";
		break;
	case DKP_DEVICE_STATE_FULLY_CHARGED:
		state = "fully-charged";
		break;
	case DKP_DEVICE_STATE_UNKNOWN:
		state = "unknown";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return state;
}

/**
 * dkp_device_state_from_text:
 **/
DkpDeviceState
dkp_device_state_from_text (const gchar *state)
{
	if (state == NULL)
		return DKP_DEVICE_STATE_UNKNOWN;
	if (egg_strequal (state, "charging"))
		return DKP_DEVICE_STATE_CHARGING;
	if (egg_strequal (state, "discharging"))
		return DKP_DEVICE_STATE_DISCHARGING;
	if (egg_strequal (state, "empty"))
		return DKP_DEVICE_STATE_EMPTY;
	if (egg_strequal (state, "fully-charged"))
		return DKP_DEVICE_STATE_FULLY_CHARGED;
	return DKP_DEVICE_STATE_UNKNOWN;
}

/**
 * dkp_device_technology_to_text:
 **/
const gchar *
dkp_device_technology_to_text (DkpDeviceTechnology technology_enum)
{
	const gchar *technology = NULL;
	switch (technology_enum) {
	case DKP_DEVICE_TECHNOLGY_LITHIUM_ION:
		technology = "lithium-ion";
		break;
	case DKP_DEVICE_TECHNOLGY_LITHIUM_POLYMER:
		technology = "lithium-polymer";
		break;
	case DKP_DEVICE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE:
		technology = "lithium-iron-phosphate";
		break;
	case DKP_DEVICE_TECHNOLGY_LEAD_ACID:
		technology = "lead-acid";
		break;
	case DKP_DEVICE_TECHNOLGY_NICKEL_CADMIUM:
		technology = "nickel-cadmium";
		break;
	case DKP_DEVICE_TECHNOLGY_NICKEL_METAL_HYDRIDE:
		technology = "nickel-metal-hydride";
		break;
	case DKP_DEVICE_TECHNOLGY_UNKNOWN:
		technology = "unknown";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return technology;
}

/**
 * dkp_device_technology_from_text:
 **/
DkpDeviceTechnology
dkp_device_technology_from_text (const gchar *technology)
{
	if (technology == NULL)
		return DKP_DEVICE_TECHNOLGY_UNKNOWN;
	if (egg_strequal (technology, "lithium-ion"))
		return DKP_DEVICE_TECHNOLGY_LITHIUM_ION;
	if (egg_strequal (technology, "lithium-polymer"))
		return DKP_DEVICE_TECHNOLGY_LITHIUM_POLYMER;
	if (egg_strequal (technology, "lithium-iron-phosphate"))
		return DKP_DEVICE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE;
	if (egg_strequal (technology, "lead-acid"))
		return DKP_DEVICE_TECHNOLGY_LEAD_ACID;
	if (egg_strequal (technology, "nickel-cadmium"))
		return DKP_DEVICE_TECHNOLGY_NICKEL_CADMIUM;
	if (egg_strequal (technology, "nickel-metal-hydride"))
		return DKP_DEVICE_TECHNOLGY_NICKEL_METAL_HYDRIDE;
	return DKP_DEVICE_TECHNOLGY_UNKNOWN;
}

