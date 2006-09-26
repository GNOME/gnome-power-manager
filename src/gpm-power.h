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
	GPM_POWER_KIND_PRIMARY,
	GPM_POWER_KIND_UPS,
	GPM_POWER_KIND_MOUSE,
	GPM_POWER_KIND_KEYBOARD,
	GPM_POWER_KIND_PDA,
} GpmPowerKind;

typedef enum {
	GPM_POWER_UNIT_MWH,
	GPM_POWER_UNIT_CSR,
	GPM_POWER_UNIT_PERCENT,
	GPM_POWER_UNIT_UNKNOWN,
} GpmPowerUnit;

typedef struct {
	gint		design_charge;
	gint		last_full_charge;
	gint		current_charge;
	gint		charge_rate_smoothed;	/* exp ave smoothed */
	gint		charge_rate_raw;	/* no smoothing done */
	gint		percentage_charge;
	gint		remaining_time;
	gint		capacity;
	gboolean	is_rechargeable;
	gboolean	is_present;
	gboolean	is_charging;
	gboolean	is_discharging;
} GpmPowerStatus;

typedef struct {
	gchar		*udi;
	gchar		*product;
	gchar		*vendor;
	gchar		*technology;
	gchar		*serial;
	gchar		*model;
	gint		 charge_rate_previous;
	GpmPowerKind	 battery_kind;
	GpmPowerStatus	 battery_status;
	GpmPowerUnit	 unit;
} GpmPowerDevice;

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
	void		(* button_pressed)		(GpmPower	*power,
							 const gchar	*type,
							 gboolean	 state);
	void		(* ac_state_changed)		(GpmPower	*power,
							 gboolean	 on_ac);
	void		(* battery_status_changed)	(GpmPower	*power,
							 GpmPowerKind	 battery_kind);
	void		(* battery_removed)		(GpmPower	*power,
							 const gchar	*udi);
	void		(* battery_perhaps_recall)	(GpmPower	*power,
							 const gchar	*oem_vendor,
							 const gchar	*website);
} GpmPowerClass;

GType		 gpm_power_get_type			(void);
GpmPower	*gpm_power_new				(void);
gboolean	 gpm_power_get_on_ac			(GpmPower	*power,
							 gboolean	*on_ac,
							 GError		**error);
gboolean	 gpm_power_get_battery_status		(GpmPower	*power,
							 GpmPowerKind	 battery_kind,
							 GpmPowerStatus *battery_status);
GpmPowerDevice	*gpm_power_get_battery_device_entry	(GpmPower	*power,
							 GpmPowerKind	 battery_kind,
							 guint		 device_num);
gboolean	 gpm_power_get_status_summary		(GpmPower	*power,
							 gchar		**summary,
							 GError		**error);
gint		 gpm_power_get_num_devices_of_kind	(GpmPower	*power,
							 GpmPowerKind	 battery_kind);
GpmPowerDevice	*gpm_power_get_device_from_udi		(GpmPower	*power,
							 const gchar	*udi);
GString		*gpm_power_status_for_device		(GpmPowerDevice *device);
GString		*gpm_power_status_for_device_more	(GpmPowerDevice *device);
void		 gpm_power_update_all			(GpmPower	*power);
gboolean	 gpm_power_battery_is_charged		(GpmPowerStatus *status);
const gchar 	*gpm_power_kind_to_localised_string	(GpmPowerKind	 battery_kind);
const gchar 	*gpm_power_kind_to_string		(GpmPowerKind	 battery_kind);
gchar		*gpm_power_get_icon_from_status		(GpmPowerStatus *device_status,
							 GpmPowerKind    kind);

G_END_DECLS

#endif /* __GPM_POWER_H */
