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
#include <libdbus-proxy.h>

#include "libhal-marshal.h"
#include "libhal-gpower.h"
#include "libhal-gdevice.h"
#include "libhal-gcpufreq.h"
#include "libhal-gmanager.h"

static void     hal_gcpufreq_class_init (HalGCpufreqClass *klass);
static void     hal_gcpufreq_init       (HalGCpufreq      *hal);
static void     hal_gcpufreq_finalize   (GObject	  *object);

#define LIBHAL_CPUFREQ_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), LIBHAL_TYPE_CPUFREQ, HalGCpufreqPrivate))

struct HalGCpufreqPrivate
{
	DbusProxy		*gproxy;
	guint			 available_governors;
	HalGCpufreqType		 current_governor;
};

G_DEFINE_TYPE (HalGCpufreq, hal_gcpufreq, G_TYPE_OBJECT)

static gpointer hal_gcpufreq_object = NULL;

/**
 * hal_gcpufreq_string_to_enum:
 * @governor: The cpufreq kernel governor, e.g. "powersave"
 * Return value: The HalGCpufreqType value, e.g. LIBHAL_CPUFREQ_POWERSAVE
 **/
HalGCpufreqType
hal_gcpufreq_string_to_enum (const gchar *governor)
{
	HalGCpufreqType cpufreq_type = LIBHAL_CPUFREQ_UNKNOWN;
	if (governor == NULL) {
		cpufreq_type = LIBHAL_CPUFREQ_NOTHING;
	} else if (strcmp (governor, CODE_CPUFREQ_ONDEMAND) == 0) {
		cpufreq_type = LIBHAL_CPUFREQ_ONDEMAND;
	} else if (strcmp (governor, CODE_CPUFREQ_CONSERVATIVE) == 0) {
		cpufreq_type = LIBHAL_CPUFREQ_CONSERVATIVE;
	} else if (strcmp (governor, CODE_CPUFREQ_POWERSAVE) == 0) {
		cpufreq_type = LIBHAL_CPUFREQ_POWERSAVE;
	} else if (strcmp (governor, CODE_CPUFREQ_USERSPACE) == 0) {
		cpufreq_type = LIBHAL_CPUFREQ_USERSPACE;
	} else if (strcmp (governor, CODE_CPUFREQ_PERFORMANCE) == 0) {
		cpufreq_type = LIBHAL_CPUFREQ_PERFORMANCE;
	} else if (strcmp (governor, CODE_CPUFREQ_NOTHING) == 0) {
		cpufreq_type = LIBHAL_CPUFREQ_NOTHING;
	}
	return cpufreq_type;
}

/**
 * hal_gcpufreq_string_to_enum:
 * @cpufreq_type: The HalGCpufreqType value, e.g. LIBHAL_CPUFREQ_POWERSAVE
 * Return value: The cpufreq kernel governor, e.g. "powersave"
 **/
const gchar *
hal_gcpufreq_enum_to_string (HalGCpufreqType cpufreq_type)
{
	const char *governor;
	if (cpufreq_type == LIBHAL_CPUFREQ_ONDEMAND) {
		governor = CODE_CPUFREQ_ONDEMAND;
	} else if (cpufreq_type == LIBHAL_CPUFREQ_CONSERVATIVE) {
		governor = CODE_CPUFREQ_CONSERVATIVE;
	} else if (cpufreq_type == LIBHAL_CPUFREQ_POWERSAVE) {
		governor = CODE_CPUFREQ_POWERSAVE;
	} else if (cpufreq_type == LIBHAL_CPUFREQ_USERSPACE) {
		governor = CODE_CPUFREQ_USERSPACE;
	} else if (cpufreq_type == LIBHAL_CPUFREQ_PERFORMANCE) {
		governor = CODE_CPUFREQ_PERFORMANCE;
	} else if (cpufreq_type == LIBHAL_CPUFREQ_NOTHING) {
		governor = CODE_CPUFREQ_NOTHING;
	} else {
		governor = "unknown";
	}
	return governor;
}

