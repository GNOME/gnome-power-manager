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

#ifndef __GPM_WARNING_H
#define __GPM_WARNING_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_WARNING		(gpm_warning_get_type ())
#define GPM_WARNING(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_WARNING, GpmWarning))
#define GPM_WARNING_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_WARNING, GpmWarningClass))
#define GPM_IS_WARNING(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_WARNING))
#define GPM_IS_WARNING_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_WARNING))
#define GPM_WARNING_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_WARNING, GpmWarningClass))

typedef enum {
	GPM_WARNING_NONE = 0,
	GPM_WARNING_DISCHARGING = 1,
	GPM_WARNING_LOW = 2,
	GPM_WARNING_VERY_LOW = 3,
	GPM_WARNING_CRITICAL = 4,
	GPM_WARNING_ACTION = 5
} GpmWarningState;

typedef enum {
	GPM_WARNING_TIME = 0,
	GPM_WARNING_PERCENTAGE = 1,
	GPM_WARNING_AUTO = 2
} GpmWarningPolicy;

typedef struct GpmWarningPrivate GpmWarningPrivate;

typedef struct
{
	GObject		   parent;
	GpmWarningPrivate *priv;
} GpmWarning;

typedef struct
{
	GObjectClass	parent_class;
} GpmWarningClass;

GType		 gpm_warning_get_type	(void);
GpmWarning	*gpm_warning_new	(void);

GpmWarningState	 gpm_warning_get_state	(GpmWarning		*warning,
					 GpmPowerStatus		*status,
					 GpmWarningPolicy	 policy);
const gchar	*gpm_warning_get_title	(GpmWarningState	 warning_type);

G_END_DECLS

#endif /* __GPM_WARNING_H */
