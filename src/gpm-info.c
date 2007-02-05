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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <string.h>
#include <time.h>
#include <dbus/dbus-gtype-specialized.h>

#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-dpms.h"
#include "gpm-hal.h"
#include "gpm-info.h"
#include "gpm-info-data.h"
#include "gpm-power.h"
#include "gpm-stock-icons.h"
#include "gpm-idle.h"

static void     gpm_info_class_init (GpmInfoClass *klass);
static void     gpm_info_init       (GpmInfo      *info);
static void     gpm_info_finalize   (GObject      *object);

#define GPM_INFO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO, GpmInfoPrivate))

#define GPM_INFO_DATA_POLL		5	/* seconds */

#define GPM_DBUS_STRUCT_INT_INT (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))
#define GPM_DBUS_STRUCT_INT_INT_INT (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))

struct GpmInfoPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmButton		*button;
	GpmControl		*control;
	GpmDpms			*dpms;
	GpmHal			*hal;
	GpmIdle			*idle;
	GpmPower		*power;

	GpmInfoData		*events;
	GpmInfoData		*rate_data;
	GpmInfoData		*time_data;
	GpmInfoData		*percentage_data;
	GpmInfoData		*voltage_data;

	time_t			 start_time;
	gboolean		 is_laptop;
};

G_DEFINE_TYPE (GpmInfo, gpm_info, G_TYPE_OBJECT)

/**
 * gpm_info_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_info_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("gpm_info_error");
	}
	return quark;
}

/**
 * gpm_info_explain_reason:
 * @manager: This class instance
 * @event: The event type, e.g. GPM_EVENT_DPMS_OFF
 * @pre: The action we are about to do, e.g. "Suspending computer"
 * @post: The reason we are performing the policy action, e.g. "battery critical"
 *
 * Helper function
 **/
void
gpm_info_explain_reason (GpmInfo      *info,
			 GpmGraphWidgetEvent event,
			 const gchar  *pre,
			 const gchar  *post)
{
	gchar *message;
	if (post) {
		message = g_strdup_printf (_("%s because %s"), pre, post);
	} else {
		message = g_strdup (pre);
	}
	gpm_syslog (message);
	gpm_info_event_log (info, event, message);
	g_free (message);
}

/**
 * device_list_to_strv:
 **/
static gchar **
device_list_to_strv (GList *list)
{
	char **value = NULL;
	char *device;
	GList *l;
	int i = 0;

	value = g_new0 (gchar *, g_list_length (list) + 1);
	for (l=list; l != NULL; l=l->next) {
		device = (gchar *) l->data;
		value[i] = g_strdup (device);
		++i;
	}
	value[i] = NULL;
	return value;
}

/**
 * gpm_statistics_get_types:
 * @info: This class instance
 * @types: The return type STRV artay
 *
 * Return value: TRUE for success.
 **/
gboolean
gpm_statistics_get_types (GpmInfo  *info,
			  gchar  ***types,
			  GError  **error)
{
	GList *list = NULL;
	GList *data;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);
	g_return_val_if_fail (types != NULL, FALSE);

	/* only return the type if we have data */
	data = gpm_info_data_get_list (info->priv->rate_data);
	if (g_list_length (data) > 0) {
		list = g_list_append (list, "power");
	}
	data = gpm_info_data_get_list (info->priv->time_data);
	if (g_list_length (data) > 0) {
		list = g_list_append (list, "time");
	}
	data = gpm_info_data_get_list (info->priv->percentage_data);
	if (g_list_length (data) > 0) {
		list = g_list_append (list, "charge");
	}
	data = gpm_info_data_get_list (info->priv->voltage_data);
	if (g_list_length (data) > 0) {
		list = g_list_append (list, "voltage");
	}

	*types = device_list_to_strv (list);

	g_list_free (list);
	return TRUE;
}

