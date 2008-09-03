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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "egg-debug.h"
#include "gpm-common.h"

/**
 * gpm_precision_round_up:
 * @value: The input value
 * @smallest: The smallest increment allowed
 *
 * 101, 10	110
 * 95,  10	100
 * 0,   10	10
 * 112, 10	120
 * 100, 10	100
 **/
guint
gpm_precision_round_up (guint value, guint smallest)
{
	guint division;
	if (value == 0) {
		return smallest;
	}
	if (smallest == 0) {
		egg_warning ("divisor zero");
		return 0;
	}
	division = value / smallest;
	/* add one unit to scale if we can't contain */
	if (value % smallest != 0) {
		division++;
	}
	division *= smallest;
	return division;
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
guint
gpm_precision_round_down (guint value, guint smallest)
{
	guint division;
	if (value == 0) {
		return 0;
	}
	if (smallest == 0) {
		egg_warning ("divisor zero");
		return 0;
	}
	division = value / smallest;
	division *= smallest;
	return division;
}

/**
 * gpm_exponential_average:
 * @previous: The old value
 * @new: The new value
 * @slew: The slew rate as a percentage
 *
 * We should do an exponentially weighted average so that high frequency
 * changes are smoothed. This should mean the output does not change
 * drastically between updates.
 **/
gint
gpm_exponential_average (gint previous, gint new, guint slew)
{
	gint result = 0;
	gfloat factor = 0;
	gfloat factor_inv = 1;
	if (previous == 0 || slew == 0) {
		/* startup, or re-initialization - we have no data */
		egg_debug ("Quoting output with only one value...");
		result = new;
	} else {
		factor = (gfloat) slew / 100.0f;
		factor_inv = 1.0f - factor;
		result = (gint) ((factor_inv * (gfloat) new) + (factor * (gfloat) previous));
	}
	return result;
}

/**
 * gpm_percent_to_discrete:
 * @percentage: The percentage to convert
 * @levels: The number of discrete levels
 *
 * We have to be carefull when converting from %->discrete as precision is very
 * important if we want the highest value.
 *
 * Return value: The discrete value for this percentage.
 **/
guint
gpm_percent_to_discrete (guint percentage, guint levels)
{
	/* check we are in range */
	if (percentage > 100) {
		return levels;
	}
	if (levels == 0) {
		egg_warning ("levels is 0!");
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
	if (discrete > levels) {
		return 100;
	}
	if (levels == 0) {
		egg_warning ("levels is 0!");
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
gpm_discrete_to_fraction (guint discrete,
			  guint levels)
{
	/* check we are in range */
	if (discrete > levels) {
		return 1.0;
	}
	if (levels == 0) {
		egg_warning ("levels is 0!");
		return 0.0;
	}
	return (guint) ((gfloat) discrete / ((gfloat) (levels - 1)));
}

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

GpmIconPolicy
gpm_tray_icon_mode_from_string (const gchar *str)
{
	if (str == NULL) {
		return GPM_ICON_POLICY_NEVER;
	}

	if (strcmp (str, "always") == 0) {
		return GPM_ICON_POLICY_ALWAYS;
	} else if (strcmp (str, "present") == 0) {
		return GPM_ICON_POLICY_PRESENT;
	} else if (strcmp (str, "charge") == 0) {
		return GPM_ICON_POLICY_CHARGE;
	} else if (strcmp (str, "critical") == 0) {
		return GPM_ICON_POLICY_CRITICAL;
	} else if (strcmp (str, "never") == 0) {
		return GPM_ICON_POLICY_NEVER;
	} else {
		return GPM_ICON_POLICY_NEVER;
	}
}

const gchar *
gpm_tray_icon_mode_to_string (GpmIconPolicy mode)
{
	if (mode == GPM_ICON_POLICY_ALWAYS) {
		return "always";
	} else if (mode == GPM_ICON_POLICY_PRESENT) {
		return "present";
	} else if (mode == GPM_ICON_POLICY_CHARGE) {
		return "charge";
	} else if (mode == GPM_ICON_POLICY_CRITICAL) {
		return "critical";
	} else if (mode == GPM_ICON_POLICY_NEVER) {
		return "never";
	} else {
		return "never";
	}
}

/**
 * gpm_help_display:
 * @link_id: Subsection of gnome-power-manager help section
 **/
void
gpm_help_display (char * link_id)
{
	GError *error = NULL;
	char *command;
	const char *lang;
	char *uri = NULL;
	GdkScreen *gscreen;

	int i;

	const char * const * langs = g_get_language_names ();

	for (i = 0; langs[i]; i++) {
		lang = langs[i];
		if (strchr (lang, '.')) {
			continue;
		}

		uri = g_build_filename(DATADIR,
				       "/gnome/help/gnome-power-manager/",
					lang,
				       "/gnome-power-manager.xml",
					NULL);
					
		if (g_file_test (uri, G_FILE_TEST_EXISTS)) {
                    break;
		}
	}
	
	if (link_id) {
		command = g_strconcat ("gnome-open ghelp://", uri, "?", link_id, NULL);
	} else {
		command = g_strconcat ("gnome-open ghelp://", uri,  NULL);
	}

	gscreen = gdk_screen_get_default();
	gdk_spawn_command_line_on_screen (gscreen, command, &error);
	if (error != NULL) {
		GtkWidget *d;

		d = gtk_message_dialog_new(NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				"%s", error->message);
		gtk_dialog_run(GTK_DIALOG(d));
		gtk_widget_destroy(d);
		g_error_free(error);
	}

	g_free (command);
	g_free (uri);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef GPM_BUILD_TESTS
#include "gpm-self-test.h"

void
gpm_st_common (GpmSelfTest *test)
{
	guint value;
	gfloat fvalue;

	if (gpm_st_start (test, "GpmCommon") == FALSE) {
		return;
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 0,10");
	value = gpm_precision_round_down (0, 10);
	if (value == 0) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 4,10");
	value = gpm_precision_round_down (4, 10);
	if (value == 0) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 11,10");
	value = gpm_precision_round_down (11, 10);
	if (value == 10) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 201,2");
	value = gpm_precision_round_down (201, 2);
	if (value == 200) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 100,10");
	value = gpm_precision_round_down (100, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 0,10");
	value = gpm_precision_round_up (0, 10);
	if (value == 10) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 4,10");
	value = gpm_precision_round_up (4, 10);
	if (value == 10) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 11,10");
	value = gpm_precision_round_up (11, 10);
	if (value == 20) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 201,2");
	value = gpm_precision_round_up (201, 2);
	if (value == 202) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 100,10");
	value = gpm_precision_round_up (100, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 0/10 levels");
	value = gpm_discrete_to_percent (0, 10);
	if (value == 0) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "conversion incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 9/10 levels");
	value = gpm_discrete_to_percent (9, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "conversion incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 20/10 levels");
	value = gpm_discrete_to_percent (20, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "conversion incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 0/10 levels");
	fvalue = gpm_discrete_to_fraction (0, 10);
	if (fvalue > -0.01 && fvalue < 0.01) {
		gpm_st_success (test, "got %f", fvalue);
	} else {
		gpm_st_failed (test, "conversion incorrect (%f)", fvalue);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 9/10 levels");
	fvalue = gpm_discrete_to_fraction (9, 10);
	if (fvalue > -1.01 && fvalue < 1.01) {
		gpm_st_success (test, "got %f", fvalue);
	} else {
		gpm_st_failed (test, "conversion incorrect (%f)", fvalue);
	}

	gpm_st_end (test);
}

#endif

