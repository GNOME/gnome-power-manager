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
#include <gconf/gconf-client.h>

#include <libhal-gmanager.h>

#include "gpm-tray-icon.h"
#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-conf.h"
#include "gpm-prefs-core.h"
#include "egg-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-screensaver.h"
#include "gpm-prefs-server.h"

#ifdef HAVE_GCONF_DEFAULTS
#include <polkit-gnome/polkit-gnome.h>
#endif

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
	gboolean		 has_ambient;
	gboolean		 has_button_lid;
	gboolean		 has_button_suspend;
	gboolean		 can_shutdown;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	GpmConf			*conf;
	GpmScreensaver		*screensaver;
#ifdef HAVE_GCONF_DEFAULTS
	PolKitGnomeAction	*default_action;
#endif
};

enum {
	ACTION_HELP,
	ACTION_CLOSE,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmPrefs, gpm_prefs, G_TYPE_OBJECT)

/* The text that should appear in the action combo boxes */
#define ACTION_INTERACTIVE_TEXT		_("Ask me")
#define ACTION_SUSPEND_TEXT		_("Suspend")
#define ACTION_SHUTDOWN_TEXT		_("Shutdown")
#define ACTION_HIBERNATE_TEXT		_("Hibernate")
#define ACTION_BLANK_TEXT		_("Blank screen")
#define ACTION_NOTHING_TEXT		_("Do nothing")

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
			egg_warning ("Couldn't connect to PowerManager %s",
				     error->message);
			g_error_free (error);
		}
		return FALSE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_PATH,
					   GPM_DBUS_INTERFACE);
	ret = dbus_g_proxy_call (proxy, method, &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &value,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("%s failed!", method);
		return FALSE;
	}
	g_object_unref (proxy);
	return value;
}

/**
 * gpm_dbus_method_int:
 * @method: The g-p-m DBUS method name, e.g. "AllowedSuspend"
 **/
