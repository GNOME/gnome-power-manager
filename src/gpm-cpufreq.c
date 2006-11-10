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

#include "gpm-marshal.h"
#include "gpm-proxy.h"
#include "gpm-hal.h"
#include "gpm-cpufreq.h"
#include "gpm-debug.h"

static void     gpm_cpufreq_class_init (GpmCpuFreqClass *klass);
static void     gpm_cpufreq_init       (GpmCpuFreq      *hal);
static void     gpm_cpufreq_finalize   (GObject	*object);

#define GPM_CPUFREQ_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CPUFREQ, GpmCpuFreqPrivate))

struct GpmCpuFreqPrivate
{
	GpmProxy		*gproxy;
	GpmHal			*hal;
	guint			 available_governors;
	GpmCpuFreqEnum		 current_governor;
};

static gpointer      gpm_cpufreq_object = NULL;

G_DEFINE_TYPE (GpmCpuFreq, gpm_cpufreq, G_TYPE_OBJECT)

/**
 * gpm_cpufreq_string_to_enum:
 * @governor: The cpufreq kernel governor, e.g. "powersave"
 * Return value: The GpmCpuFreqEnum value, e.g. GPM_CPUFREQ_POWERSAVE
 **/
GpmCpuFreqEnum
gpm_cpufreq_string_to_enum (const gchar *governor)
{
	GpmCpuFreqEnum cpufreq_type = GPM_CPUFREQ_UNKNOWN;
	g_return_val_if_fail (governor != NULL, FALSE);
	if (strcmp (governor, CODE_CPUFREQ_ONDEMAND) == 0) {
		cpufreq_type = GPM_CPUFREQ_ONDEMAND;
	} else if (strcmp (governor, CODE_CPUFREQ_CONSERVATIVE) == 0) {
		cpufreq_type = GPM_CPUFREQ_CONSERVATIVE;
	} else if (strcmp (governor, CODE_CPUFREQ_POWERSAVE) == 0) {
		cpufreq_type = GPM_CPUFREQ_POWERSAVE;
	} else if (strcmp (governor, CODE_CPUFREQ_USERSPACE) == 0) {
		cpufreq_type = GPM_CPUFREQ_USERSPACE;
	} else if (strcmp (governor, CODE_CPUFREQ_PERFORMANCE) == 0) {
		cpufreq_type = GPM_CPUFREQ_PERFORMANCE;
	} else if (strcmp (governor, CODE_CPUFREQ_NOTHING) == 0) {
		cpufreq_type = GPM_CPUFREQ_NOTHING;
	}
	return cpufreq_type;
}

/**
 * gpm_cpufreq_string_to_enum:
 * @cpufreq_type: The GpmCpuFreqEnum value, e.g. GPM_CPUFREQ_POWERSAVE
 * Return value: The cpufreq kernel governor, e.g. "powersave"
 **/
const gchar *
gpm_cpufreq_enum_to_string (GpmCpuFreqEnum cpufreq_type)
{
	const char *governor;
	if (cpufreq_type == GPM_CPUFREQ_ONDEMAND) {
		governor = CODE_CPUFREQ_ONDEMAND;
	} else if (cpufreq_type == GPM_CPUFREQ_CONSERVATIVE) {
		governor = CODE_CPUFREQ_CONSERVATIVE;
	} else if (cpufreq_type == GPM_CPUFREQ_POWERSAVE) {
		governor = CODE_CPUFREQ_POWERSAVE;
	} else if (cpufreq_type == GPM_CPUFREQ_USERSPACE) {
		governor = CODE_CPUFREQ_USERSPACE;
	} else if (cpufreq_type == GPM_CPUFREQ_PERFORMANCE) {
		governor = CODE_CPUFREQ_PERFORMANCE;
	} else if (cpufreq_type == GPM_CPUFREQ_NOTHING) {
		governor = CODE_CPUFREQ_NOTHING;
	} else {
		governor = "unknown";
	}
	return governor;
}

/**
 * gpm_cpufreq_set_performance:
 *
 * @cpufreq: This class instance
 * @performance: The percentage perfomance figure
 * Return value: If the method succeeded
 **/