/**
 * hal_gcpufreq_set_performance:
 *
 * @cpufreq: This class instance
 * @performance: The percentage perfomance figure
 * Return value: If the method succeeded
 **/
gboolean
hal_gcpufreq_set_performance (HalGCpufreq *cpufreq, guint performance)
{
	GError *error = NULL;
	gboolean ret;
	HalGCpufreqType cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (performance >= 0, FALSE);
	g_return_val_if_fail (performance <= 100, FALSE);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == LIBHAL_CPUFREQ_UNKNOWN) {
		hal_gcpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor == LIBHAL_CPUFREQ_PERFORMANCE ||
	    cpufreq->priv->current_governor == LIBHAL_CPUFREQ_POWERSAVE) {
		return FALSE;
	}

	proxy = dbus_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "SetCPUFreqPerformance", &error,
				 G_TYPE_INT, performance,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		g_warning ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_gcpufreq_set_governor:
 *
 * @cpufreq: This class instance
 * @cpufreq_type: The CPU governor type, e.g. LIBHAL_CPUFREQ_CONSERVATIVE
 * Return value: If the method succeeded
 **/
gboolean
hal_gcpufreq_set_governor (HalGCpufreq    *cpufreq,
			   HalGCpufreqType cpufreq_type)
{
	GError *error = NULL;
	gboolean ret;
	const gchar *governor;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type != LIBHAL_CPUFREQ_UNKNOWN, FALSE);

	governor = hal_gcpufreq_enum_to_string (cpufreq_type);
	g_return_val_if_fail (governor != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "SetCPUFreqGovernor", &error,
				 G_TYPE_STRING, governor,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		g_warning ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		return FALSE;
	}

	/* save the cache */
	cpufreq->priv->current_governor = cpufreq_type;
	return TRUE;
}

/**
 * hal_gcpufreq_get_governors:
 *
 * @cpufreq: This class instance
 * @cpufreq_type: Return variable, The CPU governor type as an combined bitwise type
 * Return value: If the method succeeded
 **/
gboolean
hal_gcpufreq_get_governors (HalGCpufreq     *cpufreq,
			    HalGCpufreqType *cpufreq_type)
{
	GError *error = NULL;
	gboolean ret;
	char **strlist;
	int i = 0;
	DBusGProxy *proxy;
	HalGCpufreqType types = LIBHAL_CPUFREQ_UNKNOWN;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		*cpufreq_type = LIBHAL_CPUFREQ_UNKNOWN;
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetCPUFreqAvailableGovernors", &error,
				 G_TYPE_INVALID,
				 G_TYPE_STRV, &strlist,
				 G_TYPE_INVALID);
	if (error) {
		g_warning ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		*cpufreq_type = LIBHAL_CPUFREQ_UNKNOWN;
		return FALSE;
	}

	/* treat as binary flags */
	while (strlist && strlist[i]) {
		types += hal_gcpufreq_string_to_enum (strlist[i]);
		++i;
	}

	/* when we have conservative and ondemand available, only expose
	   ondemand in the UI. They are too similar and ondemand is better. */
	if (types & LIBHAL_CPUFREQ_ONDEMAND && types & LIBHAL_CPUFREQ_CONSERVATIVE) {
		types -= LIBHAL_CPUFREQ_CONSERVATIVE;
	}

	/* We never allow the user to use userspace. */
	if (types & LIBHAL_CPUFREQ_USERSPACE) {
		types -= LIBHAL_CPUFREQ_USERSPACE;
	}

	*cpufreq_type = types;
	cpufreq->priv->available_governors = i;
	return TRUE;
}

/**
 * hal_gcpufreq_get_number_governors:
 *
 * @cpufreq: This class instance
 * @use_cache: if we should force a cache update
 * Return value: the number of available governors
 **/
