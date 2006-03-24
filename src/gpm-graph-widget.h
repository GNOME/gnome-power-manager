/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_SIMPLE_GRAPH_H__
#define __GPM_SIMPLE_GRAPH_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GPM_TYPE_SIMPLE_GRAPH		(gpm_simple_graph_get_type ())
#define GPM_SIMPLE_GRAPH(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GPM_TYPE_SIMPLE_GRAPH, GpmSimpleGraph))
#define GPM_SIMPLE_GRAPH_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GPM_SIMPLE_GRAPH, GpmSimpleGraphClass))
#define GPM_IS_SIMPLE_GRAPH(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GPM_TYPE_SIMPLE_GRAPH))
#define GPM_IS_SIMPLE_GRAPH_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_SIMPLE_GRAPH))
#define GPM_SIMPLE_GRAPH_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GPM_TYPE_SIMPLE_GRAPH, GpmSimpleGraphClass))

typedef struct GpmSimpleGraph		GpmSimpleGraph;
typedef struct GpmSimpleGraphClass	GpmSimpleGraphClass;
typedef struct GpmSimpleGraphPrivate	GpmSimpleGraphPrivate;

typedef struct {
	int x;
	int y;
} GpmDataPoint;

typedef enum {
	GPM_GRAPH_TYPE_PERCENTAGE,
	GPM_GRAPH_TYPE_TIME,
	GPM_GRAPH_TYPE_RATE,
	GPM_GRAPH_TYPE_LAST
} GpmSimpleGraphAxisType;

struct GpmSimpleGraph
{
	GtkDrawingArea		 parent;
	GpmSimpleGraphPrivate	*priv;
};

struct GpmSimpleGraphClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 gpm_simple_graph_get_type	(void);
GtkWidget	*gpm_simple_graph_new		(void);

void		 gpm_simple_graph_set_invert_x	(GpmSimpleGraph *graph, gboolean inv);
void		 gpm_simple_graph_set_invert_y	(GpmSimpleGraph *graph, gboolean inv);
void		 gpm_simple_graph_set_data	(GpmSimpleGraph *graph, GList *list);
void		 gpm_simple_graph_set_axis_x	(GpmSimpleGraph *graph, GpmSimpleGraphAxisType axis);
void		 gpm_simple_graph_set_axis_y	(GpmSimpleGraph *graph, GpmSimpleGraphAxisType axis);

G_END_DECLS

#endif
