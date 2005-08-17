/***************************************************************************
 *
 * gpm-dbus-server.h : DBUS listener
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

#if 0

#include <glib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include "gpm-main.h"
#include "gpm-common.h"
#include "gpm-dbus-server.h"

GPtrArray *registered;

/** Finds the registered list position from the dbus connection name.
 *
 *  @param  dbusName	The dbus connection, e.g. 0:13
 *  @return				The position
 */
static int
vetoFindName (const char *dbusName)
{
	int a;
	RegProgram *regprog;
	for (a=0;a<registered->len;a++) {
		regprog = (RegProgram *) g_ptr_array_index (registered, a);
		if (strcmp (regprog->dbusName->str, dbusName) == 0) {
			return a;
		}
	}
	return -1;
}

/** Process the vetoACK signal
 *
 *  @param  dbusName	The dbus connection, e.g. 0:13
 *  @param  flags		The dbus flags, e.g. GPM_DBUS_SCREENSAVE|GPM_DBUS_LOGOFF
 */
static gboolean
vetoACK (const char *dbusName, gint flags)
{
	g_return_val_if_fail (registered, FALSE);
	g_return_val_if_fail (dbusName, FALSE);

	int a = vetoFindName (dbusName);
	if (a == -1) {
		g_warning ("Program '%s' sent vetoACK.\n"
			   "It MUST call vetoActionRegisterInterest first!", dbusName);
		return FALSE;
	}

	RegProgram *regprog = (RegProgram *) g_ptr_array_index (registered, a);

	GString *flagtext = convert_gpmdbus_to_string (flags);
	g_debug ("vetoACK received from %s\n"
		 " ENUM = %s, Application = %s\n", dbusName, flagtext->str, regprog->appName->str);
	g_string_free (flagtext, TRUE);

	regprog->isACK = TRUE;
	return TRUE;
}

/** Process the vetoNACK signal
 *
 *  @param  dbusName	The dbus connection, e.g. 0:13
 *  @param  flags		The dbus flags, e.g. GPM_DBUS_SCREENSAVE|GPM_DBUS_LOGOFF
 *  @param  reason		The reason given for the NACK
 */
static gboolean
vetoNACK (const char *dbusName, gint flags, char *reason)
{
	g_return_val_if_fail (registered, FALSE);
	g_return_val_if_fail (dbusName, FALSE);

	int a = vetoFindName (dbusName);
	if (a == -1) {
		g_warning ("Program '%s' sent vetoNACK.\n"
			   "It MUST call vetoActionRegisterInterest first!", dbusName);
		return FALSE;
	}

	RegProgram *regprog = (RegProgram *) g_ptr_array_index (registered, a);

	GString *flagtext = convert_gpmdbus_to_string (flags);
	g_debug ("vetoNACK received from %s\n"
		 " ENUM = %s, Reason = %s\n", dbusName, flagtext->str, reason);
	g_string_free (flagtext, TRUE);

	regprog->isNACK = TRUE;
	regprog->reason = g_string_new (reason);
	return TRUE;
}

/** Process the vetoActionRegisterInterest signal
 *
 *  @param  dbusName	The dbus connection, e.g. 0:13
 *  @param  flags		The dbus flags, e.g. GPM_DBUS_SCREENSAVE|GPM_DBUS_LOGOFF
 *  @param  appname		The localised application name, e.g. "Totem"
 */
static gboolean
vetoActionRegisterInterest (const char *dbusName, gint flags, gchar *appName)
{
	g_return_val_if_fail (registered, FALSE);

	int a;
	a = vetoFindName (dbusName);
	if (a != -1) {
		g_warning ("Program '%s' has already called "
			   "vetoActionRegisterInterest on this DBUS connection!", appName);
		return FALSE;
	}

	GString *flagtext = convert_gpmdbus_to_string (flags);
	g_debug ("vetoActionRegisterInterest received from %s\n"
		 " ENUM = %s, Application = %s\n", dbusName, flagtext->str, appName);
	g_string_free (flagtext, TRUE);

	RegProgram *reg = g_new (RegProgram, 1);
	reg->flags = flags;
	reg->dbusName = g_string_new (dbusName);
	reg->appName = g_string_new (appName);
	reg->reason = NULL;
	reg->isACK = FALSE;
	reg->isNACK = FALSE;
	g_ptr_array_add (registered, (gpointer) reg);
	return TRUE;
}

