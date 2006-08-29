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

#include "config.h"

#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-marshal.h"
#include "gpm-proxy.h"
#include "gpm-hal.h"
#include "gpm-hal-cpufreq.h"
#include "gpm-debug.h"

static void     gpm_hal_cpufreq_class_init (GpmHalCpuFreqClass *klass);
static void     gpm_hal_cpufreq_init       (GpmHalCpuFreq      *hal);
static void     gpm_hal_cpufreq_finalize   (GObject	*object);

#define GPM_GPM_CPUFREQ_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_CPUFREQ, GpmHalCpuFreqPrivate))

struct GpmHalCpuFreqPrivate
{
	gboolean		 has_hardware;
	GpmProxy		*gproxy;
	GpmHal			*hal;
	guint			 available_governors;
	GpmHalCpuFreqEnum	 current_governor;
};

static gpointer      gpm_hal_cpufreq_object = NULL;

G_DEFINE_TYPE (GpmHalCpuFreq, gpm_hal_cpufreq, G_TYPE_OBJECT)

/**
 * gpm_hal_cpufreq_string_to_enum:
 * @governor: The cpufreq kernel governor, e.g. "powersave"
 * Return value: The GpmHalCpuFreqEnum value, e.g. GPM_CPUFREQ_POWERSAVE
 **/
GpmHalCpuFreqEnum
gpm_hal_cpufreq_string_to_enum (const gchar *governor)
{
	GpmHalCpuFreqEnum cpufreq_type = GPM_CPUFREQ_UNKNOWN;
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
 * gpm_hal_cpufreq_string_to_enum:
 * @cpufreq_type: The GpmHalCpuFreqEnum value, e.g. GPM_CPUFREQ_POWERSAVE
 * Return value: The cpufreq kernel governor, e.g. "powersave"
 **/
const gchar *
gpm_hal_cpufreq_enum_to_string (GpmHalCpuFreqEnum cpufreq_type)
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
 * gpm_hal_cpufreq_handle_error:
 * @ret: return value (==0 in this case)
 * @method: The method name
 * Return value: If the method succeeded
 **/
static gboolean
gpm_hal_cpufreq_handle_error (GError *error, const gchar *method)
{
	gboolean retval = TRUE;

	g_return_val_if_fail (method != NULL, FALSE);

	if (error) {
		gpm_debug ("%s failed\n(%s)", method, error->message);
		retval = FALSE;
		g_error_free (error);
	}
	return retval;
}

/**
 * gpm_hal_cpufreq_has_hardware:
 *
 * @cpufreq: This cpufreq class instance
 * Return value: If we have cpufreq support in hal
 **/
gboolean
gpm_hal_cpufreq_has_hardware (GpmHalCpuFreq *cpufreq)
{
	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);
	return cpufreq->priv->has_hardware;
}

/**
 * gpm_hal_cpufreq_set_performance:
 *
 * @cpufreq: This cpufreq class instance
 * @performance: The percentage perfomance figure
 * Return value: If the method succeeded
 **/
gboolean
gpm_hal_cpufreq_set_performance (GpmHalCpuFreq *cpufreq, guint performance)
{
	GError *error = NULL;
	gboolean retval;
	GpmHalCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (performance >= 0, FALSE);
	g_return_val_if_fail (performance <= 100, FALSE);

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		return FALSE;
	}

	gpm_debug ("Doing SetCPUFreqPerformance (%i)", performance);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_hal_cpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_USERSPACE) {
		gpm_debug ("not valid for current governor!");
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	dbus_g_proxy_call (proxy, "SetCPUFreqPerformance", &error,
			   G_TYPE_INT, performance, G_TYPE_INVALID,
			   G_TYPE_INVALID);
	retval = gpm_hal_cpufreq_handle_error (error, "SetCPUFreqPerformance");
	return retval;
}

/**
 * gpm_hal_cpufreq_set_governor:
 *
 * @cpufreq: This cpufreq class instance
 * @cpufreq_type: The CPU governor type, e.g. GPM_CPUFREQ_CONSERVATIVE
 * Return value: If the method succeeded
 **/
gboolean
gpm_hal_cpufreq_set_governor (GpmHalCpuFreq    *cpufreq,
			      GpmHalCpuFreqEnum cpufreq_type)
{
	GError *error = NULL;
	gboolean retval;
	const gchar *governor;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type != GPM_CPUFREQ_UNKNOWN, FALSE);
	governor = gpm_hal_cpufreq_enum_to_string (cpufreq_type);
	g_return_val_if_fail (governor != NULL, FALSE);

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing SetCPUFreqGovernor (%s)", governor);
	dbus_g_proxy_call (proxy, "SetCPUFreqGovernor", &error,
			   G_TYPE_STRING, governor, G_TYPE_INVALID,
			   G_TYPE_INVALID);
	retval = gpm_hal_cpufreq_handle_error (error, "SetCPUFreqGovernor");

	/* save the cache */
	if (retval == TRUE) {
		cpufreq->priv->current_governor = cpufreq_type;
	}

	return retval;
}

