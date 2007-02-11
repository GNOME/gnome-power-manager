/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#include <glade/glade.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <math.h>
#include <string.h>

#include "gpm-tray-icon.h"
#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-conf.h"
#include "gpm-hal.h"
#include "gpm-cpufreq.h"
#include "gpm-prefs-core.h"
#include "gpm-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-screensaver.h"
#include "gpm-powermanager.h"

static void     gpm_prefs_class_init (GpmPrefsClass *klass);
static void     gpm_prefs_init       (GpmPrefs      *prefs);
static void     gpm_prefs_finalize   (GObject	    *object);

#define GPM_PREFS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PREFS, GpmPrefsPrivate))

struct GpmPrefsPrivate
{
	GladeXML		*glade_xml;
	gboolean		 has_batteries;
	gboolean		 has_lcd;
	gboolean		 has_ups;
	gboolean		 has_button_lid;
	gboolean		 has_button_suspend;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	GpmConf			*conf;
	GpmScreensaver		*screensaver;
	GpmCpuFreq		*cpufreq;
	GpmCpuFreqEnum		 cpufreq_types;
	GpmHal			*hal;
};

enum {
	ACTION_HELP,
	ACTION_CLOSE,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmPrefs, gpm_prefs, G_TYPE_OBJECT)

/* The text that should appear in the action combo boxes */
#define ACTION_INTERACTIVE_TEXT		_("Ask me")
#define ACTION_SUSPEND_TEXT		_("Suspend")
#define ACTION_SHUTDOWN_TEXT		_("Shutdown")
#define ACTION_HIBERNATE_TEXT		_("Hibernate")
#define ACTION_BLANK_TEXT		_("Blank screen")
#define ACTION_NOTHING_TEXT		_("Do nothing")

/* The text that should appear in the processor combo box */
#define CPUFREQ_NOTHING_TEXT		_("Do nothing")
#define CPUFREQ_ONDEMAND_TEXT		_("Based on processor load")
#define CPUFREQ_CONSERVATIVE_TEXT	_("Automatic power saving")
#define CPUFREQ_POWERSAVE_TEXT		_("Maximum power saving")
#define CPUFREQ_PERFORMANCE_TEXT	_("Always maximum speed")

/* If sleep time in a slider is set to 61 it is considered as never */
const int NEVER_TIME_ON_SLIDER = 61;

/**
 * gpm_prefs_class_init:
 * @klass: This prefs class instance
 **/
static void
gpm_prefs_class_init (GpmPrefsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_prefs_finalize;
	g_type_class_add_private (klass, sizeof (GpmPrefsPrivate));

	signals [ACTION_HELP] =
		g_signal_new ("action-help",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPrefsClass, action_help),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [ACTION_CLOSE] =
		g_signal_new ("action-close",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPrefsClass, action_close),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * gpm_prefs_activate_window:
 * @prefs: This prefs class instance
 *
 * Activates (shows) the window.
 **/
void
gpm_prefs_activate_window (GpmPrefs *prefs)
{
	GtkWidget *widget;
	widget = glade_xml_get_widget (prefs->priv->glade_xml, "window_preferences");
	gtk_window_present (GTK_WINDOW (widget));
}

/**
 * gpm_dbus_method_bool:
 * @method: The g-p-m DBUS method name, e.g. "AllowedSuspend"
 **/
static gboolean
gpm_dbus_method_bool (const gchar *method)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error;
	gboolean ret;
	gboolean value = FALSE;
	error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		if (error) {
			gpm_warning ("Couldn't connect to PowerManager %s",
				     error->message);
			g_error_free (error);
		}
		return FALSE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_PATH_CONTROL,
					   GPM_DBUS_INTERFACE_CONTROL);
	ret = dbus_g_proxy_call (proxy, method, &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &value,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("%s failed!", method);
		return FALSE;
	}
	g_object_unref (proxy);
	return value;
}

/**
 * gpm_prefs_help_cb:
 * @widget: The GtkWidget object
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_help_cb (GtkWidget *widget,
		   GpmPrefs  *prefs)
{
	gpm_debug ("emitting action-help");
	g_signal_emit (prefs, signals [ACTION_HELP], 0);
}

/**
 * gpm_prefs_icon_radio_cb:
 * @widget: The GtkWidget object
 **/
