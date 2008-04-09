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
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>

#include "gpm-common.h"
#include "gpm-debug.h"

#define CONSOLE_RESET		0
#define CONSOLE_BLACK 		30
#define CONSOLE_RED		31
#define CONSOLE_GREEN		32
#define CONSOLE_YELLOW		33
#define CONSOLE_BLUE		34
#define CONSOLE_MAGENTA		35
#define CONSOLE_CYAN		36
#define CONSOLE_WHITE		37

static gboolean is_init = FALSE;	/* if we are initialised */
static gboolean do_verbose = FALSE;	/* if we should print out debugging */
static gboolean is_console = FALSE;
static GSList *list = NULL;
static gchar va_args_buffer [1025];

/**
 * gpm_debug_is_verbose:
 **/
gboolean
gpm_debug_is_verbose (void)
{
	return do_verbose;
}

/**
 * pk_set_console_mode:
 **/
static void
gpm_set_console_mode (guint console_code)
{
	gchar command[13];

	/* don't put extra commands into logs */
	if (!is_console) {
		return;
	}
	/* Command is the control command to the terminal */
	sprintf (command, "%c[%dm", 0x1B, console_code);
	printf ("%s", command);
}

/**
 * gpm_print_line:
 **/
static void
gpm_print_line (const gchar *func, const gchar *file, const int line, const gchar *buffer, guint color)
{
	gchar *str_time;
	gchar *header;
	time_t the_time;

	time (&the_time);
	str_time = g_new0 (gchar, 255);
	strftime (str_time, 254, "%H:%M:%S", localtime (&the_time));

	/* generate header text */
	header = g_strdup_printf ("TI:%s\tFI:%s\tFN:%s,%d", str_time, file, func, line);
	g_free (str_time);

	/* always in light green */
	gpm_set_console_mode (CONSOLE_GREEN);
	printf ("%s\n", header);

	/* different colours according to the severity */
	gpm_set_console_mode (color);
	printf (" - %s\n", buffer);
	gpm_set_console_mode (CONSOLE_RESET);

	/* flush this output, as we need to debug */
	fflush (stdout);

	g_free (header);
}

/**
 * gpm_debug_real:
 **/
void
gpm_debug_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	if (do_verbose == FALSE) {
		return;
	}

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	gpm_print_line (func, file, line, buffer, CONSOLE_BLUE);

	g_free(buffer);
}

/**
 * gpm_warning_real:
 **/
void
gpm_warning_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	if (do_verbose == FALSE) {
		return;
	}

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	if (!is_console) {
		printf ("*** WARNING ***\n");
	}
	gpm_print_line (func, file, line, buffer, CONSOLE_RED);

	g_free(buffer);
}

/**
 * gpm_syslog_internal:
 * @format: This va format string, e.g. ("test %s", hello)
 *
 * Logs some text to the syslog, usually in /var/log/messages
 **/
static void
gpm_syslog_internal (const gchar *string)
{
	gpm_debug ("Saving to syslog: %s", string);
	syslog (LOG_NOTICE, "(%s) %s", g_get_user_name (), string);
}


/**
 * gpm_error_real:
 **/
void
gpm_error_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	if (!is_console) {
		printf ("*** ERROR ***\n");
	}
	gpm_print_line (func, file, line, buffer, CONSOLE_RED);
	g_free(buffer);

	exit (1);
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
	gchar *buffer = NULL;

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	gpm_syslog_internal (buffer);
	g_free (buffer);
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
	/* check if we are on console */
	if (isatty (fileno (stdout)) == 1) {
		is_console = TRUE;
	}
	gpm_debug ("Verbose debugging %i (on console %i)", do_verbose, is_console);

	/* open syslog */
	openlog ("gnome-power-manager", LOG_NDELAY, LOG_USER);
}

/**
 * gpm_debug_shutdown:
 **/
void
gpm_debug_shutdown (void)
{
	if (is_init == FALSE) {
		return;
	}

	gpm_debug ("Shutting down debugging");
	is_init = FALSE;

	/* shut down syslog */
	closelog ();
}
