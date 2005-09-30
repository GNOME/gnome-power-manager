/***************************************************************************
 *
 * gpm-prefs.c : GNOME Power Preferences
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 *
 * Taken in part from:
 * - GNOME Tutorial, example 1
 * http://www.gnome.org/~newren/tutorials/developing-with-gnome/html/apd.html
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

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

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-libnotify.h"
#include "gpm-main.h"
#include "gpm-screensaver.h"

#include "dbus-common.h"
#include "glibhal-main.h"
#include "glibhal-extras.h"

static GladeXML *all_pref_widgets;
static gboolean isVerbose;

/** Finds out if we are running on AC
 *
 *  @param  value		The return value, passed by ref
 *  @return			Success
 */
gboolean
gpm_is_on_ac (gboolean *value)
{
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gpm_proxy = NULL;
	GError *error = NULL;
	gboolean retval = TRUE;

	/* assertion checks */
	g_assert (value);

	dbus_get_session_connection (&session_connection);
	gpm_proxy = dbus_g_proxy_new_for_name (session_connection,
			GPM_DBUS_SERVICE,
			GPM_DBUS_PATH,
			GPM_DBUS_INTERFACE);
	if (!dbus_g_proxy_call (gpm_proxy, "isOnAc", &error,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, value, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (gpm_proxy));
	return retval;
}

/** queries org.gnome.GnomePowerManager.isOnBattery
 *
 *  @param  value	return value, passed by ref
 *  @return		TRUE for success, FALSE for failure
 */
gboolean
gpm_is_on_mains (gboolean *value)
{
	DBusGConnection *session_connection = NULL;
	DBusGProxy *gpm_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	/* assertion checks */
	g_assert (value);

	dbus_get_session_connection (&session_connection);
	gpm_proxy = dbus_g_proxy_new_for_name (session_connection,
			GPM_DBUS_SERVICE, GPM_DBUS_PATH, GPM_DBUS_INTERFACE);
	retval = TRUE;
	if (!dbus_g_proxy_call (gpm_proxy, "isOnBattery", &error,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, value, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (gpm_proxy));
	return retval;
}

/** Sets/Hides GTK visibility
 *
 *  @param  widgetname		the libglade widget name
 *  @param  set			should widget be visible?
 */
static void
gtk_set_visibility (const char *widgetname, gboolean set)
{
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (widgetname);

	widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	if (!widget) {
		g_warning ("gtk_set_visibility: widget '%s' not found", widgetname);
		return;
	}

	if (set)
		gtk_widget_show_all (widget);
	else
		gtk_widget_hide_all (widget);
}

/** Sets/Clears GTK Checkbox
 *
 *  @param  widgetname		the libglade widget name
 *  @param  set			should check be ticked?
 */
static void
gtk_set_check (const char *widgetname, gboolean set)
{
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (widgetname);

	widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	if (!widget) {
		g_warning ("widget '%s' failed to load, aborting", widgetname);
		return;
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), set);
}

/** Modifies a GTK Label
 *
 *  @param  widgetname		the libglade widget name
 *  @param  label		The new text
 */
static void
gtk_set_label (const char *widgetname, const gchar *label)
{
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (widgetname);

	widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	gtk_label_set_markup (GTK_LABEL (widget), label);
}

/** Shows/hides/renames controls based on hasData, i.e. what hardware is in the system.
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

	/* checkboxes */
	gboolean displayIcon = gconf_client_get_bool (client, GCONF_ROOT "general/display_icon", NULL);
	gboolean displayIconFull = gconf_client_get_bool (client, GCONF_ROOT "general/display_icon_full", NULL);
	gtk_set_check ("checkbutton_display_icon", displayIcon);
	gtk_set_check ("checkbutton_display_icon_full", displayIconFull);

	hasBatteries =   (hal_num_devices_of_capability ("battery") > 0);
	hasAcAdapter =   (hal_num_devices_of_capability ("ac_adapter") > 0);
	hasButtonPower = (hal_num_devices_of_capability_with_value ("button", "button.type", "power") > 0);
	hasButtonSleep = (hal_num_devices_of_capability_with_value ("button", "button.type", "sleep") > 0);
	hasButtonLid =   (hal_num_devices_of_capability_with_value ("button", "button.type", "lid") > 0);
#if 0
	hasHardDrive =   (hal_num_devices_of_capability_with_value ("storage", "storage.bus", "ide") > 0);
#endif
	hasLCD = 	 (hal_num_devices_of_capability ("laptop_panel") > 0);

	if (gscreensaver_is_running ()) {
		gtk_set_visibility ("button_gnome_screensave", TRUE);
		hasDisplays = gconf_client_get_bool (client, "/apps/gnome-screensaver/dpms_enabled", NULL);
	} else {
		gtk_set_visibility ("button_gnome_screensave", FALSE);
		hasDisplays = FALSE;
	}

	/* frame labels */
	if (hasBatteries) {
		widget = glade_xml_get_widget (all_pref_widgets, "label_frame_ac");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Running on AC adapter</b>");
		widget = glade_xml_get_widget (all_pref_widgets, "label_frame_batteries");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Running on batteries</b>");
	} else {
		widget = glade_xml_get_widget (all_pref_widgets, "label_frame_ac");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Configuration</b>");
	}

	/* top frame */
	gtk_set_visibility ("frame_batteries", hasBatteries);
	gtk_set_visibility ("combobox_battery_critical", hasBatteries);
	gtk_set_visibility ("label_battery_critical_action", hasBatteries);
	gtk_set_visibility ("label_battery_critical", hasBatteries);
	gtk_set_visibility ("label_battery_low", hasBatteries);
	gtk_set_visibility ("hscale_battery_low", hasBatteries);
	gtk_set_visibility ("hscale_battery_critical", hasBatteries);
	/* assumes only battery options are in this frame */
	gtk_set_visibility ("frame_other_options", hasBatteries);

	/* options */
	gtk_set_visibility ("combobox_button_lid", hasButtonLid);
	gtk_set_visibility ("label_button_lid", hasButtonLid);

	gtk_set_visibility ("combobox_button_power", hasButtonPower);
	gtk_set_visibility ("label_button_power", hasButtonPower);

	gtk_set_visibility ("combobox_button_suspend", hasButtonSleep);
	gtk_set_visibility ("label_button_suspend", hasButtonSleep);

	gtk_set_visibility ("combobox_ac_fail", hasAcAdapter);
	gtk_set_visibility ("label_ac_fail", hasAcAdapter);

	/* variables */
	gtk_set_visibility ("hscale_ac_brightness", hasLCD);
	gtk_set_visibility ("label_ac_brightness", hasLCD);
	gtk_set_visibility ("hscale_batteries_brightness", hasLCD);
	gtk_set_visibility ("label_batteries_brightness", hasLCD);

	gtk_set_visibility ("hscale_ac_display", hasDisplays);
	gtk_set_visibility ("label_ac_display", hasDisplays);
	gtk_set_visibility ("hscale_batteries_display", hasDisplays & hasBatteries);
	gtk_set_visibility ("label_batteries_display", hasDisplays & hasBatteries);

	/* set the display stuff to set gnome-screensaver dpms timeout */
	gtk_set_visibility ("hscale_ac_display", hasDisplays);
	gtk_set_visibility ("label_ac_display", hasDisplays);
	gtk_set_visibility ("hscale_batteries_display", hasDisplays & hasBatteries);
	gtk_set_visibility ("label_batteries_display", hasDisplays & hasBatteries);
}

