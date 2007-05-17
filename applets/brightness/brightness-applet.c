/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * GNOME Power Manager Brightness Applet
 * Copyright (C) 2006 Benjamin Canou <bookeldor@gmail.com>
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
#include <gdk/gdkkeysyms.h>
#include <libdbus-proxy.h>

#include "brightness-applet.h"
#include "../src/gpm-common.h"

static void      gpm_brightness_applet_class_init (GpmBrightnessAppletClass *klass);
static void      gpm_brightness_applet_init       (GpmBrightnessApplet *applet);

G_DEFINE_TYPE (GpmBrightnessApplet, gpm_brightness_applet, PANEL_TYPE_APPLET)

static void      retrieve_icon                    (GpmBrightnessApplet *applet);
static void      check_size                       (GpmBrightnessApplet *applet);
static gboolean  draw_applet_cb                   (GpmBrightnessApplet *applet);
static gboolean  destroy_popup_cb                 (GpmBrightnessApplet *applet);
static void      update_tooltip                   (GpmBrightnessApplet *applet);
static void      update_level                     (GpmBrightnessApplet *applet, gboolean hw_get, gboolean hw_set);
static gboolean  plus_cb                          (GtkWidget *w, GpmBrightnessApplet *applet);
static gboolean  minus_cb                         (GtkWidget *w, GpmBrightnessApplet *applet);
static gboolean  key_press_cb                     (GpmBrightnessApplet *applet, GdkEventKey   *event);
static gboolean  scroll_cb                        (GpmBrightnessApplet *applet, GdkEventScroll *event);
static gboolean  slide_cb                         (GtkWidget *w, GpmBrightnessApplet *applet);
static void      create_popup                     (GpmBrightnessApplet *applet);
static gboolean  popup_cb                         (GpmBrightnessApplet *applet, GdkEventButton *event);
static void      dialog_about_cb                  (BonoboUIComponent *uic, gpointer data, const gchar *verbname);
static gboolean  bonobo_cb                        (PanelApplet *_applet, const gchar *iid, gpointer data);
static void      destroy_cb                       (GtkObject *object);

#define GPM_BRIGHTNESS_APPLET_OAFID		"OAFIID:GNOME_BrightnessApplet"
#define GPM_BRIGHTNESS_APPLET_FACTORY_OAFID	"OAFIID:GNOME_BrightnessApplet_Factory"
#define GPM_BRIGHTNESS_APPLET_ICON		"gpm-brightness-lcd"
#define GPM_BRIGHTNESS_APPLET_ICON_DISABLED	"gpm-brightness-lcd-disabled"
#define GPM_BRIGHTNESS_APPLET_NAME		_("Power Manager Brightness Applet")
#define GPM_BRIGHTNESS_APPLET_DESC		_("Adjusts laptop panel brightness.")
#define PANEL_APPLET_VERTICAL(p)					\
	 (((p) == PANEL_APPLET_ORIENT_LEFT) || ((p) == PANEL_APPLET_ORIENT_RIGHT))

/**
 * gpm_applet_get_brightness:
 * Return value: Success value, or zero for failure
 **/
static gboolean
gpm_applet_get_brightness (GpmBrightnessApplet *applet)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;
	guint policy_brightness;

	proxy = dbus_proxy_get_proxy (applet->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		printf ("jjk");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &policy_brightness,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		printf ("jjk");
		g_error_free (error);
	}
	if (ret == TRUE) {
		applet->level = policy_brightness;
	} else {
		/* abort as the DBUS method failed */
		printf ("jjk");
		g_warning ("GetBrightness failed!");
	}

	return ret;
}

/**
 * gpm_applet_set_brightness:
 * Return value: Success value, or zero for failure
 **/
static gboolean
gpm_applet_set_brightness (GpmBrightnessApplet *applet)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	proxy = dbus_proxy_get_proxy (applet->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "SetBrightness", &error,
				 G_TYPE_UINT, applet->level,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		g_warning ("SetBrightness failed!");
	}

	return ret;
}

/**
 * retrieve_icon:
 * @applet: Brightness applet instance
 *
 * retrieve an icon from stock with a size adapted to panel
 **/
static void
retrieve_icon (GpmBrightnessApplet *applet)
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
	if (applet->enabled == TRUE) {
		icon = GPM_BRIGHTNESS_APPLET_ICON;
	} else {
		icon = GPM_BRIGHTNESS_APPLET_ICON_DISABLED;
	}
	applet->icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						 icon, applet->size - 2, 0, NULL);

	if (applet->icon == NULL) {
		g_warning ("Cannot find %s!", icon);
	} else {
		/* update size cache */
		applet->icon_height = gdk_pixbuf_get_height (applet->icon);
		applet->icon_width = gdk_pixbuf_get_width (applet->icon);
	}
}

