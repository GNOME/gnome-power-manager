/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_WEBCAM_H
#define __GPM_WEBCAM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_WEBCAM		(gpm_webcam_get_type ())
#define GPM_WEBCAM(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_WEBCAM, GpmWebcam))
#define GPM_WEBCAM_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_WEBCAM, GpmWebcamClass))
#define GPM_IS_WEBCAM(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_WEBCAM))
#define GPM_IS_WEBCAM_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_WEBCAM))
#define GPM_WEBCAM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_WEBCAM, GpmWebcamClass))

typedef struct GpmWebcamPrivate GpmWebcamPrivate;

typedef struct
{
	GObject		  parent;
	GpmWebcamPrivate *priv;
} GpmWebcam;

typedef struct
{
	GObjectClass	parent_class;
} GpmWebcamClass;

GType		 gpm_webcam_get_type		(void);
GpmWebcam	*gpm_webcam_new			(void);

gboolean	 gpm_webcam_get_brightness	(GpmWebcam	*webcam,
						 gfloat		*brightness);

G_END_DECLS

#endif /* __GPM_WEBCAM_H */