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
	test->type = "GpmGraphWidget   ";

	create_graph_window (test);
	gpm_graph_widget_enable_legend (GPM_GRAPH_WIDGET (graph), TRUE);
	gpm_graph_widget_enable_events (GPM_GRAPH_WIDGET (graph), TRUE);

	/********** TYPES *************/
	gpm_st_title_graph (test, "graph loaded, visible, and set to y=percent x=time");
	wait_for_input (test);

	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	gpm_graph_widget_set_axis_type_y (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_TIME);

	gpm_st_title_graph (test, "graph loaded, visible, and set to y=time x=percent");
	wait_for_input (test);

	/********** KEY DATA *************/
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), 0xff0000, "red data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), 0x00ff00, "blue data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), 0x0000ff, "green data");

	gpm_st_title_graph (test, "red green blue key data added");
	wait_for_input (test);

	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (graph));

	gpm_st_title_graph (test, "data items cleared, no key remains");
	wait_for_input (test);

	/********** KEY EVENT *************/
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 0, 0xff0000, GPM_GRAPH_WIDGET_SHAPE_CIRCLE, "red circle");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 1, 0x00ff00, GPM_GRAPH_WIDGET_SHAPE_SQUARE, "green square");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 2, 0x0000ff, GPM_GRAPH_WIDGET_SHAPE_TRIANGLE, "blue triangle");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 3, 0xffffff, GPM_GRAPH_WIDGET_SHAPE_DIAMOND, "white diamond");
	/* todo, check if id's are not repeating... */

	gpm_st_title_graph (test, "red green blue white key events added");
	wait_for_input (test);

	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (graph));

	gpm_st_title_graph (test, "event items cleared, no key remains");
	wait_for_input (test);


}

