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

#ifndef __INHIBIT_APPLET_H
#define __INHIBIT_APPLET_H

#include <glib-object.h>
#include <panel-applet.h>
#include <libdbus-proxy.h>

G_BEGIN_DECLS

#define GPM_TYPE_INHIBIT_APPLET		(gpm_inhibit_applet_get_type ())
#define GPM_INHIBIT_APPLET(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_INHIBIT_APPLET, GpmInhibitApplet))
#define GPM_INHIBIT_APPLET_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_INHIBIT_APPLET, GpmInhibitAppletClass))
#define GPM_IS_INHIBIT_APPLET(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_INHIBIT_APPLET))
#define GPM_IS_INHIBIT_APPLET_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_INHIBIT_APPLET))
#define GPM_INHIBIT_APPLET_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_INHIBIT_APPLET, GpmInhibitAppletClass))

typedef struct{
	PanelApplet parent;
	/* applet state */
	guint cookie;
	GtkTooltips *tooltip;
	/* the icon and a cache for size*/
	GdkPixbuf *icon;
	gint icon_width, icon_height;
	/* connection to g-p-m */
	DbusProxy *gproxy;
	guint level;
	/* a cache for panel size */
	gint size;
} GpmInhibitApplet;

typedef struct{
	PanelAppletClass	parent_class;
} GpmInhibitAppletClass;

GType                gpm_inhibit_applet_get_type  (void);

G_END_DECLS

#endif /* __INHIBIT_APPLET_H */
