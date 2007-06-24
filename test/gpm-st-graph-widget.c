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

#include <glib.h>
#include <math.h>
#include "gpm-st-main.h"

#include "../src/gpm-graph-widget.h"
#include "../src/gpm-debug.h"
#include "../src/gpm-array.h"
#include "../src/gpm-common.h"

#include <gtk/gtk.h>
#include <glib.h>

GtkWidget *window;
GtkWidget *graph;
GtkWidget *label;

static gint
close_handler (GtkWidget *widget, gpointer gdata)
{
	gtk_main_quit ();
	return FALSE;
}

static void
clicked_passed_cb (GtkWidget *widget, gpointer gdata)
{
	GpmSelfTest *test = (GpmSelfTest *) gdata;
//	g_print("Passed was clicked.\n");
	gpm_st_success (test, NULL);
	gtk_main_quit ();
}

static void
clicked_failed_cb (GtkWidget *widget, gpointer gdata)
{
	GpmSelfTest *test = (GpmSelfTest *) gdata;
//	g_print("Failed was clicked.\n");
	gpm_st_failed (test, NULL);
	gtk_main_quit ();
}

static void
create_graph_window (GpmSelfTest *test)
{
	GtkWidget *button_passed;
	GtkWidget *button_failed;
	GtkWidget *vbox;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	graph = gpm_graph_widget_new ();
	vbox = gtk_vbox_new (FALSE, 0);
	label = gtk_label_new("Title");

	button_passed = gtk_button_new_with_label("Passed");
	gtk_signal_connect(GTK_OBJECT(button_passed), "clicked", GTK_SIGNAL_FUNC(clicked_passed_cb), test);
	button_failed = gtk_button_new_with_label("Failed");
	gtk_signal_connect(GTK_OBJECT(button_failed), "clicked", GTK_SIGNAL_FUNC(clicked_failed_cb), test);

	gtk_widget_set_size_request (graph, 600, 300);
	gtk_signal_connect (GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(close_handler), test);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), graph, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button_passed, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button_failed, FALSE, FALSE, 0);
	gtk_container_border_width (GTK_CONTAINER (window), 0);

	gtk_widget_show (vbox);
	gtk_widget_show (label);
	gtk_widget_show (button_passed);
	gtk_widget_show (button_failed);
}

static void
wait_for_input (GpmSelfTest *test)
{
	gtk_widget_hide_all (window);
	gtk_widget_show_all (window);
	gtk_main ();
}

void
gpm_st_title_graph (GpmSelfTest *test, const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	gtk_label_set_label (GTK_LABEL (label), va_args_buffer);
	g_print ("> check #%u\t%s: \t%s...", test->total+1, test->type, va_args_buffer);
	test->total++;
}

