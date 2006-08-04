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

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "gpm-screensaver.h"
#include "gpm-debug.h"
#include "gpm-dbus-session-monitor.h"

static void     gpm_screensaver_class_init (GpmScreensaverClass *klass);
static void     gpm_screensaver_init       (GpmScreensaver      *screensaver);
static void     gpm_screensaver_finalize   (GObject		*object);

#define GPM_SCREENSAVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SCREENSAVER, GpmScreensaverPrivate))

#define GS_LISTENER_SERVICE	"org.gnome.ScreenSaver"
#define GS_LISTENER_PATH	"/org/gnome/ScreenSaver"
#define GS_LISTENER_INTERFACE	"org.gnome.ScreenSaver"

#define GS_PREF_DIR		"/apps/gnome-screensaver"
#define GS_PREF_LOCK_ENABLED	GS_PREF_DIR "/lock_enabled"
#define GS_PREF_IDLE_DELAY	GS_PREF_DIR "/idle_delay"

struct GpmScreensaverPrivate
{
	GpmDbusSessionMonitor	*dbus_session;
	DBusGConnection		*session_connection;
	DBusGProxy		*gs_proxy;
	GConfClient		*gconf_client;
	gboolean		 is_connected;	/* if we are connected to g-s */
	int			 idle_delay;	/* the setting in g-s-p, cached */
};

enum {
	GS_DELAY_CHANGED,
	CONNECTION_CHANGED,
	AUTH_REQUEST,
	DAEMON_START,
	DAEMON_STOP,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmScreensaver, gpm_screensaver, G_TYPE_OBJECT)

/** Invoked when we get the AuthenticationRequestBegin from g-s when the user
 *  has moved their mouse and we are showing the authentication box.
 */
static void
gpm_screensaver_auth_begin (DBusGProxy     *proxy,
			    GpmScreensaver *screensaver)
{
	const gboolean value = TRUE;
	gpm_debug ("emitting auth-request : (%i)", value);
	g_signal_emit (screensaver, signals [AUTH_REQUEST], 0, value);
}

/** Invoked when we get the AuthenticationRequestEnd from g-s when the user
 *  has entered a valid password or re-authenticated.
 */
static void
gpm_screensaver_auth_end (DBusGProxy     *proxy,
			  GpmScreensaver *screensaver)
{
	const gboolean value = FALSE;
	gpm_debug ("emitting auth-request : (%i)", value);
	g_signal_emit (screensaver, signals [AUTH_REQUEST], 0, value);
}

/**
 * gpm_screensaver_connect:
 * @screensaver: This screensaver class instance
 **/
