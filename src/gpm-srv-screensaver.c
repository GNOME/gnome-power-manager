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
#include "gpm-srv-screensaver.h"
#include "egg-debug.h"
#include "gpm-button.h"
#include "gpm-dpms.h"
#include "gpm-ac-adapter.h"
#include "gpm-brightness.h"

static void     gpm_srv_screensaver_class_init (GpmSrvScreensaverClass *klass);
static void     gpm_srv_screensaver_init       (GpmSrvScreensaver      *srv_screensaver);
static void     gpm_srv_screensaver_finalize   (GObject		*object);

#define GPM_SRV_SCREENSAVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SRV_SCREENSAVER, GpmSrvScreensaverPrivate))

struct GpmSrvScreensaverPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmButton		*button;
	GpmBrightness		*brightness;
	GpmConf			*conf;
	GpmDpms			*dpms;
	GpmScreensaver		*screensaver;
	guint32         	 ac_throttle_id;
	guint32         	 dpms_throttle_id;
	guint32         	 lid_throttle_id;
};

G_DEFINE_TYPE (GpmSrvScreensaver, gpm_srv_screensaver, G_TYPE_OBJECT)

/**
 * screensaver_auth_request_cb:
 * @manager: This manager class instance
 * @auth: If we are trying to authenticate
 *
 * Called when the user is trying or has authenticated
 **/
static void
screensaver_auth_request_cb (GpmScreensaver *screensaver,
			     gboolean        auth_begin,
			     GpmSrvScreensaver *srv_screensaver)
{
	/* only clear errors if we have finished the authentication */
	if (auth_begin) {
		GError  *error;

		/* TODO: This may be a bid of a bodge, as we will have multiple
			 resume requests -- maybe this need a logic cleanup */
		if (srv_screensaver->priv->brightness) {
			egg_debug ("undimming lcd due to auth begin");
//			gpm_brightness_undim (srv_screensaver->priv->brightness);
		}

		/* We turn on the monitor unconditionally, as we may be using
		 * a smartcard to authenticate and DPMS might still be on.
		 * See #350291 for more details */
		error = NULL;
		gpm_dpms_set_mode_enum (srv_screensaver->priv->dpms, GPM_DPMS_MODE_ON, &error);
		if (error != NULL) {
			egg_warning ("Failed to turn on DPMS: %s", error->message);
			g_error_free (error);
		}
	}
}

