/***************************************************************************
 *
 * gpm-dbus-common.c : DBUS Common functions
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

/* 
 * TODO
 * This file will be made 90% smaller when we use the glib bindings of DBUS
 * when version 0.34+ is being carried by distros.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <glib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include "gpm-main.h"
#include "gpm-dbus-common.h"

/** Converts an dbus ENUM to it's string representation 
 *
 *  @param  value		The dbus ENUM
 *  @return				action string, e.g. "Shutdown"
 */
gchar *
convert_dbus_enum_to_string (gint value)
{
	if (value == GPM_DBUS_SCREENSAVE)
		return _("screensave");
	else if (value == GPM_DBUS_POWEROFF)
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
 *  @param  value		The dbus ENUM's
 *  @return				action string, e.g. "{GPM_DBUS_SCREENSAVE|GPM_DBUS_LOGOFF}"
 */
GString *
convert_gpmdbus_to_string (gint value)
{
	GString *retval = g_string_new ("{");
	if (value & GPM_DBUS_SCREENSAVE)
		g_string_append (retval, "GPM_DBUS_SCREENSAVE|");
	if (value & GPM_DBUS_POWEROFF)
		g_string_append (retval, "GPM_DBUS_POWEROFF|");
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

gboolean
dbus_send_signal_bool (DBusConnection *connection, const char *action, gboolean value)
{
	g_return_val_if_fail (connection, FALSE);
	DBusMessage *message;
	message = dbus_message_new_signal (GPM_DBUS_PATH, GPM_DBUS_INTERFACE_SIGNAL, action);
	if (!message) {
		g_warning ("dbus_message_new_signal failed to construct message.");
		return FALSE;
	}

	dbus_message_append_args (message, DBUS_TYPE_BOOLEAN, &value, DBUS_TYPE_INVALID);
	dbus_connection_send (connection, message, NULL);
	dbus_message_unref (message);
	g_print ("dbus_send_signal_bool : '%s' Argument : '%i'\n", action, value);
	return TRUE;
}

gboolean
dbus_send_signal_int (DBusConnection *connection, const char *action, gint value)
{
	g_return_val_if_fail (connection, FALSE);
	DBusMessage *message;
	message = dbus_message_new_signal (GPM_DBUS_PATH, GPM_DBUS_INTERFACE_SIGNAL, action);
	if (!message) {
		g_warning ("dbus_message_new_signal failed to construct message.");
		return FALSE;
	}

	dbus_message_append_args (message, DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID);
	dbus_connection_send (connection, message, NULL);
	dbus_message_unref (message);
	g_print ("dbus_send_signal_int : '%s' Argument : '%i'\n", action, value);
	return TRUE;
}