static gboolean
gpm_screensaver_connect (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->is_connected) {
		/* sometimes dbus goes crazy and we get two events */
		return FALSE;
	}
	screensaver->priv->gs_proxy = dbus_g_proxy_new_for_name (screensaver->priv->session_connection,
								 GS_LISTENER_SERVICE,
								 GS_LISTENER_PATH,
								 GS_LISTENER_INTERFACE);
	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	screensaver->priv->is_connected = TRUE;
	gpm_debug ("gnome-screensaver connected to the session DBUS");

	g_signal_emit (screensaver, signals [CONNECTION_CHANGED], 0, screensaver->priv->is_connected);

	/* get AuthenticationRequestBegin */
	dbus_g_proxy_add_signal (screensaver->priv->gs_proxy,
				 "AuthenticationRequestBegin", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (screensaver->priv->gs_proxy,
				     "AuthenticationRequestBegin",
				     G_CALLBACK (gpm_screensaver_auth_begin),
				     screensaver, NULL);

	/* get AuthenticationRequestEnd */
	dbus_g_proxy_add_signal (screensaver->priv->gs_proxy,
				 "AuthenticationRequestEnd", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (screensaver->priv->gs_proxy,
				     "AuthenticationRequestEnd",
				     G_CALLBACK (gpm_screensaver_auth_end),
				     screensaver, NULL);
	return TRUE;
}

/**
 * gpm_screensaver_disconnect:
 * @screensaver: This screensaver class instance
 **/
static gboolean
gpm_screensaver_disconnect (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (! screensaver->priv->is_connected) {
		/* sometimes dbus goes crazy and we get two events */
		return FALSE;
	}
	if (screensaver->priv->gs_proxy) {
		g_object_unref (G_OBJECT (screensaver->priv->gs_proxy));
		screensaver->priv->gs_proxy = NULL;
	}
	screensaver->priv->is_connected = FALSE;

	g_signal_emit (screensaver, signals [CONNECTION_CHANGED], 0, screensaver->priv->is_connected);
	gpm_debug ("gnome-screensaver disconnected from the session DBUS");
	return TRUE;
}

/**
 * gconf_key_changed_cb:
 *
 * Turn a gconf key change into a signal
 **/
static void
gconf_key_changed_cb (GConfClient  *client,
		      guint	    cnxn_id,
		      GConfEntry   *entry,
		      gpointer	    user_data)
{
	GpmScreensaver *screensaver = GPM_SCREENSAVER (user_data);

	g_return_if_fail (GPM_IS_SCREENSAVER (screensaver));
	g_return_if_fail (entry != NULL);

	if (strcmp (entry->key, GS_PREF_IDLE_DELAY) == 0) {
		screensaver->priv->idle_delay = gconf_client_get_int (client, entry->key, NULL);
		gpm_debug ("emitting gs-delay-changed : %i", screensaver->priv->idle_delay);
		g_signal_emit (screensaver, signals [GS_DELAY_CHANGED], 0, screensaver->priv->idle_delay);
	}
}

/**
 * gpm_screensaver_lock_enabled:
 * @screensaver: This screensaver class instance
 * Return value: If gnome-screensaver is set to lock the screen on screensave
 **/
gboolean
gpm_screensaver_lock_enabled (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	return gconf_client_get_bool (screensaver->priv->gconf_client,
				      GS_PREF_LOCK_ENABLED, NULL);
}

/**
 * gpm_screensaver_lock_set:
 * @screensaver: This screensaver class instance
 * @lock: If gnome-screensaver should lock the screen on screensave
 **/
gboolean
gpm_screensaver_lock_set (GpmScreensaver *screensaver, gboolean lock)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	gconf_client_set_bool (screensaver->priv->gconf_client,
			       GS_PREF_LOCK_ENABLED, lock, NULL);
	return TRUE;
}

/**
 * gpm_screensaver_get_delay:
 * @screensaver: This screensaver class instance
 * Return value: The delay for the idle time set in gnome-screesaver-properties.
 **/
int
gpm_screensaver_get_delay (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), 0);
	return screensaver->priv->idle_delay;
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

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (! screensaver->priv->is_connected) {
		gpm_debug ("Not locking, as gnome-screensaver not running");
		return FALSE;
	}

	gpm_debug ("doing gnome-screensaver lock");

	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}
	dbus_g_proxy_call_no_reply (screensaver->priv->gs_proxy, "Lock", G_TYPE_INVALID);

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
 * @screensaver: This screensaver class instance
 * @reason:      The reason for throttling
 * Return value: Success value, or zero for failure
 **/
guint
gpm_screensaver_add_throttle (GpmScreensaver *screensaver,
			      const char     *reason)
{
	GError  *error = NULL;
	gboolean res;
	guint32  cookie;
	guint32  ret;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), 0);
	g_return_val_if_fail (reason != NULL, 0);

	if (! screensaver->priv->is_connected) {
		gpm_debug ("Cannot throttle now as gnome-screensaver not running");
		return 0;
	}

	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	res = dbus_g_proxy_call (screensaver->priv->gs_proxy,
				 "Throttle",
				 &error,
				 G_TYPE_STRING, "Power Manager",
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &cookie,
				 G_TYPE_INVALID);
	ret = 0;
	if (res) {
		ret = cookie;
	}

	gpm_debug ("adding throttle reason: '%s': id %u", reason, cookie);

	return ret;
}

