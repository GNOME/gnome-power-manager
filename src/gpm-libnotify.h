/***************************************************************************
 *
 * gpm-libnotify.h : LibNotify shared code
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 **************************************************************************/

#ifndef _GPMLIBNOTIFY_H
#define _GPMLIBNOTIFY_H

#define LIBNOTIFY_URGENCY_CRITICAL	1
#define LIBNOTIFY_URGENCY_NORMAL	2
#define LIBNOTIFY_URGENCY_LOW		3

gboolean libnotify_init (const gchar *nicename);
gboolean libnotify_event (const gchar *content, const gint urgency, GtkWidget *point);

#endif	/* _GPMLIBNOTIFY_H */
