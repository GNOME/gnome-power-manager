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

#ifndef __GPMSRV_SCREENSAVER_H
#define __GPMSRV_SCREENSAVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_SRV_SCREENSAVER		(gpm_srv_screensaver_get_type ())
#define GPM_SRV_SCREENSAVER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_SRV_SCREENSAVER, GpmSrvScreensaver))
#define GPM_SRV_SCREENSAVER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_SRV_SCREENSAVER, GpmSrvScreensaverClass))
#define GPM_IS_SRV_SCREENSAVER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_SRV_SCREENSAVER))
#define GPM_IS_SRV_SCREENSAVER_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_SRV_SCREENSAVER))
#define GPM_SRV_SCREENSAVER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_SRV_SCREENSAVER, GpmSrvScreensaverClass))

typedef struct GpmSrvScreensaverPrivate GpmSrvScreensaverPrivate;

typedef struct
{
	GObject		       parent;
	GpmSrvScreensaverPrivate *priv;
} GpmSrvScreensaver;

typedef struct
{
	GObjectClass	parent_class;
	void		(* gs_delay_changed)		(GpmSrvScreensaver	*srv_screensaver,
					    		 gint		 delay);
	void		(* connection_changed)		(GpmSrvScreensaver	*srv_screensaver,
					    		 gboolean	 connected);
	void		(* auth_request)		(GpmSrvScreensaver	*srv_screensaver,
					    		 gboolean	 auth);
	void		(* idle_changed)		(GpmSrvScreensaver	*srv_screensaver,
					    		 gboolean	 is_idle);
} GpmSrvScreensaverClass;

GType			 gpm_srv_screensaver_get_type	(void);
GpmSrvScreensaver	*gpm_srv_screensaver_new	(void);

G_END_DECLS

#endif	/* __GPMSRV_SCREENSAVER_H */
