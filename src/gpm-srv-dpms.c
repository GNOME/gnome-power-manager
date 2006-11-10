/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "gpm-ac-adapter.h"
#include "gpm-conf.h"
#include "gpm-debug.h"
#include "gpm-dpms.h"
#include "gpm-srv-dpms.h"
#include "gpm-hal.h"
#include "gpm-idle.h"

static void     gpm_srv_dpms_class_init (GpmSrvDpmsClass *klass);
static void     gpm_srv_dpms_init       (GpmSrvDpms      *srv_dpms);
static void     gpm_srv_dpms_finalize   (GObject      *object);

#define GPM_SRV_DPMS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SRV_DPMS, GpmSrvDpmsPrivate))

struct GpmSrvDpmsPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmConf			*conf;
	GpmDpms			*dpms;
	GpmHal			*hal;
	GpmIdle			*idle;
};

G_DEFINE_TYPE (GpmSrvDpms, gpm_srv_dpms, G_TYPE_OBJECT)

/**
 * gpm_srv_dpms_sync_policy:
 * @srv_dpms: This class instance
 *
 * Sync the SRV_DPMS policy with what we have set in gconf.
 **/
void
gpm_srv_dpms_sync_policy (GpmSrvDpms *srv_dpms)
{
	GError  *error;
	gboolean res;
	guint    timeout = 0;
	guint    standby = 0;
	guint    suspend = 0;
	guint    off = 0;
	gchar   *dpms_method;
	GpmDpmsMethod method;
	GpmAcAdapterState state;

	/* get the ac state */
	gpm_ac_adapter_get_state (srv_dpms->priv->ac_adapter, &state);

	error = NULL;

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_uint (srv_dpms->priv->conf, GPM_CONF_AC_SLEEP_DISPLAY, &timeout);
		gpm_conf_get_string (srv_dpms->priv->conf, GPM_CONF_AC_DPMS_METHOD, &dpms_method);
	} else {
		gpm_conf_get_uint (srv_dpms->priv->conf, GPM_CONF_BATTERY_SLEEP_DISPLAY, &timeout);
		gpm_conf_get_string (srv_dpms->priv->conf, GPM_CONF_BATTERY_DPMS_METHOD, &dpms_method);
	}

	/* convert the string types to standard types */
	method = gpm_dpms_method_from_string (dpms_method);
	g_free (dpms_method);

	/* check if method is valid */
	if (method == GPM_DPMS_METHOD_UNKNOWN) {
		gpm_warning ("SRV_DPMS method unknown. Possible schema problem!");
		return;
	}

	/* choose a sensible default */
	if (method == GPM_DPMS_METHOD_DEFAULT) {
		gpm_debug ("choosing sensible default");
		if (gpm_hal_is_laptop (srv_dpms->priv->hal)) {
			gpm_debug ("laptop, so use GPM_SRV_DPMS_METHOD_OFF");
			method = GPM_DPMS_METHOD_OFF;
		} else {
			gpm_debug ("not laptop, so use GPM_SRV_DPMS_METHOD_STAGGER");
			method = GPM_DPMS_METHOD_STAGGER;
		}
	}

	/* Some monitors do not support certain suspend states, so we have to
	 * provide a way to only use the one that works. */
	if (method == GPM_DPMS_METHOD_STAGGER) {
		/* suspend after one timeout, turn off after another */
		standby = timeout;
		suspend = timeout;
		off     = timeout * 2;
	} else if (method == GPM_DPMS_METHOD_STANDBY) {
		standby = timeout;
		suspend = 0;
		off     = 0;
	} else if (method == GPM_DPMS_METHOD_SUSPEND) {
		standby = 0;
		suspend = timeout;
		off     = 0;
	} else if (method == GPM_DPMS_METHOD_OFF) {
		standby = 0;
		suspend = 0;
		off     = timeout;
	} else {
		/* wtf? */
		gpm_warning ("unknown srv_dpms mode!");
	}

	gpm_debug ("SRV_DPMS parameters %d %d %d, method '%i'", standby, suspend, off, method);

	error = NULL;
	res = gpm_dpms_set_enabled (srv_dpms->priv->dpms, TRUE, &error);
	if (error) {
		gpm_warning ("Unable to enable SRV_DPMS: %s", error->message);
		g_error_free (error);
		return;
	}

	error = NULL;
	res = gpm_dpms_set_timeouts (srv_dpms->priv->dpms, standby, suspend, off, &error);
	if (error) {
		gpm_warning ("Unable to get SRV_DPMS timeouts: %s", error->message);
		g_error_free (error);
		return;
	}
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmSrvDpms     *srv_dpms)
{
	if (strcmp (key, GPM_CONF_BATTERY_SLEEP_DISPLAY) == 0 ||
	    strcmp (key, GPM_CONF_AC_SLEEP_DISPLAY) == 0 ||
	    strcmp (key, GPM_CONF_AC_DPMS_METHOD) == 0 ||
	    strcmp (key, GPM_CONF_BATTERY_DPMS_METHOD) == 0) {
		gpm_srv_dpms_sync_policy (srv_dpms);
	}
}

