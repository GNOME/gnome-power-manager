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

#ifndef __GPMHAL_H
#define __GPMHAL_H

#include <glib.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define	HAL_DBUS_SERVICE		"org.freedesktop.Hal"
#define	HAL_DBUS_PATH_MANAGER		"/org/freedesktop/Hal/Manager"
#define	HAL_DBUS_INTERFACE_MANAGER	"org.freedesktop.Hal.Manager"
#define	HAL_DBUS_INTERFACE_DEVICE	"org.freedesktop.Hal.Device"
#define	HAL_DBUS_INTERFACE_LAPTOP_PANEL	"org.freedesktop.Hal.Device.LaptopPanel"
#define	HAL_DBUS_INTERFACE_POWER	"org.freedesktop.Hal.Device.SystemPowerManagement"
#define HAL_ROOT_COMPUTER		"/org/freedesktop/Hal/devices/computer"

G_BEGIN_DECLS

gboolean	 gpm_hal_get_dbus_connection		(DBusGConnection **connection);

gboolean	 gpm_hal_has_power_management		(void);

gboolean	 gpm_hal_is_running			(void);
gboolean	 gpm_hal_is_laptop			(void);
gboolean	 gpm_hal_is_on_ac			(void);
gboolean	 gpm_hal_can_suspend			(void);
gboolean	 gpm_hal_suspend			(gint		 wakeup);
gboolean	 gpm_hal_can_hibernate			(void);
gboolean	 gpm_hal_hibernate			(void);
gboolean	 gpm_hal_shutdown			(void);
gboolean	 gpm_hal_reboot				(void);
gboolean	 gpm_hal_enable_power_save		(gboolean	 enable);
gboolean	 gpm_hal_device_get_bool		(const gchar	*udi,
							 const gchar	*key,
							 gboolean	*value);
gboolean	 gpm_hal_device_get_string		(const gchar	*udi,
							 const gchar	*key,
							 gchar		**value);
gboolean	 gpm_hal_device_get_int			(const gchar	*udi,
							 const gchar	*key,
							 gint		*value);
gboolean	 gpm_hal_find_device_capability		(const gchar	*capability,
							 gchar		***value);

gint		 gpm_hal_num_devices_of_capability	(const gchar	*capability);
gint		 gpm_hal_num_devices_of_capability_with_value (const gchar *capability,
							 const gchar	*key,
							 const gchar	*value);
void		 gpm_hal_free_capability		(gchar		**value);

G_END_DECLS

#endif	/* __GPMHAL_H */
