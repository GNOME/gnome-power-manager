/***************************************************************************
 *
 * gpm.h : gnome-power-manager
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 * Taken in part from:
 * - notibat (C) 2004 Benjamin Kahn <xkahn@zoned.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

#ifndef _GPM_H
#define _GPM_H

#define QUICK_AC 	1
#define G_DISABLE_ASSERT 0

/* where our settings are stored in the gconf tree */
#define GCONF_ROOT_SANS_SLASH	"/apps/gnome-power"
#define GCONF_ROOT		GCONF_ROOT_SANS_SLASH "/"
#define SELECTION_NAME 		"_GPM_SELECTION"

#define NICENAME 		_("GNOME Power Manager")
#define NICEDESC 		_("Power Manager for the GNOME desktop")

void action_policy_do (gint policy_number);

#endif	/* _GPM_H */
