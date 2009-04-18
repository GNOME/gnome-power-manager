/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2009 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2007-2009 William Jon McCann <mccann@jhu.edu>
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

#ifndef __EGG_IDLETIME_H
#define __EGG_IDLETIME_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EGG_TYPE_IDLETIME		(egg_idletime_get_type ())
#define EGG_IDLETIME(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EGG_TYPE_IDLETIME, EggIdletime))
#define EGG_IDLETIME_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EGG_TYPE_IDLETIME, EggIdletimeClass))
#define EGG_IS_IDLETIME(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EGG_TYPE_IDLETIME))
#define EGG_IS_IDLETIME_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EGG_TYPE_IDLETIME))
#define EGG_IDLETIME_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EGG_TYPE_IDLETIME, EggIdletimeClass))

typedef struct EggIdletimePrivate EggIdletimePrivate;

typedef struct
{
	GObject			 parent;
	EggIdletimePrivate	*priv;
} EggIdletime;

typedef struct
{
	GObjectClass	 parent_class;
	void		(* alarm_expired)	(EggIdletime	*idletime,
						 guint		 timer_id);
	void		(* reset)		(EggIdletime	*idletime);
} EggIdletimeClass;

GType		 egg_idletime_get_type		(void);
EggIdletime	*egg_idletime_new		(void);
guint		 egg_idletime_add_watch		(EggIdletime	*idletime,
						 guint		 interval);
void		 egg_idletime_remove_watch	(EggIdletime	*idletime,
						 guint		 id);
#ifdef EGG_TEST
void		 egg_idletime_test		(gpointer	 data);
#endif

G_END_DECLS

#endif /* __EGG_IDLETIME_H */
