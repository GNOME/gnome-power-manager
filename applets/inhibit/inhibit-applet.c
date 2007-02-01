/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * GNOME Power Manager Inhibit Applet
 * Copyright (C) 2006 Benjamin Canou <bookeldor@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <panel-applet.h>
#include <gtk/gtk.h>
#include <gtk/gtkbox.h>
#include <libgnomeui/gnome-help.h>

#include "inhibit-applet.h"
#include "../../src/gpm-common.h"

static void      gpm_inhibit_applet_class_init (GpmInhibitAppletClass *klass);
static void      gpm_inhibit_applet_init       (GpmInhibitApplet *applet);

G_DEFINE_TYPE (GpmInhibitApplet, gpm_inhibit_applet, PANEL_TYPE_APPLET)

static void      retrieve_icon                    (GpmInhibitApplet *applet);
static void      check_size                       (GpmInhibitApplet *applet);
static gboolean  draw_applet_cb                   (GpmInhibitApplet *applet);
static void      update_tooltip                   (GpmInhibitApplet *applet);
static gboolean  click_cb                         (GpmInhibitApplet *applet, GdkEventButton *event);
static void      dialog_about_cb                  (BonoboUIComponent *uic, gpointer data, const gchar *verbname);
static gboolean  bonobo_cb                        (PanelApplet *_applet, const gchar *iid, gpointer data);
static void      destroy_cb                       (GtkObject *object);

#define GPM_INHIBIT_APPLET_OAFID		"OAFIID:GNOME_InhibitApplet"
#define GPM_INHIBIT_APPLET_FACTORY_OAFID	"OAFIID:GNOME_InhibitApplet_Factory"
#define GPM_INHIBIT_APPLET_ICON_INHIBIT		"gpm-inhibit"
#define GPM_INHIBIT_APPLET_ICON_UNINHIBIT	"gpm-hibernate"
#define GPM_INHIBIT_APPLET_NAME			_("Power Manager Inhibit Applet")
#define GPM_INHIBIT_APPLET_DESC			_("Allows user to inhibit automatic power saving.")
#define PANEL_APPLET_VERTICAL(p)					\
	 (((p) == PANEL_APPLET_ORIENT_LEFT) || ((p) == PANEL_APPLET_ORIENT_RIGHT))

/**
 * retrieve_icon:
 * @applet: Inhibit applet instance
 *
 * retrieve an icon from stock with a size adapted to panel
 **/
static void
retrieve_icon (GpmInhibitApplet *applet)
{
	const gchar *icon;

	/* free */
	if (applet->icon != NULL) {
		g_object_unref (applet->icon);
		applet->icon = NULL;
	}

	if (applet->size <= 2) {
		return;
	}

	/* get icon */
	if (applet->cookie > 0) {
		icon = GPM_INHIBIT_APPLET_ICON_INHIBIT;
	} else {
		icon = GPM_INHIBIT_APPLET_ICON_UNINHIBIT;
	}
	applet->icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						 icon,
						 applet->size - 2,
						 0,
						 NULL);

	/* update size cache */
	applet->icon_height = gdk_pixbuf_get_height (applet->icon);
	applet->icon_width = gdk_pixbuf_get_width (applet->icon);
}

/**
 * check_size:
 * @applet: Inhibit applet instance
 *
 * check if panel size has changed and applet adapt size
 **/
static void
check_size (GpmInhibitApplet *applet)
{
	/* we don't use the size function here, but the yet allocated size because the
	   size value is false (kind of rounded) */
	if (PANEL_APPLET_VERTICAL(panel_applet_get_orient (PANEL_APPLET (applet)))) {
		if (applet->size != GTK_WIDGET(applet)->allocation.width) {
			applet->size = GTK_WIDGET(applet)->allocation.width;
			retrieve_icon (applet);
			gtk_widget_set_size_request (GTK_WIDGET(applet), applet->size, applet->icon_height + 2);
		}
	} else {
		if (applet->size != GTK_WIDGET(applet)->allocation.height) {
			applet->size = GTK_WIDGET(applet)->allocation.height;
			retrieve_icon (applet);
			gtk_widget_set_size_request (GTK_WIDGET(applet), applet->icon_width + 2, applet->size);
		}
	}
}

/**
 * draw_applet_cb:
 * @applet: Inhibit applet instance
 *
 * draws applet content (background + icon)
 **/
