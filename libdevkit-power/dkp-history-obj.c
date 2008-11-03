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

#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include <stdlib.h>

#include "egg-debug.h"
#include "dkp-enum.h"
#include "dkp-history-obj.h"

/**
 * dkp_history_obj_clear_internal:
 **/
static void
dkp_history_obj_clear_internal (DkpHistoryObj *obj)
{
	obj->time = 0;
	obj->value = 0.0f;
	obj->state = 0;
}

/**
 * dkp_history_obj_copy:
 **/
DkpHistoryObj *
dkp_history_obj_copy (const DkpHistoryObj *cobj)
{
	DkpHistoryObj *obj;
	obj = g_new0 (DkpHistoryObj, 1);
	obj->time = cobj->time;
	obj->value = cobj->value;
	obj->state = cobj->state;
	return obj;
}

/**
 * dkp_history_obj_equal:
 **/
gboolean
dkp_history_obj_equal (const DkpHistoryObj *obj1, const DkpHistoryObj *obj2)
{
	if (obj1->time == obj2->time &&
	    obj1->value == obj2->value &&
	    obj1->state == obj2->state)
		return TRUE;
	return FALSE;
}

/**
 * dkp_history_obj_print:
 **/
gboolean
dkp_history_obj_print (const DkpHistoryObj *obj)
{
	g_print ("%i\t%.3f\t%s", obj->time, obj->value, dkp_device_state_to_text (obj->state));
	return TRUE;
}

/**
 * dkp_history_obj_new:
 **/
DkpHistoryObj *
dkp_history_obj_new (void)
{
	DkpHistoryObj *obj;
	obj = g_new0 (DkpHistoryObj, 1);
	dkp_history_obj_clear_internal (obj);
	return obj;
}

/**
 * dkp_history_obj_clear:
 **/
gboolean
dkp_history_obj_clear (DkpHistoryObj *obj)
{
	if (obj == NULL)
		return FALSE;
	dkp_history_obj_free (obj);
	dkp_history_obj_clear_internal (obj);
	return TRUE;
}

/**
 * dkp_history_obj_free:
 **/
gboolean
dkp_history_obj_free (DkpHistoryObj *obj)
{
	if (obj == NULL)
		return FALSE;
	g_free (obj);
	return TRUE;
}

/**
 * dkp_history_obj_create:
 **/
DkpHistoryObj *
dkp_history_obj_create (gdouble value, DkpDeviceState state)
{
	DkpHistoryObj *obj;
	GTimeVal timeval;

	g_get_current_time (&timeval);
	obj = dkp_history_obj_new ();
	obj->time = timeval.tv_sec;
	obj->value = value;
	obj->state = state;
	return obj;
}

/**
 * dkp_history_obj_from_string:
 **/
DkpHistoryObj *
dkp_history_obj_from_string (const gchar *text)
{
	DkpHistoryObj *obj = NULL;
	gchar **parts = NULL;
	guint length;

	if (text == NULL)
		goto out;

	/* split by tab */
	parts = g_strsplit (text, "\t", 0);
	length = g_strv_length (parts);
	if (length != 3) {
		egg_warning ("invalid string: '%s'", text);
		goto out;
	}

	/* parse and create */
	obj = dkp_history_obj_new ();
	obj->time = atoi (parts[0]);
	obj->value = atof (parts[1]);
	obj->state = dkp_device_state_from_text (parts[2]);
out:
	g_strfreev (parts);
	return obj;
}

/**
 * dkp_history_obj_to_string:
 **/
gchar *
dkp_history_obj_to_string (const DkpHistoryObj *obj)
{
	if (obj == NULL)
		return NULL;
	return g_strdup_printf ("%i\t%.3f\t%s", obj->time, obj->value, dkp_device_state_to_text (obj->state));
}