guint
hal_gcpufreq_get_number_governors (HalGCpufreq *cpufreq,
				   gboolean    use_cache)
{
	HalGCpufreqType cpufreq_type;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);

	if (use_cache == FALSE || cpufreq->priv->available_governors == -1) {
		hal_gcpufreq_get_governors (cpufreq, &cpufreq_type);
	}
	return cpufreq->priv->available_governors;
}

/**
 * hal_gcpufreq_get_consider_nice:
 *
 * @cpufreq: This class instance
 * @consider_nice: Return variable, if consider niced processes
 * Return value: If the method succeeded
 **/
gboolean
hal_gcpufreq_get_consider_nice (HalGCpufreq *cpufreq,
			        gboolean      *consider_nice)
{
	GError *error = NULL;
	gboolean ret;
	HalGCpufreqType cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (consider_nice != NULL, FALSE);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == LIBHAL_CPUFREQ_UNKNOWN) {
		hal_gcpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != LIBHAL_CPUFREQ_ONDEMAND &&
	    cpufreq->priv->current_governor != LIBHAL_CPUFREQ_CONSERVATIVE) {
		*consider_nice = FALSE;
		return FALSE;
	}

	proxy = dbus_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetCPUFreqConsiderNice", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, consider_nice,
				 G_TYPE_INVALID);
	if (error) {
		g_warning ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_gcpufreq_get_performance:
 *
 * @cpufreq: This class instance
 * @performance: Return variable, the percentage performance
 * Return value: If the method succeeded
 **/
gboolean
hal_gcpufreq_get_performance (HalGCpufreq *cpufreq,
		              guint         *performance)
{
	GError *error = NULL;
	gboolean ret;
	HalGCpufreqType cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (performance != NULL, FALSE);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == LIBHAL_CPUFREQ_UNKNOWN) {
		hal_gcpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != LIBHAL_CPUFREQ_USERSPACE) {
		*performance = -1;
		return FALSE;
	}

	proxy = dbus_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetCPUFreqPerformance", &error,
				 G_TYPE_INVALID,
				 G_TYPE_INT, performance,
				 G_TYPE_INVALID);
	if (error) {
		g_warning ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_gcpufreq_get_governor:
 *
 * @cpufreq: This class instance
 * @cpufreq_type: Return variable, the governor type, e.g. LIBHAL_CPUFREQ_POWERSAVE
 * Return value: If the method succeeded
 **/
gboolean
hal_gcpufreq_get_governor (HalGCpufreq     *cpufreq,
			   HalGCpufreqType *cpufreq_type)
{
	GError *error = NULL;
	gboolean ret;
	gchar *governor;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);
	g_return_val_if_fail (cpufreq_type, FALSE);

	*cpufreq_type = LIBHAL_CPUFREQ_UNKNOWN;

	/* use the cache */
	if (cpufreq->priv->current_governor != LIBHAL_CPUFREQ_UNKNOWN) {
		return cpufreq->priv->current_governor;
	}

	proxy = dbus_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetCPUFreqGovernor", &error,
				 G_TYPE_INVALID,
				 G_TYPE_STRING, &governor,
				 G_TYPE_INVALID);
	if (error) {
		g_warning ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		return FALSE;
	}

	/* convert to enumerated type */
	if (governor != NULL) {
		*cpufreq_type = hal_gcpufreq_string_to_enum (governor);
		cpufreq->priv->current_governor = *cpufreq_type;
		g_free (governor);
	}

	return TRUE;
}

/**
 * hal_gcpufreq_set_consider_nice:
 *
 * @cpufreq: This class instance
 * @enable: True to consider nice processes
 * Return value: If the method succeeded
 **/
