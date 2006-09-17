/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
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

#include <glib.h>
#include <glib/gi18n.h>

#include <glade/glade.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>
#include <math.h>
#include <string.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-hal.h"
#include "gpm-hal-cpufreq.h"
#include "gpm-prefs-core.h"
#include "gpm-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-screensaver.h"

static void     gpm_prefs_class_init (GpmPrefsClass *klass);
static void     gpm_prefs_init       (GpmPrefs      *prefs);
static void     gpm_prefs_finalize   (GObject	    *object);

#define GPM_PREFS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PREFS, GpmPrefsPrivate))

struct GpmPrefsPrivate
{
	GladeXML		*glade_xml;
	GConfClient		*gconf_client;
	gboolean		 has_batteries;
	gboolean		 has_lcd;
	gboolean		 has_ups;
	gboolean		 has_button_lid;
	gboolean		 has_cpufreq;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	GpmScreensaver		*screensaver;
	GpmHalCpuFreq		*cpufreq;
	GpmHalCpuFreqEnum	 cpufreq_types;
	GpmHal			*hal;
};

enum {
	ACTION_HELP,
	ACTION_CLOSE,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmPrefs, gpm_prefs, G_TYPE_OBJECT)

static GConfEnumStringPair icon_policy_enum_map [] = {
	{ GPM_ICON_POLICY_ALWAYS,	"always"   },
	{ GPM_ICON_POLICY_PRESENT,	"present"  },
	{ GPM_ICON_POLICY_CHARGE,	"charge"   },
	{ GPM_ICON_POLICY_CRITICAL,	"critical" },
	{ GPM_ICON_POLICY_NEVER,	"never"    },
	{ 0, NULL }
};

/* The text that should appear in the action combo boxes */
#define ACTION_INTERACTIVE_TEXT		_("Ask me")
#define ACTION_SUSPEND_TEXT		_("Suspend")
#define ACTION_SHUTDOWN_TEXT		_("Shutdown")
#define ACTION_HIBERNATE_TEXT		_("Hibernate")
#define ACTION_BLANK_TEXT		_("Blank screen")
#define ACTION_NOTHING_TEXT		_("Do nothing")

/* The text that should appear in the processor combo box */
#define CPUFREQ_ONDEMAND_TEXT		_("Based on processor load")
#define CPUFREQ_CONSERVATIVE_TEXT	_("Automatic power saving")
#define CPUFREQ_POWERSAVE_TEXT		_("Maximum power saving")
#define CPUFREQ_USERSPACE_TEXT		_("Custom")
#define CPUFREQ_PERFORMANCE_TEXT	_("Maximum speed")
#define CPUFREQ_NOTHING_TEXT 		_("Do nothing")

#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/PowerManager"
#define	GPM_DBUS_INTERFACE		"org.gnome.PowerManager"

