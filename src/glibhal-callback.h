/***************************************************************************
 *
 * glibhal-callback.h : GLIB replacement for libhal, providing callbacks
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 **************************************************************************/

#ifndef _GLIBHALCALLBACK_H
#define _GLIBHALCALLBACK_H

#include <dbus/dbus-glib.h>

typedef void (*HalDeviceAdded) (const char *udi);
typedef void (*HalDeviceRemoved) (const char *udi);
typedef void (*HalDeviceNewCapability) (const char *udi, const char *capability);
typedef void (*HalDeviceLostCapability) (const char *udi, const char *capability);
typedef void (*HalDevicePropertyModified) (const char *udi, const char *key, gboolean removed, gboolean added);
typedef void (*HalDeviceCondition) (const char *udi, const char *name, const char *detail);

typedef struct {
	gboolean			initialized;
	HalDeviceAdded			device_added;
	HalDeviceRemoved		device_removed;
	HalDeviceNewCapability		device_new_capability;
	HalDeviceLostCapability		device_lost_capability;
	HalDevicePropertyModified	device_property_modified;
	HalDeviceCondition		device_condition;
	gboolean			registered_device_added;
	gboolean			registered_device_removed;
	gboolean			registered_device_new_capability;
	gboolean			registered_device_lost_capability;
	gboolean			registered_device_condition;
} HalContext;

gboolean libhal_glib_init (void);
#if 1
gboolean libhal_device_removed (HalDeviceRemoved callback);
#endif
gboolean libhal_device_added (HalDeviceAdded callback);
gboolean libhal_device_new_capability (HalDeviceNewCapability callback);
gboolean libhal_device_lost_capability (HalDeviceLostCapability callback);
gboolean libhal_device_property_modified (HalDevicePropertyModified callback);
gboolean libhal_device_condition (HalDeviceCondition callback);

void libhal_register_property_modified (const char *udi);
void libhal_register_condition (const char *udi);

#endif	/* _GLIBHALCALLBACK_H */
