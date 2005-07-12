/***************************************************************************
 *
 * gpm-dbus-test.h : DBUS test program
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

#include <glib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "gpm-dbus-common.h"
#include "gpm-main.h"

#define GPM_DBUS_TEST_APP "GNOME Power Test"

static gboolean doACK = FALSE;
static gboolean doNACK = FALSE;

DBusHandlerResult
dbus_signal_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	/* User data is the event loop we are running in */
	GMainLoop *loop = user_data;

	/* A signal from the connection saying we are about to be disconnected */
	if (dbus_message_is_signal (message, DBUS_INTERFACE_DBUS, "Disconnected")) {
		g_print ("Disconnected\n");
		/* Tell the main loop to quit */
		g_main_loop_quit (loop);
		/* We have handled this message, don't pass it on */
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal (message, GPM_DBUS_INTERFACE_SIGNAL, "mainsStatusChanged")) {
		DBusError error;
		gboolean value;
		dbus_error_init (&error);
		if (dbus_message_get_args (message, &error, DBUS_TYPE_BOOLEAN, &value, DBUS_TYPE_INVALID)) {
			g_print ("mainsStatusChanged received: AC = %i\n", value);
		} else {
			g_print ("mainsStatusChanged received, but error getting message: %s\n", error.message);
			dbus_error_free (&error);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal (message, GPM_DBUS_INTERFACE_SIGNAL, "performingAction")) {
		DBusError error;
		gint value;
		dbus_error_init (&error);
		if (dbus_message_get_args (message, &error, DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID)) {
			GString *flags = convert_gpmdbus_to_string (value);
			g_print ("performingAction received: ENUM = %s\n", flags->str);
			g_string_free (flags, TRUE);
		} else {
			g_print ("performingAction received, but error getting message: %s\n", error.message);
			dbus_error_free (&error);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal (message, GPM_DBUS_INTERFACE_SIGNAL, "actionAboutToHappen")) {
		DBusError error;
		gint value;
		dbus_error_init (&error);
		if (dbus_message_get_args (message, &error, DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID)) {
			GString *flags = convert_gpmdbus_to_string (value);
			g_print ("actionAboutToHappen received: ENUM = %s\n", flags->str);
			g_string_free (flags, TRUE);
			if (doACK)
				dbus_send_signal_int (connection, "vetoACK", value);
			if (doNACK)
				dbus_send_signal_int_string (connection, "vetoNACK", value, "It's a Sunday!");
		} else {
			g_print ("actionAboutToHappen received, but error getting message: %s\n", error.message);
			dbus_error_free (&error);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/** Prints program usage.
 *
 */
static void print_usage (void)
{
	g_print ("\nusage : gnome-power-dbus-test [options]\n");
	g_print (
		"\n"
		"    --help             Show this information and exit\n"
		"\n"
		" METHODS\n"
		"    --isUserIdle       xxx\n"
		"    --isRunningOnMains xxx\n"
		"    --isActive         xxx\n"
		" MONITOR\n"
		"    --monitor          xxx\n"
		"    --doNothing        xxx\n"
		"    --doACK            xxx\n"
		"    --doNACK           xxx\n"
		" SYNC\n"
		"    --registerUnregister xxx\n"
		"    --wrongACK\n"
		"\n");
}

int
main (int argc, char **argv)
{
	GMainLoop *loop;
	DBusConnection *connection;
	DBusError error;

	/* initialise threads */
	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();

	/* Create a new event loop to run in */
	loop = g_main_loop_new (NULL, FALSE);

	/* Get a connection to the system connection */
	dbus_error_init (&error);
	connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
	if (!connection) {
		g_warning ("Failed to connect to the D-BUS daemon: %s",
			   error.message);
		dbus_error_free (&error);
		return 1;
	}

	/* Set up this connection to work in a GLib event loop */
	dbus_connection_setup_with_g_main (connection, NULL);

	/* listening to messages from all objects as no path is specified 
	 * This will provide a link back for an object changed request
	 */
	dbus_error_init (&error);
	dbus_bus_add_match (connection, "type='signal',interface='" GPM_DBUS_INTERFACE_SIGNAL "'", &error);
	if (dbus_error_is_set (&error)) {
		g_print ("dbus_bus_add_match Failed. Error says: \n'%s'\n", error.message);
		return 0;
	}
	if (!dbus_connection_add_filter (connection, dbus_signal_filter, loop, NULL))
		g_print ("Cannot apply filter\n");

	gboolean isOkay = FALSE;
	gboolean doMonitor = FALSE;

	int a;
	for (a=1; a < argc; a++) {
		if (strcmp (argv[a], "--help") == 0) {
			print_usage ();
			return 1;
		} else if (strcmp (argv[a], "--monitor") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
		} else if (strcmp (argv[a], "--wrongACK") == 0) {
			isOkay = TRUE;
			/* 
			 * testing sending ACK and NACK before Registering
			 */
			dbus_send_signal_int (connection, "vetoACK", GPM_DBUS_ALL);
			dbus_send_signal_int_string (connection, "vetoNACK", GPM_DBUS_ALL, "It's a Sunday");
		} else if (strcmp (argv[a], "--registerUnregister") == 0) {
			isOkay = TRUE;
			g_print ("Registering and Unregistering GPM_DBUS_POWEROFF\n");
			/* 
			 * testing unregistering before registering
			 */
			dbus_send_signal_int (connection, "vetoActionUnregisterInterest", GPM_DBUS_ALL);
			/* 
			 * testing double registering
			 * TODO : this now fails, but it should pass if the flags are different
			 */
			dbus_send_signal_int_string (connection, "vetoActionRegisterInterest", GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, GPM_DBUS_TEST_APP);
			dbus_send_signal_int_string (connection, "vetoActionRegisterInterest", GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, GPM_DBUS_TEST_APP);
			/* 
			 * testing double unregistering
			 */
			dbus_send_signal_int (connection, "vetoActionUnregisterInterest", GPM_DBUS_ALL);
			dbus_send_signal_int (connection, "vetoActionUnregisterInterest", GPM_DBUS_ALL);
			/* 
			 * testing program quit (should do automatic disconnect)
			 */
			dbus_send_signal_int_string (connection, "vetoActionRegisterInterest", GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, GPM_DBUS_TEST_APP);
		} else if (strcmp (argv[a], "--doACK") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			doACK = TRUE;
			g_print ("Testing ACK with monitor\n");
			dbus_send_signal_int_string (connection, "vetoActionRegisterInterest", GPM_DBUS_ALL, GPM_DBUS_TEST_APP);
		} else if (strcmp (argv[a], "--doNACK") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			doNACK = TRUE;
			g_print ("Testing NACK with monitor\n");
			dbus_send_signal_int_string (connection, "vetoActionRegisterInterest", GPM_DBUS_ALL, GPM_DBUS_TEST_APP);
		} else if (strcmp (argv[a], "--doNothing") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			g_print ("Testing no-response with monitor\n");
			dbus_send_signal_int_string (connection, "vetoActionRegisterInterest", GPM_DBUS_ALL, GPM_DBUS_TEST_APP);
		} else if (strcmp (argv[a], "--isActive") == 0 || 
			   strcmp (argv[a], "--isUserIdle") == 0 || 
			   strcmp (argv[a], "--isRunningOnMains") == 0) {
			isOkay = TRUE;
			gboolean boolvalue;
			if (!get_bool_value (connection, argv[a]+2, &boolvalue))
				g_error ("Method '%s' failed\n", argv[a]+2);
			if (boolvalue)
				g_print ("%s TRUE\n", argv[a]+2);
			else	
				g_print ("%s FALSE\n", argv[a]+2);
		}
	}
	if (!isOkay)
		print_usage ();
	
	if (!doMonitor)
		return 0;

	/* Start the event loop */
	g_main_loop_run (loop);
	return 0;
}