/** Callback for gconf_key_changed
 *
 */
static void
callback_gconf_key_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	/* assertion checks */
	g_assert (client);

	if (gconf_entry_get_value (entry) == NULL)
		return;

	/*
	 * just recalculate the UI, as the gconf keys are read there.
	 * this removes the need for lots of global variables.
	 */
#if 0
	recalc ();
#endif
}

/** Callback for combo_changed
 *
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

	/* assertion checks */
	g_assert (widget);

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

/** Callback for hscale_changed
 *
 */
static void
callback_hscale_changed (GtkWidget *widget, gpointer user_data)
{
	GConfClient *client = NULL;
	const gchar *widgetname = NULL;
	gchar *policypath = NULL;
	gdouble value;
	gint oldgconfvalue;
	gdouble divisions = -1;
	gboolean onbattery;

	/* assertion checks */
	g_assert (widget);

	client = gconf_client_get_default ();
	policypath = g_object_get_data ((GObject*) widget, "policypath");

	value = gtk_range_get_value (GTK_RANGE (widget));
	oldgconfvalue = gconf_client_get_int (client, policypath, NULL);

	/*
	 * Code for divisions of 5 seconds
	 */
	widgetname = gtk_widget_get_name (widget);
	if (strcmp (widgetname, "hscale_ac_computer") == 0)
		divisions = 5;
	else if (strcmp (widgetname, "hscale_batteries_computer") == 0)
		divisions = 5;

	if (divisions > 0) {
		double double_segment = ((gdouble) value / divisions);
		double v2 = ceil(double_segment) * divisions;
		gtk_range_set_value (GTK_RANGE (widget), v2);
		/* if not different, then we'll return, and wait for the proper event */
		if ((int) v2 != (int) value)
			return;
	}

	/*
	 * if calculated value not substantially different to existing
	 * gconf value, then no point continuing
	 */
	if (fabs (oldgconfvalue - value) < 0.1)
		return;

	/* if this is hscale for battery_low, then set upper range of hscale for
	 * battery_critical maximum to value
	 * (This stops criticalThreshold > lowThreshold)
	 */
	if (strcmp (widgetname, "hscale_battery_low") == 0) {
		GtkWidget *widget2;
		widget2 = glade_xml_get_widget (all_pref_widgets, "hscale_battery_critical");
		gtk_range_set_range (GTK_RANGE (widget2), 0, value);
	}

	/* for AC and battery, change the brightness in real-time */
	if (gpm_is_on_mains (&onbattery)) {
		if ((!onbattery && strcmp (widgetname, "hscale_ac_brightness") == 0) ||
		    (onbattery && strcmp (widgetname, "hscale_batteries_brightness") == 0))
			hal_set_brightness (value);
	} else
		g_warning (GPM_DBUS_SERVICE ".isOnBattery failed");

	g_return_if_fail (policypath);
	g_debug ("[%s] = (%f)", policypath, value);
	gconf_client_set_int (client, policypath, (gint) value, NULL);
}

