/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GPM_LIGHT_SENSOR_H
#define __GPM_LIGHT_SENSOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_LIGHT_SENSOR		(gpm_light_sensor_get_type ())
#define GPM_LIGHT_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_LIGHT_SENSOR, GpmLightSensor))
#define GPM_LIGHT_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_LIGHT_SENSOR, GpmLightSensorClass))
#define GPM_IS_LIGHT_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_LIGHT_SENSOR))
#define GPM_IS_LIGHT_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_LIGHT_SENSOR))
#define GPM_LIGHT_SENSOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_LIGHT_SENSOR, GpmLightSensorClass))

typedef struct GpmLightSensorPrivate GpmLightSensorPrivate;

typedef struct
{
	GObject		      parent;
	GpmLightSensorPrivate *priv;
} GpmLightSensor;

typedef struct
{
	GObjectClass	parent_class;
	void		(* sensor_changed)		(GpmLightSensor	*brightness_sensor,
							 guint	         brightness_level);
} GpmLightSensorClass;

GType		 gpm_light_sensor_get_type		(void);
GpmLightSensor *gpm_light_sensor_new			(void);

gboolean	 gpm_light_sensor_get_absolute		(GpmLightSensor	*brightness,
							 guint		*brightness_level);
gboolean	 gpm_light_sensor_get_relative		(GpmLightSensor	*brightness,
							 gfloat		*difference);
gboolean	 gpm_light_sensor_calibrate		(GpmLightSensor	*brightness);
gboolean	 gpm_light_sensor_has_hw		(GpmLightSensor *sensor);

G_END_DECLS

#endif /* __GPM_LIGHT_SENSOR_H */
