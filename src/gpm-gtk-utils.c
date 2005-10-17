/** @file	gpm-gtk-utils.c
 *  @brief	Simple wrapper functions for easy libglade stuff
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-04
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

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <glade/glade.h>
#include <gtk/gtk.h>

/** Sets/Hides GTK visibility
 *
 *  @param	allwidgets	The glade XML
 *  @param	widgetname	The libglade widget name
 *  @param	set		Should widget be visible?
 *  @return			Success
 */
gboolean
gpm_gtk_set_visibility (GladeXML *allwidgets, const gchar *widgetname, gboolean set)
{
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (widgetname);

	widget = glade_xml_get_widget (allwidgets, widgetname);
	if (!widget) {
		g_warning ("gpm_gtk_set_visibility: widget '%s' not found",
				widgetname);
		return FALSE;
	}

	if (set)
		gtk_widget_show_all (widget);
	else
		gtk_widget_hide_all (widget);
	return TRUE;
}

/** Sets/Clears GTK Checkbox
 *
 *  @param	allwidgets	The glade XML
 *  @param	widgetname	The libglade widget name
 *  @param	set		Should check be ticked?
 *  @return			Success
 */
gboolean
gpm_gtk_set_check (GladeXML *allwidgets, const gchar *widgetname, gboolean set)
{
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (widgetname);

	widget = glade_xml_get_widget (allwidgets, widgetname);
	if (!widget) {
		g_warning ("widget '%s' failed to load, aborting", widgetname);
		return FALSE;
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), set);
	return TRUE;
}

/** Make GTK Checkbox sensitive or not sensitive
 *
 *  @param	allwidgets	The glade XML
 *  @param	widgetname	The libglade widget name
 *  @param	set		Should check be ticked?
 *  @return			Success
 */
gboolean
gpm_gtk_set_sensitive (GladeXML *allwidgets, const gchar *widgetname, gboolean set)
{
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (widgetname);

	widget = glade_xml_get_widget (allwidgets, widgetname);
	if (!widget) {
		g_warning ("widget '%s' failed to load, aborting", widgetname);
		return FALSE;
	}
	gtk_widget_set_sensitive (GTK_WIDGET (widget), set);
	return TRUE;
}

/** Modifies a GTK Label
 *
 *  @param	allwidgets	The glade XML
 *  @param	widgetname	The libglade widget name
 *  @param	label		The new text
 *  @return			Success
 */
gboolean
gpm_gtk_set_label (GladeXML *allwidgets, const gchar *widgetname, const gchar *label)
{
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (widgetname);

	widget = glade_xml_get_widget (allwidgets, widgetname);
	if (!widget) {
		g_warning ("widget '%s' failed to load, aborting", widgetname);
		return FALSE;
	}
	gtk_label_set_markup (GTK_LABEL (widget), label);
	return TRUE;
}
