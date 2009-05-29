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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <dkp-client.h>
#include <dkp-device.h>

#include "egg-debug.h"

#include "gpm-common.h"
#include "gpm-devicekit.h"
#include "gpm-marshal.h"
#include "gpm-engine.h"
#include "gpm-stock-icons.h"
#include "gpm-prefs-server.h"
#include "gpm-phone.h"

static void     gpm_engine_finalize   (GObject	  *object);

#define GPM_ENGINE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_ENGINE, GpmEnginePrivate))
#define GPM_ENGINE_RESUME_DELAY		2*1000
#define GPM_ENGINE_WARN_ACCURACY	20

/* Left to convert:
 * 1. Recall data
 */

struct GpmEnginePrivate
{
	GConfClient		*conf;
	DkpClient		*client;
	GPtrArray		*array;
	GpmPhone		*phone;
	GpmIconPolicy		 icon_policy;
	gchar			*previous_icon;
	gchar			*previous_summary;

	gboolean		 use_time_primary;
	gboolean		 time_is_accurate;

	guint			 low_percentage;
	guint			 critical_percentage;
	guint			 action_percentage;
	guint			 low_time;
	guint			 critical_time;
	guint			 action_time;
};

enum {
	ICON_CHANGED,
	SUMMARY_CHANGED,
	FULLY_CHARGED,
	CHARGE_LOW,
	CHARGE_CRITICAL,
	CHARGE_ACTION,
	DISCHARGING,
	LOW_CAPACITY,
	PERHAPS_RECALL,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_engine_object = NULL;

G_DEFINE_TYPE (GpmEngine, gpm_engine, G_TYPE_OBJECT)

typedef enum {
	GPM_ENGINE_WARNING_NONE = 0,
	GPM_ENGINE_WARNING_DISCHARGING = 1,
	GPM_ENGINE_WARNING_LOW = 2,
	GPM_ENGINE_WARNING_CRITICAL = 3,
	GPM_ENGINE_WARNING_ACTION = 4
} GpmEngineWarning;

/**
 * gpm_engine_get_warning_csr:
 **/
static GpmEngineWarning
gpm_engine_get_warning_csr (GpmEngine *engine, const DkpObject *obj)
{
	if (obj->percentage < 26.0f)
		return GPM_ENGINE_WARNING_LOW;
	else if (obj->percentage < 13.0f)
		return GPM_ENGINE_WARNING_CRITICAL;
	return GPM_ENGINE_WARNING_NONE;
}

/**
 * gpm_engine_get_warning_percentage:
 **/
static GpmEngineWarning
gpm_engine_get_warning_percentage (GpmEngine *engine, const DkpObject *obj)
{
	/* this is probably an error condition */
	if (obj->percentage == 0) {
		egg_warning ("percentage zero, something's gone wrong");
		return GPM_ENGINE_WARNING_NONE;
	}
	if (obj->percentage <= engine->priv->action_percentage)
		return GPM_ENGINE_WARNING_ACTION;
	else if (obj->percentage <= engine->priv->critical_percentage)
		return GPM_ENGINE_WARNING_CRITICAL;
	else if (obj->percentage <= engine->priv->low_percentage)
		return GPM_ENGINE_WARNING_LOW;
	return GPM_ENGINE_WARNING_NONE;
}

/**
 * gpm_engine_get_warning_time:
 **/
static GpmEngineWarning
gpm_engine_get_warning_time (GpmEngine *engine, const DkpObject *obj)
{
	/* this is probably an error condition */
	if (obj->time_to_empty == 0) {
		egg_debug ("time zero, falling back to percentage for %s", dkp_device_type_to_text (obj->type));
		return gpm_engine_get_warning_percentage (engine, obj);
	}

	if (obj->time_to_empty <= engine->priv->action_time)
		return GPM_ENGINE_WARNING_ACTION;
	else if (obj->time_to_empty <= engine->priv->critical_time)
		return GPM_ENGINE_WARNING_CRITICAL;
	else if (obj->time_to_empty <= engine->priv->low_time)
		return GPM_ENGINE_WARNING_LOW;
	return GPM_ENGINE_WARNING_NONE;
}

/**
 * gpm_engine_get_warning:
 *
 * This gets the possible engine state for the device according to the
 * policy, which could be per-percent, or per-time.
 *
 * Return value: A GpmEngine state, e.g. GPM_ENGINE_WARNING_DISCHARGING
 **/
static GpmEngineWarning
gpm_engine_get_warning (GpmEngine *engine, const DkpObject *obj)
{
	GpmEngineWarning type;

	/* default to no engine */
	type = GPM_ENGINE_WARNING_NONE;

	if (obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD) {

		type = gpm_engine_get_warning_csr (engine, obj);

	} else if (obj->type == DKP_DEVICE_TYPE_UPS ||
		   obj->type == DKP_DEVICE_TYPE_PDA) {

		type = gpm_engine_get_warning_percentage (engine, obj);

	} else if (obj->type == DKP_DEVICE_TYPE_PHONE) {

		type = gpm_engine_get_warning_percentage (engine, obj);

	} else if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		/* only use the time when it is accurate, and GConf is not disabled */
		if (engine->priv->use_time_primary)
			type = gpm_engine_get_warning_time (engine, obj);
		else
			type = gpm_engine_get_warning_percentage (engine, obj);
	}