/* If sleep time in a slider is set to 61 it is considered as never sleep */
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
	gboolean value;

	value = FALSE;
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
					   GPM_DBUS_PATH,
					   GPM_DBUS_INTERFACE);
	if (! dbus_g_proxy_call (proxy,
				 method,
				 &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &value,
				 G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("Couldn't connect to PowerManager %s",
				     error->message);
			g_error_free (error);
		}
		value = FALSE;
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
	str = gconf_enum_to_string (icon_policy_enum_map, policy);
	gpm_debug ("Changing %s to %s", GPM_PREF_ICON_POLICY, str);
	gconf_client_set_string (prefs->priv->gconf_client,
				 GPM_PREF_ICON_POLICY,
				 str, NULL);
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
		/* we auto-add the gss idle time to stop users getting
		   confused. */
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

	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (range), "gconf_key");
	gpm_debug ("Changing %s to %i", gpm_pref_key, value);
	gconf_client_set_int (prefs->priv->gconf_client, gpm_pref_key, value, NULL);
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

	widget = glade_xml_get_widget (prefs->priv->glade_xml, widget_name);
	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_time_cb), prefs);

	value = gconf_client_get_int (prefs->priv->gconf_client, gpm_pref_key, NULL);

	is_writable = gconf_client_key_is_writable (prefs->priv->gconf_client,
						    gpm_pref_key, NULL);

	gtk_widget_set_sensitive (widget, is_writable);

	if (value == 0) {
		value = NEVER_TIME_ON_SLIDER;
	} else {
		int gs_idle_time;
		/* policy is in seconds, slider is in minutes */
		value /= 60;
		gs_idle_time = gpm_screensaver_get_delay (prefs->priv->screensaver);
		value += gs_idle_time;
	}

	gtk_range_set_value (GTK_RANGE (widget), value);

	g_object_set_data (G_OBJECT (widget), "gconf_key", (gpointer) gpm_pref_key);

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
	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (range), "gconf_key");

	g_object_set_data (G_OBJECT (range), "gconf_key", (gpointer) gpm_pref_key);
	gpm_debug ("Changing %s to %i", gpm_pref_key, (int) value);
	gconf_client_set_int (prefs->priv->gconf_client, gpm_pref_key, (gint) value, NULL);
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

	value = gconf_client_get_int (prefs->priv->gconf_client, gpm_pref_key, NULL);
	is_writable = gconf_client_key_is_writable (prefs->priv->gconf_client,
						    gpm_pref_key, NULL);

	gtk_widget_set_sensitive (widget, is_writable);

	gtk_range_set_value (GTK_RANGE (widget), value);

	g_object_set_data (G_OBJECT (widget), "gconf_key", (gpointer) gpm_pref_key);

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
	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (widget), "gconf_key");
	gpm_debug ("Changing %s to %s", gpm_pref_key, action);
	gconf_client_set_string (prefs->priv->gconf_client, gpm_pref_key, action, NULL);
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

	value = gconf_client_get_string (prefs->priv->gconf_client,
					 gpm_pref_key, NULL);
	is_writable = gconf_client_key_is_writable (prefs->priv->gconf_client,
						    gpm_pref_key, NULL);

	gtk_widget_set_sensitive (widget, is_writable);

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
			gpm_critical_error ("Unknown action read from gconf: %s",
					    actions[i]);
		}

		if (strcmp (value, actions[i]) == 0)
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added - 1);
		i++;
	}
	g_object_set_data (G_OBJECT (widget), "gconf_key", (gpointer) gpm_pref_key);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_action_combo_changed_cb),
			  prefs);

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
	GtkWidget *twidget;
	const gchar *widget_name;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget_name = gtk_widget_get_name (widget);
	if (widget_name && strcmp (widget_name, "checkbutton_display_state_change") == 0) {
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"label_display_ac_brightness");
		gtk_widget_set_sensitive (twidget, checked);
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"hscale_display_ac_brightness");
		gtk_widget_set_sensitive (twidget, checked);
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"label_display_battery_brightness");
		gtk_widget_set_sensitive (twidget, checked);
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"hscale_display_battery_brightness");
		gtk_widget_set_sensitive (twidget, checked);
	}

	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (widget), "gconf_key");
	gpm_debug ("Changing %s to %i", gpm_pref_key, checked);
	gconf_client_set_bool (prefs->priv->gconf_client, gpm_pref_key, checked, NULL);
}

