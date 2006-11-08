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

#include "gpm-conf.h"
#include "gpm-screensaver.h"
#include "gpm-debug.h"
#include "gpm-proxy.h"
#include "gpm-button.h"
#include "gpm-dpms.h"
#include "gpm-ac-adapter.h"
#include "gpm-brightness-lcd.h"

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
	GpmProxy		*gproxy;
	GpmConf			*conf;
	GpmButton		*button;
	GpmDpms			*dpms;
	GpmAcAdapter		*ac_adapter;
	GpmBrightnessLcd	*brightness_lcd;
	guint			 idle_delay;	/* the setting in g-s-p, cached */
	guint32         	 ac_throttle_id;
	guint32         	 dpms_throttle_id;
	guint32         	 lid_throttle_id;
};

enum {
	GS_DELAY_CHANGED,
	CONNECTION_CHANGED,
	IDLE_CHANGED,
	AUTH_REQUEST,
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
	GError  *error;
	gboolean res;

	gpm_debug ("emitting auth-request : (%i)", value);
	g_signal_emit (screensaver, signals [AUTH_REQUEST], 0, value);

	/* TODO: This may be a bid of a bodge, as we will have multiple
		 resume requests -- maybe this need a logic cleanup */
	if (screensaver->priv->brightness_lcd) {
		gpm_debug ("undimming lcd due to auth begin");
		gpm_brightness_lcd_undim (screensaver->priv->brightness_lcd);
	}

