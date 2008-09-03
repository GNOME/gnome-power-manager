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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-conf.h"
#include "gpm-screensaver.h"
#include "egg-debug.h"
#include <libdbus-proxy.h>

static void     gpm_screensaver_class_init (GpmScreensaverClass *klass);
static void     gpm_screensaver_init       (GpmScreensaver      *screensaver);
static void     gpm_screensaver_finalize   (GObject		*object);

#define GPM_SCREENSAVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SCREENSAVER, GpmScreensaverPrivate))

struct GpmScreensaverPrivate
{
	DbusProxy		*gproxy;
	GpmConf			*conf;
	guint			 idle_delay;	/* the setting in g-s-p, cached */
};

enum {
	GS_DELAY_CHANGED,
	CONNECTION_CHANGED,
	SESSION_IDLE_CHANGED,
	POWERSAVE_IDLE_CHANGED,
	AUTH_REQUEST,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_screensaver_object = NULL;

G_DEFINE_TYPE (GpmScreensaver, gpm_screensaver, G_TYPE_OBJECT)

/** Invoked when we get the AuthenticationRequestBegin from g-s when the user
 *  has moved their mouse and we are showing the authentication box.
 */
static void
gpm_screensaver_auth_begin (DBusGProxy     *proxy,
			    GpmScreensaver *screensaver)
{
	egg_debug ("emitting auth-request : (%i)", TRUE);
	g_signal_emit (screensaver, signals [AUTH_REQUEST], 0, TRUE);
}

/** Invoked when we get the AuthenticationRequestEnd from g-s when the user
 *  has entered a valid password or re-authenticated.
 */
static void
gpm_screensaver_auth_end (DBusGProxy     *proxy,
			  GpmScreensaver *screensaver)
{
	egg_debug ("emitting auth-request : (%i)", FALSE);
	g_signal_emit (screensaver, signals [AUTH_REQUEST], 0, FALSE);
}

/** Invoked when we get the AuthenticationRequestEnd from g-s when the user
 *  has entered a valid password or re-authenticated.
 */
static void
gpm_screensaver_session_idle_changed (DBusGProxy     *proxy,
			      gboolean        is_idle,
			      GpmScreensaver *screensaver)
{
	egg_debug ("emitting session-idle-changed : (%i)", is_idle);
	g_signal_emit (screensaver, signals [SESSION_IDLE_CHANGED], 0, is_idle);
}

/** Invoked after a short delay
 */
static void
gpm_screensaver_powersave_idle_changed (DBusGProxy     *proxy,
			                gboolean        is_idle,
			                GpmScreensaver *screensaver)
{
	egg_debug ("emitting powersave-idle-changed : (%i)", is_idle);
	g_signal_emit (screensaver, signals [POWERSAVE_IDLE_CHANGED], 0, is_idle);
}

/**
 * gpm_screensaver_proxy_connect_more:
 * @screensaver: This class instance
 **/
static gboolean
gpm_screensaver_proxy_connect_more (GpmScreensaver *screensaver)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = dbus_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	g_signal_emit (screensaver, signals [CONNECTION_CHANGED], 0, TRUE);

	/* get AuthenticationRequestBegin */
	dbus_g_proxy_add_signal (proxy, "AuthenticationRequestBegin", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy,
				     "AuthenticationRequestBegin",
				     G_CALLBACK (gpm_screensaver_auth_begin),
				     screensaver, NULL);

	/* get AuthenticationRequestEnd */
	dbus_g_proxy_add_signal (proxy, "AuthenticationRequestEnd", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy,
				     "AuthenticationRequestEnd",
				     G_CALLBACK (gpm_screensaver_auth_end),
				     screensaver, NULL);

	/* get SessionIdleChanged */
	dbus_g_proxy_add_signal (proxy, "SessionIdleChanged", G_TYPE_BOOLEAN, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy,
				     "SessionIdleChanged",
				     G_CALLBACK (gpm_screensaver_session_idle_changed),
				     screensaver, NULL);

	/* get SessionIdleChanged */
	dbus_g_proxy_add_signal (proxy, "SessionPowerManagementIdleChanged", G_TYPE_BOOLEAN, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy,
				     "SessionPowerManagementIdleChanged",
				     G_CALLBACK (gpm_screensaver_powersave_idle_changed),
				     screensaver, NULL);

	return TRUE;
}

/**
 * gpm_screensaver_proxy_disconnect_more:
 * @screensaver: This class instance
 **/
static gboolean
gpm_screensaver_proxy_disconnect_more (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	g_signal_emit (screensaver, signals [CONNECTION_CHANGED], 0, FALSE);
	egg_debug ("gnome-screensaver disconnected from the session DBUS");
	return TRUE;
}

/**
 * gconf_key_changed_cb:
 *
 * Turn a gconf key change into a signal
 **/
static void
gconf_key_changed_cb (GpmConf        *conf,
		      const gchar    *key,
		      GpmScreensaver *screensaver)
{
	g_return_if_fail (GPM_IS_SCREENSAVER (screensaver));
	egg_debug ("key : %s", key);

	if (strcmp (key, GS_PREF_IDLE_DELAY) == 0) {
		gpm_conf_get_uint (screensaver->priv->conf, key, &screensaver->priv->idle_delay);
		egg_debug ("emitting gs-delay-changed : %i", screensaver->priv->idle_delay);
		g_signal_emit (screensaver, signals [GS_DELAY_CHANGED], 0, screensaver->priv->idle_delay);
	}
}

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

	gpm_conf_get_bool (screensaver->priv->conf, GS_PREF_LOCK_ENABLED, &enabled);

	return enabled;
}

/**
 * gpm_screensaver_lock_set:
 * @screensaver: This class instance
 * @lock: If gnome-screensaver should lock the screen on screensave
 **/
