/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2008 Richard Hughes <richard@hughsie.com>
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

/* local .la */
#include <libunique.h>

#include "gpm-common.h"
#include "gpm-conf.h"
#include "egg-debug.h"
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
	gpm_help_display ("statistics");
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
	gtk_main_quit ();
}

/**
 * gpm_statistics_activated_cb
 * @statistics: This statistics class instance
 *
 * We have been asked to show the window
 **/
static void
gpm_statistics_activated_cb (LibUnique *libunique, GpmStatistics *statistics)
{
	gpm_statistics_activate_window (statistics);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean verbose = FALSE;
	GOptionContext *context;
	GpmStatistics *statistics = NULL;
	gboolean ret;
	LibUnique *libunique;

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	context = g_option_context_new (N_("GNOME Power Statistics"));

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_parse (context, &argc, &argv, NULL);

	gtk_init (&argc, &argv);
	egg_debug_init (verbose);

	/* are we already activated? */
	libunique = libunique_new ();
	ret = libunique_assign (libunique, "org.freedesktop.PowerManagement.Statistics");
	if (!ret) {
		goto unique_out;
	}

	statistics = gpm_statistics_new ();
	g_signal_connect (libunique, "activated",
			  G_CALLBACK (gpm_statistics_activated_cb), statistics);
	g_signal_connect (statistics, "action-help",
			  G_CALLBACK (gpm_statistics_help_cb), statistics);
	g_signal_connect (statistics, "action-close",
			  G_CALLBACK (gpm_statistics_close_cb), statistics);
	gtk_main ();
	g_object_unref (statistics);

unique_out:
	g_object_unref (libunique);

/* seems to not work...
	g_option_context_free (context); */

	return 0;
}
