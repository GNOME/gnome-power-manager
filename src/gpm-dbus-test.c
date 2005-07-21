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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 *
 **************************************************************************/

#include <glib.h>
#include <string.h>
#include <dbus/dbus-glib.h>
/*
#include "gpm-dbus-common.h"
*/
#include "gpm-main.h"

#define	GPM_DBUS_SERVICE			"net.sf.GnomePower"
#define	GPM_DBUS_PATH				"/net/sf/GnomePower"
#define	GPM_DBUS_INTERFACE			"net.sf.GnomePower"
#define	GPM_DBUS_INTERFACE_SIGNAL	"net.sf.GnomePower.Signal"
#define	GPM_DBUS_INTERFACE_ERROR	"net.sf.GnomePower.Error"

#define GPM_DBUS_SCREENSAVE	1
#define GPM_DBUS_POWEROFF	2
#define GPM_DBUS_SUSPEND	4
#define GPM_DBUS_HIBERNATE	8
#define GPM_DBUS_LOGOFF		16
#define GPM_DBUS_ALL		255

#define GPM_DBUS_TEST_APP "GNOME Power Test"

DBusGProxy *session_proxy;
DBusGProxy *signal_proxy;

static gboolean doACK = FALSE;
static gboolean doNACK = FALSE;

static void
process_error (GError *error)
{
		if (error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
			g_printerr ("Caught remote method exception %s: %s\n",
						dbus_g_error_get_name (error),
						error->message);
		else
			g_printerr ("Error: %s\n", error->message);
		g_error_free (error);
}

static void
signal_handler_mainsStatusChanged (DBusGProxy *proxy, gboolean value, gpointer user_data)
{
	g_print ("mainsStatusChanged received: AC = %i\n", value);
}

static void
signal_handler_performingAction (DBusGProxy *proxy, gint value, gpointer user_data)
{
#if 0
	GString *flags = convert_gpmdbus_to_string (value);
	g_print ("performingAction received: ENUM = %s\n", flags->str);
	g_string_free (flags, TRUE);
#endif
	g_print ("performingAction received: ENUM = %i\n", value);
}

static void
signal_handler_actionAboutToHappen (DBusGProxy *proxy, gint value, gpointer user_data)
{
#if 0
	GString *flags = convert_gpmdbus_to_string (value);
	g_print ("actionAboutToHappen received: ENUM = %s\n", flags->str);
	g_string_free (flags, TRUE);
#endif
	g_print ("actionAboutToHappen received: ENUM = %i\n", value);

	GError *error = NULL;
	gboolean boolret;

	if (doACK)
		if (!dbus_g_proxy_call (session_proxy, "vetoACK", &error, 
								G_TYPE_INT, value, G_TYPE_INVALID,
								G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
			process_error (error);
	if (doNACK)
		if (!dbus_g_proxy_call (session_proxy, "vetoNACK", &error, 
				G_TYPE_INT, value, G_TYPE_STRING, "Unsaved file needs to be saved.", G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
			process_error (error);
}

#if 0
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
				dbus_send_signal_int_string (connection, "vetoNACK", value, "Unsaved file needs to be saved.");
		} else {
			g_print ("actionAboutToHappen received, but error getting message: %s\n", error.message);
			dbus_error_free (&error);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
#endif

/** Prints program usage.
 *
 */
static void print_usage (void)
{
	g_print ("\nusage : gnome-power-dbus-test [options]\n");
	g_print (
		"\n"
		"		--help               Show this information and exit\n"
		"\n"
		" METHODS\n"
		"		--isUserIdle         xxx\n"
		"		--isRunningOnMains   xxx\n"
		"		--isActive           xxx\n"
		" MONITOR\n"
		"		--monitor            xxx\n"
		"		--doNothing          xxx\n"
		"		--doACK              xxx\n"
		"		--doNACK             xxx\n"
		" SYNC\n"
		"		--registerUnregister xxx\n"
		"		--wrongACK\n"
		"\n");
}

int
main (int argc, char **argv)
{
	GMainLoop *loop;
	DBusGConnection *session_connection;
	GError *error = NULL;

	/* initialise threads */
	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();

	/* Create a new event loop to run in */
	loop = g_main_loop_new (NULL, FALSE);

	/* Get a connection to the session connection */
	session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (session_connection == NULL)
	{
		g_printerr ("Failed to open connection to bus: %s\n", error->message);
		g_error_free (error);
		return 0;
	}

	session_proxy = dbus_g_proxy_new_for_name (session_connection,
							GPM_DBUS_SERVICE,
							GPM_DBUS_PATH,
							GPM_DBUS_INTERFACE);
	signal_proxy = dbus_g_proxy_new_for_name (session_connection,
							GPM_DBUS_SERVICE,
							GPM_DBUS_PATH,
							GPM_DBUS_INTERFACE_SIGNAL);

	dbus_g_proxy_add_signal (signal_proxy, "mainsStatusChanged", 
		G_TYPE_BOOLEAN, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (signal_proxy, "mainsStatusChanged", 
		G_CALLBACK (signal_handler_mainsStatusChanged), NULL, NULL);

	dbus_g_proxy_add_signal (signal_proxy, "performingAction", 
		G_TYPE_INT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (signal_proxy, "performingAction", 
		G_CALLBACK (signal_handler_performingAction), NULL, NULL);

	dbus_g_proxy_add_signal (signal_proxy, "actionAboutToHappen", 
		G_TYPE_INT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (signal_proxy, "actionAboutToHappen", 
		G_CALLBACK (signal_handler_actionAboutToHappen), NULL, NULL);


#if 0
	/* Set up this connection to work in a GLib event loop */
	dbus_connection_setup_with_g_main (connection, NULL);
#endif

	/* listening to messages from all objects as no path is specified 
	 * This will provide a link back for an object changed request
	 */
#if 0
	dbus_bus_add_match (connection, "type='signal',interface='" GPM_DBUS_INTERFACE_SIGNAL "'", &error);
	if (dbus_error_is_set (&error)) {
		g_print ("dbus_bus_add_match Failed. Error says: \n'%s'\n", error.message);
		return 0;
	}
	if (!dbus_connection_add_filter (connection, dbus_signal_filter, loop, NULL))
		g_print ("Cannot apply filter\n");
#endif

	gboolean isOkay = FALSE;
	gboolean doMonitor = FALSE;
	gboolean boolret;

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

			if (!dbus_g_proxy_call (session_proxy, "vetoACK", &error, 
									G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
									G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
#if 0
			dbus_g_proxy_call_no_reply (session_proxy, "vetoNACK", 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_STRING, "It's a Sunday", G_TYPE_INVALID);
#endif
			if (!dbus_g_proxy_call (session_proxy, "vetoNACK", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_STRING, "It's a Sunday", G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);

		} else if (strcmp (argv[a], "--registerUnregister") == 0) {
			isOkay = TRUE;
			g_print ("Registering and Unregistering GPM_DBUS_POWEROFF\n");
			/* 
			 * testing unregistering before registering
			 */
			if (!dbus_g_proxy_call (session_proxy, "vetoActionUnregisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
			/* 
			 * testing double registering
			 * TODO : this now fails, but it should pass if the flags are different
			 */
			if (!dbus_g_proxy_call (session_proxy, "vetoActionRegisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
			if (!dbus_g_proxy_call (session_proxy, "vetoActionRegisterInterest", &error, 
									G_TYPE_INT, GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
									G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
			/* 
			 * testing double unregistering
			 */
			if (!dbus_g_proxy_call (session_proxy, "vetoActionUnregisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
			if (!dbus_g_proxy_call (session_proxy, "vetoActionUnregisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
			/* 
			 * testing program quit (should do automatic disconnect)
			 */
			if (!dbus_g_proxy_call (session_proxy, "vetoActionRegisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
		} else if (strcmp (argv[a], "--doACK") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			doACK = TRUE;
			g_print ("Testing ACK with monitor\n");
			if (!dbus_g_proxy_call (session_proxy, "vetoActionRegisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
		} else if (strcmp (argv[a], "--doNACK") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			doNACK = TRUE;
			g_print ("Testing NACK with monitor\n");
			if (!dbus_g_proxy_call (session_proxy, "vetoActionRegisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
		} else if (strcmp (argv[a], "--doNothing") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			g_print ("Testing no-response with monitor\n");
			if (!dbus_g_proxy_call (session_proxy, "vetoActionRegisterInterest", &error, 
					G_TYPE_INT, GPM_DBUS_POWEROFF | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
		} else if (strcmp (argv[a], "--isActive") == 0 || 
				 strcmp (argv[a], "--isUserIdle") == 0 || 
				 strcmp (argv[a], "--isRunningOnMains") == 0) {
			isOkay = TRUE;

			if (!dbus_g_proxy_call (session_proxy, argv[a]+2, &error, 
					G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				process_error (error);
			if (boolret)
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

	/* close session connections */
	g_object_unref (signal_proxy);
	g_object_unref (session_proxy);
	return 0;
}