/** Callback for button_help
 *
 */
static void
callback_help (GtkWidget *widget, gpointer user_data)
{
	/* for now, show website */
	gnome_url_show (GPMURL, NULL);
}

/** Callback for button_gnome_screensave
 *
 */
static void
callback_screensave (GtkWidget *widget, gpointer user_data)
{
	gboolean retval;
	retval = g_spawn_command_line_async ("gnome-screensaver-preferences", NULL);
	if (!retval)
		g_warning ("Couldn't execute gnome-screensaver-preferences");
}

/** Callback for check_changed
 *
 */
static void
callback_check_changed (GtkWidget *widget, gpointer user_data)
{
	GConfClient *client = NULL;
	gboolean value;
	gchar *policypath = NULL;

	/* assertion checks */
	g_assert (widget);

	client = gconf_client_get_default ();
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	policypath = g_object_get_data ((GObject*) widget, "policypath");
	g_return_if_fail (policypath);
	/*int policytype = (int) g_object_get_data ((GObject*) widget, "policytype");*/

	g_debug ("[%s] = (%i)", policypath, value);
	gconf_client_set_bool (client, policypath, value, NULL);
}

/** Prints program usage.
 *
 */
static void
print_usage (void)
{
	g_print ("\nusage : gnome-power-preferences [--verbose] [--help]\n");
	g_print (
		"\n"
		"        --verbose               Show extra debugging\n"
		"        --help                  Show this information and exit\n"
		"\n");
}

/** simple callback formatting a GtkScale to "10%"
 *
 */
static gchar*
format_value_callback_percent (GtkScale *scale, gdouble value)
{
	/* assertion checks */
	g_assert (scale);

	return g_strdup_printf ("%i%%", (gint) value);
}

/** callback formatting a GtkScale to brightness levels
 *
 */
static gchar*
format_value_callback_percent_lcd (GtkScale *scale, gdouble value)
{
	int *steps = NULL;

	/* assertion checks */
	g_assert (scale);

	steps = g_object_get_data ((GObject*) GTK_WIDGET (scale), "lcdsteps");
	if (!steps)
		return NULL;
	return g_strdup_printf ("%i%%", (gint) value * 100 / (*steps - 1));
}

/** simple callback that converts minutes into pretty text
 *
 */
static gchar*
format_value_callback_time (GtkScale *scale, gdouble value)
{
	gchar unitstring[32];
	GString *strvalue = NULL;

	/* assertion checks */
	g_assert (scale);

	if ((gint) value == 0)
		return g_strdup_printf ("Never");

	strvalue = get_timestring_from_minutes (value);
	strcpy (unitstring, strvalue->str);
	g_string_free (strvalue, TRUE);
	return g_strdup_printf ("%s", unitstring);
}

/** Sets the hscales up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the GConf policy path,
 *				e.g. "policy/ac/brightness"
 *  @param  policytype		the policy type, e.g. POLICY_PERCENT
 */