/**
 * gpm_hal_cpufreq_get_governors:
 *
 * @cpufreq: This cpufreq class instance
 * @cpufreq_type: Return variable, The CPU governor type as an combined bitwise type
 * Return value: If the method succeeded
 **/
gboolean
gpm_hal_cpufreq_get_governors (GpmHalCpuFreq     *cpufreq,
			       GpmHalCpuFreqEnum *cpufreq_type)
{
	GError *error = NULL;
	gboolean retval;
	char **strlist;
	int i = 0;
	DBusGProxy *proxy;

	*cpufreq_type = GPM_CPUFREQ_UNKNOWN;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type != NULL, FALSE);

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqAvailableGovernors");
	dbus_g_proxy_call (proxy, "GetCPUFreqAvailableGovernors", &error,
			   G_TYPE_INVALID,
			   G_TYPE_STRV, &strlist,
			   G_TYPE_INVALID);
	retval = gpm_hal_cpufreq_handle_error (error, "GetCPUFreqAvailableGovernors");

	/* treat as binary flags */
	if (retval) {
		while (strlist && strlist[i]) {
			*cpufreq_type += gpm_hal_cpufreq_string_to_enum (strlist[i]);
			++i;
		}
	}
	cpufreq->priv->available_governors = i;

	return retval;
}

/**
 * gpm_hal_cpufreq_get_number_governors:
 *
 * @cpufreq: This cpufreq class instance
 * @use_cache: if we should force a cache update
 * Return value: the number of available governors
 **/
guint
gpm_hal_cpufreq_get_number_governors (GpmHalCpuFreq *cpufreq,
				      gboolean       use_cache)
{
	GpmHalCpuFreqEnum cpufreq_type;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		return 0;
	}

	if (use_cache == FALSE || cpufreq->priv->available_governors == -1) {
		gpm_hal_cpufreq_get_governors (cpufreq, &cpufreq_type);
	}
	return cpufreq->priv->available_governors;
}

/**
 * gpm_hal_cpufreq_get_consider_nice:
 *
 * @cpufreq: This cpufreq class instance
 * @consider_nice: Return variable, if consider niced processes
 * Return value: If the method succeeded
 **/
gboolean
gpm_hal_cpufreq_get_consider_nice (GpmHalCpuFreq *cpufreq,
				   gboolean      *consider_nice)
{
	GError *error = NULL;
	gboolean retval;
	GpmHalCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (consider_nice != NULL, FALSE);

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		*consider_nice = FALSE;
		return FALSE;
	}

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_hal_cpufreq_get_governor (cpufreq, &cpufreq_type);
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
		g_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqConsiderNice");
	dbus_g_proxy_call (proxy, "GetCPUFreqConsiderNice", &error,
			   G_TYPE_INVALID,
			   G_TYPE_BOOLEAN, consider_nice,
			   G_TYPE_INVALID);
	retval = gpm_hal_cpufreq_handle_error (error, "GetCPUFreqConsiderNice");
	return retval;
}

/**
 * gpm_hal_cpufreq_get_performance:
 *
 * @cpufreq: This cpufreq class instance
 * @performance: Return variable, the percentage performance
 * Return value: If the method succeeded
 **/
gboolean
gpm_hal_cpufreq_get_performance (GpmHalCpuFreq *cpufreq,
				 guint         *performance)
{
	GError *error = NULL;
	gboolean retval;
	GpmHalCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (performance != NULL, FALSE);

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		*performance = -1;
		return FALSE;
	}

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_hal_cpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_USERSPACE) {
		gpm_debug ("not valid for current governor!");
		*performance = -1;
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqPerformance");
	dbus_g_proxy_call (proxy, "GetCPUFreqPerformance", &error,
			   G_TYPE_INVALID,
			   G_TYPE_INT, performance,
			   G_TYPE_INVALID);
	retval = gpm_hal_cpufreq_handle_error (error, "GetCPUFreqPerformance");
	return retval;
}

/**
 * gpm_hal_cpufreq_get_governor:
 *
 * @cpufreq: This cpufreq class instance
 * @cpufreq_type: Return variable, the governor type, e.g. GPM_CPUFREQ_POWERSAVE
 * Return value: If the method succeeded
 **/
