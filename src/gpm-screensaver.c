/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gconf/gconf-client.h>

#include "gpm-screensaver.h"
#include "gpm-common.h"
#include "egg-debug.h"

static void     gpm_screensaver_finalize   (GObject		*object);

#define GPM_SCREENSAVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SCREENSAVER, GpmScreensaverPrivate))

#define GS_LISTENER_SERVICE	"org.gnome.ScreenSaver"
#define GS_LISTENER_PATH	"/"
#define GS_LISTENER_INTERFACE	"org.gnome.ScreenSaver"

struct GpmScreensaverPrivate
{
	GDBusProxy		*proxy;
	GConfClient		*conf;
};

enum {
	AUTH_REQUEST,
	LAST_SIGNAL
};

static gpointer gpm_screensaver_object = NULL;

G_DEFINE_TYPE (GpmScreensaver, gpm_screensaver, G_TYPE_OBJECT)

/**
 * gpm_screensaver_lock_enabled:
 * @screensaver: This class instance
 * Return value: If gnome-screensaver is set to lock the screen on screensave
 **/
gboolean
gpm_screensaver_lock_enabled (GpmScreensaver *screensaver)
{
	gboolean enabled;
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	enabled = gconf_client_get_bool (screensaver->priv->conf, GS_CONF_PREF_LOCK_ENABLED, NULL);
	return enabled;
}

/**
 * gpm_screensaver_lock
 * @screensaver: This class instance
 * Return value: Success value
 **/
gboolean
gpm_screensaver_lock (GpmScreensaver *screensaver)
{
	guint sleepcount = 0;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	egg_debug ("doing gnome-screensaver lock");
	g_dbus_proxy_call (screensaver->priv->proxy,
			   "Lock",
			   NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);

	/* When we send the Lock signal to g-ss it takes maybe a second
	   or so to fade the screen and lock. If we suspend mid fade then on
	   resume the X display is still present for a split second
	   (since fade is gamma) and as such it can leak information.
	   Instead we wait until g-ss reports running and thus blanked
	   solidly before we continue from the screensaver_lock action.
	   The interior of g-ss is async, so we cannot get the dbus method
	   to block until lock is complete. */
	while (! gpm_screensaver_check_running (screensaver)) {
		/* Sleep for 1/10s */
		g_usleep (1000 * 100);
		if (sleepcount++ > 50) {
			egg_debug ("timeout waiting for gnome-screensaver");
			break;
		}
	}

	return TRUE;
}

/**
 * gpm_screensaver_add_throttle:
 * @screensaver: This class instance
 * @reason: The reason for throttling
 * Return value: Success value, or zero for failure
 **/
guint
gpm_screensaver_add_throttle (GpmScreensaver *screensaver, const char *reason)
{
	GError *error = NULL;
	GVariant *retval = NULL;
	guint32 cookie = 0;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), 0);
	g_return_val_if_fail (reason != NULL, 0);

	if (screensaver->priv->proxy == NULL) {
		egg_warning ("not connected to the screensaver");
		goto out;
	}

	retval = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "Throttle",
					 g_variant_new ("(ss)",
							"Power screensaver",
							reason),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1, NULL,
					 &error);
	if (retval == NULL) {
		/* abort as the DBUS method failed */
		egg_warning ("Throttle failed: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_variant_get (retval, "(s)", &cookie);
	egg_debug ("adding throttle reason: '%s': id %u", reason, cookie);
out:
	if (retval != NULL)
		g_variant_unref (retval);
	return cookie;
}

/**
 * gpm_screensaver_remove_throttle:
 **/
gboolean
gpm_screensaver_remove_throttle (GpmScreensaver *screensaver, guint cookie)
{
	gboolean ret = FALSE;
	GVariant *retval = NULL;
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		egg_warning ("not connected to the screensaver");
		goto out;
	}

	egg_debug ("removing throttle: id %u", cookie);
	retval = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "UnThrottle",
					 g_variant_new ("(s)", cookie),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1, NULL,
					 &error);
	if (retval == NULL) {
		/* abort as the DBUS method failed */
		egg_warning ("UnThrottle failed!: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	/* success */
	ret = TRUE; 
out:
	if (retval != NULL)
		g_variant_unref (retval);
	return ret;
}

/**
 * gpm_screensaver_check_running:
 * @screensaver: This class instance
 * Return value: TRUE if gnome-screensaver is running
 **/
gboolean
gpm_screensaver_check_running (GpmScreensaver *screensaver)
{
	GVariant *retval = NULL;
	gboolean active = FALSE;
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		egg_warning ("not connected to screensaver");
		goto out;
	}

	retval = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "GetActive",
					 NULL, G_DBUS_CALL_FLAGS_NONE,
					 -1, NULL, &error);

	if (retval == NULL) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_variant_get (retval, "(b)", &active);
out:
	if (retval != NULL)
		g_variant_unref (retval);
	return active;
}

/**
 * gpm_screensaver_poke:
 * @screensaver: This class instance
 *
 * Pokes GNOME Screensaver simulating hardware events. This displays the unlock
 * dialogue when we resume, so the user doesn't have to move the mouse or press
 * any key before the window comes up.
 **/
gboolean
gpm_screensaver_poke (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	egg_debug ("poke");
	g_dbus_proxy_call (screensaver->priv->proxy,
			   "SimulateUserActivity",
			   NULL, G_DBUS_CALL_FLAGS_NONE,
			   -1, NULL, NULL, NULL);

	return TRUE;
}

/**
 * gpm_screensaver_class_init:
 * @klass: This class instance
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
 * @screensaver: This class instance
 **/
static void
gpm_screensaver_init (GpmScreensaver *screensaver)
{
	GDBusConnection *connection;
	GError *error = NULL;

	screensaver->priv = GPM_SCREENSAVER_GET_PRIVATE (screensaver);

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
	screensaver->priv->proxy =
		g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
			G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
			NULL,
			GS_LISTENER_SERVICE,
			GS_LISTENER_PATH,
			GS_LISTENER_INTERFACE,
			NULL, &error);
	if (screensaver->priv->proxy == NULL) {
		egg_warning ("failed to setup screensaver proxy: %s", error->message);
		g_error_free (error);
	}
	screensaver->priv->conf = gconf_client_get_default ();
}

/**
 * gpm_screensaver_finalize:
 * @object: This class instance
 **/
static void
gpm_screensaver_finalize (GObject *object)
{
	GpmScreensaver *screensaver;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SCREENSAVER (object));

	screensaver = GPM_SCREENSAVER (object);
	screensaver->priv = GPM_SCREENSAVER_GET_PRIVATE (screensaver);

	g_object_unref (screensaver->priv->conf);
	g_object_unref (screensaver->priv->proxy);

	G_OBJECT_CLASS (gpm_screensaver_parent_class)->finalize (object);
}

/**
 * gpm_screensaver_new:
 * Return value: new GpmScreensaver instance.
 **/
GpmScreensaver *
gpm_screensaver_new (void)
{
	if (gpm_screensaver_object != NULL) {
		g_object_ref (gpm_screensaver_object);
	} else {
		gpm_screensaver_object = g_object_new (GPM_TYPE_SCREENSAVER, NULL);
		g_object_add_weak_pointer (gpm_screensaver_object, &gpm_screensaver_object);
	}
	return GPM_SCREENSAVER (gpm_screensaver_object);
}

