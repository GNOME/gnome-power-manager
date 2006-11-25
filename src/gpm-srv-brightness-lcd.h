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

#ifndef __GPM_SRV_BRIGHTNESS_LCD_H
#define __GPM_SRV_BRIGHTNESS_LCD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_SRV_BRIGHTNESS_LCD		(gpm_srv_brightness_lcd_get_type ())
#define GPM_SRV_BRIGHTNESS_LCD(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_SRV_BRIGHTNESS_LCD, GpmSrvBrightnessLcd))
#define GPM_SRV_BRIGHTNESS_LCD_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_SRV_BRIGHTNESS_LCD, GpmSrvBrightnessLcdClass))
#define GPM_IS_SRV_BRIGHTNESS_LCD(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_SRV_BRIGHTNESS_LCD))
#define GPM_IS_SRV_BRIGHTNESS_LCD_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_SRV_BRIGHTNESS_LCD))
#define GPM_SRV_BRIGHTNESS_LCD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_SRV_BRIGHTNESS_LCD, GpmSrvBrightnessLcdClass))

typedef struct GpmSrvBrightnessLcdPrivate GpmSrvBrightnessLcdPrivate;

typedef struct
{
	GObject		         parent;
	GpmSrvBrightnessLcdPrivate *priv;
} GpmSrvBrightnessLcd;

typedef struct
{
	GObjectClass	parent_class;
} GpmSrvBrightnessLcdClass;

typedef enum
{
	 GPM_BRIGHTNESS_LCD_ERROR_GENERAL,
	 GPM_BRIGHTNESS_LCD_ERROR_DATA_NOT_AVAILABLE
} GpmSrvBrightnessLcdError;

GType		 gpm_srv_brightness_lcd_get_type	(void);
GQuark		 gpm_srv_brightness_lcd_error_quark	(void);
GpmSrvBrightnessLcd *gpm_srv_brightness_lcd_new		(void);

gboolean gpm_brightness_lcd_get_policy_brightness	(GpmSrvBrightnessLcd	*srv_brightness,
							 gint			*brightness,
							 GError			**error);
gboolean gpm_brightness_lcd_set_policy_brightness	(GpmSrvBrightnessLcd	*srv_brightness,
							 gint			 brightness,
							 GError			**error);

G_END_DECLS

#endif /* __GPM_SRV_BRIGHTNESS_LCD_H */
