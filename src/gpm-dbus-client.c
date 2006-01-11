/** @file	gpm-dbus-client.c
 *  @brief	Common DBUS client stuff for g-p-m and g-p-p
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-04
 *
 * This module is used to query properties from g-p-m such as
 * isOnAc or isOnBattery
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
 * @addtogroup	dbus
 * @{
 */

#include <glib.h>
#include <dbus/dbus-glib.h>

#include "gpm-common.h"
#include "gpm-manager.h"
#include "gpm-dbus-client.h"

/** Finds out if we are running on AC
 *
 *  @param	value		The return value, passed by ref
 *  @return			Success
 */
gboolean
gpm_is_on_ac (gboolean *value)
{
	DBusGConnection *connection = NULL;
	DBusGProxy *gpm_proxy = NULL;
	GError *error = NULL;
	gboolean retval = TRUE;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		g_warning ("gpm_is_on_ac: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	gpm_proxy = dbus_g_proxy_new_for_name (connection,
			GPM_DBUS_SERVICE,
			GPM_DBUS_PATH,
			GPM_DBUS_INTERFACE);
	if (!dbus_g_proxy_call (gpm_proxy, "isOnAc", &error,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, value, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("gpm_is_on_ac: %s", error->message);
			g_error_free (error);
		}
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (gpm_proxy));
	return retval;
}

/** Queries org.gnome.PowerManager.isOnBattery
 *
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 */
gboolean
gpm_is_on_mains (gboolean *value)
{
	DBusGConnection *connection = NULL;
	DBusGProxy *gpm_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		g_warning ("gpm_is_on_mains: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	gpm_proxy = dbus_g_proxy_new_for_name (connection,
			GPM_DBUS_SERVICE, GPM_DBUS_PATH, GPM_DBUS_INTERFACE);
	retval = TRUE;
	if (!dbus_g_proxy_call (gpm_proxy, "isOnBattery", &error,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, value, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("gpm_is_on_mains: %s", error->message);
			g_error_free (error);
		}
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (gpm_proxy));
	return retval;
}
/** @} */
