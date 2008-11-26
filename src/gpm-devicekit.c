/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "egg-debug.h"
#include "egg-precision.h"

#include "dkp-enum.h"
#include "dkp-object.h"

#include "gpm-devicekit.h"
#include "gpm-common.h"

/**
 * gpm_devicekit_get_object_icon_index:
 * @percent: The charge of the device
 *
 * The index value depends on the percentage charge:
 *	00-10  = 000
 *	10-30  = 020
 *	30-50  = 040
 *	50-70  = 060
 *	70-90  = 080
 *	90-100 = 100
 *
 * Return value: The character string for the filename suffix.
 **/
static const gchar *
gpm_devicekit_get_object_icon_index (const DkpObject *obj)
{
	if (obj->percentage < 10)
		return "000";
	else if (obj->percentage < 30)
		return "020";
	else if (obj->percentage < 50)
		return "040";
	else if (obj->percentage < 70)
		return "060";
	else if (obj->percentage < 90)
		return "080";
	return "100";
}

/**
 * gpm_devicekit_get_object_icon:
 *
 * Need to free the return value
 *
 **/
gchar *
gpm_devicekit_get_object_icon (const DkpObject *obj)
{
	gchar *filename = NULL;
	const gchar *prefix = NULL;
	const gchar *index_str;

	g_return_val_if_fail (obj != NULL, NULL);

	/* get correct icon prefix */
	prefix = dkp_device_type_to_text (obj->type);

	/* TODO: rename icons when we remove HAL backend */
	if (obj->type == DKP_DEVICE_TYPE_BATTERY)
		prefix = "primary";

	/* get the icon from some simple rules */
	if (obj->type == DKP_DEVICE_TYPE_LINE_POWER) {
		filename = g_strdup ("gpm-ac-adapter");
	} else if (obj->type == DKP_DEVICE_TYPE_MONITOR) {
		filename = g_strdup ("gpm-monitor");
	} else if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
		   obj->type == DKP_DEVICE_TYPE_UPS) {
		if (!obj->is_present) {
			/* battery missing */
			filename = g_strdup_printf ("gpm-%s-missing", prefix);

		} else if (obj->state == DKP_DEVICE_STATE_FULLY_CHARGED) {
			filename = g_strdup_printf ("gpm-%s-charged", prefix);

		} else if (obj->state == DKP_DEVICE_STATE_CHARGING) {
			index_str = gpm_devicekit_get_object_icon_index (obj);
			filename = g_strdup_printf ("gpm-%s-%s-charging", prefix, index_str);

		} else {
			index_str = gpm_devicekit_get_object_icon_index (obj);
			filename = g_strdup_printf ("gpm-%s-%s", prefix, index_str);
		}
	} else if (obj->type == DKP_DEVICE_TYPE_MOUSE ||
		   obj->type == DKP_DEVICE_TYPE_KEYBOARD ||
		   obj->type == DKP_DEVICE_TYPE_PHONE) {
		if (obj->percentage < 26)
			index_str = "000";
		else if (obj->percentage < 51)
			index_str = "030";
		else if (obj->percentage < 60)
			index_str = "060";
		else
			index_str = "100";
		filename = g_strdup_printf ("gpm-%s-%s", prefix, index_str);
	}
	egg_debug ("got filename: %s", filename);
	return filename;
}


#define GPM_DKP_TIME_PRECISION			5*60
#define GPM_DKP_TEXT_MIN_TIME			120

/**
 * gpm_devicekit_get_object_summary:
 **/