	/* If we have no important engines, we should test for discharging */
	if (type == GPM_ENGINE_WARNING_NONE) {
		if (obj->state == DKP_DEVICE_STATE_DISCHARGING)
			type = GPM_ENGINE_WARNING_DISCHARGING;
	}
	return type;
}

/**
 * gpm_engine_get_summary:
 * @engine: This engine class instance
 * @string: The returned string
 *
 * Returns the complete tooltip ready for display
 **/
gchar *
gpm_engine_get_summary (GpmEngine *engine)
{
	guint i;
	GPtrArray *array;
	const DkpDevice *device;
	const DkpObject	*obj;
	GString *tooltip = NULL;
	gchar *part;

	g_return_val_if_fail (GPM_IS_ENGINE (engine), NULL);

	/* need to get AC state */
	tooltip = g_string_new ("");

	/* do we have specific device types? */
	array = engine->priv->array;
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (engine->priv->array, i);
		obj = dkp_device_get_object (device);
		part = gpm_devicekit_get_object_summary (obj);
		if (part != NULL)
			g_string_append_printf (tooltip, "%s\n", part);
		g_free (part);
	}

	/* remove the last \n */
	g_string_truncate (tooltip, tooltip->len-1);

	egg_debug ("tooltip: %s", tooltip->str);

	return g_string_free (tooltip, FALSE);
}

/**
 * gpm_engine_get_icon_priv:
 *
 * Returns the icon
 **/
static gchar *
gpm_engine_get_icon_priv (GpmEngine *engine, DkpDeviceType type, GpmEngineWarning warning, gboolean state)
{
	guint i;
	GPtrArray *array;
	const DkpDevice *device;
	const DkpObject	*obj;
	GpmEngineWarning warning_temp;

	/* do we have specific device types? */
	array = engine->priv->array;
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (engine->priv->array, i);
		obj = dkp_device_get_object (device);
		warning_temp = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-warning-old"));
		if (obj->type == type && obj->is_present) {
			if (warning != GPM_ENGINE_WARNING_NONE) {
				if (warning_temp == warning)
					return gpm_devicekit_get_object_icon (obj);
				continue;
			}
			if (state) {
				if (obj->state == DKP_DEVICE_STATE_CHARGING || obj->state == DKP_DEVICE_STATE_DISCHARGING)
					return gpm_devicekit_get_object_icon (obj);
				continue;
			}
			return gpm_devicekit_get_object_icon (obj);
		}
	}
	return NULL;
}

