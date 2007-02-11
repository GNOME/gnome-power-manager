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

#ifndef __GPMPOLKIT_H
#define __GPMPOLKIT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_POLKIT		(gpm_polkit_get_type ())
#define GPM_POLKIT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_POLKIT, GpmPolkit))
#define GPM_POLKIT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_POLKIT, GpmPolkitClass))
#define GPM_IS_POLKIT(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_POLKIT))
#define GPM_IS_POLKIT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_POLKIT))
#define GPM_POLKIT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_POLKIT, GpmPolkitClass))

typedef struct GpmPolkitPrivate GpmPolkitPrivate;

typedef struct
{
	GObject		  parent;
	GpmPolkitPrivate *priv;
} GpmPolkit;

typedef struct
{
	GObjectClass	parent_class;
	void		(* daemon_start)		(GpmPolkit	*polkit);
	void		(* daemon_stop)			(GpmPolkit	*polkit);
} GpmPolkitClass;

GType		 gpm_polkit_get_type			(void);
GpmPolkit	*gpm_polkit_new				(void);

gboolean	 gpm_polkit_is_user_privileged		(GpmPolkit	*polkit,
							 const gchar	*privilege);

G_END_DECLS

#endif	/* __GPMPOLKIT_H */
