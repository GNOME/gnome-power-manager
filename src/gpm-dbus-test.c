/*! @file	gpm-dbus-test.c
 *  @brief	DBUS test program
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This is a test program that fully exercises the DBUS API provided
 * in g-p-m. It is not expeted to be installed on the users system, but
 * instead be used a development aide.
 *
 *  @todo	Fix to use up to date g-p-m API.
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


#include <glib.h>
#include <string.h>
#include <dbus/dbus-glib.h>
#include "gpm-main.h"
#include "gpm-common.h"

#include "dbus-common.h"
#include "glibhal-main.h"
#include "glibhal-extras.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define GPM_DBUS_TEST_APP "GNOME Power Test"

DBusGProxy *gpm_proxy;
DBusGProxy *signal_proxy;

static gboolean doACK = FALSE;
static gboolean doNACK = FALSE;

static void
signal_handler_mainsStatusChanged (DBusGProxy *proxy, gboolean value, gpointer user_data)
{
	g_print ("mainsStatusChanged received: AC = %i\n", value);
}

static void
signal_handler_performingAction (DBusGProxy *proxy, gint value, gpointer user_data)
{
	GString *flags = convert_gpmdbus_to_string (value);
	g_print ("performingAction received: ENUM = %s\n", flags->str);
	g_string_free (flags, TRUE);
}

static void
signal_handler_actionAboutToHappen (DBusGProxy *proxy, gint value, gpointer user_data)
{
	GString *flags = convert_gpmdbus_to_string (value);
	GError *error = NULL;
	gboolean boolret;

	g_print ("actionAboutToHappen received: ENUM = %s\n", flags->str);
	g_string_free (flags, TRUE);


	if (doACK)
		if (!dbus_g_proxy_call (gpm_proxy, "vetoACK", &error, 
				G_TYPE_INT, value, G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
			dbus_glib_error (error);
	if (doNACK)
		if (!dbus_g_proxy_call (gpm_proxy, "vetoNACK", &error, 
				G_TYPE_INT, value, G_TYPE_STRING, "Unsaved file needs to be saved.", G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
			dbus_glib_error (error);
}

static void
name_owner_changed (DBusGProxy *proxy, const char *name, const char *prev_owner, const char *new_owner, gpointer user_data)
{
	g_print ("(signal NameOwnerChanged) name owner changed for %s from %s to %s\n",
				name, prev_owner, new_owner);
}

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
		"		--isUserIdle         Checks if the user is idle\n"
		"		--isRunningOnMains   Checks to see if user is running on mains\n"
		"		--isActive           Checks to see if user is active\n");
	g_print (
		" MONITOR\n"
		"		--monitor            Monitors bus, outputing to consol\n"
		"		--doNothing          ActionRegister, then does nothing on actionAboutToHappen.\n"
		"		--doACK              ActionRegister, then does vetoACK on actionAboutToHappen\n"
		"		--doNACK             ActionRegister, then does vetoNACK on actionAboutToHappen\n"
		" SYNC\n"
		"		--registerUnregister Abuses ActionRegister and ActionUnregister\n"
		"		--wrongACK           Abuses ACK and NACK"
		"\n");
}

int
main (int argc, char **argv)
{
	GMainLoop *loop = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *dbus_proxy = NULL;
	GError *error = NULL;
	gboolean isOkay;
	gboolean doMonitor;
	gboolean boolret;
	int a;

	/* initialise threads */
	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();

	/* Create a new event loop to run in */
	loop = g_main_loop_new (NULL, FALSE);

	/* Get a connection to the session connection */
	if (!dbus_get_session_connection (&session_connection))
		return FALSE;
	gpm_proxy = dbus_g_proxy_new_for_name (session_connection,
			GPM_DBUS_SERVICE,
			GPM_DBUS_PATH,
			GPM_DBUS_INTERFACE);
	signal_proxy = dbus_g_proxy_new_for_name (session_connection,
			GPM_DBUS_SERVICE,
			GPM_DBUS_PATH,
			GPM_DBUS_INTERFACE);

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

	/* add session connection tracking */
	dbus_proxy = dbus_g_proxy_new_for_name (session_connection,
							DBUS_SERVICE_DBUS,
							DBUS_PATH_DBUS,
							DBUS_INTERFACE_DBUS);

	dbus_g_proxy_add_signal (dbus_proxy, "NameOwnerChanged",
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (dbus_proxy, "NameOwnerChanged",
		G_CALLBACK (name_owner_changed), NULL, NULL);

	isOkay = FALSE;
	doMonitor = FALSE;

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

			if (!dbus_g_proxy_call (gpm_proxy, "Ack", &error, 
									G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
									G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
			if (!dbus_g_proxy_call (gpm_proxy, "Nack", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_STRING, "It's a Sunday", G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);

		} else if (strcmp (argv[a], "--registerUnregister") == 0) {
			isOkay = TRUE;
			g_print ("Registering and Unregistering GPM_DBUS_SHUTDOWN\n");
			/* 
			 * testing unregistering before registering
			 */
			if (!dbus_g_proxy_call (gpm_proxy, "ActionUnregister", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
			/* 
			 * testing double registering
			 * TODO : this now fails, but it should pass if the flags are different
			 */
			if (!dbus_g_proxy_call (gpm_proxy, "ActionRegister", &error, 
					G_TYPE_INT, GPM_DBUS_SHUTDOWN | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
			if (!dbus_g_proxy_call (gpm_proxy, "ActionRegister", &error, 
									G_TYPE_INT, GPM_DBUS_SHUTDOWN | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
									G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
			/* 
			 * testing double unregistering
			 */
			if (!dbus_g_proxy_call (gpm_proxy, "ActionUnregister", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
			if (!dbus_g_proxy_call (gpm_proxy, "ActionUnregister", &error, 
					G_TYPE_INT, GPM_DBUS_ALL, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
			/* 
			 * testing program quit (should do automatic disconnect)
			 */
			if (!dbus_g_proxy_call (gpm_proxy, "ActionRegister", &error, 
					G_TYPE_INT, GPM_DBUS_SHUTDOWN | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
		} else if (strcmp (argv[a], "--doACK") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			doACK = TRUE;
			g_print ("Testing ACK with monitor\n");
			if (!dbus_g_proxy_call (gpm_proxy, "ActionRegister", &error, 
					G_TYPE_INT, GPM_DBUS_SHUTDOWN | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
		} else if (strcmp (argv[a], "--doNACK") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			doNACK = TRUE;
			g_print ("Testing NACK with monitor\n");
			if (!dbus_g_proxy_call (gpm_proxy, "ActionRegister", &error, 
					G_TYPE_INT, GPM_DBUS_SHUTDOWN | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
		} else if (strcmp (argv[a], "--doNothing") == 0) {
			isOkay = TRUE;
			doMonitor = TRUE;
			g_print ("Testing no-response with monitor\n");
			if (!dbus_g_proxy_call (gpm_proxy, "ActionRegister", &error, 
					G_TYPE_INT, GPM_DBUS_SHUTDOWN | GPM_DBUS_LOGOFF, G_TYPE_STRING, GPM_DBUS_TEST_APP, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
		} else if (strcmp (argv[a], "--isUserIdle") == 0 || 
			   strcmp (argv[a], "--isRunningOnMains") == 0) {
			isOkay = TRUE;

			if (!dbus_g_proxy_call (gpm_proxy, argv[a]+2, &error, 
					G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID))
				dbus_glib_error (error);
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
	g_object_unref (gpm_proxy);
	return 0;
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */
