/** @file	gpm-dbus-client.h
 *  @brief	Common DBUS client stuff for g-p-m and g-p-p
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
/**
 * @addtogroup	dbus
 * @{
 */

#ifndef _GPMDBUSCLIENT_H
#define _GPMDBUSCLIENT_H

#include <dbus/dbus-glib.h>

gboolean gpm_is_on_ac (gboolean *value);
gboolean gpm_is_on_mains (gboolean *value);

#endif	/* _GPMDBUSCLIENT_H */
/** @} */
