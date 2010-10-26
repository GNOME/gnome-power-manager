/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gpm-phone.h"
#include "egg-debug.h"
#include "gpm-marshal.h"

static void     gpm_phone_finalize   (GObject	    *object);

#define GPM_PHONE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PHONE, GpmPhonePrivate))

struct GpmPhonePrivate
{
	GDBusProxy		*proxy;
	GDBusConnection		*connection;
	guint			 watch_id;
	gboolean		 present;
	guint			 percentage;
	gboolean		 onac;	 
};

enum {
	DEVICE_ADDED,
	DEVICE_REMOVED,
	DEVICE_REFRESH,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_phone_object = NULL;

G_DEFINE_TYPE (GpmPhone, gpm_phone, G_TYPE_OBJECT)

/**
 * gpm_phone_coldplug:
 * Return value: Success value, or zero for failure
 **/
gboolean
gpm_phone_coldplug (GpmPhone *phone)
{
	GError  *error = NULL;
	GVariant *reply;
	gboolean ret;

	g_return_val_if_fail (phone != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PHONE (phone), FALSE);

	if (phone->priv->proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	reply = g_dbus_proxy_call_sync (phone->priv->proxy, "Coldplug",
			NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
	if (error != NULL) {
		egg_warning ("DEBUG: ERROR: %s", error->message);
		g_error_free (error);
	}

	if (reply != NULL) {
		ret = TRUE;
		g_variant_unref (reply);
	} else
		ret = FALSE;

	return ret;
}

/**
 * gpm_phone_get_present:
 * Return value: if present
 **/
gboolean
gpm_phone_get_present (GpmPhone	*phone, guint idx)
{
	g_return_val_if_fail (phone != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PHONE (phone), FALSE);
	return phone->priv->present;
}

/**
 * gpm_phone_get_percentage:
 * Return value: if present
 **/
guint
gpm_phone_get_percentage (GpmPhone *phone, guint idx)
{
	g_return_val_if_fail (phone != NULL, 0);
	g_return_val_if_fail (GPM_IS_PHONE (phone), 0);
	return phone->priv->percentage;
}

/**
 * gpm_phone_get_on_ac:
 * Return value: if present
 **/
gboolean
gpm_phone_get_on_ac (GpmPhone *phone, guint idx)
{
	g_return_val_if_fail (phone != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PHONE (phone), FALSE);
	return phone->priv->onac;
}

/**
 * gpm_phone_get_num_batteries:
 * Return value: number of phone batteries monitored
 **/
guint
gpm_phone_get_num_batteries (GpmPhone *phone)
{
	g_return_val_if_fail (phone != NULL, 0);
	g_return_val_if_fail (GPM_IS_PHONE (phone), 0);
	if (phone->priv->present) {
		return 1;
	}
	return 0;
}

/** Invoked when we get the BatteryStateChanged
 */
static void
gpm_phone_battery_state_changed (GDBusProxy *proxy, guint idx, guint percentage, gboolean on_ac, GpmPhone *phone)
{
	g_return_if_fail (GPM_IS_PHONE (phone));

	egg_debug ("got BatteryStateChanged %i = %i (%i)", idx, percentage, on_ac);
	phone->priv->percentage = percentage;
	phone->priv->onac = on_ac;
	phone->priv->present = TRUE;
	egg_debug ("emitting device-refresh : (%i)", idx);
	g_signal_emit (phone, signals [DEVICE_REFRESH], 0, idx);
}

/** Invoked when we get NumberBatteriesChanged
 */
static void
gpm_phone_num_batteries_changed (GDBusProxy *proxy, guint number, GpmPhone *phone)
{
	g_return_if_fail (GPM_IS_PHONE (phone));

	egg_debug ("got NumberBatteriesChanged %i", number);
	if (number > 1) {
		egg_warning ("number not 0 or 1, not valid!");
		return;
	}

	/* are we removed? */
	if (number == 0) {
		phone->priv->present = FALSE;
		phone->priv->percentage = 0;
		phone->priv->onac = FALSE;
		egg_debug ("emitting device-removed : (%i)", 0);
		g_signal_emit (phone, signals [DEVICE_REMOVED], 0, 0);
		return;
	}

	if (phone->priv->present) {
		egg_warning ("duplicate NumberBatteriesChanged with no change");
		return;
	}

	/* reset to defaults until we get BatteryStateChanged */
	phone->priv->present = TRUE;
	phone->priv->percentage = 0;
	phone->priv->onac = FALSE;
	egg_debug ("emitting device-added : (%i)", 0);
	g_signal_emit (phone, signals [DEVICE_ADDED], 0, 0);
}

/**
 * gpm_phone_generic_signal_cb:
 */
static void
gpm_phone_generic_signal_cb (GDBusProxy *proxy,
			     gchar *sender_name, gchar *signal_name,
			     GVariant *parameters, gpointer user_data) {

	GpmPhone *self = GPM_PHONE (user_data);

	if (!g_strcmp0 (signal_name, "BatteryStateChanged")) {
		guint idx, percentage;
		gboolean on_ac;

		g_variant_get (parameters, "(uub)", &idx, &percentage, &on_ac);
		gpm_phone_battery_state_changed (proxy, idx, percentage, on_ac, self);
		return;
	}

	if (!g_strcmp0 (signal_name, "NumberBatteriesChanged")) {
		guint number;

		g_variant_get (parameters, "(u)", &number);
		gpm_phone_num_batteries_changed (proxy, number, self);
		return;
	}

	/* not a signal we're interested in */
}

/**
 * gpm_phone_class_init:
 * @klass: This class instance
 **/
static void
gpm_phone_class_init (GpmPhoneClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_phone_finalize;
	g_type_class_add_private (klass, sizeof (GpmPhonePrivate));

	signals [DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPhoneClass, device_added),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	signals [DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPhoneClass, device_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	signals [DEVICE_REFRESH] =
		g_signal_new ("device-refresh",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPhoneClass, device_refresh),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
}

/**
 * gpm_phone_service_appeared_cb:
 */
static void
gpm_phone_service_appeared_cb (GDBusConnection *connection,
			       const gchar *name, const gchar *name_owner,
			       GpmPhone *phone)
{
	GError *error = NULL;

	g_return_if_fail (GPM_IS_PHONE (phone));

	if (phone->priv->connection == NULL) {
		egg_debug ("get connection");
		g_clear_error (&error);
		phone->priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
		if (phone->priv->connection == NULL) {
			egg_warning ("Could not connect to DBUS daemon: %s", error->message);
			g_error_free (error);
			phone->priv->connection = NULL;
			return;
		}
	}
	if (phone->priv->proxy == NULL) {
		egg_debug ("get proxy");
		g_clear_error (&error);
		phone->priv->proxy = g_dbus_proxy_new_sync (phone->priv->connection,
				G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
				NULL,
				GNOME_PHONE_MANAGER_DBUS_SERVICE,
				GNOME_PHONE_MANAGER_DBUS_PATH,
				GNOME_PHONE_MANAGER_DBUS_INTERFACE,
				NULL, &error);
		if (phone->priv->proxy == NULL) {
			egg_warning ("Cannot connect, maybe the daemon is not running: %s", error->message);
			g_error_free (error);
			phone->priv->proxy = NULL;
			return;
		}

		g_signal_connect (phone->priv->proxy, "g-signal", G_CALLBACK(gpm_phone_generic_signal_cb), phone);
	}
}

/**
 * gpm_phone_service_vanished_cb:
 */
static void
gpm_phone_service_vanished_cb (GDBusConnection *connection,
				const gchar *name,
				GpmPhone *phone)
{
	g_return_if_fail (GPM_IS_PHONE (phone));

	if (phone->priv->proxy != NULL) {
		egg_debug ("removing proxy");
		g_object_unref (phone->priv->proxy);
		phone->priv->proxy = NULL;
		if (phone->priv->present) {
			phone->priv->present = FALSE;
			phone->priv->percentage = 0;
			egg_debug ("emitting device-removed : (%i)", 0);
			g_signal_emit (phone, signals [DEVICE_REMOVED], 0, 0);
		}
	}
	return;
}

/**
 * gpm_phone_init:
 * @phone: This class instance
 **/
static void
gpm_phone_init (GpmPhone *phone)
{
	phone->priv = GPM_PHONE_GET_PRIVATE (phone);

	phone->priv->connection = NULL;
	phone->priv->proxy = NULL;
	phone->priv->present = FALSE;
	phone->priv->percentage = 0;
	phone->priv->onac = FALSE;

	phone->priv->watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
						  GNOME_PHONE_MANAGER_DBUS_SERVICE,
						  G_BUS_NAME_WATCHER_FLAGS_NONE,
						  (GBusNameAppearedCallback) gpm_phone_service_appeared_cb,
						  (GBusNameVanishedCallback) gpm_phone_service_vanished_cb,
						  phone, NULL);
}

/**
 * gpm_phone_finalize:
 * @object: This class instance
 **/
static void
gpm_phone_finalize (GObject *object)
{
	GpmPhone *phone;
	g_return_if_fail (GPM_IS_PHONE (object));

	phone = GPM_PHONE (object);
	phone->priv = GPM_PHONE_GET_PRIVATE (phone);

	if (phone->priv->proxy != NULL)
		g_object_unref (phone->priv->proxy);
	g_bus_unwatch_name (phone->priv->watch_id);

	G_OBJECT_CLASS (gpm_phone_parent_class)->finalize (object);
}

/**
 * gpm_phone_new:
 * Return value: new GpmPhone instance.
 **/
GpmPhone *
gpm_phone_new (void)
{
	if (gpm_phone_object != NULL) {
		g_object_ref (gpm_phone_object);
	} else {
		gpm_phone_object = g_object_new (GPM_TYPE_PHONE, NULL);
		g_object_add_weak_pointer (gpm_phone_object, &gpm_phone_object);
	}
	return GPM_PHONE (gpm_phone_object);
}