static void
gpm_prefs_icon_radio_cb (GtkWidget *widget,
			 GpmPrefs  *prefs)
{
	const gchar *str;
	gint policy;

	policy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "policy"));
	str = gpm_tray_icon_mode_to_string (policy);
	gpm_debug ("Changing %s to %s", GPM_CONF_ICON_POLICY, str);
	gpm_conf_set_string (prefs->priv->conf, GPM_CONF_ICON_POLICY, str);
}

/**
 * gpm_prefs_format_percentage_cb:
 * @scale: The GtkScale object
 * @value: The value in %.
 **/
static gchar *
gpm_prefs_format_percentage_cb (GtkScale *scale,
				gdouble   value)
{
	return g_strdup_printf ("%.0f%%", value);
}

/**
 * gpm_prefs_format_time_cb:
 * @scale: The GtkScale object
 * @value: The value in minutes.
 * @prefs: This prefs class instance
 **/
static gchar *
gpm_prefs_format_time_cb (GtkScale *scale,
			  gdouble   value,
			  GpmPrefs *prefs)
{
	gchar *str;
	if ((gint) value == NEVER_TIME_ON_SLIDER) {
		str = g_strdup (_("Never"));
	} else {
		str = gpm_get_timestring (value * 60);
	}
	return str;
}

/**
 * gpm_prefs_sleep_slider_changed_cb:
 * @range: The GtkRange object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_prefs_sleep_slider_changed_cb (GtkRange *range,
				   GpmPrefs *prefs)
{
	int value;
	int gs_idle_time;
	char *gpm_pref_key;

	value = (int) gtk_range_get_value (range);

	if (value == NEVER_TIME_ON_SLIDER) {
		/* power manager interprets 0 as Never */
		value = 0;
	} else {
		/* We take away the g-s idle time as the slider represents
		 * global time but we only do our timeout from when g-s
		 * declares the session idle */
		gs_idle_time = gpm_screensaver_get_delay (prefs->priv->screensaver);
		value -= gs_idle_time;

		/* policy is in seconds, slider is in minutes */
		value *= 60;
	}

	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (range), "conf_key");
	gpm_debug ("Changing %s to %i", gpm_pref_key, value);
	gpm_conf_set_int (prefs->priv->conf, gpm_pref_key, value);
}

/**
 * gpm_prefs_setup_sleep_slider:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static GtkWidget *
gpm_prefs_setup_sleep_slider (GpmPrefs    *prefs,
			      const gchar *widget_name,
			      const gchar *gpm_pref_key)
{
	GtkWidget *widget;
	gint value;
	gboolean is_writable;
	guint gs_idle_time;

	widget = glade_xml_get_widget (prefs->priv->glade_xml, widget_name);
	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_time_cb), prefs);

	gpm_conf_get_int (prefs->priv->conf, gpm_pref_key, &value);
	gpm_conf_is_writable (prefs->priv->conf, gpm_pref_key, &is_writable);

	gtk_widget_set_sensitive (widget, is_writable);

	if (value == 0) {
		value = NEVER_TIME_ON_SLIDER;
	} else {
		/* policy is in seconds, slider is in minutes */
		value /= 60;
		gs_idle_time = gpm_screensaver_get_delay (prefs->priv->screensaver);
		value += gs_idle_time;
	}

	gtk_range_set_value (GTK_RANGE (widget), value);

	g_object_set_data (G_OBJECT (widget), "conf_key", (gpointer) gpm_pref_key);

	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (gpm_prefs_sleep_slider_changed_cb),
			  prefs);

	return widget;
}