/**
 * gpm_prefs_setup_checkbox:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static GtkWidget*
gpm_prefs_setup_checkbox (GpmPrefs    *prefs,
			  const gchar *widget_name,
			  const gchar *gpm_pref_key)
{

	GladeXML    *xml = prefs->priv->glade_xml;
	gboolean checked;
	GtkWidget *widget;

	gpm_debug ("Setting up %s", gpm_pref_key);

	widget = glade_xml_get_widget (xml, widget_name);

	checked = gconf_client_get_bool (prefs->priv->gconf_client, gpm_pref_key, NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);

	g_object_set_data (G_OBJECT (widget), "gconf_key", (gpointer) gpm_pref_key);
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
 * @key: The gconf key
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
 * gconf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
gconf_key_changed_cb (GConfClient *client,
		      guint	   cnxn_id,
		      GConfEntry  *entry,
		      gpointer	   user_data)
{
	GpmPrefs *prefs = GPM_PREFS (user_data);
	gboolean  enabled;

	gpm_debug ("Key changed %s", entry->key);

	if (gconf_entry_get_value (entry) == NULL) {
		return;
	}

	if (strcmp (entry->key, GPM_PREF_AC_LOWPOWER) == 0) {
		enabled = gconf_client_get_bool (prefs->priv->gconf_client,
				  		 GPM_PREF_AC_LOWPOWER, NULL);
		gpm_debug ("need to enable checkbox");

	} else if (strcmp (entry->key, GPM_PREF_UPS_LOWPOWER) == 0) {
		enabled = gconf_client_get_bool (prefs->priv->gconf_client,
				  		 GPM_PREF_UPS_LOWPOWER, NULL);
		gpm_debug ("need to enable checkbox");

	} else if (strcmp (entry->key, GPM_PREF_BATTERY_LOWPOWER) == 0) {
		enabled = gconf_client_get_bool (prefs->priv->gconf_client,
				  		 GPM_PREF_BATTERY_LOWPOWER, NULL);
		gpm_debug ("need to enable checkbox");
	}
}

/**
 * gpm_prefs_processor_slider_changed_cb:
 * @range: The GtkRange object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_prefs_processor_slider_changed_cb (GtkRange *range,
				       GpmPrefs *prefs)
{
	gint value;
	gchar *gpm_pref_key;

	value = (gint) gtk_range_get_value (range);
	gpm_pref_key = (gchar *) g_object_get_data (G_OBJECT (range), "gconf_key");
	gpm_debug ("Changing %s to %i", gpm_pref_key, value);
	gconf_client_set_int (prefs->priv->gconf_client, gpm_pref_key, value, NULL);
}

/**
 * gpm_prefs_setup_processor_slider:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static GtkWidget *
gpm_prefs_setup_processor_slider (GpmPrefs   *prefs,
				  const gchar *widget_name,
				  const gchar *gpm_pref_key)
{
	GtkWidget *widget;
	gint value;
	gboolean is_writable;

	widget = glade_xml_get_widget (prefs->priv->glade_xml, widget_name);
	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_percentage_cb), prefs);

	value = gconf_client_get_int (prefs->priv->gconf_client, gpm_pref_key, NULL);

	is_writable = gconf_client_key_is_writable (prefs->priv->gconf_client,
						    gpm_pref_key, NULL);

	gtk_widget_set_sensitive (widget, is_writable);

	gtk_range_set_value (GTK_RANGE (widget), value);

	g_object_set_data (G_OBJECT (widget), "gconf_key", (gpointer) gpm_pref_key);

	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (gpm_prefs_processor_slider_changed_cb),
			  prefs);

	return widget;
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
	gboolean show_custom = FALSE;
	GtkWidget *twidget;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (value == NULL) {
		g_warning ("active text failed");
		return;
	}
	if (strcmp (value, CPUFREQ_ONDEMAND_TEXT) == 0) {
		policy = CODE_CPUFREQ_ONDEMAND;
	} else if (strcmp (value, CPUFREQ_CONSERVATIVE_TEXT) == 0) {
		policy = CODE_CPUFREQ_CONSERVATIVE;
	} else if (strcmp (value, CPUFREQ_POWERSAVE_TEXT) == 0) {
		policy = CODE_CPUFREQ_POWERSAVE;
	} else if (strcmp (value, CPUFREQ_USERSPACE_TEXT) == 0) {
		policy = CODE_CPUFREQ_USERSPACE;
		show_custom = TRUE;
	} else if (strcmp (value, CPUFREQ_PERFORMANCE_TEXT) == 0) {
		policy = CODE_CPUFREQ_PERFORMANCE;
	} else if (strcmp (value, CPUFREQ_NOTHING_TEXT) == 0) {
		policy = CODE_CPUFREQ_NOTHING;
	} else {
		g_assert (FALSE);
	}

	/* show other options */
	if (strcmp (gtk_widget_get_name (widget), "combobox_processor_ac_profile") == 0) {
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"label_processor_ac_custom");
		gtk_widget_set_sensitive (twidget, show_custom);
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"hscale_processor_ac_custom");
		gtk_widget_set_sensitive (twidget, show_custom);
	} else {
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"label_processor_battery_custom");
		gtk_widget_set_sensitive (twidget, show_custom);
		twidget = glade_xml_get_widget (prefs->priv->glade_xml,
						"hscale_processor_battery_custom");
		gtk_widget_set_sensitive (twidget, show_custom);
	}

	g_free (value);
	gpm_pref_key = (char *) g_object_get_data (G_OBJECT (widget), "gconf_key");
	gpm_debug ("Changing %s to %s", gpm_pref_key, policy);
	gconf_client_set_string (prefs->priv->gconf_client, gpm_pref_key, policy, NULL);
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
				 GpmHalCpuFreqEnum cpufreq_types)
{
	gchar *value;
	guint n_added = 0;
	gboolean has_option = FALSE;
	gboolean is_writable;
	GtkWidget *widget;
	GpmHalCpuFreqEnum cpufreq_type;

	widget = glade_xml_get_widget (prefs->priv->glade_xml, widget_name);
	value = gconf_client_get_string (prefs->priv->gconf_client,
					 gpm_pref_key, NULL);
	is_writable = gconf_client_key_is_writable (prefs->priv->gconf_client,
						    gpm_pref_key, NULL);

	gtk_widget_set_sensitive (widget, is_writable);

	if (value == NULL) {
		gpm_warning ("invalid schema, please re-install");
		value = g_strdup ("nothing");
	}

	g_object_set_data (G_OBJECT (widget), "gconf_key", (gpointer) gpm_pref_key);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_processor_combo_changed_cb),
			  prefs);

	cpufreq_type = gpm_hal_cpufreq_string_to_enum (value);

	if (cpufreq_types & GPM_CPUFREQ_ONDEMAND) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   CPUFREQ_ONDEMAND_TEXT);
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
	if (cpufreq_types & GPM_CPUFREQ_USERSPACE) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   CPUFREQ_USERSPACE_TEXT);
		if (cpufreq_type == GPM_CPUFREQ_USERSPACE) {
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

	/* always add nothing as a valid option */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
				   CPUFREQ_NOTHING_TEXT);
	if (has_option == FALSE || cpufreq_type == GPM_CPUFREQ_NOTHING) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added);
	}
	g_free (value);
}

