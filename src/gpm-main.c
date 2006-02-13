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
#include <popt.h>

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

typedef void (*GpmDbusSignalHandler) (const gchar *name, const gboolean connected);
static DBusGProxy *proxy_bus_nlost = NULL;
static DBusGProxy *proxy_bus_noc = NULL;
static GpmDbusSignalHandler gpm_sig_handler_noc;
static GpmDbusSignalHandler gpm_sig_handler_nlost;

static void gpm_exit (void);

/** Callback for the DBUS NameOwnerChanged function.
 *
 *  @param	name		The DBUS name, e.g. org.freedesktop.Hal
 *  @param	connected	Time in minutes that computer has been idle
 */
static void
signalhandler_noc (const char *name, const gboolean connected)
{
	/* ignore that don't all apply */
	if (strcmp (name, HAL_DBUS_SERVICE) != 0)
		return;

	if (!connected) {
		gpm_warning ("HAL has been disconnected! %s will now quit.", GPM_NAME);
		/* for now, quit */
		gpm_exit ();
		return;
	}
	/** @todo: handle reconnection to the HAL bus */
	gpm_warning ("hal re-connected\n");
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

/** NameOwnerChanged signal handler
 *
 *  @param	proxy		A valid DBUS Proxy
 *  @param	name		The session name, e.g. org.gnome.test
 *  @param	prev		The previous name
 *  @param	new		The new name
 *  @param	user_data	Unused
 */
static void
gpm_signal_handler_noc (DBusGProxy *proxy,
	const char *name,
	const char *prev,
	const char *new,
	gpointer user_data)
{
	if (strlen (new) == 0)
		gpm_sig_handler_noc (name, FALSE);
	else if (strlen (prev) == 0)
		gpm_sig_handler_noc (name, TRUE);
}

/** NameLost signal handler
 *
 *  @param	proxy		A valid DBUS Proxy
 *  @param	name		The Condition name, e.g. org.gnome.test
 *  @param	user_data	Unused
 */
static void
gpm_signal_handler_nlost (DBusGProxy *proxy, const char *name, gpointer user_data)
{
	gpm_debug ("name=%s", name);
	gpm_sig_handler_nlost (name, TRUE);
}

/** NameOwnerChanged callback assignment
 *
 *  @param	callback	The callback
 *  @return			If we assigned the callback okay
 */
static gboolean
gpm_dbus_init_noc (DBusGConnection *connection, GpmDbusSignalHandler callback)
{
	GError *error = NULL;
	g_assert (!proxy_bus_noc);

	/* assign callback */
	gpm_sig_handler_noc = callback;

	/* get NameOwnerChanged proxy */
	proxy_bus_noc = dbus_g_proxy_new_for_name_owner (connection,
						         DBUS_SERVICE_DBUS,
						         DBUS_PATH_DBUS,
						         DBUS_INTERFACE_DBUS,
						         &error);
	dbus_g_proxy_add_signal (proxy_bus_noc, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy_bus_noc, "NameOwnerChanged",
				     G_CALLBACK (gpm_signal_handler_noc),
				     NULL, NULL);
	return TRUE;
}

/** NameLost callback assignment
 *
 *  @param	callback	The callback
 *  @return			If we assigned the callback okay
 */
static gboolean
gpm_dbus_init_nlost (DBusGConnection *connection, GpmDbusSignalHandler callback)
{
	GError *error = NULL;
	g_assert (!proxy_bus_nlost);

	/* assign callback */
	gpm_sig_handler_nlost = callback;

	proxy_bus_nlost = dbus_g_proxy_new_for_name_owner (connection,
							   DBUS_SERVICE_DBUS,
							   DBUS_PATH_DBUS,
							   DBUS_INTERFACE_DBUS,
							   &error);
	dbus_g_proxy_add_signal (proxy_bus_nlost, "NameLost",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy_bus_nlost, "NameLost",
				     G_CALLBACK (gpm_signal_handler_nlost),
				     NULL, NULL);
	return TRUE;
}

/** NameOwnerChanged callback removal
 *
 *  @return			Success
 */
static gboolean
gpm_dbus_remove_noc (void)
{
	g_assert (proxy_bus_noc);
	g_object_unref (G_OBJECT (proxy_bus_noc));
	proxy_bus_noc = NULL;
	return TRUE;
}

/** NameLost callback removal
 *
 *  @return			Success
 */
static gboolean
gpm_dbus_remove_nlost (void)
{
	g_assert (proxy_bus_nlost);
	g_object_unref (G_OBJECT (proxy_bus_nlost));
	proxy_bus_nlost = NULL;
	return TRUE;
}

/** registers org.gnome.PowerManager on a connection
 *
 *  @return			If we successfully registered the object
 *
 *  @note	This function MUST be called before DBUS service will work.
 */
static gboolean
gpm_object_register (DBusGConnection *connection,
		     GObject         *object)
{
	DBusGProxy *bus_proxy = NULL;
	GError *error = NULL;
	guint request_name_result;

	bus_proxy = dbus_g_proxy_new_for_name (connection,
					       DBUS_SERVICE_DBUS,
					       DBUS_PATH_DBUS,
					       DBUS_INTERFACE_DBUS);
/*
 * Add this define hack until we depend on DBUS 0.60, as the
 * define names have changed.
 * should fix bug: http://bugzilla.gnome.org/show_bug.cgi?id=322435
 */
#ifdef DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT
	/* add the legacy stuff for FC4 */
	if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
		G_TYPE_STRING, GPM_DBUS_SERVICE,
		G_TYPE_UINT, DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT,
		G_TYPE_INVALID,
		G_TYPE_UINT, &request_name_result,
		G_TYPE_INVALID)) {
		g_error ("Failed to acquire %s: %s", GPM_DBUS_SERVICE, error->message);
		return FALSE;
	}
