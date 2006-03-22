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
	const char *name;
	gboolean    custom;
} GpmStockIcon;

gboolean
gpm_stock_icons_init (void)
{
	GtkIconTheme *theme = gtk_icon_theme_get_default ();
	int           i;

	static const GpmStockIcon items [] = {
		/* GPM custom icons */
		{ GPM_STOCK_AC_ADAPTER, TRUE },
		{ GPM_STOCK_KEYBOARD_LOW, TRUE },
		{ GPM_STOCK_MOUSE_LOW, TRUE },
		{ GPM_STOCK_BATTERY_BROKEN, TRUE },
		{ GPM_STOCK_BATTERY_CHARGED, TRUE },
		{ GPM_STOCK_BATTERY_CHARGING_000, TRUE },
		{ GPM_STOCK_BATTERY_CHARGING_020, TRUE },
		{ GPM_STOCK_BATTERY_CHARGING_040, TRUE },
		{ GPM_STOCK_BATTERY_CHARGING_060, TRUE },
		{ GPM_STOCK_BATTERY_CHARGING_080, TRUE },
		{ GPM_STOCK_BATTERY_CHARGING_100, TRUE },
		{ GPM_STOCK_BATTERY_DISCHARGING_000, TRUE },
		{ GPM_STOCK_BATTERY_DISCHARGING_020, TRUE },
		{ GPM_STOCK_BATTERY_DISCHARGING_040, TRUE },
		{ GPM_STOCK_BATTERY_DISCHARGING_060, TRUE },
		{ GPM_STOCK_BATTERY_DISCHARGING_080, TRUE },
		{ GPM_STOCK_BATTERY_DISCHARGING_100, TRUE },
		{ GPM_STOCK_UPS_BROKEN, TRUE },
		{ GPM_STOCK_UPS_CHARGED, TRUE },
		{ GPM_STOCK_UPS_CHARGING_000, TRUE },
		{ GPM_STOCK_UPS_CHARGING_020, TRUE },
		{ GPM_STOCK_UPS_CHARGING_040, TRUE },
		{ GPM_STOCK_UPS_CHARGING_060, TRUE },
		{ GPM_STOCK_UPS_CHARGING_080, TRUE },
		{ GPM_STOCK_UPS_CHARGING_100, TRUE },
		{ GPM_STOCK_UPS_DISCHARGING_000, TRUE },
		{ GPM_STOCK_UPS_DISCHARGING_020, TRUE },
		{ GPM_STOCK_UPS_DISCHARGING_040, TRUE },
		{ GPM_STOCK_UPS_DISCHARGING_060, TRUE },
		{ GPM_STOCK_UPS_DISCHARGING_080, TRUE },
		{ GPM_STOCK_UPS_DISCHARGING_100, TRUE },
		{ GPM_STOCK_POWER_INFORMATION, TRUE },
		{ GPM_STOCK_SUSPEND_TO_DISK, TRUE },
		{ GPM_STOCK_SUSPEND_TO_RAM, TRUE },

	};

	g_return_val_if_fail (factory == NULL, FALSE);

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);

	for (i = 0; i < (int) G_N_ELEMENTS (items); i++) {
		GtkIconSet *icon_set;
		GdkPixbuf  *pixbuf;

		if (items[i].custom) {
			char *filename;

			filename = g_strconcat (GPM_DATA, G_DIR_SEPARATOR_S, items[i].name, ".png", NULL);
			pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
			g_free (filename);
		} else {
			/* we should really add all the sizes */
			int size;

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
