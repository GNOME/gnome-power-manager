/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libhal-gdevice.h>
#include <libhal-gmanager.h>

#include "gpm-marshal.h"
#include "gpm-ac-adapter.h"
#include "gpm-common.h"
#include "gpm-cell-array.h"
#include "gpm-cell-unit.h"
#include "gpm-cell.h"
#include "gpm-debug.h"

static void     gpm_cell_array_class_init (GpmCellArrayClass *klass);
static void     gpm_cell_array_init       (GpmCellArray      *cell_array);
static void     gpm_cell_array_finalize   (GObject	  *object);

#define GPM_CELL_ARRAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CELL_ARRAY, GpmCellArrayPrivate))

struct GpmCellArrayPrivate
{
	HalGManager	*hal_manager;
	GpmCellUnit	 unit;
	GpmAcAdapter	*ac_adapter;
	GPtrArray	*array;
	gboolean	 done_fully_charged;
	gboolean	 done_recall;
	gboolean	 done_capacity;
};

enum {
	PERCENT_CHANGED,
	FULLY_CHARGED,
	STATUS_CHANGED,
	PERHAPS_RECALL,
	LOW_CAPACITY,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmCellArray, gpm_cell_array, G_TYPE_OBJECT)

/**
 * gpm_cell_array_get_time_discharge:
 **/
guint 
gpm_cell_array_get_time_discharge (GpmCellArray *cell_array)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell_array != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL_ARRAY (cell_array), 0);

	unit = &(cell_array->priv->unit);

	return unit->time_discharge;
}

/**
 * gpm_cell_array_get_num_cells:
 **/
guint
gpm_cell_array_get_num_cells (GpmCellArray *cell_array)
{
	g_return_val_if_fail (cell_array != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL_ARRAY (cell_array), 0);

	return cell_array->priv->array->len;
}

/**
 * gpm_cell_get_icon:
 **/
gchar *
gpm_cell_array_get_icon (GpmCellArray *cell_array)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell_array != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL_ARRAY (cell_array), 0);

	unit = &(cell_array->priv->unit);
	return gpm_cell_unit_get_icon (unit);
}

/**
 * gpm_cell_get_cell:
 **/
GpmCell *
gpm_cell_array_get_cell (GpmCellArray *cell_array, guint id)
{
	GpmCell *cell;

	g_return_val_if_fail (cell_array != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL_ARRAY (cell_array), 0);

	if (id > cell_array->priv->array->len) {
		gpm_warning ("not valid cell id");
		return FALSE;
	}

	cell = (GpmCell *) g_ptr_array_index (cell_array->priv->array, id);
	return cell;
}

/**
 * gpm_cell_array_get_time_charge:
 **/
guint 
gpm_cell_array_get_time_charge (GpmCellArray *cell_array)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell_array != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL_ARRAY (cell_array), 0);

	unit = &(cell_array->priv->unit);

	return unit->time_charge;
}

/**
 * gpm_cell_perhaps_recall_cb:
 */
static void
gpm_cell_perhaps_recall_cb (GpmCell *cell, gchar *oem_vendor, gchar *website, GpmCellArray *cell_array)
{
	/* only emit this once per startup */
	if (cell_array->priv->done_recall == FALSE) {
		/* just proxy it to the GUI layer */
		g_signal_emit (cell_array, signals [PERHAPS_RECALL], 0, oem_vendor, website);
		cell_array->priv->done_recall = TRUE;
	}
}

/**
 * gpm_cell_low_capacity_cb:
 */
static void
gpm_cell_low_capacity_cb (GpmCell *cell, guint capacity, GpmCellArray *cell_array)
{
	/* only emit this once per startup */
	if (cell_array->priv->done_capacity == FALSE) {
		/* just proxy it to the GUI layer */
		g_signal_emit (cell_array, signals [LOW_CAPACITY], 0, capacity);
		cell_array->priv->done_capacity = TRUE;
	}
}

/**
 * gpm_cell_array_update:
 *
 * Updates the unit. This is
 * needed on multibattery laptops where the time needs to be computed over
 * two or more battereies. Some laptop batteries discharge one after the other,
 * some discharge simultanously.
 * This also does sanity checking on the values to make sure they are sane.
 */
