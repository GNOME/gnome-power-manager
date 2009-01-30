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

#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include <stdlib.h>

#include "egg-debug.h"
#include "dkp-enum.h"
#include "dkp-wakeups-obj.h"

/**
 * dkp_wakeups_obj_clear_internal:
 **/
static void
dkp_wakeups_obj_clear_internal (DkpWakeupsObj *obj)
{
	obj->id = 0;
	obj->old = 0;
	obj->value = 0.0f;
	obj->is_userspace = FALSE;
	obj->cmdline = NULL;
	obj->details = NULL;
}

/**
 * dkp_wakeups_obj_copy:
 **/
DkpWakeupsObj *
dkp_wakeups_obj_copy (const DkpWakeupsObj *cobj)
{
	DkpWakeupsObj *obj;
	obj = g_new0 (DkpWakeupsObj, 1);
	obj->id = cobj->id;
	obj->value = cobj->value;
	obj->is_userspace = cobj->is_userspace;
	return obj;
}

/**
 * dkp_wakeups_obj_equal:
 **/
gboolean
dkp_wakeups_obj_equal (const DkpWakeupsObj *obj1, const DkpWakeupsObj *obj2)
{
	if (obj1->id == obj2->id)
		return TRUE;
	return FALSE;
}

/**
 * dkp_wakeups_obj_print:
 **/
gboolean
dkp_wakeups_obj_print (const DkpWakeupsObj *obj)
{
	g_print ("userspace:%i id:%i, interrupts:%.1f, cmdline:%s, details:%s\n", obj->is_userspace, obj->id, obj->value, obj->cmdline, obj->details);
	return TRUE;
}

/**
 * dkp_wakeups_obj_new:
 **/
DkpWakeupsObj *
dkp_wakeups_obj_new (void)
{
	DkpWakeupsObj *obj;
	obj = g_new0 (DkpWakeupsObj, 1);
	dkp_wakeups_obj_clear_internal (obj);
	return obj;
}

/**
 * dkp_wakeups_obj_free:
 **/
void
dkp_wakeups_obj_free (DkpWakeupsObj *obj)
{
	if (obj == NULL)
		return;
	g_free (obj->cmdline);
	g_free (obj->details);
	g_free (obj);
	return;
}