/**
 * gpm_prefs_brightness_slider_changed_cb:
 * @range: The GtkRange object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_prefs_brightness_slider_changed_cb (GtkRange *range,
					GpmPrefs *prefs)
{
	gdouble value;
	gchar *gpm_pref_key;

	value = gtk_range_get_value (range);
	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (range), "conf_key");

	g_object_set_data (G_OBJECT (range), "conf_key", (gpointer) gpm_pref_key);
	gpm_debug ("Changing %s to %i", gpm_pref_key, (int) value);
	gpm_conf_set_int (prefs->priv->conf, gpm_pref_key, (gint) value);
}

/**
 * gpm_prefs_setup_brightness_slider:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static GtkWidget *
gpm_prefs_setup_brightness_slider (GpmPrefs    *prefs,
				   const gchar *widget_name,
				   const gchar *gpm_pref_key)
{
	GladeXML    *xml = prefs->priv->glade_xml;
	GtkWidget *widget;
	int value;
	gboolean is_writable;

	widget = glade_xml_get_widget (xml, widget_name);

	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_percentage_cb), NULL);

	gpm_conf_get_int (prefs->priv->conf, gpm_pref_key, &value);
	gpm_conf_is_writable (prefs->priv->conf, gpm_pref_key, &is_writable);

	gtk_widget_set_sensitive (widget, is_writable);

	gtk_range_set_value (GTK_RANGE (widget), value);

	g_object_set_data (G_OBJECT (widget), "conf_key", (gpointer) gpm_pref_key);

	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (gpm_prefs_brightness_slider_changed_cb),
			  prefs);
	return widget;
}

/**
 * gpm_prefs_action_combo_changed_cb:
 * @widget: The GtkWidget object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_prefs_action_combo_changed_cb (GtkWidget *widget,
				   GpmPrefs  *prefs)
{
	gchar *value;
	const gchar *action;
	gchar *gpm_pref_key;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));

	if (strcmp (value, ACTION_SUSPEND_TEXT) == 0) {
		action = ACTION_SUSPEND;
	} else if (strcmp (value, ACTION_HIBERNATE_TEXT) == 0) {
		action = ACTION_HIBERNATE;
	} else if (strcmp (value, ACTION_SHUTDOWN_TEXT) == 0) {
		action = ACTION_SHUTDOWN;
	} else if (strcmp (value, ACTION_BLANK_TEXT) == 0) {
		action = ACTION_BLANK;
	} else if (strcmp (value, ACTION_NOTHING_TEXT) == 0) {
		action = ACTION_NOTHING;
	} else if (strcmp (value, ACTION_INTERACTIVE_TEXT) == 0) {
		action = ACTION_INTERACTIVE;
	} else {
		g_assert (FALSE);
	}

	g_free (value);
	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (widget), "conf_key");
	gpm_debug ("Changing %s to %s", gpm_pref_key, action);
	gpm_conf_set_string (prefs->priv->conf, gpm_pref_key, action);
}

/**
 * gpm_prefs_setup_action_combo:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The GConf key for this preference setting.
 * @actions: The actions to associate in an array.
 **/
static void
gpm_prefs_setup_action_combo (GpmPrefs     *prefs,
			      const gchar  *widget_name,
			      const gchar  *gpm_pref_key,
			      const gchar **actions)
{
	GladeXML    *xml = prefs->priv->glade_xml;
	gchar *value;
	gint i = 0;
	gint n_added = 0;
	gboolean is_writable;
	GtkWidget *widget;

	widget = glade_xml_get_widget (xml, widget_name);

	gpm_conf_get_string (prefs->priv->conf, gpm_pref_key, &value);
	gpm_conf_is_writable (prefs->priv->conf, gpm_pref_key, &is_writable);

	gtk_widget_set_sensitive (widget, is_writable);

	g_object_set_data (G_OBJECT (widget), "conf_key", (gpointer) gpm_pref_key);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_action_combo_changed_cb), prefs);

	if (value == NULL) {
		gpm_warning ("invalid schema, please re-install");
		value = g_strdup ("nothing");
	}

	while (actions[i] != NULL) {
		if (strcmp (actions[i], ACTION_SHUTDOWN) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_SHUTDOWN_TEXT);
			n_added++;
		} else if ((strcmp (actions[i], ACTION_SUSPEND) == 0) && !prefs->priv->can_suspend) {
			gpm_debug ("Cannot add option, as cannot suspend.");
		} else if ((strcmp (actions[i], ACTION_HIBERNATE) == 0) && !prefs->priv->can_hibernate) {
			gpm_debug ("Cannot add option, as cannot hibernate.");
		} else if ((strcmp (actions[i], ACTION_SUSPEND) == 0) && prefs->priv->can_suspend) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_SUSPEND_TEXT);
			n_added++;
		} else if ((strcmp (actions[i], ACTION_HIBERNATE) == 0) && prefs->priv->can_hibernate) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_HIBERNATE_TEXT);
			n_added++;
		} else if (strcmp (actions[i], ACTION_BLANK) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_BLANK_TEXT);
			n_added++;
		} else if (strcmp (actions[i], ACTION_INTERACTIVE) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_INTERACTIVE_TEXT);
			n_added++;
		} else if (strcmp (actions[i], ACTION_NOTHING) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_NOTHING_TEXT);
			n_added++;
		} else {
			gpm_critical_error ("Unknown action read from conf: %s",
					    actions[i]);
		}

		if (strcmp (value, actions[i]) == 0)
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added - 1);
		i++;
	}

	g_free (value);
}

