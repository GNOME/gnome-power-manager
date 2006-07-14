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

#include <glib-object.h>

G_BEGIN_DECLS

#define	HAL_DBUS_SERVICE		"org.freedesktop.Hal"
#define	HAL_DBUS_PATH_MANAGER		"/org/freedesktop/Hal/Manager"
#define	HAL_DBUS_INTERFACE_MANAGER	"org.freedesktop.Hal.Manager"
#define	HAL_DBUS_INTERFACE_DEVICE	"org.freedesktop.Hal.Device"
#define	HAL_DBUS_INTERFACE_LAPTOP_PANEL	"org.freedesktop.Hal.Device.LaptopPanel"
#define	HAL_DBUS_INTERFACE_POWER	"org.freedesktop.Hal.Device.SystemPowerManagement"
#define HAL_ROOT_COMPUTER		"/org/freedesktop/Hal/devices/computer"

#define GPM_TYPE_HAL		(gpm_hal_get_type ())
#define GPM_HAL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_HAL, GpmHal))
#define GPM_HAL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_HAL, GpmHalClass))
#define GPM_IS_HAL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_HAL))
#define GPM_IS_HAL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_HAL))
#define GPM_HAL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_HAL, GpmHalClass))

typedef struct GpmHalPrivate GpmHalPrivate;

typedef struct
{
	GObject		 parent;
	GpmHalPrivate	*priv;
} GpmHal;

/* Signals emitted from GpmHal are:
 *
 * device-added
 * device-removed
 * device-property-modified
 * device-condition
 * new-capability
 * lost-capability
 * daemon-start
 * daemon-stop
 */

typedef struct
{
	GObjectClass	parent_class;
	void		(* device_added)		(GpmHal		*hal,
							 const char	*udi);
	void		(* device_removed)		(GpmHal		*hal,
							 const char	*udi);
	void		(* device_property_modified)	(GpmHal		*hal,
							 const char	*udi,
							 const char	*key,
							 gboolean	 is_added,
							 gboolean	 is_removed,
							 gboolean	 finally);
	void		(* device_condition)		(GpmHal		*hal,
							 const char	*udi,
							 const char	*condition,
							 const char	*details);
	void		(* new_capability)		(GpmHal		*hal,
							 const char	*udi,
							 const char	*capability);
	void		(* lost_capability)		(GpmHal		*hal,
							 const char	*udi,
							 const char	*capability);
	void		(* daemon_start)		(GpmHal		*hal);
	void		(* daemon_stop)			(GpmHal		*hal);
} GpmHalClass;

GType		 gpm_hal_get_type			(void);
GpmHal		*gpm_hal_new				(void);
gboolean	 gpm_hal_get_idle			(GpmHal		*hal);

gboolean	 gpm_hal_has_power_management		(GpmHal		*hal);
gboolean	 gpm_hal_is_running			(GpmHal		*hal);
gboolean	 gpm_hal_is_laptop			(GpmHal		*hal);
gboolean	 gpm_hal_is_on_ac			(GpmHal		*hal);
gboolean	 gpm_hal_can_suspend			(GpmHal		*hal);
gboolean	 gpm_hal_suspend			(GpmHal		*hal,
							 gint		 wakeup);
gboolean	 gpm_hal_can_hibernate			(GpmHal		*hal);
gboolean	 gpm_hal_hibernate			(GpmHal		*hal);
gboolean	 gpm_hal_shutdown			(GpmHal		*hal);
gboolean	 gpm_hal_reboot				(GpmHal		*hal);
gboolean	 gpm_hal_enable_power_save		(GpmHal		*hal,
							 gboolean	 enable);
gboolean	 gpm_hal_device_get_bool		(GpmHal		*hal,
							 const gchar	*udi,
							 const gchar	*key,
							 gboolean	*value);
gboolean	 gpm_hal_device_get_string		(GpmHal		*hal,
							 const gchar	*udi,
							 const gchar	*key,
							 gchar		**value);
gboolean	 gpm_hal_device_get_int			(GpmHal		*hal,
							 const gchar	*udi,
							 const gchar	*key,
							 gint		*value);
gboolean	 gpm_hal_device_find_capability		(GpmHal		*hal,
							 const gchar	*capability,
							 gchar	      ***value);
gboolean	 gpm_hal_device_rescan_capability	(GpmHal		*hal,
							 const char	*capability);
gint		 gpm_hal_num_devices_of_capability	(GpmHal		*hal,
							 const gchar	*capability);
gint		 gpm_hal_num_devices_of_capability_with_value (GpmHal	*hal,
							 const gchar	*capability,
							 const gchar	*key,
							 const gchar	*value);
void		 gpm_hal_free_capability		(GpmHal		*hal,
							 gchar		**value);
gboolean	 gpm_hal_device_watch_condition		(GpmHal		*hal,
							 const char	*udi);
gboolean	 gpm_hal_device_watch_propery_modified	(GpmHal		*hal,
							 const char	*udi);
gboolean	 gpm_hal_device_remove_condition	(GpmHal		*hal,
							 const char	*udi);
gboolean	 gpm_hal_device_remove_propery_modified	(GpmHal		*hal,
							 const char	*udi);

G_END_DECLS

#endif	/* __GPMHAL_H */
