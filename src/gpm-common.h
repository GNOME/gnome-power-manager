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

/* common descriptions of this program */
#define GPM_NAME 			_("Power Manager")
#define GPM_DESCRIPTION 		_("Power Manager for the GNOME desktop")

/* help location */
#define GPM_HOMEPAGE_URL	 	"http://www.gnome.org/projects/gnome-power-manager/"
#define GPM_BUGZILLA_URL		"http://bugzilla.gnome.org/buglist.cgi?product=gnome-power-manager"
#define GPM_FAQ_URL			"http://live.gnome.org/GnomePowerManager/Faq"

typedef enum {
	GPM_ICON_POLICY_ALWAYS,
	GPM_ICON_POLICY_PRESENT,
	GPM_ICON_POLICY_CHARGE,
	GPM_ICON_POLICY_CRITICAL,
	GPM_ICON_POLICY_NEVER
} GpmIconPolicy;

gchar		*gpm_get_timestring		(guint		 time);
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
