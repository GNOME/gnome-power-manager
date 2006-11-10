/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GPM_POLICY_H
#define __GPM_POLICY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_POLICY		(gpm_policy_get_type ())
#define GPM_POLICY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_POLICY, GpmPolicy))
#define GPM_POLICY_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_POLICY, GpmPolicyClass))
#define GPM_IS_POLICY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_POLICY))
#define GPM_IS_POLICY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_POLICY))
#define GPM_POLICY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_POLICY, GpmPolicyClass))

typedef struct GpmPolicyPrivate GpmPolicyPrivate;

typedef struct
{
	GObject		      parent;
	GpmPolicyPrivate *priv;
} GpmPolicy;

typedef struct
{
	GObjectClass	parent_class;
} GpmPolicyClass;

GType		 gpm_policy_get_type		(void);
GpmPolicy	*gpm_policy_new			(void);
gboolean	 gpm_policy_allowed_suspend	(GpmPolicy	*policy,
						 gboolean	*can);
gboolean	 gpm_policy_allowed_hibernate	(GpmPolicy	*policy,
						 gboolean	*can);
gboolean	 gpm_policy_allowed_shutdown	(GpmPolicy	*policy,
						 gboolean	*can);
gboolean	 gpm_policy_allowed_reboot	(GpmPolicy	*policy,
						 gboolean	*can);

G_END_DECLS

#endif /* __GPM_POLICY_H */