static gint
gpm_dbus_method_int (const gchar *method)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error;
	gboolean ret;
	gint value = 0;
	error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		if (error) {
			egg_warning ("Couldn't connect to PowerManager %s",
				     error->message);
			g_error_free (error);
		}
		return FALSE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_PATH,
					   GPM_DBUS_INTERFACE);
	ret = dbus_g_proxy_call (proxy, method, &error,
				 G_TYPE_INVALID,
				 G_TYPE_INT, &value,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("%s failed!", method);
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
	egg_debug ("emitting action-help");
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
	egg_debug ("Changing %s to %s", GPM_CONF_UI_ICON_POLICY, str);
	gpm_conf_set_string (prefs->priv->conf, GPM_CONF_UI_ICON_POLICY, str);
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
	egg_debug ("Changing %s to %i", gpm_pref_key, value);
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
	egg_debug ("Changing %s to %i", gpm_pref_key, (int) value);
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
	egg_debug ("Changing %s to %s", gpm_pref_key, action);
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
		egg_warning ("invalid schema, please re-install");
		value = g_strdup ("nothing");
	}

	while (actions[i] != NULL) {
		if ((strcmp (actions[i], ACTION_SHUTDOWN) == 0) && !prefs->priv->can_shutdown) {
			egg_debug ("Cannot add option, as cannot shutdown.");
		} else if (strcmp (actions[i], ACTION_SHUTDOWN) == 0 && prefs->priv->can_shutdown) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_SHUTDOWN_TEXT);
			n_added++;
		} else if ((strcmp (actions[i], ACTION_SUSPEND) == 0) && !prefs->priv->can_suspend) {
			egg_debug ("Cannot add option, as cannot suspend.");
		} else if ((strcmp (actions[i], ACTION_HIBERNATE) == 0) && !prefs->priv->can_hibernate) {
			egg_debug ("Cannot add option, as cannot hibernate.");
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
			egg_error ("Unknown action read from conf: %s", actions[i]);
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
	egg_debug ("Changing %s to %i", gpm_pref_key, checked);
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

	egg_debug ("Setting up %s", gpm_pref_key);

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
	egg_debug ("emitting action-close");
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
		egg_warning ("gnome-screensaver timeout is really big. "
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
	int value;
	GtkWidget *widget;
	gboolean  enabled;

	if (strcmp (key, GPM_CONF_BACKLIGHT_BRIGHTNESS_AC) == 0) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hscale_ac_brightness");
		gpm_conf_get_int (conf, key, &value);
		gtk_range_set_value (GTK_RANGE (widget), value);
	}

	if (strcmp (key, GPM_CONF_LOWPOWER_AC) == 0) {
		gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_LOWPOWER_AC, &enabled);
		egg_debug ("need to enable checkbox");

	} else if (strcmp (key, GPM_CONF_LOWPOWER_UPS) == 0) {
		gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_LOWPOWER_UPS, &enabled);
		egg_debug ("need to enable checkbox");

	} else if (strcmp (key, GPM_CONF_LOWPOWER_BATT) == 0) {
		gpm_conf_get_bool (prefs->priv->conf, GPM_CONF_LOWPOWER_BATT, &enabled);
		egg_debug ("need to enable checkbox");
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

	gpm_conf_get_string (prefs->priv->conf, GPM_CONF_UI_ICON_POLICY, &icon_policy_str);
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

	gpm_conf_is_writable (prefs->priv->conf, GPM_CONF_UI_ICON_POLICY, &is_writable);
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
	  			  GPM_CONF_UI_ENABLE_BEEPING);

	if (prefs->priv->has_batteries) {
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
	const gchar  *button_lid_actions[] =
				{ACTION_NOTHING,
				 ACTION_BLANK,
				 ACTION_SUSPEND,
				 ACTION_HIBERNATE,
				 ACTION_SHUTDOWN,
				 NULL};

	gpm_prefs_setup_action_combo (prefs, "combobox_ac_lid",
				      GPM_CONF_BUTTON_LID_AC,
				      button_lid_actions);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ac_computer",
				      GPM_CONF_TIMEOUT_SLEEP_COMPUTER_AC);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ac_display",
				      GPM_CONF_TIMEOUT_SLEEP_DISPLAY_AC);
	gpm_prefs_setup_brightness_slider (prefs, "hscale_ac_brightness",
					   GPM_CONF_BACKLIGHT_BRIGHTNESS_AC);

	gpm_prefs_setup_checkbox (prefs, "checkbutton_ac_display_dim",
				  GPM_CONF_BACKLIGHT_IDLE_DIM_AC);

	delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_ac_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_ac_display", delay);

	if (prefs->priv->has_button_lid == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_ac_lid");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_lcd == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_ac_brightness");
		gtk_widget_hide_all (widget);
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_ac_display_dim");
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

	const gchar  *button_lid_actions[] =
				{ACTION_NOTHING,
				 ACTION_BLANK,
				 ACTION_SUSPEND,
				 ACTION_HIBERNATE,
				 ACTION_SHUTDOWN,
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
				      GPM_CONF_BUTTON_LID_BATT,
				      button_lid_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_battery_critical",
				      GPM_CONF_ACTIONS_CRITICAL_BATT,
				      battery_critical_actions);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_battery_computer",
				      GPM_CONF_TIMEOUT_SLEEP_COMPUTER_BATT);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_battery_display",
				      GPM_CONF_TIMEOUT_SLEEP_DISPLAY_BATT);

	/* set up the battery reduce checkbox */
	gpm_prefs_setup_checkbox (prefs, "checkbutton_battery_display_reduce",
	  			  GPM_CONF_BACKLIGHT_BATTERY_REDUCE);

	gpm_prefs_setup_checkbox (prefs, "checkbutton_battery_display_dim",
				  GPM_CONF_BACKLIGHT_IDLE_DIM_BATT);

	if (prefs->priv->has_ambient == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_general_ambient");
		gtk_widget_hide_all (widget);
	}

	delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_battery_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_battery_display", delay);

	if (prefs->priv->has_button_lid == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_battery_lid");
		gtk_widget_hide_all (widget);
	}
	if (prefs->priv->has_lcd == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_battery_display_dim");
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
				      GPM_CONF_ACTIONS_LOW_UPS,
				      ups_low_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_ups_critical",
				      GPM_CONF_ACTIONS_CRITICAL_UPS,
				      ups_low_actions);
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ups_computer",
				      GPM_CONF_TIMEOUT_SLEEP_COMPUTER_BATT);
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
				  GPM_CONF_AMBIENT_ENABLE);

	if (prefs->priv->has_ambient == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "checkbutton_general_ambient");
		gtk_widget_hide_all (widget);
	}

	if (prefs->priv->has_button_suspend == FALSE) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "hbox_general_suspend");
		gtk_widget_hide_all (widget);
	}
}

#ifdef HAVE_GCONF_DEFAULTS
/**
 * pk_prefs_set_defaults_cb:
 **/