gboolean
gpm_screensaver_lock_set (GpmScreensaver *screensaver, gboolean lock)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	gpm_conf_set_bool (screensaver->priv->conf, GS_PREF_LOCK_ENABLED, lock);
	return TRUE;
}

/**
 * gpm_screensaver_get_delay:
 * @screensaver: This class instance
 * Return value: The delay for the idle time set in gnome-screesaver-properties.
 **/
gint
gpm_screensaver_get_delay (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), 0);
	return screensaver->priv->idle_delay;
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
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = dbus_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	egg_debug ("doing gnome-screensaver lock");
	dbus_g_proxy_call_no_reply (proxy, "Lock", G_TYPE_INVALID);

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
			break;
		}
	}

	return TRUE;
}

/**
 * gpm_screensaver_add_throttle:
 * @screensaver: This class instance
 * @reason:      The reason for throttling
 * Return value: Success value, or zero for failure
 **/
guint
gpm_screensaver_add_throttle (GpmScreensaver *screensaver,
			      const char     *reason)
{
	GError  *error = NULL;
	gboolean ret;
	guint32  cookie;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), 0);
	g_return_val_if_fail (reason != NULL, 0);

	proxy = dbus_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return 0;
	}

	/* shouldn't be, but make sure proxy valid */
	if (proxy == NULL) {
		egg_warning ("g-s proxy is NULL!");
		return 0;
	}

	ret = dbus_g_proxy_call (proxy, "Throttle", &error,
				 G_TYPE_STRING, "Power screensaver",
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &cookie,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("Throttle failed!");
		return 0;
	}

	egg_debug ("adding throttle reason: '%s': id %u", reason, cookie);
	return cookie;
}

gboolean
gpm_screensaver_remove_throttle (GpmScreensaver *screensaver,
				 guint           cookie)
{
	gboolean ret;
	DBusGProxy *proxy;
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = dbus_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	egg_debug ("removing throttle: id %u", cookie);
	ret = dbus_g_proxy_call (proxy, "UnThrottle", &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("UnThrottle failed!");
		return FALSE;
	}

	return TRUE;
}

/**
 * gpm_screensaver_check_running:
 * @screensaver: This class instance
 * Return value: TRUE if gnome-screensaver is running
 **/
gboolean
gpm_screensaver_check_running (GpmScreensaver *screensaver)
{
	gboolean ret;
	gboolean temp = TRUE;
	DBusGProxy *proxy;
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = dbus_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetActive", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &temp,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}

	return ret;
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
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = dbus_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	egg_debug ("poke");
	dbus_g_proxy_call_no_reply (proxy,
				    "SimulateUserActivity",
				    G_TYPE_INVALID);
	return TRUE;
}

/**
 * gpm_screensaver_get_idle:
 * @screensaver: This class instance
 * @time_secs: The returned idle time, passed by ref
 * Return value: Success value.
 **/
gboolean
gpm_screensaver_get_idle (GpmScreensaver *screensaver, gint *time_secs)
{
	GError *error = NULL;
	gboolean ret = TRUE;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	g_return_val_if_fail (time != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetActiveTime", &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, time_secs,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("GetActiveTime failed!");
		return FALSE;
	}

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

	signals [GS_DELAY_CHANGED] =
		g_signal_new ("gs-delay-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, gs_delay_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	signals [CONNECTION_CHANGED] =
		g_signal_new ("connection-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, connection_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	signals [AUTH_REQUEST] =
		g_signal_new ("auth-request",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, auth_request),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	signals [SESSION_IDLE_CHANGED] =
		g_signal_new ("session-idle-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, session_idle_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals [POWERSAVE_IDLE_CHANGED] =
		g_signal_new ("powersave-idle-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, session_idle_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/**
 * proxy_status_cb:
 * @proxy: The dbus raw proxy
 * @status: The status of the service, where TRUE is connected
 * @screensaver: This class instance
 **/
static void
proxy_status_cb (DBusGProxy     *proxy,
		 gboolean	 status,
		 GpmScreensaver *screensaver)
{
	g_return_if_fail (GPM_IS_SCREENSAVER (screensaver));

	if (status) {
		gpm_screensaver_proxy_connect_more (screensaver);
	} else {
		gpm_screensaver_proxy_disconnect_more (screensaver);
	}
}

/**
 * gpm_screensaver_init:
 * @screensaver: This class instance
 **/
static void
gpm_screensaver_init (GpmScreensaver *screensaver)
{
	DBusGProxy *proxy;

	screensaver->priv = GPM_SCREENSAVER_GET_PRIVATE (screensaver);

	screensaver->priv->gproxy = dbus_proxy_new ();
	proxy = dbus_proxy_assign (screensaver->priv->gproxy,
				  DBUS_PROXY_SESSION,
				  GS_LISTENER_SERVICE,
				  GS_LISTENER_PATH,
				  GS_LISTENER_INTERFACE);

	g_signal_connect (screensaver->priv->gproxy, "proxy-status",
			  G_CALLBACK (proxy_status_cb),
			  screensaver);

	gpm_screensaver_proxy_connect_more (screensaver);

	screensaver->priv->conf = gpm_conf_new ();
	g_signal_connect (screensaver->priv->conf, "value-changed",
			  G_CALLBACK (gconf_key_changed_cb), screensaver);

	/* get value of delay in g-s-p */
	gpm_conf_get_uint (screensaver->priv->conf, GS_PREF_IDLE_DELAY,
			   &screensaver->priv->idle_delay);
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

	gpm_screensaver_proxy_disconnect_more (screensaver);
	g_object_unref (screensaver->priv->conf);
	g_object_unref (screensaver->priv->gproxy);

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
