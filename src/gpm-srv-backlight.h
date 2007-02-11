/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2004-2005 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GPM_SRV_BACKLIGHT_H
#define __GPM_SRV_BACKLIGHT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_SRV_BACKLIGHT		(gpm_srv_backlight_get_type ())
#define GPM_SRV_BACKLIGHT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_SRV_BACKLIGHT, GpmSrvBacklight))
#define GPM_SRV_BACKLIGHT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_SRV_BACKLIGHT, GpmSrvBacklightClass))
#define GPM_IS_SRV_BACKLIGHT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_SRV_BACKLIGHT))
#define GPM_IS_SRV_BACKLIGHT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_SRV_BACKLIGHT))
#define GPM_SRV_BACKLIGHT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_SRV_BACKLIGHT, GpmSrvBacklightClass))

typedef struct GpmSrvBacklightPrivate GpmSrvBacklightPrivate;

typedef struct
{
	GObject		         parent;
	GpmSrvBacklightPrivate *priv;
} GpmSrvBacklight;

typedef struct
{
	GObjectClass	parent_class;
	void		(* brightness_changed)		(GpmSrvBacklight	*srv_brightness,
							 gint			 brightness);
	void		(* mode_changed)		(GpmSrvBacklight	*srv_backlight,
							 const gchar		*mode);
} GpmSrvBacklightClass;

typedef enum
{
	 GPM_BACKLIGHT_ERROR_GENERAL,
	 GPM_BACKLIGHT_ERROR_DATA_NOT_AVAILABLE,
	 GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT
} GpmSrvBacklightError;

GType		 gpm_srv_backlight_get_type		(void);
GQuark		 gpm_srv_backlight_error_quark		(void);
GpmSrvBacklight *gpm_srv_backlight_new			(void);

gboolean	gpm_backlight_get_brightness		(GpmSrvBacklight	*srv_brightness,
							 guint			*brightness,
							 GError			**error);
gboolean	gpm_backlight_set_brightness		(GpmSrvBacklight	*srv_brightness,
							 guint			 brightness,
							 GError			**error);
gboolean	 gpm_backlight_get_mode			(GpmSrvBacklight	*srv_backlight,
							 const gchar		**mode,
							 GError			**error);
gboolean	 gpm_backlight_set_mode			(GpmSrvBacklight	*srv_backlight,
							 const gchar		*mode,
							 GError			**error);

G_END_DECLS

#endif /* __GPM_SRV_BACKLIGHT_H */
