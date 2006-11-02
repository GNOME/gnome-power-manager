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

#ifndef __GPM_BATTERY_H
#define __GPM_BATTERY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_BATTERY		(gpm_battery_get_type ())
#define GPM_BATTERY(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BATTERY, GpmBattery))
#define GPM_BATTERY_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BATTERY, GpmBatteryClass))
#define GPM_IS_BATTERY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BATTERY))
#define GPM_IS_BATTERY_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BATTERY))
#define GPM_BATTERY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BATTERY, GpmBatteryClass))

typedef struct GpmBatteryPrivate GpmBatteryPrivate;

typedef struct
{
	GObject			 parent;
	GpmBatteryPrivate	*priv;
} GpmBattery;

typedef struct
{
	GObjectClass	parent_class;
	void		(* button_pressed)		(GpmBattery	*battery,
							 const gchar	*type,
							 gboolean	 state);
	void		(* battery_added)		(GpmBattery	*battery,
							 const gchar	*udi);
	void		(* battery_removed)		(GpmBattery	*battery,
							 const gchar	*udi);
	void		(* battery_modified)		(GpmBattery	*battery,
							 const gchar	*udi,
							 const gchar	*key,
							 gboolean	 finally);
} GpmBatteryClass;

GType			 gpm_battery_get_type		(void);
GpmBattery		*gpm_battery_new		(void);

void			 gpm_battery_coldplug		(GpmBattery	*battery);

G_END_DECLS

#endif /* __GPM_BATTERY_H */
