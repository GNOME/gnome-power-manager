/** @file	gpm-networkmanager.c
 *  @brief	Functions to query and control NetworkManager
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-17
 *
 * This module deals with communicating through DBUS to NetworkManager.
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
 * @addtogroup	nm		NetworkManager integration
 * @brief	nm		integration into GNOME Power Manager using DBUS
 *
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "gpm-dbus-common.h"
#include "glibhal-main.h"
#include "gpm-networkmanager.h"

/** Lock the screen using GNOME Screensaver
 *
 *  @return			TRUE if gnome-screensaver locked the screen.
 */
gboolean
gpm_networkmanager_sleep (void)
{
	GError *error = NULL;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *nm_proxy = NULL;
	gboolean boolret;

	g_debug ("NetworkManager sleep");
	if (!dbus_get_system_connection (&system_connection))
		return FALSE;
	nm_proxy = dbus_g_proxy_new_for_name (system_connection,
			NM_LISTENER_SERVICE,
			NM_LISTENER_PATH,
			NM_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (nm_proxy, "sleep", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_debug ("NetworkManager service is not running.");
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (nm_proxy));
	if (!boolret) {
		g_debug ("NetworkManager sleep failed");
		return FALSE;
	}
	return TRUE;
}

gboolean
gpm_networkmanager_wake (void)
{
	GError *error = NULL;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *nm_proxy = NULL;
	gboolean boolret;

	g_debug ("NetworkManager wake");
	if (!dbus_get_system_connection (&system_connection))
		return FALSE;
	nm_proxy = dbus_g_proxy_new_for_name (system_connection,
			NM_LISTENER_SERVICE,
			NM_LISTENER_PATH,
			NM_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (nm_proxy, "wake", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_debug ("NetworkManager service is not running.");
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (nm_proxy));
	if (!boolret) {
		g_debug ("NetworkManager wake failed");
		return FALSE;
	}
	return TRUE;
}

/** @} */
