/***************************************************************************
 *
 * gpm-screensaver.c : GLIB replacement for libhal, the extra stuff
 *
 * This module deals with communicating through DBUS to 
 * GNOME Screensaver.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "dbus-common.h"
#include "glibhal-main.h"
#include "gpm-screensaver.h"

/** Sets the DPMS timeout to a known value
 *
 *  @param timeout		Timeout in minutes
 */
gboolean
gscreensaver_set_dpms_timeout (gint timeout)
{
	GConfClient *client;

	if (timeout < 0 || timeout > 10 * 60 * 60)
		return FALSE;
	g_debug ("Adjusting gnome-screensaver dpms_suspend value to %i.", timeout);
	client = gconf_client_get_default ();
	gconf_client_set_int (client, GS_GCONF_ROOT "dpms_suspend", timeout, NULL);
	return TRUE;
}

/** If set to lock on screensave, instruct gnome-screensaver to lock screen
 *  and return TRUE.
 *  if set not to lock, then do nothing, and return FALSE.
 */
gboolean
gscreensaver_lock_check (void)
{
	GConfClient *client = gconf_client_get_default ();
	gboolean should_lock;

	should_lock = gconf_client_get_bool (client, GS_GCONF_ROOT "lock", NULL);
	if (!should_lock)
		return FALSE;
	gscreensaver_lock ();
	return TRUE;
}

/** Finds out if gnome-screensaver is running
 *
 *  @return		TRUE for success, FALSE for failure
 */
gboolean
gscreensaver_is_running (void)
{
	GError *error = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gboolean boolret = TRUE;
	gboolean temp = TRUE;

	if (!dbus_get_session_connection (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
			GS_LISTENER_SERVICE,
			GS_LISTENER_PATH,
			GS_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (gs_proxy, "getActive", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &temp, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (gs_proxy));
	return boolret;
}

/** If set to lock on screensave, instruct gnome-screensaver to lock screen
 *  and return TRUE.
 *  if set not to lock, then do nothing, and return FALSE.
 */
gboolean
gscreensaver_set_throttle (gboolean throttle)
{
	GError *error = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gboolean boolret = TRUE;

	g_debug ("gnome-screensaver setThrottleEnabled : %i", throttle);
	if (!dbus_get_session_connection (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
			GS_LISTENER_SERVICE,
			GS_LISTENER_PATH,
			GS_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (gs_proxy, "setThrottleEnabled", &error,
				G_TYPE_BOOLEAN, throttle, G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_debug ("gnome-screensaver service is not running.");
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (gs_proxy));
	if (!boolret) {
		g_debug ("gnome-screensaver setThrottleEnabled failed");
		return FALSE;
	}
	return TRUE;
}

/** If set to lock on screensave, instruct gnome-screensaver to lock screen
 *  and return TRUE.
 *  if set not to lock, then do nothing, and return FALSE.
 */
gboolean
gscreensaver_lock (void)
{
	GError *error = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gboolean boolret;

	g_debug ("gnome-screensaver lock");
	if (!dbus_get_session_connection (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
			GS_LISTENER_SERVICE,
			GS_LISTENER_PATH,
			GS_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (gs_proxy, "lock", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &boolret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_debug ("gnome-screensaver service is not running.");
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (gs_proxy));
	if (!boolret) {
		g_debug ("gnome-screensaver lock failed");
		return FALSE;
	}
	return TRUE;
}

/** If set to lock on screensave, instruct gnome-screensaver to lock screen
 *  and return TRUE.
 *  if set not to lock, then do nothing, and return FALSE.
 */
gboolean
gscreensaver_get_idle (gint *time)
{
	GError *error = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gboolean boolret = TRUE;

	if (!dbus_get_session_connection (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
			GS_LISTENER_SERVICE,
			GS_LISTENER_PATH,
			GS_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (gs_proxy, "getIdleTime", &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, time, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_debug ("gnome-screensaver service is not running.");
		boolret = FALSE;
	}
	g_print ("gscreensaver_get_idle: %i\n", *time);
	g_object_unref (G_OBJECT (gs_proxy));
	if (!boolret) {
		g_debug ("gnome-screensaver get idle failed");
		return FALSE;
	}
	return TRUE;
}
