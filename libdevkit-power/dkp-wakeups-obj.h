/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

#ifndef __DKP_WAKEUPS_OBJ_H__
#define __DKP_WAKEUPS_OBJ_H__

#include <glib.h>
#include "dkp-enum.h"

G_BEGIN_DECLS

typedef struct
{
	gboolean		 is_userspace;
	guint			 id;
	guint			 old;
	gfloat			 value;
	gchar			*cmdline;
	gchar			*details;
} DkpWakeupsObj;

DkpWakeupsObj	*dkp_wakeups_obj_new		(void);
void		 dkp_wakeups_obj_free		(DkpWakeupsObj		*obj);
DkpWakeupsObj	*dkp_wakeups_obj_copy		(const DkpWakeupsObj	*cobj);
gboolean	 dkp_wakeups_obj_print		(const DkpWakeupsObj	*obj);
gboolean	 dkp_wakeups_obj_equal		(const DkpWakeupsObj	*obj1,
						 const DkpWakeupsObj	*obj2);

G_END_DECLS

#endif /* __DKP_WAKEUPS_OBJ_H__ */