static void
gpm_cell_array_update (GpmCellArray *cell_array)
{
	GpmCellUnit *unit;
	GpmCellUnit *unit_temp;
	GpmCell *cell;
	guint num_present = 0;
	guint num_discharging = 0;
	guint length;
	guint i;

	/* clear old values (except previous charge rate) */
	unit = &(cell_array->priv->unit);
	gpm_cell_unit_init (unit);

	length = cell_array->priv->array->len;
	/* iterate thru all the devices to handle multiple batteries */
	for (i=0;i<length;i++) {

		/* get the correct cell */
		cell = gpm_cell_array_get_cell (cell_array, i);
		unit_temp = gpm_cell_get_unit (cell);

		if (unit_temp->is_present == FALSE) {
			continue;
		}

		num_present++;

		/* Only one device has to be present for the class to
		 * be present. */
		unit->is_present = TRUE;

		if (unit_temp->is_charging == TRUE) {
			unit->is_charging = TRUE;
		}

		if (unit_temp->is_discharging == TRUE) {
			unit->is_discharging = TRUE;
			num_discharging++;
		}

		unit->charge_design += unit_temp->charge_design;
		unit->charge_last_full += unit_temp->charge_last_full;
		unit->charge_current += unit_temp->charge_current;
		unit->rate += unit_temp->rate;
		unit->voltage += unit_temp->voltage;
		/* we have to sum this here, in case the device has no rate
		   data, and we can't compute it further down */
		unit->time_charge += unit_temp->time_charge;
		unit->time_discharge += unit_temp->time_discharge;
	}

	/* average out the voltage for the global device */
	if (num_present > 1) {
		unit->voltage /= num_present;
	}

	/* sanity check */
	if (unit->is_discharging == TRUE && unit->is_charging == TRUE) {
		gpm_warning ("Sanity check kicked in! "
			     "Multiple device object cannot be charging and discharging simultaneously!");
		unit->is_charging = FALSE;
	}

	gpm_debug ("%i devices of type %s", num_present, gpm_cell_unit_get_kind_string (unit));

	/* Perform following calculations with floating point otherwise we might
	 * get an with batteries which have a very small charge unit and consequently
	 * a very high charge. Fixes bug #327471 */
	if (unit->is_present == TRUE) {
		gint pc = 100 * ((gfloat)unit->charge_current /
				(gfloat)unit->charge_last_full);
		if (pc < 0) {
			gpm_warning ("Corrected percentage charge (%i) and set to minimum", pc);
			pc = 0;
		} else if (pc > 100) {
			gpm_warning ("Corrected percentage charge (%i) and set to maximum", pc);
			pc = 100;
		}
		unit->percentage = pc;
	}

	/* If the primary battery is neither charging nor discharging, and
	 * the charge is low the battery is most likely broken.
	 * In this case, we'll use the ac_adaptor to determine whether it's
	 * charging or not. */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY &&
	    unit->is_charging == FALSE &&
	    unit->is_discharging == FALSE &&
	    unit->percentage > 0 &&
	    unit->percentage < GPM_CELL_UNIT_MIN_CHARGED_PERCENTAGE) {
		GpmAcAdapterState state;

		/* get the ac state */
		gpm_ac_adapter_get_state (cell_array->priv->ac_adapter, &state);
		gpm_debug ("Battery is neither charging nor discharging, "
			   "using ac_adaptor value %i", state);
		if (state == GPM_AC_ADAPTER_PRESENT) {
			unit->is_charging = TRUE;
			unit->is_discharging = FALSE;
		} else {
			unit->is_charging = FALSE;
			unit->is_discharging = TRUE;
		}
	}

	/* We only do the "better" remaining time algorithm if the battery has rate,
	   i.e not a UPS, which gives it's own battery.time_charge but has no rate */
	if (unit->rate > 0) {
		if (unit->is_discharging == TRUE) {
			unit->time_charge = 3600 * ((float)unit->charge_current /
							      (float)unit->rate);
		} else if (unit->is_charging == TRUE) {
			unit->time_charge = 3600 *
				((float)(unit->charge_last_full - unit->charge_current) /
				(float)unit->rate);
		}
	}

	/* Check the remaining time is under a set limit, to deal with broken
	   primary batteries. Fixes bug #328927 */
	if (unit->time_charge > (100 * 60 * 60)) {
		gpm_warning ("Another sanity check kicked in! "
			     "Remaining time cannot be > 100 hours!");
		unit->time_charge = 0;
	}
}

/**
 * gpm_cell_percent_changed_cb:
 */
