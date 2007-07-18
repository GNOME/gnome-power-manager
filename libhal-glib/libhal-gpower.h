/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __LIBGPOWER_H
#define __LIBGPOWER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LIBHAL_TYPE_GPOWER		(hal_gpower_get_type ())
#define LIBHAL_GPOWER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LIBHAL_TYPE_GPOWER, HalGPower))
#define LIBHAL_GPOWER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LIBHAL_TYPE_GPOWER, HalGPowerClass))
#define LIBHAL_IS_GPOWER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LIBHAL_TYPE_GPOWER))
#define LIBHAL_IS_GPOWER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LIBHAL_TYPE_GPOWER))
#define LIBHAL_GPOWER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LIBHAL_TYPE_GPOWER, HalGPowerClass))

typedef struct HalGPowerPrivate HalGPowerPrivate;

typedef struct
{
	GObject		  parent;
	HalGPowerPrivate *priv;
} HalGPower;

typedef struct
{
	GObjectClass	parent_class;
} HalGPowerClass;

GType		 hal_gpower_get_type			(void);
HalGPower	*hal_gpower_new				(void);

gboolean	 hal_gpower_has_support			(HalGPower	*power);
gboolean	 hal_gpower_can_suspend			(HalGPower	*power);
gboolean	 hal_gpower_can_hibernate		(HalGPower	*power);
gboolean	 hal_gpower_suspend			(HalGPower	*power,
							 guint		 wakeup,
							 GError		**error);
gboolean	 hal_gpower_hibernate			(HalGPower	*power,
							 GError		**error);
gboolean	 hal_gpower_shutdown			(HalGPower	*power,
							 GError		**error);
gboolean	 hal_gpower_reboot			(HalGPower	*power,
							 GError		**error);
gboolean	 hal_gpower_has_suspend_error		(HalGPower	*power,
							 gboolean	*state);
gboolean	 hal_gpower_has_hibernate_error		(HalGPower	*power,
							 gboolean	*state);
gboolean	 hal_gpower_clear_suspend_error		(HalGPower	*power,
							 GError		**error);
gboolean	 hal_gpower_clear_hibernate_error	(HalGPower	*power,
							 GError		**error);
gboolean	 hal_gpower_enable_power_save		(HalGPower	*power,
							 gboolean	 enable);

G_END_DECLS

#endif	/* __LIBGPOWER_H */
