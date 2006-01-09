/** @file	gpm-common.h
 *  @brief	Common functions shared between modules
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
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

#ifndef _GPMCOMMON_H
#define _GPMCOMMON_H

#include <gnome.h>

#include "gpm-sysdev.h"

/* common descriptions of this program */
#define NICENAME 			_("GNOME Power Manager")
#define NICEDESC 			_("Power Manager for the GNOME desktop")

/* help location */
#define GPMURL	 			"http://www.gnome.org/projects/gnome-power-manager/"

DeviceType hal_to_device_type (const gchar *type);

gchar *get_timestring_from_minutes (gint minutes);


#endif	/* _GPMCOMMON_H */