/**
 * gpm_prefs_checkbox_lock_cb:
 * @widget: The GtkWidget object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_prefs_checkbox_lock_cb (GtkWidget *widget,
			    GpmPrefs  *prefs)
{
	gboolean checked;
	gchar *gpm_pref_key;
	const gchar *widget_name;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget_name = gtk_widget_get_name (widget);
	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (widget), "conf_key");
	gpm_debug ("Changing %s to %i", gpm_pref_key, checked);
	gpm_conf_set_bool (prefs->priv->conf, gpm_pref_key, checked);
}

/**
 * gpm_prefs_setup_checkbox:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static GtkWidget *
gpm_prefs_setup_checkbox (GpmPrefs    *prefs,
			  const gchar *widget_name,
			  const gchar *gpm_pref_key)
{

	GladeXML    *xml = prefs->priv->glade_xml;
	gboolean checked;
	GtkWidget *widget;

	gpm_debug ("Setting up %s", gpm_pref_key);

	widget = glade_xml_get_widget (xml, widget_name);

	gpm_conf_get_bool (prefs->priv->conf, gpm_pref_key, &checked);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);

	g_object_set_data (G_OBJECT (widget), "conf_key", (gpointer) gpm_pref_key);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_checkbox_lock_cb), prefs);

	/* manually do the callback in case we hide elements in the cb */
	gpm_prefs_checkbox_lock_cb (widget, prefs);

	return widget;
}

/**
 * gpm_prefs_close_cb:
 * @widget: The GtkWidget object
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_close_cb (GtkWidget	*widget,
		    GpmPrefs	*prefs)
{
	gpm_debug ("emitting action-close");
	g_signal_emit (prefs, signals [ACTION_CLOSE], 0);
}

/**
 * gpm_prefs_delete_event_cb:
 * @widget: The GtkWidget object
 * @event: The event type, unused.
 * @prefs: This prefs class instance
 **/
static gboolean
gpm_prefs_delete_event_cb (GtkWidget *widget,
			  GdkEvent   *event,
			  GpmPrefs   *prefs)
{
	gpm_prefs_close_cb (widget, prefs);
	return FALSE;
}

/**
 * set_idle_hscale_stops:
 * @prefs: This prefs class instance
 * @widget_name: The widget name
 *
 * Here we make sure that the start of the hscale is set to the
 * gnome-screensaver idle time to avoid confusion.
 **/
static void
set_idle_hscale_stops (GpmPrefs    *prefs,
		       const gchar *widget_name,
		       gint         gs_idle_time)
{
	GtkWidget *widget;
	widget = glade_xml_get_widget (prefs->priv->glade_xml, widget_name);
	if (gs_idle_time + 1 > NEVER_TIME_ON_SLIDER) {
		gpm_warning ("gnome-screensaver timeout is really big. "
			     "Not sure what to do");
		return;
	}
	gtk_range_set_range (GTK_RANGE (widget), gs_idle_time + 1, NEVER_TIME_ON_SLIDER);
}

/**
 * gs_delay_changed_cb:
 * @key: The conf key
 * @prefs: This prefs class instance
 **/
static void
gs_delay_changed_cb (GpmScreensaver *screensaver,
		     gint	     delay,
		     GpmPrefs	    *prefs)
{
	/* update the start and stop points on the hscales */
	set_idle_hscale_stops (prefs, "hscale_battery_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_battery_display", delay);
	set_idle_hscale_stops (prefs, "hscale_ac_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_ac_display", delay);
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the conf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmPrefs    *prefs)
{
	gboolean  enabled;

	if (strcmp (key, GPM_CONF_AC_LOWPOWER) == 0) {
		gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_AC_LOWPOWER, &enabled);
		gpm_debug ("need to enable checkbox");

	} else if (strcmp (key, GPM_CONF_UPS_LOWPOWER) == 0) {
		gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_UPS_LOWPOWER, &enabled);
		gpm_debug ("need to enable checkbox");

	} else if (strcmp (key, GPM_CONF_BATTERY_LOWPOWER) == 0) {
		gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_BATTERY_LOWPOWER, &enabled);
		gpm_debug ("need to enable checkbox");
	}
}

