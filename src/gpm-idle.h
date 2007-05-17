/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GPM_IDLE_H
#define __GPM_IDLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_IDLE		(gpm_idle_get_type ())
#define GPM_IDLE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_IDLE, GpmIdle))
#define GPM_IDLE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_IDLE, GpmIdleClass))
#define GPM_IS_IDLE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_IDLE))
#define GPM_IS_IDLE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_IDLE))
#define GPM_IDLE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_IDLE, GpmIdleClass))

typedef enum {
	GPM_IDLE_MODE_NORMAL,
	GPM_IDLE_MODE_POWERSAVE,
	GPM_IDLE_MODE_SESSION,
	GPM_IDLE_MODE_SYSTEM
} GpmIdleMode;

typedef struct GpmIdlePrivate GpmIdlePrivate;

typedef struct
{
	GObject		parent;
	GpmIdlePrivate *priv;
} GpmIdle;

typedef struct
{
	GObjectClass	parent_class;

	void		(* idle_changed)		(GpmIdle	*idle,
							 GpmIdleMode	 mode);
} GpmIdleClass;

GType		 gpm_idle_get_type			(void);
GpmIdle		*gpm_idle_new				(void);

GpmIdleMode	 gpm_idle_get_mode			(GpmIdle	*idle);
void		 gpm_idle_set_mode			(GpmIdle	*idle,
							 GpmIdleMode	 mode);
void		 gpm_idle_set_check_cpu			(GpmIdle	*idle,
							 gboolean	 check_type_cpu);
void		 gpm_idle_set_system_timeout		(GpmIdle	*idle,
							 guint		 timeout);

G_END_DECLS

#endif /* __GPM_IDLE_H */
