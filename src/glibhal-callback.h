/** @file	glibhal-callback.h
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
 * @addtogroup	glibhal
 * @{
 */

#ifndef _GLIBHALCALLBACK_H
#define _GLIBHALCALLBACK_H

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
	HalNameOwnerChanged		device_noc;
} HalFunctions;

/** If the watch has been registered */
typedef struct {
	gboolean			device_added;
	gboolean			device_removed;
	gboolean			device_new_capability;
	gboolean			device_lost_capability;
	gboolean			device_condition;
	gboolean			device_noc;
} HalRegistered;

/** The DBUS connections used by each watch */
typedef struct {
	DBusGProxy			*device_added;
	DBusGProxy			*device_removed;
	DBusGProxy			*device_new_capability;
	DBusGProxy			*device_lost_capability;
	DBusGProxy			*device_noc;
	GPtrArray 			*device_condition;
	GPtrArray 			*device_property_modified;
} HalConnections;

/** The UDI linked list object for PropertyModified */
typedef struct {
	gchar				udi[128];
	DBusGProxy			*proxy;
} UdiProxy;

gboolean glibhal_callback_init (void);
gboolean glibhal_callback_shutdown (void);

gboolean glibhal_method_device_removed (HalDeviceRemoved callback);
gboolean glibhal_method_device_added (HalDeviceAdded callback);
gboolean glibhal_method_device_new_capability (HalDeviceNewCapability callback);
gboolean glibhal_method_device_lost_capability (HalDeviceLostCapability callback);
gboolean glibhal_method_device_property_modified (HalDevicePropertyModified callback);
gboolean glibhal_method_device_condition (HalDeviceCondition callback);
gboolean glibhal_method_noc (HalNameOwnerChanged callback);

gboolean glibhal_watch_add_device_property_modified (const gchar *udi);
gboolean glibhal_watch_add_device_condition (const gchar *udi);

gboolean glibhal_watch_remove_device_removed ();
gboolean glibhal_watch_remove_device_added ();
gboolean glibhal_watch_remove_device_new_capability ();
gboolean glibhal_watch_remove_device_lost_capability ();
gboolean glibhal_watch_remove_noc ();
gboolean glibhal_watch_remove_device_property_modified (const gchar *udi);
gboolean glibhal_watch_remove_device_condition (const gchar *udi);

#endif	/* _GLIBHALCALLBACK_H */
/** @} */