static void
gpm_cell_percent_changed_cb (GpmCell *cell, guint percent, GpmCellArray *cell_array)
{
	GpmCellUnit *unit;
	guint old_percent;

	unit = &(cell_array->priv->unit);

	/* save the old percentage so we can compare it later */
	old_percent = unit->percentage;

	/* recalculate */
	gpm_cell_array_update (cell_array);

	/* proxy to engine if different */
	if (old_percent != unit->percentage) {
		g_signal_emit (cell_array, signals [PERCENT_CHANGED], 0, unit->percentage);

		/* only emit if all devices are fully charged */
		if (cell_array->priv->done_fully_charged == FALSE &&
		    gpm_cell_unit_is_charged (unit) == TRUE) {
			g_signal_emit (cell_array, signals [FULLY_CHARGED], 0);
			cell_array->priv->done_fully_charged = TRUE;
		}

		/* We only re-enable the fully charged notification when the battery
		   drops down to 95% as some batteries charge to 100% and then fluctuate
		   from ~98% to 100%. See #338281 for details */
		if (cell_array->priv->done_fully_charged == TRUE &&
		    unit->percentage < GPM_CELL_UNIT_MIN_CHARGED_PERCENTAGE &&
		    gpm_cell_unit_is_charged (unit) == FALSE) {
			cell_array->priv->done_fully_charged = FALSE;
		}

	}
}

/**
 * gpm_cell_status_changed_cb:
 */
static void
gpm_cell_status_changed_cb (GpmCell *cell, gboolean is_charging, gboolean is_discharging, GpmCellArray *cell_array)
{
	/* recalculate */
	gpm_cell_array_update (cell_array);

	/* proxy to engine */
	g_signal_emit (cell_array, signals [STATUS_CHANGED], 0, is_charging, is_discharging);
}

/**
 * gpm_cell_array_index_udi:
 *
 * Returns -1 if not found
 */
static gint
gpm_cell_array_index_udi (GpmCellArray *cell_array, const gchar *udi)
{
	gint i;
	guint length;
	GpmCell *cell;

	length = cell_array->priv->array->len;
	for (i=0;i<length;i++) {
		cell = (GpmCell *) g_ptr_array_index (cell_array->priv->array, i);
		if (strcmp (gpm_cell_get_udi (cell), udi) == 0) {
			gpm_debug ("Found %s with udi check", udi);
			return i;
		}
	}
	gpm_debug ("Did not find %s", udi);
	return -1;
}

/**
 * gpm_check_device_key:
 **/
static gboolean
gpm_check_device_key (GpmCellArray *cell_array, const gchar *udi, const gchar *key, const gchar *value)
{
	HalGDevice *device;
	gboolean ret;
	gboolean matches = FALSE;
	gchar *rettype;

	device = hal_gdevice_new ();
	ret = hal_gdevice_set_udi (device, udi);
	if (ret == FALSE) {
		gpm_warning ("failed to set UDI %s", udi);
		return FALSE;
	}

	/* check type */
	ret = hal_gdevice_get_string (device, key, &rettype, NULL);
	if (ret == FALSE || rettype == NULL) {
		gpm_warning ("failed to get %s", key);
		return FALSE;
	}
	gpm_debug ("checking %s against %s", rettype, value);
	if (strcmp (rettype, value) == 0) {
		matches = TRUE;
	}
	g_free (rettype);
	g_object_unref (device);
	return matches;
}

/**
 * gpm_cell_array_add:
 */
static gboolean
gpm_cell_array_add (GpmCellArray *cell_array, const gchar *udi)
{
	GpmCell *cell;
	GpmCellUnit *unit;
	gint index;
	const gchar *kind_string;
	gboolean ret;

	unit = &(cell_array->priv->unit);

	/* check type */
	kind_string = gpm_cell_unit_get_kind_string (unit);
	ret = gpm_check_device_key (cell_array, udi, "battery.type", kind_string);
	if (ret == FALSE) {
		gpm_debug ("not adding %s for %s", udi, kind_string);
		return FALSE;
	}

	/* is this UDI in our array? */
	index = gpm_cell_array_index_udi (cell_array, udi);
	if (index != -1) {
		/* already added */
		gpm_debug ("already added %s", udi);
		return FALSE;
	}

	gpm_debug ("adding the right kind of battery: %s", kind_string);

	cell = gpm_cell_new ();
	g_signal_connect (cell, "perhaps-recall",
			  G_CALLBACK (gpm_cell_perhaps_recall_cb), cell_array);
	g_signal_connect (cell, "low-capacity",
			  G_CALLBACK (gpm_cell_low_capacity_cb), cell_array);
	g_signal_connect (cell, "percent-changed",
			  G_CALLBACK (gpm_cell_percent_changed_cb), cell_array);
	g_signal_connect (cell, "status-changed",
			  G_CALLBACK (gpm_cell_status_changed_cb), cell_array);
	gpm_cell_set_type (cell, unit->kind, udi);

	g_ptr_array_add (cell_array->priv->array, (gpointer) cell);
	return TRUE;
}

