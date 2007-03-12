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
#include <libgnomeui/gnome-ui-init.h>

#include "../src/gpm-debug.h"

#include "gpm-st-main.h"

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
	test->succeeded++;
	if (format == NULL) {
		g_print ("...OK\n");
		return;
	}
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("...OK [%s]\n", va_args_buffer);
}

void
gpm_st_failed (GpmSelfTest *test, const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("...FAILED [%s]\n", va_args_buffer);
}

int
main (int argc, char **argv)
{
	GOptionContext  *context;
 	GnomeProgram    *program;
	int retval;

	context = g_option_context_new ("GNOME Power Manager Self Test");
	program = gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_GOPTION_CONTEXT, context,
			    GNOME_PARAM_HUMAN_READABLE_NAME,
			    "Power Inhibit Test",
			    NULL);
	gpm_debug_init (FALSE);

	GpmSelfTest ttest;
	GpmSelfTest *test = &ttest;
	test->total = 0;
	test->succeeded = 0;
	test->type = NULL;

	gpm_st_common (test);
	gpm_st_array (test);
	gpm_st_inhibit (test);
	gpm_st_proxy (test);
	gpm_st_hal_power (test);
	gpm_st_hal_manager (test);
	gpm_st_hal_device (test);
	gpm_st_hal_devicestore (test);
	gpm_st_cell_unit (test);

	g_print ("test passes (%u/%u) : ", test->succeeded, test->total);
	if (test->succeeded == test->total) {
		g_print ("ALL OKAY\n");
		retval = 0;
	} else {
		g_print ("%u FAILURE(S)\n", test->total - test->succeeded);
		retval = 1;
	}

//	g_object_unref (program);
	g_option_context_free (context);
	return retval;
}