/** setup the sleep page */
static void
prefs_setup_sleep (GpmPrefs *prefs)
{
	GtkWidget *widget;
	gint delay;

	const gchar *sleep_type_actions[] = {ACTION_NOTHING,
					     ACTION_SUSPEND,
					     ACTION_HIBERNATE,
					     NULL};

	/* Sleep Type Combo Box */
	gpm_prefs_setup_action_combo (prefs, "combobox_sleep_general_type",
				      GPM_PREF_SLEEP_TYPE,
				      sleep_type_actions);
	/* Sleep time until we sleep */
	gpm_prefs_setup_sleep_slider (prefs, "hscale_sleep_ac_inactive",
				      GPM_PREF_AC_SLEEP_COMPUTER);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_sleep_battery_inactive",
				      GPM_PREF_BATTERY_SLEEP_COMPUTER);
//FIXME
//	gpm_prefs_setup_sleep_slider (prefs, "hscale_sleep_ups_inactive",
//				      GPM_PREF_UPS_SLEEP_COMPUTER);

	delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_sleep_battery_inactive", delay);
	set_idle_hscale_stops (prefs, "hscale_sleep_ac_inactive", delay);

	if (prefs->priv->has_batteries == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_sleep_battery");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_ups == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_sleep_ups");
		gtk_widget_hide_all (widget);
	}
}

