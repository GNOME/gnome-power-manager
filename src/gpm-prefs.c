/** @file	gpm-prefs.c
 *  @brief	GNOME Power Preferences
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *  @note	Taken in part from: GNOME Tutorial, example 1
 *		http://www.gnome.org/~newren/tutorials/
 *		developing-with-gnome/html/apd.html
 *
 *  @todo: checkbutton_require_password
 * This is the main g-p-m module, responsible for loading the glade file, 
 * populating and disabling widgets, and writing out configuration to gconf.
 */
/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/**
 * @addtogroup	prefs		GNOME Power Preferences
 * @brief			Sets policy for GNOME Power Manager
 *
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <math.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <popt.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-libnotify.h"
#include "gpm-main.h"
#include "gpm-screensaver.h"
#include "gpm-dbus-client.h"
#include "gpm-dbus-common.h"
#include "gpm-gtk-utils.h"

#include "glibhal-main.h"
#include "glibhal-extras.h"

/* If sleep time in a slider is set to 61 it is considered as never sleep */
const int NEVER_TIME_ON_SLIDER = 61;
static GladeXML *prefwidgets;
static gboolean isVerbose;

/** Shows/hides/renames controls based on hasData
 *  i.e. what hardware is in the system.
 *
 */
static void
recalc (void)
{
	GtkWidget *widget = NULL;
	GConfClient *client = gconf_client_get_default ();

	gboolean hasBatteries;
	gboolean hasAcAdapter;
	gboolean hasButtonPower;
	gboolean hasButtonSleep;
	gboolean hasButtonLid;
	gboolean hasLCD;
	gboolean hasDisplays;
	gboolean doPassword;
	gchar *policy;
	IconPolicy iconopt;

	/* radio options */
	policy = gconf_client_get_string (client,
		GCONF_ROOT "general/display_icon_policy", NULL);
	if (!policy) {
		g_warning ("You have not set an icon policy! "
			   "I'll assume you want an icon all the time...");
		policy = "always";
	}
	/* convert to enum */
	iconopt = convert_string_to_iconpolicy (policy);
	g_free (policy);

	if (iconopt == ICON_NEVER)
		gpm_gtk_set_check (prefwidgets, "radiobutton_icon_never", TRUE);
	else if (iconopt == ICON_CRITICAL)
		gpm_gtk_set_check (prefwidgets, "radiobutton_icon_critical", TRUE);
	else if (iconopt == ICON_CHARGE)
		gpm_gtk_set_check (prefwidgets, "radiobutton_icon_charge", TRUE);
	else if (iconopt == ICON_ALWAYS)
		gpm_gtk_set_check (prefwidgets, "radiobutton_icon_always", TRUE);

	/* set the correct entries for the checkboxes */
	doPassword = gconf_client_get_bool (client, 
			GCONF_ROOT "general/require_password", NULL);
	gpm_gtk_set_check (prefwidgets, "checkbutton_require_password", doPassword);

	hasBatteries =   (hal_num_devices_of_capability ("battery") > 0);
	hasAcAdapter =   (hal_num_devices_of_capability ("ac_adapter") > 0);
	hasButtonPower = (hal_num_devices_of_capability_with_value
				("button", "button.type", "power") > 0);
	hasButtonSleep = (hal_num_devices_of_capability_with_value
				("button", "button.type", "sleep") > 0);
	hasButtonLid =   (hal_num_devices_of_capability_with_value
				("button", "button.type", "lid") > 0);
	hasLCD = 	 (hal_num_devices_of_capability ("laptop_panel") > 0);

	if (gpm_screensaver_is_running ()) {
		hasDisplays = gconf_client_get_bool (client,
				GS_GCONF_ROOT "dpms_enabled", NULL);
	} else
		hasDisplays = FALSE;

	/* frame labels */
	if (hasBatteries) {
		widget = glade_xml_get_widget (prefwidgets, "label_frame_ac");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Running on AC adapter</b>");
		widget = glade_xml_get_widget (prefwidgets, "label_frame_batteries");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Running on batteries</b>");
	} else {
		widget = glade_xml_get_widget (prefwidgets, "label_frame_ac");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Configuration</b>");
	}

	/* top frame */
	gpm_gtk_set_visibility (prefwidgets, "frame_batteries", hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "combobox_battery_critical", hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "label_battery_critical_action", hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "label_battery_critical", hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "label_battery_low", hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "hscale_battery_low", hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "hscale_battery_critical", hasBatteries);
	/* assumes only battery options are in this frame */
	gpm_gtk_set_visibility (prefwidgets, "frame_other_options", hasBatteries);

	/* options */
	gpm_gtk_set_visibility (prefwidgets, "combobox_button_lid", hasButtonLid);
	gpm_gtk_set_visibility (prefwidgets, "label_button_lid", hasButtonLid);

	gpm_gtk_set_visibility (prefwidgets, "combobox_button_power", hasButtonPower);
	gpm_gtk_set_visibility (prefwidgets, "label_button_power", hasButtonPower);

	gpm_gtk_set_visibility (prefwidgets, "combobox_button_suspend", hasButtonSleep);
	gpm_gtk_set_visibility (prefwidgets, "label_button_suspend", hasButtonSleep);

	/* variables */
	gpm_gtk_set_visibility (prefwidgets, "hscale_ac_brightness", hasLCD);
	gpm_gtk_set_visibility (prefwidgets, "label_ac_brightness", hasLCD);
	gpm_gtk_set_visibility (prefwidgets, "hscale_batteries_brightness", hasLCD);
	gpm_gtk_set_visibility (prefwidgets, "label_batteries_brightness", hasLCD);

	gpm_gtk_set_visibility (prefwidgets, "hscale_ac_display", hasDisplays);
	gpm_gtk_set_visibility (prefwidgets, "label_ac_display", hasDisplays);
	gpm_gtk_set_visibility (prefwidgets, "hscale_batteries_display", hasDisplays & hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "label_batteries_display", hasDisplays & hasBatteries);

	/* set the display stuff to set gnome-screensaver dpms timeout */
	gpm_gtk_set_visibility (prefwidgets, "hscale_ac_display", hasDisplays);
	gpm_gtk_set_visibility (prefwidgets, "label_ac_display", hasDisplays);
	gpm_gtk_set_visibility (prefwidgets, "hscale_batteries_display", hasDisplays & hasBatteries);
	gpm_gtk_set_visibility (prefwidgets, "label_batteries_display", hasDisplays & hasBatteries);
}

