/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMFEEDBACK_H
#define __GPMFEEDBACK_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_FEEDBACK		(gpm_feedback_get_type ())
#define GPM_FEEDBACK(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_FEEDBACK, GpmFeedback))
#define GPM_FEEDBACK_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_FEEDBACK, GpmFeedbackClass))
#define GPM_IS_FEEDBACK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_FEEDBACK))
#define GPM_IS_FEEDBACK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_FEEDBACK))
#define GPM_FEEDBACK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_FEEDBACK, GpmFeedbackClass))

typedef struct GpmFeedbackPrivate GpmFeedbackPrivate;

typedef struct
{
	GObject		    parent;
	GpmFeedbackPrivate *priv;
} GpmFeedback;

typedef struct
{
	GObjectClass	parent_class;
} GpmFeedbackClass;

GType		 gpm_feedback_get_type			(void);
GpmFeedback	*gpm_feedback_new			(void);

gboolean	 gpm_feedback_display_value		(GpmFeedback	*feedback,
							 gfloat		 value);
gboolean	 gpm_feedback_set_icon_name		(GpmFeedback	*feedback,
							 const gchar	*icon_name);

G_END_DECLS

#endif	/* __GPMFEEDBACK_H */
