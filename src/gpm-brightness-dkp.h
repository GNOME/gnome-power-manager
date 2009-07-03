/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_BRIGHTNESS_DKP_H
#define __GPM_BRIGHTNESS_DKP_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_BRIGHTNESS_DKP		(gpm_brightness_dkp_get_type ())
#define GPM_BRIGHTNESS_DKP(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BRIGHTNESS_DKP, GpmBrightnessDkp))
#define GPM_BRIGHTNESS_DKP_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BRIGHTNESS_DKP, GpmBrightnessDkpClass))
#define GPM_IS_BRIGHTNESS_DKP(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BRIGHTNESS_DKP))
#define GPM_IS_BRIGHTNESS_DKP_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BRIGHTNESS_DKP))
#define GPM_BRIGHTNESS_DKP_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BRIGHTNESS_DKP, GpmBrightnessDkpClass))

typedef struct GpmBrightnessDkpPrivate GpmBrightnessDkpPrivate;

typedef struct
{
	GObject			 	 parent;
	GpmBrightnessDkpPrivate		*priv;
} GpmBrightnessDkp;

typedef struct
{
	GObjectClass	parent_class;
	void		(* brightness_changed)	(GpmBrightnessDkp	*brightness,
						 guint			 percentage);
} GpmBrightnessDkpClass;

GType		 gpm_brightness_dkp_get_type	(void);
GpmBrightnessDkp *gpm_brightness_dkp_new	(void);

gboolean	 gpm_brightness_dkp_has_hw	(GpmBrightnessDkp	*brightness);
gboolean	 gpm_brightness_dkp_up		(GpmBrightnessDkp	*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_dkp_down	(GpmBrightnessDkp	*brightness,
						 gboolean		*hw_changed);
gboolean	 gpm_brightness_dkp_get		(GpmBrightnessDkp	*brightness,
						 guint			*percentage);
gboolean	 gpm_brightness_dkp_set		(GpmBrightnessDkp	*brightness,
						 guint			 percentage,
						 gboolean		*hw_changed);

G_END_DECLS

#endif /* __GPM_BRIGHTNESS_DKP_H */
