/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMPOWERMANAGER_H
#define __GPMPOWERMANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_POWERMANAGER		(gpm_powermanager_get_type ())
#define GPM_POWERMANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_POWERMANAGER, GpmPowermanager))
#define GPM_POWERMANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_POWERMANAGER, GpmPowermanagerClass))
#define GPM_IS_POWERMANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_POWERMANAGER))
#define GPM_IS_POWERMANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_POWERMANAGER))
#define GPM_POWERMANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_POWERMANAGER, GpmPowermanagerClass))

#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/PowerManager"
#define	GPM_DBUS_PATH_STATS		"/org/gnome/PowerManager/Statistics"
#define	GPM_DBUS_PATH_BRIGHT_LCD	"/org/gnome/PowerManager/BrightnessLcd"
#define	GPM_DBUS_INTERFACE		"org.gnome.PowerManager"
#define	GPM_DBUS_INTERFACE_STATS	"org.gnome.PowerManager.Statistics"
#define	GPM_DBUS_INTERFACE_BRIGHT_LCD	"org.gnome.PowerManager.BrightnessLcd"

typedef struct GpmPowermanagerPrivate GpmPowermanagerPrivate;

typedef struct
{
	GObject		       parent;
	GpmPowermanagerPrivate *priv;
} GpmPowermanager;

typedef struct
{
	GObjectClass	parent_class;
} GpmPowermanagerClass;

GType		 gpm_powermanager_get_type		(void);
GpmPowermanager	*gpm_powermanager_new			(void);

gboolean	 gpm_powermanager_get_brightness_lcd	(GpmPowermanager *powermanager,
							 guint		 *brightness);
gboolean	 gpm_powermanager_set_brightness_lcd	(GpmPowermanager *powermanager,
							 guint		  brightness);
gboolean	 gpm_powermanager_inhibit		(GpmPowermanager *powermanager,
							 const gchar     *appname,
		   				    	 const gchar     *reason,
						         guint	         *cookie);
gboolean	 gpm_powermanager_uninhibit		(GpmPowermanager *powermanager,
							 guint            cookie);

G_END_DECLS

#endif	/* __GPMPOWERMANAGER_H */
