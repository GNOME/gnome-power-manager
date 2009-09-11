/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_ENGINE_H
#define __GPM_ENGINE_H

#include <glib-object.h>
#include <devkit-power-gobject/devicekit-power.h>

G_BEGIN_DECLS

#define GPM_TYPE_ENGINE		(gpm_engine_get_type ())
#define GPM_ENGINE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_ENGINE, GpmEngine))
#define GPM_ENGINE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_ENGINE, GpmEngineClass))
#define GPM_IS_ENGINE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_ENGINE))
#define GPM_IS_ENGINE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_ENGINE))
#define GPM_ENGINE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_ENGINE, GpmEngineClass))

typedef struct GpmEnginePrivate GpmEnginePrivate;

typedef struct
{
	GObject		 parent;
	GpmEnginePrivate *priv;
} GpmEngine;

typedef struct
{
	GObjectClass	parent_class;
	void		(* icon_changed)	(GpmEngine	*engine,
						 gchar		*icon);
	void		(* summary_changed)	(GpmEngine	*engine,
						 gchar		*status);
	void		(* perhaps_recall)	(GpmEngine	*engine,
						 DkpDevice	*device,
						 const gchar	*oem_vendor,
						 const gchar	*website);
	void		(* low_capacity)	(GpmEngine	*engine,
						 DkpDevice	*device);
	void		(* charge_low)		(GpmEngine	*engine,
						 DkpDevice	*device);
	void		(* charge_critical)	(GpmEngine	*engine,
						 DkpDevice	*device);
	void		(* charge_action)	(GpmEngine	*engine,
						 DkpDevice	*device);
	void		(* fully_charged)	(GpmEngine	*engine,
						 DkpDevice	*device);
	void		(* discharging)		(GpmEngine	*engine,
						 DkpDevice	*device);
} GpmEngineClass;

GType		 gpm_engine_get_type		(void);
GpmEngine	*gpm_engine_new			(void);
gchar		*gpm_engine_get_icon		(GpmEngine	*engine);
gchar		*gpm_engine_get_summary		(GpmEngine	*engine);
GPtrArray	*gpm_engine_get_devices		(GpmEngine	*engine);

G_END_DECLS

#endif	/* __GPM_ENGINE_H */

