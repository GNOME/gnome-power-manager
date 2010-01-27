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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GPM_BRIGHTNESS_XRANDR_H
#define __GPM_BRIGHTNESS_XRANDR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_BRIGHTNESS_XRANDR		(gpm_brightness_xrandr_get_type ())
#define GPM_BRIGHTNESS_XRANDR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BRIGHTNESS_XRANDR, GpmBrightnessXRandR))
#define GPM_BRIGHTNESS_XRANDR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BRIGHTNESS_XRANDR, GpmBrightnessXRandRClass))
#define GPM_IS_BRIGHTNESS_XRANDR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BRIGHTNESS_XRANDR))
#define GPM_IS_BRIGHTNESS_XRANDR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BRIGHTNESS_XRANDR))
#define GPM_BRIGHTNESS_XRANDR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BRIGHTNESS_XRANDR, GpmBrightnessXRandRClass))

typedef struct GpmBrightnessXRandRPrivate GpmBrightnessXRandRPrivate;

typedef struct
{
	GObject				 parent;
	GpmBrightnessXRandRPrivate	*priv;
} GpmBrightnessXRandR;

typedef struct
{
	GObjectClass	parent_class;
	void		(* brightness_changed)	(GpmBrightnessXRandR	*brightness,
						 guint			 percentage);
} GpmBrightnessXRandRClass;

GType		 gpm_brightness_xrandr_get_type	(void);
GpmBrightnessXRandR *gpm_brightness_xrandr_new	(void);

gboolean	 gpm_brightness_xrandr_has_hw	(GpmBrightnessXRandR	*brightness);
gboolean	 gpm_brightness_xrandr_up	(GpmBrightnessXRandR	*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_xrandr_down	(GpmBrightnessXRandR	*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_xrandr_get	(GpmBrightnessXRandR	*brightness,
						 guint			*percentage);
gboolean	 gpm_brightness_xrandr_set	(GpmBrightnessXRandR	*brightness,
						 guint			 percentage,
						 gboolean		*hw_changed);

G_END_DECLS

#endif /* __GPM_BRIGHTNESS_XRANDR_H */