/**
 * gpm_prefs_processor_combo_changed_cb:
 * @widget: The GtkWidget object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_prefs_processor_combo_changed_cb (GtkWidget *widget,
				      GpmPrefs  *prefs)
{
	gchar *value;
	const gchar *policy;
	gchar *gpm_pref_key;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (value == NULL) {
		gpm_warning ("active text failed");
		return;
	}
	if (strcmp (value, CPUFREQ_ONDEMAND_TEXT) == 0) {
		policy = CODE_CPUFREQ_ONDEMAND;
	} else if (strcmp (value, CPUFREQ_CONSERVATIVE_TEXT) == 0) {
		policy = CODE_CPUFREQ_CONSERVATIVE;
	} else if (strcmp (value, CPUFREQ_POWERSAVE_TEXT) == 0) {
		policy = CODE_CPUFREQ_POWERSAVE;
	} else if (strcmp (value, CPUFREQ_PERFORMANCE_TEXT) == 0) {
		policy = CODE_CPUFREQ_PERFORMANCE;
	} else if (strcmp (value, CPUFREQ_NOTHING_TEXT) == 0) {
		policy = CODE_CPUFREQ_NOTHING;
	} else {
		g_assert (FALSE);
	}

	g_free (value);
	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (widget), "conf_key");
	gpm_debug ("Changing %s to %s", gpm_pref_key, policy);
	gpm_conf_set_string (prefs->priv->conf, gpm_pref_key, policy);
}

/**
 * gpm_prefs_setup_action_combo:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The GConf key for this preference setting.
 * @actions: The actions to associate in an array.
 **/
static void
gpm_prefs_setup_processor_combo (GpmPrefs         *prefs,
				 const gchar      *widget_name,
				 const gchar      *gpm_pref_key,
				 GpmCpuFreqEnum cpufreq_types)
{
	gchar *value;
	guint n_added = 0;
	gboolean has_option = FALSE;
	gboolean is_writable;
	GtkWidget *widget;
	GpmCpuFreqEnum cpufreq_type;

	widget = glade_xml_get_widget (prefs->priv->glade_xml, widget_name);
	gpm_conf_get_string (prefs->priv->conf, gpm_pref_key, &value);
	gpm_conf_is_writable (prefs->priv->conf, gpm_pref_key, &is_writable);

	gtk_widget_set_sensitive (widget, is_writable);

	if (value == NULL) {
		gpm_warning ("invalid schema, please re-install");
		value = g_strdup ("nothing");
	}

	g_object_set_data (G_OBJECT (widget), "conf_key", (gpointer) gpm_pref_key);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_processor_combo_changed_cb),
			  prefs);

	cpufreq_type = gpm_cpufreq_string_to_enum (value);

	if (cpufreq_types & GPM_CPUFREQ_ONDEMAND) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   CPUFREQ_ONDEMAND_TEXT);
		if (cpufreq_type == GPM_CPUFREQ_ONDEMAND) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added);
			has_option = TRUE;
		}
		n_added++;
	}
	if (cpufreq_types & GPM_CPUFREQ_NOTHING) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   CPUFREQ_NOTHING_TEXT);
		if (cpufreq_type == GPM_CPUFREQ_ONDEMAND) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added);
			has_option = TRUE;
		}
		n_added++;
	}
	if (cpufreq_types & GPM_CPUFREQ_CONSERVATIVE) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   CPUFREQ_CONSERVATIVE_TEXT);
		if (cpufreq_type == GPM_CPUFREQ_CONSERVATIVE) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added);
			has_option = TRUE;
		}
		n_added++;
	}
	if (cpufreq_types & GPM_CPUFREQ_POWERSAVE) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   CPUFREQ_POWERSAVE_TEXT);
		if (cpufreq_type == GPM_CPUFREQ_POWERSAVE) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added);
			has_option = TRUE;
		}
		n_added++;
	}
	if (cpufreq_types & GPM_CPUFREQ_PERFORMANCE) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   CPUFREQ_PERFORMANCE_TEXT);
		if (cpufreq_type == GPM_CPUFREQ_PERFORMANCE) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added);
			has_option = TRUE;
		}
		n_added++;
	}

	if (has_option == FALSE || cpufreq_type == GPM_CPUFREQ_NOTHING) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added);
	}
	g_free (value);
}

