/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2005-2006 Jaap Haitsma <jaap@haitsma.org>
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

#ifndef __GPM_POWER_H
#define __GPM_POWER_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GPM_POWER_BATTERY_KIND_PRIMARY,
	GPM_POWER_BATTERY_KIND_UPS,
	GPM_POWER_BATTERY_KIND_MOUSE,
	GPM_POWER_BATTERY_KIND_KEYBOARD,
	GPM_POWER_BATTERY_KIND_PDA,
} GpmPowerBatteryKind;

typedef struct {
	int		design_charge;
	int		last_full_charge;
	int		current_charge;
	int		charge_rate;
	int		percentage_charge;
	int		remaining_time;
	int		capacity;
	gboolean	is_rechargeable;
	gboolean	is_present;
	gboolean	is_charging;
	gboolean	is_discharging;
} GpmPowerBatteryStatus;

typedef struct
{
	char	*title;
	char	*value;
} GpmPowerDescriptionItem;

#define GPM_TYPE_POWER	 	(gpm_power_get_type ())
#define GPM_POWER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_POWER, GpmPower))
#define GPM_POWER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_POWER, GpmPowerClass))
#define GPM_IS_POWER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_POWER))
#define GPM_IS_POWER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_POWER))
#define GPM_POWER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_POWER, GpmPowerClass))

typedef struct GpmPowerPrivate GpmPowerPrivate;

typedef struct
{
	GObject		 parent;
	GpmPowerPrivate *priv;
} GpmPower;

typedef struct
{
	GObjectClass parent_class;
	void		(* button_pressed)	(GpmPower		*power,
						 const char		*type,
						 gboolean		 state);
	void		(* ac_state_changed)	(GpmPower		*power,
						 gboolean		 on_ac);
	void		(* battery_status_changed) (GpmPower	 	*power,
						 GpmPowerBatteryKind	 battery_kind);
	void		(* battery_removed)	(GpmPower		*power,
						 const char		 *udi);
} GpmPowerClass;

GType		 gpm_power_get_type		(void);
GpmPower	*gpm_power_new			(void);
gboolean	 gpm_power_get_on_ac		(GpmPower		*power,
						 gboolean		*on_ac,
						 GError			**error);
void		 gpm_power_dbus_name_owner_changed (GpmPower		*power,
						    const char		*name,
						    const char		*prev,
						    const char		*new);
gboolean	 gpm_power_get_battery_status	(GpmPower		*power,
						 GpmPowerBatteryKind	 battery_kind,
						 GpmPowerBatteryStatus	*battery_status);
gboolean	 gpm_power_get_status_summary	(GpmPower		*power,
						 char			**summary,
						 GError			**error);
gint		 gpm_power_get_num_devices_of_kind (GpmPower		*power,
						    GpmPowerBatteryKind	 battery_kind);
GArray		*gpm_power_get_description_array (GpmPower		*power,
						  GpmPowerBatteryKind	 battery_kind,
						  gint		 device_num);
void		 gpm_power_update_all		(GpmPower *power);
void		 gpm_power_free_description_array (GArray *array);

gboolean	 gpm_power_battery_is_charged	(GpmPowerBatteryStatus	*status);
const char 	*battery_kind_to_string		(GpmPowerBatteryKind	 battery_kind);

G_END_DECLS

#endif /* __GPM_POWER_H */