gchar *
gpm_devicekit_get_object_summary (const DkpObject *obj)
{
	const gchar *type_desc = NULL;
	gchar *description = NULL;
	guint time_to_full_round;
	guint time_to_empty_round;
	gchar *time_to_full_str;
	gchar *time_to_empty_str;

	if (!obj->is_present)
		return NULL;

	type_desc = gpm_device_type_to_localised_text (obj->type, 1);

	/* don't display all the extra stuff for keyboards and mice */
	if (obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD ||
	    obj->type == DKP_DEVICE_TYPE_PDA)
		return g_strdup_printf ("%s (%.1f%%)", type_desc, obj->percentage);

	/* we care if we are on AC */
	if (obj->type == DKP_DEVICE_TYPE_PHONE) {
		if (obj->state == DKP_DEVICE_STATE_CHARGING || !obj->state == DKP_DEVICE_STATE_DISCHARGING)
			return g_strdup_printf ("%s charging (%.1f%%)", type_desc, obj->percentage);
		return g_strdup_printf ("%s (%.1f%%)", type_desc, obj->percentage);
	}

	/* precalculate so we don't get Unknown time remaining */
	time_to_full_round = egg_precision_round_down (obj->time_to_full, GPM_DKP_TIME_PRECISION);
	time_to_empty_round = egg_precision_round_down (obj->time_to_empty, GPM_DKP_TIME_PRECISION);

	/* we always display "Laptop battery 16 minutes remaining" as we need to clarify what device we are refering to */
	if (obj->state == DKP_DEVICE_STATE_FULLY_CHARGED) {

		if (obj->type == DKP_DEVICE_TYPE_BATTERY && time_to_empty_round > GPM_DKP_TEXT_MIN_TIME) {
			time_to_empty_str = gpm_get_timestring (time_to_empty_round);
			description = g_strdup_printf (_("%s fully charged (%.1f%%)\nProvides %s battery runtime"),
							type_desc, obj->percentage, time_to_empty_str);
			g_free (time_to_empty_str);
		} else {
			description = g_strdup_printf (_("%s fully charged (%.1f%%)"),
							type_desc, obj->percentage);
		}

	} else if (obj->state == DKP_DEVICE_STATE_DISCHARGING) {

		if (time_to_empty_round > GPM_DKP_TEXT_MIN_TIME) {
			time_to_empty_str = gpm_get_timestring (time_to_empty_round);
			description = g_strdup_printf (_("%s %s remaining (%.1f%%)"),
							type_desc, time_to_empty_str, obj->percentage);
			g_free (time_to_empty_str);
		} else {
			/* don't display "Unknown remaining" */
			description = g_strdup_printf (_("%s discharging (%.1f%%)"),
							type_desc, obj->percentage);
		}

	} else if (obj->state == DKP_DEVICE_STATE_CHARGING) {

		if (time_to_full_round > GPM_DKP_TEXT_MIN_TIME &&
		    time_to_empty_round > GPM_DKP_TEXT_MIN_TIME) {

			/* display both discharge and charge time */
			time_to_full_str = gpm_get_timestring (time_to_full_round);
			time_to_empty_str = gpm_get_timestring (time_to_empty_round);
			description = g_strdup_printf (_("%s %s until charged (%.1f%%)\nProvides %s battery runtime"),
							type_desc, time_to_full_str, obj->percentage, time_to_empty_str);
			g_free (time_to_full_str);
			g_free (time_to_empty_str);

		} else if (time_to_full_round > GPM_DKP_TEXT_MIN_TIME) {

			/* display only charge time */
			time_to_full_str = gpm_get_timestring (time_to_full_round);
			description = g_strdup_printf (_("%s %s until charged (%.1f%%)"),
						type_desc, time_to_full_str, obj->percentage);
			g_free (time_to_full_str);
		} else {

			/* don't display "Unknown remaining" */
			description = g_strdup_printf (_("%s charging (%.1f%%)"),
						type_desc, obj->percentage);
		}

	} else {
		egg_warning ("in an undefined state we are not charging or "
			     "discharging and the batteries are also not charged");
		description = g_strdup (_("Device state could not be read at this time"));
	}
	return description;
}

/**
 * gpm_devicekit_get_object_description:
 **/
gchar *
gpm_devicekit_get_object_description (const DkpObject *obj)
{
	GString	*details;
	const gchar *text;
	gchar *time_str;

	g_return_val_if_fail (obj != NULL, NULL);

	details = g_string_new ("");
	text = gpm_device_type_to_localised_text (obj->type, 1);
	g_string_append_printf (details, _("<b>Product:</b> %s\n"), text);
	if (!obj->is_present)
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Missing"));
	else if (obj->state == DKP_DEVICE_STATE_FULLY_CHARGED)
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Charged"));
	else if (obj->state == DKP_DEVICE_STATE_CHARGING)
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Charging"));
	else if (obj->state == DKP_DEVICE_STATE_DISCHARGING)
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Discharging"));
	if (obj->percentage >= 0)
		g_string_append_printf (details, _("<b>Percentage charge:</b> %.1f%%\n"), obj->percentage);
	if (obj->vendor)
		g_string_append_printf (details, _("<b>Vendor:</b> %s\n"), obj->vendor);
	if (obj->technology != DKP_DEVICE_TECHNOLGY_UNKNOWN) {
		text = gpm_device_technology_to_localised_text (obj->technology);
		g_string_append_printf (details, _("<b>Technology:</b> %s\n"), text);
	}
	if (obj->serial)
		g_string_append_printf (details, _("<b>Serial number:</b> %s\n"), obj->serial);
	if (obj->model)
		g_string_append_printf (details, _("<b>Model:</b> %s\n"), obj->model);
	if (obj->time_to_full > 0) {
		time_str = gpm_get_timestring (obj->time_to_full);
		g_string_append_printf (details, _("<b>Charge time:</b> %s\n"), time_str);
		g_free (time_str);
	}
	if (obj->time_to_empty > 0) {
		time_str = gpm_get_timestring (obj->time_to_empty);
		g_string_append_printf (details, _("<b>Discharge time:</b> %s\n"), time_str);
		g_free (time_str);
	}
	if (obj->capacity > 0) {
		const gchar *condition;
		if (obj->capacity > 99) {
			/* Translators: Excellent, Good, Fair and Poor are all related to battery Capacity */
			condition = _("Excellent");
		} else if (obj->capacity > 90) {
			condition = _("Good");
		} else if (obj->capacity > 70) {
			condition = _("Fair");
		} else {
			condition = _("Poor");
		}
		/* Translators: %i is a percentage and %s the condition (Excellent, Good, ...) */
		g_string_append_printf (details, _("<b>Capacity:</b> %.1f%% (%s)\n"),
					obj->capacity, condition);
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		if (obj->energy > 0)
			g_string_append_printf (details, _("<b>Current charge:</b> %.1f Wh\n"),
						obj->energy / 1000.0f);
		if (obj->energy_full > 0 &&
		    obj->energy_full_design != obj->energy_full)
			g_string_append_printf (details, _("<b>Last full charge:</b> %.1f Wh\n"),
						obj->energy_full / 1000.0f);
		if (obj->energy_full_design > 0)
			g_string_append_printf (details, _("<b>Design charge:</b> %.1f Wh\n"),
						obj->energy_full_design / 1000.0f);
		if (obj->energy_rate > 0)
			g_string_append_printf (details, _("<b>Charge rate:</b> %.1f W\n"),
						obj->energy_rate / 1000.0f);
	}
	if (obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD) {
		if (obj->energy > 0)
			g_string_append_printf (details, _("<b>Current charge:</b> %.0f/7\n"),
						obj->energy);
		if (obj->energy_full_design > 0)
			g_string_append_printf (details, _("<b>Design charge:</b> %.0f/7\n"),
						obj->energy_full_design);
	}
	/* remove the last \n */
	g_string_truncate (details, details->len-1);

	return g_string_free (details, FALSE);
}

