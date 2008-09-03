/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "egg-debug.h"
#include "gpm-self-test.h"

gboolean
gpm_st_start (GpmSelfTest *test, const gchar *name)
{
	if (test->started) {
		g_print ("Not ended test! Cannot start!\n");
		exit (1);
	}	
	test->type = g_strdup (name);
	test->started = TRUE;
	g_print ("%s...", test->type);
	return TRUE;
}

void
gpm_st_end (GpmSelfTest *test)
{
	if (test->started == FALSE) {
		g_print ("Not started test! Cannot finish!\n");
		exit (1);
	}	
	g_print ("OK\n");
	test->started = FALSE;
	g_free (test->type);
}

void
gpm_st_title (GpmSelfTest *test, const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("> check #%u\t%s: \t%s...", test->total+1, test->type, va_args_buffer);
	test->total++;
}

void
gpm_st_success (GpmSelfTest *test, const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	if (format == NULL) {
		g_print ("...OK\n");
		goto finish;
	}
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("...OK [%s]\n", va_args_buffer);
finish:
	test->succeeded++;
}

void
gpm_st_failed (GpmSelfTest *test, const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	if (format == NULL) {
		g_print ("FAILED\n");
		goto failed;
	}
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("FAILED [%s]\n", va_args_buffer);
failed:
	exit (1);
}

void
gpm_st_warning (GpmSelfTest *test, const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	if (format == NULL) {
		g_print ("UNKNOWN WARNING\n");
		goto out;
	}
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("WARNING [%s]\n", va_args_buffer);
out:
	;
}

static void
gpm_st_run_test (GpmSelfTest *test, GpmSelfTestFunc func)
{
	func (test);
}

int
main (int argc, char **argv)
{
	GOptionContext  *context;
	int retval;

	gboolean verbose = FALSE;
	char *level = NULL;
	char **tests = NULL;

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  "Show verbose debugging information", NULL },
		{ "level", '\0', 0, G_OPTION_ARG_STRING, &level,
		  "Set the printing level, [quiet|normal|all]", NULL },
		{ "tests", '\0', 0, G_OPTION_ARG_STRING_ARRAY, &tests,
		  "Debug specific modules, [common,webcam,arrayfloat]", NULL },
		{ NULL}
	};

	context = g_option_context_new ("GNOME Power Manager Self Test");
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_parse (context, &argc, &argv, NULL);
	gtk_init (&argc, &argv);

	egg_debug_init (verbose);

	GpmSelfTest ttest;
	GpmSelfTest *test = &ttest;
	test->total = 0;
	test->succeeded = 0;
	test->type = NULL;
	test->started = FALSE;

	/* auto */
	gpm_st_run_test (test, gpm_st_common);
	gpm_st_run_test (test, gpm_st_array_float);
	gpm_st_run_test (test, gpm_st_array);
	gpm_st_run_test (test, gpm_st_cell_unit);
	gpm_st_run_test (test, gpm_st_cell);
	gpm_st_run_test (test, gpm_st_cell_array);
	gpm_st_run_test (test, gpm_st_inhibit);
	gpm_st_run_test (test, gpm_st_profile);
	gpm_st_run_test (test, gpm_st_phone);

	/* manual */
	gpm_st_run_test (test, gpm_st_graph_widget);

#if 0
	gpm_st_run_test (test, gpm_st_proxy);
	gpm_st_run_test (test, gpm_st_hal_power);
	gpm_st_run_test (test, gpm_st_hal_manager);
	gpm_st_run_test (test, gpm_st_hal_device);
	gpm_st_run_test (test, gpm_st_hal_devicestore);
	gpm_st_run_test (test, gpm_st_idletime);
#endif

	g_print ("test passes (%u/%u) : ", test->succeeded, test->total);
	if (test->succeeded == test->total) {
		g_print ("ALL OKAY\n");
		retval = 0;
	} else {
		g_print ("%u FAILURE(S)\n", test->total - test->succeeded);
		retval = 1;
	}

	g_option_context_free (context);
	return retval;
}

