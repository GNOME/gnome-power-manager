/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

typedef enum {
	GPM_SOUND_AC_UNPLUGGED,
	GPM_SOUND_POWER_LOW,
	GPM_SOUND_SUSPEND_FAILURE,
	GPM_SOUND_LAST
} GpmSound;

gchar		*gpm_get_timestring		(guint		time);
void		 gpm_warning_beep		(void);
guint		 gpm_percent_to_discrete	(guint		percentage,
						 guint		levels);
guint		 gpm_discrete_to_percent	(guint		discrete,
						 guint		levels);
GpmIconPolicy	 gpm_tray_icon_mode_from_string	(const gchar	*mode);
const gchar	*gpm_tray_icon_mode_to_string	(GpmIconPolicy	 mode);
void		 gpm_event_sound		(GpmSound sound);

G_END_DECLS

#endif	/* __GPMCOMMON_H */
