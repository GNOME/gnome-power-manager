/** @file	gpm-dbus-common.c
 *  @brief	Common GLIB DBUS routines
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module contains all the dbus checker functions, that make
 * some attempt to sanitise the dbus error codes. It also gives
 * output to the user in the event they cannot connect to the session
 * or system dbus connections.
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
 * @addtogroup	dbus		Integration with DBUS
 * @brief			DBUS service and client funcionality.
 *
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include "gpm_marshal.h"
#include "gpm-dbus-common.h"

/** Handle a glib error, freeing if needed.
 *  We echo to debug, as we don't want the typical user sending in bug reports.
 *  Use --verbose to view these warnings.
 */
gboolean
dbus_glib_error (GError *error)
{
	g_return_val_if_fail (error, FALSE);
	if (error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
		g_debug ("Caught remote method exception %s: %s",
					dbus_g_error_get_name (error),
					error->message);
	else
		g_debug ("Error: %s", error->message);
	g_error_free (error);
	return TRUE;
}

/** Gets the DBUS service
 *
 *  @param	connection	A valid DBUS connection
 *  @param	service		Service, e.g. org.gnome.random
 *  @return			Success value.
 */
gboolean
dbus_get_service (DBusGConnection *connection, const gchar *service)
{
	DBusGProxy *bus_proxy = NULL;
	GError *error = NULL;
	gboolean ret = TRUE;
	guint request_name_result;

	/* assertion checks */
	g_assert (connection);
	g_assert (service);

	bus_proxy = dbus_g_proxy_new_for_name (connection, 
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS);
/*
 * Add this define hack until we depend on DBUS 0.60, as the
 * define names (and meanings have changed)
 * should fix bug: http://bugzilla.gnome.org/show_bug.cgi?id=322435
 */
#ifdef DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT
	if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
		G_TYPE_STRING, service,
		G_TYPE_UINT, DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT,
		G_TYPE_INVALID,
		G_TYPE_UINT, &request_name_result,
		G_TYPE_INVALID)) {
		g_error ("Failed to acquire %s: %s", service, error->message);
		ret = FALSE;
	}
#else
	if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
		G_TYPE_STRING, service,
		G_TYPE_UINT, 0,
		G_TYPE_INVALID,
		G_TYPE_UINT, &request_name_result,
		G_TYPE_INVALID)) {
		g_error ("Failed to acquire %s: %s", service, error->message);
		ret = FALSE;
	}
#endif

	if (ret && request_name_result != 1 /* NEED_TO_FIND_VALUE */)
		ret = FALSE;

	/* free the bus_proxy */
	g_object_unref (G_OBJECT (bus_proxy));
	return ret;
}

/** Gets the dbus session service, informing user if not found
 *
 * Initialize session DBUS conection - this *might* fail as it's 
 * potentially the first time the user will use this functionality.
 * If so, tell them how to fix the issue.
 *
 *  @param	connection	A valid DBUS connection, passed by ref
 *  @return			Success value.
 */
gboolean
dbus_get_session_connection (DBusGConnection **connection)
{
	GError *error = NULL;
	*connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (*connection == NULL) {
		g_warning ("Failed to open connection to dbus session bus: %s\n",
			error->message);
		dbus_glib_error (error);
		g_print ("This program cannot start until you start the dbus"
			 "session daemon\n"
			 "This is usually started in X or gnome startup "
			 "(depending on distro)\n"
			 "You can launch the session dbus-daemon manually with"
			 "this command:\n"
			 "eval `dbus-launch --auto-syntax`\n\n"
			 "If this works, add \"dbus-lauch --auto-syntax\" "
			 "to ~/.xinitrc\n\n");
		return FALSE;
	}
	return TRUE;
}

/** Gets the dbus session service, informing user if not found
 *
 * This *shouldn't* fail as HAL will not work without the system messagebus.
 *
 *  @param	connection	A valid DBUS connection, passed by ref
 *  @return			Success value.
 */
gboolean
dbus_get_system_connection (DBusGConnection **connection)
{
	GError *error = NULL;
	*connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (*connection == NULL) {
		g_warning ("Failed to open connection to dbus system bus: %s\n",
			error->message);
		dbus_glib_error (error);
		g_print ("This program cannot start until you start the dbus"
			 "system daemon\n"
			 "This is usually started in initscripts, and is "
			 "usually called messagebus\n"
			 "It is STRONGLY recommended you reboot your compter"
			 "after restarting messagebus\n\n");
		return FALSE;
	}
	return TRUE;
}

/** Finds out if the service name is registered on the system bus
 *
 *  @param	service_name	The name of the dbus service to check
 *				e.g. "org.freedesktop.NetworkManager"
 *  @return			TRUE if the service is registed with the
 *				system bus.
 */
gboolean
dbus_is_active_service (const gchar *service_name)
{
	GError *error = NULL;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *dbus_proxy = NULL;
	gboolean isOnBus = FALSE;
	gchar **service_list;
	gint i = 0;

	g_debug ("dbus_is_active_service");
	if (!dbus_get_system_connection (&system_connection))
		return FALSE;
	dbus_proxy = dbus_g_proxy_new_for_name (system_connection,
			DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	if (!dbus_proxy) {
		g_warning ("Failed to get name owner");
		return FALSE;
	}

	if (!dbus_g_proxy_call (dbus_proxy, "ListNames", &error,
				G_TYPE_INVALID,
				G_TYPE_STRV, &service_list,
				G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning ("DBUS service is not running.");
		return FALSE;
	}

	/* Print the results */
 	while (service_list[i]) {
		if (strcmp (service_list[i], service_name) == 0) {
			isOnBus = TRUE;
			/* no point continuing to search */
			break;
		}
		/* only print if named */
		if (service_list[i][0] != ':')
			g_debug ("found : %s", service_list[i]);
		++i;
	}
	/* free list */
	g_strfreev (service_list);
	g_object_unref (G_OBJECT (dbus_proxy));
	return isOnBus;
}

/** @} */
