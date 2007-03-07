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

#ifndef __LIBHAL_GDEVICESTORE_H
#define __LIBHAL_GDEVICESTORE_H

#include <glib-object.h>
#include "libhal-gdevice.h"

G_BEGIN_DECLS

#define LIBHAL_TYPE_GDEVICESTORE		(hal_gdevicestore_get_type ())
#define LIBHAL_GDEVICESTORE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), LIBHAL_TYPE_GDEVICESTORE, HalGDevicestore))
#define LIBHAL_GDEVICESTORE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LIBHAL_TYPE_GDEVICESTORE, HalGDevicestoreClass))
#define LIBHAL_IS_GDEVICESTORE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LIBHAL_TYPE_GDEVICESTORE))
#define LIBHAL_IS_GDEVICESTORE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), LIBHAL_TYPE_GDEVICESTORE))
#define LIBHAL_GDEVICESTORE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LIBHAL_TYPE_GDEVICESTORE, HalGDevicestoreClass))

typedef struct HalGDevicestorePrivate HalGDevicestorePrivate;

typedef struct
{
	GObject		     parent;
	HalGDevicestorePrivate *priv;
} HalGDevicestore;

typedef struct
{
	GObjectClass	parent_class;
	void		(* device_removed)		(HalGDevicestore *devicestore,
							 HalGDevice	 *device);
} HalGDevicestoreClass;

GType		 hal_gdevicestore_get_type		(void);
HalGDevicestore	*hal_gdevicestore_new			(void);

HalGDevice	*hal_gdevicestore_find_udi		(HalGDevicestore *devicestore,
							 const gchar	 *udi);
gboolean	 hal_gdevicestore_insert		(HalGDevicestore *devicestore,
							 HalGDevice	 *device);
gboolean	 hal_gdevicestore_present		(HalGDevicestore *devicestore,
							 HalGDevice	 *device);
gboolean	 hal_gdevicestore_remove		(HalGDevicestore *devicestore,
							 HalGDevice	 *device);
gboolean	 hal_gdevicestore_print			(HalGDevicestore *devicestore);

G_END_DECLS

#endif	/* __LIBHAL_GDEVICESTORE_H */
