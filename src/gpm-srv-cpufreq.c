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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <glib/gi18n.h>

#include "gpm-ac-adapter.h"
#include "gpm-cpufreq.h"
#include "gpm-conf.h"
#include "gpm-debug.h"
#include "gpm-srv-cpufreq.h"

static void     gpm_srv_cpufreq_class_init (GpmSrvCpuFreqClass *klass);
static void     gpm_srv_cpufreq_init       (GpmSrvCpuFreq      *hal);
static void     gpm_srv_cpufreq_finalize   (GObject	*object);

#define GPM_SRV_CPUFREQ_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SRV_CPUFREQ, GpmSrvCpuFreqPrivate))

struct GpmSrvCpuFreqPrivate
{
	GpmCpuFreq		*cpufreq;
	GpmConf			*conf;
	GpmAcAdapter		*ac_adapter;
};

G_DEFINE_TYPE (GpmSrvCpuFreq, gpm_srv_cpufreq, G_TYPE_OBJECT)

/**
 * gpm_srv_cpufreq_sync_policy:
 * @srv_cpufreq: This class instance
 * @on_ac: If we are on AC power
 *
 * Changes the srv_cpufreq policy if required
 **/
static gboolean
gpm_srv_cpufreq_sync_policy (GpmSrvCpuFreq *srv_cpufreq)
{
	gboolean cpufreq_consider_nice;
	GpmAcAdapterState state;
	guint cpufreq_performance;
	gchar *cpufreq_policy;
	GpmCpuFreqEnum srv_cpufreq_type;

	gpm_ac_adapter_get_state (srv_cpufreq->priv->ac_adapter, &state);

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_bool (srv_cpufreq->priv->conf, GPM_CONF_USE_NICE, &cpufreq_consider_nice);
		gpm_conf_get_string (srv_cpufreq->priv->conf, GPM_CONF_AC_CPUFREQ_POLICY, &cpufreq_policy);
		gpm_conf_get_uint (srv_cpufreq->priv->conf, GPM_CONF_AC_CPUFREQ_VALUE, &cpufreq_performance);
	} else {
		gpm_conf_get_bool (srv_cpufreq->priv->conf, GPM_CONF_USE_NICE, &cpufreq_consider_nice);
		gpm_conf_get_string (srv_cpufreq->priv->conf, GPM_CONF_BATTERY_CPUFREQ_POLICY, &cpufreq_policy);
		gpm_conf_get_uint (srv_cpufreq->priv->conf, GPM_CONF_BATTERY_CPUFREQ_VALUE, &cpufreq_performance);
	}

	/* use enumerated value */
	srv_cpufreq_type = gpm_cpufreq_string_to_enum (cpufreq_policy);
	g_free (cpufreq_policy);

	/* change to the right governer and settings */
	gpm_cpufreq_set_governor (srv_cpufreq->priv->cpufreq, srv_cpufreq_type);
	gpm_cpufreq_set_consider_nice (srv_cpufreq->priv->cpufreq, cpufreq_consider_nice);
	gpm_cpufreq_set_performance (srv_cpufreq->priv->cpufreq, cpufreq_performance);
	return TRUE;
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf       *conf,
		     const gchar   *key,
		     GpmSrvCpuFreq *srv_cpufreq)
{
	/* if any change, just resync the whole lot */
	if (strcmp (key, GPM_CONF_AC_CPUFREQ_POLICY) == 0 ||
	    strcmp (key, GPM_CONF_AC_CPUFREQ_VALUE) == 0 ||
	    strcmp (key, GPM_CONF_BATTERY_CPUFREQ_POLICY) == 0 ||
	    strcmp (key, GPM_CONF_BATTERY_CPUFREQ_VALUE) == 0 ||
	    strcmp (key, GPM_CONF_USE_NICE) == 0) {

		gpm_srv_cpufreq_sync_policy (srv_cpufreq);
	}
}

/**
 * ac_adapter_changed_cb:
 * @ac_adapter: The ac_adapter class instance
 * @on_ac: if we are on AC power
 * @srv_cpufreq: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter   *ac_adapter,
			gboolean       on_ac,
			GpmSrvCpuFreq *srv_cpufreq)
{
	gpm_srv_cpufreq_sync_policy (srv_cpufreq);
}

/**
 * gpm_srv_cpufreq_class_init:
 * @klass: This class instance
 **/
static void
gpm_srv_cpufreq_class_init (GpmSrvCpuFreqClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_srv_cpufreq_finalize;
	g_type_class_add_private (klass, sizeof (GpmSrvCpuFreqPrivate));
}

/**
 * gpm_srv_cpufreq_init:
 *
 * @srv_cpufreq: This class instance
 **/
static void
gpm_srv_cpufreq_init (GpmSrvCpuFreq *srv_cpufreq)
{
	srv_cpufreq->priv = GPM_SRV_CPUFREQ_GET_PRIVATE (srv_cpufreq);

	/* we use cpufreq as the master class */
	srv_cpufreq->priv->cpufreq = gpm_cpufreq_new ();

	/* get changes from gconf */
	srv_cpufreq->priv->conf = gpm_conf_new ();
	g_signal_connect (srv_cpufreq->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), srv_cpufreq);

	/* we use ac_adapter for the ac-adapter-changed signal */
	srv_cpufreq->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (srv_cpufreq->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), srv_cpufreq);

	/* sync policy */
	gpm_srv_cpufreq_sync_policy (srv_cpufreq);
}

/**
 * gpm_srv_cpufreq_finalize:
 * @object: This class instance
 **/
static void
gpm_srv_cpufreq_finalize (GObject *object)
{
	GpmSrvCpuFreq *srv_cpufreq;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_CPUFREQ (object));

	srv_cpufreq = GPM_SRV_CPUFREQ (object);
	srv_cpufreq->priv = GPM_SRV_CPUFREQ_GET_PRIVATE (srv_cpufreq);

	if (srv_cpufreq->priv->cpufreq != NULL) {
		g_object_unref (srv_cpufreq->priv->cpufreq);
	}
	if (srv_cpufreq->priv->conf != NULL) {
		g_object_unref (srv_cpufreq->priv->conf);
	}
	if (srv_cpufreq->priv->ac_adapter != NULL) {
		g_object_unref (srv_cpufreq->priv->ac_adapter);
	}
	G_OBJECT_CLASS (gpm_srv_cpufreq_parent_class)->finalize (object);
}

/**
 * gpm_srv_cpufreq_new:
 * Return value: new GpmSrvCpuFreq instance.
 **/
GpmSrvCpuFreq *
gpm_srv_cpufreq_new (void)
{
	GpmSrvCpuFreq *srv_cpufreq = NULL;

	/* only load if we have the hardware */
	if (gpm_cpufreq_has_hw() == TRUE) {
		srv_cpufreq = g_object_new (GPM_TYPE_SRV_CPUFREQ, NULL);
	}

	return srv_cpufreq;
}
