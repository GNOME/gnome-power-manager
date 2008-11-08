/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __DKP_OBJECT_H__
#define __DKP_OBJECT_H__

#include <glib.h>
#include "dkp-enum.h"

G_BEGIN_DECLS

typedef struct {
	guint64		 	 update_time;
	gchar			*vendor;
	gchar			*model;
	gchar			*serial;
	gchar			*native_path;
	gboolean		 power_supply;
	gboolean		 online;
	gboolean		 is_present;
	gboolean		 is_rechargeable;
	gboolean		 has_history;
	gboolean		 has_statistics;
	DkpDeviceType		 type;
	DkpDeviceState		 state;
	DkpDeviceTechnology	 technology;
	gdouble			 capacity;		/* percent */
	gdouble			 energy;		/* Watt Hours */
	gdouble			 energy_empty;		/* Watt Hours */
	gdouble			 energy_full;		/* Watt Hours */
	gdouble			 energy_full_design;	/* Watt Hours */
	gdouble			 energy_rate;		/* Watts */
	gdouble			 voltage;		/* Volts */
	gint64			 time_to_empty;		/* seconds */
	gint64			 time_to_full;		/* seconds */
	gdouble			 percentage;		/* percent */
} DkpObject;

DkpObject	*dkp_object_new			(void);
gboolean	 dkp_object_clear		(DkpObject		*obj);
gboolean	 dkp_object_free		(DkpObject		*obj);
gchar		*dkp_object_get_id		(DkpObject		*obj);
DkpObject	*dkp_object_copy		(const DkpObject	*cobj);
gboolean	 dkp_object_print		(const DkpObject	*obj);
gboolean	 dkp_object_diff		(const DkpObject	*old,
						 const DkpObject	*obj);
gboolean	 dkp_object_equal		(const DkpObject	*obj1,
						 const DkpObject	*obj2);
gboolean	 dkp_object_set_from_map	(DkpObject		*obj,
						 GHashTable		*hash_table);

G_END_DECLS

#endif /* __DKP_OBJECT_H__ */

