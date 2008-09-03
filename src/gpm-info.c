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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <string.h>
#include <dbus/dbus-gtype-specialized.h>

#include <libhal-gmanager.h>

#include "egg-color.h"
#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-dpms.h"
#include "gpm-info.h"
#include "gpm-profile.h"
#include "gpm-array.h"
#include "gpm-engine.h"
#include "gpm-stock-icons.h"
#include "gpm-idle.h"
#include "gpm-graph-widget.h"

static void     gpm_info_class_init (GpmInfoClass *klass);
static void     gpm_info_init       (GpmInfo      *info);
static void     gpm_info_finalize   (GObject      *object);

#define GPM_INFO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO, GpmInfoPrivate))

#define GPM_INFO_DATA_POLL		5	/* seconds */
#define EGG_COLOR_CHARGING			EGG_COLOR_BLUE
#define EGG_COLOR_DISCHARGING			EGG_COLOR_DARK_RED
#define EGG_COLOR_CHARGED			EGG_COLOR_GREEN

#define GPM_DBUS_STRUCT_INT_INT (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))
#define GPM_DBUS_STRUCT_INT_INT_INT (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))
#define GPM_DBUS_STRUCT_INT_STRING (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_INT, G_TYPE_STRING, G_TYPE_INVALID))
#define GPM_DBUS_STRUCT_INT_INT_INT_STRING (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INVALID))

struct GpmInfoPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmButton		*button;
	GpmControl		*control;
	GpmDpms			*dpms;
	GpmIdle			*idle;
	GpmProfile		*profile;
	GpmEngineCollection	*collection;

	GpmArray		*events;
	GpmArray		*rate_data;
	GpmArray		*time_data;
	GpmArray		*percentage_data;
	GpmArray		*voltage_data;

	GTimer			*timer;
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
		gchar *reason;
		reason = g_strdup_printf (_("Reason: %s"), post);
		message = g_strconcat (pre, " ", reason, NULL);
		g_free (reason);
	} else {
		message = g_strdup (pre);
	}
	egg_debug ("%s", message);
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
 * gpm_statistics_add_data_type:
 **/
static void
gpm_statistics_add_data_type (GPtrArray *array,
			      guint colour,
			      const gchar *description)
{
	GValue *value;

	value = g_new0 (GValue, 1);
	g_value_init (value, GPM_DBUS_STRUCT_INT_STRING);
	g_value_take_boxed (value, dbus_g_type_specialized_construct (GPM_DBUS_STRUCT_INT_STRING));
	dbus_g_type_struct_set (value, 0, colour, 1, description, -1);
	g_ptr_array_add (array, g_value_get_boxed (value));
	g_free (value);
}

/**
 * gpm_statistics_add_event_type:
 **/
static void
gpm_statistics_add_event_type (GPtrArray *array,
			       guint id,
			       guint colour,
			       guint shape,
			       const gchar *description)
{
	GValue *value;

	value = g_new0 (GValue, 1);
	g_value_init (value, GPM_DBUS_STRUCT_INT_INT_INT_STRING);
	g_value_take_boxed (value, dbus_g_type_specialized_construct (GPM_DBUS_STRUCT_INT_INT_INT_STRING));
	dbus_g_type_struct_set (value, 0, id, 1, colour, 2, shape, 3, description, -1);
	g_ptr_array_add (array, g_value_get_boxed (value));
	g_free (value);
}

/**
 * gpm_statistics_add_event_type:
 **/