gboolean
hal_gcpufreq_set_consider_nice (HalGCpufreq *cpufreq,
			        gboolean    consider_nice)
{
	GError *error = NULL;
	gboolean ret;
	HalGCpufreqType cpufreq_type;
	DBusGProxy *proxy;

	g_return_val_if_fail (cpufreq != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_CPUFREQ (cpufreq), FALSE);

	/* we need to find the current governor to see if it's sane */
	if (cpufreq->priv->current_governor == LIBHAL_CPUFREQ_UNKNOWN) {
		hal_gcpufreq_get_governor (cpufreq, &cpufreq_type);
	}

	/* only applies to some governors */
	if (cpufreq->priv->current_governor != LIBHAL_CPUFREQ_ONDEMAND &&
	    cpufreq->priv->current_governor != LIBHAL_CPUFREQ_CONSERVATIVE) {
		return FALSE;
	}

	proxy = dbus_proxy_get_proxy (cpufreq->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "SetCPUFreqConsiderNice", &error,
				 G_TYPE_BOOLEAN, consider_nice,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		g_warning ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_gcpufreq_class_init:
 * @klass: This class instance
 **/
static void
hal_gcpufreq_class_init (HalGCpufreqClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = hal_gcpufreq_finalize;
	g_type_class_add_private (klass, sizeof (HalGCpufreqPrivate));
}

/**
 * hal_gcpufreq_init:
 *
 * @cpufreq: This class instance
 **/
static void
hal_gcpufreq_init (HalGCpufreq *cpufreq)
{
	cpufreq->priv = LIBHAL_CPUFREQ_GET_PRIVATE (cpufreq);

	cpufreq->priv->gproxy = dbus_proxy_new ();
	dbus_proxy_assign (cpufreq->priv->gproxy,
			  DBUS_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  HAL_ROOT_COMPUTER,
			  HAL_DBUS_INTERFACE_CPUFREQ);

	/* set defaults */
	cpufreq->priv->available_governors = -1;
	cpufreq->priv->current_governor = LIBHAL_CPUFREQ_UNKNOWN;
}

/**
 * hal_gcpufreq_finalize:
 * @object: This class instance
 **/
static void
hal_gcpufreq_finalize (GObject *object)
{
	HalGCpufreq *cpufreq;
	g_return_if_fail (object != NULL);
	g_return_if_fail (LIBHAL_IS_CPUFREQ (object));

	cpufreq = LIBHAL_CPUFREQ (object);
	cpufreq->priv = LIBHAL_CPUFREQ_GET_PRIVATE (cpufreq);

	if (cpufreq->priv->gproxy != NULL) {
		g_object_unref (cpufreq->priv->gproxy);
	}

	G_OBJECT_CLASS (hal_gcpufreq_parent_class)->finalize (object);
}

/**
 * hal_gcpufreq_has_hw:
 *
 * Self contained function that works out if we have the hardware.
 * If not, we return FALSE and the module is unloaded.
 **/
gboolean
hal_gcpufreq_has_hw (void)
{
	HalGManager *hal_manager;
	gchar **names;
	gboolean ret = TRUE;

	/* okay, as singleton */
	hal_manager = hal_gmanager_new ();
	ret = hal_gmanager_find_capability (hal_manager, "cpufreq_control", &names, NULL);

	/* nothing found */
	if (names == NULL || names[0] == NULL) {
		ret = FALSE;
	}
	hal_gmanager_free_capability (names);
	g_object_unref (hal_manager);

	return ret;
}

/**
 * hal_gcpufreq_new:
 * Return value: new HalGCpufreq instance.
 **/
HalGCpufreq *
hal_gcpufreq_new (void)
{
	if (hal_gcpufreq_object != NULL) {
		g_object_ref (hal_gcpufreq_object);
	} else {
		/* only load an instance of this module if we have the hardware */
		if (hal_gcpufreq_has_hw () == FALSE) {
			return NULL;
		}
		hal_gcpufreq_object = g_object_new (LIBHAL_TYPE_CPUFREQ, NULL);
		g_object_add_weak_pointer (hal_gcpufreq_object, &hal_gcpufreq_object);
	}
	return LIBHAL_CPUFREQ (hal_gcpufreq_object);
}