/**
 * gpm_statistics_get_axis_type:
 * @info: This class instance
 * @type: The graph type, e.g. "charge", "power", "time", etc.
 * @axis_type_x: The axis type, only "percentage", "power" or "time"
 * @axis_type_y: The axis type, only "percentage", "power" or "time"
 *
 * Return value: TRUE for success, if FALSE then error set
 **/
gboolean
gpm_statistics_get_axis_type (GpmInfo *info,
			      gchar   *type,
			      gchar  **axis_type_x,
			      gchar  **axis_type_y,
			      GError **error)
{
	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	g_return_val_if_fail (axis_type_x != NULL, FALSE);
	g_return_val_if_fail (axis_type_y != NULL, FALSE);

	if (strcmp (type, "power") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("power");
		return TRUE;
	}
	if (strcmp (type, "time") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("time");
		return TRUE;
	}
	if (strcmp (type, "charge") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("percentage");
		return TRUE;
	}
	if (strcmp (type, "voltage") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("voltage");
		return TRUE;
	}
	
	/* not recognised... */
	*error = g_error_new (gpm_info_error_quark (),
			      GPM_INFO_ERROR_INVALID_TYPE,
			      "Invalid type %s", type);
	return FALSE;
}

/**
 * gpm_statistics_get_event_log:
 * @info: This class instance
 * @seconds: The amount of data to get, currently unused.
 *
 * Return value: TRUE for success
 **/
gboolean
gpm_statistics_get_event_log (GpmInfo    *info,
			      gint 	  seconds,
			      GPtrArray **array,
			      GError    **error)
{
	GList *events, *l;
	GpmInfoDataPoint *new;
	GValue *value;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);
	g_return_val_if_fail (array != NULL, FALSE);

	gpm_debug ("seconds=%i", seconds);
	events = gpm_info_data_get_list (info->priv->events);
	*array = g_ptr_array_sized_new (g_list_length (events));

	for (l=events; l != NULL; l=l->next) {
		new = (GpmInfoDataPoint *) l->data;
		value = g_new0 (GValue, 1);
		g_value_init (value, GPM_DBUS_STRUCT_INT_INT);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (GPM_DBUS_STRUCT_INT_INT));
		dbus_g_type_struct_set (value, 0, new->time, 1, new->value, -1);
		g_ptr_array_add (*array, g_value_get_boxed (value));
		g_free (value);
	}
	g_list_free (events);

	return TRUE;
}

/**
 * gpm_statistics_get_data:
 * @info: This class instance
 * @seconds: The amount of data to get, currently unused.
 * @type: The graph type, e.g. "charge", "power", "time", etc.
 *
 * Return value: TRUE for success
 **/
gboolean
gpm_statistics_get_data (GpmInfo     *info,
			 gint 	      seconds,
			 const gchar *type,
			 GPtrArray  **array,
			 GError	    **error)
{
	GList *events, *l;
	GpmInfoDataPoint *new;
	GValue *value;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	g_return_val_if_fail (array != NULL, FALSE);

	if (info->priv->is_laptop == FALSE) {
		gpm_warning ("Data not available as not a laptop");
		*error = g_error_new (gpm_info_error_quark (),
				      GPM_INFO_ERROR_DATA_NOT_AVAILABLE,
				      "Data not available as not a laptop");
		return FALSE;
	}

	if (strcmp (type, "power") == 0) {
		events = gpm_info_data_get_list (info->priv->rate_data);
	} else if (strcmp (type, "time") == 0) {
		events = gpm_info_data_get_list (info->priv->time_data);
	} else if (strcmp (type, "charge") == 0) {
		events = gpm_info_data_get_list (info->priv->percentage_data);
	} else if (strcmp (type, "voltage") == 0) {
		events = gpm_info_data_get_list (info->priv->voltage_data);
	} else {
		gpm_warning ("Data type %s not known!", type);
		*error = g_error_new (gpm_info_error_quark (),
				      GPM_INFO_ERROR_INVALID_TYPE,
				      "Data type %s not known!", type);
		return FALSE;
	}

	if (events == NULL) {
		gpm_warning ("Data not available");
		*error = g_error_new (gpm_info_error_quark (),
				      GPM_INFO_ERROR_DATA_NOT_AVAILABLE,
				      "Data not available");
		return FALSE;
	}		

	/* TODO! */
	gpm_debug ("seconds=%i", seconds);
	*array = g_ptr_array_sized_new (g_list_length (events));

	for (l=events; l != NULL; l=l->next) {
		new = (GpmInfoDataPoint *) l->data;
		value = g_new0 (GValue, 1);
		g_value_init (value, GPM_DBUS_STRUCT_INT_INT_INT);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (GPM_DBUS_STRUCT_INT_INT_INT));
		dbus_g_type_struct_set (value, 0, new->time, 1, new->value, 2, new->colour, -1);
		g_ptr_array_add (*array, g_value_get_boxed (value));
		g_free (value);
	}

	g_list_free (events);
	return TRUE;
}