static void
pk_prefs_set_defaults_cb (PolKitGnomeAction *default_action, GpmPrefs *prefs)
{
	DBusGProxy *proxy;
	DBusGConnection *connection;
	GError *error;
	gchar *keys[5] = {
		"/apps/gnome-power-manager/actions",
		"/apps/gnome-power-manager/ui",
		"/apps/gnome-power-manager/buttons",
		"/apps/gnome-power-manager/backlight",
		"/apps/gnome-power-manager/timeout"
	};

	error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		g_warning ("failed to get system bus connection: %s", error->message);
		g_error_free (error);
		return;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.gnome.GConf.Defaults",
					   "/",
					   "org.gnome.GConf.Defaults");
	if (proxy == NULL) {
		g_warning ("Cannot connect to defaults mechanism");
		return;
	}

	GConfClient *client;
	client = gconf_client_get_default ();
	gconf_client_suggest_sync (client, NULL);
	g_object_unref (client);
	dbus_g_proxy_call (proxy, "SetSystem", &error,
			   G_TYPE_STRV, keys,
			   G_TYPE_STRV, NULL,
			   G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref (proxy);
}

/**
 * gpk_prefs_create_custom_widget:
 **/
static GtkWidget *
gpk_prefs_create_custom_widget (GladeXML *xml, gchar *func_name, gchar *name,
				     gchar *string1, gchar *string2,
				     gint int1, gint int2, gpointer user_data)
{
	GpmPrefs *prefs = GPM_PREFS (user_data);
	if (strcmp (name, "button_default") == 0) {
		return polkit_gnome_action_create_button (prefs->priv->default_action);
	}
	egg_warning ("name unknown=%s", name);
	return NULL;
}

/**
 * gpk_prefs_setup_policykit:
 *
 * We have to do this before the glade stuff if done as the custom handler needs the actions setup
 **/
static void
gpk_prefs_setup_policykit (GpmPrefs *prefs)
{
	PolKitAction *pk_action;

	g_return_if_fail (GPM_IS_PREFS (prefs));

	/* set default */
	pk_action = polkit_action_new ();
	polkit_action_set_action_id (pk_action, "org.gnome.gconf.defaults.set-system");
	prefs->priv->default_action = polkit_gnome_action_new_default ("set-defaults", pk_action,
								       _("Make Default"), NULL);
	g_object_set (prefs->priv->default_action,
		      "no-icon-name", GTK_STOCK_FLOPPY,
		      "auth-icon-name", GTK_STOCK_FLOPPY,
		      "yes-icon-name", GTK_STOCK_FLOPPY,
		      "self-blocked-icon-name", GTK_STOCK_FLOPPY,
		      NULL);
	polkit_action_unref (pk_action);
}
#endif

/**
 * gpm_prefs_init:
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_init (GpmPrefs *prefs)
{
	GtkWidget *main_window;
	GtkWidget *widget;
	gint caps;

	prefs->priv = GPM_PREFS_GET_PRIVATE (prefs);

	prefs->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (prefs->priv->screensaver, "gs-delay-changed",
			  G_CALLBACK (gs_delay_changed_cb), prefs);

	prefs->priv->conf = gpm_conf_new ();
	g_signal_connect (prefs->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), prefs);

	caps = gpm_dbus_method_int ("GetPreferencesOptions");
	prefs->priv->has_batteries = ((caps & GPM_PREFS_SERVER_BATTERY) > 0);
	prefs->priv->has_ups = ((caps & GPM_PREFS_SERVER_UPS) > 0);
	prefs->priv->has_lcd = ((caps & GPM_PREFS_SERVER_BACKLIGHT) > 0);
	prefs->priv->has_ambient = ((caps & GPM_PREFS_SERVER_AMBIENT) > 0);
	prefs->priv->has_button_lid = ((caps & GPM_PREFS_SERVER_LID) > 0);
	prefs->priv->has_button_suspend = TRUE;
	prefs->priv->can_shutdown = gpm_dbus_method_bool ("CanShutdown");
	prefs->priv->can_suspend = gpm_dbus_method_bool ("CanSuspend");
	prefs->priv->can_hibernate = gpm_dbus_method_bool ("CanHibernate");
	egg_debug ("caps=%i", caps);

#ifdef HAVE_GCONF_DEFAULTS
	/* use custom widgets */
	glade_set_custom_handler (gpk_prefs_create_custom_widget, prefs);

	/* we have to do this before we connect up the glade file */
	gpk_prefs_setup_policykit (prefs);
#endif

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

#ifdef HAVE_GCONF_DEFAULTS
	g_signal_connect (prefs->priv->default_action, "activate",
			  G_CALLBACK (pk_prefs_set_defaults_cb), prefs);
#endif

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