/** Callback for gconf_key_changed
 *
 * @param	client		A valid GConfClient
 * @param	cnxn_id		Unknown
 * @param	entry		The key that was modified
 * @param	user_data	user_data pointer. No function.
 */
static void
callback_gconf_key_changed (GConfClient *client,
	guint cnxn_id,
	GConfEntry *entry,
	gpointer user_data)
{
	if (gconf_entry_get_value (entry) == NULL)
		return;
}

/** Callback for combo_changed
 *
 * @param	widget		The combobox widget
 * @param	user_data	user_data pointer. No function.
 *
 * @note	We get the following data from widget: policypath, policydata
 */
static void
callback_combo_changed (GtkWidget *widget, gpointer user_data)
{
	GConfClient *client = NULL;
	gint value;
	gchar *policypath = NULL;
	gchar *policyoption = NULL;
	GPtrArray *policydata = NULL;
	gint *pdata = NULL;

	client = gconf_client_get_default ();
	value = gtk_combo_box_get_active(GTK_COMBO_BOX (widget));
	policypath = g_object_get_data ((GObject*) widget, "policypath");
	policydata = g_object_get_data ((GObject*) widget, "policydata");

	/* we have to convert from the virtual mapping to a policy mapping */
	pdata = (gint*) g_ptr_array_index (policydata, value);

	g_debug ("[%s] = (%i)", policypath, *pdata);

	/* we have to convert to the gconf store string */
	policyoption = convert_policy_to_string (*pdata);
	gconf_client_set_string (client, policypath, policyoption, NULL);
}

