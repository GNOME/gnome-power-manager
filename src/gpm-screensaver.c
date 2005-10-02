/*! @file	gpm-screensaver.c
 *  @brief	Functions to query and control GNOME Screensaver
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module deals with communicating through DBUS and gconf to 
 * GNOME Screensaver.
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
 *  @param	timeout		Timeout in minutes
 *  @return			TRUE if timeout was valid
 */
gboolean
gscreensaver_set_dpms_timeout (gint timeout)
{
	GConfClient *client;

	if (timeout < 0 || timeout > 10 * 60 * 60)
		return FALSE;
	g_debug ("Adjusting gnome-screensaver dpms_suspend to %i.", timeout);
	client = gconf_client_get_default ();
	gconf_client_set_int (client, GS_GCONF_ROOT "dpms_suspend", timeout, NULL);
	return TRUE;
}

/** If set to lock on screensave, do so.
 *
 * Instruct gnome-screensaver to lock screen if set gconf, if not to lock,
 * then do nothing, and return FALSE.
 *
 *  @return			TRUE if we locked the screen
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
 *  @return			TRUE if gnome-screensaver is running
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

/** Sets the throttle for gnome-screensaver
 *
 *  @param	throttle	If we should disable CPU hungry screensavers
 *  @return			TRUE if gnome-screensaver changed its status.
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

/** Lock the screen using GNOME Screensaver
 *
 *  @return			TRUE if gnome-screensaver locked the screen.
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

/** Lock the screen using GNOME Screensaver
 *
 *  @param	time		The returned idle time, passed by ref.
 *  @return			TRUE if we got a valid idle time.
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