/** setup the display page */
static void
prefs_setup_display (GpmPrefs *prefs)
{
	GtkWidget *widget;
	gint delay;

	/* Sleep time for display to blank */
	gpm_prefs_setup_sleep_slider (prefs, "hscale_display_ac_sleep",
				      GPM_PREF_AC_SLEEP_DISPLAY);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_display_battery_sleep",
				      GPM_PREF_BATTERY_SLEEP_DISPLAY);
	/* Display brightness */
	gpm_prefs_setup_brightness_slider (prefs, "hscale_display_ac_brightness",
					   GPM_PREF_AC_BRIGHTNESS);
	gpm_prefs_setup_brightness_slider (prefs, "hscale_display_battery_brightness",
					   GPM_PREF_BATTERY_BRIGHTNESS);

	/* set up the general checkboxes */
	gpm_prefs_setup_checkbox (prefs, "checkbutton_display_dim",
				  GPM_PREF_DISPLAY_IDLE_DIM);
	gpm_prefs_setup_checkbox (prefs, "checkbutton_display_state_change",
				  GPM_PREF_DISPLAY_STATE_CHANGE);
	gpm_prefs_setup_checkbox (prefs, "checkbutton_display_ambient",
				  GPM_PREF_DISPLAY_STATE_CHANGE);
	/* for now, hide */
	widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_display_ambient");
	gtk_widget_hide_all (widget);

	delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_display_battery_sleep", delay);
	set_idle_hscale_stops (prefs, "hscale_display_ac_sleep", delay);

	if (prefs->priv->has_lcd == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hscale_display_ac_brightness");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "label_display_ac_brightness");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hscale_display_battery_brightness");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "label_display_battery_brightness");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_display_ac_dim");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_display_battery_dim");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_batteries == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_display_battery");
		gtk_widget_hide_all (widget);
	}
}

/** setup the processor page */
static void
prefs_setup_processor (GpmPrefs *prefs)
{
	GtkWidget    *widget;
	GtkWidget    *notebook;
	gint	      page;

	/* remove if we have no options to set */
	if (prefs->priv->has_cpufreq == FALSE) {
		notebook = glade_xml_get_widget (prefs->priv->glade_xml, "notebook_preferences");
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_processor");
		page = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (widget));
		gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page);
		return;
	}

	gpm_prefs_setup_processor_combo (prefs, "combobox_processor_ac_profile",
					 GPM_PREF_AC_CPUFREQ_POLICY, prefs->priv->cpufreq_types);
	gpm_prefs_setup_processor_combo (prefs, "combobox_processor_battery_profile",
					 GPM_PREF_BATTERY_CPUFREQ_POLICY, prefs->priv->cpufreq_types);

	gpm_prefs_setup_processor_slider (prefs, "hscale_processor_battery_custom",
					  GPM_PREF_BATTERY_CPUFREQ_VALUE);
	gpm_prefs_setup_processor_slider (prefs, "hscale_processor_ac_custom",
					  GPM_PREF_AC_CPUFREQ_VALUE);
}

