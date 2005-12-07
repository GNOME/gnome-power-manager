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
#include "compiler.h"


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

/** Gets the position to "point" to (i.e. center of the icon)
 *
 *  @param	widget		the GtkWidget
 *  @param	x		X co-ordinate return
 *  @param	y		Y co-ordinate return
 *  @return			Success, return FALSE when no icon present
 */
gboolean
get_widget_position (GtkWidget *widget, gint *x, gint *y)
{
	GdkPixbuf* pixbuf = NULL;

	/* assertion checks */
	g_assert (widget);
	g_assert (x);
	g_assert (y);

	gdk_window_get_origin (GDK_WINDOW (widget->window), x, y);
	pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (widget));
	*x += (gdk_pixbuf_get_width (pixbuf) / 2);
	*y += gdk_pixbuf_get_height (pixbuf);
	g_debug ("widget position x=%i, y=%i", *x, *y);
	return TRUE;
}

/** Runs a tool in BINDIR
 *
 *  @param	program		The program name
 *  @return			Success
 *
 *  @note	This will append a prefix of BINDIR to the path
 *		It is mainly used to run g-p-p and g-p-i
 */
gboolean
run_bin_program (const gchar *program)
{
	gchar *path = NULL;
	gboolean ret = TRUE;

	/* assertion checks */
	g_assert (program);

	path = g_strconcat (BINDIR, "/", program, NULL);
	if (!g_spawn_command_line_async (path, NULL)) {
		g_warning ("Couldn't execute command: %s", path);
		ret = FALSE;
	}
	g_free (path);
	return ret;
}

/** Runs a file set in GConf
 *
 *  @param	path		The gconf path
 *  @return			Success
 */
gboolean
run_gconf_script (const char *path)
{
	GConfClient *client = NULL;
	gchar *command = NULL;
	gboolean ret = FALSE;

	/* assertion checks */
	g_assert (path);

	client = gconf_client_get_default ();
	command = gconf_client_get_string (client, path, NULL);
	if (!command) {
		g_warning ("'%s' is missing!", path);
		return FALSE;
	}
	g_debug ("Executing '%s'", command);
	ret = g_spawn_command_line_async (command, NULL);
	if (!ret)
		g_warning ("Couldn't execute '%s'.", command);
	g_free (command);
	return ret;
}

/** This is a dummy function that is called only when not set verbose
 *  and we shouldn't spew all the debug stuff
 *
 *  @param  log_domain		Unused
 *  @param  log_level		Unused
 *  @param  message		Unused
 *  @param  user_data		Unused
 *
 *  @todo			We need to have generic logging
 */
void
g_log_ignore (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
}

/** Converts an action string representation to it's ENUM
 *
 *  @param  gconfstring		The string name
 *  @return			The action ENUM
 */
ActionType
convert_string_to_policy (const gchar *gconfstring)
{
	/* assertion checks */
	g_assert (gconfstring);

	if (strcmp (gconfstring, "nothing") == 0)
		return ACTION_NOTHING;
	if (strcmp (gconfstring, "suspend") == 0)
		return ACTION_SUSPEND;
	if (strcmp (gconfstring, "shutdown") == 0)
		return ACTION_SHUTDOWN;
	if (strcmp (gconfstring, "hibernate") == 0)
		return ACTION_HIBERNATE;

	g_warning ("gconfstring '%s' not converted", gconfstring);
	return ACTION_UNKNOWN;
}

/** Converts an action ENUM to it's string representation
 *
 *  @param  value		The action ENUM
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
	g_warning ("value '%i' not converted", value);
	return NULL;
}

/** Converts an incon policy string representation to it's ENUM
 *
 *  @param  gconfstring		The string name
 *  @return			The action ENUM
 */
IconPolicy
convert_string_to_iconpolicy (const gchar *gconfstring)
{
	/* assertion checks */
	g_assert (gconfstring);

	if (strcmp (gconfstring, "never") == 0)
		return ICON_NEVER;
	else if (strcmp (gconfstring, "critical") == 0)
		return ICON_CRITICAL;
	else if (strcmp (gconfstring, "charge") == 0)
		return ICON_CHARGE;
	else if (strcmp (gconfstring, "always") == 0)
		return ICON_ALWAYS;
	g_warning ("gpn_icon_update: unknown icon gconfstring %s", gconfstring);
	return ICON_ALWAYS;
}


/** Converts an HAL string representation to it's ENUM
 *
 *  @param  type		The HAL battery type
 *  @return			The DeviceType ENUM
 */
DeviceType
convert_haltype_to_batttype (const gchar *type)
{
	/* assertion checks */
	g_assert (type);

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
	g_warning ("convert_haltype_to_batttype got unknown type '%s'", type);
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
