/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMSTATISTICS_H
#define __GPMSTATISTICS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_STATISTICS		(gpm_statistics_get_type ())
#define GPM_STATISTICS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_STATISTICS, GpmStatistics))
#define GPM_STATISTICS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_STATISTICS, GpmStatisticsClass))
#define GPM_IS_STATISTICS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_STATISTICS))
#define GPM_IS_STATISTICS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_STATISTICS))
#define GPM_STATISTICS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_STATISTICS, GpmStatisticsClass))

typedef struct GpmStatisticsPrivate GpmStatisticsPrivate;

typedef struct
{
	GObject		 parent;
	GpmStatisticsPrivate *priv;
} GpmStatistics;

typedef struct
{
	GObjectClass	parent_class;
	void		(* action_help)			(GpmStatistics	*statistics);
	void		(* action_close)		(GpmStatistics	*statistics);
} GpmStatisticsClass;

GType		 gpm_statistics_get_type		(void);
GpmStatistics	*gpm_statistics_new			(void);
void		 gpm_statistics_activate_window		(GpmStatistics	*statistics);

G_END_DECLS

#endif	/* __GPMSTATISTICS_H */
