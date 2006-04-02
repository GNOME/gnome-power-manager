/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "gpm-screensaver.h"
#include "gpm-debug.h"

static void     gpm_screensaver_class_init (GpmScreensaverClass *klass);
static void     gpm_screensaver_init       (GpmScreensaver      *screensaver);
static void     gpm_screensaver_finalize   (GObject		*object);

#define GPM_SCREENSAVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SCREENSAVER, GpmScreensaverPrivate))

#define GS_LISTENER_SERVICE	"org.gnome.ScreenSaver"
#define GS_LISTENER_PATH	"/org/gnome/ScreenSaver"
#define GS_LISTENER_INTERFACE	"org.gnome.ScreenSaver"

struct GpmScreensaverPrivate
{
	DBusGConnection		*session_connection;
	DBusGProxy		*gs_proxy;
	GConfClient		*gconf_client;
};

G_DEFINE_TYPE (GpmScreensaver, gpm_screensaver, G_TYPE_OBJECT)

/**
 * gpm_screensaver_class_init:
 * @klass: This screensaver class instance
 **/
static void
gpm_screensaver_class_init (GpmScreensaverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_screensaver_finalize;
	g_type_class_add_private (klass, sizeof (GpmScreensaverPrivate));
}

/**
 * gpm_screensaver_init:
 * @screensaver: This screensaver class instance
 **/
static void
gpm_screensaver_init (GpmScreensaver *screensaver)
{
	GError *error = NULL;
	screensaver->priv = GPM_SCREENSAVER_GET_PRIVATE (screensaver);

	screensaver->priv->gconf_client = gconf_client_get_default ();

	screensaver->priv->session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (! screensaver->priv->session_connection) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_critical_error ("Cannot connect to DBUS Session Daemon");
	}

	screensaver->priv->gs_proxy = dbus_g_proxy_new_for_name (screensaver->priv->session_connection,
							         GS_LISTENER_SERVICE,
							         GS_LISTENER_PATH,
							         GS_LISTENER_INTERFACE);

}

/**
 * gpm_screensaver_finalize:
 * @object: This screensaver class instance
 **/
static void
gpm_screensaver_finalize (GObject *object)
{
	GpmScreensaver *screensaver;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SCREENSAVER (object));

	screensaver = GPM_SCREENSAVER (object);
	screensaver->priv = GPM_SCREENSAVER_GET_PRIVATE (screensaver);

	g_object_unref (G_OBJECT (screensaver->priv->gs_proxy));
	g_object_unref (screensaver->priv->gconf_client);

	G_OBJECT_CLASS (gpm_screensaver_parent_class)->finalize (object);
}

/**
 * gpm_screensaver_new:
 * Return value: new GpmScreensaver instance.
 **/
GpmScreensaver *
gpm_screensaver_new (void)
{
	GpmScreensaver *screensaver;
	screensaver = g_object_new (GPM_TYPE_SCREENSAVER, NULL);
	return GPM_SCREENSAVER (screensaver);
}

/**
 * gpm_screensaver_lock_enabled:
 * @screensaver: This screensaver class instance
 * Return value: If gnome-screensaver is set to lock the screen on screensave
 **/
gboolean
gpm_screensaver_lock_enabled (GpmScreensaver *screensaver)
{
	return gconf_client_get_bool (screensaver->priv->gconf_client,
				      GS_PREF_LOCK_ENABLED, NULL);
}

/**
 * gpm_screensaver_lock_set:
 * @screensaver: This screensaver class instance
 * @lock: If gnome-screensaver should lock the screen on screensave
 **/
void
gpm_screensaver_lock_set (GpmScreensaver *screensaver, gboolean lock)
{
	gconf_client_set_bool (screensaver->priv->gconf_client,
			       GS_PREF_LOCK_ENABLED, lock, NULL);
}

/**
 * gpm_screensaver_lock
 * @screensaver: This screensaver class instance
 * Return value: Success value
 **/
gboolean
gpm_screensaver_lock (GpmScreensaver *screensaver)
{
	int sleepcount = 0;
	if (! gpm_screensaver_is_running (screensaver)) {
		gpm_debug ("Not locking, as gnome-screensaver not running");
		return FALSE;
	}

	gpm_debug ("doing gnome-screensaver lock");
	dbus_g_proxy_call_no_reply (screensaver->priv->gs_proxy, "Lock", G_TYPE_INVALID);

	/* When we send the Lock signal to g-ss it takes maybe a second
	   or so to fade the screen and lock. If we suspend mid fade then on
	   resume the X display is still present for a split second
	   (since fade is gamma) and as such it can leak information.
	   Instead we wait until g-ss reports running and thus blanked
	   solidly before we continue from the screensaver_lock action.
	   The interior of g-ss is async, so we cannot get the dbus method
	   to block until lock is complete. */
	while (! gpm_screensaver_is_running (screensaver)) {
		/* Sleep for 1/10s */
		g_usleep (1000 * 100);
		if (sleepcount++ > 50) {
			break;
		}
	}

	return TRUE;
}

/**
 * gpm_screensaver_enable_throttle:
 * @screensaver: This screensaver class instance
 * @enable: If we should disable CPU hungry screensavers
 * Return value: Success value.
 **/
gboolean
gpm_screensaver_enable_throttle (GpmScreensaver *screensaver, gboolean enable)
{
	GError *error = NULL;
	gboolean boolret = TRUE;

	gpm_debug ("setThrottleEnabled : %i", enable);
	if (!dbus_g_proxy_call (screensaver->priv->gs_proxy, "setThrottleEnabled", &error,
				G_TYPE_BOOLEAN, enable, G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_debug ("gnome-screensaver service is not running.");
		boolret = FALSE;
	}
	if (!boolret) {
		gpm_debug ("setThrottleEnabled failed");
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_screensaver_is_running:
 * @screensaver: This screensaver class instance
 * Return value: TRUE if gnome-screensaver is running
 **/
gboolean
gpm_screensaver_is_running (GpmScreensaver *screensaver)
{
	GError *error = NULL;
	gboolean boolret = TRUE;
	gboolean temp = TRUE;

	if (!dbus_g_proxy_call (screensaver->priv->gs_proxy, "getActive", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &temp, G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		boolret = FALSE;
	}
	return boolret;
}

/**
 * gpm_screensaver_poke:
 * @screensaver: This screensaver class instance
 *
 * Pokes GNOME Screensaver simulating hardware events. This displays the unlock
 * dialogue when we resume, so the user doesn't have to move the mouse or press
 * any key before the window comes up.
 **/
void
gpm_screensaver_poke (GpmScreensaver *screensaver)
{
	gpm_debug ("poke");
	dbus_g_proxy_call_no_reply (screensaver->priv->gs_proxy, "Poke", G_TYPE_INVALID);
}

/**
 * gpm_screensaver_get_idle:
 * @screensaver: This screensaver class instance
 * @time: The returned idle time, passed by ref
 * Return value: Success value.
 **/
gboolean
gpm_screensaver_get_idle (GpmScreensaver *screensaver, gint *time)
{
	GError *error = NULL;
	gboolean boolret = TRUE;

	if (!dbus_g_proxy_call (screensaver->priv->gs_proxy, "getActiveTime", &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, time, G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_debug ("gnome-screensaver service is not running.");
		boolret = FALSE;
	}

	if (!boolret) {
		gpm_debug ("get idle failed");
		return FALSE;
	}
	return TRUE;
}
