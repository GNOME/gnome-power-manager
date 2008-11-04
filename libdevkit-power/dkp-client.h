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

#ifndef __DKP_CLIENT_H
#define __DKP_CLIENT_H

#include <glib-object.h>
#include <dkp-enum.h>
#include "dkp-client-device.h"

G_BEGIN_DECLS

#define DKP_TYPE_CLIENT			(dkp_client_get_type ())
#define DKP_CLIENT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_CLIENT, DkpClient))
#define DKP_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_CLIENT, DkpClientClass))
#define DKP_IS_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_CLIENT))
#define DKP_IS_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_CLIENT))
#define DKP_CLIENT_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_CLIENT, DkpClientClass))
#define DKP_CLIENT_ERROR		(dkp_client_error_quark ())
#define DKP_CLIENT_TYPE_ERROR		(dkp_client_error_get_type ())

typedef struct DkpClientPrivate DkpClientPrivate;

typedef struct
{
	 GObject		 parent;
	 DkpClientPrivate	*priv;
} DkpClient;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*added)		(DkpClient		*client,
							 const DkpClientDevice	*device);
	void			(*changed)		(DkpClient		*client,
							 const DkpClientDevice	*device);
	void			(*removed)		(DkpClient		*client,
							 const DkpClientDevice	*device);
	void			(*on_battery_changed)	(DkpClient		*client,
							 gboolean		 on_battery);
	void			(*low_battery_changed)	(DkpClient		*client,
							 gboolean		 low_battery);
} DkpClientClass;

GType		 dkp_client_get_type			(void) G_GNUC_CONST;
DkpClient	*dkp_client_new				(void);
GPtrArray	*dkp_client_enumerate_devices		(DkpClient		*client,
							 GError			**error);
gboolean	 dkp_client_get_on_battery		(DkpClient		*client,
							 gboolean		*on_battery,
							 GError			**error);
gboolean	 dkp_client_get_low_battery		(DkpClient		*client,
							 gboolean		*low_battery,
							 GError			**error);
gboolean	 dkp_client_can_suspend			(DkpClient		*client,
							 gboolean		 interactive,
							 gboolean		*can_suspend,
							 GError			**error);
gboolean	 dkp_client_can_hibernate		(DkpClient		*client,
							 gboolean		 interactive,
							 gboolean		*can_hibernate,
							 GError			**error);

G_END_DECLS

#endif /* __DKP_CLIENT_H */

