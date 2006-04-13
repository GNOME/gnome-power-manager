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

#ifndef __GPM_GRAPH_H__
#define __GPM_GRAPH_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GPM_TYPE_GRAPH		(gpm_graph_get_type ())
#define GPM_GRAPH(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GPM_TYPE_GRAPH, GpmGraph))
#define GPM_GRAPH_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GPM_GRAPH, GpmGraphClass))
#define GPM_IS_GRAPH(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GPM_TYPE_GRAPH))
#define GPM_IS_GRAPH_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_GRAPH))
#define GPM_GRAPH_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GPM_TYPE_GRAPH, GpmGraphClass))

typedef struct GpmGraph		GpmGraph;
typedef struct GpmGraphClass	GpmGraphClass;
typedef struct GpmGraphPrivate	GpmGraphPrivate;

typedef enum {
	GPM_GRAPH_COLOUR_DEFAULT,
	GPM_GRAPH_COLOUR_WHITE,
	GPM_GRAPH_COLOUR_BLACK,
	GPM_GRAPH_COLOUR_RED,
	GPM_GRAPH_COLOUR_BLUE,
	GPM_GRAPH_COLOUR_GREEN,
	GPM_GRAPH_COLOUR_MAGENTA,
	GPM_GRAPH_COLOUR_YELLOW,
	GPM_GRAPH_COLOUR_CYAN,
	GPM_GRAPH_COLOUR_DARK_BLUE,
	GPM_GRAPH_COLOUR_DARK_RED,
	GPM_GRAPH_COLOUR_DARK_MAGENTA,
	GPM_GRAPH_COLOUR_DARK_YELLOW,
	GPM_GRAPH_COLOUR_DARK_GREEN,
	GPM_GRAPH_COLOUR_DARK_CYAN,
	GPM_GRAPH_COLOUR_LAST
} GpmGraphColour;

typedef enum {
	GPM_GRAPH_EVENT_ON_AC,
	GPM_GRAPH_EVENT_ON_BATTERY,
	GPM_GRAPH_EVENT_SCREEN_DIM,
	GPM_GRAPH_EVENT_DPMS_OFF,
	GPM_GRAPH_EVENT_DPMS_ON,
	GPM_GRAPH_EVENT_SUSPEND,
	GPM_GRAPH_EVENT_HIBERNATE,
	GPM_GRAPH_EVENT_RESUME,
	GPM_GRAPH_EVENT_LID_CLOSED,
	GPM_GRAPH_EVENT_LID_OPENED,
	GPM_GRAPH_EVENT_LAST
} GpmGraphEvent;

typedef enum {
	GPM_GRAPH_TYPE_PERCENTAGE,
	GPM_GRAPH_TYPE_TIME,
	GPM_GRAPH_TYPE_RATE,
	GPM_GRAPH_TYPE_LAST
} GpmGraphAxisType;

struct GpmGraph
{
	GtkDrawingArea	 parent;
	GpmGraphPrivate	*priv;
};

struct GpmGraphClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 gpm_graph_get_type		(void);
GtkWidget	*gpm_graph_new			(void);

void		 gpm_graph_set_invert_x		(GpmGraph	*graph,
						 gboolean	 inv);
void		 gpm_graph_enable_legend	(GpmGraph	*graph,
						 gboolean	 enable);
void		 gpm_graph_set_invert_y		(GpmGraph	*graph,
						 gboolean	 inv);
void		 gpm_graph_set_data		(GpmGraph	*graph,
						 GList		*list);
void		 gpm_graph_set_events		(GpmGraph	*graph,
						 GList		*events);
void		 gpm_graph_set_axis_x		(GpmGraph	*graph,
						 GpmGraphAxisType axis);
void		 gpm_graph_set_axis_y		(GpmGraph	*graph,
						 GpmGraphAxisType axis);
const char *	 gpm_graph_event_description	(GpmGraphEvent event);
GpmGraphColour	 gpm_graph_event_colour		(GpmGraphEvent event);

G_END_DECLS

#endif