static void
update_dpms_throttle (GpmSrvScreensaver *srv_screensaver)
{
	GpmDpmsMode mode;
	gpm_dpms_get_mode_enum (srv_screensaver->priv->dpms, &mode, NULL);

	/* Throttle the srv_screensaver when DPMS is active since we can't see it anyway */
	if (mode == GPM_DPMS_MODE_ON) {
		if (srv_screensaver->priv->dpms_throttle_id != 0) {
			gpm_screensaver_remove_throttle (srv_screensaver->priv->screensaver, srv_screensaver->priv->dpms_throttle_id);
			srv_screensaver->priv->dpms_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (srv_screensaver->priv->dpms_throttle_id != 0) {
			gpm_screensaver_remove_throttle (srv_screensaver->priv->screensaver, srv_screensaver->priv->dpms_throttle_id);
		}
		srv_screensaver->priv->dpms_throttle_id = gpm_screensaver_add_throttle (srv_screensaver->priv->screensaver, _("Display DPMS activated"));
	}
}

static void
update_ac_throttle (GpmSrvScreensaver *srv_screensaver,
		    gboolean	       on_ac)
{
	/* Throttle the srv_screensaver when we are not on AC power so we don't
	   waste the battery */
	if (on_ac) {
		if (srv_screensaver->priv->ac_throttle_id != 0) {
			gpm_screensaver_remove_throttle (srv_screensaver->priv->screensaver, srv_screensaver->priv->ac_throttle_id);
			srv_screensaver->priv->ac_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (srv_screensaver->priv->ac_throttle_id != 0) {
			gpm_screensaver_remove_throttle (srv_screensaver->priv->screensaver, srv_screensaver->priv->ac_throttle_id);
		}
		srv_screensaver->priv->ac_throttle_id = gpm_screensaver_add_throttle (srv_screensaver->priv->screensaver, _("On battery power"));
	}
}

static void
update_lid_throttle (GpmSrvScreensaver *srv_screensaver,
		     gboolean           lid_is_closed)
{
	/* Throttle the screensaver when the lid is close since we can't see it anyway
	   and it may overheat the laptop */
	if (lid_is_closed == FALSE) {
		if (srv_screensaver->priv->lid_throttle_id != 0) {
			gpm_screensaver_remove_throttle (srv_screensaver->priv->screensaver, srv_screensaver->priv->lid_throttle_id);
			srv_screensaver->priv->lid_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (srv_screensaver->priv->lid_throttle_id != 0) {
			gpm_screensaver_remove_throttle (srv_screensaver->priv->screensaver, srv_screensaver->priv->lid_throttle_id);
		}
		srv_screensaver->priv->lid_throttle_id = gpm_screensaver_add_throttle (srv_screensaver->priv->screensaver, _("Laptop lid is closed"));
	}
}

/**
 * button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @srv_screensaver: This class instance
 **/
static void
button_pressed_cb (GpmButton      *button,
		   const gchar    *type,
		   GpmSrvScreensaver *srv_screensaver)
{
	egg_debug ("Button press event type=%s", type);

	/* really belongs in gnome-srv_screensaver */
	if (strcmp (type, GPM_BUTTON_LOCK) == 0) {
		gpm_screensaver_lock (srv_screensaver->priv->screensaver);

	} else if (strcmp (type, GPM_BUTTON_LID_CLOSED) == 0) {
		/* Disable or enable the fancy srv_screensaver, as we don't want
		 * this starting when the lid is shut */
		update_lid_throttle (srv_screensaver, TRUE);

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {
		update_lid_throttle (srv_screensaver, FALSE);

	}
}

/**
 * dpms_mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @srv_screensaver: This class instance
 *
 * What happens when the DPMS mode is changed.
 **/
static void
dpms_mode_changed_cb (GpmDpms        *dpms,
		      GpmDpmsMode     mode,
		      GpmSrvScreensaver *srv_screensaver)
{
	egg_debug ("DPMS mode changed: %d", mode);

	update_dpms_throttle (srv_screensaver);
}

/**
 * ac_adapter_changed_cb:
 * @ac_adapter: The ac_adapter class instance
 * @on_ac: if we are on AC ac_adapter
 * @srv_screensaver: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter      *ac_adapter,
		       gboolean		  on_ac,
		       GpmSrvScreensaver *srv_screensaver)
{
	update_ac_throttle (srv_screensaver, on_ac);

	/* simulate user input, but only when the lid is open */
	if (gpm_button_is_lid_closed (srv_screensaver->priv->button) == FALSE) {
		gpm_screensaver_poke (srv_screensaver->priv->screensaver);
	}
}

/**
 * gpm_srv_screensaver_class_init:
 * @klass: This class instance
 **/
static void
gpm_srv_screensaver_class_init (GpmSrvScreensaverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_srv_screensaver_finalize;
	g_type_class_add_private (klass, sizeof (GpmSrvScreensaverPrivate));

}

/**
 * gpm_srv_screensaver_init:
 * @srv_screensaver: This class instance
 **/
static void
gpm_srv_screensaver_init (GpmSrvScreensaver *srv_screensaver)
{
	gboolean on_ac;

	srv_screensaver->priv = GPM_SRV_SCREENSAVER_GET_PRIVATE (srv_screensaver);

	srv_screensaver->priv->conf = gpm_conf_new ();

	/* we use screensaver as the master class */
	srv_screensaver->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (srv_screensaver->priv->screensaver, "auth-request",
 			  G_CALLBACK (screensaver_auth_request_cb), srv_screensaver);

	/* we use button for the button-pressed signals */
	srv_screensaver->priv->button = gpm_button_new ();
	g_signal_connect (srv_screensaver->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), srv_screensaver);

	/* we use dpms so we turn off the srv_screensaver when dpms is on */
	srv_screensaver->priv->dpms = gpm_dpms_new ();
	g_signal_connect (srv_screensaver->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), srv_screensaver);

	/* we use ac_adapter so we can poke the srv_screensaver and throttle */
	srv_screensaver->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (srv_screensaver->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), srv_screensaver);

	/* we use brightness so we undim when we need authentication */
	srv_screensaver->priv->brightness = gpm_brightness_new ();

	/* init to unthrottled */
	srv_screensaver->priv->ac_throttle_id = 0;
	srv_screensaver->priv->dpms_throttle_id = 0;
	srv_screensaver->priv->lid_throttle_id = 0;

	/* update ac throttle */
	on_ac = gpm_ac_adapter_is_present (srv_screensaver->priv->ac_adapter);
	update_ac_throttle (srv_screensaver, on_ac);

}

/**
 * gpm_srv_screensaver_finalize:
 * @object: This class instance
 **/
static void
gpm_srv_screensaver_finalize (GObject *object)
{
	GpmSrvScreensaver *srv_screensaver;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_SCREENSAVER (object));

	srv_screensaver = GPM_SRV_SCREENSAVER (object);
	srv_screensaver->priv = GPM_SRV_SCREENSAVER_GET_PRIVATE (srv_screensaver);

	g_object_unref (srv_screensaver->priv->conf);
	g_object_unref (srv_screensaver->priv->screensaver);
	if (srv_screensaver->priv->button != NULL) {
		g_object_unref (srv_screensaver->priv->button);
	}
	if (srv_screensaver->priv->dpms != NULL) {
		g_object_unref (srv_screensaver->priv->dpms);
	}
	if (srv_screensaver->priv->ac_adapter != NULL) {
		g_object_unref (srv_screensaver->priv->ac_adapter);
	}
	if (srv_screensaver->priv->brightness != NULL) {
		g_object_unref (srv_screensaver->priv->brightness);
	}
	if (srv_screensaver->priv->ac_adapter != NULL) {
		g_object_unref (srv_screensaver->priv->ac_adapter);
	}

	G_OBJECT_CLASS (gpm_srv_screensaver_parent_class)->finalize (object);
}

/**
 * gpm_srv_screensaver_new:
 * Return value: new GpmSrvScreensaver instance.
 **/
GpmSrvScreensaver *
gpm_srv_screensaver_new (void)
{
	GpmSrvScreensaver *srv_screensaver;
	srv_screensaver = g_object_new (GPM_TYPE_SRV_SCREENSAVER, NULL);
	return GPM_SRV_SCREENSAVER (srv_screensaver);
}
