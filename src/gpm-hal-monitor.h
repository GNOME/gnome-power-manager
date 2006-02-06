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

#ifndef __GPM_HAL_MONITOR_H
#define __GPM_HAL_MONITOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_HAL_MONITOR          (gpm_hal_monitor_get_type ())
#define GPM_HAL_MONITOR(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_HAL_MONITOR, GpmHalMonitor))
#define GPM_HAL_MONITOR_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_HAL_MONITOR, GpmHalMonitorClass))
#define GPM_IS_HAL_MONITOR(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_HAL_MONITOR))
#define GPM_IS_HAL_MONITOR_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_HAL_MONITOR))
#define GPM_HAL_MONITOR_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_HAL_MONITOR, GpmHalMonitorClass))

typedef struct GpmHalMonitorPrivate GpmHalMonitorPrivate;

typedef struct
{
        GObject               parent;
        GpmHalMonitorPrivate *priv;
} GpmHalMonitor;

typedef struct
{
        GObjectClass      parent_class;

        void              (* button_pressed)        (GpmHalMonitor    *monitor,
                                                     const char       *type,
                                                     const char       *detail,
                                                     gboolean          state);

        void              (* ac_power_changed)      (GpmHalMonitor    *monitor,
                                                     gboolean          on_ac);

        void              (* battery_added)         (GpmHalMonitor    *monitor,
                                                     const char       *udi,
                                                     const char       *reserved);
        void              (* battery_removed)       (GpmHalMonitor    *monitor,
                                                     const char       *udi);

        void              (* battery_property_modified) (GpmHalMonitor    *monitor,
                                                         const char       *udi,
                                                         const char       *key);
} GpmHalMonitorClass;

GType             gpm_hal_monitor_get_type         (void);

GpmHalMonitor   * gpm_hal_monitor_new              (void);

gboolean          gpm_hal_monitor_get_on_ac        (GpmHalMonitor *monitor);

G_END_DECLS

#endif /* __GPM_HAL_MONITOR_H */