	/* We turn on the monitor unconditionally, as we may be using
	 * a smartcard to authenticate and DPMS might still be on.
	 * See #350291 for more details */
	error = NULL;
	res = gpm_dpms_set_mode (screensaver->priv->dpms, GPM_DPMS_MODE_ON, &error);
	if (! res) {
		gpm_warning ("Failed to turn on DPMS: %s", error->message);
		g_error_free (error);
	}
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

/** Invoked when we get the AuthenticationRequestEnd from g-s when the user
 *  has entered a valid password or re-authenticated.
 */
static void
gpm_screensaver_idle_changed (DBusGProxy     *proxy,
			      gboolean        is_idle,
			      GpmScreensaver *screensaver)
{
	gpm_debug ("emitting idle-changed : (%i)", is_idle);
	g_signal_emit (screensaver, signals [IDLE_CHANGED], 0, is_idle);
}

static void
update_dpms_throttle (GpmScreensaver *screensaver)
{
	GpmDpmsMode mode;
	gpm_dpms_get_mode (screensaver->priv->dpms, &mode, NULL);

	/* Throttle the screensaver when DPMS is active since we can't see it anyway */
	if (mode == GPM_DPMS_MODE_ON) {
		if (screensaver->priv->dpms_throttle_id != 0) {
			gpm_screensaver_remove_throttle (screensaver, screensaver->priv->dpms_throttle_id);
			screensaver->priv->dpms_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (screensaver->priv->dpms_throttle_id != 0) {
			gpm_screensaver_remove_throttle (screensaver, screensaver->priv->dpms_throttle_id);
		}
		screensaver->priv->dpms_throttle_id = gpm_screensaver_add_throttle (screensaver, _("Display DPMS activated"));
	}
}

static void
update_ac_throttle (GpmScreensaver *screensaver,
		    GpmAcAdapterState state)
{
	/* Throttle the screensaver when we are not on AC power so we don't
	   waste the battery */
	if (state == GPM_AC_ADAPTER_PRESENT) {
		if (screensaver->priv->ac_throttle_id != 0) {
			gpm_screensaver_remove_throttle (screensaver, screensaver->priv->ac_throttle_id);
			screensaver->priv->ac_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (screensaver->priv->ac_throttle_id != 0) {
			gpm_screensaver_remove_throttle (screensaver, screensaver->priv->ac_throttle_id);
		}
		screensaver->priv->ac_throttle_id = gpm_screensaver_add_throttle (screensaver, _("On battery power"));
	}
}

static void
update_lid_throttle (GpmScreensaver *screensaver,
		     gboolean        lid_is_closed)
{
	gboolean laptop_using_ext_mon;

	/* action differs if just a laptop with a monitor connected */
	gpm_conf_get_bool (screensaver->priv->conf, GPM_CONF_LAPTOP_USES_EXT_MON,
			   &laptop_using_ext_mon);

	/* Throttle the screensaver when the lid is close since we can't see it anyway
	   and it may overheat the laptop */
	if (lid_is_closed == FALSE || laptop_using_ext_mon == TRUE) {
		if (screensaver->priv->lid_throttle_id != 0) {
			gpm_screensaver_remove_throttle (screensaver, screensaver->priv->lid_throttle_id);
			screensaver->priv->lid_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (screensaver->priv->lid_throttle_id != 0) {
			gpm_screensaver_remove_throttle (screensaver, screensaver->priv->lid_throttle_id);
		}
		screensaver->priv->lid_throttle_id = gpm_screensaver_add_throttle (screensaver, _("Laptop lid is closed"));
	}
}

/**
 * gpm_screensaver_proxy_connect_more:
 * @screensaver: This screensaver class instance
 **/
static gboolean
gpm_screensaver_proxy_connect_more (GpmScreensaver *screensaver)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = gpm_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
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
				     G_CALLBACK (gpm_screensaver_idle_changed),
				     screensaver, NULL);

	update_dpms_throttle (screensaver);
//	update_lid_throttle (screensaver, lid_is_closed);

	return TRUE;
}

/**
 * gpm_screensaver_proxy_disconnect_more:
 * @screensaver: This screensaver class instance
 **/
static gboolean
gpm_screensaver_proxy_disconnect_more (GpmScreensaver *screensaver)
{
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	g_signal_emit (screensaver, signals [CONNECTION_CHANGED], 0, FALSE);
	gpm_debug ("gnome-screensaver disconnected from the session DBUS");
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

	if (strcmp (key, GS_PREF_IDLE_DELAY) == 0) {
		gpm_conf_get_uint (screensaver->priv->conf, key, &screensaver->priv->idle_delay);
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
	gboolean enabled;
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	gpm_conf_get_bool (screensaver->priv->conf, GS_PREF_LOCK_ENABLED, &enabled);

	return enabled;
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
	gpm_conf_set_bool (screensaver->priv->conf, GS_PREF_LOCK_ENABLED, lock);
	return TRUE;
}

/**
 * gpm_screensaver_get_delay:
 * @screensaver: This screensaver class instance
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
 * @screensaver: This screensaver class instance
 * Return value: Success value
 **/
gboolean
gpm_screensaver_lock (GpmScreensaver *screensaver)
{
	guint sleepcount = 0;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = gpm_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("doing gnome-screensaver lock");
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
 * @screensaver: This screensaver class instance
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

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);
	g_return_val_if_fail (reason != NULL, 0);

	proxy = gpm_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	/* shouldn't be, but make sure proxy valid */
	if (proxy == NULL) {
		gpm_warning ("g-s proxy is NULL!");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "Throttle", &error,
				 G_TYPE_STRING, "Power screensaver",
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &cookie,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("Throttle failed!");
		return 0;
	}

	gpm_debug ("adding throttle reason: '%s': id %u", reason, cookie);
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

	proxy = gpm_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("removing throttle: id %u", cookie);
	ret = dbus_g_proxy_call (proxy, "UnThrottle", &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("UnThrottle failed!");
		return FALSE;
	}

	return TRUE;
}

/**
 * gpm_screensaver_check_running:
 * @screensaver: This screensaver class instance
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

	proxy = gpm_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "GetActive", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &temp,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}

	return ret;
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
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	proxy = gpm_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("poke");
	dbus_g_proxy_call_no_reply (proxy,
				    "SimulateUserActivity",
				    G_TYPE_INVALID);
	return TRUE;
}

/**
 * gpm_screensaver_get_idle:
 * @screensaver: This screensaver class instance
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

	proxy = gpm_proxy_get_proxy (screensaver->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "GetActiveTime", &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, time_secs,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetActiveTime failed!");
		return FALSE;
	}

	return TRUE;
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

	signals [IDLE_CHANGED] =
		g_signal_new ("idle-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmScreensaverClass, idle_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/**
 * proxy_status_cb:
 * @proxy: The dbus raw proxy
 * @status: The status of the service, where TRUE is connected
 * @screensaver: This screensaver class instance
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
 * button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @screensaver: This class instance
 **/
static void
button_pressed_cb (GpmButton      *button,
		   const gchar    *type,
		   GpmScreensaver *screensaver)
{
	gpm_debug ("Button press event type=%s", type);

	/* really belongs in gnome-screensaver */
	if (strcmp (type, GPM_BUTTON_LOCK) == 0) {
		gpm_screensaver_lock (screensaver);

	} else if (strcmp (type, GPM_BUTTON_LID_CLOSED) == 0) {
		/* Disable or enable the fancy screensaver, as we don't want
		 * this starting when the lid is shut */
		update_lid_throttle (screensaver, TRUE);

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {
		update_lid_throttle (screensaver, FALSE);

	}
}

/**
 * dpms_mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @screensaver: This class instance
 *
 * What happens when the DPMS mode is changed.
 **/
static void
dpms_mode_changed_cb (GpmDpms        *dpms,
		      GpmDpmsMode     mode,
		      GpmScreensaver *screensaver)
{
	gpm_debug ("DPMS mode changed: %d", mode);

	update_dpms_throttle (screensaver);
}

/**
 * ac_adapter_changed_cb:
 * @ac_adapter: The ac_adapter class instance
 * @on_ac: if we are on AC ac_adapter
 * @screensaver: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter     *ac_adapter,
		       GpmAcAdapterState state,
		       GpmScreensaver   *screensaver)
{
	update_ac_throttle (screensaver, state);

	/* simulate user input, to fix #333525 */
	gpm_screensaver_poke (screensaver);
}

/**
 * gpm_screensaver_service_init:
 *
 * @screensaver: This class instance
 *
 * This starts the interactive parts of the class, for instance it makes the
 * the class respond to button presses and AC state changes.
 *
 * If your are using this class in the preferences or info programs you don't
 * need to call this function
 **/
gboolean
gpm_screensaver_service_init (GpmScreensaver *screensaver)
{
	GpmAcAdapterState state;

	g_return_val_if_fail (screensaver != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	/* we use button for the button-pressed signals */
	screensaver->priv->button = gpm_button_new ();
	g_signal_connect (screensaver->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), screensaver);

	/* we use dpms so we turn off the screensaver when dpms is on */
	screensaver->priv->dpms = gpm_dpms_new ();
	g_signal_connect (screensaver->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), screensaver);

	/* we use ac_adapter so we can poke the screensaver and throttle */
	screensaver->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (screensaver->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), screensaver);