static gboolean
draw_applet_cb (GpmInhibitApplet *applet)
{
	gint w, h, bg_type;

	GdkColor color;
	GdkPixmap *backbuf, *background;
	GdkGC *gc_backbuf, *gc_widget;

	if (GTK_WIDGET(applet)->window == NULL) {
		return FALSE;
	}

	/* retrieve applet size */
	check_size (applet);
	if (applet->size <= 2) {
		return FALSE;
	}

	w = GTK_WIDGET(applet)->allocation.width;
	h = GTK_WIDGET(applet)->allocation.height;

	/* draw background */
	backbuf = gdk_pixmap_new (GTK_WIDGET(applet)->window, w, h, -1);
	gc_backbuf = gdk_gc_new (backbuf);

	bg_type = panel_applet_get_background (PANEL_APPLET (applet), &color, &background);
	if (bg_type == PANEL_PIXMAP_BACKGROUND) {
		/* fill with given background pixmap */
		gdk_draw_drawable (backbuf, gc_backbuf, background, 0, 0, 0, 0, w, h);
	} else {
		/* fill with appropriate color */
		color = gtk_rc_get_style (GTK_WIDGET(applet))->bg[GTK_STATE_NORMAL];
		gdk_gc_set_rgb_fg_color (gc_backbuf,&color);
		gdk_gc_set_fill (gc_backbuf,GDK_SOLID);
		gdk_draw_rectangle (backbuf, gc_backbuf, TRUE, 0, 0, w, h);
	}

	/* draw icon at center */
	gdk_draw_pixbuf (backbuf, gc_backbuf, applet->icon,
			 0, 0, (w - applet->icon_width)/2, (h - applet->icon_height)/2,
			 applet->icon_width, applet->icon_height,
			 GDK_RGB_DITHER_NONE, 0, 0);

	/* blit back buffer to applet */
	gc_widget = gdk_gc_new (GTK_WIDGET(applet)->window);
	gdk_draw_drawable (GTK_WIDGET(applet)->window,
			   gc_widget, backbuf,
			   0, 0, 0, 0, w, h);

	return TRUE;
}

/**
 * update_tooltip:
 * @applet: Inhibit applet instance
 *
 * sets tooltip's content (percentage or disabled)
 **/
static void
update_tooltip (GpmInhibitApplet *applet)
{
	static gchar buf[101];
	if (applet->cookie > 0) {
		snprintf (buf, 100, _("Automatic sleep inhibited"));
	} else {
		snprintf (buf, 100, _("Automatic sleep enabled"));
	}
	gtk_tooltips_set_tip (applet->tooltip, GTK_WIDGET(applet), buf, NULL);
}

/**
 * click_cb:
 * @applet: Inhibit applet instance
 *
 * pops and unpops
 **/
static gboolean
click_cb (GpmInhibitApplet *applet, GdkEventButton *event)
{
	/* react only to left mouse button */
	if (event->button != 1) {
		return FALSE;
	}

	if (applet->cookie > 0) {
		g_debug ("uninhibiting %u", applet->cookie);
		gpm_powermanager_uninhibit (applet->powermanager, applet->cookie);
		applet->cookie = 0;
	} else {
		g_debug ("inhibiting");
		gpm_powermanager_inhibit_auto (applet->powermanager,
					       GPM_INHIBIT_APPLET_NAME,
					       _("Manual inhibit"),
					       &(applet->cookie));
	}
	/* update icon */
	retrieve_icon (applet);
	update_tooltip (applet);
	draw_applet_cb (applet);

	return TRUE;
}

/**
 * dialog_about_cb:
 *
 * displays about dialog
 **/
static void
dialog_about_cb (BonoboUIComponent *uic, gpointer data, const gchar *verbname)
{
	GtkAboutDialog *about;

	GdkPixbuf *logo =
		gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
					  GPM_INHIBIT_APPLET_ICON_INHIBIT,
					  128, 0, NULL);

	static const gchar *authors[] = {
		"Benjamin Canou <bookeldor@gmail.com>",
		"Richard Hughes <richard@hughsie.com>",
		NULL
	};
	const char *documenters [] = {
		NULL
	};
	const char *license[] = {
		 N_("Licensed under the GNU General Public License Version 2"),
		 N_("Power Manager is free software; you can redistribute it and/or\n"
		   "modify it under the terms of the GNU General Public License\n"
		   "as published by the Free Software Foundation; either version 2\n"
		   "of the License, or (at your option) any later version."),
		 N_("Power Manager is distributed in the hope that it will be useful,\n"
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		   "GNU General Public License for more details."),
		 N_("You should have received a copy of the GNU General Public License\n"
		   "along with this program; if not, write to the Free Software\n"
		   "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA\n"
		   "02110-1301, USA.")
	};
	const char *translator_credits = NULL;
	char	   *license_trans;

	license_trans = g_strconcat (_(license[0]), "\n\n", _(license[1]), "\n\n",
				     _(license[2]), "\n\n", _(license[3]), "\n", NULL);

	about = (GtkAboutDialog*) gtk_about_dialog_new ();
	gtk_about_dialog_set_name (about, GPM_INHIBIT_APPLET_NAME);
	gtk_about_dialog_set_version (about, VERSION);
	gtk_about_dialog_set_copyright (about, _("Copyright \xc2\xa9 2006 Richard Hughes"));
	gtk_about_dialog_set_comments (about, GPM_INHIBIT_APPLET_DESC);
	gtk_about_dialog_set_authors (about, authors);
	gtk_about_dialog_set_documenters (about, documenters);
	gtk_about_dialog_set_translator_credits (about, translator_credits);
	gtk_about_dialog_set_logo (about, logo);
	gtk_about_dialog_set_license (about, license_trans);
	gtk_about_dialog_set_website (about, GPM_HOMEPAGE_URL);

	g_signal_connect (G_OBJECT(about), "response",
			  G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show (GTK_WIDGET(about));

	g_free (license_trans);
	gdk_pixbuf_unref (logo);
}

