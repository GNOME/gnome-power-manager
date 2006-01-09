/** @file	gpm-common.c
 *  @brief	Common functions shared between modules
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module contains functions that are shared between g-p-m and
 * g-p-m so that as much code can be re-used as possible.
 * There's a bit of everything in this file...
 */
/*
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/** @todo factor these out into gpm-only modules */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include "gpm-common.h"
#include "gpm-sysdev.h"

/** Converts the HAL battery.type string to a DeviceType ENUM
 *
 *  @param  type		The battery type, e.g. "primary"
 *  @return			The DeviceType
 */
DeviceType
hal_to_device_type (const gchar *type)
{
	if (strcmp (type, "ups") == 0)
		return BATT_UPS;
	else if (strcmp (type, "mouse") == 0)
		return BATT_MOUSE;
	else if (strcmp (type, "keyboard") == 0)
		return BATT_KEYBOARD;
	else if (strcmp (type, "pda") == 0)
		return BATT_PDA;
	else if (strcmp (type, "primary") == 0)
		return BATT_PRIMARY;
	g_warning ("Unknown battery type '%s'", type);
	return BATT_PRIMARY;
}

/** Returns the time string, e.g. "2 hours 3 minutes"
 *
 *  @param  minutes		Minutes to convert to string
 *  @return			The timestring
 *
 *  @note	minutes == 0 is returned as "Unknown"
 */
gchar *
get_timestring_from_minutes (gint minutes)
{
	gchar* timestring = NULL;
	gint hours;

	if (minutes == 0) {
		timestring = g_strdup_printf (_("Unknown"));
		return timestring;
	}
	if (minutes < 60) {
		timestring = g_strdup_printf (ngettext (
				"%i minute",
				"%i minutes",
				minutes), minutes);
		return timestring;
	}

	hours = minutes / 60;
	minutes = minutes % 60;

	if (minutes == 0) 
		timestring = g_strdup_printf (ngettext (
				"%i hour",
				"%i hours",
				hours), hours);
	else
		/* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
		 * Swap order with "%2$s %2$i %1$s %1$i if needed */
		timestring = g_strdup_printf (_("%i %s, %i %s"),
				hours, ngettext ("hour", "hours", hours),
				minutes, ngettext ("minute", "minutes", minutes));
	return timestring;
}
