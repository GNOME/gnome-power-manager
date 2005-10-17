/** @file	glibhal-main.h
 *  @brief	GLIB replacement for libhal, the extra stuff
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
 * @addtogroup	glibhal
 * @{
 */

#ifndef _GLIBHALMAIN_H
#define _GLIBHALMAIN_H

#include <dbus/dbus-glib.h>

gboolean is_hald_running (void);
gboolean hal_pm_check (void);
gboolean hal_device_get_bool (const gchar *udi, const gchar *key, gboolean *value);
gboolean hal_device_get_string (const gchar *udi, const gchar *key, gchar **value);
gboolean hal_device_get_int (const gchar *udi, const gchar *key, gint *value);
gboolean hal_find_device_capability (const gchar *capability, gchar ***value);
gint hal_num_devices_of_capability (const gchar *capability);
gint hal_num_devices_of_capability_with_value (const gchar *capability, const gchar *key, const gchar *value);
void hal_free_capability (gchar **value);

#endif	/* _GLIBHALMAIN_H */
/** @} */
