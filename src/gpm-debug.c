/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>

#include "gpm-debug.h"
#include "gpm-common.h"

static gboolean debugging = FALSE;
static gboolean done_warning = FALSE;
static FILE    *debug_out = NULL;

/* Based on rhythmbox/lib/rb-debug.c */
void
gpm_debug_real (gboolean warning,
		const char *func,
		const char *file,
		const int   line,
		const char *format, ...)
{
	va_list args;
	char    buffer [1025];
	char   *str_time;
	time_t  the_time;
	FILE   *output_file;

	if (!warning && debugging == FALSE) {
		return;
	}

	va_start (args, format);
	g_vsnprintf (buffer, 1024, format, args);
	va_end (args);

	time (&the_time);
	str_time = g_new0 (char, 255);
	strftime (str_time, 254, "%H:%M:%S", localtime (&the_time));

	if (debug_out) {
		output_file = debug_out;
	} else {
		output_file = stderr;
	}

	if (warning) {
		/* do extra stuff for a warning */
		fprintf (output_file, "*** WARNING ***\n");
		fprintf (output_file, "[%s] %s:%d (%s):\t %s\n",
			 func, file, line, str_time, buffer);
		if (! done_warning) {
			fprintf (output_file, "%s has encountered a non-critical warning.\n"
				 "Consult %s for any known issues or a possible fix.\n"
				 "Please file a bug with this complete message if not present\n",
				 "GNOME Power Manager", GPM_BUGZILLA_URL);
			done_warning = TRUE;
		}
		fprintf (output_file, "*** WARNING ***\n");
	} else {
		/* just do debug */
		fprintf (output_file, "[%s] %s:%d (%s):\t %s\n",
			 func, file, line, str_time, buffer);
	}

	if (debug_out) {
		fflush (output_file);
	}
	g_free (str_time);
}

void
gpm_debug_init (gboolean debug,
		gboolean to_file)
{
	/* return if already initialized */
	if (debugging == TRUE) {
		return;
	}

	debugging = debug;

	if (debug && to_file) {
		const char path [50] = "gnome_power_manager_debug_XXXXXX";
		int fd;

		fd = g_file_open_tmp (path, NULL, NULL);

		if (fd >= 0) {
			debug_out = fdopen (fd, "a");
		}
	}

	gpm_debug ("Debugging %s", (debug) ? "enabled" : "disabled");
}

void
gpm_debug_shutdown (void)
{
	if (! debugging)
		return;

	gpm_debug ("Shutting down debugging");

	debugging = FALSE;

	if (debug_out != NULL) {
		fclose (debug_out);
		debug_out = NULL;
	}
}