/** Gets the battery percentage time for the number of minutes
 *
 * @param	value		The number of minutes we would last
 * @return			The time we would last (in minutes) for the
 *				percentage battery charge. 0 for invalid.
 *
 * @note	Only takes into accound first battery, as this is an estimate.
 */
gint
get_battery_time_for_percentage (gint value)
{
	/**	@bug	Need to use HAL to get battery */
	gchar *udi = "/org/freedesktop/Hal/devices/acpi_BAT1";
	gint percentage;
	gint time;
	gboolean discharging;

	hal_device_get_bool (udi, "battery.rechargeable.is_discharging", &discharging);

	/* rate information is useless when charging */
	if (!discharging)
		return 0;

	/* get values. if they are wrong, return 0 */
	hal_device_get_int (udi, "battery.charge_level.percentage", &percentage);
	hal_device_get_int (udi, "battery.remaining_time", &time);

	if (time > 0 && percentage > 0) {
		time = time / 60;
		return (value * ((double) time / (double) percentage));
	}
	return 0;
}

/** Set the "Estimated" battery string widget
 *
 * @param	widget		The widget we want to change
 * @param	value		The number of minutes we would last
 *
 * @note	Return value is "Estimated 6 minutes" in italic.
 */
static void
set_estimated_label_widget (GtkWidget *widget, gint value)
{
	gchar *timestring;
	gchar *estimated;

	/* assertion checks */
	g_assert (widget);

	if (value > 1) {
		timestring = get_timestring_from_minutes (value);
		estimated = g_strdup_printf ("<i>Estimated %s</i>", timestring);
		gtk_widget_show_all (GTK_WIDGET (widget));
		gtk_label_set_markup (GTK_LABEL (widget), estimated);
		g_free (timestring);
		g_free (estimated);
	} else {
		/* hide if no valid number */
		gtk_widget_hide_all (GTK_WIDGET (widget));
	}
}

/** Callback when critical or low level are changed
 *
 * @param	widget		The combobox widget
 * @param	user_data	user_data pointer. No function.
 *
 * @note	We get the following data from widget: policypath
 */
static void
callback_hscale_low_critical_level_changed (GtkWidget *widget, gpointer user_data)
{
	GConfClient *client = NULL;
	const gchar *widgetname = NULL;
	gchar *policypath = NULL;
	gdouble value;
	gint timepercentage;
	GtkWidget* widget2;

	policypath = g_object_get_data ((GObject*) widget, "policypath");
	g_return_if_fail (policypath);

	value = gtk_range_get_value (GTK_RANGE (widget));
	g_debug ("[%s] = (%f)", policypath, value);

	timepercentage = get_battery_time_for_percentage (value);

	widgetname = gtk_widget_get_name (widget);
	if (strcmp (widgetname, "hscale_battery_low") == 0) {
		widget2 = glade_xml_get_widget (prefwidgets,
			"hscale_battery_critical");
		/* Make sure that critical level can never be below low level */
		gtk_range_set_range (GTK_RANGE (widget2), 0, value);

		widget2 = glade_xml_get_widget (prefwidgets, "label_battery_low_estimate");
		set_estimated_label_widget (widget2, timepercentage);
	}
	else if (strcmp (widgetname, "hscale_battery_critical") == 0) {
		widget2 = glade_xml_get_widget (prefwidgets, "label_battery_critical_estimate");
		set_estimated_label_widget (widget2, timepercentage);
	}
	else {
		g_critical ("callback_hscale_low_critical_level_changed() widget: %s does not exist",
			    widgetname);
	}

	client = gconf_client_get_default ();
	gconf_client_set_int (client, policypath, (gint) value, NULL);
}


/** Callback when brightness is changed
 *
 * @param	widget		The combobox widget
 * @param	user_data	user_data pointer. No function.
 *
 * @note	We get the following data from widget: policypath
 */
