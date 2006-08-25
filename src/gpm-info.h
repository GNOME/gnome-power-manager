/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#ifndef __GPMINFO_H
#define __GPMINFO_H

#include <glib-object.h>
#include "gpm-graph-widget.h"

G_BEGIN_DECLS

#define GPM_TYPE_INFO		(gpm_info_get_type ())
#define GPM_INFO(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_INFO, GpmInfo))
#define GPM_INFO_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_INFO, GpmInfoClass))
#define GPM_IS_INFO(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_INFO))
#define GPM_IS_INFO_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_INFO))
#define GPM_INFO_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_INFO, GpmInfoClass))

typedef struct GpmInfoPrivate GpmInfoPrivate;

typedef struct
{
	GObject	 	parent;
	GpmInfoPrivate *priv;
} GpmInfo;

typedef struct
{
	GObjectClass	parent_class;
} GpmInfoClass;

GType		 gpm_info_get_type			(void);
GpmInfo		*gpm_info_new				(void);

void		 gpm_info_show_window			(GpmInfo	*info);
void		 gpm_info_event_log			(GpmInfo	*info,
							 GpmGraphWidgetEvent	 event,
							 const char	*desc);

gboolean	 gpm_statistics_moo			(GpmInfo	*info,
							 gboolean	*allowed,
							 GError		**error);

G_END_DECLS

#endif	/* __GPMINFO_H */