/**
 * gpm_engine_get_icon:
 *
 * Returns the icon
 **/
gchar *
gpm_engine_get_icon (GpmEngine *engine)
{
	gchar *icon = NULL;

	g_return_val_if_fail (GPM_IS_ENGINE (engine), NULL);

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_NEVER) {
		egg_debug ("no icon allowed, so no icon will be displayed.");
		return NULL;
	}

	/* we try CRITICAL: BATTERY, UPS, MOUSE, KEYBOARD */
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_BATTERY, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_UPS, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_MOUSE, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_KEYBOARD, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_CRITICAL) {
		egg_debug ("no devices critical, so no icon will be displayed.");
		return NULL;
	}

	/* we try (DIS)CHARGING: BATTERY, UPS */
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_BATTERY, GPM_ENGINE_WARNING_NONE, TRUE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_UPS, GPM_ENGINE_WARNING_NONE, TRUE);
	if (icon != NULL)
		return icon;

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_CHARGE) {
		egg_debug ("no devices (dis)charging, so no icon will be displayed.");
		return NULL;
	}

	/* we try PRESENT: BATTERY, UPS */
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_BATTERY, GPM_ENGINE_WARNING_NONE, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, DKP_DEVICE_TYPE_UPS, GPM_ENGINE_WARNING_NONE, FALSE);
	if (icon != NULL)
		return icon;

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_PRESENT) {
		egg_debug ("no devices present, so no icon will be displayed.");
		return NULL;
	}

	/* we fallback to the ac_adapter icon */
	egg_debug ("Using fallback");
	return g_strdup (GPM_STOCK_AC_ADAPTER);
}

/**
 * gpm_engine_recalculate_state_icon:
 */
static gboolean
gpm_engine_recalculate_state_icon (GpmEngine *engine)
{
	gchar *icon;

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), FALSE);

	/* show a different icon if we are disconnected */
	icon = gpm_engine_get_icon (engine);
	if (icon == NULL) {
		/* none before, now none */
		if (engine->priv->previous_icon == NULL)
			return FALSE;
		/* icon before, now none */
		egg_debug ("** EMIT: icon-changed: none");
		g_signal_emit (engine, signals [ICON_CHANGED], 0, NULL);

		g_free (engine->priv->previous_icon);
		engine->priv->previous_icon = NULL;
		return TRUE;
	}

	/* no icon before, now icon */
	if (engine->priv->previous_icon == NULL) {
		egg_debug ("** EMIT: icon-changed: %s", icon);
		g_signal_emit (engine, signals [ICON_CHANGED], 0, icon);
		engine->priv->previous_icon = icon;
		return TRUE;
	}

	/* icon before, now different */
	if (strcmp (engine->priv->previous_icon, icon) != 0) {
		g_free (engine->priv->previous_icon);
		engine->priv->previous_icon = icon;
		egg_debug ("** EMIT: icon-changed: %s", icon);
		g_signal_emit (engine, signals [ICON_CHANGED], 0, icon);
		return TRUE;
	}

	egg_debug ("no change");
	/* nothing to do */
	g_free (icon);
	return FALSE;
}

/**
 * gpm_engine_recalculate_state_summary:
 */
static gboolean
gpm_engine_recalculate_state_summary (GpmEngine *engine)
{
	gchar *summary;

	summary = gpm_engine_get_summary (engine);
	if (engine->priv->previous_summary == NULL) {
		engine->priv->previous_summary = summary;
		egg_debug ("** EMIT: summary-changed(1): %s", summary);
		g_signal_emit (engine, signals [SUMMARY_CHANGED], 0, summary);
		return TRUE;
	}	

	if (strcmp (engine->priv->previous_summary, summary) != 0) {
		g_free (engine->priv->previous_summary);
		engine->priv->previous_summary = summary;
		egg_debug ("** EMIT: summary-changed(2): %s", summary);
		g_signal_emit (engine, signals [SUMMARY_CHANGED], 0, summary);
		return TRUE;
	}
	egg_debug ("no change");
	/* nothing to do */
	g_free (summary);
	return FALSE;
}