static void
gpm_statistics_add_events_typical (GPtrArray *array)
{
	/* add the general key items, TODO specify which ones make sense */
	gpm_statistics_add_event_type (array, GPM_EVENT_ON_AC,
				       EGG_COLOR_BLUE,
				       GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
				       _("On AC"));
	gpm_statistics_add_event_type (array, GPM_EVENT_ON_BATTERY,
				       EGG_COLOR_RED,
				       GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
				       _("On battery"));
	gpm_statistics_add_event_type (array, GPM_EVENT_SESSION_POWERSAVE,
				       EGG_COLOR_WHITE,
				       GPM_GRAPH_WIDGET_SHAPE_SQUARE,
				       _("Session powersave"));
	gpm_statistics_add_event_type (array, GPM_EVENT_SESSION_IDLE,
				       EGG_COLOR_YELLOW,
				       GPM_GRAPH_WIDGET_SHAPE_SQUARE,
				       _("Session idle"));
	gpm_statistics_add_event_type (array, GPM_EVENT_SESSION_ACTIVE,
				       EGG_COLOR_DARK_YELLOW,
				       GPM_GRAPH_WIDGET_SHAPE_SQUARE,
				       _("Session active"));
	gpm_statistics_add_event_type (array, GPM_EVENT_SUSPEND,
				       EGG_COLOR_RED,
				       GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
				/* Translators: translate ONLY the string part after the |*/
				       Q_("label shown on graph|Suspend"));
	gpm_statistics_add_event_type (array, GPM_EVENT_RESUME,
				       EGG_COLOR_DARK_RED,
				       GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
				/* Translators: translate ONLY the string part after the |*/
				       Q_("label shown on graph|Resume"));
	gpm_statistics_add_event_type (array, GPM_EVENT_HIBERNATE,
				       EGG_COLOR_MAGENTA,
				       GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
				/* Translators: translate ONLY the string part after the |*/
				       Q_("label shown on graph|Hibernate"));
	gpm_statistics_add_event_type (array, GPM_EVENT_LID_CLOSED,
				       EGG_COLOR_GREEN,
				       GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
				       _("Lid closed"));
	gpm_statistics_add_event_type (array, GPM_EVENT_LID_OPENED,
				       EGG_COLOR_DARK_GREEN,
				       GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
				       _("Lid opened"));
	gpm_statistics_add_event_type (array, GPM_EVENT_NOTIFICATION,
				       EGG_COLOR_GREY,
				       GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
				       _("Notification"));
#ifdef HAVE_DPMS_EXTENSION
	gpm_statistics_add_event_type (array, GPM_EVENT_DPMS_ON,
				       EGG_COLOR_CYAN,
				       GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
				       _("DPMS On"));
	gpm_statistics_add_event_type (array, GPM_EVENT_DPMS_STANDBY,
				       EGG_COLOR_CYAN,
				       GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
				       _("DPMS Standby"));
	gpm_statistics_add_event_type (array, GPM_EVENT_DPMS_SUSPEND,
				       EGG_COLOR_CYAN,
				       GPM_GRAPH_WIDGET_SHAPE_SQUARE,
				       _("DPMS Suspend"));
	gpm_statistics_add_event_type (array, GPM_EVENT_DPMS_OFF,
				       EGG_COLOR_CYAN,
				       GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
				       _("DPMS Off"));
#endif
}

/**
 * gpm_statistics_get_parameters:
 **/
