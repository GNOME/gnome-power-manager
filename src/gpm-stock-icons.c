/** @file	gpm-screensaver.c
 *  @brief	Functions to query and control GNOME Screensaver
 *  @author	2002		Jorn Baayen
 *		2003,2004	Colin Walters <walters@verbum.org>
 *
 * This file registers new custom stock icons so that we can use them in a
 * generic way.
 */
/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "gpm-stock-icons.h"

static GtkIconFactory *factory = NULL;

void
gpm_stock_icons_init (void)
{
	int i;

	static const char *items[] =
	{
		GPM_STOCK_AC_0_OF_8, GPM_STOCK_AC_1_OF_8, GPM_STOCK_AC_2_OF_8,
		GPM_STOCK_AC_3_OF_8, GPM_STOCK_AC_4_OF_8, GPM_STOCK_AC_5_OF_8,
		GPM_STOCK_AC_6_OF_8, GPM_STOCK_AC_7_OF_8, GPM_STOCK_AC_8_OF_8,
		GPM_STOCK_BAT_0_OF_8, GPM_STOCK_BAT_1_OF_8, GPM_STOCK_BAT_2_OF_8,
		GPM_STOCK_BAT_3_OF_8, GPM_STOCK_BAT_4_OF_8, GPM_STOCK_BAT_5_OF_8,
		GPM_STOCK_BAT_6_OF_8, GPM_STOCK_BAT_7_OF_8, GPM_STOCK_BAT_8_OF_8,
		GPM_STOCK_UPS_0_OF_8, GPM_STOCK_UPS_1_OF_8, GPM_STOCK_UPS_2_OF_8,
		GPM_STOCK_UPS_3_OF_8, GPM_STOCK_UPS_4_OF_8, GPM_STOCK_UPS_5_OF_8,
		GPM_STOCK_UPS_6_OF_8, GPM_STOCK_UPS_7_OF_8, GPM_STOCK_UPS_8_OF_8,
		GPM_STOCK_AC_ADAPTER, GPM_STOCK_MOUSE, GPM_STOCK_KEYBOARD
	};

	g_return_if_fail (factory == NULL);

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);

	for (i = 0; i < (int) G_N_ELEMENTS (items); i++) {
		GtkIconSet *icon_set;
		GdkPixbuf *pixbuf;
		char *file_name;

		file_name = g_strconcat (GPM_DATA, items[i], ".png", NULL);
		pixbuf = gdk_pixbuf_new_from_file (file_name, NULL);
		g_free (file_name);

		icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
		gtk_icon_factory_add (factory, items[i], icon_set);
		gtk_icon_set_unref (icon_set);
		
		g_object_unref (G_OBJECT (pixbuf));
	}
}


void
gpm_stock_icons_shutdown (void)
{
	g_return_if_fail (factory != NULL);

	gtk_icon_factory_remove_default (factory);

	g_object_unref (G_OBJECT (factory));
}
