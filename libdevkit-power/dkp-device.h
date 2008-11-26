/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#ifndef __DKP_DEVICE_H
#define __DKP_DEVICE_H

#include <glib-object.h>
#include <dkp-enum.h>
#include <dkp-object.h>
#include "egg-obj-list.h"

G_BEGIN_DECLS

#define DKP_TYPE_DEVICE		(dkp_device_get_type ())
#define DKP_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_DEVICE, DkpDevice))
#define DKP_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_DEVICE, DkpDeviceClass))
#define DKP_IS_DEVICE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_DEVICE))
#define DKP_IS_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_DEVICE))
#define DKP_DEVICE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_DEVICE, DkpDeviceClass))
#define DKP_DEVICE_ERROR	(dkp_device_error_quark ())
#define DKP_DEVICE_TYPE_ERROR	(dkp_device_error_get_type ())

typedef struct DkpDevicePrivate DkpDevicePrivate;

typedef struct
{
	 GObject		 parent;
	 DkpDevicePrivate	*priv;
} DkpDevice;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*changed)		(DkpDevice		*device,
							 const DkpObject	*obj);
} DkpDeviceClass;

GType		 dkp_device_get_type			(void) G_GNUC_CONST;
DkpDevice	*dkp_device_new				(void);

const DkpObject	*dkp_device_get_object			(const DkpDevice	*device);
const gchar	*dkp_device_get_object_path		(const DkpDevice	*device);
gboolean	 dkp_device_set_object_path		(DkpDevice		*device,
							 const gchar		*object_path);

gboolean	 dkp_device_print			(const DkpDevice	*device);
gboolean	 dkp_device_refresh			(DkpDevice		*device);
EggObjList	*dkp_device_get_history			(const DkpDevice	*device,
							 const gchar		*type,
							 guint			 timespec,
							 guint			 resolution);
EggObjList	*dkp_device_get_statistics		(const DkpDevice	*device,
							 const gchar		*type);

G_END_DECLS

#endif /* __DKP_DEVICE_H */