static void
callback_hscale_brightness_changed (GtkWidget *widget, gpointer user_data)
{
	GConfClient *client = NULL;
	const gchar *widgetname = NULL;
	gchar *policypath = NULL;
	gdouble value;
	gboolean onbattery;

	policypath = g_object_get_data ((GObject*) widget, "policypath");
	g_return_if_fail (policypath);

	value = gtk_range_get_value (GTK_RANGE (widget));
	g_debug ("[%s] = (%f)", policypath, value);

	widgetname = gtk_widget_get_name (widget);
	/* Change the brightness in real-time */
	if (gpm_is_on_mains (&onbattery)) {
		if ((!onbattery && strcmp (widgetname, "hscale_ac_brightness") == 0) ||
		    (onbattery && strcmp (widgetname, "hscale_batteries_brightness") == 0)) {
			hal_set_brightness (value);
		}
	} else {
		g_warning (GPM_DBUS_SERVICE ".isOnBattery failed");
	}

	client = gconf_client_get_default ();
	gconf_client_set_int (client, policypath, (gint) value, NULL);
}


/** Callback when sleep time sliders are changed
 *
 * @param	widget		The combobox widget
 * @param	user_data	user_data pointer. No function.
 *
 * @note	We get the following data from widget: policypath
 */

static void
callback_hscale_sleep_time_changed (GtkWidget *widget, gpointer user_data)
{
	GConfClient *client = NULL;
	gchar *policypath = NULL;
	gdouble value;

	policypath = g_object_get_data ((GObject*) widget, "policypath");
	g_return_if_fail (policypath);
	
	value = gtk_range_get_value (GTK_RANGE (widget));
	g_debug ("[%s] = (%f)", policypath, value);
	
	if ( (gint)value == NEVER_TIME_ON_SLIDER) {
		value = 0; /* gnome power manager interprets 0 as Never */
	}
	client = gconf_client_get_default ();
	gconf_client_set_int (client, policypath, (gint) value, NULL);
}


/** Callback for button_help
 *
 * @param	widget		Unused
 * @param	user_data	Unused
 */
static void
callback_help (GtkWidget *widget, gpointer user_data)
{
	/* for now, show website */
	gnome_url_show (GPMURL, NULL);
}

/** Callback for the changed signals for the checkboxes
 *
 * @param	widget		The checkbox widget
 * @param	user_data	Unused
 *
 * @note	We get the following data from widget: policypath
 */
static void
callback_check_changed (GtkWidget *widget, gpointer user_data)
{
	const gchar *widgetname;
	GConfClient *client = gconf_client_get_default ();
	gboolean value;

	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	widgetname = gtk_widget_get_name (widget);
	if (strcmp (widgetname, "checkbutton_require_password") == 0)
		gconf_client_set_bool (client, 
			GCONF_ROOT "general/require_password", value, NULL);
}

/** Callback for the changed signals for the radio buttons
 *
 * @param	widget		The checkbox widget
 * @param	user_data	Unused
 *
 * @note	We get the following data from widget: policypath
 */
static void
callback_radio_changed (GtkWidget *widget, gpointer user_data)
{
	GConfClient *client = NULL;
	gchar *policy = NULL;
	const gchar *widgetname;

	widgetname = gtk_widget_get_name (widget);

	if (strcmp (widgetname, "radiobutton_icon_never") == 0)
		policy = "never";
	else if (strcmp (widgetname, "radiobutton_icon_critical") == 0)
		policy = "critical";
	else if (strcmp (widgetname, "radiobutton_icon_charge") == 0)
		policy = "charge";
	else if (strcmp (widgetname, "radiobutton_icon_always") == 0)
		policy = "always";
	else
		g_error ("callback_radio_changed trucked up");

	client = gconf_client_get_default ();
	gconf_client_set_string (client, 
		GCONF_ROOT "general/display_icon_policy", policy, NULL);
}

