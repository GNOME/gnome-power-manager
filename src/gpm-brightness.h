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

#ifndef __GPM_BRIGHTNESS_H
#define __GPM_BRIGHTNESS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_BRIGHTNESS		(gpm_brightness_get_type ())
#define GPM_BRIGHTNESS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BRIGHTNESS, GpmBrightness))
#define GPM_BRIGHTNESS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BRIGHTNESS, GpmBrightnessClass))
#define GPM_IS_BRIGHTNESS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BRIGHTNESS))
#define GPM_IS_BRIGHTNESS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BRIGHTNESS))
#define GPM_BRIGHTNESS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BRIGHTNESS, GpmBrightnessClass))

#define GPM_BRIGHTNESS_DIM_INTERVAL	5 /* ms */

typedef struct GpmBrightnessPrivate GpmBrightnessPrivate;

typedef struct
{
	GObject		         parent;
	GpmBrightnessPrivate	*priv;
} GpmBrightness;

typedef struct
{
	GObjectClass	parent_class;
	void		(* brightness_changed)	(GpmBrightness		*brightness,
						 guint			 percentage);
} GpmBrightnessClass;

GType		 gpm_brightness_get_type	(void);
GpmBrightness	*gpm_brightness_new		(void);

gboolean	 gpm_brightness_has_hw		(GpmBrightness		*brightness);
gboolean	 gpm_brightness_up		(GpmBrightness		*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_down		(GpmBrightness		*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_get		(GpmBrightness		*brightness,
						 guint			*percentage);
gboolean	 gpm_brightness_set		(GpmBrightness		*brightness,
						 guint			 percentage,
						 gboolean		*hw_changed);

/* not designed to be used outside of gpm-brightness-*.c */
guint		 gpm_brightness_get_step	(guint			 levels);

G_END_DECLS

#endif /* __GPM_BRIGHTNESS_H */