void
gpm_st_graph_widget (GpmSelfTest *test)
{
	GpmArray *data;
	GpmArray *data_more;
	GpmArray *events;
	gboolean ret;

	test->type = "GpmGraphWidget   ";

	create_graph_window (test);
	gpm_graph_widget_enable_legend (GPM_GRAPH_WIDGET (graph), TRUE);
	gpm_graph_widget_enable_events (GPM_GRAPH_WIDGET (graph), TRUE);

	/********** TYPES *************/
	gpm_st_title_graph (test, "graph loaded, visible, no key, and set to y=percent x=time");
	wait_for_input (test);

	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	gpm_graph_widget_set_axis_type_y (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_TIME);

	gpm_st_title_graph (test, "now set to y=time x=percent");
	wait_for_input (test);

	/********** KEY DATA *************/
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), GPM_COLOUR_RED, "red data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), GPM_COLOUR_GREEN, "green data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), GPM_COLOUR_BLUE, "blue data");

	gpm_st_title_graph (test, "red green blue key data added");
	wait_for_input (test);

	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (graph));

	gpm_st_title_graph (test, "data items cleared, no key remains");
	wait_for_input (test);

	/********** KEY EVENT *************/
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 0,
					GPM_COLOUR_RED,
					GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
					"red circle");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 1,
					GPM_COLOUR_GREEN,
					GPM_GRAPH_WIDGET_SHAPE_SQUARE,
					"green square");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 2,
					GPM_COLOUR_BLUE,
					GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
					"blue triangle");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 3,
					GPM_COLOUR_WHITE,
					GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
					"white diamond");

	gpm_st_title_graph (test, "red green blue white key events added");
	wait_for_input (test);


	/********** KEY EVENT duplicate test *************/
	gpm_st_title (test, "duplicate key event test");
	ret = gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 3,
					      GPM_COLOUR_WHITE,
					      GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
					      "white diamond");
	if (ret == FALSE) {
		gpm_st_success (test, "refused duplicate id");
	} else {
		gpm_st_failed (test, "added duplicate ID!");
	}

	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (graph));

	gpm_st_title_graph (test, "event items cleared, no key remains");
	wait_for_input (test);

	/********** DATA *************/
	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	gpm_graph_widget_set_axis_type_y (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);

	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), GPM_COLOUR_RED, "red data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), GPM_COLOUR_BLUE, "blue data");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 0, GPM_COLOUR_GREEN, GPM_GRAPH_WIDGET_SHAPE_SQUARE, "green square");
	
	/********** ADD INVALID DATA *************/
	data = gpm_array_new ();
	gpm_array_append (data, 50, 0, GPM_COLOUR_RED);
	gpm_array_append (data, 40, 100, GPM_COLOUR_RED);
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_st_title (test, "add invalid data");
	ret = gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	if (ret == FALSE) {
		gpm_st_success (test, "ignored");
	} else {
		gpm_st_failed (test, "failed to ignore invalid data");
	}
	g_object_unref (data);

	/********** ADD NO DATA *************/
	data = gpm_array_new ();
	gpm_st_title (test, "add zero data");
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	ret = gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	if (ret == FALSE) {
		gpm_st_success (test, "ignored");
	} else {
		gpm_st_failed (test, "failed to ignore zero data");
	}
	g_object_unref (data);

	/********** ADD VALID DATA *************/
	data = gpm_array_new ();
	gpm_array_append (data, 0, 0, GPM_COLOUR_RED);
	gpm_array_append (data, 100, 100, GPM_COLOUR_RED);
	gpm_st_title (test, "add valid data");
	ret = gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	if (ret == TRUE) {
		gpm_st_success (test, NULL);
	} else {
		gpm_st_failed (test, "failed to add valid data");
	}

	/********** SHOW VALID DATA *************/
	gpm_st_title_graph (test, "red line shown gradient up");
	wait_for_input (test);

	/*********** second line **************/
	data_more = gpm_array_new ();
	gpm_array_append (data_more, 0, 100, GPM_COLOUR_BLUE);
	gpm_array_append (data_more, 100, 0, GPM_COLOUR_BLUE);
	gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data_more);

	gpm_st_title_graph (test, "red line shown gradient up, blue gradient down");
	wait_for_input (test);

	/*********** dots **************/
	events = gpm_array_new ();
	gpm_array_append (events, 25, 0, 0);
	gpm_array_append (events, 50, 0, 0);
	gpm_array_append (events, 75, 0, 0);
	gpm_graph_widget_events_add (GPM_GRAPH_WIDGET (graph), events);

	gpm_st_title_graph (test, "events follow red line (primary)");
	wait_for_input (test);

	/*********** stacked dots **************/
	gpm_array_append (events, 76, 0, 0);
	gpm_array_append (events, 77, 0, 0);
	gpm_graph_widget_events_add (GPM_GRAPH_WIDGET (graph), events);

	gpm_st_title_graph (test, "three events stacked at ~75");
	wait_for_input (test);

	/*********** events removed **************/
	gpm_graph_widget_events_clear (GPM_GRAPH_WIDGET (graph));
	gpm_st_title_graph (test, "events removed");
	wait_for_input (test);

	/*********** data lines removed **************/
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_st_title_graph (test, "all lines and event removed");
	wait_for_input (test);

	g_object_unref (events);
	g_object_unref (data);
	g_object_unref (data_more);

	/********** AUTORANGING PERCENT (close) *************/
	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), GPM_COLOUR_RED, "red data");
	data = gpm_array_new ();
	gpm_array_append (data, 0, 75, GPM_COLOUR_RED);
	gpm_array_append (data, 20, 78, GPM_COLOUR_RED);
	gpm_array_append (data, 40, 74, GPM_COLOUR_RED);
	gpm_array_append (data, 60, 72, GPM_COLOUR_RED);
	gpm_array_append (data, 80, 78, GPM_COLOUR_RED);
	gpm_array_append (data, 100, 79, GPM_COLOUR_RED);
	gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	gpm_st_title_graph (test, "autorange y axis between 70 and 80");
	wait_for_input (test);
	g_object_unref (data);

	/********** AUTORANGING PERCENT (extreams) *************/
	data = gpm_array_new ();
	gpm_array_append (data, 0, 6, GPM_COLOUR_RED);
	gpm_array_append (data, 100, 85, GPM_COLOUR_RED);
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	gpm_st_title_graph (test, "autorange y axis between 0 and 100");
	wait_for_input (test);
	g_object_unref (data);
}

