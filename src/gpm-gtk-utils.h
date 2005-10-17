/** @file	gpm-gtk-utils.h
 *  @brief	Simple wrapper functions for easy libglade stuff
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-04
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
#ifndef _GPMGTKUTILS_H
#define _GPMGTKUTILS_H

gboolean gpm_gtk_set_visibility (GladeXML *allwidgets, const gchar *widgetname, gboolean set);
gboolean gpm_gtk_set_check (GladeXML *allwidgets, const gchar *widgetname, gboolean set);
gboolean gpm_gtk_set_label (GladeXML *allwidgets, const gchar *widgetname, const gchar *label);
gboolean gpm_gtk_set_sensitive (GladeXML *allwidgets, const gchar *widgetname, gboolean set);

#endif	/* _GPMGTKUTILS_H */
