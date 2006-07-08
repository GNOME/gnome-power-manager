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

#include "gpm-screensaver.h"
#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-hal.h"
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
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	GpmScreensaver		*screensaver;
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
 * gpm_dbus_method_bool:
 * @method: The g-p-m DBUS method name, e.g. "AllowedSuspend"
 **/
static gboolean
gpm_dbus_method_bool (const char *method)
{
	DBusGConnection *connection;
	DBusGProxy      *proxy;
	GError	   *error;
	gboolean	  value;

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
	const char  *str;
	int	     policy;

	policy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "policy"));
	str = gconf_enum_to_string (icon_policy_enum_map, policy);
	gpm_debug ("Changing %s to %s", GPM_PREF_ICON_POLICY, str);
	gconf_client_set_string (prefs->priv->gconf_client,
				 GPM_PREF_ICON_POLICY,
				 str, NULL);
}

/**
 * gpm_prefs_format_brightness_cb:
 * @scale: The GtkScale object
 * @value: The value in %.
 **/
static char *
gpm_prefs_format_brightness_cb (GtkScale *scale,
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
static char *
gpm_prefs_format_time_cb (GtkScale *scale,
			  gdouble   value,
			  GpmPrefs *prefs)
{
	char *str;
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
gpm_prefs_setup_sleep_slider (GpmPrefs  *prefs,
			      char	*widget_name,
			      char	*gpm_pref_key)
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
		/* policy is in seconds, slider is in minutes */
		value /= 60;
		int gs_idle_time = gpm_screensaver_get_delay (prefs->priv->screensaver);
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
	char *gpm_pref_key;

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
gpm_prefs_setup_brightness_slider (GpmPrefs *prefs,
				   char     *widget_name,
				   char     *gpm_pref_key)
{
	GladeXML    *xml = prefs->priv->glade_xml;
	GtkWidget *widget;
	int value;
	gboolean is_writable;

	widget = glade_xml_get_widget (xml, widget_name);

	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_brightness_cb), NULL);

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
	char *value;
	char *action;
	char *gpm_pref_key;

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
gpm_prefs_setup_action_combo (GpmPrefs    *prefs,
			      char	  *widget_name,
			      char	  *gpm_pref_key,
			      const char **actions)
{
	GladeXML    *xml = prefs->priv->glade_xml;
	char *value;
	int i = 0;
	int n_added = 0;
	gboolean is_writable;
	GtkWidget *widget;

	widget = glade_xml_get_widget (xml, widget_name);

	value = gconf_client_get_string (prefs->priv->gconf_client,
					 gpm_pref_key, NULL);
	is_writable = gconf_client_key_is_writable (prefs->priv->gconf_client,
						    gpm_pref_key, NULL);

	gtk_widget_set_sensitive (widget, is_writable);

	if (! value) {
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
	char *gpm_pref_key;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

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
gpm_prefs_setup_checkbox (GpmPrefs  *prefs,
			  char	    *widget_name,
			  char	    *gpm_pref_key)
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

	return widget;
}

/**
 * setup_battery_sliders:
 * @prefs: This prefs class instance
 **/
static void
setup_battery_sliders (GpmPrefs *prefs)
{
	GtkWidget   *vbox_battery_brightness;

	/* Sleep time on batteries */
	gpm_prefs_setup_sleep_slider (prefs, "hscale_battery_computer",
				      GPM_PREF_BATTERY_SLEEP_COMPUTER);

	/* Sleep time for display when on batteries */
	gpm_prefs_setup_sleep_slider (prefs, "hscale_battery_display",
				      GPM_PREF_BATTERY_SLEEP_DISPLAY);

	/* Display brightness when on batteries */
	gpm_prefs_setup_brightness_slider (prefs, "hscale_battery_brightness",
					   GPM_PREF_BATTERY_BRIGHTNESS);

	if (! prefs->priv->has_lcd) {
		vbox_battery_brightness = glade_xml_get_widget (prefs->priv->glade_xml,
								"vbox_battery_brightness");
		gtk_widget_hide_all (vbox_battery_brightness);
	}
}

/**
 * setup_ac_sliders:
 * @prefs: This prefs class instance
 **/
static void
setup_ac_sliders (GpmPrefs *prefs)
{
	GladeXML    *xml = prefs->priv->glade_xml;
	GtkWidget   *vbox_ac_brightness;

	/* Sleep time on AC */
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ac_computer",
				      GPM_PREF_AC_SLEEP_COMPUTER);

	/* Sleep time for display on AC */
	gpm_prefs_setup_sleep_slider (prefs, "hscale_ac_display",
				      GPM_PREF_AC_SLEEP_DISPLAY);

	/* Display brightness when on AC */
	gpm_prefs_setup_brightness_slider (prefs, "hscale_ac_brightness",
					   GPM_PREF_AC_BRIGHTNESS);

	if (! prefs->priv->has_lcd) {
		vbox_ac_brightness = glade_xml_get_widget (xml, "vbox_ac_brightness");
		gtk_widget_hide_all (vbox_ac_brightness);
	}
}

/**
 * setup_sleep_type:
 * @prefs: This prefs class instance
 **/
static void
setup_sleep_type (GpmPrefs *prefs)
{
	GtkWidget    *checkbutton_dim_idle;
	const char   *sleep_type_actions[] = {ACTION_NOTHING,
					      ACTION_SUSPEND,
					      ACTION_HIBERNATE,
					      NULL};

	/* Sleep Type Combo Box */

	gpm_prefs_setup_action_combo (prefs, "combobox_sleep_type",
				      GPM_PREF_SLEEP_TYPE,
				      sleep_type_actions);

	/* set up the "do we dim screen on idle checkbox */
	checkbutton_dim_idle = gpm_prefs_setup_checkbox (prefs, "checkbutton_dim_idle",
				  			 GPM_PREF_IDLE_DIM_SCREEN);

	if (! prefs->priv->has_lcd) {
		gtk_widget_hide_all (checkbutton_dim_idle);
	}
}

/**
 * setup_ac_actions:
 * @prefs: This prefs class instance
 **/
static void
setup_ac_actions (GpmPrefs *prefs)
{
	GtkWidget    *checkbox_powersave;
	GtkWidget    *vbox_ac_actions;
	const char   *button_lid_actions[] = {ACTION_NOTHING,
					      ACTION_BLANK,
					      ACTION_SUSPEND,
					      ACTION_HIBERNATE,
					      NULL};

	gpm_prefs_setup_action_combo (prefs, "combobox_ac_lid_close",
				      GPM_PREF_AC_BUTTON_LID,
				      button_lid_actions);

	checkbox_powersave = gpm_prefs_setup_checkbox (prefs,
						       "checkbutton_ac_powersave",
						       GPM_PREF_AC_LOWPOWER);
	/* hide elements that do not apply */
	if (! prefs->priv->has_batteries) {
		gtk_widget_hide_all (checkbox_powersave);
		if (!prefs->priv->has_button_lid) {
			/* hide the whole vbox if we don't have a lid */
			vbox_ac_actions = glade_xml_get_widget (prefs->priv->glade_xml,
								"vbox_ac_actions");
			gtk_widget_hide_all (vbox_ac_actions);
		}
	}
}

/**
 * setup_battery_actions:
 * @prefs: This prefs class instance
 **/
static void
setup_battery_actions (GpmPrefs *prefs)
{
	GtkWidget    *label_button_lid;
	GtkWidget    *combo_button_lid;
	const char   *button_lid_actions[] = {ACTION_NOTHING,
					      ACTION_BLANK,
					      ACTION_SUSPEND,
					      ACTION_HIBERNATE,
					      NULL};
	const char   *battery_critical_actions[] = {ACTION_NOTHING,
						    ACTION_SUSPEND,
						    ACTION_HIBERNATE,
						    ACTION_SHUTDOWN,
						    NULL};

	/* Button Lid Combo Box */
	gpm_prefs_setup_action_combo (prefs, "combobox_battery_lid_close",
				      GPM_PREF_BATTERY_BUTTON_LID,
				      button_lid_actions);

	if (! prefs->priv->has_button_lid) {
		label_button_lid = glade_xml_get_widget (prefs->priv->glade_xml,
							 "label_battery_button_lid");
		combo_button_lid = glade_xml_get_widget (prefs->priv->glade_xml,
							 "combobox_battery_lid_close");
		gtk_widget_hide_all (label_button_lid);
		gtk_widget_hide_all (combo_button_lid);
	}

	gpm_prefs_setup_checkbox (prefs, "checkbutton_battery_powersave",
	  			  GPM_PREF_BATTERY_LOWPOWER);

	gpm_prefs_setup_action_combo (prefs, "combobox_battery_critical",
				      GPM_PREF_BATTERY_CRITICAL,
				      battery_critical_actions);
}

/**
 * setup_ups_actions:
 * @prefs: This prefs class instance
 **/
static void
setup_ups_actions (GpmPrefs *prefs)
{
	const char   *battery_ups_actions[] = {ACTION_NOTHING,
					       ACTION_HIBERNATE,
					       ACTION_SHUTDOWN,
					       NULL};
	gpm_prefs_setup_checkbox (prefs, "checkbutton_ups_powersave",
	  			  GPM_PREF_UPS_LOWPOWER);

	gpm_prefs_setup_action_combo (prefs, "combobox_ups_critical",
				      GPM_PREF_UPS_CRITICAL,
				      battery_ups_actions);
}

/**
 * setup_icon_policy:
 * @prefs: This prefs class instance
 **/
static void
setup_icon_policy (GpmPrefs *prefs)
{
	char	    *icon_policy_str;
	int	     icon_policy;
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
							"radiobutton_icon_always");
	radiobutton_icon_present = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_icon_present");
	radiobutton_icon_charge = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_icon_charge");
	radiobutton_icon_critical = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_icon_critical");
	radiobutton_icon_never = glade_xml_get_widget (prefs->priv->glade_xml,
							"radiobutton_icon_never");

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

	if (! prefs->priv->has_batteries) {
		/* Hide battery radio options if we have no batteries */
		gtk_widget_hide_all (radiobutton_icon_charge);
		gtk_widget_hide_all (radiobutton_icon_critical);
	}
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
gpm_prefs_delete_event_cb (GtkWidget	*widget,
			  GdkEvent	*event,
			  GpmPrefs	*prefs)
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
set_idle_hscale_stops (GpmPrefs   *prefs,
		       const char *widget_name,
		       int	   gs_idle_time)
{
	GtkWidget *widget;
	widget = glade_xml_get_widget (prefs->priv->glade_xml, widget_name);
	gtk_range_set_range (GTK_RANGE (widget), gs_idle_time + 1, NEVER_TIME_ON_SLIDER);
}

/**
 * gs_delay_changed_cb:
 * @key: The gconf key
 * @prefs: This prefs class instance
 **/
static void
gs_delay_changed_cb (GpmScreensaver *screensaver,
		    int		     delay,
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
 * gpm_prefs_init:
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_init (GpmPrefs *prefs)
{
	GtkWidget    *main_window;
	GtkWidget    *widget;

	prefs->priv = GPM_PREFS_GET_PRIVATE (prefs);

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

	prefs->priv->has_lcd = gpm_hal_num_devices_of_capability ("laptop_panel") > 0;
	prefs->priv->has_batteries = gpm_hal_num_devices_of_capability_with_value ("battery",
							"battery.type",
							"primary") > 0;
	prefs->priv->has_ups = gpm_hal_num_devices_of_capability_with_value ("battery",
							"battery.type",
							"ups") > 0;
	prefs->priv->has_button_lid = gpm_hal_num_devices_of_capability_with_value ("button",
							"button.type",
							"lid") > 0;
	prefs->priv->can_suspend = gpm_dbus_method_bool ("AllowedSuspend");
	prefs->priv->can_hibernate = gpm_dbus_method_bool ("AllowedHibernate");

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

	setup_icon_policy (prefs);
	setup_ac_actions (prefs);
	setup_ac_sliders (prefs);
	setup_battery_actions (prefs);
	setup_battery_sliders (prefs);
	setup_ups_actions (prefs);
	setup_sleep_type (prefs);

	const char   *power_button_actions[] = {ACTION_INTERACTIVE,
						ACTION_SUSPEND,
						ACTION_HIBERNATE,
						ACTION_SHUTDOWN,
						NULL};

	/* Power button action */
	gpm_prefs_setup_action_combo (prefs, "combobox_power_button",
				      GPM_PREF_BUTTON_POWER,
				      power_button_actions);

	int delay = gpm_screensaver_get_delay (prefs->priv->screensaver);
	set_idle_hscale_stops (prefs, "hscale_battery_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_battery_display", delay);
	set_idle_hscale_stops (prefs, "hscale_ac_computer", delay);
	set_idle_hscale_stops (prefs, "hscale_ac_display", delay);

	GtkWidget *notebook;
	int page;
	notebook = glade_xml_get_widget (prefs->priv->glade_xml, "gpm_notebook");
	/* if no options then disable frame as it will be empty */
	if (! prefs->priv->has_batteries) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_battery_power");
		page = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (widget));
		gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page);
	}
	if (! prefs->priv->has_ups) {
		widget = glade_xml_get_widget (prefs->priv->glade_xml, "vbox_ups_power");
		page = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (widget));
		gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page);
	}

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
