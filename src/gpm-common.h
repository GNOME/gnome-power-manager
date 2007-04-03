/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#ifndef __GPMCOMMON_H
#define __GPMCOMMON_H

#include <glib.h>

G_BEGIN_DECLS

#define	GPM_COLOUR_WHITE		0xffffff
#define	GPM_COLOUR_BLACK		0x000000
#define	GPM_COLOUR_RED			0xff0000
#define	GPM_COLOUR_GREEN		0x00ff00
#define	GPM_COLOUR_BLUE			0x0000ff
#define	GPM_COLOUR_CYAN			0x00ffff
#define	GPM_COLOUR_MAGENTA		0xff00ff
#define	GPM_COLOUR_YELLOW		0xffff00
#define	GPM_COLOUR_GREY			0xcccccc
#define	GPM_COLOUR_DARK_RED		0x600000
#define	GPM_COLOUR_DARK_GREEN		0x006000
#define	GPM_COLOUR_DARK_BLUE		0x000060
#define	GPM_COLOUR_DARK_CYAN		0x006060
#define	GPM_COLOUR_DARK_MAGENTA		0x600060
#define	GPM_COLOUR_DARK_YELLOW		0x606000
#define	GPM_COLOUR_DARK_GREY		0x606060

typedef enum {
	GPM_ICON_POLICY_ALWAYS,
	GPM_ICON_POLICY_PRESENT,
	GPM_ICON_POLICY_CHARGE,
	GPM_ICON_POLICY_CRITICAL,
	GPM_ICON_POLICY_NEVER
} GpmIconPolicy;

gchar		*gpm_get_timestring		(guint		 time);
guint		 gpm_precision_round_up		(guint		 value,
						 guint		 smallest);
guint		 gpm_precision_round_down	(guint		 value,
						 guint		 smallest);
guint		 gpm_percent_to_discrete	(guint		 percentage,
						 guint		 levels);
gint		 gpm_exponential_average	(gint		 previous,
						 gint		 new,
						 guint		 slew);
guint		 gpm_discrete_to_percent	(guint		 discrete,
						 guint		 levels);
guint32		 gpm_rgb_to_colour		(guint8		 red,
						 guint8		 green,
						 guint8		 blue);
void		 gpm_colour_to_rgb		(guint32	 colour,
						 guint8		*red,
						 guint8		*green,
						 guint8		*blue);
GpmIconPolicy	 gpm_tray_icon_mode_from_string	(const gchar	*mode);
const gchar	*gpm_tray_icon_mode_to_string	(GpmIconPolicy	 mode);

G_END_DECLS

#endif	/* __GPMCOMMON_H */
