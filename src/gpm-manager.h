/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifndef __GPM_MANAGER_H
#define __GPM_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_MANAGER         (gpm_manager_get_type ())
#define GPM_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_MANAGER, GpmManager))
#define GPM_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_MANAGER, GpmManagerClass))
#define GS_IS_MANAGER(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_MANAGER))
#define GS_IS_MANAGER_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_MANAGER))
#define GPM_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_MANAGER, GpmManagerClass))

typedef struct GpmManagerPrivate GpmManagerPrivate;

typedef struct
{
        GObject            parent;
        GpmManagerPrivate *priv;
} GpmManager;

typedef struct
{
        GObjectClass      parent_class;

        void              (* on_ac_changed)         (GpmManager *manager,
                                                     gboolean    on_ac);
        void              (* dpms_mode_changed)     (GpmManager *manager,
                                                     const char *mode);
} GpmManagerClass;

GType          gpm_manager_get_type         (void);

GpmManager   * gpm_manager_new              (void);

gboolean       gpm_manager_get_on_ac        (GpmManager    *manager,
                                             gboolean      *on_ac,
                                             GError       **error);
gboolean       gpm_manager_set_on_ac        (GpmManager    *manager,
                                             gboolean       on_ac,
                                             GError       **error);

gboolean       gpm_manager_get_dpms_mode    (GpmManager    *manager,
                                             const char   **mode,
                                             GError       **error);
gboolean       gpm_manager_set_dpms_mode    (GpmManager    *manager,
                                             const char    *mode,
                                             GError       **error);

void           gpm_manager_suspend          (GpmManager    *manager);
void           gpm_manager_hibernate        (GpmManager    *manager);
void           gpm_manager_shutdown         (GpmManager    *manager);

G_END_DECLS

#endif /* __GPM_MANAGER_H */
