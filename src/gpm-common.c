/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>

#include "gpm-common.h"

/**
 * gpm_percent_to_discrete:
 * @percentage: The percentage to convert
 * @levels: The number of discrete levels
 *
 * We have to be carefull when converting from %->discrete as precision is very
 * important if we want the highest value.
 *
 * Return value: The discrete value for this percentage.
 **/
guint
gpm_percent_to_discrete (guint percentage,
			 guint levels)
{
	/* check we are in range */
	if (percentage > 100) {
		return levels;
	}
	return ((gfloat) percentage * (gfloat) (levels - 1)) / 100.0f;
}

/**
 * gpm_discrete_to_percent:
 * @hw: The discrete level
 * @levels: The number of discrete levels
 *
 * We have to be carefull when converting from discrete->%.
 *
 * Return value: The percentage for this discrete value.
 **/
guint
gpm_discrete_to_percent (guint discrete,
			 guint levels)
{
	/* check we are in range */
	if (discrete > levels) {
		return 100;
	}
	return (guint) ((float) discrete * (100.0f / (float) (levels - 1)));
}

/**
 * gpm_warning_beep
 **/
void
gpm_warning_beep (void)
{
	GdkDisplay *display = gdk_display_get_default ();
	gdk_display_beep (display);
}

/**
 * gpm_get_timestring:
 * @time_secs: The time value to convert in seconds
 * @cookie: The cookie we are looking for
 *
 * Returns a localised timestring
 *
 * Return value: The time string, e.g. "2 hours 3 minutes"
 **/
gchar *
gpm_get_timestring (guint time_secs)
{
	char* timestring = NULL;
	gint  hours;
	gint  minutes;

	/* Add 0.5 to do rounding */
	minutes = (int) ( ( time_secs / 60.0 ) + 0.5 );

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

GpmIconPolicy
gpm_tray_icon_mode_from_string (const gchar *str)
{
	if (str == NULL) {
		return GPM_ICON_POLICY_NEVER;
	}

	if (strcmp (str, "always") == 0) {
		return GPM_ICON_POLICY_ALWAYS;
	} else if (strcmp (str, "present") == 0) {
		return GPM_ICON_POLICY_PRESENT;
	} else if (strcmp (str, "charge") == 0) {
		return GPM_ICON_POLICY_CHARGE;
	} else if (strcmp (str, "critical") == 0) {
		return GPM_ICON_POLICY_CRITICAL;
	} else if (strcmp (str, "never") == 0) {
		return GPM_ICON_POLICY_NEVER;
	} else {
		return GPM_ICON_POLICY_NEVER;
	}
}

const gchar *
gpm_tray_icon_mode_to_string (GpmIconPolicy mode)
{
	if (mode == GPM_ICON_POLICY_ALWAYS) {
		return "always";
	} else if (mode == GPM_ICON_POLICY_PRESENT) {
		return "present";
	} else if (mode == GPM_ICON_POLICY_CHARGE) {
		return "charge";
	} else if (mode == GPM_ICON_POLICY_CRITICAL) {
		return "critical";
	} else if (mode == GPM_ICON_POLICY_NEVER) {
		return "never";
	} else {
		return "never";
	}
}
