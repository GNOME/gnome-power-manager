/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_HAL_BRIGHTNESS_SENSOR_H
#define __GPM_HAL_BRIGHTNESS_SENSOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_HAL_BRIGHTNESS_SENSOR		(gpm_hal_brightness_sensor_get_type ())
#define GPM_HAL_BRIGHTNESS_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_HAL_BRIGHTNESS_SENSOR, GpmHalBrightnessSensor))
#define GPM_HAL_BRIGHTNESS_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_HAL_BRIGHTNESS_SENSOR, GpmHalBrightnessSensorClass))
#define GPM_IS_HAL_BRIGHTNESS_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_HAL_BRIGHTNESS_SENSOR))
#define GPM_IS_HAL_BRIGHTNESS_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_HAL_BRIGHTNESS_SENSOR))
#define GPM_HAL_BRIGHTNESS_SENSOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_HAL_BRIGHTNESS_SENSOR, GpmHalBrightnessSensorClass))

typedef struct GpmHalBrightnessSensorPrivate GpmHalBrightnessSensorPrivate;

typedef struct
{
	GObject		      parent;
	GpmHalBrightnessSensorPrivate *priv;
} GpmHalBrightnessSensor;

typedef struct
{
	GObjectClass	parent_class;
} GpmHalBrightnessSensorClass;

GType		 gpm_hal_brightness_sensor_get_type	(void);
GpmHalBrightnessSensor *gpm_hal_brightness_sensor_new		(void);

gboolean	 gpm_hal_brightness_sensor_up		(GpmHalBrightnessSensor	*brightness);
gboolean	 gpm_hal_brightness_sensor_down		(GpmHalBrightnessSensor	*brightness);
gboolean	 gpm_hal_brightness_sensor_get		(GpmHalBrightnessSensor	*brightness,
							 guint			*brightness_level);
gboolean	 gpm_hal_brightness_sensor_set_dim		(GpmHalBrightnessSensor	*brightness,
							 guint			 brightness_level);
gboolean	 gpm_hal_brightness_sensor_set_std		(GpmHalBrightnessSensor	*brightness,
							 guint			 brightness_level);
gboolean	 gpm_hal_brightness_sensor_dim		(GpmHalBrightnessSensor	*brightness);
gboolean	 gpm_hal_brightness_sensor_undim		(GpmHalBrightnessSensor	*brightness);

G_END_DECLS

#endif /* __GPM_HAL_BRIGHTNESS_SENSOR_H */
