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

#include "config.h"

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-help.h>

#include "gpm-gconf.h"
#include "gpm-debug.h"
#include "gpm-statistics-core.h"

/**
 * gpm_statistics_help_cb
 * @statistics: This statistics class instance
 *
 * What to do when help is requested
 **/
static void
gpm_statistics_help_cb (GpmStatistics *statistics)
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
 * gpm_statistics_close_cb
 * @statistics: This statistics class instance
 *
 * What to do when we are asked to close for whatever reason
 **/
static void
gpm_statistics_close_cb (GpmStatistics *statistics)
{
	g_object_unref (statistics);
	exit (0);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean	 verbose = FALSE;
	GOptionContext  *context;
 	GnomeProgram    *program;
	GpmStatistics	*statistics = NULL;
	GMainLoop       *loop;

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

	statistics = gpm_statistics_new ();

	g_signal_connect (statistics, "action-help",
			  G_CALLBACK (gpm_statistics_help_cb), NULL);
	g_signal_connect (statistics, "action-close",
			  G_CALLBACK (gpm_statistics_close_cb), NULL);

	loop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (loop);

	g_object_unref (statistics);

	gpm_debug_shutdown ();

	g_object_unref (program);
	g_option_context_free (context);

	return 0;
}
