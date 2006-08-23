/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#ifndef __GPMHAL_POWER_H
#define __GPMHAL_POWER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_HAL_POWER		(gpm_hal_power_get_type ())
#define GPM_HAL_POWER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_HAL_POWER, GpmHalPower))
#define GPM_HAL_POWER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_HAL_POWER, GpmHalPowerClass))
#define GPM_IS_HAL_POWER(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_HAL_POWER))
#define GPM_IS_HAL_POWER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_HAL_POWER))
#define GPM_HAL_POWER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_HAL_POWER, GpmHalPowerClass))

typedef struct GpmHalPowerPrivate GpmHalPowerPrivate;

typedef struct
{
	GObject		  parent;
	GpmHalPowerPrivate *priv;
} GpmHalPower;

typedef struct
{
	GObjectClass	parent_class;
	void		(* daemon_start)		(GpmHalPower	*hal_power);
	void		(* daemon_stop)			(GpmHalPower	*hal_power);
} GpmHalPowerClass;

GType		 gpm_hal_power_get_type			(void);
GpmHalPower	*gpm_hal_power_new			(void);

gboolean	 gpm_hal_power_has_power_management	(GpmHalPower	*hal);
gboolean	 gpm_hal_power_can_suspend		(GpmHalPower	*hal);
gboolean	 gpm_hal_power_suspend			(GpmHalPower	*hal,
							 gint		 wakeup);
gboolean	 gpm_hal_power_can_hibernate		(GpmHalPower	*hal);
gboolean	 gpm_hal_power_hibernate		(GpmHalPower	*hal);
gboolean	 gpm_hal_power_shutdown			(GpmHalPower	*hal);
gboolean	 gpm_hal_power_reboot			(GpmHalPower	*hal);
gboolean	 gpm_hal_power_enable_power_save	(GpmHalPower	*hal,
							 gboolean	 enable);
gboolean	 gpm_hal_power_is_laptop		(GpmHalPower	*hal);
gboolean	 gpm_hal_power_is_on_ac			(GpmHalPower	*hal);

G_END_DECLS

#endif	/* __GPMHAL_POWER_H */