#else
	if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
		G_TYPE_STRING, GPM_DBUS_SERVICE,
		G_TYPE_UINT, 0,
		G_TYPE_INVALID,
		G_TYPE_UINT, &request_name_result,
		G_TYPE_INVALID)) {
		g_error ("Failed to acquire %s: %s", GPM_DBUS_SERVICE, error->message);
		return FALSE;
	}
#endif

 	if (request_name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		return FALSE;

	/* free the bus_proxy */
	g_object_unref (G_OBJECT (bus_proxy));

	dbus_g_object_type_install_info (GPM_TYPE_MANAGER, &dbus_glib_gpm_manager_object_info);
	dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH, object);

	return TRUE;
}

static void
gpm_exit (void)
{
	gpm_stock_icons_shutdown ();
	gpm_dbus_remove_noc ();
	gpm_dbus_remove_nlost ();

	gpm_debug_shutdown ();

	exit (0);
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
	gint             i;
	GMainLoop       *loop;
	GnomeClient     *master;
	GnomeClientFlags flags;
	DBusGConnection *system_connection;
	DBusGConnection *session_connection;
	gboolean         verbose = FALSE;
	gboolean         no_daemon;
	GpmManager      *manager;
	GError          *error = NULL;

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
			    GNOME_PARAM_HUMAN_READABLE_NAME, GPM_NAME,
			    NULL);

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

	gpm_debug_init (verbose, ! no_daemon);

	/* check dbus connections, exit if not valid */
	system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
		/* abort at this point */
		g_error ("This program cannot start until you start the dbus"
			 "system daemon\n"
			 "This is usually started in initscripts, and is "
			 "usually called messagebus\n"
			 "It is STRONGLY recommended you reboot your compter"
			 "after restarting messagebus\n\n");
	}

	session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
		/* abort at this point */
		g_error ("This program cannot start until you start the dbus session daemon\n"
			 "This is usually started in X or gnome startup "
			 "(depending on distro)\n"
			 "You can launch the session dbus-daemon manually with this command:\n"
			 "eval `dbus-launch --auto-syntax`\n");
	}

	if (! gpm_stock_icons_init())
		g_error ("Cannot continue without stock icons");

	if (! no_daemon && daemon (0, 0))
		g_error ("Could not daemonize: %s", g_strerror (errno));

	manager = gpm_manager_new ();

	if (!gpm_object_register (session_connection, G_OBJECT (manager))) {
		gpm_warning ("%s is already running in this session.", GPM_NAME);
		return 0;
	}

	dbus_g_object_type_install_info (GPM_TYPE_MANAGER, &dbus_glib_gpm_manager_object_info);

	/* initialise NameOwnerChanged and NameLost */
	gpm_dbus_init_noc (system_connection, signalhandler_noc);
	gpm_dbus_init_nlost (system_connection, signalhandler_nlost);

	loop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (loop);

	g_object_unref (manager);

	gpm_exit ();

	return 0;
}
/** @} */