static void
hscale_setup_action (const char *widgetname, const char *policypath, int policytype)
{
	GConfClient *client = NULL;
	GtkWidget *widget = NULL;
	gint value;

	/* assertion checks */
	g_assert (widgetname);
	g_assert (policypath);

	client = gconf_client_get_default ();
	widget = glade_xml_get_widget (all_pref_widgets, widgetname);

	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);
	g_object_set_data ((GObject*) widget, "policytype", (gpointer) policytype);

	value = gconf_client_get_int (client, policypath, NULL);
	g_debug ("'%s' -> [%s] = (%i)", widgetname, policypath, value);

	if (policytype == POLICY_LCD)
		g_signal_connect (G_OBJECT (widget), "format-value", G_CALLBACK (format_value_callback_percent_lcd), NULL);
	else if (policytype == POLICY_PERCENT)
		g_signal_connect (G_OBJECT (widget), "format-value", G_CALLBACK (format_value_callback_percent), NULL);
	else
		g_signal_connect (G_OBJECT (widget), "format-value", G_CALLBACK (format_value_callback_time), NULL);
	gtk_range_set_value (GTK_RANGE (widget), (int) value);
	g_signal_connect (G_OBJECT (widget), "value-changed", G_CALLBACK (callback_hscale_changed), NULL);
}

/** Sets the checkboxes up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the GConf policy path,
 *				e.g. "policy/ac/brightness"
 */
static void
checkbox_setup_action (const char *widgetname, const char *policypath)
{
	GConfClient *client = NULL;
	GtkWidget *widget = NULL;
	gboolean value;

	/* assertion checks */
	g_assert (widgetname);
	g_assert (policypath);

	client = gconf_client_get_default ();
	widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (callback_check_changed), NULL);
	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);

	value = gconf_client_get_bool (client, policypath, NULL);
	g_debug ("'%s' -> [%s] = (%i)", widgetname, policypath, value);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

/** Sets the comboboxes up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the GConf policy path,
 *				e.g. "policy/ac/brightness"
 *  @param  ptrarray		the policy data, in a pointer array
 */
static void
combo_setup_dynamic (const char *widgetname, const char *policypath, GPtrArray *ptrarray)
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
	widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	g_return_if_fail (widget);

	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);
	g_object_set_data ((GObject*) widget, "policydata", (gpointer) ptrarray);

	/* add text to combo boxes in order of parray */
	for (a=0;a < ptrarray->len;a++) {
		pdata = (gint*) g_ptr_array_index (ptrarray, a);
		if (*pdata == ACTION_SHUTDOWN)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Shutdown"));
		else if (*pdata == ACTION_SUSPEND)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Suspend"));
		else if (*pdata == ACTION_HIBERNATE)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Hibernate"));
		else if (*pdata == ACTION_WARNING)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Send warning"));
		else if (*pdata == ACTION_NOTHING)
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Do nothing"));
		else
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Unknown");
	}

	/* we have to get the gconf string, and convert it into a policy option */
	policyoption = gconf_client_get_string (client, policypath, NULL);
	if (!policyoption) {
		g_warning ("Cannot find %s, maybe a bug in the gconf schema!", policyoption);
		return;
	}

	/* select the correct entry, i.e. map the policy to virtual mapping */
	value = convert_string_to_policy (policyoption);
	for (a=0;a < ptrarray->len;a++) {
		pdata = (int*) g_ptr_array_index (ptrarray, a);
		if (*pdata == value) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), a);
			break;
		}
	}

	/* connect this signal up to the genric changed box */
	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (callback_combo_changed), NULL);
}

