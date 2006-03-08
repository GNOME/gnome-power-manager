/**
 * graph.h
 *
 * Test GpmSimpleGraph in a GtkWindow
 *
 * (c) 2006, Richard Hughes <richard@hughsie.com>
 *
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
	int x; //0..100
	int y; //0..100
} GpmSimpleDataPoint;

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
void		 gpm_simple_graph_set_stop_x	(GpmSimpleGraph *graph, gint stop);
void		 gpm_simple_graph_set_stop_y	(GpmSimpleGraph *graph, gint stop);
void		 gpm_simple_graph_set_data	(GpmSimpleGraph *graph, GList *list);
void		 gpm_simple_graph_set_axis_x	(GpmSimpleGraph *graph, GpmSimpleGraphAxisType axis);
void		 gpm_simple_graph_set_axis_y	(GpmSimpleGraph *graph, GpmSimpleGraphAxisType axis);

G_END_DECLS

#endif
