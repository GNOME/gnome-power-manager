/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2006 David Zeuthen <davidz@redhat.com>
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

#ifndef __GPM_HAL_BRIGHTNESS_KBD_H
#define __GPM_HAL_BRIGHTNESS_KBD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_HAL_BRIGHTNESS_KBD		(gpm_hal_brightness_kbd_get_type ())
#define GPM_HAL_BRIGHTNESS_KBD(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_HAL_BRIGHTNESS_KBD, GpmHalBrightnessKbd))
#define GPM_HAL_BRIGHTNESS_KBD_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_HAL_BRIGHTNESS_KBD, GpmHalBrightnessKbdClass))
#define GPM_IS_HAL_BRIGHTNESS_KBD(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_HAL_BRIGHTNESS_KBD))
#define GPM_IS_HAL_BRIGHTNESS_KBD_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_HAL_BRIGHTNESS_KBD))
#define GPM_HAL_BRIGHTNESS_KBD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_HAL_BRIGHTNESS_KBD, GpmHalBrightnessKbdClass))

typedef struct GpmHalBrightnessKbdPrivate GpmHalBrightnessKbdPrivate;

typedef struct
{
	GObject		      parent;
	GpmHalBrightnessKbdPrivate *priv;
} GpmHalBrightnessKbd;

typedef struct
{
	GObjectClass	parent_class;
} GpmHalBrightnessKbdClass;

GType		 gpm_hal_brightness_kbd_get_type	(void);
GpmHalBrightnessKbd *gpm_hal_brightness_kbd_new	(void);

gboolean	 gpm_hal_brightness_kbd_up		(GpmHalBrightnessKbd	*brightness);
gboolean	 gpm_hal_brightness_kbd_down		(GpmHalBrightnessKbd	*brightness);
gboolean	 gpm_hal_brightness_kbd_get		(GpmHalBrightnessKbd	*brightness,
							 guint			*brightness_level);
gboolean	 gpm_hal_brightness_kbd_set_dim		(GpmHalBrightnessKbd	*brightness,
							 guint			 brightness_level);
gboolean	 gpm_hal_brightness_kbd_set_std		(GpmHalBrightnessKbd	*brightness,
							 guint			 brightness_level);
gboolean	 gpm_hal_brightness_kbd_dim		(GpmHalBrightnessKbd	*brightness);
gboolean	 gpm_hal_brightness_kbd_undim		(GpmHalBrightnessKbd	*brightness);
gboolean	 gpm_hal_brightness_kbd_toggle		(GpmHalBrightnessKbd	*brightness);

G_END_DECLS

#endif /* __GPM_HAL_BRIGHTNESS_KBD_H */