void
refresh_info_page (void)
{
	gchar *returnstring;
	GtkWidget *widget = NULL;

	/* set vendor */
	if (hal_device_get_string ("/org/freedesktop/Hal/devices/computer",
				"smbios.system.manufacturer",
				&returnstring)) {
		gtk_set_label ("label_info_vendor", returnstring);
		g_free (returnstring);
	} else
		gtk_set_visibility ("label_info_vendor", FALSE);

	/* set model */
	if (hal_device_get_string ("/org/freedesktop/Hal/devices/computer",
				"smbios.system.product",
				&returnstring)) {
		gtk_set_label ("label_info_model", returnstring);
		g_free (returnstring);
	} else
		gtk_set_visibility ("label_info_model", FALSE);

	/* set formfactor */
	if (hal_device_get_string ("/org/freedesktop/Hal/devices/computer",
				"smbios.chassis.type",
				&returnstring)) {
		gtk_set_label ("label_info_formfactor", returnstring);
		g_free (returnstring);
	} else
		gtk_set_visibility ("label_info_formfactor", FALSE);

	/* Hardcoded for now */
	widget = glade_xml_get_widget (all_pref_widgets, "checkbutton_info_suspend");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	widget = glade_xml_get_widget (all_pref_widgets, "checkbutton_info_hibernate");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	widget = glade_xml_get_widget (all_pref_widgets, "checkbutton_info_cpufreq");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

	widget = glade_xml_get_widget (all_pref_widgets, "checkbutton_info_lowpowermode");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), hal_is_laptop ());

	/* TODO */
	gtk_set_visibility ("frame_info_batteries", FALSE);
	gtk_set_visibility ("frame_info_ups", FALSE);

}

/** Main program
 *
 */