/**
 * help_cb:
 *
 * open gpm help
 **/
static void
help_cb (BonoboUIComponent *uic, gpointer data, const gchar *verbname)
{
	GError *error = NULL;
	GpmInhibitApplet *applet = GPM_INHIBIT_APPLET(data);
	GnomeProgram *program = gnome_program_get ();

	gnome_help_display_with_doc_id (program, "gnome-power-manager",
					"gnome-power-manager.xml",
					"applets-inhibit", &error);
	if (error != NULL) {
		GtkWidget *dialog =
			gtk_message_dialog_new (GTK_WINDOW (GTK_WIDGET(applet)->parent),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_error_free (error);
	}
}

/**
 * destroy_cb:
 * @object: Class instance to destroy
 **/
static void
destroy_cb (GtkObject *object)
{
	GpmInhibitApplet *applet = GPM_INHIBIT_APPLET(object);

	if (applet->powermanager != NULL) {
		g_object_unref (applet->powermanager);
	}
	if (applet->icon != NULL) {
		gdk_pixbuf_unref (applet->icon);
	}
}

/**
 * gpm_inhibit_applet_class_init:
 * @klass: Class instance
 **/
static void
gpm_inhibit_applet_class_init (GpmInhibitAppletClass *class)
{
	/* nothing to do here */
}

/**
 * gpm_inhibit_applet_init:
 * @applet: Inhibit applet instance
 **/
static void
gpm_inhibit_applet_init (GpmInhibitApplet *applet)
{
	/* initialize fields */
	applet->size = 0;
	applet->icon = NULL;
	applet->cookie = 0;
	applet->tooltip = gtk_tooltips_new ();
	applet->powermanager = gpm_powermanager_new ();
	update_tooltip (applet);

	/* prepare */
	panel_applet_set_flags (PANEL_APPLET (applet), PANEL_APPLET_EXPAND_MINOR);

	/* show */
	gtk_widget_show_all (GTK_WIDGET(applet));

	/* set appropriate size and load icon accordingly */
	check_size (applet);
	draw_applet_cb (applet);

	/* connect */
	g_signal_connect (G_OBJECT(applet), "button-press-event",
			  G_CALLBACK(click_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "expose-event",
			  G_CALLBACK(draw_applet_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "change-background",
			  G_CALLBACK(draw_applet_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "change-orient",
			  G_CALLBACK(draw_applet_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "destroy",
			  G_CALLBACK(destroy_cb), NULL);
}

/**
 * bonobo_cb:
 * @_applet: GpmInhibitApplet instance created by the bonobo factory
 * @iid: Bonobo id
 *
 * the function called by bonobo factory after creation
 **/
static gboolean
bonobo_cb (PanelApplet *_applet, const gchar *iid, gpointer data)
{
	GpmInhibitApplet *applet = GPM_INHIBIT_APPLET(_applet);

	static BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("About", dialog_about_cb),
		BONOBO_UI_VERB ("Help", help_cb),
		BONOBO_UI_VERB_END
	};

	if (strcmp (iid, GPM_INHIBIT_APPLET_OAFID) != 0) {
		return FALSE;
	}

	panel_applet_setup_menu_from_file (PANEL_APPLET (applet),
					   DATADIR,
					   "GNOME_InhibitApplet.xml",
					   NULL, verbs, applet);
	draw_applet_cb (applet);
	return TRUE;
}

/**
 * this generates a main with a bonobo factory
 **/
PANEL_APPLET_BONOBO_FACTORY
 (/* the factory iid */
 GPM_INHIBIT_APPLET_FACTORY_OAFID,
 /* generates brighness applets instead of regular gnome applets  */
 GPM_TYPE_INHIBIT_APPLET,
 /* the applet name and version */
 "InhibitApplet", VERSION,
 /* our callback (with no user data) */
 bonobo_cb, NULL);
