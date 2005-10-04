/*! @file	gpm-common.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include "gpm-common.h"

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

/** Finds a device from the objectData table
 *
 *  @param	parray		pointer array to GenericObject
 *  @param	udi		HAL UDI
 */
gint
find_udi_parray_index (GPtrArray *parray, const gchar *udi)
{
	GenericObject *slotData = NULL;
	gint a;

	/* assertion checks */
	g_assert (parray);
	g_assert (udi);

	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		g_return_val_if_fail (slotData, -1);
		if (strcmp (slotData->udi, udi) == 0)
			return a;
	}
	return -1;
}

/** Finds a device from the objectData table
 *
 *  @param	parray		pointer array to GenericObject
 *  @param	udi		HAL UDI
 */
GenericObject *
genericobject_find (GPtrArray *parray, const gchar *udi)
{
	gint a;

	/* assertion checks */
	g_assert (parray);
	g_assert (udi);

	a = find_udi_parray_index (parray, udi);
	if (a != -1)
		return (GenericObject *) g_ptr_array_index (parray, a);
	return NULL;
}

/** Adds a device to the objectData table *IF DOES NOT EXIST*
 *
 *  @param	parray		pointer array to GenericObject
 *  @param	udi		HAL UDI
 *  @return			TRUE if we added to the table
 */
GenericObject *
genericobject_add (GPtrArray *parray, const gchar *udi)
{
	GenericObject *slotData = NULL;
	gint a;

	/* assertion checks */
	g_assert (parray);
	g_assert (udi);

	a = find_udi_parray_index (parray, udi);
	if (a != -1)
		return NULL;

	slotData = g_new (GenericObject, 1);
	strcpy (slotData->udi, udi);
	slotData->powerDevice = POWER_UNKNOWN;
	g_ptr_array_add (parray, (gpointer) slotData);
	return slotData;
}

/** Runs a tool in BINDIR
 *
 *  @param	path		The program name
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
	if (command) {
		g_debug ("Executing '%s'", command);
		ret = g_spawn_command_line_async (command, NULL);
		if (!ret)
			g_warning ("Couldn't execute '%s'.", command);
		g_free (command);
	} else
		g_warning ("'%s' is missing!", path);
	return ret;
}

/** Converts an dbus ENUM to it's string representation
 *
 *  @param	value		The dbus ENUM
 *  @return			action string, e.g. "Shutdown"
 */
gchar *
convert_dbus_enum_to_string (gint value)
{
	if (value == GPM_DBUS_SCREENSAVE)
		return _("screensave");
	else if (value == GPM_DBUS_SHUTDOWN)
		return _("shutdown");
	else if (value == GPM_DBUS_SUSPEND)
		return _("software suspend");
	else if (value == GPM_DBUS_HIBERNATE)
		return _("hibernation");
	else if (value == GPM_DBUS_LOGOFF)
		return _("session log off");
	g_warning ("value '%i' not converted", value);
	return NULL;
}

/** Converts an dbus ENUMs to it's text representation
 * (only really useful for debugging)
 *
 *  @param	value		The dbus ENUM's
 *  @return			action string, e.g. "{GPM_DBUS_SCREENSAVE|GPM_DBUS_LOGOFF}"
 */
GString *
convert_gpmdbus_to_string (gint value)
{
	GString *retval = g_string_new ("{");
	if (value & GPM_DBUS_SCREENSAVE)
		g_string_append (retval, "GPM_DBUS_SCREENSAVE|");
	if (value & GPM_DBUS_SHUTDOWN)
		g_string_append (retval, "GPM_DBUS_SHUTDOWN|");
	if (value & GPM_DBUS_SUSPEND)
		g_string_append (retval, "GPM_DBUS_SUSPEND|");
	if (value & GPM_DBUS_HIBERNATE)
		g_string_append (retval, "GPM_DBUS_HIBERNATE|");
	if (value & GPM_DBUS_LOGOFF)
		g_string_append (retval, "GPM_DBUS_LOGOFF|");

	/* replace the final | with } */
	retval->str[retval->len-1] = '}';
	return retval;
}

/** Gets the timestring from a slot object
 *
 *  @param	slotData	the GenericObject reference
 *  @return			the timestring, e.g. "13 minutes until charged"
 */