/**
 * power_on_ac_changed_cb:
 * @power: The power class instance
 * @on_ac: if we are on AC power
 * @manager: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean      on_ac,
		       GpmSrvDpms      *srv_dpms)
{
	gpm_srv_dpms_sync_policy (srv_dpms);	
}

/**
 * idle_changed_cb:
 * @idle: The idle class instance
 * @mode: The idle mode, e.g. GPM_IDLE_MODE_SESSION
 * @manager: This class instance
 *
 * This callback is called when gnome-screensaver detects that the idle state
 * has changed. GPM_IDLE_MODE_SESSION is when the session has become inactive,
 * and GPM_IDLE_MODE_SYSTEM is where the session has become inactive, AND the
 * session timeout has elapsed for the idle action.
 **/
static void
idle_changed_cb (GpmIdle     *idle,
		 GpmIdleMode  mode,
		 GpmSrvDpms     *srv_dpms)
{
	GError  *error;

	if (mode == GPM_IDLE_MODE_NORMAL) {

		/* deactivate display power management */
		error = NULL;
		gpm_dpms_set_active (srv_dpms->priv->dpms, FALSE, &error);
		if (error) {
			gpm_debug ("Unable to set DPMS not active: %s", error->message);
		}

		/* sync timeouts */
		gpm_srv_dpms_sync_policy (srv_dpms);

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		/* activate display power management */
		error = NULL;
		gpm_dpms_set_active (srv_dpms->priv->dpms, TRUE, &error);
		if (error) {
			gpm_debug ("Unable to set DPMS active: %s", error->message);
		}

		/* sync timeouts */
		gpm_srv_dpms_sync_policy (srv_dpms);
	}
}

static void
gpm_srv_dpms_class_init (GpmSrvDpmsClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = gpm_srv_dpms_finalize;
	g_type_class_add_private (klass, sizeof (GpmSrvDpmsPrivate));
}

static void
gpm_srv_dpms_init (GpmSrvDpms *srv_dpms)
{
	srv_dpms->priv = GPM_SRV_DPMS_GET_PRIVATE (srv_dpms);

	srv_dpms->priv->conf = gpm_conf_new ();
	g_signal_connect (srv_dpms->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), srv_dpms);

	/* we use hal to see if we are a laptop */
	srv_dpms->priv->hal = gpm_hal_new ();

	/* master class */
	srv_dpms->priv->dpms = gpm_dpms_new ();

	/* we use power for the ac-adapter-changed */
	srv_dpms->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (srv_dpms->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), srv_dpms);

	/* watch for idle mode changes */
	srv_dpms->priv->idle = gpm_idle_new ();
	g_signal_connect (srv_dpms->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), srv_dpms);

	gpm_srv_dpms_sync_policy (srv_dpms);
}

static void
gpm_srv_dpms_finalize (GObject *object)
{
	GpmSrvDpms *srv_dpms;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_DPMS (object));

	srv_dpms = GPM_SRV_DPMS (object);

	g_return_if_fail (srv_dpms->priv != NULL);

	if (srv_dpms->priv->dpms != NULL) {
		g_object_unref (srv_dpms->priv->dpms);
	}
	if (srv_dpms->priv->conf != NULL) {
		g_object_unref (srv_dpms->priv->conf);
	}
	if (srv_dpms->priv->ac_adapter != NULL) {
		g_object_unref (srv_dpms->priv->ac_adapter);
	}
	if (srv_dpms->priv->hal != NULL) {
		g_object_unref (srv_dpms->priv->hal);
	}
	if (srv_dpms->priv->idle != NULL) {
		g_object_unref (srv_dpms->priv->idle);
	}

	G_OBJECT_CLASS (gpm_srv_dpms_parent_class)->finalize (object);
}

GpmSrvDpms *
gpm_srv_dpms_new (void)
{
	GpmSrvDpms *srv_dpms = NULL;

	/* only load if we have the hardware */
	if (gpm_dpms_has_hw () == TRUE) {
		srv_dpms = g_object_new (GPM_TYPE_SRV_DPMS, NULL);;
	}

	return srv_dpms;
}
