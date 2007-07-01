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

#ifndef __GPMINHIBIT_H
#define __GPMINHIBIT_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

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
	void		(* has_inhibit_changed)		(GpmInhibit	*inhibit,
							 gboolean	 has_inhibit);
} GpmInhibitClass;

typedef enum
{
	 GPM_INHIBIT_ERROR_GENERAL
} GpmInhibitError;

GpmInhibit	*gpm_inhibit_new			(void);
GType		 gpm_inhibit_get_type			(void);
GQuark		 gpm_inhibit_error_quark		(void);
gboolean	 gpm_inhibit_has_inhibit		(GpmInhibit	*inhibit,
							 gboolean	*valid,
							 GError		**error);
void		 gpm_inhibit_get_message		(GpmInhibit	*inhibit,
							 GString	*message,
							 const gchar	*action);

void		 gpm_inhibit_inhibit			(GpmInhibit	*inhibit,
							 const gchar	*application,
							 const gchar	*reason,
							 DBusGMethodInvocation *context,
							 GError		**error);
gboolean	 gpm_inhibit_un_inhibit			(GpmInhibit	*inhibit,
							 guint32	 cookie,
							 GError		**error);
gboolean	 gpm_inhibit_get_requests		(GpmInhibit	*inhibit,
							 gchar		***requests,
							 GError		**error);


G_END_DECLS

#endif	/* __GPMINHIBIT_H */
