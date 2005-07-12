/***************************************************************************
 *
 * common.c : Common functions shared between modules
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include "gpm-common.h"

void
g_log_ignore (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
	/*
	 * This is a dummy function that is called only when not set verbose
	 * (and we shouldn't spew all the debug stuff)
	 */
}

/** Converts an action string representation to it's ENUM
 *
 *  @param  gconfstring		The string name
 *  @return			The action ENUM
 */
int
convert_string_to_policy (const gchar *gconfstring)
{
	g_assert (gconfstring);
	if (strcmp (gconfstring, "nothing") == 0)
		return ACTION_NOTHING;
	if (strcmp (gconfstring, "suspend") == 0)
		return ACTION_SUSPEND;
	if (strcmp (gconfstring, "shutdown") == 0)
		return ACTION_SHUTDOWN;
	if (strcmp (gconfstring, "hibernate") == 0)
		return ACTION_HIBERNATE;
	if (strcmp (gconfstring, "warning") == 0)
		return ACTION_WARNING;

	g_warning ("gconfstring '%s' not converted", gconfstring);
	return ACTION_UNKNOWN;
}

/** Converts an action ENUM to it's string representation
 *
 *  @param  powerDevice		The action ENUM
 *  @return			action string, e.g. "shutdown"
 */
gchar *
convert_policy_to_string (gint value)
{
	if (value == ACTION_NOTHING)
		return "nothing";
	else if (value == ACTION_SUSPEND)
		return "suspend";
	else if (value == ACTION_SHUTDOWN)
		return "shutdown";
	else if (value == ACTION_HIBERNATE)
		return "hibernate";
	else if (value == ACTION_WARNING)
		return "warning";
	g_warning ("value '%i' not converted", value);
	return NULL;
}

/** Converts an HAL string representation to it's ENUM
 *
 *  @param  gconfstring		The HAL battery type
 *  @return			The powerDevice ENUM
 */
gint convert_haltype_to_powerdevice (const gchar *type)
{
	g_assert (type);
	if (strcmp (type, "ac_adapter") == 0)
		return POWER_AC_ADAPTER;
	else if (strcmp (type, "ups") == 0)
		return POWER_UPS;
	else if (strcmp (type, "mouse") == 0)
		return POWER_MOUSE;
	else if (strcmp (type, "keyboard") == 0)
		return POWER_KEYBOARD;
	else if (strcmp (type, "pda") == 0)
		return POWER_PDA;
	else if (strcmp (type, "primary") == 0)
		return POWER_PRIMARY_BATTERY;
	return POWER_UNKNOWN;
}

/** Converts a powerDevice to it's GNOME icon
 *
 *  @param  powerDevice		The powerDevice ENUM
 *  @return			GNOME Icon, e.g. "gnome-dev-pda"
 */
gchar *
convert_powerdevice_to_gnomeicon (gint powerDevice)
{
	if (powerDevice == POWER_UPS)
		return "gnome-dev-ups";
	else if (powerDevice == POWER_MOUSE)
		return "gnome-dev-mouse-optical";
	else if (powerDevice == POWER_AC_ADAPTER)
		return "gnome-dev-acadapter";
	else if (powerDevice == POWER_KEYBOARD)
		return "gnome-dev-keyboard";
	else if (powerDevice == POWER_PRIMARY_BATTERY)
		return "gnome-dev-battery";
	else if (powerDevice == POWER_PDA)
		return "gnome-dev-pda";
	return NULL;
}

/** Converts a powerDevice to it's human readable form
 *
 *  @param  powerDevice		The powerDevice ENUM
 *  @return			Human string, e.g. "Laptop battery"
 */