	/* we use brightness so we undim when we need authentication */
	screensaver->priv->brightness_lcd = gpm_brightness_lcd_new ();

	/* init to unthrottled */
	screensaver->priv->ac_throttle_id = 0;
	screensaver->priv->dpms_throttle_id = 0;
	screensaver->priv->lid_throttle_id = 0;

	/* update ac throttle */
	gpm_ac_adapter_get_state (screensaver->priv->ac_adapter, &state);
	update_ac_throttle (screensaver, state);

	return TRUE;
}

/**
 * gpm_screensaver_init:
 * @screensaver: This screensaver class instance
 **/
static void
gpm_screensaver_init (GpmScreensaver *screensaver)
{
	DBusGProxy *proxy;

	screensaver->priv = GPM_SCREENSAVER_GET_PRIVATE (screensaver);

	screensaver->priv->gproxy = gpm_proxy_new ();
	proxy = gpm_proxy_assign (screensaver->priv->gproxy,
				  GPM_PROXY_SESSION,
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

	gpm_screensaver_proxy_disconnect_more (screensaver);
	g_object_unref (screensaver->priv->conf);
	g_object_unref (screensaver->priv->gproxy);
	if (screensaver->priv->button != NULL) {
		g_object_unref (screensaver->priv->button);
	}
	if (screensaver->priv->dpms != NULL) {
		g_object_unref (screensaver->priv->dpms);
	}
	if (screensaver->priv->ac_adapter != NULL) {
		g_object_unref (screensaver->priv->ac_adapter);
	}
	if (screensaver->priv->brightness_lcd != NULL) {
		g_object_unref (screensaver->priv->brightness_lcd);
	}
	if (screensaver->priv->ac_adapter != NULL) {
		g_object_unref (screensaver->priv->ac_adapter);
	}

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
