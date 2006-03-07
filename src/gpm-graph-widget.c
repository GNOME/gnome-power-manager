/**
 * graph.c
 *
 * Test GpmSimpleGraph in a GtkWindow
 *
 * (c) 2006, Richard Hughes <richard@hughsie.com>
 *
 */

#include "config.h"
#include <gtk/gtk.h>
#include <math.h>

#include "gpm-graph-widget.h"
#include "gpm-debug.h"

G_DEFINE_TYPE (GpmSimpleGraph, gpm_simple_graph, GTK_TYPE_DRAWING_AREA);
#define GPM_SIMPLE_GRAPH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SIMPLE_GRAPH, GpmSimpleGraphPrivate))

struct GpmSimpleGraphPrivate
{
	gboolean	use_grid;

	gboolean	invert_x;
	gboolean	invert_y;

	gint		stop_x;
	gint		stop_y;

	gint		box_x; /* size of the white box, not the widget */
	gint		box_y;
	gint		box_width;
	gint		box_height;
	
	GList		*list;
};

static gboolean gpm_simple_graph_expose (GtkWidget *graph, GdkEventExpose *event);

static void
gpm_simple_graph_class_init (GpmSimpleGraphClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);

	widget_class->expose_event = gpm_simple_graph_expose;
	
	g_type_class_add_private (class, sizeof (GpmSimpleGraphPrivate));
}

static void
gpm_simple_graph_init (GpmSimpleGraph *graph)
{
	graph->priv = GPM_SIMPLE_GRAPH_GET_PRIVATE (graph);
	graph->priv->invert_x = FALSE;
	graph->priv->invert_y = FALSE;
	graph->priv->stop_x = 60;
	graph->priv->stop_y = 100;
	graph->priv->use_grid = TRUE;
	graph->priv->list = NULL;
}

/** Sets the inverse policy for the X axis, i.e. to count from 0..Y or Y..0 */
void
gpm_simple_graph_set_invert_x (GpmSimpleGraph *graph, gboolean inv)
{
	graph->priv->invert_x = inv;
}

/** Sets the inverse policy for the Y axis, i.e. to count from 0..Y or Y..0 */
void
gpm_simple_graph_set_invert_y (GpmSimpleGraph *graph, gboolean inv)
{
	graph->priv->invert_y = inv;
}

/** Sets the stop point for the X axis, i.e. the maximum number */
void
gpm_simple_graph_set_stop_x (GpmSimpleGraph *graph, gint stop)
{
	graph->priv->stop_x = stop;
}

/** Sets the stop point for the Y axis, i.e. the maximum number */
void
gpm_simple_graph_set_stop_y (GpmSimpleGraph *graph, gint stop)
{
	graph->priv->stop_y = stop;
}

/** Sets the data for the graph. This data has to be normalised on both
    axes to 0..100 and 0..100. You MUST NOT free the list before the widget. */
void
gpm_simple_graph_set_data (GpmSimpleGraph *graph, GList *list)
{
	graph->priv->list = list;
}

#ifdef HAVE_CAIRO
static char *
get_hour (int totalminutes)
{
	int hours = totalminutes / 60;
	int minutes =  totalminutes - (hours * 60);
	char *text = g_strdup_printf ("%iH%02i", hours, minutes);
	return text;
}

static void
draw_grid (GpmSimpleGraph *graph, cairo_t *cr)
{
	int a, b;
	double dotted[] = {1., 2.};
	double divwidth  = graph->priv->box_width / 10;
	double divheight = graph->priv->box_height / 10;

	cairo_save (cr); /* push stack */

	cairo_set_line_width (cr, 1);
	cairo_set_dash (cr, dotted, 2, 0.0);

	/* do vertical lines */
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	for (a=1; a<10; a++) {
		b = graph->priv->box_x + (a * divwidth);
		cairo_move_to (cr, b + 0.5, graph->priv->box_y);
		cairo_line_to (cr, b + 0.5, graph->priv->box_y + graph->priv->box_height);		
		cairo_stroke (cr);
	}

	/* do horizontal lines */
	for (a=1; a<10; a++) {
		b = graph->priv->box_y + (a * divheight);
		cairo_move_to (cr, graph->priv->box_x, b + 0.5);
		cairo_line_to (cr, graph->priv->box_x + graph->priv->box_width, b + 0.5);		
		cairo_stroke (cr);
	}

	cairo_restore (cr); /* pop stack */	
}

