/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "egg-debug.h"
#include "dkp-wakeups.h"

static void	dkp_wakeups_class_init	(DkpWakeupsClass	*klass);
static void	dkp_wakeups_init	(DkpWakeups		*wakeups);
static void	dkp_wakeups_finalize	(GObject		*object);

#define DKP_WAKEUPS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_WAKEUPS, DkpWakeupsPrivate))

struct DkpWakeupsPrivate
{
	DBusGConnection		*bus;
	DBusGProxy		*proxy;
};

enum {
	DKP_WAKEUPS_DATA_CHANGED,
	DKP_WAKEUPS_TOTAL_CHANGED,
	DKP_WAKEUPS_LAST_SIGNAL
};

static guint signals [DKP_WAKEUPS_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DkpWakeups, dkp_wakeups, G_TYPE_OBJECT)

/**
 * dkp_wakeups_get_total:
 **/
guint
dkp_wakeups_get_total (DkpWakeups *wakeups, GError **error)
{
	guint total = 0;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (DKP_IS_WAKEUPS (wakeups), FALSE);
	g_return_val_if_fail (wakeups->priv->proxy != NULL, FALSE);

	ret = dbus_g_proxy_call (wakeups->priv->proxy, "GetTotal", &error_local,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &total,
				 G_TYPE_INVALID);
	if (!ret) {
		egg_warning ("Couldn't get total: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
	}
	return total;
}

/**
 * dkp_wakeups_get_data:
 *
 * Returns an array of %DkpWakeupsObj's
 **/
GPtrArray *
dkp_wakeups_get_data (DkpWakeups *wakeups, GError **error)
{
	GError *error_local = NULL;
	GType g_type_gvalue_array;
	GPtrArray *gvalue_ptr_array = NULL;
	GValueArray *gva;
	GValue *gv;
	guint i;
	DkpWakeupsObj *obj;
	GPtrArray *array = NULL;
	gboolean ret;

	g_return_val_if_fail (DKP_IS_WAKEUPS (wakeups), FALSE);
	g_return_val_if_fail (wakeups->priv->proxy != NULL, FALSE);

	g_type_gvalue_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_BOOLEAN,
						G_TYPE_UINT,
						G_TYPE_DOUBLE,
						G_TYPE_STRING,
						G_TYPE_STRING,
						G_TYPE_INVALID));

	/* get compound data */
	ret = dbus_g_proxy_call (wakeups->priv->proxy, "GetData", &error_local,
				 G_TYPE_INVALID,
				 g_type_gvalue_array, &gvalue_ptr_array,
				 G_TYPE_INVALID);
	if (!ret) {
		egg_debug ("GetData on failed: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* no data */
	if (gvalue_ptr_array->len == 0)
		goto out;

	/* convert */
	array = g_ptr_array_new ();
	for (i=0; i<gvalue_ptr_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (gvalue_ptr_array, i);
		obj = dkp_wakeups_obj_new ();

		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		obj->is_userspace = g_value_get_boolean (gv);
		g_value_unset (gv);

		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		obj->id = g_value_get_uint (gv);
		g_value_unset (gv);

		/* 2 */
		gv = g_value_array_get_nth (gva, 2);
		obj->value = g_value_get_double (gv);
		g_value_unset (gv);

		/* 3 */
		gv = g_value_array_get_nth (gva, 3);
		obj->cmdline = g_strdup (g_value_get_string (gv));
		g_value_unset (gv);

		/* 4 */
		gv = g_value_array_get_nth (gva, 4);
		obj->details = g_strdup (g_value_get_string (gv));
		g_value_unset (gv);

		/* add */
		g_ptr_array_add (array, obj);
		g_value_array_free (gva);
	}
out:
	if (gvalue_ptr_array != NULL)
		g_ptr_array_free (gvalue_ptr_array, TRUE);
	return array;
}

/**
 * dkp_wakeups_total_changed_cb:
 **/
static void
dkp_wakeups_total_changed_cb (DBusGProxy *proxy, guint value, DkpWakeups *wakeups)
{
	g_signal_emit (wakeups, signals [DKP_WAKEUPS_TOTAL_CHANGED], 0, value);
}

/**
 * dkp_wakeups_data_changed_cb:
 **/
static void
dkp_wakeups_data_changed_cb (DBusGProxy *proxy, DkpWakeups *wakeups)
{
	g_signal_emit (wakeups, signals [DKP_WAKEUPS_DATA_CHANGED], 0);
}

/**
 * dkp_wakeups_class_init:
 * @klass: The DkpWakeupsClass
 **/
static void
dkp_wakeups_class_init (DkpWakeupsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dkp_wakeups_finalize;

	signals [DKP_WAKEUPS_DATA_CHANGED] =
		g_signal_new ("data-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpWakeupsClass, data_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [DKP_WAKEUPS_TOTAL_CHANGED] =
		g_signal_new ("total-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpWakeupsClass, data_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (DkpWakeupsPrivate));
}

/**
 * dkp_wakeups_init:
 * @wakeups: This class instance
 **/
static void
dkp_wakeups_init (DkpWakeups *wakeups)
{
	GError *error = NULL;

	wakeups->priv = DKP_WAKEUPS_GET_PRIVATE (wakeups);

	/* get on the bus */
	wakeups->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (wakeups->priv->bus == NULL) {
		egg_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to main interface */
	wakeups->priv->proxy = dbus_g_proxy_new_for_name (wakeups->priv->bus,
							 "org.freedesktop.DeviceKit.Power",
							 "/org/freedesktop/DeviceKit/Power/Wakeups",
							 "org.freedesktop.DeviceKit.Power.Wakeups");
	if (wakeups->priv->proxy == NULL) {
		egg_warning ("Couldn't connect to proxy");
		goto out;
	}
	dbus_g_proxy_add_signal (wakeups->priv->proxy, "TotalChanged", G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (wakeups->priv->proxy, "DataChanged", G_TYPE_INVALID);

	/* all callbacks */
	dbus_g_proxy_connect_signal (wakeups->priv->proxy, "TotalChanged",
				     G_CALLBACK (dkp_wakeups_total_changed_cb), wakeups, NULL);
	dbus_g_proxy_connect_signal (wakeups->priv->proxy, "DataChanged",
				     G_CALLBACK (dkp_wakeups_data_changed_cb), wakeups, NULL);
out:
	return;
}

/**
 * dkp_wakeups_finalize:
 * @object: The object to finalize
 **/
static void
dkp_wakeups_finalize (GObject *object)
{
	DkpWakeups *wakeups;

	g_return_if_fail (DKP_IS_WAKEUPS (object));

	wakeups = DKP_WAKEUPS (object);
	if (wakeups->priv->proxy != NULL)
		g_object_unref (wakeups->priv->proxy);

	G_OBJECT_CLASS (dkp_wakeups_parent_class)->finalize (object);
}

/**
 * dkp_wakeups_new:
 *
 * Return value: a new DkpWakeups object.
 **/
DkpWakeups *
dkp_wakeups_new (void)
{
	DkpWakeups *wakeups;
	wakeups = g_object_new (DKP_TYPE_WAKEUPS, NULL);
	return DKP_WAKEUPS (wakeups);
}

