/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#ifndef __GPM_POWER_H
#define __GPM_POWER_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	BATT_TYPE_PRIMARY,
	BATT_TYPE_UPS,
	BATT_TYPE_MOUSE,
	BATT_TYPE_KEYBOARD,
	BATT_TYPE_PDA,
	BATT_TYPE_LAST
} BatteryType;

typedef struct {
	int      design_charge;
	int      last_full_charge;
	int      current_charge;
	int      charge_rate;
	int      percentage_charge;
	int      remaining_time;
	gboolean is_rechargeable;
	gboolean is_present;
	gboolean is_charging;
	gboolean is_discharging;
} BatteryStatus;

#define GPM_TYPE_POWER         (gpm_power_get_type ())
#define GPM_POWER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_POWER, GpmPower))
#define GPM_POWER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_POWER, GpmPowerClass))
#define GPM_IS_POWER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_POWER))
#define GPM_IS_POWER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_POWER))
#define GPM_POWER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_POWER, GpmPowerClass))

typedef struct GpmPowerPrivate GpmPowerPrivate;

typedef struct
{
        GObject          parent;
        GpmPowerPrivate *priv;
} GpmPower;

typedef struct
{
        GObjectClass      parent_class;
        void              (* button_pressed)        (GpmPower           *power,
                                                     const char         *type,
                                                     const char         *detail,
                                                     gboolean            state);
        void              (* ac_state_changed)      (GpmPower           *power,
						     gboolean            on_ac);
        void              (* battery_power_changed) (GpmPower           *power,
						     BatteryType         battery_type,
						     int                 percentage,
						     int                 minutes,
						     gboolean            discharging);
} GpmPowerClass;

GType            gpm_power_get_type           (void);

GpmPower       * gpm_power_new                (void);

gboolean         gpm_power_get_on_ac               (GpmPower           *power,
						    gboolean           *on_ac,
						    GError            **error);

gboolean         gpm_power_get_battery_status      (GpmPower           *power,
						    BatteryType         battery_type,
						    BatteryStatus      *battery_status);

gboolean         gpm_power_get_status_summary      (GpmPower           *power,
						    char              **summary,
						    GError            **error);

G_END_DECLS

#endif /* __GPM_POWER_H */
