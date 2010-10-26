/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "gpm-common.h"

/**
 * gpm_get_timestring:
 * @time_secs: The time value to convert in seconds
 * @cookie: The cookie we are looking for
 *
 * Returns a localised timestring
 *
 * Return value: The time string, e.g. "2 hours 3 minutes"
 **/
gchar *
gpm_get_timestring (guint time_secs)
{
	char* timestring = NULL;
	gint  hours;
	gint  minutes;

	/* Add 0.5 to do rounding */
	minutes = (int) ( ( time_secs / 60.0 ) + 0.5 );

	if (minutes == 0) {
		timestring = g_strdup (_("Unknown time"));
		return timestring;
	}

	if (minutes < 60) {
		timestring = g_strdup_printf (ngettext ("%i minute",
							"%i minutes",
							minutes), minutes);
		return timestring;
	}

	hours = minutes / 60;
	minutes = minutes % 60;

	if (minutes == 0)
		timestring = g_strdup_printf (ngettext (
				"%i hour",
				"%i hours",
				hours), hours);
	else
		/* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
		 * Swap order with "%2$s %2$i %1$s %1$i if needed */
		timestring = g_strdup_printf (_("%i %s %i %s"),
				hours, ngettext ("hour", "hours", hours),
				minutes, ngettext ("minute", "minutes", minutes));
	return timestring;
}

/**
 * gpm_help_display:
 * @link_id: Subsection of gnome-power-manager help section
 **/
void
gpm_help_display (const gchar *link_id)
{
	GError *error = NULL;
	gchar *uri;

	if (link_id != NULL)
		uri = g_strconcat ("ghelp:gnome-power-manager?", link_id, NULL);
	else
		uri = g_strdup ("ghelp:gnome-power-manager");

	gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		GtkWidget *d;
		d = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error->message);
		gtk_dialog_run (GTK_DIALOG(d));
		gtk_widget_destroy (d);
		g_error_free (error);
	}
	g_free (uri);
}

/**
 * gpm_precision_round_up:
 * @value: The input value
 * @smallest: The smallest increment allowed
 *
 * 101, 10	110
 * 95,  10	100
 * 0,   10	0
 * 112, 10	120
 * 100, 10	100
 **/
gint
gpm_precision_round_up (gfloat value, gint smallest)
{
	gfloat division;
	if (fabs (value) < 0.01)
		return 0;
	if (smallest == 0) {
		g_warning ("divisor zero");
		return 0;
	}
	division = (gfloat) value / (gfloat) smallest;
	division = ceilf (division);
	division *= smallest;
	return (gint) division;
}

/**
 * gpm_precision_round_down:
 * @value: The input value
 * @smallest: The smallest increment allowed
 *
 * 101, 10	100
 * 95,  10	90
 * 0,   10	0
 * 112, 10	110
 * 100, 10	100
 **/
gint
gpm_precision_round_down (gfloat value, gint smallest)
{
	gfloat division;
	if (fabs (value) < 0.01)
		return 0;
	if (smallest == 0) {
		g_warning ("divisor zero");
		return 0;
	}
	division = (gfloat) value / (gfloat) smallest;
	division = floorf (division);
	division *= smallest;
	return (gint) division;
}

/**
 * gpm_discrete_from_percent:
 * @percentage: The percentage to convert
 * @levels: The number of discrete levels
 *
 * We have to be carefull when converting from %->discrete as precision is very
 * important if we want the highest value.
 *
 * Return value: The discrete value for this percentage.
 **/
guint
gpm_discrete_from_percent (guint percentage, guint levels)
{
	/* check we are in range */
	if (percentage > 100)
		return levels;
	if (levels == 0) {
		g_warning ("levels is 0!");
		return 0;
	}
	return ((gfloat) percentage * (gfloat) (levels - 1)) / 100.0f;
}

/**
 * gpm_discrete_to_percent:
 * @hw: The discrete level
 * @levels: The number of discrete levels
 *
 * We have to be carefull when converting from discrete->%.
 *
 * Return value: The percentage for this discrete value.
 **/
guint
gpm_discrete_to_percent (guint discrete, guint levels)
{
	/* check we are in range */
	if (discrete > levels)
		return 100;
	if (levels == 0) {
		g_warning ("levels is 0!");
		return 0;
	}
	return (guint) ((gfloat) discrete * (100.0f / (gfloat) (levels - 1)));
}

/**
 * gpm_discrete_to_fraction:
 * @hw: The discrete level
 * @levels: The number of discrete levels
 *
 * We have to be careful when converting from discrete->fractions.
 *
 * Return value: The floating point fraction (0..1) for this discrete value.
 **/
gfloat
gpm_discrete_to_fraction (guint discrete, guint levels)
{
	/* check we are in range */
	if (discrete > levels)
		return 1.0;
	if (levels == 0) {
		g_warning ("levels is 0!");
		return 0.0;
	}
	return (guint) ((gfloat) discrete / ((gfloat) (levels - 1)));
}

/**
 * gpm_color_from_rgb:
 * @red: The red value
 * @green: The green value
 * @blue: The blue value
 **/
guint32
gpm_color_from_rgb (guint8 red, guint8 green, guint8 blue)
{
	guint32 color = 0;
	color += (guint32) red * 0x10000;
	color += (guint32) green * 0x100;
	color += (guint32) blue;
	return color;
}

/**
 * gpm_color_to_rgb:
 * @red: The red value
 * @green: The green value
 * @blue: The blue value
 **/
void
gpm_color_to_rgb (guint32 color, guint8 *red, guint8 *green, guint8 *blue)
{
	*red = (color & 0xff0000) / 0x10000;
	*green = (color & 0x00ff00) / 0x100;
	*blue = color & 0x0000ff;
}

