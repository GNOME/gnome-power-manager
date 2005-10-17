/** @file	gpm-dbus-common.h
 *  @brief	Common GLIB DBUS routines
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
/**
 * @addtogroup	dbus
 * @{
 */

#ifndef _DBUSCOMMON_H
#define _DBUSCOMMON_H

#include <dbus/dbus-glib.h>
#include "compiler.h"

gboolean dbus_glib_error (GError *error);
gboolean dbus_get_system_connection (DBusGConnection **connection) G_GNUC_WARNUNCHECKED;
gboolean dbus_get_session_connection (DBusGConnection **connection) G_GNUC_WARNUNCHECKED;
gboolean dbus_get_service (DBusGConnection *connection, const gchar *service) G_GNUC_WARNUNCHECKED;

#endif	/* _DBUSCOMMON_H */
/** @} */
