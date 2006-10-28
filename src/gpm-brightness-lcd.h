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

#ifndef __GPM_BRIGHTNESS_LCD_H
#define __GPM_BRIGHTNESS_LCD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_BRIGHTNESS_LCD		(gpm_brightness_lcd_get_type ())
#define GPM_BRIGHTNESS_LCD(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BRIGHTNESS_LCD, GpmBrightnessLcd))
#define GPM_BRIGHTNESS_LCD_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BRIGHTNESS_LCD, GpmBrightnessLcdClass))
#define GPM_IS_BRIGHTNESS_LCD(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BRIGHTNESS_LCD))
#define GPM_IS_BRIGHTNESS_LCD_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BRIGHTNESS_LCD))
#define GPM_BRIGHTNESS_LCD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BRIGHTNESS_LCD, GpmBrightnessLcdClass))

typedef struct GpmBrightnessLcdPrivate GpmBrightnessLcdPrivate;

typedef struct
{
	GObject		         parent;
	GpmBrightnessLcdPrivate *priv;
} GpmBrightnessLcd;

typedef struct
{
	GObjectClass	parent_class;
} GpmBrightnessLcdClass;

GType		 gpm_brightness_lcd_get_type		(void);
GpmBrightnessLcd *gpm_brightness_lcd_new		(void);
gboolean	 gpm_brightness_lcd_service_init	(GpmBrightnessLcd *brightness);

gboolean	 gpm_brightness_lcd_up			(GpmBrightnessLcd	*brightness);
gboolean	 gpm_brightness_lcd_down		(GpmBrightnessLcd	*brightness);
gboolean	 gpm_brightness_lcd_get			(GpmBrightnessLcd	*brightness,
							 guint			*brightness_level);
gboolean	 gpm_brightness_lcd_set_dim		(GpmBrightnessLcd	*brightness,
							 guint			 brightness_level);
gboolean	 gpm_brightness_lcd_set_std		(GpmBrightnessLcd	*brightness,
							 guint			 brightness_level);
gboolean	 gpm_brightness_lcd_dim			(GpmBrightnessLcd	*brightness);
gboolean	 gpm_brightness_lcd_undim		(GpmBrightnessLcd	*brightness);

G_END_DECLS

#endif /* __GPM_BRIGHTNESS_LCD_H */
