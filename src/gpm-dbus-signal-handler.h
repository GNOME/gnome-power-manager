/** @file	gpm-dbus-signal-handler.h
 *  @brief	DBUS signal handler for NameOwnerChanged and NameLost
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

#ifndef _GPMDBUSSIGNALHANDLER_H
#define _GPMDBUSSIGNALHANDLER_H

typedef void (*GpmDbusSignalHandler) (const gchar *name, const gboolean connected);

gboolean gpm_dbus_init_noc (DBusGConnection *connection, GpmDbusSignalHandler callback);
gboolean gpm_dbus_init_nlost (DBusGConnection *connection, GpmDbusSignalHandler callback);

gboolean gpm_dbus_remove_noc ();
gboolean gpm_dbus_remove_nlost ();

#endif	/* _GPMDBUSSIGNALHANDLER_H */
/** @} */
