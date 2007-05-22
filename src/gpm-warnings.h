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

#ifndef __GPM_WARNINGS_H
#define __GPM_WARNINGS_H

#include <glib-object.h>
#include "gpm-cell-unit.h"

G_BEGIN_DECLS

#define GPM_TYPE_WARNINGS		(gpm_warnings_get_type ())
#define GPM_WARNINGS(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_WARNINGS, GpmWarnings))
#define GPM_WARNINGS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_WARNINGS, GpmWarningsClass))
#define GPM_IS_WARNINGS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_WARNINGS))
#define GPM_IS_WARNINGS_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_WARNINGS))
#define GPM_WARNINGS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_WARNINGS, GpmWarningsClass))

typedef enum {
	GPM_WARNINGS_NONE = 0,
	GPM_WARNINGS_DISCHARGING = 1,
	GPM_WARNINGS_LOW = 2,
	GPM_WARNINGS_CRITICAL = 3,
	GPM_WARNINGS_ACTION = 4
} GpmWarningsState;

typedef struct GpmWarningsPrivate GpmWarningsPrivate;

typedef struct
{
	GObject		   parent;
	GpmWarningsPrivate *priv;
} GpmWarnings;

typedef struct
{
	GObjectClass	parent_class;
} GpmWarningsClass;

GType		 gpm_warnings_get_type	(void);
GpmWarnings	*gpm_warnings_new	(void);

GpmWarningsState	 gpm_warnings_get_state	(GpmWarnings		*warnings,
					 GpmCellUnit		*unit);

G_END_DECLS

#endif /* __GPM_WARNINGS_H */
