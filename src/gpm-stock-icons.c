/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Jorn Baayen
 * Copyright (C) 2003,2004 Colin Walters <walters@verbum.org>
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

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "gpm-stock-icons.h"
#include "gpm-debug.h"

static GtkIconFactory *factory = NULL;

typedef struct {
	const gchar *name;
	gboolean     custom;
} GpmStockIcon;

gboolean
gpm_stock_icons_init (void)
{
	GtkIconTheme *theme = gtk_icon_theme_get_default ();
	guint i;

	static const GpmStockIcon items [] = {
		{ GPM_STOCK_HIBERNATE, FALSE },
		{ GPM_STOCK_SUSPEND, FALSE },
	};

	g_return_val_if_fail (factory == NULL, FALSE);

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);

	for (i = 0; i < (guint) G_N_ELEMENTS (items); i++) {
		GtkIconSet *icon_set;
		GdkPixbuf  *pixbuf;

		if (items[i].custom) {
			gchar *filename;

			filename = g_strconcat (GPM_DATA, G_DIR_SEPARATOR_S, items[i].name, ".png", NULL);
			pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
			g_free (filename);
		} else {
			/* we should really add all the sizes */
			gint size;

			gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &size, NULL);
			pixbuf = gtk_icon_theme_load_icon (theme,
							   items[i].name,
							   size,
							   0,
							   NULL);
		}

		if (pixbuf) {
			icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
			gtk_icon_factory_add (factory, items[i].name, icon_set);
			gtk_icon_set_unref (icon_set);

			g_object_unref (G_OBJECT (pixbuf));
		} else {
			gpm_warning ("Unable to load icon %s", items[i].name);
		}
	}

	return TRUE;
}

void
gpm_stock_icons_shutdown (void)
{
	g_return_if_fail (factory != NULL);

	gtk_icon_factory_remove_default (factory);

	g_object_unref (G_OBJECT (factory));
}
