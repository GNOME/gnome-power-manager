/*! @file	gpm-info.c
 *  @brief	GNOME Power Information
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-04
 *
 * This is the main g-p-i module, which displays information about the
 * power devices on your system, and the capabilities that g-p-m has to use.
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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gnome.h>

#include "gpm-common.h"
#include "gpm-gtk-utils.h"
#include "glibhal-main.h"
#include "glibhal-extras.h"

static GladeXML *all_info_widgets;

/** Sets up the Information tab with the correct information
 *
 *  @todo  We need to set the battery and UPS information here.
 */
void
refresh_info_page (void)
{
	gchar *returnstring;
	GtkWidget *widget = NULL;

	/* set vendor */
	if (hal_device_get_string ("/org/freedesktop/Hal/devices/computer",
				"smbios.system.manufacturer",
				&returnstring)) {
		gpm_gtk_set_label (all_info_widgets, "label_info_vendor", returnstring);
		g_free (returnstring);
	} else
		gpm_gtk_set_visibility (all_info_widgets, "label_info_vendor", FALSE);

	/* set model */
	if (hal_device_get_string ("/org/freedesktop/Hal/devices/computer",
				"smbios.system.product",
				&returnstring)) {
		gpm_gtk_set_label (all_info_widgets, "label_info_model", returnstring);
		g_free (returnstring);
	} else
		gpm_gtk_set_visibility (all_info_widgets, "label_info_model", FALSE);

	/* set formfactor */
	if (hal_device_get_string ("/org/freedesktop/Hal/devices/computer",
				"smbios.chassis.type",
				&returnstring)) {
		gpm_gtk_set_label (all_info_widgets, "label_info_formfactor", returnstring);
		g_free (returnstring);
	} else
		gpm_gtk_set_visibility (all_info_widgets, "label_info_formfactor", FALSE);

	/* Hardcoded for now */
	widget = glade_xml_get_widget (all_info_widgets,
			"checkbutton_info_suspend");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	widget = glade_xml_get_widget (all_info_widgets,
			"checkbutton_info_hibernate");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	widget = glade_xml_get_widget (all_info_widgets,
			"checkbutton_info_cpufreq");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

	widget = glade_xml_get_widget (all_info_widgets,
			"checkbutton_info_lowpowermode");
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), hal_is_laptop ());

	/* TODO */
	gpm_gtk_set_visibility (all_info_widgets, "frame_info_batteries", FALSE);
	gpm_gtk_set_visibility (all_info_widgets, "frame_info_ups", FALSE);

	gint i;
	gchar **device_names = NULL;
	gchar *battery_type;
	gchar *name;
	/* devices of type battery */
	hal_find_device_capability ("battery", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of batteries");
		return FALSE;
	}
#if 0
	for (i = 0; device_names[i]; i++) {
		hal_device_get_string (device_names[i], "battery.type", &battery_type);
		if (strcmp (battery_type, "primary") == 0) {
			if (i == 0)
				name = "batt1";
			if (i == 1)
				name = "batt2";
			gpm_gtk_set_label (all_info_widgets, "label_" name "_desc", battery_type;
		}
		g_free (battery_type);
	}
#endif
	hal_free_capability (device_names);
	return TRUE;
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

	gtk_init (&argc, &argv);

	/* load the interface */
	all_info_widgets = glade_xml_new (GPM_DATA "gpm-info.glade", NULL, NULL);
	if (!all_info_widgets)
		g_error ("glade file failed to load, aborting");

	/* Set the button callbacks */
	widget = glade_xml_get_widget (all_info_widgets, "button_close");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gtk_main_quit), NULL);
	widget = glade_xml_get_widget (all_info_widgets, "button_help");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (callback_help), NULL);

	/* for now, static refresh */
	refresh_info_page ();

	/* Get the main_window quit */
	widget = glade_xml_get_widget (all_info_widgets, "window_info");
	if (!widget)
		g_error ("Main window failed to load, aborting");
	g_signal_connect (G_OBJECT (widget), "delete_event",
		G_CALLBACK (gtk_main_quit), NULL);

	/* main loop */
	gtk_main ();
	return 0;
}