gboolean
gpm_hal_cpufreq_get_governor (GpmHalCpuFreq     *cpufreq,
			      GpmHalCpuFreqEnum *cpufreq_type)
{
	GError *error = NULL;
	gboolean retval;
	gchar *governor;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type, FALSE);

	*cpufreq_type = GPM_CPUFREQ_UNKNOWN;

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		return FALSE;
	}

	/* use the cache */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_UNKNOWN) {
		return cpufreq->priv->current_governor;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing GetCPUFreqGovernor");
	dbus_g_proxy_call (proxy, "GetCPUFreqGovernor", &error,
			   G_TYPE_INVALID,
			   G_TYPE_STRING, &governor,
			   G_TYPE_INVALID);
	retval = gpm_hal_cpufreq_handle_error (error, "GetCPUFreqGovernor");

	/* convert to enumerated type */
	if (retval == TRUE && governor != NULL) {
		*cpufreq_type = gpm_hal_cpufreq_string_to_enum (governor);
		cpufreq->priv->current_governor = *cpufreq_type;
	}

	return retval;
}

/**
 * gpm_hal_cpufreq_set_consider_nice:
 *
 * @cpufreq: This cpufreq class instance
 * @enable: True to consider nice processes
 * Return value: If the method succeeded
 **/
gboolean
gpm_hal_cpufreq_set_consider_nice (GpmHalCpuFreq *cpufreq,
				   gboolean       consider_nice)
{
	GError *error = NULL;
	gboolean retval;
	GpmHalCpuFreqEnum cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_CPUFREQ (cpufreq), FALSE);

	/* do we support speedstep and have a new enough hal? */
	if (! cpufreq->priv->has_hardware) {
		return FALSE;
	}

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == GPM_CPUFREQ_UNKNOWN) {
		gpm_hal_cpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != GPM_CPUFREQ_ONDEMAND &&
	    cpufreq->priv->current_governor != GPM_CPUFREQ_CONSERVATIVE) {
		gpm_debug ("not valid for current governor!");
		return FALSE;
	}

	proxy = gpm_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	gpm_debug ("Doing SetCPUFreqConsiderNice (%i)", consider_nice);
	dbus_g_proxy_call (proxy, "SetCPUFreqConsiderNice", &error,
			   G_TYPE_BOOLEAN, consider_nice, G_TYPE_INVALID,
			   G_TYPE_INVALID);
	retval = gpm_hal_cpufreq_handle_error (error, "SetCPUFreqConsiderNice");
	return retval;
}

/**
 * gpm_hal_cpufreq_class_init:
 * @klass: This cpufreq class instance
 **/
static void
gpm_hal_cpufreq_class_init (GpmHalCpuFreqClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_hal_cpufreq_finalize;
	g_type_class_add_private (klass, sizeof (GpmHalCpuFreqPrivate));
}

/**
 * gpm_hal_cpufreq_init:
 *
 * @cpufreq: This cpufreq class instance
 **/
static void
gpm_hal_cpufreq_init (GpmHalCpuFreq *cpufreq)
{
	int num_caps;

	cpufreq->priv = GPM_GPM_CPUFREQ_GET_PRIVATE (cpufreq);

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
	cpufreq->priv->has_hardware = FALSE;

	num_caps = gpm_hal_num_devices_of_capability (cpufreq->priv->hal,
						      "cpufreq_control");

	/* if we have cpufreq_control then we can use hal for cpufreq control */
	if (num_caps > 0) {
		cpufreq->priv->has_hardware = TRUE;
	}
}

/**
 * gpm_hal_cpufreq_finalize:
 * @object: This cpufreq class instance
 **/
static void
gpm_hal_cpufreq_finalize (GObject *object)
{
	GpmHalCpuFreq *cpufreq;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL_CPUFREQ (object));

	cpufreq = GPM_HAL_CPUFREQ (object);
	cpufreq->priv = GPM_GPM_CPUFREQ_GET_PRIVATE (cpufreq);

	g_object_unref (cpufreq->priv->hal);
	g_object_unref (cpufreq->priv->gproxy);

	G_OBJECT_CLASS (gpm_hal_cpufreq_parent_class)->finalize (object);
}

/**
 * gpm_hal_cpufreq_new:
 * Return value: new GpmHalCpuFreq instance.
 **/
GpmHalCpuFreq *
gpm_hal_cpufreq_new (void)
{
	if (gpm_hal_cpufreq_object) {
		g_object_ref (gpm_hal_cpufreq_object);
	} else {
		gpm_hal_cpufreq_object = g_object_new (GPM_TYPE_HAL_CPUFREQ, NULL);
		g_object_add_weak_pointer (gpm_hal_cpufreq_object,
					   (gpointer *) &gpm_hal_cpufreq_object);
	}
	return GPM_HAL_CPUFREQ (gpm_hal_cpufreq_object);
}
