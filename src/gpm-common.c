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
#include "compiler.h"

/** Sets a widget image to a themed
 *
 *  @param	widget		The widget to set
 *  @param	name		The icon name, e.g. gnome-dev-battery
 *  @param	size		The icon size, e.g. 24
 *  @return			Success if the icon was set
 */
gboolean
gpm_set_icon_with_theme (GtkWidget *widget, const gchar *name, gint size)
{
	/* set image */
	GdkPixbuf *pixbuf = NULL;
	if (!gpm_icon_theme_fallback (&pixbuf, name, size)) {
		g_warning ("Cannot find themed icon '%s'", name);
		return FALSE;
	}
	gtk_image_set_from_pixbuf (GTK_IMAGE (widget), pixbuf);
	g_object_unref (pixbuf);
	return TRUE;
}

/** Get a image (pixbuf) trying the theme first, falling back to locally
 *  if not present. This means we do not have to check in configure.in for lots
 *  of obscure icons.
 *
 *  @param	pixbuf		A returned GTK pixbuf
 *  @param	name		the icon name, e.g. gnome-battery
 *  @param	size		the icon size, e.g. 22
 *  @return			If we found a valid image
 *
 *  @note	If we cannot find the specific themed GNOME icon we use the
 *		builtin fallbacks. This makes GPM more portible between distros
 *
 *  @note	You need to deallocate the pixbuf with g_object_unref ();
 *
 */
gboolean __must_check
gpm_icon_theme_fallback (GdkPixbuf **pixbuf, const gchar *name, gint size)
{
	GError *err = NULL;
	GString *fallback = NULL;
	GtkIconInfo *iinfo = NULL;

	/* assertion checks */
	g_assert (name);

	iinfo = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
			name, size, GTK_ICON_LOOKUP_USE_BUILTIN);
	if (iinfo) {
		g_debug ("Using stock icon for %s", name);
		*pixbuf = gtk_icon_info_load_icon (iinfo, &err);
		gtk_icon_info_free (iinfo);
	} else {
		g_debug ("Using fallback icon for %s", name);
		fallback = g_string_new ("");
		g_string_printf (fallback, GPM_DATA "%s.png", name);
		g_debug ("Using filename %s", fallback->str);
		*pixbuf = gdk_pixbuf_new_from_file (fallback->str, &err);
		g_string_free (fallback, TRUE);
	}
	/* check we actually got the icon */
	if (!*pixbuf) {
		g_warning ("failed to get %s!", name);
		return FALSE;
	}
	return TRUE;
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

/** Gets the timestring from a slot object
 *
 *  @param	slotData	the GenericObject reference
 *  @return			the timestring, e.g. "13 minutes until charged"
 */
gchar *
get_time_string (GenericObject *slotData)
{
	gchar* timestring = NULL;
	gchar* retval = NULL;

	/* assertion checks */
	g_assert (slotData);

	timestring = get_timestring_from_minutes (slotData->minutesRemaining);
	if (!timestring)
		return NULL;
	if (slotData->isCharging)
		retval = g_strdup_printf ("%s %s", timestring, _("until charged"));
	else
		retval = g_strdup_printf ("%s %s", timestring, _("remaining"));

	g_free (timestring);

	return retval;
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
PowerDevice
convert_haltype_to_powerdevice (const gchar *type)
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
 *
 *  @note	minutes == 0 is returned as "Unknown"
 */
gchar *
get_timestring_from_minutes (gint minutes)
{
	gchar* timestring = NULL;
	gint hours;

	if (minutes == 0)
		timestring = g_strdup_printf (_("Unknown"));
	else if (minutes == 1)
		timestring = g_strdup_printf (_("1 minute"));
	else if (minutes < 60)
		timestring = g_strdup_printf (_("%i minutes"), minutes);
	else {
		hours = minutes / 60;
		minutes = minutes % 60;
		if (minutes == 0) {
			if (hours == 1)
				timestring = g_strdup_printf (_("1 hour"));
			else
				timestring = g_strdup_printf (_("%i hours"), hours);
		} else if (minutes == 1) {
			if (hours == 1)
				timestring = g_strdup_printf (_("1 hour, 1 minute"));
			else
				timestring = g_strdup_printf (_("%i hours, 1 minute"), hours);
		} else if (hours == 1) {
			timestring = g_strdup_printf (_("1 hour, %i minutes"), minutes);
		} else {
			timestring = g_strdup_printf (_("%i hours, %i minutes"), hours, minutes);
		}
	}
	return timestring;
}
