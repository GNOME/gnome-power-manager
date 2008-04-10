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

#ifndef __GPM_BRIGHTNESS_HAL_H
#define __GPM_BRIGHTNESS_HAL_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_BRIGHTNESS_HAL		(gpm_brightness_hal_get_type ())
#define GPM_BRIGHTNESS_HAL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BRIGHTNESS_HAL, GpmBrightnessHal))
#define GPM_BRIGHTNESS_HAL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BRIGHTNESS_HAL, GpmBrightnessHalClass))
#define GPM_IS_BRIGHTNESS_HAL(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BRIGHTNESS_HAL))
#define GPM_IS_BRIGHTNESS_HAL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BRIGHTNESS_HAL))
#define GPM_BRIGHTNESS_HAL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BRIGHTNESS_HAL, GpmBrightnessHalClass))

typedef struct GpmBrightnessHalPrivate GpmBrightnessHalPrivate;

typedef struct
{
	GObject			 	 parent;
	GpmBrightnessHalPrivate		*priv;
} GpmBrightnessHal;

typedef struct
{
	GObjectClass	parent_class;
	void		(* brightness_changed)	(GpmBrightnessHal	*brightness,
						 guint			 percentage);
} GpmBrightnessHalClass;

GType		 gpm_brightness_hal_get_type	(void);
GpmBrightnessHal *gpm_brightness_hal_new	(void);

gboolean	 gpm_brightness_hal_has_hw	(GpmBrightnessHal	*brightness);
gboolean	 gpm_brightness_hal_up		(GpmBrightnessHal	*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_hal_down	(GpmBrightnessHal	*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_hal_get		(GpmBrightnessHal	*brightness,
						 guint			*percentage);
gboolean	 gpm_brightness_hal_set		(GpmBrightnessHal	*brightness,
						 guint			 percentage,
						 gboolean		*hw_changed);

G_END_DECLS

#endif /* __GPM_BRIGHTNESS_HAL_H */