/** setup the notification page */
static void
prefs_setup_notification (GpmPrefs *prefs)
{
	gchar	    *icon_policy_str;
	gint	     icon_policy;
	GtkWidget   *radiobutton_icon_always;
	GtkWidget   *radiobutton_icon_present;
	GtkWidget   *radiobutton_icon_charge;
	GtkWidget   *radiobutton_icon_critical;
	GtkWidget   *radiobutton_icon_never;
	gboolean     is_writable;

	gpm_conf_get_string (prefs->priv->conf, GPM_CONF_ICON_POLICY, &icon_policy_str);
	icon_policy = gpm_tray_icon_mode_from_string (icon_policy_str);
	g_free (icon_policy_str);

	radiobutton_icon_always = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_notification_always");
	radiobutton_icon_present = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_notification_present");
	radiobutton_icon_charge = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_notification_charge");
	radiobutton_icon_critical = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_notification_critical");
	radiobutton_icon_never = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_notification_never");

	gpm_conf_is_writable (prefs->priv->conf, GPM_CONF_ICON_POLICY, &is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_always, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_present, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_charge, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_critical, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_never, is_writable);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_always),
				      icon_policy == GPM_ICON_POLICY_ALWAYS);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_present),
				      icon_policy == GPM_ICON_POLICY_PRESENT);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_charge),
				      icon_policy == GPM_ICON_POLICY_CHARGE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_critical),
				      icon_policy == GPM_ICON_POLICY_CRITICAL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_never),
				      icon_policy == GPM_ICON_POLICY_NEVER);

	g_object_set_data (G_OBJECT (radiobutton_icon_always), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_ALWAYS));
	g_object_set_data (G_OBJECT (radiobutton_icon_present), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_PRESENT));
	g_object_set_data (G_OBJECT (radiobutton_icon_charge), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_CHARGE));
	g_object_set_data (G_OBJECT (radiobutton_icon_critical), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_CRITICAL));
	g_object_set_data (G_OBJECT (radiobutton_icon_never), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_NEVER));

	/* only connect the callbacks after we set the value, else the conf
	   keys gets written to (for a split second), and the icon flickers. */
	g_signal_connect (radiobutton_icon_always, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_present, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_charge, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_critical, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_never, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);

	/* set up the sound checkbox */
	gpm_prefs_setup_checkbox (prefs, "checkbutton_notification_sound",
	  			  GPM_CONF_ENABLE_BEEPING);

	if (prefs->priv->has_batteries == TRUE) {
		/* there's no use case for displaying this option */
		gtk_widget_hide_all (radiobutton_icon_never);
	}
	if (prefs->priv->has_batteries == FALSE) {
		/* Hide battery radio options if we have no batteries */
		gtk_widget_hide_all (radiobutton_icon_charge);
		gtk_widget_hide_all (radiobutton_icon_critical);
	}
	if (prefs->priv->has_batteries == FALSE && prefs->priv->has_ups == FALSE) {
		/* Hide battery present option if no ups or primary */
		gtk_widget_hide_all (radiobutton_icon_present);
	}
}

static void
prefs_setup_ac (GpmPrefs *prefs)
{
	GtkWidget *widget;
	gint delay;
	gboolean show_cpufreq;
	const gchar  *button_lid_actions[] =
				{ACTION_NOTHING,
				 ACTION_BLANK,
				 ACTION_SUSPEND,
				 ACTION_HIBERNATE,
				 NULL};

	gpm_prefs_setup_action_combo (prefs, "combobox_ac_lid",
				      GPM_CONF_AC_BUTTON_LID,
				      button_lid_actions);
	gpm_prefs_setup_processor_combo (prefs, "combobox_ac_cpu",
					 GPM_CONF_AC_CPUFREQ_POLICY, prefs->priv->cpufreq_types);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ac_computer",
				      GPM_CONF_AC_SLEEP_COMPUTER);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ac_display",
				      GPM_CONF_AC_SLEEP_DISPLAY);
	gpm_prefs_setup_brightness_slider (prefs, "hscale_ac_brightness",
					   GPM_CONF_AC_BRIGHTNESS);

	delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_ac_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_ac_display", delay);

	if (prefs->priv->has_button_lid == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_ac_lid");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->cpufreq == NULL) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_ac_cpu");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_lcd == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_ac_brightness");
		gtk_widget_hide_all (widget);
	}

	gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_SHOW_CPUFREQ_UI, &show_cpufreq);
	if (show_cpufreq == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_ac_cpu");
		gtk_widget_hide_all (widget);
	}
}

