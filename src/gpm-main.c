/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/** @file	gpm-main.c
 *  @brief	GNOME Power Manager session daemon
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *  @note	Taken in part from:
 *  @note	- lshal   (C) 2003 David Zeuthen, <david@fubar.dk>
 *  @note	- notibat (C) 2004 Benjamin Kahn, <xkahn@zoned.net>
 *
 * This is the main daemon for g-p-m. It handles all the setup and
 * tear-down of all the dynamic arrays, mainloops and icons in g-p-m.
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
/**
 * @addtogroup	main		GNOME Power Manager (session daemon)
 * @brief			The session daemon run for each user
 *
 * @{
 */
/** @mainpage	GNOME Power Manager
 *
 *  @section	intro		Introduction
 *
 *  GNOME Power Manager is a session daemon that takes care of power management.
 *
 *  GNOME Power Manager uses information provided by HAL to display icons and
 *  handle system and user actions in a GNOME session. Authorised users can set
 *  policy and change preferences.
 *  GNOME Power Manager acts as a policy agent on top of the Project Utopia
 *  stack, which includes the kernel, hotplug, udev, and HAL.
 *  GNOME Power Manager listens for HAL events and responds with
 *  user-configurable reactions.
 *  The main focus is the user interface; e.g. allowing configuration of
 *  power management from the desktop in a sane way (no need for root password,
 *  and no need to edit configuration files)
 *  Most of the backend code is actually in HAL for abstracting various power
 *  aware devices (UPS's) and frameworks (ACPI, PMU, APM etc.) - so the
 *  desktop parts are fairly lightweight and straightforward.
 */

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <popt.h>

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <glade/glade.h>

#include "gpm-dbus-server.h"
#include "gpm-dbus-common.h"
#include "gpm-dbus-signal-handler.h"
#include "gpm-stock-icons.h"
#include "gpm-hal.h"

#include "gnome-power-glue.h"

#include "gpm-manager.h"
#include "gpm-main.h"

static void
gpm_main_log_dummy (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
}

/** Generic exit
 *
 */
static void
gpm_exit (void)
{
	g_debug ("Quitting!");

	gpm_stock_icons_shutdown ();
	gpm_dbus_remove_noc ();
	gpm_dbus_remove_nlost ();

	exit (0);
}

/** Callback for the DBUS NameOwnerChanged function.
 *
 *  @param	name		The DBUS name, e.g. org.freedesktop.Hal
 *  @param	connected	Time in minutes that computer has been idle
 */
static void
signalhandler_noc (const char *name, const gboolean connected)
{
	g_debug ("signalhandler_noc: (%i) %s", connected, name);
	/* ignore that don't all apply */
	if (strcmp (name, HAL_DBUS_SERVICE) != 0)
		return;

	if (!connected) {
		g_warning ("HAL has been disconnected! GNOME Power Manager will now quit.");

		/* Wait for HAL to be running again */
		while (!gpm_hal_is_running ()) {
			g_warning ("GNOME Power Manager cannot connect to HAL!");
			g_usleep (1000*500);
		}
		/* for now, quit */
		gpm_exit ();
		return;
	}
	/** @todo: handle reconnection to the HAL bus */
	g_warning ("hal re-connected\n");
}

/** Callback for the DBUS NameLost function.
 *
 *  @param	name		The DBUS name, e.g. org.freedesktop.Hal
 *  @param	connected	Always true.
 */
static void
signalhandler_nlost (const char *name, const gboolean connected)
{
	if (strcmp (name, GPM_DBUS_SERVICE) != 0)
		return;
	gpm_exit ();
}

/** Main entry point
 *
 *  @param	argc		Number of arguments given to program
 *  @param	argv		Arguments given to program
 *  @return			Return code
 */
int
main (int argc, char *argv[])
{
	gint i;
	GMainLoop *loop;
	GnomeClient *master;
	GnomeClientFlags flags;
	DBusGConnection *system_connection;
	DBusGConnection *session_connection;
	gboolean verbose = FALSE;
	gboolean no_daemon;
	GpmManager *manager;

	struct poptOption options[] = {
		{ "no-daemon", '\0', POPT_ARG_NONE, NULL, 0,
		  N_("Do not daemonize"), NULL },
		{ "verbose", '\0', POPT_ARG_NONE, NULL, 0,
		  N_("Show extra debugging information"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	i = 0;
	options[i++].arg = &no_daemon;
	options[i++].arg = &verbose;

	no_daemon = FALSE;

	gnome_program_init (argv[0], VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("GNOME Power Manager"),
			    NULL);

	if (!verbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, gpm_main_log_dummy, NULL);

	master = gnome_master_client ();
	flags = gnome_client_get_flags (master);

	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		/* We'll disable this as users are getting constant crashes */
		/* gnome_client_set_restart_style (master, GNOME_RESTART_IMMEDIATELY);*/
		gnome_client_flush (master);
	}

	g_signal_connect (GTK_OBJECT (master), "die", G_CALLBACK (gpm_exit), NULL);

	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* check dbus connections, exit if not valid */
	if (!gpm_dbus_get_system_connection (&system_connection))
		g_error ("Unable to get system dbus connection");
	if (!gpm_dbus_get_session_connection (&session_connection))
		g_error ("Unable to get session dbus connection");

	if (!gpm_stock_icons_init())
		g_error ("Cannot continue without stock icons");

	if (!no_daemon && daemon (0, 0))
		g_error ("Could not daemonize: %s", g_strerror (errno));

	/* install gpm object */
	dbus_g_object_type_install_info (gpm_object_get_type (),
					 &dbus_glib_gpm_object_object_info);

#if GPM_SYSTEM_BUS
	if (!gpm_object_register (system_connection)) {
		g_warning ("Failed to register.");
		return 0;
	}
#else
	if (!gpm_object_register (session_connection)) {
		g_warning ("GNOME Power Manager is already running in this session.");
		return 0;
	}
#endif

	/* initialise NameOwnerChanged and NameLost */
	gpm_dbus_init_noc (system_connection, signalhandler_noc);
	gpm_dbus_init_nlost (system_connection, signalhandler_nlost);

	loop = g_main_loop_new (NULL, FALSE);

	manager = gpm_manager_new ();

	g_main_loop_run (loop);

	g_object_unref (manager);

	gpm_exit ();

	return 0;
}
/** @} */
