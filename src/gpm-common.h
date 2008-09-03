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


#define	GPM_DBUS_SERVICE		"org.freedesktop.PowerManagement"
#define	GPM_DBUS_INTERFACE		"org.freedesktop.PowerManagement"
#define	GPM_DBUS_INTERFACE_WIDGET	"org.freedesktop.PowerManagement.Widget"
#define	GPM_DBUS_INTERFACE_BACKLIGHT	"org.freedesktop.PowerManagement.Backlight"
#define	GPM_DBUS_INTERFACE_STATS	"org.freedesktop.PowerManagement.Statistics"
#define	GPM_DBUS_INTERFACE_INHIBIT	"org.freedesktop.PowerManagement.Inhibit"
#define	GPM_DBUS_PATH			"/org/freedesktop/PowerManagement"
#define	GPM_DBUS_PATH_BACKLIGHT		"/org/freedesktop/PowerManagement/Backlight"
#define	GPM_DBUS_PATH_WIDGET		"/org/freedesktop/PowerManagement/Widget"
#define	GPM_DBUS_PATH_STATS		"/org/freedesktop/PowerManagement/Statistics"
#define	GPM_DBUS_PATH_INHIBIT		"/org/freedesktop/PowerManagement/Inhibit"

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
gfloat		 gpm_discrete_to_fraction	(guint		 discrete,
						 guint		 levels);
GpmIconPolicy	 gpm_tray_icon_mode_from_string	(const gchar	*mode);
const gchar	*gpm_tray_icon_mode_to_string	(GpmIconPolicy	 mode);
void 		 gpm_help_display		(char		*link_id);

G_END_DECLS

#endif	/* __GPMCOMMON_H */