/** simple callback formatting a GtkScale to "XX%"
 *
 *
 * @param	scale		The GTK scale slider
 * @param	value		The value we want to format
 * @return			The completed string, e.g. "10%"
 */
static gchar*
format_value_callback_low_critical_level (GtkScale *scale, gdouble value)
{
	return g_strdup_printf ("%i%%", (gint) value);
}

/** callback formatting a GtkScale to brightness levels
 *
 * @param	scale		The GTK scale slider
 * @param	value		The value we want to format
 * @return			The completed string, e.g. "10%"
 *
 * @note	We get the following data from widget: lcdsteps
 */
static gchar*
format_value_callback_brightness (GtkScale *scale, gdouble value)
{
	int *steps = NULL;

	steps = g_object_get_data ((GObject*) GTK_WIDGET (scale), "lcdsteps");
	if (!steps)
		return NULL;
	return g_strdup_printf ("%i%%", (gint) value * 100 / (*steps - 1));
}

/** simple callback that converts minutes into pretty text
 *
 * @param	scale		The GTK scale slider
 * @param	value		The value we want to format
 * @return			The completed string, e.g. "10%"
 */
static gchar*
format_value_callback_time (GtkScale *scale, gdouble value)
{
	if ((gint) value == NEVER_TIME_ON_SLIDER)
		return g_strdup ("Never");

	return get_timestring_from_minutes (value);
}

/** Sets the hscales up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the *full* GConf policy path,
 *				e.g. "/apps/g-p-m/policy/ac/brightness"
 *  @param  policytype		the policy type, e.g. POLICY_PERCENT
 */
static void
hscale_setup_action (const gchar *widgetname, const gchar *policypath, PolicyType policytype)
{
	GConfClient *client = NULL;
	GtkWidget *widget = NULL;
	gint value;

	/* assertion checks */
	g_assert (widgetname);
	g_assert (policypath);

	client = gconf_client_get_default ();
	widget = glade_xml_get_widget (prefwidgets, widgetname);

	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);
	g_object_set_data ((GObject*) widget, "policytype", (gpointer) policytype);

	value = gconf_client_get_int (client, policypath, NULL);
	g_debug ("'%s' -> [%s] = (%i)", widgetname, policypath, value);

	if (policytype == POLICY_LCD) {
		g_signal_connect (G_OBJECT (widget), "format-value",
			G_CALLBACK (format_value_callback_brightness), NULL);
		g_signal_connect (G_OBJECT (widget), "value-changed",
			G_CALLBACK (callback_hscale_brightness_changed), NULL);
	}
	else if (policytype == POLICY_PERCENT) {
		g_signal_connect (G_OBJECT (widget), "format-value",
			G_CALLBACK (format_value_callback_low_critical_level), NULL);
		g_signal_connect (G_OBJECT (widget), "value-changed",
			G_CALLBACK (callback_hscale_low_critical_level_changed), NULL);
	}
	else if (policytype = POLICY_TIME) {
		if (value == 0) {
			value = NEVER_TIME_ON_SLIDER;
		}
		g_signal_connect (G_OBJECT (widget), "format-value",
			G_CALLBACK (format_value_callback_time), NULL);
		g_signal_connect (G_OBJECT (widget), "value-changed",
			G_CALLBACK (callback_hscale_sleep_time_changed), NULL);
	}
	else {
		g_assert (FALSE);
	}
	
	gtk_range_set_value (GTK_RANGE (widget), (int) value);
}

/** Sets the checkboxes up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 */
static void
radiobutton_setup_action (const gchar *widgetname)
{
	GtkWidget *widget = NULL;

	widget = glade_xml_get_widget (prefwidgets, widgetname);
	g_signal_connect (G_OBJECT (widget), "clicked",
		G_CALLBACK (callback_radio_changed), NULL);
}

/** Sets the comboboxes up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the GConf policy path,
 *				e.g. "policy/ac/brightness"
 *  @param  ptrarray		the policy data, in a pointer array
 */
