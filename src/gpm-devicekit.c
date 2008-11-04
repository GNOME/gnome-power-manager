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
gpm_devicekit_get_object_icon_index (DkpObject *obj)
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
gpm_devicekit_get_object_icon (DkpObject *obj)
{
	gchar *filename = NULL;
	const gchar *prefix = NULL;
	const gchar *index_str;

	g_return_val_if_fail (obj != NULL, NULL);

	/* get correct icon prefix */
	prefix = dkp_device_type_to_text (obj->type);

	/* get the icon from some simple rules */
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
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

/**
 * gpm_devicekit_get_object_description:
 **/
gchar *
gpm_devicekit_get_object_description (DkpObject *obj)
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
		icon = "computer";
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