/** Process the vetoActionUnregisterInterest signal
 *
 *  @param  dbusName	The dbus connection, e.g. 0:13
 *  @param  flags		The dbus flags, e.g. GPM_DBUS_SCREENSAVE|GPM_DBUS_LOGOFF
 */
static gboolean
vetoActionUnregisterInterest (const char *dbusName, gint flags, gboolean suppressError)
{
	g_return_val_if_fail (registered, FALSE);

	int a = vetoFindName (dbusName);
	if (a == -1) {
		if (!suppressError)
			g_warning ("Program '%s' has called vetoActionUnregisterInterest "
			   "without calling vetoActionRegisterInterest!", dbusName);
		return FALSE;
	}

	RegProgram *regprog = (RegProgram *) g_ptr_array_index (registered, a);
	GString *flagtext = convert_gpmdbus_to_string (flags);
	g_debug ("vetoActionUnregisterInterest received from %s\n"
			 " ENUM = %s, Application = %s\n", regprog->dbusName->str,
		 	flagtext->str, regprog->appName->str);
	g_string_free (flagtext, TRUE);

	/* remove from list */
	g_ptr_array_remove_index (registered, a);
	g_free (regprog);
	return TRUE;
}

DBusHandlerResult
dbus_signal_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	/* User data is the event loop we are running in */
	/* A signal from the connection saying we are about to be disconnected */
	if (dbus_message_is_signal (message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
		char *oldservicename = NULL;
		char *newservicename = NULL;
		if (!dbus_message_get_args (message, NULL,
					    DBUS_TYPE_STRING, &oldservicename,
					    DBUS_TYPE_STRING, &newservicename,
					    DBUS_TYPE_INVALID)) {
			g_warning ("Invalid NameOwnerChanged signal from bus!");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		g_debug ("'%s' -> '%s'\n", oldservicename, newservicename);
		if (newservicename && strlen(newservicename) > 0) {
			g_warning ("Disconnected due to crash '%s'", newservicename);
			vetoActionUnregisterInterest (newservicename, GPM_DBUS_ALL, TRUE);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusMessage *
dbus_method_handler (DBusMessage *message, DBusError *error)
{
	DBusMessage *message_reply;

	if (dbus_message_is_method_call (message, GPM_DBUS_SERVICE, "isActive")) {
		g_debug ("Got 'isActive'\n");
		if (!dbus_message_get_args (message, error, DBUS_TYPE_INVALID)) {
			g_warning ("Incorrect arguments 'bool isActive(void)'");
			return NULL;
			}
		message_reply = dbus_message_new_method_return (message);
		gboolean data_value = TRUE;
		dbus_message_append_args (message_reply, DBUS_TYPE_BOOLEAN, &data_value, DBUS_TYPE_INVALID);
		return message_reply;
	} else if (dbus_message_is_method_call (message, GPM_DBUS_SERVICE, "isUserIdle")) {
		g_debug ("Got 'isUserIdle'\n");
		if (!dbus_message_get_args (message, error, DBUS_TYPE_INVALID)) {
			g_warning ("Incorrect arguments 'bool isUserIdle(void)'");
			return NULL;
			}
		message_reply = dbus_message_new_method_return (message);
		gboolean data_value = FALSE; /** TODO */
		dbus_message_append_args (message_reply, DBUS_TYPE_BOOLEAN, &data_value, DBUS_TYPE_INVALID);
		return message_reply;
	} else if (dbus_message_is_method_call (message, GPM_DBUS_SERVICE, "vetoACK")) {
		g_debug ("Got 'vetoACK'\n");
		gint value;
		gboolean data_value = FALSE;
		const char *from = dbus_message_get_sender (message);
		if (dbus_message_get_args (message, error, DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID)) {
			data_value = vetoACK (from, value);
		} else {
			g_warning ("vetoACK received, but error getting message: %s", error->message);
			dbus_error_free (error);
		}
		message_reply = dbus_message_new_method_return (message);
		dbus_message_append_args (message_reply, DBUS_TYPE_BOOLEAN, &data_value, DBUS_TYPE_INVALID);
		return message_reply;
	} else if (dbus_message_is_method_call (message, GPM_DBUS_SERVICE, "vetoNACK")) {
		g_debug ("Got 'vetoNACK'\n");
		gint value;
		gboolean data_value = FALSE;
		char *reason;
		const char *from = dbus_message_get_sender (message);
		if (dbus_message_get_args (message, error, DBUS_TYPE_INT32, &value, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID)) {
			data_value = vetoNACK (from, value, reason);
		} else {
			g_warning ("vetoNACK received, but error getting message: %s", error->message);
			dbus_error_free (error);
		}
		message_reply = dbus_message_new_method_return (message);
		dbus_message_append_args (message_reply, DBUS_TYPE_BOOLEAN, &data_value, DBUS_TYPE_INVALID);
		return message_reply;
	} else if (dbus_message_is_method_call (message, GPM_DBUS_SERVICE, "vetoActionRegisterInterest")) {
		g_debug ("Got 'vetoActionRegisterInterest'\n");
		gint value;
		gboolean data_value = FALSE;
		char *appname;
		const char *from = dbus_message_get_sender (message);
		if (dbus_message_get_args (message, error, DBUS_TYPE_INT32, &value, DBUS_TYPE_STRING, &appname, DBUS_TYPE_INVALID)) {
			data_value = vetoActionRegisterInterest (from, value, appname);
		} else {
			g_warning ("vetoActionRegisterInterest received, but error getting message: %s", error->message);
			dbus_error_free (error);
		}
		message_reply = dbus_message_new_method_return (message);
		dbus_message_append_args (message_reply, DBUS_TYPE_BOOLEAN, &data_value, DBUS_TYPE_INVALID);
		return message_reply;
	} else if (dbus_message_is_method_call (message, GPM_DBUS_SERVICE, "vetoActionUnregisterInterest")) {
		g_debug ("Got 'vetoActionUnregisterInterest'\n");
		gint value;
		gboolean data_value = FALSE;
		const char *from = dbus_message_get_sender (message);
		if (dbus_message_get_args (message, error, DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID)) {
			data_value = vetoActionUnregisterInterest (from, value, TRUE);
		} else {
			g_warning ("vetoActionUnregisterInterest received, but error getting message: %s", error->message);
			dbus_error_free (error);
		}
		message_reply = dbus_message_new_method_return (message);
		dbus_message_append_args (message_reply, DBUS_TYPE_BOOLEAN, &data_value, DBUS_TYPE_INVALID);
		return message_reply;
	} else if (dbus_message_is_method_call (message, GPM_DBUS_SERVICE, "isRunningOnMains")) {
		g_debug ("Got 'isRunningOnMains'\n");
		if (!dbus_message_get_args (message, error, DBUS_TYPE_INVALID)) {
			g_warning ("Incorrect arguments 'bool isRunningOnMains(void)'");
			return NULL;
			}
		message_reply = dbus_message_new_method_return (message);
		gboolean data_value = TRUE; /** TODO */
		dbus_message_append_args (message_reply, DBUS_TYPE_BOOLEAN, &data_value, DBUS_TYPE_INVALID);
		return message_reply;
	}
	return NULL;
}

static DBusMessage *
dbus_create_error_reply (DBusMessage *message)
{
	char *msg;
	DBusMessage *reply;

	if (dbus_message_get_type (message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return dbus_message_new_error (message,
					       GPM_DBUS_INTERFACE_ERROR,
					       "Message type is not message_call");

	msg = g_strdup_printf ("Unknown method name '%s' on interface '%s'",
			     dbus_message_get_member (message),
			     dbus_message_get_interface (message));
	reply = dbus_message_new_error (message, GPM_DBUS_INTERFACE_ERROR, msg);
	g_free (msg);

	return reply;
}

#endif