gboolean
gpm_screensaver_remove_throttle (GpmScreensaver *screensaver,
				 guint           cookie)
{
	gboolean res;
	GError  *error;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	gpm_debug ("removing throttle: id %u", cookie);

	error = NULL;
	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	res = dbus_g_proxy_call (screensaver->priv->gs_proxy,
				 "UnThrottle",
				 &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	return res;
}

/**
 * gpm_screensaver_check_running:
 * @screensaver: This screensaver class instance
 * Return value: TRUE if gnome-screensaver is running
 **/
gboolean
gpm_screensaver_check_running (GpmScreensaver *screensaver)
{
	GError *error = NULL;
	gboolean boolret = TRUE;
	gboolean temp = TRUE;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	if (!dbus_g_proxy_call (screensaver->priv->gs_proxy, "GetActive", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &temp, G_TYPE_INVALID)) {
		if (error) {
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
gboolean
gpm_screensaver_poke (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (! screensaver->priv->is_connected) {
		gpm_debug ("Not poke'ing, as gnome-screensaver not running");
		return FALSE;
	}

	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	gpm_debug ("poke");
	dbus_g_proxy_call_no_reply (screensaver->priv->gs_proxy,
				    "SimulateUserActivity",
				    G_TYPE_INVALID);
	return TRUE;
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

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	g_return_val_if_fail (time != NULL, FALSE);

	if (! screensaver->priv->is_connected) {
		gpm_debug ("Not getting idle, as gnome-screensaver not running");
		return FALSE;
	}

	/* shouldn't be, but make sure proxy valid */
	if (screensaver->priv->gs_proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	if (!dbus_g_proxy_call (screensaver->priv->gs_proxy, "GetActiveTime", &error,
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

/**
 * dbus_name_owner_changed_session_cb:
 * @power: The power class instance
 * @name: The DBUS name, e.g. hal.freedesktop.org
 * @prev: The previous name, e.g. :0.13
 * @new: The new name, e.g. :0.14
 * @manager: This manager class instance
 *
 * The name-owner-changed session DBUS callback.
 **/
static void
dbus_name_owner_changed_session_cb (GpmDbusSessionMonitor *dbus_monitor,
				    const char	   *name,
				    const char     *prev,
				    const char     *new,
				    GpmScreensaver *screensaver)
{
	if (strcmp (name, GS_LISTENER_SERVICE) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0 ) {
			gpm_screensaver_disconnect (screensaver);
			g_debug ("emitting daemon-stop");
			g_signal_emit (screensaver, signals [DAEMON_STOP], 0);
		}
		if (strlen (prev) == 0 && strlen (new) != 0 ) {
			gpm_screensaver_connect (screensaver);
			g_debug ("emitting daemon-start");
			g_signal_emit (screensaver, signals [DAEMON_START], 0);
		}
	}
}

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

	signals [DAEMON_START] =
		g_signal_new ("daemon-start",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, daemon_start),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	signals [DAEMON_STOP] =
		g_signal_new ("daemon-stop",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, daemon_stop),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
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

	screensaver->priv->dbus_session = gpm_dbus_session_monitor_new ();
	g_signal_connect (screensaver->priv->dbus_session, "name-owner-changed",
			  G_CALLBACK (dbus_name_owner_changed_session_cb), screensaver);

	screensaver->priv->is_connected = FALSE;
	screensaver->priv->gs_proxy = NULL;
	screensaver->priv->gconf_client = gconf_client_get_default ();

	screensaver->priv->session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (! screensaver->priv->session_connection) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_critical_error ("Cannot connect to DBUS Session Daemon");
	}
	/* blindly try to connect */
	gpm_screensaver_connect (screensaver);
	if (! gpm_screensaver_check_running (screensaver)) {
		screensaver->priv->is_connected = FALSE;
		gpm_warning ("gnome-screensaver has not been started yet");
	}

	/* get value of delay in g-s-p */
	screensaver->priv->idle_delay = gconf_client_get_int (screensaver->priv->gconf_client,
							      GS_PREF_IDLE_DELAY, NULL);

	/* set up the monitoring for gnome-screensaver */
	gconf_client_add_dir (screensaver->priv->gconf_client,
			      GS_PREF_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);

	gconf_client_notify_add (screensaver->priv->gconf_client,
				 GS_PREF_DIR,
				 gconf_key_changed_cb,
				 screensaver, NULL, NULL);
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

	gpm_screensaver_disconnect (screensaver);
	g_object_unref (screensaver->priv->gconf_client);
	g_object_unref (screensaver->priv->dbus_session);
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