static void
draw_labels (GpmSimpleGraph *graph, cairo_t *cr)
{
	int a, b;
	gchar *text;
	gint value;
	double divwidth  = graph->priv->box_width / 10;
	double divheight = graph->priv->box_height / 10;
	gint length_x = graph->priv->stop_x - 0;

	cairo_save (cr); /* push stack */

	/* setup font */
	cairo_font_options_t *options;
	options = cairo_font_options_create ();
	cairo_set_font_options (cr, options);

	/* do time text */
	cairo_set_source_rgb (cr, 0, 0, 0);	
	for (a=0; a<11; a++) {
		b = graph->priv->box_x + (a * divwidth);
		cairo_move_to (cr, b - 18, graph->priv->box_y + graph->priv->box_height + 15);
		if (graph->priv->invert_x) {
			value = (length_x / 10) * (10 - a);
		} else {
			value = (length_x / 10) * a;
		}
		text = get_hour (value);
		cairo_show_text (cr, text);
		g_free (text);
	}
	
	/* do percentage text */
	for (a=0; a<11; a++) {
		b = graph->priv->box_y + (a * divheight);
		cairo_move_to (cr, graph->priv->box_x - 35, b + 5);
		if (graph->priv->invert_y) {
			value = a * 10;
		} else {
			value = (10 - a) * 10;
		}
		text = g_strdup_printf ("%i%%", value);
		cairo_show_text (cr, text);
		g_free (text);
	}

	cairo_font_options_destroy (options);

	cairo_restore (cr); /* pop stack */	
}

static void
draw_line (GpmSimpleGraph *graph, cairo_t *cr)
{
	int a;
	double unitx = graph->priv->box_width / 100.f;
	double unity = graph->priv->box_height / 100.f;

	if (! graph->priv->list) {
		gpm_debug ("no data");
		return;
	}

	cairo_save (cr); /* push stack */

	cairo_set_line_width (cr, 2);

	GpmSimpleDataPoint *new = NULL;
	GpmSimpleDataPoint *old = (GpmSimpleDataPoint *) g_list_nth_data (graph->priv->list, 0);
	
	for (a=1; a<=g_list_length (graph->priv->list)-1; a++) {
		new = (GpmSimpleDataPoint *) g_list_nth_data (graph->priv->list, a);
		if (new->x < 0 || new->x > 100) {
			new->x = 50;
		}
		if (new->y < 0 || new->y > 100) {
			new->y = 50;
		}
		cairo_move_to (cr,
			       graph->priv->box_x + (unitx * old->x),
			       graph->priv->box_y + (unity * (100 - old->y)));
		cairo_line_to (cr,
			       graph->priv->box_x + (unitx * new->x),
			       graph->priv->box_y + (unity * (100 - new->y)));		
		if (new->y < old->y) {
			cairo_set_source_rgb (cr, 1, 0, 0);
		} else {
			cairo_set_source_rgb (cr, 0, 1, 0);
		}
		cairo_stroke (cr);
		old = new;
	}

	cairo_restore (cr); /* pop stack */	
}


static void
draw_graph (GtkWidget *graph_widget, cairo_t *cr)
{

	GpmSimpleGraph *graph = (GpmSimpleGraph*) graph_widget;

	cairo_save (cr); /* push stack */

	graph->priv->box_x = graph_widget->allocation.x + 40;
	graph->priv->box_y = graph_widget->allocation.y + 5;
	graph->priv->box_width = graph_widget->allocation.width - (5 + 40);
	graph->priv->box_height = graph_widget->allocation.height - (5 + 20);

	/* background */
	cairo_rectangle (cr, graph->priv->box_x, graph->priv->box_y,
			 graph->priv->box_width, graph->priv->box_height);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_fill (cr);
	
	if (graph->priv->use_grid) {
		draw_grid (graph, cr);
	}
	draw_labels (graph, cr);
	draw_line (graph, cr);

	/* solid outline box */
	cairo_rectangle (cr, graph->priv->box_x + 0.5, graph->priv->box_y + 0.5,
			 graph->priv->box_width - 1 , graph->priv->box_height - 1);
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);

	cairo_restore (cr); /* pop stack */
}
#endif

static gboolean
gpm_simple_graph_expose (GtkWidget *graph, GdkEventExpose *event)
{
#ifdef HAVE_CAIRO
	cairo_t *cr;

	/* get a cairo_t */
	cr = gdk_cairo_create (graph->window);

	cairo_rectangle (cr,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);
	cairo_clip (cr);

	/* not sure why, maybe bug in gtk? */
	graph->allocation.x = event->area.x;
	graph->allocation.y = event->area.y;

	draw_graph (graph, cr);

	cairo_destroy (cr);
#endif
	return FALSE;
}

GtkWidget *
gpm_simple_graph_new (void)
{
	return g_object_new (GPM_TYPE_SIMPLE_GRAPH, NULL);
}
