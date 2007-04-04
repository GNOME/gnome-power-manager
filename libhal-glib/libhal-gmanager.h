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

#ifndef __LIBHAL_GMANAGER_H
#define __LIBHAL_GMANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define	HAL_DBUS_SERVICE		 	"org.freedesktop.Hal"
#define	HAL_DBUS_PATH_MANAGER		 	"/org/freedesktop/Hal/Manager"
#define	HAL_DBUS_INTERFACE_MANAGER	 	"org.freedesktop.Hal.Manager"
#define	HAL_DBUS_INTERFACE_DEVICE	 	"org.freedesktop.Hal.Device"
#define	HAL_DBUS_INTERFACE_LAPTOP_PANEL	 	"org.freedesktop.Hal.Device.LaptopPanel"
#define	HAL_DBUS_INTERFACE_POWER	 	"org.freedesktop.Hal.Device.SystemPowerManagement"
#define	HAL_DBUS_INTERFACE_CPUFREQ	 	"org.freedesktop.Hal.Device.CPUFreq"
#define	HAL_DBUS_INTERFACE_KBD_BACKLIGHT 	"org.freedesktop.Hal.Device.KeyboardBacklight"
#define	HAL_DBUS_INTERFACE_LIGHT_SENSOR	 	"org.freedesktop.Hal.Device.LightSensor"
#define HAL_ROOT_COMPUTER		 	"/org/freedesktop/Hal/devices/computer"

#define LIBHAL_TYPE_GMANAGER		(hal_gmanager_get_type ())
#define LIBHAL_GMANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LIBHAL_TYPE_GMANAGER, HalGManager))
#define LIBHAL_GMANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), LIBHAL_TYPE_GMANAGER, HalGManagerClass))
#define LIBHAL_IS_GMANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LIBHAL_TYPE_GMANAGER))
#define LIBHAL_IS_GMANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LIBHAL_TYPE_GMANAGER))
#define LIBHAL_GMANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LIBHAL_TYPE_GMANAGER, HalGManagerClass))

typedef struct HalGManagerPrivate HalGManagerPrivate;

typedef struct
{
	GObject		 parent;
	HalGManagerPrivate	*priv;
} HalGManager;

/* Signals emitted from HalGManager are:
 *
 * device-added
 * device-removed
 * new-capability
 * lost-capability
 * daemon-start
 * daemon-stop
 */

typedef struct
{
	GObjectClass	parent_class;
	void		(* device_added)		(HalGManager	*manager,
							 const gchar	*udi);
	void		(* device_removed)		(HalGManager	*manager,
							 const gchar	*udi);
	void		(* new_capability)		(HalGManager	*manager,
							 const gchar	*udi,
							 const gchar	*capability);
	void		(* lost_capability)		(HalGManager	*manager,
							 const gchar	*udi,
							 const gchar	*capability);
	void		(* daemon_start)		(HalGManager	*manager);
	void		(* daemon_stop)			(HalGManager	*manager);
} HalGManagerClass;

GType		 hal_gmanager_get_type			(void);
HalGManager	*hal_gmanager_new			(void);

gboolean	 hal_gmanager_is_running		(HalGManager	*manager);
gint		 hal_gmanager_num_devices_of_capability (HalGManager	*manager,
							 const gchar	*capability);
gint		 hal_gmanager_num_devices_of_capability_with_value (HalGManager *manager,
							 const gchar	*capability,
							 const gchar	*key,
							 const gchar	*value);
gboolean	 hal_gmanager_find_capability		(HalGManager	*manager,
							 const gchar	*capability,
							 gchar     	***value,
							 GError		**error);
gboolean	 hal_gmanager_find_device_string_match	(HalGManager	*manager,
							 const gchar	*key,
							 const gchar	*value,
							 gchar		***devices,
							 GError		**error);
void		 hal_gmanager_free_capability		(gchar		**value);
gboolean	 hal_gmanager_is_laptop			(HalGManager	*manager);

G_END_DECLS

#endif	/* __LIBHAL_GMANAGER_H */
