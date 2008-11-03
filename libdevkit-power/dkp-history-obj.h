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

#ifndef __DKP_HISTORY_OBJ_H__
#define __DKP_HISTORY_OBJ_H__

#include <glib.h>
#include "dkp-enum.h"

G_BEGIN_DECLS

typedef struct
{
	guint			 time;
	gdouble			 value;
	DkpDeviceState		 state;
} DkpHistoryObj;

DkpHistoryObj	*dkp_history_obj_new		(void);
gboolean	 dkp_history_obj_clear		(DkpHistoryObj		*obj);
gboolean	 dkp_history_obj_free		(DkpHistoryObj		*obj);
DkpHistoryObj	*dkp_history_obj_copy		(const DkpHistoryObj	*cobj);
gboolean	 dkp_history_obj_print		(const DkpHistoryObj	*obj);
DkpHistoryObj	*dkp_history_obj_create		(gdouble		 value,
						 DkpDeviceState		 state);
gboolean	 dkp_history_obj_equal		(const DkpHistoryObj	*obj1,
						 const DkpHistoryObj	*obj2);
DkpHistoryObj	*dkp_history_obj_from_string	(const gchar		*text);
gchar		*dkp_history_obj_to_string	(const DkpHistoryObj	*obj);

G_END_DECLS

#endif /* __DKP_HISTORY_OBJ_H__ */