/** setup the actions page */
static void
prefs_setup_actions (GpmPrefs *prefs)
{
	GtkWidget    *widget;
	const gchar  *button_lid_actions[] = {ACTION_NOTHING,
					      ACTION_BLANK,
					      ACTION_SUSPEND,
					      ACTION_HIBERNATE,
					      NULL};
	const gchar  *battery_critical_actions[] = {ACTION_NOTHING,
						    ACTION_SUSPEND,
						    ACTION_HIBERNATE,
						    ACTION_SHUTDOWN,
						    NULL};
	const gchar  *battery_ups_actions[] = {ACTION_NOTHING,
					       ACTION_HIBERNATE,
					       ACTION_SHUTDOWN,
					       NULL};
	const gchar  *power_button_actions[] = {ACTION_INTERACTIVE,
						ACTION_SUSPEND,
						ACTION_HIBERNATE,
						ACTION_SHUTDOWN,
						NULL};
	/* Power button action */
	gpm_prefs_setup_action_combo (prefs, "combobox_actions_general_power",
				      GPM_PREF_BUTTON_POWER,
				      power_button_actions);

	/* Lid close actions */
	gpm_prefs_setup_action_combo (prefs, "combobox_actions_ac_lid",
				      GPM_PREF_AC_BUTTON_LID,
				      button_lid_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_actions_battery_lid",
				      GPM_PREF_BATTERY_BUTTON_LID,
				      button_lid_actions);

	/* set up the LowPowerMode checkbox */
	gpm_prefs_setup_checkbox (prefs, "checkbutton_actions_battery_low_power",
	  			  GPM_PREF_BATTERY_LOWPOWER);

	/* battery critical actions */
	gpm_prefs_setup_action_combo (prefs, "combobox_actions_battery_critical",
				      GPM_PREF_BATTERY_CRITICAL,
				      battery_critical_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_actions_ups_critical",
				      GPM_PREF_UPS_CRITICAL,
				      battery_ups_actions);

	gpm_prefs_setup_action_combo (prefs, "combobox_actions_ups_low",
				      GPM_PREF_UPS_LOW,
				      battery_ups_actions);

	if (prefs->priv->has_button_lid == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "combobox_actions_ac_lid");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "combobox_actions_battery_lid");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "label_actions_ac_lid");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "label_actions_battery_lid");
		gtk_widget_hide_all (widget);
		/* remove whole box as there is nothing in it */
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_actions_ac");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_ups == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_actions_ups");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_batteries == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_actions_battery");
		gtk_widget_hide_all (widget);
	}
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

	icon_policy_str = gconf_client_get_string (prefs->priv->gconf_client,
						   GPM_PREF_ICON_POLICY, NULL);
	icon_policy = GPM_ICON_POLICY_ALWAYS;
	gconf_string_to_enum (icon_policy_enum_map, icon_policy_str, &icon_policy);
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

	is_writable = gconf_client_key_is_writable (prefs->priv->gconf_client,
						    GPM_PREF_ICON_POLICY, NULL);
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

	/* only connect the callbacks after we set the value, else the gconf
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
	  			  GPM_PREF_ENABLE_BEEPING);

	if (prefs->priv->has_batteries == FALSE) {
		/* Hide battery radio options if we have no batteries */
		gtk_widget_hide_all (radiobutton_icon_charge);
		gtk_widget_hide_all (radiobutton_icon_critical);
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
	prefs->priv->cpufreq = gpm_hal_cpufreq_new ();

	prefs->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (prefs->priv->screensaver, "gs-delay-changed",
			  G_CALLBACK (gs_delay_changed_cb), prefs);

	prefs->priv->gconf_client = gconf_client_get_default ();

	gconf_client_notify_add (prefs->priv->gconf_client,
				 GPM_PREF_DIR,
				 gconf_key_changed_cb,
				 prefs,
				 NULL,
				 NULL);

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

	prefs->priv->can_suspend = gpm_dbus_method_bool ("AllowedSuspend");
	prefs->priv->can_hibernate = gpm_dbus_method_bool ("AllowedHibernate");

	/* only enable cpufreq stuff if we have the hardware */
	prefs->priv->has_cpufreq = gpm_hal_cpufreq_has_hardware (prefs->priv->cpufreq);
	gboolean ret = FALSE;
	if (prefs->priv->has_cpufreq) {
		ret = gpm_hal_cpufreq_get_governors (prefs->priv->cpufreq,
							   &prefs->priv->cpufreq_types);
	}
	/* method failed, can't assume we have cpu frequency scaling */
	if (ret == FALSE) {
		prefs->priv->cpufreq_types = GPM_CPUFREQ_NOTHING;
		prefs->priv->has_cpufreq = FALSE;
	}

	prefs->priv->glade_xml = glade_xml_new (GPM_DATA "/gpm-prefs.glade", NULL, NULL);

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

	/* the 'Sleep' tab */
	prefs_setup_sleep (prefs);

	/* the 'Display' tab */
	prefs_setup_display (prefs);

	/* the 'Processing Speed' tab */
	prefs_setup_processor (prefs);

	/* the 'Actions' tab */
	prefs_setup_actions (prefs);

	/* the 'Notification' tab */
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

	g_object_unref (prefs->priv->gconf_client);
	g_object_unref (prefs->priv->screensaver);
	g_object_unref (prefs->priv->cpufreq);
	g_object_unref (prefs->priv->hal);
	/* FIXME: we should unref some stuff */

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