gboolean
gpm_statistics_get_parameters (GpmInfo   *info,
			       gchar	 *type,
			       gchar	 **axis_type_x,
			       gchar	 **axis_type_y,
			       gchar	 **axis_desc_x,
			       gchar	 **axis_desc_y,
			       GPtrArray **key_data,
			       GPtrArray **key_event,
			       GError	 **error)
{
	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	g_return_val_if_fail (axis_type_x != NULL, FALSE);
	g_return_val_if_fail (axis_type_y != NULL, FALSE);
	g_return_val_if_fail (axis_desc_x != NULL, FALSE);
	g_return_val_if_fail (axis_desc_y != NULL, FALSE);
	g_return_val_if_fail (key_data != NULL, FALSE);
	g_return_val_if_fail (key_event != NULL, FALSE);

	*key_data = g_ptr_array_new ();
	*key_event = g_ptr_array_new ();

	if (strcmp (type, "power") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("power");
		*axis_desc_x = g_strdup (_("Time since startup"));
		*axis_desc_y = g_strdup (_("Power"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_GREEN, _("Power"));
		gpm_statistics_add_events_typical (*key_event);
		return TRUE;
	}
	if (strcmp (type, "time") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("time");
		*axis_desc_x = g_strdup (_("Time since startup"));
		*axis_desc_y = g_strdup (_("Estimated time"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_GREEN, _("Time"));
		gpm_statistics_add_events_typical (*key_event);
		return TRUE;
	}
	if (strcmp (type, "charge") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("percentage");
		*axis_desc_x = g_strdup (_("Time since startup"));
		*axis_desc_y = g_strdup (_("Battery percentage"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_GREEN, _("Percentage"));
		gpm_statistics_add_events_typical (*key_event);
		return TRUE;
	}
	if (strcmp (type, "voltage") == 0) {
		*axis_type_x = g_strdup ("time");
		*axis_type_y = g_strdup ("voltage");
		*axis_desc_x = g_strdup (_("Time since startup"));
		*axis_desc_y = g_strdup (_("Battery Voltage"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_GREEN, _("Voltage"));
		gpm_statistics_add_events_typical (*key_event);
		return TRUE;
	}
	if (strcmp (type, "profile-charge-accuracy") == 0) {
		*axis_type_x = g_strdup ("percentage");
		*axis_type_y = g_strdup ("percentage");
		*axis_desc_x = g_strdup (_("Battery percentage"));
		*axis_desc_y = g_strdup (_("Accuracy of reading"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_RED, _("Trusted"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_DARK_RED, _("Untrusted"));
		return TRUE;
	}
	if (strcmp (type, "profile-discharge-accuracy") == 0) {
		*axis_type_x = g_strdup ("percentage");
		*axis_type_y = g_strdup ("percentage");
		*axis_desc_x = g_strdup (_("Battery percentage"));
		*axis_desc_y = g_strdup (_("Accuracy of reading"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_BLUE, _("Trusted"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_DARK_BLUE, _("Untrusted"));
		return TRUE;
	}
	if (strcmp (type, "profile-charge-time") == 0) {
		*axis_type_x = g_strdup ("percentage");
		*axis_type_y = g_strdup ("time");
		*axis_desc_x = g_strdup (_("Battery percentage"));
		*axis_desc_y = g_strdup (_("Average time elapsed"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_RED, _("Valid data"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_DARK_GREY, _("Extrapolated data"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_BLUE, _("No data"));
		gpm_statistics_add_event_type (*key_event, GPM_EVENT_HIBERNATE,
					       EGG_COLOR_CYAN,
					       GPM_GRAPH_WIDGET_SHAPE_SQUARE,
				 	      _("Start point"));
		gpm_statistics_add_event_type (*key_event, GPM_EVENT_SUSPEND,
					       EGG_COLOR_YELLOW,
					       GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
				 	      _("Stop point"));
		return TRUE;
	}
	if (strcmp (type, "profile-discharge-time") == 0) {
		*axis_type_x = g_strdup ("percentage");
		*axis_type_y = g_strdup ("time");
		*axis_desc_x = g_strdup (_("Battery percentage"));
		*axis_desc_y = g_strdup (_("Average time elapsed"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_RED, _("Valid data"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_DARK_GREY, _("Extrapolated data"));
		gpm_statistics_add_data_type (*key_data, EGG_COLOR_BLUE, _("No data"));
		gpm_statistics_add_event_type (*key_event, GPM_EVENT_HIBERNATE,
					       EGG_COLOR_CYAN,
					       GPM_GRAPH_WIDGET_SHAPE_SQUARE,
				 	      _("Start point"));
		gpm_statistics_add_event_type (*key_event, GPM_EVENT_SUSPEND,
					       EGG_COLOR_YELLOW,
					       GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
				 	      _("Stop point"));
		return TRUE;
	}

	/* not recognised... */
	*error = g_error_new (gpm_info_error_quark (),
			      GPM_INFO_ERROR_INVALID_TYPE,
			      "Invalid type %s", type);
	return FALSE;
}

/**
 * gpm_statistics_get_types:
 * @info: This class instance
 * @types: The return type STRV artay
 *
 * Return value: TRUE for success.
 **/
gboolean
gpm_statistics_get_data_types (GpmInfo  *info,
			       gchar  ***types,
			       GError  **error)
{
	GList *list = NULL;
	GpmArray *array;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);
	g_return_val_if_fail (types != NULL, FALSE);

	/* only return the type if we have sufficient data */
	array = info->priv->rate_data;
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "power");
	}
	array = info->priv->time_data;
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "time");
	}
	array = info->priv->percentage_data;
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "charge");
	}
	array = info->priv->voltage_data;
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "voltage");
	}
	array = gpm_profile_get_data_time_percent (info->priv->profile, FALSE);
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "profile-charge-time");
	}
	array = gpm_profile_get_data_time_percent (info->priv->profile, TRUE);
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "profile-discharge-time");
	}
	array = gpm_profile_get_data_accuracy_percent (info->priv->profile, FALSE);
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "profile-charge-accuracy");
	}
	array = gpm_profile_get_data_accuracy_percent (info->priv->profile, TRUE);
	if (array != NULL && gpm_array_get_size (array) > 2) {
		list = g_list_append (list, "profile-discharge-accuracy");
	}

	*types = device_list_to_strv (list);

	g_list_free (list);
	return TRUE;
}

/**
 * gpm_statistics_get_event_log:
 * @info: This class instance
 *
 * Return value: TRUE for success
 **/
