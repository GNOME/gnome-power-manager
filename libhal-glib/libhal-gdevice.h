/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __LIBHAL_GDEVICE_H
#define __LIBHAL_GDEVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LIBHAL_TYPE_GDEVICE		(hal_gdevice_get_type ())
#define LIBHAL_GDEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LIBHAL_TYPE_GDEVICE, HalGDevice))
#define LIBHAL_GDEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LIBHAL_TYPE_GDEVICE, HalGDeviceClass))
#define LIBHAL_IS_GDEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LIBHAL_TYPE_GDEVICE))
#define LIBHAL_IS_GDEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LIBHAL_TYPE_GDEVICE))
#define LIBHAL_GDEVICE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LIBHAL_TYPE_GDEVICE, HalGDeviceClass))

typedef struct HalGDevicePrivate HalGDevicePrivate;

typedef struct
{
	GObject		     parent;
	HalGDevicePrivate *priv;
} HalGDevice;

/* Signals emitted from HalGDevice are:
 *
 * device-property-modified
 * device-condition
 */

typedef struct
{
	GObjectClass	parent_class;
	void		(* device_property_modified)	(HalGDevice	*device,
							 const gchar	*key,
							 gboolean	 is_added,
							 gboolean	 is_removed,
							 gboolean	 finally);
	void		(* device_condition)		(HalGDevice	*device,
							 const gchar	*condition,
							 const gchar	*details);
} HalGDeviceClass;

GType		 hal_gdevice_get_type			(void);
HalGDevice	*hal_gdevice_new			(void);

gboolean	 hal_gdevice_set_udi			(HalGDevice	*device,
							 const gchar	*udi);
const gchar	*hal_gdevice_get_udi			(HalGDevice	*device);
gboolean	 hal_gdevice_get_bool			(HalGDevice	*device,
							 const gchar	*key,
							 gboolean	*value,
							 GError		**error);
gboolean	 hal_gdevice_get_string			(HalGDevice	*device,
							 const gchar	*key,
							 gchar		**value,
							 GError		**error);
gboolean	 hal_gdevice_get_int			(HalGDevice	*device,
							 const gchar	*key,
							 gint		*value,
							 GError		**error);
gboolean	 hal_gdevice_get_uint			(HalGDevice	*device,
							 const gchar	*key,
							 guint		*value,
							 GError		**error);
gboolean	 hal_gdevice_query_capability		(HalGDevice	*device,
							 const gchar	*capability,
							 gboolean	*has_capability,
							 GError		**error);
gboolean	 hal_gdevice_watch_condition		(HalGDevice	*device);
gboolean	 hal_gdevice_watch_property_modified	(HalGDevice	*device);
gboolean	 hal_gdevice_remove_condition		(HalGDevice	*device);
gboolean	 hal_gdevice_remove_property_modified	(HalGDevice	*device);

G_END_DECLS

#endif	/* __LIBHAL_GDEVICE_H */