/**
 * gpm_cell_array_set_type:
 **/
gboolean
gpm_cell_array_set_type (GpmCellArray *cell_array, GpmCellUnitKind kind)
{
	/* get all the hal devices of this type */
	guint i;
	gchar **device_names = NULL;
	GError *error;
	gboolean ret;
	GpmCellUnit *unit;

	g_return_val_if_fail (cell_array != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL_ARRAY (cell_array), 0);

	unit = &(cell_array->priv->unit);

	/* devices of type battery */
	error = NULL;
	ret = hal_gmanager_find_capability (cell_array->priv->hal_manager, "battery", &device_names, &error);
	if (ret == FALSE) {
		gpm_warning ("Couldn't obtain list of batteries: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	/* get the correct type */
	unit->kind = kind;

	/* Try to add all, the add will fail for batteries not of the correct type */
	for (i=0; device_names[i]; i++) {
		gpm_cell_array_add (cell_array, device_names[i]);
	}

	hal_gmanager_free_capability (device_names);

	/* recalculate */
	gpm_cell_array_update (cell_array);

	return TRUE;
}

/**
 * gpm_cell_array_get_description:
 **/
gchar *
gpm_cell_array_get_description (GpmCellArray *cell_array)
{
	const gchar *type_desc = NULL;
	gchar *charge_timestring;
	gchar *discharge_timestring;
	gchar *description = NULL;
	GpmCellUnit *unit;
	gboolean plural = FALSE;

	g_return_val_if_fail (cell_array != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL_ARRAY (cell_array), 0);

	unit = &(cell_array->priv->unit);

	if (unit->is_present == FALSE) {
		return NULL;
	}

	/* localized name */
	if (cell_array->priv->array->len > 1) {
		plural = TRUE;
	}
	type_desc = gpm_cell_unit_get_kind_localised (unit, plural);

	/* don't display all the extra stuff for keyboards and mice */
	if (unit->kind == GPM_CELL_UNIT_KIND_MOUSE ||
	    unit->kind == GPM_CELL_UNIT_KIND_KEYBOARD ||
	    unit->kind == GPM_CELL_UNIT_KIND_PDA) {
		return g_strdup_printf ("%s (%i%%)\n", type_desc, unit->percentage);
	}

	/* We always display "Laptop Battery 16 minutes remaining" as we need
	   to clarify what device we are refering to. For details see :
	   http://bugzilla.gnome.org/show_bug.cgi?id=329027 */
	if (gpm_cell_unit_is_charged (unit) == TRUE) {

		description = g_strdup_printf (_("%s fully charged (%i%%)"),
						type_desc, unit->percentage);

	} else if (unit->is_discharging == TRUE) {

		if (unit->time_discharge > 60) {
			discharge_timestring = gpm_get_timestring (unit->time_discharge);
			description = g_strdup_printf (_("%s %s remaining (%i%%)"),
						type_desc, discharge_timestring, unit->percentage);
			g_free (discharge_timestring);
		} else {
			/* don't display "Unknown remaining" */
			description = g_strdup_printf (_("%s discharging (%i%%)"),
						type_desc, unit->percentage);
		}

	} else if (unit->is_charging == TRUE) {

		if (unit->time_charge > 60 && unit->time_discharge > 60) {
			/* display both discharge and charge time */
			charge_timestring = gpm_get_timestring (unit->time_discharge);
			discharge_timestring = gpm_get_timestring (unit->time_discharge);
			description = g_strdup_printf (_("%s %s until charged (%i%%)\nProvides %s battery runtime"),
						type_desc, charge_timestring, unit->percentage, discharge_timestring);
			g_free (charge_timestring);
			g_free (discharge_timestring);
		} else if (unit->time_charge > 60) {
			/* display only charge time */
			charge_timestring = gpm_get_timestring (unit->time_discharge);
			description = g_strdup_printf (_("%s %s until charged (%i%%)"),
						type_desc, charge_timestring, unit->percentage);
			g_free (charge_timestring);
		} else {
			/* don't display "Unknown remaining" */
			description = g_strdup_printf (_("%s charging (%i%%)\n"),
						type_desc, unit->percentage);
		}

	} else {
		gpm_warning ("in an undefined state we are not charging or "
			     "discharging and the batteries are also not charged");
	}
	return description;
}

/**
 * hal_device_removed_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @cell_array: This cell_array instance
 */
static gboolean
hal_device_removed_cb (HalGManager  *hal_manager,
		       const gchar  *udi,
		       GpmCellArray *cell_array)
{
	gint index;
	GpmCell *cell;

	/* is this UDI in our array? */
	index = gpm_cell_array_index_udi (cell_array, udi);
	if (index == -1) {
		/* nope */
		return FALSE;
	}

	gpm_debug ("Removing udi=%s", udi);

	/* we unref as the device has gone away */
	cell = (GpmCell *) g_ptr_array_index (cell_array->priv->array, index);
	g_object_unref (cell);

	/* remove from the devicestore */
	g_ptr_array_remove_index (cell_array->priv->array, index);

	return TRUE;
}

/**
 * hal_new_capability_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @capability: the capability, e.g. "battery"
 * @cell_array: This cell_array instance
 */
static void
hal_new_capability_cb (HalGManager  *hal_manager,
		       const gchar  *udi,
		       const gchar  *capability,
		       GpmCellArray *cell_array)
{
	gpm_debug ("udi=%s, capability=%s", udi, capability);

	if (strcmp (capability, "battery") == 0) {
		gpm_cell_array_add (cell_array, udi);
	}
}

/**
 * hal_device_added_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @cell_array: This cell_array instance
 */
static void
hal_device_added_cb (HalGManager  *hal_manager,
		     const gchar  *udi,
		     GpmCellArray *cell_array)
{
	gboolean is_battery;
	gboolean dummy;
	HalGDevice *device;

	/* find out if the new device has capability battery
	   this might fail for CSR as the addon is weird */
	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, udi);
	hal_gdevice_query_capability (device, "battery", &is_battery, NULL);

	/* try harder */
	if (is_battery == FALSE) {
		is_battery = hal_gdevice_get_bool (device, "battery.present", &dummy, NULL);
	}

	/* if a battery, then add */
	if (is_battery == TRUE) {
		gpm_cell_array_add (cell_array, udi);
	}
	g_object_unref (device);
}

/**
 * gpm_cell_array_class_init:
 * @cell_array: This class instance
 **/
static void
gpm_cell_array_class_init (GpmCellArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_cell_array_finalize;
	g_type_class_add_private (klass, sizeof (GpmCellArrayPrivate));

	signals [PERCENT_CHANGED] =
		g_signal_new ("percent-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellArrayClass, percent_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [PERCENT_CHANGED] =
		g_signal_new ("status-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellArrayClass, status_changed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 2, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	signals [LOW_CAPACITY] =
		g_signal_new ("low-capacity",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellArrayClass, low_capacity),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [PERHAPS_RECALL] =
		g_signal_new ("perhaps-recall",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellArrayClass, perhaps_recall),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);
	signals [FULLY_CHARGED] =
		g_signal_new ("fully-charged",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellArrayClass, fully_charged),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * gpm_cell_array_init:
 * @cell_array: This class instance
 **/
static void
gpm_cell_array_init (GpmCellArray *cell_array)
{
	cell_array->priv = GPM_CELL_ARRAY_GET_PRIVATE (cell_array);

	cell_array->priv->array = g_ptr_array_new ();
	cell_array->priv->done_recall = FALSE;
	cell_array->priv->done_capacity = FALSE;
	cell_array->priv->done_fully_charged = FALSE;
	cell_array->priv->ac_adapter = gpm_ac_adapter_new ();
	cell_array->priv->hal_manager = hal_gmanager_new ();
	g_signal_connect (cell_array->priv->hal_manager, "device-added",
			  G_CALLBACK (hal_device_added_cb), cell_array);
	g_signal_connect (cell_array->priv->hal_manager, "device-removed",
			  G_CALLBACK (hal_device_removed_cb), cell_array);
	g_signal_connect (cell_array->priv->hal_manager, "new-capability",
			  G_CALLBACK (hal_new_capability_cb), cell_array);

	gpm_cell_unit_init (&cell_array->priv->unit);
}

/**
 * gpm_cell_array_finalize:
 * @object: This class instance
 **/
static void
gpm_cell_array_finalize (GObject *object)
{
	GpmCellArray *cell_array;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CELL_ARRAY (object));

	cell_array = GPM_CELL_ARRAY (object);
	cell_array->priv = GPM_CELL_ARRAY_GET_PRIVATE (cell_array);

	g_ptr_array_free (cell_array->priv->array, TRUE);
	g_object_unref (cell_array->priv->ac_adapter);
	g_object_unref (cell_array->priv->hal_manager);
}

/**
 * gpm_cell_array_new:
 * Return value: new class instance.
 **/
GpmCellArray *
gpm_cell_array_new (void)
{
	GpmCellArray *cell_array;
	cell_array = g_object_new (GPM_TYPE_CELL_ARRAY, NULL);
	return GPM_CELL_ARRAY (cell_array);
}