/**
 * gpm_info_event_log
 * @info: This info class instance
 * @event: The event description, e.g. "Application started"
 * @desc: A more detailed description, or NULL is none required.
 *
 * Adds an point to the event log
 **/
void
gpm_info_event_log (GpmInfo	       *info,
		    GpmGraphWidgetEvent event,
		    const gchar        *desc)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (GPM_IS_INFO (info));
	gpm_debug ("Adding %i to the event log", event);

	gpm_info_data_add_always (info->priv->events,
				  time (NULL) - info->priv->start_time,
				  event, 0, desc);
}

/**
 * gpm_info_log_do_poll:
 * @data: gpointer to this info class instance
 *
 * This is the callback to get the log data every timeout period, where we have
 * to add points to the database and also update the graphs.
 **/
static gboolean
gpm_info_log_do_poll (gpointer data)
{
	GpmInfo *info = (GpmInfo*) data;

	int value_x;
	int colour;

	GpmPowerStatus battery_status;
	gpm_power_get_battery_status (info->priv->power,
				      GPM_POWER_KIND_PRIMARY,
				      &battery_status);

	if (info->priv->is_laptop == TRUE) {
		/* work out seconds elapsed */
		value_x = time (NULL) - (info->priv->start_time + GPM_INFO_DATA_POLL);

		/* set the correct colours */
		if (battery_status.is_discharging) {
			colour = GPM_GRAPH_WIDGET_COLOUR_DISCHARGING;
		} else if (battery_status.is_charging) {
			colour = GPM_GRAPH_WIDGET_COLOUR_CHARGING;
		} else {
			colour = GPM_GRAPH_WIDGET_COLOUR_CHARGED;
		}

		gpm_info_data_add (info->priv->percentage_data,
				   value_x,
				   battery_status.percentage_charge, colour);

		/* sanity check to less than 100W */
		if (battery_status.charge_rate_raw < 100000) {
			gpm_info_data_add (info->priv->rate_data,
					   value_x,
					   battery_status.charge_rate_raw, colour);
		}

		/* sanity check to less than 10 hours */
		if (battery_status.remaining_time < 10*60*60) {
			gpm_info_data_add (info->priv->time_data,
					   value_x,
					   battery_status.remaining_time, colour);
		}
		gpm_info_data_add (info->priv->voltage_data,
				   value_x,
				   battery_status.voltage, colour);
	}
	return TRUE;
}

/**
 * power_on_ac_changed_cb:
 * @power: The power class instance
 * @on_ac: if we are on AC power
 * @manager: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter     *ac_adapter,
		       GpmAcAdapterState state,
		       GpmInfo          *info)
{
	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_info_event_log (info, GPM_EVENT_ON_AC,
				    _("AC adapter inserted"));
	} else {
		gpm_info_event_log (info, GPM_EVENT_ON_BATTERY,
				    _("AC adapter removed"));
	}
}

/**
 * button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @brightness: This class instance
 **/
