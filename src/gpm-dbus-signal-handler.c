/** @file	gpm-dbus-signal-handler.c
 *  @brief	DBUS signal handler for NameOwnerChanged and NameLost
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-12-23
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
#include "gpm-dbus-common.h"
#include "gpm-dbus-signal-handler.h"

static DBusGProxy *proxy_bus_nlost = NULL;
static DBusGProxy *proxy_bus_noc = NULL;
static GpmDbusSignalHandler gpm_sig_handler_noc;
static GpmDbusSignalHandler gpm_sig_handler_nlost;

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
	g_debug ("gpm_signal_handler_noc name=%s, prev=%s, new=%s", name, prev, new);

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
	g_debug ("gpm_signal_handler_nlost name=%s", name);
	gpm_sig_handler_nlost (name, TRUE);
}

/** NameOwnerChanged callback assignment
 *
 *  @param	callback	The callback
 *  @return			If we assigned the callback okay
 */
gboolean
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
gboolean
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
gboolean
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
gboolean
gpm_dbus_remove_nlost (void)
{
	g_assert (proxy_bus_nlost);
	g_object_unref (G_OBJECT (proxy_bus_nlost));
	proxy_bus_nlost = NULL;
	return TRUE;
}

/** @} */