gchar *
convert_powerdevice_to_string (gint powerDevice)
{
	if (powerDevice == POWER_UPS)
		return _("UPS");
	else if (powerDevice == POWER_AC_ADAPTER)
		return _("AC Adapter");
	else if (powerDevice == POWER_MOUSE)
		return _("Logitech mouse");
	else if (powerDevice == POWER_KEYBOARD)
		return _("Logitech keyboard");
	else if (powerDevice == POWER_PRIMARY_BATTERY)
		return _("Laptop battery");
	else if (powerDevice == POWER_PDA)
		return _("PDA");
	return _("Unknown device");
}

/** Updates .percentageCharge in a GenericObject
 *  This function is needed because of the different ways that batteries 
 *  can represent their charge.
 *
 *  @param  slotData		the GenericObject reference
 *  @param  datavalue 		Data value, either percentage or new mWh current
 */
void
update_percentage_charge (GenericObject *slotData)
{
	g_assert (slotData);
	g_assert (slotData->powerDevice != POWER_NONE);

	/* These devices cannot have charge, assume 0% */
	if (!slotData->present ||
	    slotData->powerDevice==POWER_AC_ADAPTER || 
	    slotData->powerDevice==POWER_UNKNOWN ) {
		slotData->percentageCharge = 0;
		return;
	}

	/* shouldn't happen */
	if (slotData->rawLastFull <= 0) {
		g_debug ("Error: slotData->rawLastFull = %i", slotData->rawLastFull);
		slotData->rawLastFull = 100;
	}

	/* 
	 * Work out the ACTUAL percentage charge of the battery 
	 * using cached values
	 */
	slotData->percentageCharge = ((double) slotData->rawCharge / 
					(double) slotData->rawLastFull) * 100;

	/* make sure results are sensible */
	if (slotData->percentageCharge < 0) {
		g_debug ("Error: slotData->percentageCharge = %i", slotData->percentageCharge);
		slotData->percentageCharge = 0;
	}
	if (slotData->percentageCharge > 100) {
		g_debug ("Error: slotData->percentageCharge = %i", slotData->percentageCharge);
		slotData->percentageCharge = 100;
	}
	return;
}

/** Gets the charge state string from a slot object
 *
 *  @param  slotData		the GenericObject reference
 *  @return			the charge string, e.g. "fully charged"
 */
gchar *
get_chargestate_string (GenericObject *slotData)
{
	g_assert (slotData);
	if (!slotData->present)
		return _("missing");
	else if (slotData->isCharging)
		return _("charging");
	else if (slotData->isDischarging)
		return _("discharging");
	else if (!slotData->isCharging && 
		 !slotData->isDischarging && 
		 slotData->percentageCharge > 98)
		return _("fully charged");
	else if (!slotData->isCharging &&
		 !slotData->isDischarging)
		return _("charged");
	return _("unknown");
}

/** Returns the time string, e.g. "2 hours 3 minutes"
 *
 *  @param  minutes		Minutes to convert to string
 *  @return			The timestring 
 */
GString *
get_timestring_from_minutes (gint minutes)
{
	/* g_debug ("get_timestring_from_minutes (%i)", minutes)); */
	GString* timestring;
	gint hours;

	if (minutes == 0)
		return NULL;

	timestring = g_string_new ("");
	if (minutes == 1)
		g_string_printf (timestring, _("%i minute"), minutes);
	else if (minutes < 60)
		g_string_printf (timestring, _("%i minutes"), minutes);
	else {
		hours = minutes / 60;
		minutes = minutes - (hours * 60);
		if (minutes == 0) {
			if (hours == 1)
				g_string_printf (timestring, _("%i hour"), hours);
			else
				g_string_printf (timestring, _("%i hours"), hours);
		} else {
			if (hours == 1) {
				if (minutes == 1)
					g_string_printf (timestring, 
					_("%i hour %i minute"), hours, minutes);
				else
					g_string_printf (timestring, 
					_("%i hour %i minutes"), hours, minutes);
			} else
				g_string_printf (timestring, 
					_("%i hours %i minutes"), hours, minutes);
		}
	}
	return timestring;
}
