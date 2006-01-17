/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
 *
 * Authors:
 *          William Jon McCann <mccann@jhu.edu>
 *          Richard Hughes <richard@hughsie.com>
 *
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

#include <glib/gi18n.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-hal.h"
#include "gpm-hal-callback.h"
#include "gpm-marshal.h"

#include "gpm-hal-monitor.h"

static void     gpm_hal_monitor_class_init (GpmHalMonitorClass *klass);
static void     gpm_hal_monitor_init       (GpmHalMonitor      *hal_monitor);
static void     gpm_hal_monitor_finalize   (GObject            *object);

#define GPM_HAL_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_MONITOR, GpmHalMonitorPrivate))

struct GpmHalMonitorPrivate
{
	gboolean enabled;
	gboolean has_power_management;
};

enum {
	BUTTON_PRESSED,
	AC_POWER_CHANGED,
	BATTERY_PROPERTY_MODIFIED,
	BATTERY_ADDED,
	BATTERY_REMOVED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_MODE
};

static GObjectClass *parent_class = NULL;
static guint	     signals [LAST_SIGNAL] = { 0, };

static gpointer      monitor_object = NULL;

G_DEFINE_TYPE (GpmHalMonitor, gpm_hal_monitor, G_TYPE_OBJECT)

static void
gpm_hal_monitor_set_property (GObject		 *object,
			      guint		  prop_id,
			      const GValue	 *value,
			      GParamSpec	 *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_hal_monitor_get_property (GObject		 *object,
			      guint		  prop_id,
			      GValue		 *value,
			      GParamSpec	 *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_hal_monitor_class_init (GpmHalMonitorClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize	   = gpm_hal_monitor_finalize;
	object_class->get_property = gpm_hal_monitor_get_property;
	object_class->set_property = gpm_hal_monitor_set_property;

	g_type_class_add_private (klass, sizeof (GpmHalMonitorPrivate));

	signals [BUTTON_PRESSED] =
		g_signal_new ("button-pressed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, button_pressed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals [AC_POWER_CHANGED] =
		g_signal_new ("ac-power-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, ac_power_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_BOOLEAN);
	signals [BATTERY_PROPERTY_MODIFIED] =
		g_signal_new ("battery-property-modified",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, battery_property_modified),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	signals [BATTERY_ADDED] =
		g_signal_new ("battery-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, battery_added),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	signals [BATTERY_REMOVED] =
		g_signal_new ("battery-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, battery_removed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
}


/** When we have a device removed
 *
 *  @param	udi		The HAL UDI
 */
static void
hal_device_removed (const gchar *udi,
		    gpointer	 user_data)
{
	GpmHalMonitor *monitor;

	monitor = GPM_HAL_MONITOR (user_data);

	g_debug ("hal_device_removed: udi=%s", udi);

	/* these may not all be batteries but oh well */
	g_signal_emit (monitor, signals [BATTERY_REMOVED], 0, udi);

	/* remove watch */
	gpm_hal_watch_remove_device_property_modified (udi);
}

/** When we have a new device hot-plugged
 *
 *  @param	udi		UDI
 *  @param	capability	Name of capability
 */
static void
hal_device_new_capability (const char *udi,
			   const char *capability,
			   gpointer    user_data)
{
	GpmHalMonitor *monitor;

	monitor = GPM_HAL_MONITOR (user_data);

	g_debug ("hal_device_new_capability: udi=%s, capability=%s",
		 udi, capability);

	if (strcmp (capability, "battery") == 0) {
		g_signal_emit (monitor, signals [BATTERY_ADDED], 0, udi);
	}
}

static void
monitor_change_on_ac (GpmHalMonitor *monitor,
		      gboolean	     on_ac)
{
	g_signal_emit (monitor, signals [AC_POWER_CHANGED], 0, on_ac);
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi		The HAL UDI
 *  @param	key		Property key
 *  @param	is_added	If the key was added
 *  @param	is_removed	If the key was removed
 */
static void
hal_device_property_modified (const gchar *udi,
			      const gchar *key,
			      gboolean	   is_added,
			      gboolean	   is_removed,
			      gpointer	   user_data)
{
	GpmHalMonitor *monitor;

#if 0
	g_debug ("hal_device_property_modified: udi=%s, key=%s, added=%i, removed=%i",
		 udi, key, is_added, is_removed);
#endif

	/* only process modified entries, not added or removed keys */
	if (is_removed || is_added)
		return;

	monitor = GPM_HAL_MONITOR (user_data);

	if (strcmp (key, "ac_adapter.present") == 0) {
		gboolean on_ac = gpm_hal_is_on_ac ();

		monitor_change_on_ac (monitor, on_ac);

		return;
	}

	/* no point continuing any further if we are never going to match ...*/
	if (strncmp (key, "battery", 7) != 0)
		return;

	g_signal_emit (monitor, signals [BATTERY_PROPERTY_MODIFIED], 0, udi, key);
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi			Univerisal Device Id
 *  @param	name		Name of condition
 *  @param	details	D-BUS message with parameters
 */
static void
hal_device_condition (const char *udi,
		      const char *name,
		      const char *details,
		      gpointer	  user_data)
{
	GpmHalMonitor *monitor;

	monitor = GPM_HAL_MONITOR (user_data);

	g_assert (udi);
	g_assert (name);
	g_assert (details);

	g_debug ("hal_device_condition: udi=%s, name=%s, details=%s",
		 udi, name, details);

	if (strcmp (name, "ButtonPressed") == 0) {
		char	 *type = NULL;
		gboolean  value;

		gpm_hal_device_get_string (udi, "button.type", &type);

		if (!type) {
			g_warning ("You must have a button type for %s!", udi);
			return;
		}

		g_debug ("ButtonPressed : %s", type);

		if (strcmp (type, "power") == 0) {
			value = TRUE;
		} else if (strcmp (type, "sleep") == 0) {
			value = TRUE;
		} else if (strcmp (type, "lid") == 0) {
			gpm_hal_device_get_bool (udi, "button.state.value", &value);
		} else if (strcmp (type, "virtual") == 0) {
			value = TRUE;

			if (!details) {
				g_warning ("Virtual buttons must have details for %s!", udi);
				return;
			}
		} else {
			g_warning ("Button '%s' unrecognised", type);
			g_free (type);
			return;
		}

		g_signal_emit (monitor, signals [BUTTON_PRESSED], 0, type, details, value);

		g_free (type);
	}
}

static gboolean
gpm_coldplug_acadapter (GpmHalMonitor *monitor)
{
	gint    i;
	gchar **device_names = NULL;

	/* devices of type ac_adapter */
	gpm_hal_find_device_capability ("ac_adapter", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of ac_adapters");
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		/* assume only one */
		gpm_hal_watch_add_device_property_modified (device_names[i]);

	}

	gpm_hal_free_capability (device_names);

	return TRUE;
}

static gboolean
gpm_coldplug_buttons (GpmHalMonitor *monitor)
{
	gint    i;
	gchar **device_names = NULL;

	/* devices of type button */
	gpm_hal_find_device_capability ("button", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of buttons");
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		/*
		 * We register this here, as buttons are not present
		 * in object data, and do not need to be added manually.
		*/
		gpm_hal_watch_add_device_condition (device_names[i]);
	}

	gpm_hal_free_capability (device_names);

	return TRUE;
}

/** Coldplugs devices of type battery & ups at startup
 *
 *  @return			If any devices of capability battery were found.
 */
static gboolean
gpm_coldplug_batteries (GpmHalMonitor *monitor)
{
	gint    i;
	gchar **device_names = NULL;

	/* devices of type battery */
	gpm_hal_find_device_capability ("battery", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of batteries");
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		g_debug ("signalling battery-added: %s", device_names[i]);

		g_signal_emit (monitor, signals [BATTERY_ADDED], 0, device_names[i]);

		/* register this with HAL so we get PropertyModified events */
		gpm_hal_watch_add_device_property_modified (device_names[i]);
	}

	gpm_hal_free_capability (device_names);

	return TRUE;
}

static void
hal_monitor_start (GpmHalMonitor *monitor)
{
	/* assign the callback functions */
	gpm_hal_callback_init (monitor);
	gpm_hal_method_device_removed (hal_device_removed);
	gpm_hal_method_device_new_capability (hal_device_new_capability);
	gpm_hal_method_device_property_modified (hal_device_property_modified);
	gpm_hal_method_device_condition (hal_device_condition);

	/* sets up these devices and adds watches */
	gpm_coldplug_batteries (monitor);
	gpm_coldplug_acadapter (monitor);
	gpm_coldplug_buttons (monitor);
}

static void
hal_monitor_stop (GpmHalMonitor *monitor)
{
	gpm_hal_callback_shutdown ();
}

gboolean
gpm_hal_monitor_get_on_ac (GpmHalMonitor *monitor)
{
	gboolean on_ac;

	g_return_val_if_fail (GPM_IS_HAL_MONITOR (monitor), FALSE);

	on_ac = gpm_hal_is_on_ac ();

	return on_ac;
}

static gboolean
start_idle (GpmHalMonitor *monitor)
{
	hal_monitor_start (monitor);
	return FALSE;
}

static void
gpm_hal_monitor_init (GpmHalMonitor *monitor)
{
	monitor->priv = GPM_HAL_MONITOR_GET_PRIVATE (monitor);

	monitor->priv->enabled = gpm_hal_is_running ();

	if (! monitor->priv->enabled) {
		g_warning ("%s cannot connect to HAL!", NICENAME);
	}

	monitor->priv->has_power_management = gpm_hal_has_power_management ();

	if (! monitor->priv->has_power_management) {
		g_warning ("HAL does not have modern PowerManagement capability");
	}

	if (monitor->priv->enabled
	    && monitor->priv->has_power_management) {
		g_idle_add ((GSourceFunc)start_idle, monitor);
	}
}

static void
gpm_hal_monitor_finalize (GObject *object)
{
	GpmHalMonitor *monitor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL_MONITOR (object));

	monitor = GPM_HAL_MONITOR (object);

	g_return_if_fail (monitor->priv != NULL);

	hal_monitor_stop (monitor);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmHalMonitor *
gpm_hal_monitor_new (void)
{
        if (monitor_object) {
                g_object_ref (monitor_object);
        } else {
                monitor_object = g_object_new (GPM_TYPE_HAL_MONITOR, NULL);
                g_object_add_weak_pointer (monitor_object,
                                           (gpointer *) &monitor_object);
        }

	return GPM_HAL_MONITOR (monitor_object);
}
