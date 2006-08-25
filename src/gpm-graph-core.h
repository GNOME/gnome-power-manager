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

#ifndef __GPMGRAPH_H
#define __GPMGRAPH_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_GRAPH		(gpm_graph_get_type ())
#define GPM_GRAPH(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_GRAPH, GpmGraph))
#define GPM_GRAPH_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_GRAPH, GpmGraphClass))
#define GPM_IS_GRAPH(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_GRAPH))
#define GPM_IS_GRAPH_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_GRAPH))
#define GPM_GRAPH_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_GRAPH, GpmGraphClass))

typedef struct GpmGraphPrivate GpmGraphPrivate;

typedef struct
{
	GObject		 parent;
	GpmGraphPrivate *priv;
} GpmGraph;

typedef struct
{
	GObjectClass	parent_class;
	void		(* action_help)			(GpmGraph	*graph);
	void		(* action_close)		(GpmGraph	*graph);
} GpmGraphClass;

GType		 gpm_graph_get_type			(void);
GpmGraph	*gpm_graph_new				(void);

G_END_DECLS

#endif	/* __GPMGRAPH_H */
