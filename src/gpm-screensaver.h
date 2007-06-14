/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMSCREENSAVER_H
#define __GPMSCREENSAVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_SCREENSAVER		(gpm_screensaver_get_type ())
#define GPM_SCREENSAVER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_SCREENSAVER, GpmScreensaver))
#define GPM_SCREENSAVER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_SCREENSAVER, GpmScreensaverClass))
#define GPM_IS_SCREENSAVER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_SCREENSAVER))
#define GPM_IS_SCREENSAVER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_SCREENSAVER))
#define GPM_SCREENSAVER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_SCREENSAVER, GpmScreensaverClass))

#define GS_LISTENER_SERVICE	"org.gnome.ScreenSaver"
#define GS_LISTENER_PATH	"/org/gnome/ScreenSaver"
#define GS_LISTENER_INTERFACE	"org.gnome.ScreenSaver"

typedef struct GpmScreensaverPrivate GpmScreensaverPrivate;

typedef struct
{
	GObject		       parent;
	GpmScreensaverPrivate *priv;
} GpmScreensaver;

typedef struct
{
	GObjectClass	parent_class;
	void		(* gs_delay_changed)		(GpmScreensaver	*screensaver,
					    		 gint		 delay);
	void		(* connection_changed)		(GpmScreensaver	*screensaver,
					    		 gboolean	 connected);
	void		(* auth_request)		(GpmScreensaver	*screensaver,
					    		 gboolean	 auth);
	void		(* session_idle_changed)	(GpmScreensaver	*screensaver,
					    		 gboolean	 is_idle);
	void		(* powersave_idle_changed)	(GpmScreensaver	*screensaver,
					    		 gboolean	 is_idle);
} GpmScreensaverClass;

GType		 gpm_screensaver_get_type		(void);
GpmScreensaver	*gpm_screensaver_new			(void);

int		 gpm_screensaver_get_delay		(GpmScreensaver	*screensaver);
gboolean	 gpm_screensaver_lock			(GpmScreensaver	*screensaver);
gboolean	 gpm_screensaver_lock_enabled		(GpmScreensaver	*screensaver);
gboolean	 gpm_screensaver_lock_set		(GpmScreensaver	*screensaver,
							 gboolean	 lock);
gboolean	 gpm_screensaver_lock_enabled		(GpmScreensaver	*screensaver);
guint32 	 gpm_screensaver_add_throttle    	(GpmScreensaver	*screensaver,
							 const gchar	*reason);
gboolean 	 gpm_screensaver_remove_throttle    	(GpmScreensaver	*screensaver,
							 guint32         cookie);
gboolean	 gpm_screensaver_check_running		(GpmScreensaver	*screensaver);
gboolean	 gpm_screensaver_poke			(GpmScreensaver	*screensaver);
gboolean	 gpm_screensaver_get_idle		(GpmScreensaver	*screensaver,
							 gint		*time);

G_END_DECLS

#endif	/* __GPMSCREENSAVER_H */
