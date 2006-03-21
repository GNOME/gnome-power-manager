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

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-hal.h"
#include "gpm-marshal.h"
#include "gpm-debug.h"

#include "gpm-hal-monitor.h"

static void     gpm_hal_monitor_class_init (GpmHalMonitorClass *klass);
static void     gpm_hal_monitor_init       (GpmHalMonitor      *hal_monitor);
static void     gpm_hal_monitor_finalize   (GObject	    *object);

#define GPM_HAL_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_MONITOR, GpmHalMonitorPrivate))

struct GpmHalMonitorPrivate
{
	DBusGConnection *connection;
	DBusGProxy	*proxy_hal;

	GHashTable      *devices;

	gboolean	 enabled;
	gboolean	 has_power_management;
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
gpm_hal_monitor_class_init (GpmHalMonitorClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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
			      gpm_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
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

static void
monitor_change_on_ac (GpmHalMonitor *monitor,
		      gboolean	     on_ac)
{
	gpm_debug ("emitting ac-power-changed : %i", on_ac);
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
watch_device_property_modified (DBusGProxy    *proxy,
				const char    *udi,
				const char    *key,
				gboolean       is_added,
				gboolean       is_removed,
				GpmHalMonitor *monitor)
{
	gpm_debug ("udi=%s, key=%s, added=%i, removed=%i", udi, key, is_added, is_removed);

	/* only process modified entries, not added or removed keys */
	if (is_removed || is_added) {
		return;
	}

	if (strcmp (key, "ac_adapter.present") == 0) {
		gboolean on_ac = gpm_hal_is_on_ac ();

		monitor_change_on_ac (monitor, on_ac);

		return;
	}

	/* no point continuing any further if we are never going to match ...*/
	if (strncmp (key, "battery", 7) != 0)
		return;

	gpm_debug ("emitting battery-property-modified : %s, %s", udi, key);
	g_signal_emit (monitor, signals [BATTERY_PROPERTY_MODIFIED], 0, udi, key);
}

static void
watch_device_properties_modified (DBusGProxy    *proxy,
				  gint	   type,
				  GPtrArray     *properties,
				  GpmHalMonitor *monitor)
{
	GValueArray *array;
	const char  *udi;
	const char  *key;
	gboolean     added;
	gboolean     removed;
	guint	i;

	udi = dbus_g_proxy_get_path (proxy);
	gpm_debug ("property modified '%s'", udi);

	array = NULL;

	for (i = 0; i < properties->len; i++) {
		array = g_ptr_array_index (properties, i);
		if (array->n_values != 3) {
			gpm_warning ("array->n_values invalid (!3)");
			return;
		}

		key = g_value_get_string (g_value_array_get_nth (array, 0));
		removed = g_value_get_boolean (g_value_array_get_nth (array, 1));
		added = g_value_get_boolean (g_value_array_get_nth (array, 2));

		watch_device_property_modified (proxy, udi, key, removed, added, monitor);
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi		Univerisal Device Id
 *  @param	name		Name of condition
 *  @param	details		D-BUS message with parameters
 */
static void
watch_device_condition (DBusGProxy    *proxy,
			const char    *condition_name,
			const char    *details,
			GpmHalMonitor *monitor)
{
	const char *udi = NULL;
	char	   *button_name = NULL;
	gboolean    value;

	udi = dbus_g_proxy_get_path (proxy);

	gpm_debug ("udi=%s, condition_name=%s", udi, condition_name);

	if (strcmp (condition_name, "ButtonPressed") == 0) {
		/* We can get two different types of ButtonPressed condition
		   1. The old acpi hardware buttons
		      udi="acpi_foo", details="";
		      button.type="power"
		   2. The new keyboard buttons
		      udi="foo_Kbd_Port_logicaldev_input", details="sleep"
		      button.type=""
		 */
		if (strcmp (details, "") == 0) {
			/* no details about the event, so we get more info
			   for type 1 buttons */
			gpm_hal_device_get_string (udi, "button.type", &button_name);
		} else {
			button_name = g_strdup (details);
		}

		/* buttons without state should default to true, although this shouldn't matter */
		value = TRUE;
		/* we need to get the button state for lid buttons */
		if (strcmp (button_name, "lid") == 0) {
			gpm_hal_device_get_bool (udi, "button.state.value", &value);
		}

		/* we now emit all buttons, even the ones we don't know */
		gpm_debug ("emitting button-pressed : %s (%i)", button_name, value);
		g_signal_emit (monitor, signals [BUTTON_PRESSED], 0, button_name, value);

		g_free (button_name);
	}
}

static void
watch_device_connect_condition (GpmHalMonitor *monitor,
				const char    *udi)
{
	DBusGProxy *proxy;

	proxy = g_hash_table_lookup (monitor->priv->devices, udi);
	if (proxy == NULL) {
		gpm_warning ("Device is not being watched: %s", udi);
		return;
	}

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy, "Condition",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "Condition",
				     G_CALLBACK (watch_device_condition), monitor, NULL);
}

static void
watch_device_connect_property_modified (GpmHalMonitor *monitor,
					const char    *udi)
{
	DBusGProxy *proxy;
	GType       struct_array_type, struct_type;

	proxy = g_hash_table_lookup (monitor->priv->devices, udi);
	if (proxy == NULL) {
		gpm_warning ("Device is not being watched: %s", udi);
		return;
	}
#if (DBUS_VERSION_MAJOR == 0) && (DBUS_VERSION_MINOR < 61)
	struct_type = G_TYPE_VALUE_ARRAY;
#else
	struct_type = dbus_g_type_get_struct ("GValueArray", 
						G_TYPE_STRING, 
						G_TYPE_BOOLEAN, 
						G_TYPE_BOOLEAN, 
						G_TYPE_INVALID);
#endif
	struct_array_type = dbus_g_type_get_collection ("GPtrArray", struct_type);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__INT_BOXED,
					   G_TYPE_NONE, G_TYPE_INT,
					   struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy, "PropertyModified",
				 G_TYPE_INT, struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "PropertyModified",
				     G_CALLBACK (watch_device_properties_modified), monitor, NULL);

}

static gboolean
watch_device_add (GpmHalMonitor *monitor,
		  const char    *udi)
{
	DBusGProxy *proxy;
	GError     *error;

	gpm_debug ("Adding new device to watch: %s", udi);

	proxy = g_hash_table_lookup (monitor->priv->devices, udi);
	if (proxy != NULL) {
		gpm_warning ("Device is already being watched: %s", udi);
		return FALSE;
	}

	gpm_debug ("Creating proxy for: %s", udi);
	error = NULL;
	proxy = dbus_g_proxy_new_for_name_owner (monitor->priv->connection,
						 HAL_DBUS_SERVICE,
						 udi,
						 HAL_DBUS_INTERFACE_DEVICE,
						 &error);
	if (proxy == NULL) {
		gpm_warning ("Could not create proxy for UDI: %s: %s", udi, error->message);
		g_error_free (error);
		return FALSE;
	}

	g_hash_table_insert (monitor->priv->devices,
			     g_strdup (udi),
			     proxy);

	return TRUE;
}

static void
watch_add_battery (GpmHalMonitor *monitor,
		   const char    *udi)
{
	watch_device_add (monitor, udi);
	watch_device_connect_property_modified (monitor, udi);

	gpm_debug ("emitting battery-added : %s", udi);
	g_signal_emit (monitor, signals [BATTERY_ADDED], 0, udi);
}

static void
watch_remove_battery (GpmHalMonitor *monitor,
		      const char    *udi)
{
	gpointer key, value;
	char *udi_key;
	DBusGProxy *proxy = NULL;

	if (!g_hash_table_lookup_extended (monitor->priv->devices, udi, &key, &value)) {
		gpm_warning ("Device is not being watched: %s", udi);
		return;
	}

	udi_key = key;
	proxy = value;

	dbus_g_proxy_disconnect_signal (proxy, "Condition",
					G_CALLBACK (watch_device_condition), monitor);

	dbus_g_proxy_disconnect_signal (proxy, "PropertyModified",
				     G_CALLBACK (watch_device_properties_modified), monitor);

	g_hash_table_remove (monitor->priv->devices, udi);

	gpm_debug ("emitting battery-removed : %s", udi);
	g_signal_emit (monitor, signals [BATTERY_REMOVED], 0, udi_key);

	g_object_unref (proxy);
	g_free (udi_key);
}

static void
watch_add_button (GpmHalMonitor *monitor,
		  const char    *udi)
{
	watch_device_add (monitor, udi);
	watch_device_connect_condition (monitor, udi);
}

static void
watch_add_ac_adapter (GpmHalMonitor *monitor,
		      const char    *udi)
{
	watch_device_add (monitor, udi);
	watch_device_connect_property_modified (monitor, udi);
}

static void
hal_device_removed (DBusGProxy    *proxy,
		    const char    *udi,
		    GpmHalMonitor *monitor)
{
	gpm_debug ("udi=%s", udi);

	/* FIXME: these may not all be batteries */
	watch_remove_battery (monitor, udi);
}

static void
hal_new_capability (DBusGProxy    *proxy,
		    const char    *udi,
		    const char    *capability,
		    GpmHalMonitor *monitor)
{
	gpm_debug ("udi=%s, capability=%s", udi, capability);

	if (strcmp (capability, "battery") == 0) {
		watch_add_battery (monitor, udi);
	}
}

static void
hal_disconnect_signals (GpmHalMonitor *monitor)
{
	gpm_debug ("Disconnecting signals from HAL");

	if (monitor->priv->proxy_hal) {
		/* it looks like we leak memory, but I'm pretty sure dbus
		   cleans up the proxy for us */
		dbus_g_proxy_disconnect_signal (monitor->priv->proxy_hal, "DeviceRemoved",
						G_CALLBACK (hal_device_removed), monitor);
		dbus_g_proxy_disconnect_signal (monitor->priv->proxy_hal, "NewCapability",
						G_CALLBACK (hal_new_capability), monitor);
		g_object_unref (monitor->priv->proxy_hal);
		monitor->priv->proxy_hal = NULL;
	}
	/* we are now not using HAL */
	monitor->priv->enabled = FALSE;
}

static void
hal_connect_signals (GpmHalMonitor *monitor)
{
	GError *error;

	gpm_debug ("Connecting signals to HAL");
	monitor->priv->proxy_hal = dbus_g_proxy_new_for_name_owner (monitor->priv->connection,
								    HAL_DBUS_SERVICE,
								    HAL_DBUS_PATH_MANAGER,
								    HAL_DBUS_INTERFACE_MANAGER,
								    &error);

	dbus_g_proxy_add_signal (monitor->priv->proxy_hal, "DeviceRemoved",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->proxy_hal, "DeviceRemoved",
				     G_CALLBACK (hal_device_removed), monitor, NULL);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (monitor->priv->proxy_hal, "NewCapability",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->proxy_hal, "NewCapability",
				     G_CALLBACK (hal_new_capability), monitor, NULL);
	/* we are now using HAL */
	monitor->priv->enabled = TRUE;
}

static gboolean
coldplug_acadapter (GpmHalMonitor *monitor)
{
	int    i;
	char **device_names = NULL;

	/* devices of type ac_adapter */
	gpm_hal_find_device_capability ("ac_adapter", &device_names);
	if (! device_names) {
		gpm_debug ("Couldn't obtain list of ac_adapters");
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		watch_add_ac_adapter (monitor, device_names [i]);
	}

	gpm_hal_free_capability (device_names);

	return TRUE;
}

static gboolean
coldplug_buttons (GpmHalMonitor *monitor)
{
	int    i;
	char **device_names = NULL;

	/* devices of type button */
	gpm_hal_find_device_capability ("button", &device_names);
	if (! device_names) {
		gpm_debug ("Couldn't obtain list of buttons");
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		watch_add_button (monitor, device_names [i]);
	}

	gpm_hal_free_capability (device_names);

	return TRUE;
}

/** Coldplugs devices of type battery & ups at startup
 *
 *  @return			If any devices of capability battery were found.
 */
static gboolean
coldplug_batteries (GpmHalMonitor *monitor)
{
	int    i;
	char **device_names = NULL;

	/* devices of type battery */
	gpm_hal_find_device_capability ("battery", &device_names);
	if (! device_names) {
		gpm_debug ("Couldn't obtain list of batteries");
		return FALSE;
	}

	for (i = 0; device_names [i]; i++) {
		watch_add_battery (monitor, device_names [i]);
	}

	gpm_hal_free_capability (device_names);

	return TRUE;
}

static void
coldplug_all (GpmHalMonitor *monitor)
{
	/* sets up these devices and adds watches */
	gpm_debug ("coldplugging all devices");
	coldplug_batteries (monitor);
	coldplug_acadapter (monitor);
	coldplug_buttons (monitor);
}


static void
remove_batteries_in_hash (const char *udi, gpointer value, GList **udis)
{
	*udis = g_list_prepend (*udis, (char *) udi);
}

static void
gpm_hash_free_devices_cache (GpmHalMonitor *monitor)
{
	GList *udis = NULL, *l;
	
	if (! monitor->priv->devices) {
		return;
	}

	gpm_debug ("freeing cache");
	/* Build a list of udis so we can cleanly remove the items
	 * with signals */
	g_hash_table_foreach (monitor->priv->devices,
			      (GHFunc) remove_batteries_in_hash,
			      &udis);

	for (l = udis; l; l = l->next)
		watch_remove_battery (monitor, l->data);
	g_list_free (udis);
	
	g_hash_table_destroy (monitor->priv->devices);
	monitor->priv->devices = NULL;
}

static void
gpm_hash_new_devices_cache (GpmHalMonitor *monitor)
{
	if (monitor->priv->devices) {
		return;
	}
	gpm_debug ("creating cache");
	monitor->priv->devices = g_hash_table_new (g_str_hash, g_str_equal);
}

void
hal_start_monitor (GpmHalMonitor *monitor)
{
	if (monitor->priv->enabled) {
		gpm_debug ("Already connected");
		return;
	}
	hal_connect_signals (monitor);
	coldplug_all (monitor);
}

void
hal_stop_monitor (GpmHalMonitor *monitor)
{
	if (! monitor->priv->enabled) {
		gpm_debug ("Already disconnected");
		return;
	}

	hal_disconnect_signals (monitor);

	/* we have to rebuild the cache */
	gpm_hash_free_devices_cache (monitor);
	gpm_hash_new_devices_cache (monitor);
}

static void
hal_monitor_start (GpmHalMonitor *monitor)
{
	if (monitor->priv->proxy_hal) {
		gpm_warning ("Monitor already started");
		return;
	}

	if (monitor->priv->enabled) {
		hal_connect_signals (monitor);
		coldplug_all (monitor);
	}
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
	GError *error;

	monitor->priv = GPM_HAL_MONITOR_GET_PRIVATE (monitor);

	monitor->priv->enabled = gpm_hal_is_running ();
	monitor->priv->proxy_hal = NULL;
	monitor->priv->devices = NULL;

	if (! monitor->priv->enabled) {
		gpm_warning ("%s cannot connect to HAL!", GPM_NAME);
	}

	gpm_hash_new_devices_cache (monitor);

	error = NULL;
	monitor->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	/* FIXME: check error */

	monitor->priv->has_power_management = gpm_hal_has_power_management ();

	if (! monitor->priv->has_power_management) {
		gpm_warning ("HAL does not have modern PowerManagement capability");
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

	hal_disconnect_signals (monitor);

	gpm_hash_free_devices_cache (monitor);

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
