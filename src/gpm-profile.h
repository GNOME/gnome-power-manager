/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_PROFILE_H
#define __GPM_PROFILE_H

#include <glib-object.h>
#include "gpm-array.h"

G_BEGIN_DECLS

#define GPM_TYPE_PROFILE		(gpm_profile_get_type ())
#define GPM_PROFILE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_PROFILE, GpmProfile))
#define GPM_PROFILE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_PROFILE, GpmProfileClass))
#define GPM_IS_PROFILE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_PROFILE))
#define GPM_IS_PROFILE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_PROFILE))
#define GPM_PROFILE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_PROFILE, GpmProfileClass))

#define GPM_PROFILE_GOOD_TRUST	40

typedef struct GpmProfilePrivate GpmProfilePrivate;

typedef struct
{
	GObject			 parent;
	GpmProfilePrivate	*priv;
} GpmProfile;

typedef struct
{
	GObjectClass	parent_class;
} GpmProfileClass;

GType			 gpm_profile_get_type			(void);
GpmProfile		*gpm_profile_new			(void);

gboolean		 gpm_profile_set_config_id		(GpmProfile	*profile,
								 const gchar	*config_id);
gboolean		 gpm_profile_use_guessing		(GpmProfile	*profile,
								 gboolean	 use_guessing);
gboolean		 gpm_profile_register_charging		(GpmProfile	*profile,
								 gboolean	 is_charging);
gboolean		 gpm_profile_register_percentage	(GpmProfile	*profile,
								 guint		 percentage);
gboolean		 gpm_profile_delete_data		(GpmProfile	*profile,
								 gboolean	 discharging);
guint			 gpm_profile_get_accuracy		(GpmProfile	*profile,
								 guint		 percentage);
GpmArray		*gpm_profile_get_data_time_percent	(GpmProfile	*profile,
								 gboolean	 discharging);
GpmArray		*gpm_profile_get_data_accuracy_percent	(GpmProfile	*profile,
								 gboolean	 discharging);
gfloat			 gpm_profile_get_accuracy_average	(GpmProfile	*profile,
								 gboolean	 discharging);
guint			 gpm_profile_get_time			(GpmProfile	*profile,
								 guint		 percentage,
								 gboolean	 discharging);
void			 gpm_profile_print			(GpmProfile	*profile);

G_END_DECLS

#endif /* __GPM_PROFILE_H */
