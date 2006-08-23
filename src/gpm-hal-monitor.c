/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-hal.h"
#include "gpm-hal-power.h"
#include "gpm-marshal.h"
#include "gpm-debug.h"

#include "gpm-hal-monitor.h"

static void     gpm_hal_monitor_class_init (GpmHalMonitorClass *klass);
static void     gpm_hal_monitor_init       (GpmHalMonitor      *hal_monitor);
static void     gpm_hal_monitor_finalize   (GObject	       *object);

#define GPM_HAL_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_MONITOR, GpmHalMonitorPrivate))

struct GpmHalMonitorPrivate
{
	GpmHal			*hal;
	GpmHalPower		*hal_power;
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

static guint	     signals [LAST_SIGNAL] = { 0, };

static gpointer      monitor_object = NULL;

G_DEFINE_TYPE (GpmHalMonitor, gpm_hal_monitor, G_TYPE_OBJECT)

/**
 * gpm_hal_monitor_class_init:
 * @klass: This class instance
 **/
static void
gpm_hal_monitor_class_init (GpmHalMonitorClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = gpm_hal_monitor_finalize;

	g_type_class_add_private (klass, sizeof (GpmHalMonitorPrivate));

	signals [BUTTON_PRESSED] =
		g_signal_new ("button-pressed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, button_pressed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_BOOLEAN);
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
			      gpm_marshal_VOID__STRING_STRING_BOOLEAN,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals [BATTERY_ADDED] =
		g_signal_new ("battery-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, battery_added),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [BATTERY_REMOVED] =
		g_signal_new ("battery-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, battery_removed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * monitor_change_on_ac:
 *
 * @on_ac: If we are on AC power
 */
static void
monitor_change_on_ac (GpmHalMonitor *monitor,
		      gboolean	     on_ac)
{
	gpm_debug ("emitting ac-power-changed : %i", on_ac);
	g_signal_emit (monitor, signals [AC_POWER_CHANGED], 0, on_ac);
}

/**
 * emit_button_pressed:
 *
 * @udi: The HAL UDI
 * @details: The event details, or "" for unknown or invalid
 *				NOTE: details cannot be NULL
 *
 * Use when we want to emit a ButtonPressed event and we know the udi.
 * We can get two different types of ButtonPressed condition
 *   1. The old acpi hardware buttons
 *      udi="acpi_foo", details="";
 *      button.type="power"
 *   2. The new keyboard buttons
 *      udi="foo_Kbd_Port_logicaldev_input", details="sleep"
 *      button.type=""
 */
static void
emit_button_pressed (GpmHalMonitor *monitor,
		     const char	   *udi,
		     const char	   *details)
{
	char	   *button_name = NULL;
	gboolean    value;

	g_return_if_fail (udi != NULL);
	g_return_if_fail (details != NULL);

	if (strcmp (details, "") == 0) {
		/* no details about the event, so we get more info
		   for type 1 buttons */
		gpm_hal_device_get_string (monitor->priv->hal, udi, "button.type", &button_name);
	} else {
		button_name = g_strdup (details);
	}

	/* Buttons without state should default to true. */
	value = TRUE;
	/* we need to get the button state for lid buttons */
	if (strcmp (button_name, "lid") == 0) {
		gpm_hal_device_get_bool (monitor->priv->hal, udi, "button.state.value", &value);
	}

	/* we now emit all buttons, even the ones we don't know */
	gpm_debug ("emitting button-pressed : %s (%i)", button_name, value);
	g_signal_emit (monitor, signals [BUTTON_PRESSED], 0, button_name, value);

	g_free (button_name);
}

/**
 * hal_device_property_modified_cb:
 *
 * @udi: The HAL UDI
 * @key: Property key
 * @is_added: If the key was added
 * @is_removed: If the key was removed
 *
 * Invoked when a property of a device in the Global Device List is
 * changed, and we have we have subscribed to changes for that device.
 */
static void
hal_device_property_modified_cb (GpmHal       *hal,
				 const char    *udi,
				 const char    *key,
				 gboolean       is_added,
				 gboolean       is_removed,
				 gboolean       finally,
				 GpmHalMonitor *monitor)
{
	gpm_debug ("udi=%s, key=%s, added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	/* do not process keys that have been removed */
	if (is_removed) {
		return;
	}

	if (strcmp (key, "ac_adapter.present") == 0) {
		gboolean on_ac = gpm_hal_power_is_on_ac (monitor->priv->hal_power);
		monitor_change_on_ac (monitor, on_ac);
		return;
	}

	/* only match battery* values */
	if (strncmp (key, "battery", 7) == 0) {
		gpm_debug ("emitting battery-property-modified : %s, %s", udi, key);
		g_signal_emit (monitor, signals [BATTERY_PROPERTY_MODIFIED], 0, udi, key, finally);
	}
	/* only match button* values */
	if (strncmp (key, "button", 6) == 0) {
		gpm_debug ("state of a button has changed : %s, %s", udi, key);
		emit_button_pressed (monitor, udi, "");
	}
}

/**
 * hal_device_condition_cb:
 *
 * @udi: Univerisal Device Id
 * @name: Name of condition
 * @details: D-BUS message with parameters
 *
 * Invoked when a property of a device in the Global Device List is
 * changed, and we have we have subscribed to changes for that device.
 */
static void
hal_device_condition_cb (GpmHal        *hal,
			 const char    *udi,
			 const char    *condition,
			 const char    *details,
			 GpmHalMonitor *monitor)
{
	gpm_debug ("udi=%s, condition=%s, details=%s", udi, condition, details);

	if (strcmp (condition, "ButtonPressed") == 0) {
		emit_button_pressed (monitor, udi, details);
	}
}

/**
 * watch_add_battery:
 *
 * @udi: The HAL UDI
 */
static void
watch_add_battery (GpmHalMonitor *monitor,
		   const char    *udi)
{
	gpm_hal_device_watch_propery_modified (monitor->priv->hal, udi);

	gpm_debug ("emitting battery-added : %s", udi);
	g_signal_emit (monitor, signals [BATTERY_ADDED], 0, udi);
}

/**
 * watch_add_button:
 *
 * @udi: The HAL UDI
 */
static void
watch_add_button (GpmHalMonitor *monitor,
		  const char    *udi)
{
	gpm_hal_device_watch_condition (monitor->priv->hal, udi);
	gpm_hal_device_watch_propery_modified (monitor->priv->hal, udi);
}

/**
 * watch_add_ac_adapter:
 *
 * @udi: The HAL UDI
 */
static void
watch_add_ac_adapter (GpmHalMonitor *monitor,
		      const char    *udi)
{
	gpm_hal_device_watch_propery_modified (monitor->priv->hal, udi);
}

/**
 * hal_device_removed_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @monitor: This monitor instance
 */
static void
hal_device_removed_cb (GpmHal        *hal,
		       const char    *udi,
		       GpmHalMonitor *monitor)
{
	gpm_debug ("udi=%s", udi);

	/* FIXME: these may not all be batteries */
	gpm_hal_device_remove_propery_modified (monitor->priv->hal, udi);
}

/**
 * hal_new_capability_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @capability: the capability, e.g. "battery"
 * @monitor: This monitor instance
 */
static void
hal_new_capability_cb (GpmHal        *hal,
		       const char    *udi,
		       const char    *capability,
		       GpmHalMonitor *monitor)
{
	gpm_debug ("udi=%s, capability=%s", udi, capability);

	if (strcmp (capability, "battery") == 0) {
		gpm_hal_device_watch_propery_modified (monitor->priv->hal, udi);
	}
}

/**
 * coldplug_acadapter:
 */
static gboolean
coldplug_acadapter (GpmHalMonitor *monitor)
{
	int    i;
	char **device_names = NULL;

	/* devices of type ac_adapter */
	gpm_hal_device_find_capability (monitor->priv->hal, "ac_adapter", &device_names);
	if (! device_names) {
		gpm_debug ("Couldn't obtain list of ac_adapters");
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		watch_add_ac_adapter (monitor, device_names [i]);
	}

	gpm_hal_free_capability (monitor->priv->hal, device_names);

	return TRUE;
}

/**
 * coldplug_buttons:
 */
static gboolean
coldplug_buttons (GpmHalMonitor *monitor)
{
	int    i;
	char **device_names = NULL;

	/* devices of type button */
	gpm_hal_device_find_capability (monitor->priv->hal, "button", &device_names);
	if (! device_names) {
		gpm_debug ("Couldn't obtain list of buttons");
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		watch_add_button (monitor, device_names [i]);
	}

	gpm_hal_free_capability (monitor->priv->hal, device_names);

	return TRUE;
}

/**
 * coldplug_batteries:
 *
 *  @return			If any devices of capability battery were found.
 *
 * Coldplugs devices of type battery & ups at startup
 */
static gboolean
coldplug_batteries (GpmHalMonitor *monitor)
{
	int    i;
	char **device_names = NULL;

	/* devices of type battery */
	gpm_hal_device_find_capability (monitor->priv->hal, "battery", &device_names);
	if (! device_names) {
		gpm_debug ("Couldn't obtain list of batteries");
		return FALSE;
	}

	for (i = 0; device_names [i]; i++) {
		watch_add_battery (monitor, device_names [i]);
	}

	gpm_hal_free_capability (monitor->priv->hal, device_names);

	return TRUE;
}

/**
 * coldplug_all:
 */
static void
coldplug_all (GpmHalMonitor *monitor)
{
	/* sets up these devices and adds watches */
	gpm_debug ("coldplugging all devices");
	coldplug_batteries (monitor);
	coldplug_acadapter (monitor);
	coldplug_buttons (monitor);
}

/**
 * gpm_hal_monitor_coldplug:
 *
 *
 * Cold-plugs (re-adds) all the basic devices.
 */
void
gpm_hal_monitor_coldplug (GpmHalMonitor *monitor)
{
	coldplug_all (monitor);
}

/**
 * gpm_hal_monitor_coldplug:
 */
static gboolean
start_idle (GpmHalMonitor *monitor)
{
	coldplug_all (monitor);
	return FALSE;
}

/**
 * gpm_hal_monitor_coldplug:
 */
static void
gpm_hal_monitor_init (GpmHalMonitor *monitor)
{
	monitor->priv = GPM_HAL_MONITOR_GET_PRIVATE (monitor);

	monitor->priv->hal = gpm_hal_new ();
	monitor->priv->hal_power = gpm_hal_power_new ();

	g_signal_connect (monitor->priv->hal, "device-removed",
			  G_CALLBACK (hal_device_removed_cb), monitor);
	g_signal_connect (monitor->priv->hal, "new-capability",
			  G_CALLBACK (hal_new_capability_cb), monitor);
	g_signal_connect (monitor->priv->hal, "property-modified",
			  G_CALLBACK (hal_device_property_modified_cb), monitor);
	g_signal_connect (monitor->priv->hal, "device-condition",
			  G_CALLBACK (hal_device_condition_cb), monitor);

	g_idle_add ((GSourceFunc)start_idle, monitor);
}

/**
 * gpm_hal_monitor_coldplug:
 *
 * @object: This monitor instance
 */
static void
gpm_hal_monitor_finalize (GObject *object)
{
	GpmHalMonitor *monitor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL_MONITOR (object));

	monitor = GPM_HAL_MONITOR (object);

	g_return_if_fail (monitor->priv != NULL);

	g_object_unref (monitor->priv->hal);
	g_object_unref (monitor->priv->hal_power);

	G_OBJECT_CLASS (gpm_hal_monitor_parent_class)->finalize (object);
}

/**
 * gpm_hal_monitor_new:
 * Return value: new GpmHalMonitor instance.
 **/
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
