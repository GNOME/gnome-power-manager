/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  @file	gpm-common.c
 *  @brief	Common functions shared between modules
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module contains functions that are shared between g-p-m and
 * g-p-m so that as much code can be re-used as possible.
 * There's a bit of everything in this file...
 *
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "gpm-common.h"

/** Returns the time string, e.g. "2 hours 3 minutes"
 *
 *  @param  minutes		Minutes to convert to string
 *  @return			The timestring
 *
 *  @note	minutes == 0 is returned as "Unknown"
 */
char *
get_timestring_from_minutes (int minutes)
{
	char* timestring = NULL;
	gint   hours;

	if (minutes == 0) {
		timestring = g_strdup_printf (_("Unknown time"));
		return timestring;
	}

	if (minutes < 60) {
		timestring = g_strdup_printf (ngettext ("%i minute",
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
