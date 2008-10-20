/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __HAL_DEVICE_STORE_H
#define __HAL_DEVICE_STORE_H

#include <glib-object.h>
#include "hal-device.h"

G_BEGIN_DECLS

#define HAL_TYPE_DEVICE_STORE		(hal_device_store_get_type ())
#define HAL_DEVICE_STORE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), HAL_TYPE_DEVICE_STORE, HalDevicestore))
#define HAL_DEVICE_STORE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), HAL_TYPE_DEVICE_STORE, HalDevicestoreClass))
#define HAL_IS_DEVICE_STORE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), HAL_TYPE_DEVICE_STORE))
#define HAL_IS_DEVICE_STORE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), HAL_TYPE_DEVICE_STORE))
#define HAL_DEVICE_STORE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), HAL_TYPE_DEVICE_STORE, HalDevicestoreClass))

typedef struct HalDevicestorePrivate HalDevicestorePrivate;

typedef struct
{
	GObject		     parent;
	HalDevicestorePrivate *priv;
} HalDevicestore;

typedef struct
{
	GObjectClass	parent_class;
	void		(* device_removed)	(HalDevicestore *device_store,
						 HalDevice	 *device);
} HalDevicestoreClass;

GType		 hal_device_store_get_type	(void);
HalDevicestore	*hal_device_store_new		(void);

HalDevice	*hal_device_store_find_udi	(HalDevicestore *device_store,
						 const gchar	 *udi);
gboolean	 hal_device_store_insert	(HalDevicestore *device_store,
						 HalDevice	 *device);
gboolean	 hal_device_store_present	(HalDevicestore *device_store,
						 HalDevice	 *device);
gboolean	 hal_device_store_remove	(HalDevicestore *device_store,
						 HalDevice	 *device);
gboolean	 hal_device_store_print		(HalDevicestore *device_store);

G_END_DECLS

#endif	/* __HAL_DEVICE_STORE_H */
