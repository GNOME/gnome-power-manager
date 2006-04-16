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

#ifndef __GPMINHIBIT_H
#define __GPMINHIBIT_H

#include <glib-object.h>
#include "gpm-power.h"

G_BEGIN_DECLS

#define GPM_TYPE_INHIBIT		(gpm_inhibit_get_type ())
#define GPM_INHIBIT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_INHIBIT, GpmInhibit))
#define GPM_INHIBIT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_INHIBIT, GpmInhibitClass))
#define GPM_IS_INHIBIT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_INHIBIT))
#define GPM_IS_INHIBIT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_INHIBIT))
#define GPM_INHIBIT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_INHIBIT, GpmInhibitClass))

typedef struct GpmInhibitPrivate GpmInhibitPrivate;

typedef struct
{
	GObject		   parent;
	GpmInhibitPrivate *priv;
} GpmInhibit;

typedef struct
{
	GObjectClass	parent_class;
} GpmInhibitClass;

GpmInhibit	*gpm_inhibit_new			(void);
GType		 gpm_inhibit_get_type			(void);

void		 gpm_inhibit_set_power			(GpmInhibit	*inhibit,
							 GpmPower	*power);
int		 gpm_inhibit_add			(GpmInhibit	*inhibit,
							 const char	*connection,
							 const char	*application,
							 const char	*reason);
void		 gpm_inhibit_remove			(GpmInhibit	*inhibit,
							 const char	*connection,
							 int		 cookie);
gboolean	 gpm_inhibit_check			(GpmInhibit	*inhibit);
void		 gpm_inhibit_get_message		(GpmInhibit	*inhibit,
							 GString	*message,
							 const char	*action);
G_END_DECLS

#endif	/* __GPMINHIBIT_H */
