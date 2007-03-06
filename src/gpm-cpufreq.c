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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <glib/gi18n.h>

#include <libhal-gcpufreq.h>

#include "gpm-ac-adapter.h"
#include "gpm-conf.h"
#include "gpm-debug.h"
#include "gpm-cpufreq.h"

static void     gpm_cpufreq_class_init (GpmCpufreqClass *klass);
static void     gpm_cpufreq_init       (GpmCpufreq      *hal);
static void     gpm_cpufreq_finalize   (GObject	*object);

#define GPM_CPUFREQ_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CPUFREQ, GpmCpufreqPrivate))

struct GpmCpufreqPrivate
{
	HalGCpufreq		*hal_cpufreq;
	GpmConf			*conf;
	GpmAcAdapter		*ac_adapter;
};

G_DEFINE_TYPE (GpmCpufreq, gpm_cpufreq, G_TYPE_OBJECT)

/**
 * gpm_cpufreq_sync_policy:
 * @cpufreq: This class instance
 * @on_ac: If we are on AC power
 *
 * Changes the cpufreq policy if required
 **/
static gboolean
gpm_cpufreq_sync_policy (GpmCpufreq *cpufreq)
{
	gboolean cpufreq_consider_nice;
	GpmAcAdapterState state;
	guint cpufreq_performance;
	gchar *cpufreq_policy;
	HalGCpufreqType cpufreq_type;

	gpm_ac_adapter_get_state (cpufreq->priv->ac_adapter, &state);

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_bool (cpufreq->priv->conf, GPM_CONF_USE_NICE, &cpufreq_consider_nice);
		gpm_conf_get_string (cpufreq->priv->conf, GPM_CONF_AC_CPUFREQ_POLICY, &cpufreq_policy);
		gpm_conf_get_uint (cpufreq->priv->conf, GPM_CONF_AC_CPUFREQ_VALUE, &cpufreq_performance);
	} else {
		gpm_conf_get_bool (cpufreq->priv->conf, GPM_CONF_USE_NICE, &cpufreq_consider_nice);
		gpm_conf_get_string (cpufreq->priv->conf, GPM_CONF_BATTERY_CPUFREQ_POLICY, &cpufreq_policy);
		gpm_conf_get_uint (cpufreq->priv->conf, GPM_CONF_BATTERY_CPUFREQ_VALUE, &cpufreq_performance);
	}

	/* use enumerated value */
	cpufreq_type = hal_gcpufreq_string_to_enum (cpufreq_policy);
	g_free (cpufreq_policy);

	/* change to the right governer and settings */
	hal_gcpufreq_set_governor (cpufreq->priv->hal_cpufreq, cpufreq_type);
	hal_gcpufreq_set_consider_nice (cpufreq->priv->hal_cpufreq, cpufreq_consider_nice);
	hal_gcpufreq_set_performance (cpufreq->priv->hal_cpufreq, cpufreq_performance);
	return TRUE;
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmCpufreq  *cpufreq)
{
	/* if any change, just resync the whole lot */
	if (strcmp (key, GPM_CONF_AC_CPUFREQ_POLICY) == 0 ||
	    strcmp (key, GPM_CONF_AC_CPUFREQ_VALUE) == 0 ||
	    strcmp (key, GPM_CONF_BATTERY_CPUFREQ_POLICY) == 0 ||
	    strcmp (key, GPM_CONF_BATTERY_CPUFREQ_VALUE) == 0 ||
	    strcmp (key, GPM_CONF_USE_NICE) == 0) {

		gpm_cpufreq_sync_policy (cpufreq);
	}
}

/**
 * ac_adapter_changed_cb:
 * @ac_adapter: The ac_adapter class instance
 * @on_ac: if we are on AC power
 * @cpufreq: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean      on_ac,
		       GpmCpufreq  *cpufreq)
{
	gpm_cpufreq_sync_policy (cpufreq);
}

/**
 * gpm_cpufreq_class_init:
 * @klass: This class instance
 **/
static void
gpm_cpufreq_class_init (GpmCpufreqClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_cpufreq_finalize;
	g_type_class_add_private (klass, sizeof (GpmCpufreqPrivate));
}

/**
 * gpm_cpufreq_init:
 *
 * @cpufreq: This class instance
 **/
static void
gpm_cpufreq_init (GpmCpufreq *cpufreq)
{
	cpufreq->priv = GPM_CPUFREQ_GET_PRIVATE (cpufreq);

	/* we use cpufreq as the master class */
	cpufreq->priv->hal_cpufreq = hal_gcpufreq_new ();

	/* get changes from gconf */
	cpufreq->priv->conf = gpm_conf_new ();
	g_signal_connect (cpufreq->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), cpufreq);

	/* we use ac_adapter for the ac-adapter-changed signal */
	cpufreq->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (cpufreq->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), cpufreq);

	/* sync policy */
	gpm_cpufreq_sync_policy (cpufreq);
}

/**
 * gpm_cpufreq_finalize:
 * @object: This class instance
 **/
static void
gpm_cpufreq_finalize (GObject *object)
{
	GpmCpufreq *cpufreq;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CPUFREQ (object));

	cpufreq = GPM_CPUFREQ (object);
	cpufreq->priv = GPM_CPUFREQ_GET_PRIVATE (cpufreq);

	if (cpufreq->priv->hal_cpufreq != NULL) {
		g_object_unref (cpufreq->priv->hal_cpufreq);
	}
	if (cpufreq->priv->conf != NULL) {
		g_object_unref (cpufreq->priv->conf);
	}
	if (cpufreq->priv->ac_adapter != NULL) {
		g_object_unref (cpufreq->priv->ac_adapter);
	}
	G_OBJECT_CLASS (gpm_cpufreq_parent_class)->finalize (object);
}

/**
 * gpm_cpufreq_new:
 * Return value: new GpmCpufreq instance.
 **/
GpmCpufreq *
gpm_cpufreq_new (void)
{
	GpmCpufreq *cpufreq = NULL;

	/* only load if we have the hardware */
	if (hal_gcpufreq_has_hw() == TRUE) {
		cpufreq = g_object_new (GPM_TYPE_CPUFREQ, NULL);
	}

	return cpufreq;
}

