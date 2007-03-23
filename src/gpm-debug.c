/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

static gboolean is_init = FALSE;	/* if we are initialised */
static gboolean do_verbose = FALSE;	/* if we should print out debugging */
static GSList *list = NULL;
static gchar va_args_buffer [1025];

/**
 * gpm_add_debug_option:
 **/
void
gpm_add_debug_option (const gchar *option)
{
	/* adding debug option to list */
	list = g_slist_prepend (list, (gpointer) option);
}

/**
 * gpm_print_line:
 **/
static void
gpm_print_line (const gchar *func,
		const gchar *file,
		const int    line,
		const gchar *buffer)
{
	gchar   *str_time;
	time_t  the_time;

	time (&the_time);
	str_time = g_new0 (gchar, 255);
	strftime (str_time, 254, "%H:%M:%S", localtime (&the_time));

	fprintf (stderr, "[%s] %s:%d (%s):\t %s\n",
		 func, file, line, str_time, buffer);
	g_free (str_time);
}

/**
 * gpm_debug_strcmp_func
 * @a: Pointer to the data to test
 * @b: Pointer to a cookie to compare
 * Return value: 0 if cookie matches
 **/
static gint
gpm_debug_strcmp_func (gconstpointer a, gconstpointer b)
{
	gchar *aa = (gchar *) a;
	gchar *bb = (gchar *) b;
	return strcmp (aa, bb);
}

/**
 * gpm_debug_in_options:
 **/
static gboolean
gpm_debug_in_options (const gchar *file)
{
	GSList *node;
	gchar *name;
	guint8 len;

	/* get rid of the "gpm-" prefix */
	name = strdup (&file[4]);

	/* get rid of .c */
	len = strlen (name);
	if (len>2) {
		name[len-2] = '\0';
	}

	/* find string in list */
	node = g_slist_find_custom (list, name, gpm_debug_strcmp_func);
	g_free (name);
	return (node != NULL);
}

/**
 * gpm_debug_real:
 **/
void
gpm_debug_real (const gchar *func,
		const gchar *file,
		const int    line,
		const gchar *format, ...)
{
	va_list args;

	if (do_verbose == FALSE && gpm_debug_in_options (file) == FALSE) {
		return;
	}

	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);

	gpm_print_line (func, file, line, va_args_buffer);
}

/**
 * gpm_warning_real:
 **/
void
gpm_warning_real (const gchar *func,
		  const gchar *file,
		  const int    line,
		  const gchar *format, ...)
{
	va_list args;

	if (do_verbose == FALSE) {
		return;
	}

	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	fprintf (stderr, "*** WARNING ***\n");
	gpm_print_line (func, file, line, va_args_buffer);
}

/**
 * gpm_bugzilla:
 *
 * Asks the user to consult and add to bugzilla.
 **/
void
gpm_bugzilla (void)
{
	fprintf (stderr, "%s has encountered a non-critical warning.\n"
		 "Consult %s for any known issues or a possible fix.\n"
		 "Please file a bug with this complete message if not present\n",
		 "GNOME Power Manager", GPM_BUGZILLA_URL);
}

/**
 * gpm_syslog:
 * @format: This va format string, e.g. ("test %s", hello)
 *
 * Logs some text to the syslog, usually in /var/log/messages
 **/
static void
gpm_syslog_internal (const gchar *string)
{
	fprintf (stderr, "Saving to syslog: %s", string);
	syslog (LOG_NOTICE, "(%s) %s", g_get_user_name (), string);
}

/**
 * gpm_syslog:
 * @format: This va format string, e.g. ("test %s", hello)
 *
 * Logs some text to the syslog, usually in /var/log/messages
 **/
void
gpm_syslog (const gchar *format, ...)
{
	va_list args;

	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);

	gpm_syslog_internal (va_args_buffer);
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
	is_init = TRUE;

	do_verbose = debug;
	gpm_debug ("Verbose debugging %s", (do_verbose) ? "enabled" : "disabled");

	/* open syslog */
	openlog ("gnome-power-manager", LOG_NDELAY, LOG_USER);
}

/**
 * gpm_critical_error:
 * @content: The content to show, e.g. "No icons detected"
 *
 * Shows a gtk critical error and logs to syslog.
 * NOTE: we will lose memory, but since this program is a critical error
 * that is the least of our problems...
 **/
void
gpm_critical_error (const gchar *format, ...)
{
	va_list args;

	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);

	g_warning (va_args_buffer);
}

/**
 * gpm_debug_shutdown:
 **/
void
gpm_debug_shutdown (void)
{
	if (! is_init) {
		return;
	}

	gpm_debug ("Shutting down debugging");
	is_init = FALSE;

	/* shut down syslog */
	closelog ();
}
