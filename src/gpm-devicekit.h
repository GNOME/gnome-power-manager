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

#ifndef __GPM_DEVICEKIT_H
#define __GPM_DEVICEKIT_H

#include <glib-object.h>
#include <devkit-power-gobject/devicekit-power.h>

G_BEGIN_DECLS

const gchar	*gpm_device_type_to_localised_text	(DkpDeviceType	 type,
							 guint		 number);
const gchar	*gpm_device_type_to_icon		(DkpDeviceType	 type);
const gchar	*gpm_device_technology_to_localised_text (DkpDeviceTechnology technology_enum);
gchar		*gpm_devicekit_get_object_icon		(DkpDevice *device);
gchar		*gpm_devicekit_get_object_summary	(DkpDevice *device);
gchar		*gpm_devicekit_get_object_description	(DkpDevice *device);

G_END_DECLS

#endif	/* __GPM_DEVICEKIT_H */