static void
combo_setup_dynamic (const gchar *widgetname,
	const gchar *policypath,
	GPtrArray *ptrarray)
{
	GConfClient *client = NULL;
	GtkWidget *widget = NULL;
	gchar *policyoption = NULL;
	gint value;
	gint a;
	gint *pdata = NULL;

	/* assertion checks */
	g_assert (widgetname);
	g_assert (policypath);
	g_assert (ptrarray);

	client = gconf_client_get_default ();
	widget = glade_xml_get_widget (prefwidgets, widgetname);
	g_return_if_fail (widget);

	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);
	g_object_set_data ((GObject*) widget, "policydata", (gpointer) ptrarray);

	/* add text to combo boxes in order of parray */
	for (a=0;a < ptrarray->len;a++) {
		pdata = (gint*) g_ptr_array_index (ptrarray, a);
		if (*pdata == ACTION_SHUTDOWN)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
				_("Shutdown"));
		else if (*pdata == ACTION_SUSPEND)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
				_("Suspend"));
		else if (*pdata == ACTION_HIBERNATE)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
				_("Hibernate"));
		else if (*pdata == ACTION_WARNING)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
				_("Send warning"));
		else if (*pdata == ACTION_NOTHING)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
				_("Do nothing"));
		else
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
				"Unknown");
	}

	/* we have to get the gconf string, and convert it into a policy option */
	policyoption = gconf_client_get_string (client, policypath, NULL);
	if (!policyoption) {
		g_warning ("Cannot find %s, maybe a bug in the gconf schema!", policyoption);
		return;
	}

	/* select the correct entry, i.e. map the policy to virtual mapping */
	value = convert_string_to_policy (policyoption);
	g_free (policyoption);
	for (a=0;a < ptrarray->len;a++) {
		pdata = (int*) g_ptr_array_index (ptrarray, a);
		if (*pdata == value) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), a);
			break;
		}
	}

	/* connect this signal up to the genric changed box */
	g_signal_connect (G_OBJECT (widget), "changed",
		G_CALLBACK (callback_combo_changed), NULL);
}

/** Main entry point
 *
 *  @param	argc		Number of arguments given to program
 *  @param	argv		Arguments given to program
 *  @return			Return code
 */
