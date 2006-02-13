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
static void     gpm_hal_monitor_finalize   (GObject            *object);

#define GPM_HAL_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_MONITOR, GpmHalMonitorPrivate))

struct GpmHalMonitorPrivate
{
	DBusGConnection *connection;
	DBusGProxy	*proxy_hal;
	DBusGProxy	*proxy_dbus;

	GHashTable      *devices;

	gboolean         enabled;
	gboolean         has_power_management;
};

enum {
	BUTTON_PRESSED,
	AC_POWER_CHANGED,
	BATTERY_PROPERTY_MODIFIED,
	BATTERY_ADDED,
	BATTERY_REMOVED,
	HAL_CONNECTED,
	HAL_DISCONNECTED,
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
	signals [HAL_CONNECTED] =
		g_signal_new ("hal-connected",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, hal_connected),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [HAL_DISCONNECTED] =
		g_signal_new ("hal-disconnected",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, hal_disconnected),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
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
				  gint           type,
				  GPtrArray     *properties,
				  GpmHalMonitor *monitor)
{
	GValueArray *array;
	const char  *udi;
	const char  *key;
	gboolean     added;
	gboolean     removed;
	guint        i;

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
 *  @param	udi			Univerisal Device Id
 *  @param	name		Name of condition
 *  @param	details	D-BUS message with parameters
 */
static void
watch_device_condition (DBusGProxy    *proxy,
			const char    *name,
			const char    *details,
			GpmHalMonitor *monitor)
{
	const char *udi = NULL;

	udi = dbus_g_proxy_get_path (proxy);

	g_assert (udi);
	g_assert (name);
	g_assert (details);

	gpm_debug ("udi=%s, name=%s, details=%s", udi, name, details);

	if (strcmp (name, "ButtonPressed") == 0) {
		char	 *type = NULL;
		gboolean  value;

		gpm_hal_device_get_string (udi, "button.type", &type);

		if (!type) {
			gpm_warning ("You must have a button type for %s!", udi);
			return;
		}

		gpm_debug ("ButtonPressed : %s", type);

		if (strcmp (type, "power") == 0) {
			value = TRUE;
		} else if (strcmp (type, "sleep") == 0) {
			value = TRUE;
		} else if (strcmp (type, "lid") == 0) {
			gpm_hal_device_get_bool (udi, "button.state.value", &value);
		} else if (strcmp (type, "virtual") == 0) {
			value = TRUE;

			if (!details) {
				gpm_warning ("Virtual buttons must have details for %s!", udi);
				return;
			}
		} else {
			gpm_warning ("Button '%s' unrecognised", type);
			g_free (type);
			return;
		}

		gpm_debug ("emitting button-pressed : %s, %s (%i)", type, details, value);
		g_signal_emit (monitor, signals [BUTTON_PRESSED], 0, type, details, value);

		g_free (type);
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
	GType       struct_array_type;

	proxy = g_hash_table_lookup (monitor->priv->devices, udi);
	if (proxy == NULL) {
		gpm_warning ("Device is not being watched: %s", udi);
		return;
	}

	struct_array_type = dbus_g_type_get_collection ("GPtrArray", G_TYPE_VALUE_ARRAY);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__INT_BOXED,
					   G_TYPE_NONE, G_TYPE_INT,
					   struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy, "PropertyModified",
				 G_TYPE_INT, struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "PropertyModified",
				     G_CALLBACK (watch_device_properties_modified), monitor, NULL);

}

static void
watch_device_disconnect_condition (GpmHalMonitor *monitor,
				   const char    *udi)
{
	DBusGProxy *proxy;

	proxy = g_hash_table_lookup (monitor->priv->devices, udi);
	if (proxy == NULL) {
		gpm_warning ("Device is not being watched: %s", udi);
		return;
	}

	dbus_g_proxy_connect_signal (proxy, "Condition",
				     G_CALLBACK (watch_device_condition), monitor, NULL);

}

static void
watch_device_disconnect_property_modified (GpmHalMonitor *monitor,
					   const char    *udi)
{
	DBusGProxy *proxy;

	proxy = g_hash_table_lookup (monitor->priv->devices, udi);
	if (proxy == NULL) {
		gpm_warning ("Device is not being watched: %s", udi);
		return;
	}

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

static gboolean
watch_device_remove (GpmHalMonitor *monitor,
		     const char    *udi)
{
	DBusGProxy *proxy;

	proxy = g_hash_table_lookup (monitor->priv->devices, udi);
	if (proxy == NULL) {
		gpm_warning ("Device is not being watched");
		return FALSE;
	}

	g_hash_table_remove (monitor->priv->devices, udi);

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
	watch_device_disconnect_condition (monitor, udi);
	watch_device_disconnect_property_modified (monitor, udi);
	watch_device_remove (monitor, udi);

	gpm_debug ("emitting battery-removed : %s", udi);
	g_signal_emit (monitor, signals [BATTERY_REMOVED], 0, udi);
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

	gpm_debug ("emitting hal-disconnected");
	g_signal_emit (monitor, signals [HAL_DISCONNECTED], 0);
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

	gpm_debug ("emitting hal-connected");
	g_signal_emit (monitor, signals [HAL_CONNECTED], 0);
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
un_coldplug_all (GpmHalMonitor *monitor)
{

	gpm_debug ("uncoldplugging (i.e removing) all devices");
	gpm_warning ("IMPLEMENT un_coldplug_all (i.e. no hashtable, "
		     "but something we can iter through; "
		     "we're loosing memory HERE and probably will segfault");

	g_hash_table_destroy (monitor->priv->devices);
	monitor->priv->devices = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							g_free,
							(GDestroyNotify)g_object_unref);
}

static void
hal_name_owner_changed (DBusGProxy *proxy,
			const char *name,
			const char *prev,
			const char *new,
			GpmHalMonitor *monitor)
{
	if (strcmp (name, HAL_DBUS_SERVICE) != 0) {
		return;
	}

	gpm_debug ("prev=%s, new=%s", prev, new);
	if (strlen (prev) != 0 && strlen (new) == 0 ) {
		if (monitor->priv->enabled) {
			/* We are already connected to HAL. A bug in DBUS can
			   sometimes trigger a double n-o-c signal */
			hal_disconnect_signals (monitor);
			un_coldplug_all (monitor);
		}
	}
	if (strlen (prev) == 0 && strlen (new) != 0 ) {
		if (! monitor->priv->enabled) {
			/* We are already connected to HAL. A bug in DBUS can
			   sometimes trigger a double n-o-c signal */
			hal_connect_signals (monitor);
			coldplug_all (monitor);
		}
	}
}

static void
hal_monitor_start (GpmHalMonitor *monitor)
{
	GError *error;

	if (monitor->priv->proxy_hal) {
		gpm_warning ("Monitor already started");
		return;
	}

	error = NULL;
	monitor->priv->proxy_dbus = dbus_g_proxy_new_for_name_owner (monitor->priv->connection,
								     DBUS_SERVICE_DBUS,
								     DBUS_PATH_DBUS,
						 		     DBUS_INTERFACE_DBUS,
								     &error);

	dbus_g_proxy_add_signal (monitor->priv->proxy_dbus, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->proxy_dbus, "NameOwnerChanged",
				     G_CALLBACK (hal_name_owner_changed),
				     monitor, NULL);

	if (monitor->priv->enabled) {
		hal_connect_signals (monitor);
		coldplug_all (monitor);
	}
}

static void
hal_monitor_stop (GpmHalMonitor *monitor)
{
	hal_disconnect_signals (monitor);
	un_coldplug_all (monitor);

	if (monitor->priv->proxy_dbus) {
		g_object_unref (monitor->priv->proxy_dbus);
		monitor->priv->proxy_dbus = NULL;
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

	if (! monitor->priv->enabled) {
		gpm_warning ("%s cannot connect to HAL!", GPM_NAME);
	}

	monitor->priv->devices = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							g_free,
							(GDestroyNotify)g_object_unref);

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

	hal_monitor_stop (monitor);

	if (monitor->priv->devices != NULL) {
		g_hash_table_destroy (monitor->priv->devices);
	}

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
