/***************************************************************************
 *
 * glibhal-extras.h : GLIB replacement for libhal, the extra stuff
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

#ifndef _GLIBHALEXTRAS_H
#define _GLIBHALEXTRAS_H

gint hal_get_brightness_steps (void);
gboolean hal_set_brightness (int brightness);
gboolean hal_set_brightness_dim (int brightness);
gboolean hal_suspend (int wakeup);
gboolean hal_hibernate (void);
gboolean hal_setlowpowermode (gboolean set);

#endif	/* _GLIBHALEXTRAS_H */