/**
 * gpm_engine_recalculate_state:
 */
static void
gpm_engine_recalculate_state (GpmEngine *engine)
{

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	gpm_engine_recalculate_state_icon (engine);
	gpm_engine_recalculate_state_summary (engine);
}

/**
 * gpm_engine_conf_key_changed_cb:
 **/
static void
gpm_engine_conf_key_changed_cb (GConfClient *conf, guint cnxn_id, GConfEntry *entry, GpmEngine *engine)
{
	GConfValue *value;
	gchar *icon_policy;

	if (entry == NULL)
		return;
	value = gconf_entry_get_value (entry);
	if (value == NULL)
		return;

	if (strcmp (entry->key, GPM_CONF_USE_TIME_POLICY) == 0) {

		engine->priv->use_time_primary = gconf_value_get_bool (value);

	} else if (strcmp (entry->key, GPM_CONF_UI_ICON_POLICY) == 0) {

		/* do we want to display the icon in the tray */
		icon_policy = gconf_client_get_string (conf, GPM_CONF_UI_ICON_POLICY, NULL);
		engine->priv->icon_policy = gpm_tray_icon_mode_from_string (icon_policy);
		g_free (icon_policy);

		/* perhaps change icon */
		gpm_engine_recalculate_state_icon (engine);
	}
}

/**
 * gpm_engine_device_check_capacity:
 **/
static gboolean
gpm_engine_device_check_capacity (GpmEngine *engine, const DkpDevice *device)
{
	const DkpObject	*obj;
	gboolean ret;

	obj = dkp_device_get_object (device);
	if (obj->type != DKP_DEVICE_TYPE_BATTERY || obj->capacity > 50.0f)
		return FALSE;

	/* only emit this if specified in gconf */
	ret = gconf_client_get_bool (engine->priv->conf, GPM_CONF_NOTIFY_LOW_CAPACITY, NULL);
	if (ret) {
		egg_debug ("** EMIT: low-capacity");
		g_signal_emit (engine, signals [LOW_CAPACITY], 0, obj->type, (guint) obj->capacity);
	}
	return TRUE;
}

/**
 * gpm_engine_device_add:
 **/
static void
gpm_engine_device_add (GpmEngine *engine, DkpDevice *device)
{
	GpmEngineWarning warning;
	const DkpObject *obj;

	/* assign warning */
	obj = dkp_device_get_object (device);
	warning = gpm_engine_get_warning (engine, obj);
	g_object_set_data (G_OBJECT(device), "engine-warning-old", GUINT_TO_POINTER(warning));

	/* check capacity */
	gpm_engine_device_check_capacity (engine, device);

	/* add old state for transitions */
	g_object_set_data (G_OBJECT(device), "engine-state-old", GUINT_TO_POINTER(obj->state));
}

/**
 * gpm_engine_coldplug_idle_cb:
 **/
static gboolean
gpm_engine_coldplug_idle_cb (GpmEngine *engine)
{
	guint i;
	GPtrArray *array;
	gboolean has_battery = FALSE;
	gboolean has_ups = FALSE;
	GpmPrefsServer *prefs_server;
	DkpDevice *device;
	const DkpObject	*obj;

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), FALSE);

	/* get array */
	engine->priv->array = dkp_client_enumerate_devices (engine->priv->client);

	/* do we have specific device types? */
	array = engine->priv->array;
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (engine->priv->array, i);
		obj = dkp_device_get_object (device);
		if (obj->type == DKP_DEVICE_TYPE_BATTERY)
			has_battery = TRUE;
		else if (obj->type == DKP_DEVICE_TYPE_UPS)
			has_ups = TRUE;
	}

	/* only show the battery prefs section if we have batteries */
	prefs_server = gpm_prefs_server_new ();
	if (has_battery)
		gpm_prefs_server_set_capability (prefs_server, GPM_PREFS_SERVER_BATTERY);
	if (has_ups)
		gpm_prefs_server_set_capability (prefs_server, GPM_PREFS_SERVER_UPS);
	g_object_unref (prefs_server);

	/* connected mobile phones */
	gpm_phone_coldplug (engine->priv->phone);

	gpm_engine_recalculate_state (engine);

	/* add to database */
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (engine->priv->array, i);
		gpm_engine_device_add (engine, device);
	}

	/* never repeat */
	return FALSE;
}