static void
button_pressed_cb (GpmButton   *power,
		   const gchar *type,
		   GpmInfo     *info)
{
	gpm_debug ("Button press event type=%s", type);

	if (strcmp (type, GPM_BUTTON_LID_CLOSED) == 0) {
		gpm_info_event_log (info,
				    GPM_EVENT_LID_CLOSED,
				    _("The laptop lid has been closed"));
		gpm_debug ("lid button CLOSED");

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {
		gpm_info_event_log (info,
				    GPM_EVENT_LID_OPENED,
				    _("The laptop lid has been re-opened"));
		gpm_debug ("lid button OPENED");
	}
}

/**
 * idle_changed_cb:
 * @idle: The idle class instance
 * @mode: The idle mode, e.g. GPM_IDLE_MODE_SESSION
 * @manager: This class instance
 *
 * This callback is called when gnome-screensaver detects that the idle state
 * has changed. GPM_IDLE_MODE_SESSION is when the session has become inactive,
 * and GPM_IDLE_MODE_SYSTEM is where the session has become inactive, AND the
 * session timeout has elapsed for the idle action.
 **/
static void
idle_changed_cb (GpmIdle     *idle,
		 GpmIdleMode  mode,
		 GpmInfo     *info)
{
	if (mode == GPM_IDLE_MODE_NORMAL) {
		gpm_info_event_log (info, GPM_EVENT_SESSION_ACTIVE, _("idle mode ended"));

	} else if (mode == GPM_IDLE_MODE_SESSION) {
		gpm_info_event_log (info, GPM_EVENT_SESSION_IDLE, _("idle mode started"));
	}
}

/**
 * dpms_mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @info: This class instance
 *
 * Log when the DPMS mode is changed.
 **/
static void
dpms_mode_changed_cb (GpmDpms    *dpms,
		      GpmDpmsMode mode,
		      GpmInfo *info)
{
	gpm_debug ("DPMS mode changed: %d", mode);

	if (mode == GPM_DPMS_MODE_ON) {
		gpm_info_event_log (info, GPM_EVENT_DPMS_ON, _("dpms on"));
	} else if (mode == GPM_DPMS_MODE_STANDBY) {
		gpm_info_event_log (info, GPM_EVENT_DPMS_STANDBY, _("dpms standby"));
	} else if (mode == GPM_DPMS_MODE_SUSPEND) {
		gpm_info_event_log (info, GPM_EVENT_DPMS_SUSPEND, _("dpms suspend"));
	} else if (mode == GPM_DPMS_MODE_OFF) {
		gpm_info_event_log (info, GPM_EVENT_DPMS_OFF, _("dpms off"));
	}
}

/**
 * control_resume_cb:
 * @control: The control class instance
 * @info: This class instance
 *
 * We have to update the caches on resume
 **/
static void
control_resume_cb (GpmControl *control,
		   GpmControlAction action,
		   GpmInfo    *info)
{
	gpm_info_explain_reason (info, GPM_EVENT_RESUME,
				_("Resuming computer"), NULL);
}

/**
 * control_sleep_failure_cb:
 * @control: The control class instance
 * @info: This class instance
 *
 * We have to log if suspend failed
 **/
static void
control_sleep_failure_cb (GpmControl  *control,
			  GpmControlAction action,
		          GpmInfo *info)
{
	if (action == GPM_CONTROL_ACTION_HIBERNATE) {
		gpm_info_event_log (info, GPM_EVENT_NOTIFICATION, _("Hibernate Problem"));
	} else {
		gpm_info_event_log (info, GPM_EVENT_NOTIFICATION, _("Suspend Problem"));
	}
}

/**
 * gpm_info_class_init:
 * @klass: This info class instance
 **/
static void
gpm_info_class_init (GpmInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_info_finalize;
	g_type_class_add_private (klass, sizeof (GpmInfoPrivate));
}

/**
 * gpm_info_init:
 * @info: This info class instance
 **/
static void
gpm_info_init (GpmInfo *info)
{
	info->priv = GPM_INFO_GET_PRIVATE (info);

	/* record our start time */
	info->priv->start_time = time (NULL);

	info->priv->hal = gpm_hal_new ();

	info->priv->control = gpm_control_new ();
	g_signal_connect (info->priv->control, "resume",
			  G_CALLBACK (control_resume_cb), info);
	g_signal_connect (info->priv->control, "sleep-failure",
			  G_CALLBACK (control_sleep_failure_cb), info);

	/* find out if we should log and display the extra graphs */
	info->priv->is_laptop = gpm_hal_is_laptop (info->priv->hal);

	/* singleton, so okay */
	info->priv->power = gpm_power_new ();

	/* we use ac_adapter so we can log the event */
	info->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (info->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), info);

	/* watch for lid open/close */
	info->priv->button = gpm_button_new ();
	g_signal_connect (info->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), info);

	/* watch for idle mode changes */
	info->priv->idle = gpm_idle_new ();
	g_signal_connect (info->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), info);

	/* watch for dpms mode changes */
	info->priv->dpms = gpm_dpms_new ();
	g_signal_connect (info->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), info);

	/* set to a blank list */
	info->priv->events = gpm_info_data_new ();
	if (info->priv->is_laptop == TRUE) {
		info->priv->percentage_data = gpm_info_data_new ();
		info->priv->rate_data = gpm_info_data_new ();
		info->priv->time_data = gpm_info_data_new ();
		info->priv->voltage_data = gpm_info_data_new ();

		/* set up the timer callback so we can log data */
		g_timeout_add (GPM_INFO_DATA_POLL * 1000, gpm_info_log_do_poll, info);

	} else {
		info->priv->percentage_data = NULL;
		info->priv->rate_data = NULL;
		info->priv->time_data = NULL;
		info->priv->voltage_data = NULL;
	}

	if (info->priv->is_laptop == TRUE) {
		/* get the maximum x-axis size from gconf */
		GpmConf *conf = gpm_conf_new ();
		guint max_time;
		gpm_conf_get_uint (conf, GPM_CONF_GRAPH_DATA_MAX_TIME, &max_time);
		g_object_unref (conf);

		gpm_info_data_set_max_time (info->priv->events, max_time);
		gpm_info_data_set_max_time (info->priv->percentage_data, max_time);
		gpm_info_data_set_max_time (info->priv->rate_data, max_time);
		gpm_info_data_set_max_time (info->priv->time_data, max_time);
		gpm_info_data_set_max_time (info->priv->voltage_data, max_time);
	}
}

