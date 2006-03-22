/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "gpm-screensaver.h"
#include "gpm-debug.h"

#define GS_LISTENER_SERVICE	"org.gnome.ScreenSaver"
#define GS_LISTENER_PATH	"/org/gnome/ScreenSaver"
#define GS_LISTENER_INTERFACE	"org.gnome.ScreenSaver"

static gboolean
gpm_screensaver_get_session_conn (DBusGConnection **connection)
{
	GError *error = NULL;
	DBusGConnection *session_conn = NULL;

	/* else get from DBUS */
	session_conn = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!session_conn) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		return FALSE;
	}
	*connection = session_conn;
	return TRUE;
}

/** Should we lock the the screen
 *
 *  @return			TRUE if we should lock the screen
 */
gboolean
gpm_screensaver_lock_enabled (void)
{
	return gconf_client_get_bool (gconf_client_get_default (), GS_PREF_LOCK_ENABLED, NULL);
}

/** Set the lock for the screensave
 *
 *  @param	lock		The lock value to set
 */
gboolean
gpm_screensaver_lock_set (gboolean lock)
{
	return gconf_client_set_bool (gconf_client_get_default (), GS_PREF_LOCK_ENABLED, lock, NULL);
}

/** Finds out if gnome-screensaver is running
 *
 *  @return			TRUE if gnome-screensaver is running
 */
gboolean
gpm_screensaver_is_running (void)
{
	GError *error = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gboolean boolret = TRUE;
	gboolean temp = TRUE;

	if (!gpm_screensaver_get_session_conn (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
					      GS_LISTENER_SERVICE,
					      GS_LISTENER_PATH,
					      GS_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (gs_proxy, "getActive", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &temp, G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (gs_proxy));
	return boolret;
}

/** Sets the throttle for gnome-screensaver
 *
 *  @param	enable		If we should disable CPU hungry screensavers
 *  @return			TRUE if gnome-screensaver changed its status.
 */
gboolean
gpm_screensaver_enable_throttle (gboolean enable)
{
	GError *error = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gboolean boolret = TRUE;

	gpm_debug ("setThrottleEnabled : %i", enable);
	if (!gpm_screensaver_get_session_conn (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
					      GS_LISTENER_SERVICE,
					      GS_LISTENER_PATH,
					      GS_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (gs_proxy, "setThrottleEnabled", &error,
				G_TYPE_BOOLEAN, enable, G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_debug ("gnome-screensaver service is not running.");
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (gs_proxy));
	if (!boolret) {
		gpm_debug ("setThrottleEnabled failed");
		return FALSE;
	}
	return TRUE;
}

/** Lock the screen using GNOME Screensaver
 *
 *  @return			TRUE if gnome-screensaver locked the screen.
 */
gboolean
gpm_screensaver_lock (void)
{
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	int sleepcount = 0;

	if (! gpm_screensaver_is_running ()) {
		gpm_debug ("Not locking, as gnome-screensaver not running");
		return FALSE;
	}

	gpm_debug ("doing gnome-screensaver lock");

	if (!gpm_screensaver_get_session_conn (&session_connection)) {
		gpm_debug ("Not locking, no session connection for screensaver");
		return FALSE;
	}
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
					      GS_LISTENER_SERVICE,
					      GS_LISTENER_PATH,
					      GS_LISTENER_INTERFACE);
	dbus_g_proxy_call_no_reply (gs_proxy, "Lock", G_TYPE_INVALID);
	g_object_unref (G_OBJECT (gs_proxy));

	/* When we send the Lock signal to g-ss it takes maybe a second
	   or so to fade the screen and lock. If we suspend mid fade then on
	   resume the X display is still present for a split second
	   (since fade is gamma) and as such it can leak information.
	   Instead we wait until g-ss reports running and thus blanked
	   solidly before we continue from the screensaver_lock action.
	   The interior of g-ss is async, so we cannot get the dbus method
	   to block until lock is complete. */
	while (! gpm_screensaver_is_running ()) {
		/* Sleep for 1/10s */
		g_usleep (1000 * 100);
		if (sleepcount++ > 50) {
			break;
		}
	}

	return TRUE;
}

/** Pokes GNOME Screensaver (displays the unlock dialogue, so the user doesn't
 *  have to move the mouse or press any key.
 *
 *  @return			TRUE if gnome-screensaver locked the screen.
 */
gboolean
gpm_screensaver_poke (void)
{
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gpm_debug ("poke");
	if (!gpm_screensaver_get_session_conn (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
					      GS_LISTENER_SERVICE,
					      GS_LISTENER_PATH,
					      GS_LISTENER_INTERFACE);
	dbus_g_proxy_call_no_reply (gs_proxy, "Poke", G_TYPE_INVALID);
	g_object_unref (G_OBJECT (gs_proxy));
	return TRUE;
}

/** Lock the screen using GNOME Screensaver
 *
 *  @param	time		The returned idle time, passed by ref.
 *  @return			TRUE if we got a valid idle time.
 */
gboolean
gpm_screensaver_get_idle (gint *time)
{
	GError *error = NULL;
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gs_proxy = NULL;
	gboolean boolret = TRUE;

	if (!gpm_screensaver_get_session_conn (&session_connection))
		return FALSE;
	gs_proxy = dbus_g_proxy_new_for_name (session_connection,
					      GS_LISTENER_SERVICE,
					      GS_LISTENER_PATH,
					      GS_LISTENER_INTERFACE);
	if (!dbus_g_proxy_call (gs_proxy, "getActiveTime", &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, time, G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_debug ("gnome-screensaver service is not running.");
		boolret = FALSE;
	}
	g_object_unref (G_OBJECT (gs_proxy));
	if (!boolret) {
		gpm_debug ("get idle failed");
		return FALSE;
	}
	return TRUE;
}