/**
 * gpm_engine_device_added_cb:
 **/
static void
gpm_engine_device_added_cb (DkpClient *client, DkpDevice *device, GpmEngine *engine)
{
	/* add to list */
	g_ptr_array_add (engine->priv->array, g_object_ref (device));

	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_device_removed_cb:
 **/
static void
gpm_engine_device_removed_cb (DkpClient *client, DkpDevice *device, GpmEngine *engine)
{
	gboolean ret;
	ret = g_ptr_array_remove (engine->priv->array, device);
	if (!ret)
		return;
	g_object_unref (device);
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_device_changed_cb:
 **/
static void
gpm_engine_device_changed_cb (DkpClient *client, DkpDevice *device, GpmEngine *engine)
{
	DkpDeviceState state;
	const DkpObject *obj;
	GpmEngineWarning warning_old;
	GpmEngineWarning warning;
	gboolean ret;

	obj = dkp_device_get_object (device);

	/* check the warning state has not changed */
	warning_old = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-warning-old"));
	warning = gpm_engine_get_warning (engine, obj);
	if (warning != warning_old) {
		if (warning == GPM_ENGINE_WARNING_LOW) {
			egg_debug ("** EMIT: charge-low");
			g_signal_emit (engine, signals [CHARGE_LOW], 0, obj);
		} else if (warning == GPM_ENGINE_WARNING_CRITICAL) {
			egg_debug ("** EMIT: charge-critical");
			g_signal_emit (engine, signals [CHARGE_CRITICAL], 0, obj);
		} else if (warning == GPM_ENGINE_WARNING_ACTION) {
			egg_debug ("** EMIT: charge-action");
			g_signal_emit (engine, signals [CHARGE_ACTION], 0, obj);
		}
		/* save new state */
		g_object_set_data (G_OBJECT(device), "engine-warning-old", GUINT_TO_POINTER(warning));
	}

	/* see if any interesting state changes have happened */
	state = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-state-old"));
	if (state != obj->state) {

		if (state == DKP_DEVICE_STATE_DISCHARGING) {
			/* only emit this if specified in gconf */
			ret = gconf_client_get_bool (engine->priv->conf, GPM_CONF_NOTIFY_DISCHARGING, NULL);
			if (ret) {
				egg_debug ("** EMIT: discharging");
				g_signal_emit (engine, signals [DISCHARGING], 0, obj->type);
			}
		} else if (state == DKP_DEVICE_STATE_FULLY_CHARGED) {
			/* only emit this if specified in gconf */
			ret = gconf_client_get_bool (engine->priv->conf, GPM_CONF_NOTIFY_FULLY_CHARGED, NULL);
			if (ret) {
				egg_debug ("** EMIT: discharging");
				g_signal_emit (engine, signals [FULLY_CHARGED], 0, obj->type);
			}
		}

		/* save new state */
		g_object_set_data (G_OBJECT(device), "engine-state-old", GUINT_TO_POINTER(obj->state));
	}

	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_get_devices:
 **/
const GPtrArray *
gpm_engine_get_devices (GpmEngine *engine)
{
	guint i;
	DkpDevice *device;
	GPtrArray *array = engine->priv->array;

	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		g_object_ref (G_OBJECT(device));
	}

	return engine->priv->array;
}

/**
 * phone_device_added_cb:
 **/
static void
phone_device_added_cb (GpmPhone *phone, guint idx, GpmEngine *engine)
{
	DkpObject *obj;
	DkpDevice *device;
	device = dkp_device_new ();

	egg_debug ("phone added %i", idx);
	
	obj = (DkpObject *) dkp_device_get_object (device);
	obj->native_path = g_strdup_printf ("phone_%i", idx);
	obj->is_rechargeable = TRUE;
	obj->type = DKP_DEVICE_TYPE_PHONE;

	/* state changed */
	gpm_engine_device_add (engine, device);
	g_ptr_array_add (engine->priv->array, g_object_ref (device));
	gpm_engine_recalculate_state (engine);
}

/**
 * phone_device_removed_cb:
 **/
static void
phone_device_removed_cb (GpmPhone *phone, guint idx, GpmEngine *engine)
{
	guint i;
	DkpDevice *device;
	const DkpObject *obj;

	egg_debug ("phone removed %i", idx);

	for (i=0; i<engine->priv->array->len; i++) {
		device = g_ptr_array_index (engine->priv->array, i);
		obj = dkp_device_get_object (device);
		if (obj->type == DKP_DEVICE_TYPE_PHONE) {
			g_ptr_array_remove_index (engine->priv->array, i);
			g_object_unref (device);
			break;
		}
	}

	/* state changed */
	gpm_engine_recalculate_state (engine);
}

/**
 * phone_device_refresh_cb:
 **/
static void
phone_device_refresh_cb (GpmPhone *phone, guint idx, GpmEngine *engine)
{
	guint i;
	DkpDevice *device;
	DkpObject *obj;

	egg_debug ("phone refresh %i", idx);

	for (i=0; i<engine->priv->array->len; i++) {
		device = g_ptr_array_index (engine->priv->array, i);
		obj = (DkpObject *) dkp_device_get_object (device);
		if (obj->type == DKP_DEVICE_TYPE_PHONE) {
			obj->is_present = gpm_phone_get_present (phone, idx);
			obj->state = gpm_phone_get_on_ac (phone, idx) ? DKP_DEVICE_STATE_CHARGING : DKP_DEVICE_STATE_DISCHARGING;
			obj->percentage = gpm_phone_get_percentage (phone, idx);
			break;
		}
	}

	/* state changed */
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_init:
 * @engine: This class instance
 **/
static void
gpm_engine_init (GpmEngine *engine)
{
	gchar *icon_policy;

	engine->priv = GPM_ENGINE_GET_PRIVATE (engine);

	engine->priv->array = g_ptr_array_new ();
	engine->priv->client = dkp_client_new ();
	g_signal_connect (engine->priv->client, "device-added",
			  G_CALLBACK (gpm_engine_device_added_cb), engine);
	g_signal_connect (engine->priv->client, "device-removed",
			  G_CALLBACK (gpm_engine_device_removed_cb), engine);
	g_signal_connect (engine->priv->client, "device-changed",
			  G_CALLBACK (gpm_engine_device_changed_cb), engine);

	engine->priv->conf = gconf_client_get_default ();
	gconf_client_notify_add (engine->priv->conf, GPM_CONF_DIR,
				 (GConfClientNotifyFunc) gpm_engine_conf_key_changed_cb,
				 engine, NULL, NULL);

	engine->priv->phone = gpm_phone_new ();
	g_signal_connect (engine->priv->phone, "device-added",
			  G_CALLBACK (phone_device_added_cb), engine);
	g_signal_connect (engine->priv->phone, "device-removed",
			  G_CALLBACK (phone_device_removed_cb), engine);
	g_signal_connect (engine->priv->phone, "device-refresh",
			  G_CALLBACK (phone_device_refresh_cb), engine);

	engine->priv->previous_icon = NULL;
	engine->priv->previous_summary = NULL;

	/* do we want to display the icon in the tray */
	icon_policy = gconf_client_get_string (engine->priv->conf, GPM_CONF_UI_ICON_POLICY, NULL);
	engine->priv->icon_policy = gpm_tray_icon_mode_from_string (icon_policy);
	g_free (icon_policy);

	/* get percentage policy */
	engine->priv->low_percentage = gconf_client_get_int (engine->priv->conf, GPM_CONF_THRESH_PERCENTAGE_LOW, NULL);
	engine->priv->critical_percentage = gconf_client_get_int (engine->priv->conf, GPM_CONF_THRESH_PERCENTAGE_CRITICAL, NULL);
	engine->priv->action_percentage = gconf_client_get_int (engine->priv->conf, GPM_CONF_THRESH_PERCENTAGE_ACTION, NULL);

	/* get time policy */
	engine->priv->low_time = gconf_client_get_int (engine->priv->conf, GPM_CONF_THRESH_TIME_LOW, NULL);
	engine->priv->critical_time = gconf_client_get_int (engine->priv->conf, GPM_CONF_THRESH_TIME_CRITICAL, NULL);
	engine->priv->action_time = gconf_client_get_int (engine->priv->conf, GPM_CONF_THRESH_TIME_ACTION, NULL);

	/* we can disable this if the time remaining is inaccurate or just plain wrong */
	engine->priv->use_time_primary = gconf_client_get_bool (engine->priv->conf, GPM_CONF_USE_TIME_POLICY, NULL);
	if (engine->priv->use_time_primary)
		egg_debug ("Using per-time notification policy");
	else
		egg_debug ("Using percentage notification policy");

	g_idle_add ((GSourceFunc) gpm_engine_coldplug_idle_cb, engine);
}

/**
 * gpm_engine_class_init:
 * @engine: This class instance
 **/
static void
gpm_engine_class_init (GpmEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_engine_finalize;
	g_type_class_add_private (klass, sizeof (GpmEnginePrivate));

	signals [ICON_CHANGED] =
		g_signal_new ("icon-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, icon_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [SUMMARY_CHANGED] =
		g_signal_new ("summary-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, summary_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [LOW_CAPACITY] =
		g_signal_new ("low-capacity",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, low_capacity),
			      NULL, NULL, gpm_marshal_VOID__UINT_UINT,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
	signals [PERHAPS_RECALL] =
		g_signal_new ("perhaps-recall",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, perhaps_recall),
			      NULL, NULL, gpm_marshal_VOID__UINT_STRING_STRING,
			      G_TYPE_NONE,
			      3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);
	signals [FULLY_CHARGED] =
		g_signal_new ("fully-charged",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, fully_charged),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [DISCHARGING] =
		g_signal_new ("discharging",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, discharging),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [CHARGE_ACTION] =
		g_signal_new ("charge-action",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_action),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [CHARGE_LOW] =
		g_signal_new ("charge-low",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_low),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [CHARGE_CRITICAL] =
		g_signal_new ("charge-critical",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_critical),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
}

/**
 * gpm_engine_finalize:
 * @object: This class instance
 **/
static void
gpm_engine_finalize (GObject *object)
{
	GpmEngine *engine;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_ENGINE (object));

	engine = GPM_ENGINE (object);
	engine->priv = GPM_ENGINE_GET_PRIVATE (engine);

	g_ptr_array_foreach (engine->priv->array, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (engine->priv->array, TRUE);

	g_object_unref (engine->priv->client);
	g_object_unref (engine->priv->phone);

	g_free (engine->priv->previous_icon);
	g_free (engine->priv->previous_summary);

	G_OBJECT_CLASS (gpm_engine_parent_class)->finalize (object);
}

/**
 * gpm_engine_new:
 * Return value: new class instance.
 **/
GpmEngine *
gpm_engine_new (void)
{
	if (gpm_engine_object != NULL) {
		g_object_ref (gpm_engine_object);
	} else {
		gpm_engine_object = g_object_new (GPM_TYPE_ENGINE, NULL);
		g_object_add_weak_pointer (gpm_engine_object, &gpm_engine_object);
	}
	return GPM_ENGINE (gpm_engine_object);

}

