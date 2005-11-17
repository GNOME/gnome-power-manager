/** @file	gpm-networkmanager.h
 *  @brief	Functions to query and control NetworkManager
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-17
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
 * @addtogroup	nm
 * @{
 */

#ifndef _GPMNETWORKMANAGER_H
#define _GPMNETWORKMANAGER_H

#define NM_LISTENER_SERVICE	"org.freedesktop.NetworkManager"
#define NM_LISTENER_PATH	"/org/freedesktop/NetworkManager"
#define NM_LISTENER_INTERFACE	"org.freedesktop.NetworkManager" //we need?

#define NM_GCONF_ROOT		"/apps/gnome-screensaver/"
#define NM_GCONF_ROOT_NO_SLASH	"/apps/gnome-screensaver"

gboolean gpm_networkmanager_sleep (void);
gboolean gpm_networkmanager_wake (void);

#endif	/* _GPMNETWORKMANAGER_H */
/** @} */
