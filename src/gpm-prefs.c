/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#if HAVE_GTKUNIQUE
#include <gtkunique/gtkunique.h>
#endif

#include "gpm-common.h"
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
	gpm_help_display ("preferences");
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

#if HAVE_GTKUNIQUE
/**
 * gtkuniqueapp_command_cb:
 **/
static void
gtkuniqueapp_command_cb (GtkUniqueApp    *app,
		         GtkUniqueCommand command,
		         const gchar     *data,
		         const gchar     *startup_id,
		         GdkScreen	 *screen,
		         guint            workspace,
		         gpointer         user_data)
{
	GpmPrefs *prefs;

	gpm_debug ("GtkUnique message %i", command);
	prefs = GPM_PREFS (user_data);
	if (command == GTK_UNIQUE_ACTIVATE) {
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
	GpmPrefs *prefs = NULL;
	GMainLoop *loop;
#if HAVE_GTKUNIQUE
	GtkUniqueApp *uniqueapp;
	const gchar *startup_id = NULL;
#endif

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	context = g_option_context_new (N_("GNOME Power Preferences"));

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);

#if HAVE_GTKUNIQUE
	/* FIXME: We don't need to get the startup id once we can
	 * depend on gtk+-2.12.  Until then we must get it BEFORE
	 * gtk_init() is called, otherwise gtk_init() will clear it
	 * and GtkUnique has to use racy workarounds.
	 */
	startup_id = g_getenv ("DESKTOP_STARTUP_ID");
#endif
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_parse (context, &argc, &argv, NULL);


	gpm_debug_init (verbose);

#if HAVE_GTKUNIQUE
	gpm_debug ("Using GtkUnique support.");

	/* Arrr! Until we depend on gtk+2 2.12 we can't just use gtk_unique_app_new */
	uniqueapp = gtk_unique_app_new_with_id ("org.freedesktop.PowerManagement.Preferences", startup_id);
	/* check to see if the user has another prefs window open */
	if (gtk_unique_app_is_running (uniqueapp)) {
		gpm_warning ("You have another instance running. "
			     "This program will now close");
		gtk_unique_app_activate (uniqueapp);

		/* FIXME: This next line should be removed once we can depend
		 * upon gtk+-2.12.  This causes the busy cursor and temporary
		 * task in the tasklist to go away too soon (though that's
		 * better than having them be stuck until the 30-second-or-so
		 * timeout ends).
		 */
		gdk_notify_startup_complete ();
	} else {
#else
	gpm_warning ("No GtkUnique support. Cannot signal other instances");
	/* we always assume we have no other running instance */
	if (1) {
#endif
		/* create a new instance of the window */
		prefs = gpm_prefs_new ();

		g_signal_connect (prefs, "action-help",
				  G_CALLBACK (gpm_prefs_help_cb), prefs);
		g_signal_connect (prefs, "action-close",
				  G_CALLBACK (gpm_prefs_close_cb), prefs);
#if HAVE_GTKUNIQUE
		/* Listen for messages from another instances */
		g_signal_connect (G_OBJECT (uniqueapp), "message",
				  G_CALLBACK (gtkuniqueapp_command_cb), prefs);
#endif
		loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (loop);

		g_object_unref (prefs);
	}

	gpm_debug_shutdown ();

#if HAVE_GTKUNIQUE
	g_object_unref (uniqueapp);
#endif
/* seems to not work...
	g_option_context_free (context); */

	return 0;
}
