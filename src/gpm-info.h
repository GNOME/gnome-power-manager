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

#ifndef __GPMINFO_H
#define __GPMINFO_H

#include <glib-object.h>
#include "gpm-graph-widget.h"
#include "gpm-engine.h"

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

typedef enum
{
	 GPM_INFO_ERROR_GENERAL,
	 GPM_INFO_ERROR_INVALID_TYPE,
	 GPM_INFO_ERROR_DATA_NOT_AVAILABLE
} GpmInfoError;

typedef enum {
	GPM_EVENT_ON_AC,
	GPM_EVENT_ON_BATTERY,
	GPM_EVENT_SESSION_POWERSAVE,
	GPM_EVENT_SESSION_IDLE,
	GPM_EVENT_SESSION_ACTIVE,
	GPM_EVENT_SUSPEND,
	GPM_EVENT_HIBERNATE,
	GPM_EVENT_RESUME,
	GPM_EVENT_LID_CLOSED,
	GPM_EVENT_LID_OPENED,
	GPM_EVENT_NOTIFICATION,
	GPM_EVENT_DPMS_ON,
	GPM_EVENT_DPMS_STANDBY,
	GPM_EVENT_DPMS_SUSPEND,
	GPM_EVENT_DPMS_OFF,
	GPM_EVENT_LAST
} GpmGraphWidgetEvent;

#define GPM_INFO_ERROR gpm_manager_error_quark ()

GType		 gpm_info_get_type			(void);
GpmInfo		*gpm_info_new				(void);
GQuark		 gpm_info_error_quark			(void);

void		 gpm_info_show_window			(GpmInfo	*info);
void		 gpm_info_event_log			(GpmInfo	*info,
							 GpmGraphWidgetEvent event,
							 const gchar	*desc);

gboolean	 gpm_statistics_get_data_types		(GpmInfo	*info,
							 gchar		***types,
							 GError		**error);
gboolean	 gpm_statistics_get_event_log		(GpmInfo	*info,
							 GPtrArray	**array,
							 GError		**error);
gboolean	 gpm_statistics_get_data		(GpmInfo	*info,
							 const gchar	*type,
							 GPtrArray	**array,
							 GError		**error);
gboolean	 gpm_statistics_get_parameters		(GpmInfo	*info,
							 gchar		*type,
							 gchar		**axis_type_x,
							 gchar		**axis_type_y,
							 gchar		**axis_desc_x,
							 gchar		**axis_desc_y,
							 GPtrArray	**key_data,
							 GPtrArray	**key_event,
							 GError		**error);
void		 gpm_info_explain_reason		(GpmInfo	*info,
							 GpmGraphWidgetEvent event,
							 const gchar	*pre,
							 const gchar	*post);
gboolean	 gpm_info_set_collection_data		(GpmInfo	*info,
							 GpmEngineCollection *collection);

G_END_DECLS

#endif	/* __GPMINFO_H */