/**
 * check_size:
 * @applet: Brightness applet instance
 *
 * check if panel size has changed and applet adapt size
 **/
static void
check_size (GpmBrightnessApplet *applet)
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
 * @applet: Brightness applet instance
 *
 * draws applet content (background + icon)
 **/
static gboolean
draw_applet_cb (GpmBrightnessApplet *applet)
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

	/* if no icon, then don't try to display */
	if (applet->icon == NULL) {
		return FALSE;
	}

	w = GTK_WIDGET(applet)->allocation.width;
	h = GTK_WIDGET(applet)->allocation.height;

	/* draw background */
	backbuf = gdk_pixmap_new (GTK_WIDGET(applet)->window, w, h, -1);
	gc_backbuf = gdk_gc_new (backbuf);

	bg_type = panel_applet_get_background (PANEL_APPLET (applet), &color, &background);
	if (bg_type == PANEL_PIXMAP_BACKGROUND && !applet->popped) {
		/* fill with given background pixmap */
		gdk_draw_drawable (backbuf, gc_backbuf, background, 0, 0, 0, 0, w, h);
	} else {
		/* fill with appropriate color */
		if (applet->popped) {
			color = gtk_rc_get_style (GTK_WIDGET(applet))->bg[GTK_STATE_SELECTED];
		} else {
			if (bg_type == PANEL_NO_BACKGROUND) {
				color = gtk_rc_get_style (GTK_WIDGET(applet))->bg[GTK_STATE_NORMAL];
			}
		}
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
 * destroy_popup_cb:
 * @applet: Brightness applet instance
 *
 * destroys the popup (called if orientation has changed)
 **/
static gboolean
destroy_popup_cb (GpmBrightnessApplet *applet)
{
	if (applet->popup != NULL) {
		gtk_widget_set_parent (applet->popup, NULL);
		gtk_widget_destroy (applet->popup);
		applet->popup = NULL;
	}
	return TRUE;
}

/**
 * update_tooltip:
 * @applet: Brightness applet instance
 *
 * sets tooltip's content (percentage or disabled)
 **/
static void
update_tooltip (GpmBrightnessApplet *applet)
{
	static gchar buf[101];
	if (applet->popped == FALSE) {
		if (applet->enabled == TRUE) {
			snprintf (buf, 100, _("LCD brightness : %d%%"), applet->level);
		} else {
			snprintf (buf, 100, _("Cannot get laptop panel brightness"));
		}
		gtk_tooltips_set_tip (applet->tooltip, GTK_WIDGET(applet), buf, NULL);
	} else {
		gtk_tooltips_set_tip (applet->tooltip, GTK_WIDGET(applet), NULL, NULL);
	}
}

/**
 * update_level:
 * @applet: Brightness applet instance
 * @get_hw: set UI value from HW value
 * @set_hw: set HW value from UI value
 *
 * updates popup and hardware level of brightness
 * FALSE FAlSE -> set UI from cached value
 * TRUE  FAlSE -> set UI from HW value
 * TRUE  FALSE -> set HW from UI value, then set UI from HW value
 * FALSE TRUE  -> set HW from UI value
 **/
static void
update_level (GpmBrightnessApplet *applet, gboolean get_hw, gboolean set_hw)
{
	if (set_hw == TRUE) {
		printf ("set applet->level=%i\n", applet->level);
		applet->enabled = gpm_applet_set_brightness (applet);
	}
	if (get_hw == TRUE) {
		applet->enabled = gpm_applet_get_brightness (applet);
		printf ("get applet->level=%i\n", applet->level);
	}
	if (applet->popup != NULL) {
		gtk_widget_set_sensitive (applet->btn_plus,applet->level < 99);
		gtk_widget_set_sensitive (applet->btn_minus,applet->level > 0);


		gtk_range_set_value (GTK_RANGE(applet->slider), (guint) applet->level);
	}
	update_tooltip (applet);
}

/**
 * plus_cb:
 * @widget: The sending widget (plus button)
 * @applet: Brightness applet instance
 *
 * callback for the plus button
 **/
static gboolean
plus_cb (GtkWidget *w, GpmBrightnessApplet *applet)
{
	if (applet->level < 99) {
		applet->level++;
	}
	update_level (applet, FALSE, TRUE);
	return TRUE;
}

/**
 * minus_cb:
 * @widget: The sending widget (minus button)
 * @applet: Brightness applet instance
 *
 * callback for the minus button
 **/
static gboolean
minus_cb (GtkWidget *w, GpmBrightnessApplet *applet)
{
	if (applet->level > 0) {
		applet->level--;
	}
	update_level (applet, FALSE, TRUE);
	return TRUE;
}

/**
 * slide_cb:
 * @widget: The sending widget (slider)
 * @applet: Brightness applet instance
 *
 * callback for the slider
 **/
static gboolean
slide_cb (GtkWidget *w, GpmBrightnessApplet *applet)
{
	applet->level = gtk_range_get_value (GTK_RANGE(applet->slider));
	update_level (applet, FALSE, TRUE);
	return TRUE;
}

/**
 * slide_cb:
 * @applet: Brightness applet instance
 * @event: The key press event
 *
 * callback handling keyboard
 * mainly escape to unpop and arrows to change brightness
 **/
static gboolean
key_press_cb (GpmBrightnessApplet *applet, GdkEventKey *event)
{
	int i;

	switch (event->keyval) {
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
	case GDK_3270_Enter:
	case GDK_Return:
	case GDK_space:
	case GDK_KP_Space:
	case GDK_Escape:
		/* if yet popped, release focus and hide then redraw applet unselected */
		if (applet->popped) {
			gdk_keyboard_ungrab (GDK_CURRENT_TIME);
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			gtk_grab_remove (GTK_WIDGET(applet));
			gtk_widget_set_state (GTK_WIDGET(applet), GTK_STATE_NORMAL);
			gtk_widget_hide (applet->popup);
			applet->popped = FALSE;
			draw_applet_cb (applet);
			update_tooltip (applet);
			return TRUE;
		} else {
			return FALSE;
		}
		break;
	case GDK_Page_Up:
		for (i = 0;i < 10;i++) {
			plus_cb (NULL, applet);
		}
		return TRUE;
		break;
	case GDK_Left:
	case GDK_Up:
		plus_cb (NULL, applet);
		return TRUE;
		break;
	case GDK_Page_Down:
		for (i = 0;i < 10;i++) {
			minus_cb (NULL, applet);
		}
		return TRUE;
		break;
	case GDK_Right:
	case GDK_Down:
		minus_cb (NULL, applet);
		return TRUE;
		break;
	default:
		return FALSE;
		break;
	}

	return FALSE;
}

/**
 * scroll_cb:
 * @applet: Brightness applet instance
 * @event: The scroll event
 *
 * callback handling mouse scrolls, either when the applet
 * is not popped and the mouse is over the applet, or when
 * the applet is popped and no matter where the mouse is
 **/
static gboolean
scroll_cb (GpmBrightnessApplet *applet, GdkEventScroll *event)
{
	if (event->type == GDK_SCROLL) {
		if (event->direction == GDK_SCROLL_UP) {
			plus_cb (NULL, applet);
		} else {
			minus_cb (NULL, applet);
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * create_popup:
 * @applet: Brightness applet instance
 *
 * cretes a new popup according to orientation of panel
 **/
static void
create_popup (GpmBrightnessApplet *applet)
{
	static GtkWidget *box, *frame;
	gint orientation = panel_applet_get_orient (PANEL_APPLET (PANEL_APPLET (applet)));

	destroy_popup_cb (applet);

	/* slider */
	if (PANEL_APPLET_VERTICAL(orientation)) {
		applet->slider = gtk_hscale_new_with_range (0, 100, 1);
		gtk_widget_set_size_request (applet->slider, 100, -1);
	} else {
		applet->slider = gtk_vscale_new_with_range (0, 100, 1);
		gtk_widget_set_size_request (applet->slider, -1, 100);
	}
	gtk_range_set_inverted (GTK_RANGE(applet->slider), TRUE);
	gtk_scale_set_draw_value (GTK_SCALE(applet->slider), FALSE);
	gtk_range_set_value (GTK_RANGE(applet->slider), applet->level);
	g_signal_connect (G_OBJECT(applet->slider), "value-changed", G_CALLBACK(slide_cb), applet);

	/* minus button */
	applet->btn_minus = gtk_button_new_with_label ("\342\210\222"); /* U+2212 MINUS SIGN */
	gtk_button_set_relief (GTK_BUTTON(applet->btn_minus), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(applet->btn_minus), "pressed", G_CALLBACK(minus_cb), applet);

	/* plus button */
	applet->btn_plus = gtk_button_new_with_label ("+");
	gtk_button_set_relief (GTK_BUTTON(applet->btn_plus), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(applet->btn_plus), "pressed", G_CALLBACK(plus_cb), applet);

	/* box */
	if (PANEL_APPLET_VERTICAL(orientation)) {
		box = gtk_hbox_new (FALSE, 1);
	} else {
		box = gtk_vbox_new (FALSE, 1);
	}
	gtk_box_pack_start (GTK_BOX(box), applet->btn_plus, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(box), applet->slider, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(box), applet->btn_minus, FALSE, FALSE, 0);

	/* frame */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER(frame), box);

	/* window */
	applet->popup = gtk_window_new (GTK_WINDOW_POPUP);
	GTK_WIDGET_UNSET_FLAGS (applet->popup, GTK_TOPLEVEL);
	gtk_widget_set_parent (applet->popup, GTK_WIDGET(applet));
	gtk_container_add (GTK_CONTAINER(applet->popup), frame);
}

/**
 * popup_cb:
 * @applet: Brightness applet instance
 *
 * pops and unpops
 **/
static gboolean
popup_cb (GpmBrightnessApplet *applet, GdkEventButton *event)
{
	gint orientation, x, y;

	/* react only to left mouse button */
	if (event->button != 1) {
		return FALSE;
	}

	/* if yet popped, release focus and hide then redraw applet unselected */
	if (applet->popped) {
		gdk_keyboard_ungrab (GDK_CURRENT_TIME);
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		gtk_grab_remove (GTK_WIDGET(applet));
		gtk_widget_set_state (GTK_WIDGET(applet), GTK_STATE_NORMAL);
		gtk_widget_hide (applet->popup);
		applet->popped = FALSE;
		draw_applet_cb (applet);
		update_tooltip (applet);
		return TRUE;
	}

	/* update UI for current brightness */
	update_level (applet, TRUE, FALSE);

	/* if disabled, don't pop */
	if (applet->enabled == FALSE) {
		return TRUE;
	}

	/* otherwise pop */
	applet->popped = TRUE;
	draw_applet_cb (applet);

	/* create a new popup (initial or if panel parameters changed) */
	if (applet->popup == NULL) {
		create_popup (applet);
	}

	/* update UI for current brightness */
	update_level (applet, FALSE, FALSE);

	gtk_widget_show_all (applet->popup);

	/* retrieve geometry parameters and move window appropriately */
	orientation = panel_applet_get_orient (PANEL_APPLET (PANEL_APPLET (applet)));
	gdk_window_get_origin (GTK_WIDGET(applet)->window, &x, &y);

	switch (orientation) {
	case PANEL_APPLET_ORIENT_DOWN:
		x += GTK_WIDGET(applet)->allocation.x
			+ GTK_WIDGET(applet)->allocation.width/2;
		y += GTK_WIDGET(applet)->allocation.y
			+ GTK_WIDGET(applet)->allocation.height;
		x -= applet->popup->allocation.width/2;
		break;
	case PANEL_APPLET_ORIENT_UP:
		x += GTK_WIDGET(applet)->allocation.x
			+ GTK_WIDGET(applet)->allocation.width/2;
		y += GTK_WIDGET(applet)->allocation.y;
		x -= applet->popup->allocation.width/2;
		y -= applet->popup->allocation.height;
		break;
	case PANEL_APPLET_ORIENT_RIGHT:
		y += GTK_WIDGET(applet)->allocation.y
			+ GTK_WIDGET(applet)->allocation.height/2;
		x += GTK_WIDGET(applet)->allocation.x
			+ GTK_WIDGET(applet)->allocation.width;
		y -= applet->popup->allocation.height/2;
		break;
	case PANEL_APPLET_ORIENT_LEFT:
		y += GTK_WIDGET(applet)->allocation.y
			+ GTK_WIDGET(applet)->allocation.height/2;
		x += GTK_WIDGET(applet)->allocation.x;
		x -= applet->popup->allocation.width;
		y -= applet->popup->allocation.height/2;
		break;
	default:
		g_assert_not_reached ();
	}

	gtk_window_move (GTK_WINDOW (applet->popup), x, y);

	/* grab input */
	gtk_widget_grab_focus (GTK_WIDGET(applet));
	gtk_grab_add (GTK_WIDGET(applet));
	gdk_pointer_grab (GTK_WIDGET(applet)->window, TRUE,
			  GDK_BUTTON_PRESS_MASK |
			  GDK_BUTTON_RELEASE_MASK |
			  GDK_POINTER_MOTION_MASK,
			  NULL, NULL, GDK_CURRENT_TIME);
	gdk_keyboard_grab (GTK_WIDGET(applet)->window,
			   TRUE, GDK_CURRENT_TIME);
	gtk_widget_set_state (GTK_WIDGET(applet), GTK_STATE_SELECTED);

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
					  GPM_BRIGHTNESS_APPLET_ICON,
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
	gtk_about_dialog_set_name (about, GPM_BRIGHTNESS_APPLET_NAME);
	gtk_about_dialog_set_version (about, VERSION);
	gtk_about_dialog_set_copyright (about, _("Copyright \xc2\xa9 2006 Benjamin Canou"));
	gtk_about_dialog_set_comments (about, GPM_BRIGHTNESS_APPLET_DESC);
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
	GpmBrightnessApplet *applet = GPM_BRIGHTNESS_APPLET(data);
	GnomeProgram *program = gnome_program_get ();

	gnome_help_display_with_doc_id (program, "gnome-power-manager",
					"gnome-power-manager.xml",
					"applets-brightness", &error);
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
	GpmBrightnessApplet *applet = GPM_BRIGHTNESS_APPLET(object);

	if (applet->gproxy != NULL) {
		g_object_unref (applet->gproxy);
	}
	if (applet->icon != NULL) {
		gdk_pixbuf_unref (applet->icon);
	}
}

/**
 * gpm_brightness_applet_class_init:
 * @klass: Class instance
 **/
static void
gpm_brightness_applet_class_init (GpmBrightnessAppletClass *class)
{
	/* nothing to do here */
}

/**
 * gpm_brightness_applet_init:
 * @applet: Brightness applet instance
 **/
static void
gpm_brightness_applet_init (GpmBrightnessApplet *applet)
{
	/* initialize fields */
	applet->size = 0;
	applet->enabled = FALSE;
	applet->popped = FALSE;
	applet->popup = NULL;
	applet->icon = NULL;
	applet->tooltip = gtk_tooltips_new ();

	applet->gproxy = dbus_proxy_new ();
	dbus_proxy_assign (applet->gproxy,
			   DBUS_PROXY_SESSION,
			   GPM_DBUS_SERVICE,
			   GPM_DBUS_PATH_BACKLIGHT,
			   GPM_DBUS_INTERFACE_BACKLIGHT);

	update_level (applet, TRUE, FALSE);

	/* prepare */
	panel_applet_set_flags (PANEL_APPLET (applet), PANEL_APPLET_EXPAND_MINOR);

	/* show */
	gtk_widget_show_all (GTK_WIDGET(applet));

	/* set appropriate size and load icon accordingly */
	check_size (applet);
	draw_applet_cb (applet);

	/* connect */
	g_signal_connect (G_OBJECT(applet), "button-press-event",
			  G_CALLBACK(popup_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "scroll-event",
			  G_CALLBACK(scroll_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "key-press-event",
			  G_CALLBACK(key_press_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "expose-event",
			  G_CALLBACK(draw_applet_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "change-background",
			  G_CALLBACK(draw_applet_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "change-orient",
			  G_CALLBACK(draw_applet_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "change-orient",
			  G_CALLBACK(destroy_popup_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "destroy",
			  G_CALLBACK(destroy_cb), NULL);
}

/**
 * bonobo_cb:
 * @_applet: GpmBrightnessApplet instance created by the bonobo factory
 * @iid: Bonobo id
 *
 * the function called by bonobo factory after creation
 **/
static gboolean
bonobo_cb (PanelApplet *_applet, const gchar *iid, gpointer data)
{
	GpmBrightnessApplet *applet = GPM_BRIGHTNESS_APPLET(_applet);

	static BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("About", dialog_about_cb),
		BONOBO_UI_VERB ("Help", help_cb),
		BONOBO_UI_VERB_END
	};

	if (strcmp (iid, GPM_BRIGHTNESS_APPLET_OAFID) != 0) {
		return FALSE;
	}

	panel_applet_setup_menu_from_file (PANEL_APPLET (applet),
					   DATADIR,
					   "GNOME_BrightnessApplet.xml",
					   NULL, verbs, applet);
	draw_applet_cb (applet);
	return TRUE;
}

/**
 * this generates a main with a bonobo factory
 **/
PANEL_APPLET_BONOBO_FACTORY
 (/* the factory iid */
 GPM_BRIGHTNESS_APPLET_FACTORY_OAFID,
 /* generates brighness applets instead of regular gnome applets  */
 GPM_TYPE_BRIGHTNESS_APPLET,
 /* the applet name and version */
 "BrightnessApplet", VERSION,
 /* our callback (with no user data) */
 bonobo_cb, NULL);