gboolean
gpm_cpufreq_set_performance (GpmCpuFreq *cpufreq, guint performance)
{
	GError *error = NULL;
	gboolean ret;
	GpmCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (performance >= 0, FALSE);
	g_return_val_if_fail (performance <= 100, FALSE);

	gpm_debug ("Doing SetCPUFreqPerformance (%i)", performance);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_cpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_USERSPACE) {
		gpm_debug ("not valid for current governor!");
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "SetCPUFreqPerformance", &error,
				 G_TYPE_INT, performance,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("SetCPUFreqPerformance failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_cpufreq_set_governor:
 *
 * @cpufreq: This class instance
 * @cpufreq_type: The CPU governor type, e.g. GPM_CPUFREQ_CONSERVATIVE
 * Return value: If the method succeeded
 **/
gboolean
gpm_cpufreq_set_governor (GpmCpuFreq    *cpufreq,
			  GpmCpuFreqEnum cpufreq_type)
{
	GError *error = NULL;
	gboolean ret;
	const gchar *governor;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type != GPM_CPUFREQ_UNKNOWN, FALSE);

	governor = gpm_cpufreq_enum_to_string (cpufreq_type);
	g_return_val_if_fail (governor != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing SetCPUFreqGovernor (%s)", governor);
	ret = dbus_g_proxy_call (proxy, "SetCPUFreqGovernor", &error,
				 G_TYPE_STRING, governor,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("SetCPUFreqGovernor failed!");
		return FALSE;
	}

	/* save the cache */
	cpufreq->priv->current_governor = cpufreq_type;
	return TRUE;
}

/**
 * gpm_cpufreq_get_governors:
 *
 * @cpufreq: This class instance
 * @cpufreq_type: Return variable, The CPU governor type as an combined bitwise type
 * Return value: If the method succeeded
 **/
gboolean
gpm_cpufreq_get_governors (GpmCpuFreq     *cpufreq,
			   GpmCpuFreqEnum *cpufreq_type)
{
	GError *error = NULL;
	gboolean ret;
	char **strlist;
	int i = 0;
	DBusGProxy *proxy;
	GpmCpuFreqEnum types = GPM_CPUFREQ_UNKNOWN;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		*cpufreq_type = GPM_CPUFREQ_UNKNOWN;
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqAvailableGovernors");
	ret = dbus_g_proxy_call (proxy, "GetCPUFreqAvailableGovernors", &error,
				 G_TYPE_INVALID,
				 G_TYPE_STRV, &strlist,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetCPUFreqAvailableGovernors failed!");
		*cpufreq_type = GPM_CPUFREQ_UNKNOWN;
		return FALSE;
	}

	/* treat as binary flags */
	while (strlist && strlist[i]) {
		types += gpm_cpufreq_string_to_enum (strlist[i]);
		++i;
	}

	/* when we have conservative and ondemand available, only expose
	   conservative in the UI. They are too similar. */
	if (types & GPM_CPUFREQ_ONDEMAND && types & GPM_CPUFREQ_CONSERVATIVE) {
		types -= GPM_CPUFREQ_ONDEMAND;
	}

	*cpufreq_type = types;
	cpufreq->priv->available_governors = i;
	return TRUE;
}

/**
 * gpm_cpufreq_get_number_governors:
 *
 * @cpufreq: This class instance
 * @use_cache: if we should force a cache update
 * Return value: the number of available governors
 **/
guint
gpm_cpufreq_get_number_governors (GpmCpuFreq *cpufreq,
				  gboolean    use_cache)
{
	GpmCpuFreqEnum cpufreq_type;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);

	if (use_cache == FALSE || cpufreq->priv->available_governors == -1) {
		gpm_cpufreq_get_governors (cpufreq, &cpufreq_type);
	}
	return cpufreq->priv->available_governors;
}

/**
 * gpm_cpufreq_get_consider_nice:
 *
 * @cpufreq: This class instance
 * @consider_nice: Return variable, if consider niced processes
 * Return value: If the method succeeded
 **/
gboolean
gpm_cpufreq_get_consider_nice (GpmCpuFreq *cpufreq,
			       gboolean      *consider_nice)
{
	GError *error = NULL;
	gboolean ret;
	GpmCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (consider_nice != NULL, FALSE);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_cpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_ONDEMAND &&
	    cpufreq->priv->current_governor != GPM_CPUFREQ_CONSERVATIVE) {
		gpm_debug ("not valid for current governor!");
		*consider_nice = FALSE;
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqConsiderNice");
	ret = dbus_g_proxy_call (proxy, "GetCPUFreqConsiderNice", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, consider_nice,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetCPUFreqConsiderNice failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_cpufreq_get_performance:
 *
 * @cpufreq: This class instance
 * @performance: Return variable, the percentage performance
 * Return value: If the method succeeded
 **/
gboolean
gpm_cpufreq_get_performance (GpmCpuFreq *cpufreq,
		             guint         *performance)
{
	GError *error = NULL;
	gboolean ret;
	GpmCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (performance != NULL, FALSE);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_cpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_USERSPACE) {
		gpm_debug ("not valid for current governor!");
		*performance = -1;
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqPerformance");
	ret = dbus_g_proxy_call (proxy, "GetCPUFreqPerformance", &error,
				 G_TYPE_INVALID,
				 G_TYPE_INT, performance,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetCPUFreqPerformance failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_cpufreq_get_governor:
 *
 * @cpufreq: This class instance
 * @cpufreq_type: Return variable, the governor type, e.g. GPM_CPUFREQ_POWERSAVE
 * Return value: If the method succeeded
 **/
gboolean
gpm_cpufreq_get_governor (GpmCpuFreq     *cpufreq,
			  GpmCpuFreqEnum *cpufreq_type)
{
	GError *error = NULL;
	gboolean ret;
	gchar *governor;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type, FALSE);

	*cpufreq_type = GPM_CPUFREQ_UNKNOWN;

	/* use the cache */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_UNKNOWN) {
		return cpufreq->priv->current_governor;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqGovernor");
	ret = dbus_g_proxy_call (proxy, "GetCPUFreqGovernor", &error,
				 G_TYPE_INVALID,
				 G_TYPE_STRING, &governor,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetCPUFreqGovernor failed!");
		return FALSE;
	}

	/* convert to enumerated type */
	if (governor != NULL) {
		*cpufreq_type = gpm_cpufreq_string_to_enum (governor);
		cpufreq->priv->current_governor = *cpufreq_type;
		g_free (governor);
	}

	return TRUE;
}

/**
 * gpm_cpufreq_set_consider_nice:
 *
 * @cpufreq: This class instance
 * @enable: True to consider nice processes
 * Return value: If the method succeeded
 **/
gboolean
gpm_cpufreq_set_consider_nice (GpmCpuFreq *cpufreq,
			       gboolean    consider_nice)
{
	GError *error = NULL;
	gboolean ret;
	GpmCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CPUFREQ (cpufreq), FALSE);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_cpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_ONDEMAND &&
	    cpufreq->priv->current_governor != GPM_CPUFREQ_CONSERVATIVE) {
		gpm_debug ("not valid for current governor!");
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing SetCPUFreqConsiderNice (%i)", consider_nice);
	ret = dbus_g_proxy_call (proxy, "SetCPUFreqConsiderNice", &error,
				 G_TYPE_BOOLEAN, consider_nice,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("SetCPUFreqConsiderNice failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_cpufreq_class_init:
 * @klass: This class instance
 **/
static void
gpm_cpufreq_class_init (GpmCpuFreqClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_cpufreq_finalize;
	g_type_class_add_private (klass, sizeof (GpmCpuFreqPrivate));
}

/**
 * gpm_cpufreq_init:
 *
 * @cpufreq: This class instance
 **/
static void
gpm_cpufreq_init (GpmCpuFreq *cpufreq)
{
	cpufreq->priv = GPM_CPUFREQ_GET_PRIVATE (cpufreq);

	cpufreq->priv->hal = gpm_hal_new ();

	cpufreq->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (cpufreq->priv->gproxy,
			  GPM_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  HAL_ROOT_COMPUTER,
			  HAL_DBUS_INTERFACE_CPUFREQ);

	/* set defaults */
	cpufreq->priv->available_governors = -1;
	cpufreq->priv->current_governor = GPM_CPUFREQ_UNKNOWN;
}

/**
 * gpm_cpufreq_finalize:
 * @object: This class instance
 **/
static void
gpm_cpufreq_finalize (GObject *object)
{
	GpmCpuFreq *cpufreq;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CPUFREQ (object));

	cpufreq = GPM_CPUFREQ (object);
	cpufreq->priv = GPM_CPUFREQ_GET_PRIVATE (cpufreq);

	if (cpufreq->priv->hal != NULL) {
		g_object_unref (cpufreq->priv->hal);
	}
	if (cpufreq->priv->gproxy != NULL) {
		g_object_unref (cpufreq->priv->gproxy);
	}
	G_OBJECT_CLASS (gpm_cpufreq_parent_class)->finalize (object);
}

/**
 * gpm_cpufreq_has_hw:
 *
 * Self contained function that works out if we have the hardware.
 * If not, we return FALSE and the module is unloaded.
 **/
gboolean
gpm_cpufreq_has_hw (void)
{
	GpmHal *hal;
	gchar **names;
	gboolean ret = TRUE;

	/* okay, as singleton - so we don't allocate more memory */
	hal = gpm_hal_new ();
	gpm_hal_device_find_capability (hal, "cpufreq_control", &names);

	/* nothing found */
	if (names == NULL || names[0] == NULL) {
		ret = FALSE;
	}

	gpm_hal_free_capability (hal, names);
	g_object_unref (hal);
	return ret;
}

/**
 * gpm_cpufreq_new:
 * Return value: new GpmCpuFreq instance.
 **/
GpmCpuFreq *
gpm_cpufreq_new (void)
{
	/* only load an instance of this module if we have the hardware */
	if (gpm_cpufreq_has_hw () == FALSE) {
		return NULL;
	}

	if (gpm_cpufreq_object) {
		g_object_ref (gpm_cpufreq_object);
	} else {
		gpm_cpufreq_object = g_object_new (GPM_TYPE_CPUFREQ, NULL);
		g_object_add_weak_pointer (gpm_cpufreq_object,
					   (gpointer *) &gpm_cpufreq_object);
	}
	return GPM_CPUFREQ (gpm_cpufreq_object);
}