/**
 * gpm_device_type_to_localised_text:
 **/
const gchar *
gpm_device_type_to_localised_text (DkpDeviceType type, guint number)
{
	const gchar *text = NULL;
	switch (type) {
	case DKP_DEVICE_TYPE_LINE_POWER:
		text = ngettext ("AC adapter", "AC adapters", number);
		break;
	case DKP_DEVICE_TYPE_BATTERY:
		text = ngettext ("Laptop battery", "Laptop batteries", number);
		break;
	case DKP_DEVICE_TYPE_UPS:
		text = ngettext ("UPS", "UPSs", number);
		break;
	case DKP_DEVICE_TYPE_MONITOR:
		text = ngettext ("Monitor", "Monitors", number);
		break;
	case DKP_DEVICE_TYPE_MOUSE:
		text = ngettext ("Wireless mouse", "Wireless mice", number);
		break;
	case DKP_DEVICE_TYPE_KEYBOARD:
		text = ngettext ("Wireless keyboard", "Wireless keyboards", number);
		break;
	case DKP_DEVICE_TYPE_PDA:
		text = ngettext ("PDA", "PDAs", number);
		break;
	case DKP_DEVICE_TYPE_PHONE:
		text = ngettext ("Cell phone", "Cell phones", number);
		break;
	default:
		egg_warning ("enum unrecognised: %i", type);
		text = dkp_device_type_to_text (type);
	}
	return text;
}

/**
 * gpm_device_type_to_icon:
 **/
const gchar *
gpm_device_type_to_icon (DkpDeviceType type)
{
	const gchar *icon = NULL;
	switch (type) {
	case DKP_DEVICE_TYPE_LINE_POWER:
		icon = "gpm-ac-adapter";
		break;
	case DKP_DEVICE_TYPE_BATTERY:
		icon = "battery";
		break;
	case DKP_DEVICE_TYPE_UPS:
		icon = "network-wired";
		break;
	case DKP_DEVICE_TYPE_MONITOR:
		icon = "application-certificate";
		break;
	case DKP_DEVICE_TYPE_MOUSE:
		icon = "mouse";
		break;
	case DKP_DEVICE_TYPE_KEYBOARD:
		icon = "input-keyboard";
		break;
	case DKP_DEVICE_TYPE_PDA:
		icon = "input-gaming";
		break;
	case DKP_DEVICE_TYPE_PHONE:
		icon = "camera-video";
		break;
	default:
		egg_warning ("enum unrecognised: %i", type);
		icon = "gtk-help";
	}
	return icon;
}

/**
 * gpm_device_technology_to_localised_text:
 **/
const gchar *
gpm_device_technology_to_localised_text (DkpDeviceTechnology technology_enum)
{
	const gchar *technology = NULL;
	switch (technology_enum) {
	case DKP_DEVICE_TECHNOLGY_LITHIUM_ION:
		technology = _("Lithium Ion");
		break;
	case DKP_DEVICE_TECHNOLGY_LITHIUM_POLYMER:
		technology = _("Lithium Polymer");
		break;
	case DKP_DEVICE_TECHNOLGY_LITHIUM_IRON_PHOSPHATE:
		technology = _("Lithium Iron Phosphate");
		break;
	case DKP_DEVICE_TECHNOLGY_LEAD_ACID:
		technology = _("Lead acid");
		break;
	case DKP_DEVICE_TECHNOLGY_NICKEL_CADMIUM:
		technology = _("Nickel Cadmium");
		break;
	case DKP_DEVICE_TECHNOLGY_NICKEL_METAL_HYDRIDE:
		technology = _("Nickel metal hydride");
		break;
	case DKP_DEVICE_TECHNOLGY_UNKNOWN:
		technology = _("Unknown technology");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return technology;
}

