/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
 *
 * Taken in part from:
 *  - lshal   (C) 2003 David Zeuthen, <david@fubar.dk>
 *  - notibat (C) 2004 Benjamin Kahn, <xkahn@zoned.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gpm-stock-icons.h"
#include "gpm-common.h"
#include "gpm-manager.h"
#include "gpm-debug.h"

static GDBusProxy *session_proxy = NULL;
static GDBusProxy *session_proxy_client = NULL;
static GMainLoop *loop = NULL;

/**
 * timed_exit_cb:
 * @loop: The main loop
 *
 * Exits the main loop, which is helpful for valgrinding g-p-m.
 *
 * Return value: FALSE, as we don't want to repeat this action.
 **/
static gboolean
timed_exit_cb (GMainLoop *_loop)
{
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * gpm_main_session_end_session_response:
 **/
static gboolean
gpm_main_session_end_session_response (gboolean is_okay, const gchar *reason)
{
	gboolean ret = FALSE;
	GVariant *retval = NULL;
	GError *error = NULL;

	g_return_val_if_fail (session_proxy_client != NULL, FALSE);

	/* no gnome-session */
	if (session_proxy == NULL) {
		g_warning ("no gnome-session");
		goto out;
	}

	retval = g_dbus_proxy_call_sync (session_proxy,
					 "EndSessionResponse",
					 g_variant_new ("(bs)",
							is_okay,
							reason),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1, NULL, &error);
	if (retval == NULL) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (retval != NULL)
		g_variant_unref (retval);
	return ret;
}

/**
 * gpm_main_session_dbus_signal_cb:
 **/
static void
gpm_main_session_dbus_signal_cb (GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer user_data)
{
	if (g_strcmp0 (signal_name, "Stop") == 0) {
		g_main_loop_quit (loop);
		return;
	}
	if (g_strcmp0 (signal_name, "QueryEndSession") == 0) {
		/* just send response */
		gpm_main_session_end_session_response (TRUE, NULL);
		return;
	}
	if (g_strcmp0 (signal_name, "EndSession") == 0) {
		/* send response */
		gpm_main_session_end_session_response (TRUE, NULL);

		/* exit loop, will unref manager */
		g_main_loop_quit (loop);
		return;
	}
}

/**
 * gpm_main_session_register_client:
 **/
static gboolean
gpm_main_session_register_client (const gchar *app_id, const gchar *client_startup_id)
{
	gboolean ret = FALSE;
	gchar *client_id = NULL;
	GError *error = NULL;
	GDBusConnection *connection;
	GVariant *retval = NULL;

	/* fallback for local running */
	if (client_startup_id == NULL)
		client_startup_id = "";

	/* get connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		g_warning ("Failed to get session connection: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	/* get org.gnome.Session interface */
	session_proxy =
		g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
			G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
			NULL,
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager",
			"org.gnome.SessionManager",
			NULL, &error);
	if (session_proxy == NULL) {
		g_warning ("Failed to get gnome-session: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	/* register ourselves */
	retval = g_dbus_proxy_call_sync (session_proxy,
					 "RegisterClient",
					 g_variant_new ("(ss)",
							app_id,
							client_startup_id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1, NULL, &error);
	if (retval == NULL) {
		g_warning ("failed to register client '%s': %s", client_startup_id, error->message);
		g_error_free (error);
		goto out;
	}

	/* get client id */
	g_variant_get (retval, "(o)", &client_id);

	/* get org.gnome.Session.ClientPrivate interface */
	session_proxy_client =
		g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
			NULL,
			"org.gnome.SessionManager",
			client_id,
			"org.gnome.SessionManager.ClientPrivate",
			NULL, &error);
	if (session_proxy_client == NULL) {
		g_warning ("failed to setup private proxy: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_signal_connect (session_proxy_client, "g-signal", G_CALLBACK (gpm_main_session_dbus_signal_cb), NULL);

	/* success */
	ret = TRUE;
	g_debug ("registered startup '%s' to client id '%s'", client_startup_id, client_id);
out:
	if (retval != NULL)
		g_variant_unref (retval);
	g_free (client_id);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GDBusConnection *system_connection;
	GDBusConnection *session_connection;
	gboolean version = FALSE;
	gboolean timed_exit = FALSE;
	gboolean immediate_exit = FALSE;
	GpmManager *manager = NULL;
	GError *error = NULL;
	GOptionContext *context;
	gint policy_owner_id;
	guint timer_id;

	const GOptionEntry options[] = {
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
		  N_("Show version of installed program and exit"), NULL },
		{ "timed-exit", '\0', 0, G_OPTION_ARG_NONE, &timed_exit,
		  N_("Exit after a small delay (for debugging)"), NULL },
		{ "immediate-exit", '\0', 0, G_OPTION_ARG_NONE, &immediate_exit,
		  N_("Exit after the manager has loaded (for debugging)"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();

	context = g_option_context_new (N_("GNOME Power Manager"));
	/* TRANSLATORS: program name, a simple app to view pending updates */
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gpm_debug_get_option_group ());
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, _("GNOME Power Manager"));
	g_option_context_parse (context, &argc, &argv, NULL);

	if (version) {
		g_print ("Version %s\n", VERSION);
		goto unref_program;
	}

	if (!g_thread_supported ())
		g_thread_init (NULL);

	gtk_init (&argc, &argv);

	g_debug ("GNOME %s %s", GPM_NAME, VERSION);

	/* check dbus connections, exit if not valid */
	system_connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_error ("This program cannot start until you start "
			   "the dbus system service.\n"
			   "It is <b>strongly recommended</b> you reboot "
			   "your computer after starting this service.");
	}

	session_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_error ("This program cannot start until you start the "
			   "dbus session service.\n\n"
			   "This is usually started automatically in X "
			   "or gnome startup when you start a new session.");
	}

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   GPM_DATA G_DIR_SEPARATOR_S "icons");

	loop = g_main_loop_new (NULL, FALSE);

	/* optionally register with the session */
	gpm_main_session_register_client ("gnome-power-manager", getenv ("DESKTOP_AUTOSTART_ID"));

	/* create a new gui object */
	manager = gpm_manager_new ();

	/* register to be a policy agent, just like kpowersave does */
	policy_owner_id = g_bus_own_name_on_connection (system_connection,
				    "org.freedesktop.Policy.Power",
				    G_BUS_NAME_OWNER_FLAGS_REPLACE, NULL, NULL, NULL, NULL);

	/* Only timeout and close the mainloop if we have specified it
	 * on the command line */
	if (timed_exit) {
		timer_id = g_timeout_add_seconds (20, (GSourceFunc) timed_exit_cb, loop);
		g_source_set_name_by_id (timer_id, "[GpmMain] timed-exit");
	}

	if (immediate_exit == FALSE) {
		g_main_loop_run (loop);
	}

	g_main_loop_unref (loop);

	if (session_proxy != NULL)
		g_object_unref (session_proxy);
	if (session_proxy_client != NULL)
		g_object_unref (session_proxy_client);
	g_object_unref (manager);
unref_program:
	g_option_context_free (context);
	return 0;
}
