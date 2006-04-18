/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libgnomeui/libgnomeui.h>
#include <glade/glade.h>

#include "gpm-stock-icons.h"
#include "gpm-hal.h"
#include "gpm-common.h"
#include "gpm-debug.h"

#include "gpm-manager.h"
#include "gpm-manager-glue.h"

static void gpm_exit (GpmManager *manager);

/**
 * gpm_object_register:
 * @connection: What we want to register to
 * @object: The GObject we want to register
 *
 * Register org.gnome.PowerManager on the session bus.
 * This function MUST be called before DBUS service will work.
 *
 * Return value: success
 **/
static gboolean
gpm_object_register (DBusGConnection *connection,
		     GObject	     *object)
{
	DBusGProxy *bus_proxy = NULL;
	GError *error = NULL;
	guint request_name_result;

	bus_proxy = dbus_g_proxy_new_for_name (connection,
					       DBUS_SERVICE_DBUS,
					       DBUS_PATH_DBUS,
					       DBUS_INTERFACE_DBUS);

	if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
		G_TYPE_STRING, GPM_DBUS_SERVICE,
		G_TYPE_UINT, 0,
		G_TYPE_INVALID,
		G_TYPE_UINT, &request_name_result,
		G_TYPE_INVALID)) {
		g_warning ("Failed to acquire %s: %s", GPM_DBUS_SERVICE, error->message);
		return FALSE;
	}

 	if (request_name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		return FALSE;

	/* free the bus_proxy */
	g_object_unref (G_OBJECT (bus_proxy));

	dbus_g_object_type_install_info (GPM_TYPE_MANAGER, &dbus_glib_gpm_manager_object_info);
	dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH, object);

	return TRUE;
}

/**
 * gpm_exit:
 * @manager: This manager class instance
 **/
static void
gpm_exit (GpmManager *manager)
{
	gpm_stock_icons_shutdown ();

	gpm_debug_shutdown ();
	g_object_unref (manager);
	exit (0);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GMainLoop       *loop;
	GnomeClient     *master;
	GnomeClientFlags flags;
	DBusGConnection *system_connection;
	DBusGConnection *session_connection;
	gboolean	 verbose = FALSE;
	gboolean	 no_daemon = FALSE;
	GpmManager      *manager = NULL;
	GError		*error = NULL;
	GOptionContext  *context;
 	GnomeProgram    *program;

	const GOptionEntry options[] = {
		{ "no-daemon", '\0', 0, G_OPTION_ARG_NONE, &no_daemon,
		  N_("Do not daemonize"), NULL },
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	context = g_option_context_new (_("GNOME Power Manager"));

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	program = gnome_program_init (argv[0], VERSION,
			   	      LIBGNOMEUI_MODULE, argc, argv,
			    	      GNOME_PROGRAM_STANDARD_PROPERTIES,
			    	      GNOME_PARAM_GOPTION_CONTEXT, context,
			    	      GNOME_PARAM_HUMAN_READABLE_NAME, GPM_NAME,
			    	      NULL);

	master = gnome_master_client ();
	flags = gnome_client_get_flags (master);

	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		/* We'll disable this as users are getting constant crashes */
		/* gnome_client_set_restart_style (master, GNOME_RESTART_IMMEDIATELY);*/
		gnome_client_flush (master);
	}

	g_signal_connect (GTK_OBJECT (master), "die", G_CALLBACK (gpm_exit), manager);

	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();

	gpm_debug_init (verbose);

	/* check dbus connections, exit if not valid */
	system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
		gpm_critical_error ("This program cannot start until you start the dbus "
				    "<i>system</i> service.\n"
				    "It is <b>strongly recommended</b> you reboot your compter "
				    "after starting messagebus.");
		/* abort at this point */
		exit (1);
	}

	session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
		gpm_critical_error ("This program cannot start until you start the "
				    "dbus <i>session</i> service.\n\n"
				    "This is usually started automatically in X "
				    "or gnome startup when you start a new session.");
		/* abort at this point */
		exit (1);
	}

	if (! gpm_stock_icons_init()) {
		gpm_critical_error ("Cannot continue without stock icons");
	}

	if (! no_daemon && daemon (0, 0)) {
		gpm_critical_error ("Could not daemonize: %s", g_strerror (errno));
	}

	manager = gpm_manager_new ();

	if (!gpm_object_register (session_connection, G_OBJECT (manager))) {
		gpm_warning ("%s is already running in this session.", GPM_NAME);
		return 0;
	}

	dbus_g_object_type_install_info (GPM_TYPE_MANAGER, &dbus_glib_gpm_manager_object_info);

	loop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (loop);

	gpm_exit (manager);

	g_object_unref (program);
	g_option_context_free (context);

	return 0;
}
