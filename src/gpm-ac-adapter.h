/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GPM_AC_ADAPTER_H
#define __GPM_AC_ADAPTER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_AC_ADAPTER		(gpm_ac_adapter_get_type ())
#define GPM_AC_ADAPTER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_AC_ADAPTER, GpmAcAdapter))
#define GPM_AC_ADAPTER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_AC_ADAPTER, GpmAcAdapterClass))
#define GPM_IS_AC_ADAPTER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_AC_ADAPTER))
#define GPM_IS_AC_ADAPTER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_AC_ADAPTER))
#define GPM_AC_ADAPTER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_AC_ADAPTER, GpmAcAdapterClass))

typedef struct GpmAcAdapterPrivate GpmAcAdapterPrivate;

typedef struct
{
	GObject		      parent;
	GpmAcAdapterPrivate *priv;
} GpmAcAdapter;

typedef struct
{
	GObjectClass	parent_class;
	void		(* ac_adapter_changed)	(GpmAcAdapter	*ac_adapter,
						 gboolean	 on_ac);
} GpmAcAdapterClass;

GType		 gpm_ac_adapter_get_type	(void);
GpmAcAdapter	*gpm_ac_adapter_new		(void);

gboolean	 gpm_ac_adapter_is_present	(GpmAcAdapter	*ac_adapter);

G_END_DECLS

#endif /* __GPM_AC_ADAPTER_H */