gboolean
gpm_statistics_get_event_log (GpmInfo    *info,
			      GPtrArray **array,
			      GError    **error)
{
	GpmArrayPoint *point;
	GpmArray *events;
	gint i;
	GValue *value;
	guint length;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);

	events = info->priv->events;
	length = gpm_array_get_size (events);
	*array = g_ptr_array_sized_new (length);
	for (i=0; i < length; i++) {
		point = gpm_array_get (events, i);
		value = g_new0 (GValue, 1);
		g_value_init (value, GPM_DBUS_STRUCT_INT_INT);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (GPM_DBUS_STRUCT_INT_INT));
		dbus_g_type_struct_set (value, 0, point->x, 1, point->y, -1);
		g_ptr_array_add (*array, g_value_get_boxed (value));
		g_free (value);
	}

	return TRUE;
}

/**
 * gpm_statistics_get_data:
 * @info: This class instance
 * @type: The graph type, e.g. "charge", "power", "time", etc.
 *
 * Return value: TRUE for success
 **/
gboolean
gpm_statistics_get_data (GpmInfo     *info,
			 const gchar *type,
			 GPtrArray  **array,
			 GError	    **error)
{
	GValue *value;
	GpmArray *events;
	GpmArrayPoint *point;
	gint i;
	guint length;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);
	g_return_val_if_fail (type != NULL, FALSE);

	if (info->priv->is_laptop == FALSE) {
		egg_warning ("Data not available as not a laptop");
		*error = g_error_new (gpm_info_error_quark (),
				      GPM_INFO_ERROR_DATA_NOT_AVAILABLE,
				      "Data not available as not a laptop");
		return FALSE;
	}

	if (strcmp (type, "power") == 0) {
		events = info->priv->rate_data;
	} else if (strcmp (type, "time") == 0) {
		events = info->priv->time_data;
	} else if (strcmp (type, "charge") == 0) {
		events = info->priv->percentage_data;
	} else if (strcmp (type, "voltage") == 0) {
		events = info->priv->voltage_data;
	} else if (strcmp (type, "profile-charge-accuracy") == 0) {
		events = gpm_profile_get_data_accuracy_percent (info->priv->profile, FALSE);
	} else if (strcmp (type, "profile-charge-time") == 0) {
		events = gpm_profile_get_data_time_percent (info->priv->profile, FALSE);
	} else if (strcmp (type, "profile-discharge-accuracy") == 0) {
		events = gpm_profile_get_data_accuracy_percent (info->priv->profile, TRUE);
	} else if (strcmp (type, "profile-discharge-time") == 0) {
		events = gpm_profile_get_data_time_percent (info->priv->profile, TRUE);
	} else {
		egg_warning ("Data type %s not known!", type);
		*error = g_error_new (gpm_info_error_quark (),
				      GPM_INFO_ERROR_INVALID_TYPE,
				      "Data type %s not known!", type);
		return FALSE;
	}

	if (events == NULL) {
		egg_warning ("Data not available");
		*error = g_error_new (gpm_info_error_quark (),
				      GPM_INFO_ERROR_DATA_NOT_AVAILABLE,
				      "Data not available");
		return FALSE;
	}

	length = gpm_array_get_size (events);
	*array = g_ptr_array_sized_new (length);
	for (i=0; i < length; i++) {
		point = gpm_array_get (events, i);
		value = g_new0 (GValue, 1);
		g_value_init (value, GPM_DBUS_STRUCT_INT_INT_INT);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (GPM_DBUS_STRUCT_INT_INT_INT));
		dbus_g_type_struct_set (value, 0, point->x, 1, point->y, 2, point->data, -1);
		g_ptr_array_add (*array, g_value_get_boxed (value));
		g_free (value);
	}
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
	egg_debug ("Adding %i to the event log", event);

	gpm_array_append (info->priv->events,
			  g_timer_elapsed (info->priv->timer, NULL), event, 0);
}

/**
 * gpm_info_set_collection_data:
 **/
gboolean
gpm_info_set_collection_data (GpmInfo             *info,
			      GpmEngineCollection *collection)
{
	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO (info), FALSE);

	info->priv->collection = collection;
	return TRUE;
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
	GpmCellArray *array;
	GpmCellUnit *unit;

	int value_x;
	int colour;

	array = info->priv->collection->primary;
	unit = gpm_cell_array_get_unit (array);

	if (info->priv->is_laptop) {
		/* work out seconds elapsed */
		value_x = g_timer_elapsed (info->priv->timer, NULL) - GPM_INFO_DATA_POLL;

		/* set the correct colours */
		if (unit->is_discharging) {
			colour = EGG_COLOR_DISCHARGING;
		} else if (unit->is_charging) {
			colour = EGG_COLOR_CHARGING;
		} else {
			colour = EGG_COLOR_CHARGED;
		}

		if (unit->percentage > 0) {
			gpm_array_add (info->priv->percentage_data, value_x,
				       unit->percentage, colour);
		}
		if (unit->rate > 0) {
			gpm_array_add (info->priv->rate_data, value_x,
				       unit->rate, colour);
		}
		if (unit->voltage > 0) {
			gpm_array_add (info->priv->voltage_data, value_x,
				       unit->voltage, colour);
		}
	}
	return TRUE;
}

