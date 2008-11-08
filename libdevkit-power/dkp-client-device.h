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

#ifndef __DKP_CLIENT_DEVICE_H
#define __DKP_CLIENT_DEVICE_H

#include <glib-object.h>
#include <dkp-enum.h>
#include <dkp-object.h>
#include "egg-obj-list.h"

G_BEGIN_DECLS

#define DKP_TYPE_CLIENT_DEVICE		(dkp_client_device_get_type ())
#define DKP_CLIENT_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_CLIENT_DEVICE, DkpClientDevice))
#define DKP_CLIENT_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_CLIENT_DEVICE, DkpClientDeviceClass))
#define DKP_IS_CLIENT_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_CLIENT_DEVICE))
#define DKP_IS_CLIENT_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_CLIENT_DEVICE))
#define DKP_CLIENT_DEVICE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_CLIENT_DEVICE, DkpClientDeviceClass))
#define DKP_CLIENT_DEVICE_ERROR		(dkp_client_device_error_quark ())
#define DKP_CLIENT_DEVICE_TYPE_ERROR	(dkp_client_device_error_get_type ())

typedef struct DkpClientDevicePrivate DkpClientDevicePrivate;

typedef struct
{
	 GObject		 parent;
	 DkpClientDevicePrivate	*priv;
} DkpClientDevice;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*changed)		(DkpClientDevice	*device,
							 const DkpObject	*obj);
} DkpClientDeviceClass;

GType		 dkp_client_device_get_type		(void) G_GNUC_CONST;
DkpClientDevice	*dkp_client_device_new			(void);

const DkpObject	*dkp_client_device_get_object		(const DkpClientDevice	*device);
const gchar	*dkp_client_device_get_object_path	(const DkpClientDevice	*device);
gboolean	 dkp_client_device_set_object_path	(DkpClientDevice	*device,
							 const gchar		*object_path);

gboolean	 dkp_client_device_print		(const DkpClientDevice	*device);
gboolean	 dkp_client_device_refresh		(DkpClientDevice	*device);
EggObjList	*dkp_client_device_get_history		(const DkpClientDevice	*device,
							 const gchar		*type,
							 guint			 timespec,
							 guint			 resolution);
EggObjList	*dkp_client_device_get_statistics	(const DkpClientDevice	*device,
							 const gchar		*type);

G_END_DECLS

#endif /* __DKP_CLIENT_DEVICE_H */