static void
prefs_setup_battery (GpmPrefs *prefs)
{
	GtkWidget *widget;
	GtkWidget *notebook;
	gint delay;
	gint page;
	gboolean show_cpufreq;
	
	const gchar  *button_lid_actions[] =
				{ACTION_NOTHING,
				 ACTION_BLANK,
				 ACTION_SUSPEND,
				 ACTION_HIBERNATE,
				 NULL};
	const gchar  *battery_critical_actions[] =
				{ACTION_NOTHING,
				 ACTION_SUSPEND,
				 ACTION_HIBERNATE,
				 ACTION_SHUTDOWN,
				 NULL};

	if (prefs->priv->has_batteries == FALSE) {
		notebook = glade_xml_get_widget (prefs->priv->glade_xml, "notebook_preferences");
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_battery");
		page = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (widget));
		gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page);
		return;
	}

	gpm_prefs_setup_action_combo (prefs, "combobox_battery_lid",
				      GPM_CONF_BATTERY_BUTTON_LID,
				      button_lid_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_battery_critical",
				      GPM_CONF_BATTERY_CRITICAL,
				      battery_critical_actions);
	gpm_prefs_setup_processor_combo (prefs, "combobox_battery_cpu",
					 GPM_CONF_BATTERY_CPUFREQ_POLICY, prefs->priv->cpufreq_types);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_battery_computer",
				      GPM_CONF_BATTERY_SLEEP_COMPUTER);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_battery_display",
				      GPM_CONF_BATTERY_SLEEP_DISPLAY);
	gpm_prefs_setup_brightness_slider (prefs, "hscale_battery_brightness",
					   GPM_CONF_BATTERY_BRIGHTNESS);

	delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_battery_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_battery_display", delay);

	if (prefs->priv->has_button_lid == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_battery_lid");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->cpufreq == NULL) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_battery_cpu");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_lcd == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_battery_brightness");
		gtk_widget_hide_all (widget);
	}
	gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_SHOW_CPUFREQ_UI, &show_cpufreq);
	if (show_cpufreq == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_battery_cpu");
		gtk_widget_hide_all (widget);
	}
}

static void
prefs_setup_ups (GpmPrefs *prefs)
{
	GtkWidget *widget;
	GtkWidget *notebook;
	gint delay;
	gint page;

	const gchar  *ups_low_actions[] =
				{ACTION_NOTHING,
				 ACTION_HIBERNATE,
				 ACTION_SHUTDOWN,
				 NULL};

	if (prefs->priv->has_ups == FALSE) {
		notebook = glade_xml_get_widget (prefs->priv->glade_xml, "notebook_preferences");
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_ups");
		page = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (widget));
		gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page);
		return;
	}

	gpm_prefs_setup_action_combo (prefs, "combobox_ups_low",
				      GPM_CONF_UPS_LOW,
				      ups_low_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_ups_critical",
				      GPM_CONF_UPS_CRITICAL,
				      ups_low_actions);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ups_computer",
				      GPM_CONF_BATTERY_SLEEP_COMPUTER);
	delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_ups_computer", delay);
}

static void
prefs_setup_general (GpmPrefs *prefs)
{
	GtkWidget *widget;
	const gchar  *power_button_actions[] =
				{ACTION_INTERACTIVE,
				 ACTION_SUSPEND,
				 ACTION_HIBERNATE,
				 ACTION_SHUTDOWN,
				 NULL};
	const gchar  *suspend_button_actions[] =
				{ACTION_NOTHING,
				 ACTION_SUSPEND,
				 ACTION_HIBERNATE,
				 NULL};

	gpm_prefs_setup_action_combo (prefs, "combobox_general_power",
				      GPM_CONF_BUTTON_POWER,
				      power_button_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_general_suspend",
				      GPM_CONF_BUTTON_SUSPEND,
				      suspend_button_actions);
	gpm_prefs_setup_checkbox (prefs, "checkbutton_general_ambient",
				  GPM_CONF_DISPLAY_STATE_CHANGE);
	/* for now, hide */
	widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_general_ambient");
	gtk_widget_hide_all (widget);

	if (prefs->priv->has_button_suspend == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_general_suspend");
		gtk_widget_hide_all (widget);
	}
}