/**
 * ac_adapter_changed_cb:
 *
 * Does the actions when the ac source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean      on_ac,
		       GpmInfo      *info)
{
	if (on_ac) {
		gpm_info_event_log (info, GPM_EVENT_ON_AC,
				    _("AC adapter inserted"));
	} else {
		gpm_info_event_log (info, GPM_EVENT_ON_BATTERY,
				    _("AC adapter removed"));
	}
}

/**
 * button_pressed_cb:
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @brightness: This class instance
 **/
static void
button_pressed_cb (GpmButton   *button,
		   const gchar *type,
		   GpmInfo     *info)
{
	egg_debug ("Button press event type=%s", type);

	if (strcmp (type, GPM_BUTTON_LID_CLOSED) == 0) {
		gpm_info_event_log (info,
				    GPM_EVENT_LID_CLOSED,
				    _("The laptop lid has been closed"));
		egg_debug ("lid button CLOSED");

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {
		gpm_info_event_log (info,
				    GPM_EVENT_LID_OPENED,
				    _("The laptop lid has been re-opened"));
		egg_debug ("lid button OPENED");
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
	} else if (mode == GPM_IDLE_MODE_POWERSAVE) {
		gpm_info_event_log (info, GPM_EVENT_SESSION_POWERSAVE, _("powersave mode started"));
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
	egg_debug ("DPMS mode changed: %d", mode);

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
	HalGManager *hal_manager;
	info->priv = GPM_INFO_GET_PRIVATE (info);

	/* record our start time */
	info->priv->timer = g_timer_new ();

	info->priv->control = gpm_control_new ();
	g_signal_connect (info->priv->control, "resume",
			  G_CALLBACK (control_resume_cb), info);
	g_signal_connect (info->priv->control, "sleep-failure",
			  G_CALLBACK (control_sleep_failure_cb), info);

	/* find out if we should log and display the extra graphs */
	hal_manager = hal_gmanager_new ();
	info->priv->is_laptop = hal_gmanager_is_laptop (hal_manager);
	g_object_unref (hal_manager);

	/* set default, we have to set these from the manager */
	info->priv->profile = gpm_profile_new ();

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
	info->priv->events = gpm_array_new ();
	if (info->priv->is_laptop) {
		info->priv->percentage_data = gpm_array_new ();
		info->priv->rate_data = gpm_array_new ();
		info->priv->time_data = gpm_array_new ();
		info->priv->voltage_data = gpm_array_new ();

		/* set up the timer callback so we can log data */
		g_timeout_add (GPM_INFO_DATA_POLL * 1000, gpm_info_log_do_poll, info);

	} else {
		info->priv->percentage_data = NULL;
		info->priv->rate_data = NULL;
		info->priv->time_data = NULL;
		info->priv->voltage_data = NULL;
	}

	if (info->priv->is_laptop) {
		/* get the maximum x-axis size from gconf */
		GpmConf *conf = gpm_conf_new ();
		guint max_time;
		gpm_conf_get_uint (conf, GPM_CONF_STATS_MAX_TIME, &max_time);
		g_object_unref (conf);

		gpm_array_set_max_width (info->priv->events, max_time);
		gpm_array_set_max_width (info->priv->percentage_data, max_time);
		gpm_array_set_max_width (info->priv->rate_data, max_time);
		gpm_array_set_max_width (info->priv->time_data, max_time);
		gpm_array_set_max_width (info->priv->voltage_data, max_time);
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
	if (info->priv->dpms != NULL) {
		g_object_unref (info->priv->dpms);
	}
	if (info->priv->control != NULL) {
		g_object_unref (info->priv->control);
	}
	g_object_unref (info->priv->ac_adapter);
	g_object_unref (info->priv->idle);
	g_object_unref (info->priv->events);
	g_object_unref (info->priv->profile);

	g_timer_destroy (info->priv->timer);

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

