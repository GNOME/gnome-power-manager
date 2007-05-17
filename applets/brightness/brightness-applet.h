/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * GNOME Power Manager / Brightness Applet
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

#ifndef __BRIGHTNESS_APPLET_H
#define __BRIGHTNESS_APPLET_H

#include <glib-object.h>
#include <panel-applet.h>
#include <dbus/dbus-glib.h>
#include <libdbus-watch.h>

G_BEGIN_DECLS

#define GPM_TYPE_BRIGHTNESS_APPLET		(gpm_brightness_applet_get_type ())
#define GPM_BRIGHTNESS_APPLET(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BRIGHTNESS_APPLET, GpmBrightnessApplet))
#define GPM_BRIGHTNESS_APPLET_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BRIGHTNESS_APPLET, GpmBrightnessAppletClass))
#define GPM_IS_BRIGHTNESS_APPLET(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BRIGHTNESS_APPLET))
#define GPM_IS_BRIGHTNESS_APPLET_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BRIGHTNESS_APPLET))
#define GPM_BRIGHTNESS_APPLET_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BRIGHTNESS_APPLET, GpmBrightnessAppletClass))

typedef struct{
	PanelApplet parent;
	/* applet state */
	gboolean call_worked; /* g-p-m refusing action */
	gboolean popped; /* the popup is shown */
	/* the popup and its widgets */
	GtkWidget *popup, *slider, *btn_plus, *btn_minus;
	GtkTooltips *tooltip;
	/* the icon and a cache for size*/
	GdkPixbuf *icon;
	gint icon_width, icon_height;
	/* connection to g-p-m */
	DBusGProxy *proxy;
	DBusGConnection *connection;
	DbusWatch *watch;
	guint level;
	/* a cache for panel size */
	gint size;
} GpmBrightnessApplet;

typedef struct{
	PanelAppletClass	parent_class;
} GpmBrightnessAppletClass;

GType                gpm_brightness_applet_get_type  (void);

G_END_DECLS

#endif /* __BRIGHTNESS_APPLET_H */