int
main (int argc, char **argv)
{
	GtkWidget *widget = NULL;
	GConfClient *client = NULL;
	gint a;
	gboolean has_gpm_connection;
	gdouble value;
	gint steps;

	/* provide dynamic storage for comboboxes */
	GPtrArray *ptrarr_button_power = NULL;
	GPtrArray *ptrarr_button_suspend = NULL;
	GPtrArray *ptrarr_button_lid = NULL;
	GPtrArray *ptrarr_ac_fail = NULL;
	GPtrArray *ptrarr_battery_critical = NULL;
	GPtrArray *ptrarr_sleep_type = NULL;

	/* provide pointers to enums */
	int pSuspend 	= 	ACTION_SUSPEND;
	int pShutdown 	= 	ACTION_SHUTDOWN;
	int pHibernate 	= 	ACTION_HIBERNATE;
	int pNothing 	= 	ACTION_NOTHING;
	int pWarning 	= 	ACTION_WARNING;

	gtk_init (&argc, &argv);
	gconf_init (argc, argv, NULL);

	isVerbose = FALSE;
	for (a=1; a < argc; a++) {
		if (strcmp (argv[a], "--verbose") == 0)
			isVerbose = TRUE;
		else if (strcmp (argv[a], "--help") == 0) {
			print_usage ();
			return EXIT_SUCCESS;
		}
	}

	if (!isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, g_log_ignore, NULL);

	/* initialise gnome */
	gnome_program_init ("GNOME Power Preferences", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);

	/* Get the GconfClient, tell it we want to monitor /apps/gnome-power */
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, GCONF_ROOT_SANS_SLASH, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (client, GCONF_ROOT_SANS_SLASH, callback_gconf_key_changed, widget, NULL, NULL);
	/*
	 * we add this even if it doesn't exist as gnome-screensaver might be 
	 * installed when g-p-m is running
	 */
	gconf_client_notify_add (client, "/apps/gnome-screensaver", callback_gconf_key_changed, widget, NULL, NULL);

	/* Initialise libnotify, if compiled in. */
	if (!libnotify_init (NICENAME))
		g_error ("Cannot initialise libnotify!");

	/* check if we have GNOME Screensaver, but have disabled dpms */
	if (gscreensaver_is_running ())
		if (!gconf_client_get_bool (client, "/apps/gnome-screensaver/dpms_enabled", NULL)) {
			libnotify_event ("You have not got DPMS support enabled in gnome-screensaver. GNOME Power Manager will enable it now.", LIBNOTIFY_URGENCY_NORMAL, NULL);
			gconf_client_set_bool (client, "/apps/gnome-screensaver/dpms_enabled", TRUE, NULL);
		}

	/* load the interface */
	all_pref_widgets = glade_xml_new (GPM_DATA "preferences.glade", NULL, NULL);
	if (!all_pref_widgets)
		g_error ("glade file failed to load, aborting");

	/* Get the main_window quit */
	widget = glade_xml_get_widget (all_pref_widgets, "window_preferences");
	if (!widget)
		g_error ("Main window failed to load, aborting");
	g_signal_connect (G_OBJECT (widget), "delete_event", G_CALLBACK (gtk_main_quit), NULL);

	/*
	 * We should warn if g-p-m is not running - but still allow to continue
	 * Note, that the query alone will be enough to lauch g-p-m using
	 * the service file.
	 */
	has_gpm_connection = gpm_is_on_ac (&a);
	if (!has_gpm_connection)
		libnotify_event ("Cannot connect to GNOME Power Manager.\nMake sure that it is running", LIBNOTIFY_URGENCY_NORMAL, NULL);

	/* Set the button callbacks */
	widget = glade_xml_get_widget (all_pref_widgets, "button_close");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gtk_main_quit), NULL);
	widget = glade_xml_get_widget (all_pref_widgets, "button_help");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (callback_help), NULL);
	widget = glade_xml_get_widget (all_pref_widgets, "button_gnome_screensave");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (callback_screensave), NULL);

	/* set gtk enables/disables */
	recalc ();

	/* checkboxes */
	checkbox_setup_action ("checkbutton_display_icon", GCONF_ROOT "general/display_icon");
	checkbox_setup_action ("checkbutton_display_icon_full", GCONF_ROOT "general/display_icon_full");
	/*
	 * Set up combo boxes with "ideal" values - if a enum is unavailable
	 * e.g. hibernate has been disabled, then it will be filtered out
	 * automatically.
	 */
	/* power button */
	ptrarr_button_power = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pHibernate);
	g_ptr_array_add (ptrarr_button_power, (gpointer) &pShutdown);
	combo_setup_dynamic ("combobox_button_power", GCONF_ROOT "policy/button_power", ptrarr_button_power);

	/* sleep button */
	ptrarr_button_suspend = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_button_suspend, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_button_suspend, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_button_suspend, (gpointer) &pHibernate);
	combo_setup_dynamic ("combobox_button_suspend", GCONF_ROOT "policy/button_suspend", ptrarr_button_power);

	/* lid "button" */
	ptrarr_button_lid = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_button_lid, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_button_lid, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_button_lid, (gpointer) &pHibernate);
	combo_setup_dynamic ("combobox_button_lid", GCONF_ROOT "policy/button_lid", ptrarr_button_lid);

	/* AC fail */
	ptrarr_ac_fail = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_ac_fail, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_ac_fail, (gpointer) &pWarning);
	g_ptr_array_add (ptrarr_ac_fail, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_ac_fail, (gpointer) &pHibernate);
	combo_setup_dynamic ("combobox_ac_fail", GCONF_ROOT "policy/ac_fail", ptrarr_ac_fail);

	/* battery critical */
	ptrarr_battery_critical = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pNothing);
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pHibernate);
	g_ptr_array_add (ptrarr_battery_critical, (gpointer) &pShutdown);
	combo_setup_dynamic ("combobox_battery_critical", GCONF_ROOT "policy/battery_critical", ptrarr_battery_critical);

	/* sleep type */
	ptrarr_sleep_type = g_ptr_array_new ();
	g_ptr_array_add (ptrarr_sleep_type, (gpointer) &pSuspend);
	g_ptr_array_add (ptrarr_sleep_type, (gpointer) &pHibernate);
	combo_setup_dynamic ("combobox_sleep_type", GCONF_ROOT "policy/sleep_type", ptrarr_sleep_type);

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
	widget = glade_xml_get_widget (all_pref_widgets, "hscale_battery_low");
	gtk_range_set_range (GTK_RANGE (widget), 0, 25);
	value = gtk_range_get_value (GTK_RANGE (widget));

	widget = glade_xml_get_widget (all_pref_widgets, "hscale_battery_critical");
	gtk_range_set_range (GTK_RANGE (widget), 0, value);

	/* set the top end for LCD sliders */
	steps = hal_get_brightness_steps ();

	widget = glade_xml_get_widget (all_pref_widgets, "hscale_ac_brightness");
	gtk_range_set_range (GTK_RANGE (widget), 0, steps - 1);
	g_object_set_data ((GObject*) widget, "lcdsteps", (gpointer) &steps);

	widget = glade_xml_get_widget (all_pref_widgets, "hscale_batteries_brightness");
	gtk_range_set_range (GTK_RANGE (widget), 0, steps - 1);
	g_object_set_data ((GObject*) widget, "lcdsteps", (gpointer) &steps);

	/* set up info page */
	refresh_info_page ();

	/* main loop */
	gtk_main ();
	g_ptr_array_free (ptrarr_button_power, TRUE);
	g_ptr_array_free (ptrarr_button_suspend, TRUE);
	g_ptr_array_free (ptrarr_button_lid, TRUE);
	g_ptr_array_free (ptrarr_ac_fail, TRUE);
	g_ptr_array_free (ptrarr_battery_critical, TRUE);
	g_ptr_array_free (ptrarr_sleep_type, TRUE);
	return 0;
}