GString *
get_time_string (GenericObject *slotData)
{
	GString* timestring = NULL;

	/* assertion checks */
	g_assert (slotData);

	timestring = get_timestring_from_minutes (slotData->minutesRemaining);
	if (!timestring)
		return NULL;
	if (slotData->isCharging)
		timestring = g_string_append (timestring, _(" until charged"));
	else
		timestring = g_string_append (timestring, _(" remaining"));

	return timestring;
}

/** Returns a virtual device that takes into account having more than one device
 *  that needs to be averaged.
 *
 *  @note	Currently we are calculating percentageCharge and minutesRemaining only.
 *
 *  @param	objectData	the device database
 *  @param	slotDataReturn	the object returned. Must not be NULL
 *  @param	powerDevice	the object to be returned. Usually POWER_PRIMARY_BATTERY
 */
void
create_virtual_of_type (GPtrArray *objectData, GenericObject *slotDataReturn, gint powerDevice)
{
	GenericObject *slotData = NULL;
	gint a;
	gint objectCount = 0;
	gint percentageCharge;
	gint minutesRemaining;

	GenericObject *slotDataTemp[5]; /* not going to get more than 5 objects */

	/* assertion checks */
	g_assert (slotDataReturn);

	for (a=0; a < objectData->len; a++) {
		slotData = (GenericObject *) g_ptr_array_index (objectData, a);
		if (slotData->powerDevice == powerDevice && slotData->present) {
			slotDataTemp[objectCount] = slotData;
			objectCount++;
		}
	}
	/* no objects */
	if (objectCount == 0) {
		g_warning ("create_virtual_of_type couldn't find device type %i", powerDevice);
		slotDataReturn = NULL;
		return;
	}

	/* short cut */
	if (objectCount == 1) {
		slotDataReturn->percentageCharge = slotDataTemp[0]->percentageCharge;
		slotDataReturn->minutesRemaining = slotDataTemp[0]->minutesRemaining;
		return;
	}

	/* work out average */
	percentageCharge = 0;
	minutesRemaining = 0;
	for (a=0;a<objectCount;a++) {
		percentageCharge += slotDataTemp[a]->percentageCharge;
		minutesRemaining += slotDataTemp[a]->minutesRemaining;
	}
	slotDataReturn->percentageCharge = percentageCharge / objectCount;
	slotDataReturn->minutesRemaining = minutesRemaining / objectCount;
}

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
	if (strcmp (gconfstring, "warning") == 0)
		return ACTION_WARNING;

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
	else if (value == ACTION_WARNING)
		return "warning";
	g_warning ("value '%i' not converted", value);
	return NULL;
}

/** Converts an HAL string representation to it's ENUM
 *
 *  @param  type		The HAL battery type
 *  @return			The powerDevice ENUM
 */
gint convert_haltype_to_powerdevice (const gchar *type)
{
	/* assertion checks */
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

/** Gets the charge state string from a slot object
 *
 *  @param  slotData		the GenericObject reference
 *  @return			the charge string, e.g. "fully charged"
 */
gchar *
get_chargestate_string (GenericObject *slotData)
{
	/* assertion checks */
	g_assert (slotData);

	if (!slotData->present)
		return _("missing");
	else if (slotData->isCharging)
		return _("charging");
	else if (slotData->isDischarging)
		return _("discharging");
	else if (!slotData->isCharging &&
		 !slotData->isDischarging &&
		 slotData->percentageCharge > 99)
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
	GString* timestring = NULL;
	gint hours;

	if (minutes == 0)
		return NULL;

	timestring = g_string_new ("");
	if (minutes == 1)
		g_string_printf (timestring, _("1 minute"));
	else if (minutes < 60)
		g_string_printf (timestring, _("%i minutes"), minutes);
	else {
		hours = minutes / 60;
		minutes = minutes - (hours * 60);
		if (minutes == 0) {
			if (hours == 1)
				g_string_printf (timestring, _("1 hour"));
			else
				g_string_printf (timestring, _("%i hours"), hours);
		} else {
			if (hours == 1) {
				if (minutes == 1)
					g_string_printf (timestring, _("1 hour 1 minute"));
				else
					g_string_printf (timestring,
					_("1 hour %i minutes"), minutes);
			} else
				g_string_printf (timestring,
					_("%i hours %i minutes"), hours, minutes);
		}
	}
	return timestring;
}