/**
 * gpm_info_finalize:
 * @object: This info class instance
 **/
static void
gpm_info_finalize (GObject *object)
{
	GpmInfo *info;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_INFO (object));

	info = GPM_INFO (object);
	info->priv = GPM_INFO_GET_PRIVATE (info);

	if (info->priv->rate_data) {
		g_object_unref (info->priv->rate_data);
	}
	if (info->priv->percentage_data) {
		g_object_unref (info->priv->percentage_data);
	}
	if (info->priv->time_data) {
		g_object_unref (info->priv->time_data);
	}
	if (info->priv->voltage_data) {
		g_object_unref (info->priv->voltage_data);
	}
	if (info->priv->ac_adapter != NULL) {
		g_object_unref (info->priv->ac_adapter);
	}
	if (info->priv->idle != NULL) {
		g_object_unref (info->priv->idle);
	}
	if (info->priv->dpms != NULL) {
		g_object_unref (info->priv->dpms);
	}
	if (info->priv->control != NULL) {
		g_object_unref (info->priv->control);
	}
	g_object_unref (info->priv->events);
	g_object_unref (info->priv->power);
	g_object_unref (info->priv->hal);

	G_OBJECT_CLASS (gpm_info_parent_class)->finalize (object);
}

/**
 * gpm_info_new:
 * Return value: new GpmInfo instance.
 **/
GpmInfo *
gpm_info_new (void)
{
	GpmInfo *info;
	info = g_object_new (GPM_TYPE_INFO, NULL);
	return GPM_INFO (info);
}