int
main (int argc, char **argv)
{
	GtkWidget *widget = NULL;
	GConfClient *client = NULL;
	gint i;
	gboolean has_gpm_connection;
	gdouble value;
	gint steps;
	GtkSizeGroup *size_group;

	/* provide dynamic storage for comboboxes */
	GPtrArray *ptrarr_button_power = NULL;
	GPtrArray *ptrarr_button_suspend = NULL;
	GPtrArray *ptrarr_button_lid = NULL;
	GPtrArray *ptrarr_battery_critical = NULL;
	GPtrArray *ptrarr_sleep_type = NULL;

	/* provide pointers to enums */
	int pSuspend 	= 	ACTION_SUSPEND;
	int pShutdown 	= 	ACTION_SHUTDOWN;
	int pHibernate 	= 	ACTION_HIBERNATE;
	int pNothing 	= 	ACTION_NOTHING;

	struct poptOption options[] = {
		{ "verbose", '\0', POPT_ARG_NONE, NULL, 0,
		  N_("Show extra debugging information"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	i = 0;
	options[i++].arg = &isVerbose;

	isVerbose = FALSE;

	/* initialise gnome */
	gnome_program_init (argv[0], VERSION,
			    LIBGNOMEUI_MODULE, 
			    argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("GNOME Power Preferences"),
			    NULL);

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gtk_window_set_default_icon_name ("gnome-dev-battery");

	if (!isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			g_log_ignore, NULL);

	/* Get the GconfClient, tell it we want to monitor /apps/gnome-power */
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, GCONF_ROOT_SANS_SLASH, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (client, GCONF_ROOT_SANS_SLASH, 
		callback_gconf_key_changed, widget, NULL, NULL);
	/*
	 * we add this even if it doesn't exist as gnome-screensaver might be 
	 * installed when g-p-m is running
	 */
	gconf_client_notify_add (client, GS_GCONF_ROOT_NO_SLASH,
		callback_gconf_key_changed, widget, NULL, NULL);

	/* Initialise libnotify, if compiled in. */
	if (!libnotify_init (NICENAME))
		g_error ("Cannot initialise libnotify!");

	/* check if we have GNOME Screensaver, but have disabled dpms */
	if (gpm_screensaver_is_running ())
		if (!gconf_client_get_bool (client, GS_GCONF_ROOT "dpms_enabled", NULL)) {
			g_warning ("You have not got DPMS support enabled"
					 "in gnome-screensaver.\n"
					 "GNOME Power Manager will enable it now.");
			gconf_client_set_bool (client, 
				GS_GCONF_ROOT "dpms_enabled", TRUE, NULL);
		}

	/* load the interface */
	prefwidgets = glade_xml_new (GPM_DATA G_DIR_SEPARATOR_S "gpm-prefs.glade", NULL, NULL);
	if (!prefwidgets)
		g_error ("glade file failed to load, aborting");

	/* Get the main_window quit */
	widget = glade_xml_get_widget (prefwidgets, "window_preferences");
	if (!widget)
		g_error ("Main window failed to load, aborting");
	g_signal_connect (G_OBJECT (widget), "delete_event",
		G_CALLBACK (gtk_main_quit), NULL);
	/*
	 * We should warn if g-p-m is not running - but still allow to continue
	 * Note, that the query alone will be enough to lauch g-p-m using
	 * the service file.
	 */
	has_gpm_connection = gpm_is_on_ac (&i);
	if (!has_gpm_connection)
		g_warning ("Cannot connect to GNOME Power Manager.\n"
			"Make sure that it is running");

	/* Set the button callbacks */
	widget = glade_xml_get_widget (prefwidgets, "button_close");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gtk_main_quit), NULL);
	widget = glade_xml_get_widget (prefwidgets, "button_help");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (callback_help), NULL);

	/* set up the checkboxes */
	widget = glade_xml_get_widget (prefwidgets, "checkbutton_require_password");
	g_signal_connect (G_OBJECT (widget), "clicked",
		G_CALLBACK (callback_check_changed), NULL);

	/* set gtk enables/disables */
	recalc ();

	/* radioboxes */
	radiobutton_setup_action ("radiobutton_icon_always");
	radiobutton_setup_action ("radiobutton_icon_charge");
	radiobutton_setup_action ("radiobutton_icon_critical");
	radiobutton_setup_action ("radiobutton_icon_never");

	/* Make sure that all comboboxes get the same size by adding their labels
           to a GtkSizeGroup  */         
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);	
	gtk_size_group_add_widget (size_group, glade_xml_get_widget (prefwidgets, "label_sleep_type")); 
	gtk_size_group_add_widget (size_group, glade_xml_get_widget (prefwidgets, "label_button_power"));
	gtk_size_group_add_widget (size_group, glade_xml_get_widget (prefwidgets, "label_button_suspend"));
	gtk_size_group_add_widget (size_group, glade_xml_get_widget (prefwidgets, "label_button_lid"));
	gtk_size_group_add_widget (size_group, glade_xml_get_widget (prefwidgets, "label_battery_critical"));
	g_object_unref (G_OBJECT (size_group)); 

	/*
	 * Set up combo boxes with "ideal" values - if a enum is unavailable
	 * e.g. hibernate has been disabled, then it will be filtered out
	 * automatically.
	 */

	/* sleep type */
	ptrarr_sleep_type = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_sleep_type, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_sleep_type, (gpointer) &pHibernate);
	combo_setup_dynamic ("combobox_sleep_type",
		GCONF_ROOT "policy/sleep_type", ptrarr_sleep_type);

	/* power button */
	ptrarr_button_power = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pHibernate);
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pShutdown);
	combo_setup_dynamic ("combobox_button_power", 
		GCONF_ROOT "policy/button_power", ptrarr_button_power);

	/* sleep button */
	ptrarr_button_suspend = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_button_suspend, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_button_suspend, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_button_suspend, (gpointer) &pHibernate);
	combo_setup_dynamic ("combobox_button_suspend",
		GCONF_ROOT "policy/button_suspend", ptrarr_button_power);

	/* lid "button" */
	ptrarr_button_lid = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_button_lid, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_button_lid, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_button_lid, (gpointer) &pHibernate);
	combo_setup_dynamic ("combobox_button_lid",
		GCONF_ROOT "policy/button_lid", ptrarr_button_lid);

	/* battery critical */
	ptrarr_battery_critical = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pHibernate);
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pShutdown);
	combo_setup_dynamic ("combobox_battery_critical",
		GCONF_ROOT "policy/battery_critical", ptrarr_battery_critical);

	/* sliders */
	hscale_setup_action ("hscale_ac_computer",
		GCONF_ROOT "policy/ac/sleep_computer", POLICY_TIME);
	hscale_setup_action ("hscale_ac_display",
		GCONF_ROOT "policy/ac/sleep_display", POLICY_TIME);
	hscale_setup_action ("hscale_ac_brightness",
		GCONF_ROOT "policy/ac/brightness", POLICY_LCD);
	hscale_setup_action ("hscale_batteries_computer",
		GCONF_ROOT "policy/battery/sleep_computer", POLICY_TIME);
	hscale_setup_action ("hscale_batteries_display",
		GCONF_ROOT "policy/battery/sleep_display", POLICY_TIME);
	hscale_setup_action ("hscale_batteries_brightness",
		GCONF_ROOT "policy/battery/brightness", POLICY_LCD);
	hscale_setup_action ("hscale_battery_low",
		GCONF_ROOT "general/threshold_low", POLICY_PERCENT);
	hscale_setup_action ("hscale_battery_critical",
		GCONF_ROOT "general/threshold_critical", POLICY_PERCENT);

	/* set up upper limit for battery_critical */
	widget = glade_xml_get_widget (prefwidgets, "hscale_battery_low");
	gtk_range_set_range (GTK_RANGE (widget), 0, 25);
	value = gtk_range_get_value (GTK_RANGE (widget));

	widget = glade_xml_get_widget (prefwidgets, "hscale_battery_critical");
	if (value > 0)
		gtk_range_set_range (GTK_RANGE (widget), 0, value);

	/* set the top end for LCD sliders */
	if (hal_get_brightness_steps (&steps)) {
		/* only set steps if we have LCD device */
		widget = glade_xml_get_widget (prefwidgets,
			"hscale_ac_brightness");
		gtk_range_set_range (GTK_RANGE (widget), 0, steps - 1);
		g_object_set_data ((GObject*) widget, "lcdsteps", (gpointer) &steps);

		widget = glade_xml_get_widget (prefwidgets,
			"hscale_batteries_brightness");
		gtk_range_set_range (GTK_RANGE (widget), 0, steps - 1);
		g_object_set_data ((GObject*) widget, "lcdsteps", (gpointer) &steps);
	}

	/* set themed battery and ac_adapter icons */
	widget = glade_xml_get_widget (prefwidgets, "image_side_battery");
	gtk_image_set_from_icon_name (GTK_IMAGE(widget), "gnome-dev-battery", GTK_ICON_SIZE_DIALOG);
	widget = glade_xml_get_widget (prefwidgets, "image_side_acadapter");
	gtk_image_set_from_icon_name (GTK_IMAGE(widget), "gnome-fs-socket", GTK_ICON_SIZE_DIALOG);

	/* main loop */
	gtk_main ();
	g_ptr_array_free (ptrarr_button_power, TRUE);
	g_ptr_array_free (ptrarr_button_suspend, TRUE);
	g_ptr_array_free (ptrarr_button_lid, TRUE);
	g_ptr_array_free (ptrarr_battery_critical, TRUE);
	g_ptr_array_free (ptrarr_sleep_type, TRUE);
	return 0;
}
/** @} */