/**
 * gpm_prefs_init:
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_init (GpmPrefs *prefs)
{
	GtkWidget    *main_window;
	GtkWidget    *widget;

	prefs->priv = GPM_PREFS_GET_PRIVATE (prefs);

	prefs->priv->hal = gpm_hal_new ();
	prefs->priv->cpufreq = gpm_cpufreq_new ();

	prefs->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (prefs->priv->screensaver, "gs-delay-changed",
			  G_CALLBACK (gs_delay_changed_cb), prefs);

	prefs->priv->conf = gpm_conf_new ();
	g_signal_connect (prefs->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), prefs);

	prefs->priv->has_lcd = gpm_hal_num_devices_of_capability (prefs->priv->hal, "laptop_panel") > 0;
	prefs->priv->has_batteries = gpm_hal_num_devices_of_capability_with_value (prefs->priv->hal, "battery",
							"battery.type",
							"primary") > 0;
	prefs->priv->has_ups = gpm_hal_num_devices_of_capability_with_value (prefs->priv->hal, "battery",
							"battery.type",
							"ups") > 0;
	prefs->priv->has_button_lid = gpm_hal_num_devices_of_capability_with_value (prefs->priv->hal, "button",
							"button.type",
							"lid") > 0;
	prefs->priv->has_button_suspend = gpm_hal_num_devices_of_capability_with_value (prefs->priv->hal, "button",
							"button.type",
							"suspend") > 0;
	prefs->priv->can_suspend = gpm_dbus_method_bool ("AllowedSuspend");
	prefs->priv->can_hibernate = gpm_dbus_method_bool ("AllowedHibernate");

	/* only enable cpufreq stuff if we have the hardware */
	if (prefs->priv->cpufreq) {
		gpm_cpufreq_get_governors (prefs->priv->cpufreq,
					       &prefs->priv->cpufreq_types);
	} else {
		prefs->priv->cpufreq_types = GPM_CPUFREQ_NOTHING;
	}

	prefs->priv->glade_xml = glade_xml_new (GPM_DATA "/gpm-prefs.glade", NULL, NULL);
	if (prefs->priv->glade_xml == NULL) {
		g_error ("Cannot find 'gpm-prefs.glade'");
	}

	main_window = glade_xml_get_widget (prefs->priv->glade_xml, "window_preferences");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW(main_window), GPM_STOCK_APP_ICON);

	/* Get the main window quit */
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gpm_prefs_delete_event_cb), prefs);

	widget = glade_xml_get_widget (prefs->priv->glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_close_cb), prefs);

	widget = glade_xml_get_widget (prefs->priv->glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_help_cb), prefs);

	prefs_setup_ac (prefs);
	prefs_setup_battery (prefs);
	prefs_setup_ups (prefs);
	prefs_setup_general (prefs);
	prefs_setup_notification (prefs);

	gtk_widget_show (main_window);
}

/**
 * gpm_prefs_finalize:
 * @object: This prefs class instance
 **/
static void
gpm_prefs_finalize (GObject *object)
{
	GpmPrefs *prefs;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_PREFS (object));

	prefs = GPM_PREFS (object);
	prefs->priv = GPM_PREFS_GET_PRIVATE (prefs);

	g_object_unref (prefs->priv->conf);
	if (prefs->priv->screensaver) {
		g_object_unref (prefs->priv->screensaver);
	}
	if (prefs->priv->cpufreq) {
		g_object_unref (prefs->priv->cpufreq);
	}
	if (prefs->priv->hal) {
		g_object_unref (prefs->priv->hal);
	}

	G_OBJECT_CLASS (gpm_prefs_parent_class)->finalize (object);
}

/**
 * gpm_prefs_new:
 * Return value: new GpmPrefs instance.
 **/
GpmPrefs *
gpm_prefs_new (void)
{
	GpmPrefs *prefs;
	prefs = g_object_new (GPM_TYPE_PREFS, NULL);
	return GPM_PREFS (prefs);
}
