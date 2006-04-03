/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
 * Originally based on rhythmbox/lib/rb-debug.c
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>

#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-common.h"

static gboolean is_init = FALSE;	/* if we are initialised */
static gboolean do_verbose = FALSE;	/* if we should print out debugging */
static gboolean done_warning = FALSE;	/* if we've done the bugzilla warning */

/**
 * gpm_print_line:
 **/
static void
gpm_print_line (const char *func,
		const char *file,
		const int   line,
		const char *buffer)
{
	char   *str_time;
	time_t  the_time;

	time (&the_time);
	str_time = g_new0 (char, 255);
	strftime (str_time, 254, "%H:%M:%S", localtime (&the_time));

	fprintf (stderr, "[%s] %s:%d (%s):\t %s\n",
		 func, file, line, str_time, buffer);
	g_free (str_time);
}

/**
 * gpm_debug_real:
 **/
void
gpm_debug_real (const char *func,
		const char *file,
		const int   line,
		const char *format, ...)
{
	va_list args;
	char    buffer [1025];

	if (! do_verbose) {
		return;
	}

	va_start (args, format);
	g_vsnprintf (buffer, 1024, format, args);
	va_end (args);

	gpm_print_line (func, file, line, buffer);
}

/**
 * gpm_warning_real:
 **/
void
gpm_warning_real (const char *func,
		  const char *file,
		  const int   line,
		  const char *format, ...)
{
	va_list args;
	char    buffer [1025];

	if (! do_verbose) {
		return;
	}

	va_start (args, format);
	g_vsnprintf (buffer, 1024, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	fprintf (stderr, "*** WARNING ***\n");
	gpm_print_line (func, file, line, buffer);
	if (! done_warning) {
		fprintf (stderr, "%s has encountered a non-critical warning.\n"
			 "Consult %s for any known issues or a possible fix.\n"
			 "Please file a bug with this complete message if not present\n",
			 "GNOME Power Manager", GPM_BUGZILLA_URL);
		done_warning = TRUE;
	}
}

/**
 * gpm_syslog:
 * @format: This va format string, e.g. ("test %s", hello)
 *
 * Logs some text to the syslog, usually in /var/log/messages
 **/
void
gpm_syslog (const char *format, ...)
{
	va_list args;
	char    buffer [1025];

	va_start (args, format);
	g_vsnprintf (buffer, 1024, format, args);
	va_end (args);

	gpm_debug ("Saving to syslog: %s", buffer);
	syslog (LOG_ALERT, "%s", buffer);
}

/**
 * gpm_debug_init:
 * @debug: If we should print out verbose logging
 **/
void
gpm_debug_init (gboolean debug)
{
	/* return if already initialized */
	if (is_init) {
		return;
	}

	do_verbose = debug;
	gpm_debug ("Verbose debugging %s", (do_verbose) ? "enabled" : "disabled");

	/* open syslog */
	openlog ("gnome-power-manager", LOG_NDELAY, LOG_USER);
}

/**
 * gpm_critical_error:
 * @content: The content to show, e.g. "No icons detected"
 *
 * Shows a gtk critical error, logs to syslog and closes the program.
 * NOTE: we will loose memory, but since this program is a critical error
 * that is the least of our problems...
 **/
void
gpm_critical_error (const char *format, ...)
{
	va_list args;
	char    buffer [1025];

	va_start (args, format);
	g_vsnprintf (buffer, 1024, format, args);
	va_end (args);

	gpm_syslog ("Critical error: %s", buffer);
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new_with_markup (NULL,
						     GTK_DIALOG_MODAL,
						     GTK_MESSAGE_WARNING,
						     GTK_BUTTONS_CLOSE,
						     "<span size='larger'><b>%s</b></span>",
						     GPM_NAME);
	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
						    buffer);
	/* we close the gtk lopp when the user clicks close */
	g_signal_connect_swapped (dialog,
				  "response",
				  G_CALLBACK (gtk_main_quit),
				  NULL);
	gtk_window_present (GTK_WINDOW (dialog));
	/* we wait here for user to click close */
	gtk_main();
}

/**
 * gpm_debug_shutdown:
 **/
void
gpm_debug_shutdown (void)
{
	if (! is_init)
		return;

	gpm_debug ("Shutting down debugging");
	is_init = FALSE;

	/* shut down syslog */
	closelog ();
}
