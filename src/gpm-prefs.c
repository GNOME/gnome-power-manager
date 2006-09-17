/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-help.h>

#if HAVE_LIBGUNIQUEAPP
#include <libguniqueapp/guniqueapp.h>
#endif

#include "gpm-prefs.h"
#include "gpm-debug.h"
#include "gpm-prefs-core.h"

/**
 * gpm_prefs_help_cb
 * @prefs: This prefs class instance
 *
 * What to do when help is requested
 **/
static void
gpm_prefs_help_cb (GpmPrefs *prefs)
{
	GError *error = NULL;

	gnome_help_display_with_doc_id (NULL, "gnome-power-manager",
					"gnome-power-manager.xml", NULL, &error);
	if (error != NULL) {
		gpm_warning (error->message);
		g_error_free (error);
	}
}

/**
 * gpm_prefs_close_cb
 * @prefs: This prefs class instance
 *
 * What to do when we are asked to close for whatever reason
 **/
static void
gpm_prefs_close_cb (GpmPrefs *prefs)
{
	g_object_unref (prefs);
	exit (0);
}

#if HAVE_LIBGUNIQUEAPP
/**
 * guniqueapp_command_cb:
 **/
static void
guniqueapp_command_cb (GUniqueApp       *app,
		       GUniqueAppCommand command,
		       gchar            *data,
		       gchar            *startup_id,
		       guint             workspace,
		       gpointer          user_data)
{
	GpmPrefs *prefs = GPM_PREFS (user_data);
	if (command == G_UNIQUE_APP_ACTIVATE) {
		gpm_prefs_activate_window (prefs);
	}
}
#endif

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean verbose = FALSE;
	GOptionContext *context;
 	GnomeProgram *program;
	GpmPrefs *prefs = NULL;
	GMainLoop *loop;
#if HAVE_LIBGUNIQUEAPP
	GUniqueApp *uniqueapp;
#endif

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	context = g_option_context_new (_("GNOME Power Preferences"));

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	program = gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_GOPTION_CONTEXT, context,
			    GNOME_PARAM_HUMAN_READABLE_NAME,
			    _("Power Preferences"),
			    NULL);

	gpm_debug_init (verbose);

#if HAVE_LIBGUNIQUEAPP
	gpm_debug ("Using libguniqueapp support.");

	/* Arrr! Until we depend on gtk+2 2.12 we can't just use g_unique_app_get */
	uniqueapp = g_unique_app_get_with_startup_id ("gnome-power-preferences",
						      g_getenv ("DESKTOP_STARTUP_ID"));
	/* check to see if the user has another prefs window open */
	if (g_unique_app_is_running (uniqueapp)) {
		gpm_warning ("You have another instance running. "
			     "This program will now close");
		g_unique_app_activate (uniqueapp);
		/* Until we depend on gtk+2 2.12, we need to do the startup
		 * notification manually. Remove later... */
		gdk_notify_startup_complete ();
	} else {
#else
	gpm_warning ("No libguniqueapp support. Cannot signal other instances");
	/* we always assume we have no other running instance */
	if (1) {
#endif
		/* create a new instance of the window */
		prefs = gpm_prefs_new ();

		g_signal_connect (prefs, "action-help",
				  G_CALLBACK (gpm_prefs_help_cb), prefs);
		g_signal_connect (prefs, "action-close",
				  G_CALLBACK (gpm_prefs_close_cb), prefs);
#if HAVE_LIBGUNIQUEAPP
		/* Listen for messages from another instances */
		g_signal_connect (G_OBJECT (uniqueapp), "message",
				  G_CALLBACK (guniqueapp_command_cb), prefs);
#endif
		loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (loop);

		g_object_unref (prefs);
	}

	gpm_debug_shutdown ();

#if HAVE_LIBGUNIQUEAPP
	g_object_unref (uniqueapp);
#endif
	g_object_unref (program);
/* seems to not work...
	g_option_context_free (context); */

	return 0;
}
