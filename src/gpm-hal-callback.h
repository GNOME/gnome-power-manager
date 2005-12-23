/** @file	gpm-hal-callback.h
 *  @brief	GLIB replacement for libhal, providing callbacks
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
 * @addtogroup	hal
 * @{
 */

#ifndef _GPMHALCALLBACK_H
#define _GPMHALCALLBACK_H

#include <dbus/dbus-glib.h>

typedef void (*HalDeviceAdded) (const gchar *udi);
typedef void (*HalDeviceRemoved) (const gchar *udi);
typedef void (*HalDeviceNewCapability) (const gchar *udi, const gchar *capability);
typedef void (*HalDeviceLostCapability) (const gchar *udi, const gchar *capability);
typedef void (*HalDevicePropertyModified) (const gchar *udi, const gchar *key, gboolean removed, gboolean added);
typedef void (*HalDeviceCondition) (const gchar *udi, const gchar *name, const gchar *detail);

typedef void (*HalNameOwnerChanged) (const gchar *name, gboolean connected);

/** The stored callback functions */
typedef struct {
	HalDeviceAdded			device_added;
	HalDeviceRemoved		device_removed;
	HalDeviceNewCapability		device_new_capability;
	HalDeviceLostCapability		device_lost_capability;
	HalDevicePropertyModified	device_property_modified;
	HalDeviceCondition		device_condition;
} HalFunctions;

/** If the watch has been registered */
typedef struct {
	gboolean			device_added;
	gboolean			device_removed;
	gboolean			device_new_capability;
	gboolean			device_lost_capability;
	gboolean			device_condition;
} HalRegistered;

/** The DBUS connections used by each watch */
typedef struct {
	DBusGProxy			*device_added;
	DBusGProxy			*device_removed;
	DBusGProxy			*device_new_capability;
	DBusGProxy			*device_lost_capability;
	GPtrArray 			*device_condition;
	GPtrArray 			*device_property_modified;
} HalConnections;

/** The UDI linked list object for PropertyModified */
typedef struct {
	gchar				udi[128];
	DBusGProxy			*proxy;
} UdiProxy;

gboolean gpm_hal_callback_init (void);
gboolean gpm_hal_callback_shutdown (void);

gboolean gpm_hal_method_device_removed (HalDeviceRemoved callback);
gboolean gpm_hal_method_device_added (HalDeviceAdded callback);
gboolean gpm_hal_method_device_new_capability (HalDeviceNewCapability callback);
gboolean gpm_hal_method_device_lost_capability (HalDeviceLostCapability callback);
gboolean gpm_hal_method_device_property_modified (HalDevicePropertyModified callback);
gboolean gpm_hal_method_device_condition (HalDeviceCondition callback);

gboolean gpm_hal_watch_add_device_property_modified (const gchar *udi);
gboolean gpm_hal_watch_add_device_condition (const gchar *udi);

gboolean gpm_hal_watch_remove_device_removed ();
gboolean gpm_hal_watch_remove_device_added ();
gboolean gpm_hal_watch_remove_device_new_capability ();
gboolean gpm_hal_watch_remove_device_lost_capability ();
gboolean gpm_hal_watch_remove_device_property_modified (const gchar *udi);
gboolean gpm_hal_watch_remove_device_condition (const gchar *udi);

#endif	/* _GPMHALCALLBACK_H */
/** @} */
