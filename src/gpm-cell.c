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
#include <glib.h>
#include <glib/gi18n.h>

#include <libhal-gdevice.h>

#include "gpm-common.h"
#include "gpm-profile.h"
#include "gpm-marshal.h"
#include "gpm-cell.h"
#include "gpm-cell-unit.h"
#include "gpm-debug.h"

static void     gpm_cell_class_init (GpmCellClass *klass);
static void     gpm_cell_init       (GpmCell      *cell);
static void     gpm_cell_finalize   (GObject	  *object);

#define GPM_CELL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CELL, GpmCellPrivate))

struct GpmCellPrivate
{
	HalGDevice	*hal_device;
	GpmCellUnit	 unit;
	GpmProfile	*profile;
	gchar		*product;
	gchar		*vendor;
	gchar		*technology;
	gchar		*serial;
	gchar		*model;
};

enum {
	PERCENT_CHANGED,
	STATUS_CHANGED,
	PERHAPS_RECALL,
	LOW_CAPACITY,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmCell, gpm_cell, G_TYPE_OBJECT)

/**
 * gpm_cell_get_time_discharge:
 **/
GpmCellUnit *
gpm_cell_get_unit (GpmCell *cell)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL (cell), 0);

	unit = &(cell->priv->unit);

	return unit;
}

/**
 * gpm_cell_get_time_discharge:
 **/
guint 
gpm_cell_get_time_discharge (GpmCell *cell)
{
	GpmCellUnit *unit;
	guint time;

	g_return_val_if_fail (cell != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL (cell), 0);

	unit = &(cell->priv->unit);

	/* primary has special profiling class */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		time = gpm_profile_get_time (cell->priv->profile, unit->percentage, TRUE);
	} else {
		time = unit->time_discharge;
	}

	return time;
}

/**
 * gpm_cell_get_time_charge:
 **/
guint 
gpm_cell_get_time_charge (GpmCell *cell)
{
	GpmCellUnit *unit;
	guint time;

	g_return_val_if_fail (cell != NULL, 0);
	g_return_val_if_fail (GPM_IS_CELL (cell), 0);

	unit = &(cell->priv->unit);

	/* primary has special profiling class */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		time = gpm_profile_get_time (cell->priv->profile, unit->percentage, FALSE);
	} else {
		time = unit->time_charge;
	}

	return time;
}

/**
 * gpm_cell_set_type:
 **/
static gboolean
gpm_cell_refresh_all (GpmCell *cell)
{
	HalGDevice *device;
	GpmCellUnit *unit;
	gboolean exists;
	gboolean is_recalled;

	device = cell->priv->hal_device;
	unit = &(cell->priv->unit);

	/* batteries might be missing */
	hal_gdevice_get_bool (device, "battery.present", &unit->is_present, NULL);
	if (unit->is_present == FALSE) {
		gpm_debug ("Battery not present, so not filling up values");
		return FALSE;
	}

	hal_gdevice_get_uint (device, "battery.charge_level.design",
				 &unit->charge_design, NULL);
	hal_gdevice_get_uint (device, "battery.charge_level.last_full",
				 &unit->charge_last_full, NULL);
	hal_gdevice_get_uint (device, "battery.charge_level.current",
				 &unit->charge_current, NULL);

	/* battery might not be rechargeable, have to check */
	hal_gdevice_get_bool (device, "battery.is_rechargeable",
				&unit->is_rechargeable, NULL);

	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY ||
	    unit->kind == GPM_CELL_UNIT_KIND_UPS) {
		if (unit->is_rechargeable) {
			hal_gdevice_get_bool (device, "battery.rechargeable.is_charging",
						&unit->is_charging, NULL);
			hal_gdevice_get_bool (device, "battery.rechargeable.is_discharging",
						&unit->is_discharging, NULL);
		}
	}

	/* sanity check that charge_level.rate exists (if it should) */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		exists = hal_gdevice_get_uint (device, "battery.charge_level.rate",
						  &unit->rate, NULL);
		if (!exists && (unit->is_discharging || unit->is_charging)) {
			gpm_warning ("could not read your battery's charge rate");
		}
	}

	/* sanity check that charge_level.percentage exists (if it should) */
	exists = hal_gdevice_get_uint (device, "battery.charge_level.percentage",
					  &unit->percentage, NULL);
	if (!exists && (unit->is_discharging || unit->is_charging)) {
		gpm_warning ("could not read your battery's percentage charge.");
	}

	/* sanity check that remaining time exists (if it should) */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY ||
	    unit->kind == GPM_CELL_UNIT_KIND_UPS) {
		exists = hal_gdevice_get_uint (device,"battery.remaining_time",
						  &unit->time_charge, NULL);
		if (exists == FALSE && (unit->is_discharging == TRUE || unit->is_charging == TRUE)) {
			gpm_warning ("could not read your battery's remaining time");
		}
	}

	/* calculate the batteries capacity if it is primary and present */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY && unit->is_present) {
		if (unit->charge_design > 0 && unit->charge_last_full > 0) {
			if (unit->charge_design != unit->charge_last_full) {
				float capacity;
				capacity = 100.0f / (float) unit->charge_design;
				unit->capacity = capacity * (float) unit->charge_last_full;
				if (unit->capacity > 100) {
					gpm_debug ("rounding down capactity from "
						   "%i to 100", unit->capacity);
					unit->capacity = 100;
				}
				if (unit->capacity > 0 && unit->capacity < 50) {
					gpm_warning ("battery has a low capacity");
					g_signal_emit (cell, signals [LOW_CAPACITY], 0, unit->capacity);
				}
			}
		}
	}

	/* get other stuff we might need to know */
	hal_gdevice_get_string (device, "info.product", &cell->priv->product, NULL);
	hal_gdevice_get_string (device, "battery.vendor", &cell->priv->vendor, NULL);
	hal_gdevice_get_string (device, "battery.technology", &cell->priv->technology, NULL);
	hal_gdevice_get_string (device, "battery.serial", &cell->priv->serial, NULL);
	hal_gdevice_get_string (device, "battery.model", &cell->priv->model, NULL);
	hal_gdevice_get_uint (device, "battery.voltage.current", &unit->voltage, NULL);

	/* this is more common than you might expect: hardware that might blow up */
	hal_gdevice_get_bool (device, "info.is_recalled", &is_recalled, NULL);
	if (is_recalled == TRUE) {
		gchar *oem_vendor;
		gchar *website;
		hal_gdevice_get_string (device, "info.recall.vendor", &oem_vendor, NULL);
		hal_gdevice_get_string (device, "info.recall.website_url", &website, NULL);
		gpm_warning ("battery is recalled");
		g_signal_emit (cell, signals [PERHAPS_RECALL], 0, oem_vendor, website);
		g_free (oem_vendor);
		g_free (website);
	}
	gpm_cell_unit_set_measure (unit);
	return TRUE;
}

/**
 * battery_key_changed:
 **/
static gboolean
battery_key_changed (HalGDevice  *device,
		     const gchar *key,
		     const gchar *oldval)
{
	gchar *newval;
	gboolean ret = TRUE;

	if (oldval == NULL) {
		return FALSE;
	}

	/* get the new value */
	hal_gdevice_get_string (device, key, &newval, NULL);

	if (newval == NULL) {
		return FALSE;
	}
	if (strcmp (newval, oldval) == 0) {
		ret = FALSE;
	}
	g_free (newval);
	return ret;
}

/**
 * hal_device_property_modified_cb:
 */
static void
hal_device_property_modified_cb (HalGDevice   *device,
				 const gchar  *key,
				 gboolean      is_added,
				 gboolean      is_removed,
				 gboolean      finally,
				 GpmCell      *cell)
{
	GpmCellUnit *unit = &(cell->priv->unit);
	const gchar *udi = hal_gdevice_get_udi (device);
	gpm_debug ("udi=%s, key=%s, added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	/* do not process keys that have been removed */
	if (is_removed == TRUE) {
		return;
	}

	/* only match battery* values */
	if (strncmp (key, "battery", 7) != 0) {
		gpm_debug ("not battery key");
		return;
	}
	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		hal_gdevice_get_bool (device, key, &unit->is_present, NULL);

	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		hal_gdevice_get_bool (device, key, &unit->is_charging, NULL);

	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		hal_gdevice_get_bool (device, key, &unit->is_discharging, NULL);

	} else if (strcmp (key, "battery.charge_level.design") == 0) {
		hal_gdevice_get_uint (device, key, &unit->charge_design, NULL);

	} else if (strcmp (key, "battery.charge_level.last_full") == 0) {
		hal_gdevice_get_uint (device, key, &unit->charge_last_full, NULL);

	} else if (strcmp (key, "battery.charge_level.current") == 0) {
		hal_gdevice_get_uint (device, key, &unit->charge_current, NULL);

	} else if (strcmp (key, "battery.charge_level.rate") == 0) {
		hal_gdevice_get_uint (device, key, &unit->rate, NULL);

	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		hal_gdevice_get_uint (device, key, &unit->percentage, NULL);

	} else if (strcmp (key, "battery.remaining_time") == 0) {
		hal_gdevice_get_uint (device, key, &unit->time_charge, NULL);

	} else if (strcmp (key, "battery.voltage.current") == 0) {
		hal_gdevice_get_uint (device, key, &unit->voltage, NULL);

	} else if (strcmp (key, "battery.model") == 0 ||
		   strcmp (key, "battery.serial") == 0 ||
		   strcmp (key, "battery.vendor") == 0 ||
		   strcmp (key, "info.product") == 0) {
		if (battery_key_changed (device, "info.product", cell->priv->product) ||
		    battery_key_changed (device, "battery.vendor", cell->priv->vendor) ||
		    battery_key_changed (device, "battery.serial", cell->priv->serial) ||
		    battery_key_changed (device, "battery.model", cell->priv->model)) {
		    	/* we have to refresh all, as it might be a different battery */
			gpm_cell_refresh_all (cell);
		}
	}
}

/**
 * gpm_cell_set_type:
 **/
gboolean
gpm_cell_set_type (GpmCell *cell, GpmCellUnitKind type, const gchar *udi)
{
	gboolean ret;
	HalGDevice *device;
	gchar *battery_kind_str;
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CELL (cell), FALSE);

	unit = &(cell->priv->unit);
	device = cell->priv->hal_device;

	ret = hal_gdevice_set_udi (device, udi);
	if (ret == FALSE) {
		gpm_warning ("cannot set udi");
		return FALSE;
	}
	g_signal_connect (device, "property-modified",
			  G_CALLBACK (hal_device_property_modified_cb), cell);

	hal_gdevice_get_string (device, "battery.type", &battery_kind_str, NULL);
	if (battery_kind_str == NULL) {
		gpm_warning ("cannot obtain battery type");
		return FALSE;
	}

	ret = gpm_cell_unit_set_kind (unit, battery_kind_str);
	if (ret == FALSE) {
		gpm_warning ("battery type %s unknown", battery_kind_str);
		g_free (battery_kind_str);
		return FALSE;
	}
	g_free (battery_kind_str);

	gpm_cell_refresh_all (cell);

	return TRUE;
}

/**
 * gpm_cell_get_icon:
 **/
gchar *
gpm_cell_get_icon (GpmCell *cell)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, NULL);
	g_return_val_if_fail (GPM_IS_CELL (cell), NULL);

	unit = &(cell->priv->unit);
	return gpm_cell_unit_get_icon (unit);
}

/**
 * gpm_cell_get_description:
 **/
gchar *
gpm_cell_get_description (GpmCell *cell)
{
	GString	*details;
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, NULL);
	g_return_val_if_fail (GPM_IS_CELL (cell), NULL);

	unit = &(cell->priv->unit);

	details = g_string_new ("");
	if (cell->priv->product) {
		g_string_append_printf (details, _("<b>Product:</b> %s\n"), cell->priv->product);
	}
	if (unit->is_present == FALSE) {
		g_string_append (details, _("<b>Status:</b> Missing\n"));
	} else if (gpm_cell_unit_is_charged (unit) == TRUE) {
		g_string_append (details, _("<b>Status:</b> Charged\n"));
	} else if (unit->is_charging) {
		g_string_append (details, _("<b>Status:</b> Charging\n"));
	} else if (unit->is_discharging) {
		g_string_append (details, _("<b>Status:</b> Discharging\n"));
	}
	if (unit->percentage >= 0) {
		g_string_append_printf (details, _("<b>Percentage charge:</b> %i%%\n"),
					unit->percentage);
	}
	if (cell->priv->vendor) {
		g_string_append_printf (details, "<b>%s</b> %s\n",
					_("Vendor:"), cell->priv->vendor);
	}
	if (cell->priv->technology) {
		const gchar *technology;
		if (strcmp (cell->priv->technology, "lithium-ion") == 0) {
			technology = _("Lithium ion");
		} else if (strcasecmp (cell->priv->technology, "lead-acid") == 0) {
			technology = _("Lead acid");
		} else if (strcasecmp (cell->priv->technology, "lithium-polymer") == 0) {
			technology = _("Lithium polymer");
		} else if (strcasecmp (cell->priv->technology, "nickel-metal-hydride") == 0) {
			technology = _("Nickel metal hydride");
		} else {
			gpm_warning ("Battery type %s not translated, please report!",
				     cell->priv->technology);
			technology = cell->priv->technology;
		}
		g_string_append_printf (details, "<b>%s</b> %s\n",
					_("Technology:"), technology);
	}
	if (cell->priv->serial) {
		g_string_append_printf (details, "<b>%s</b> %s\n",
					_("Serial number:"), cell->priv->serial);
	}
	if (cell->priv->model) {
		g_string_append_printf (details, "<b>%s</b> %s\n",
					_("Model:"), cell->priv->model);
	}
	if (unit->time_charge > 0) {
		gchar *time_str;
		time_str = gpm_get_timestring (unit->time_charge);
		g_string_append_printf (details, "<b>%s</b> %s\n",
					_("Charge time:"), time_str);
		g_free (time_str);
	}
	if (unit->time_discharge > 0) {
		gchar *time_str;
		time_str = gpm_get_timestring (unit->time_discharge);
		g_string_append_printf (details, "<b>%s</b> %s\n",
					_("Discharge time:"), time_str);
		g_free (time_str);
	}
	if (unit->capacity > 0) {
		const gchar *condition;
		if (unit->capacity > 99) {
			condition = _("Excellent");
		} else if (unit->capacity > 90) {
			condition = _("Good");
		} else if (unit->capacity > 70) {
			condition = _("Fair");
		} else {
			condition = _("Poor");
		}
		g_string_append_printf (details, "<b>%s</b> %i%% (%s)\n",
					_("Capacity:"),
					unit->capacity, condition);
	}
	if (unit->measure == GPM_CELL_UNIT_MWH) {
		if (unit->charge_current > 0) {
			g_string_append_printf (details, "<b>%s</b> %.1f Wh\n",
						_("Current charge:"),
						unit->charge_current / 1000.0f);
		}
		if (unit->charge_last_full > 0 &&
		    unit->charge_design != unit->charge_last_full) {
			g_string_append_printf (details, "<b>%s</b> %.1f Wh\n",
						_("Last full charge:"),
						unit->charge_last_full / 1000.0f);
		}
		if (unit->charge_design > 0) {
			g_string_append_printf (details, "<b>%s</b> %.1f Wh\n",
						_("Design charge:"),
						unit->charge_design / 1000.0f);
		}
		if (unit->rate > 0) {
			g_string_append_printf (details, "<b>%s</b> %.1f W\n",
						_("Charge rate:"),
						unit->rate / 1000.0f);
		}
	}
	if (unit->measure == GPM_CELL_UNIT_CSR) {
		if (unit->charge_current > 0) {
			g_string_append_printf (details, "<b>%s</b> %i/7\n",
						_("Current charge:"),
						unit->charge_current);
		}
		if (unit->charge_design > 0) {
			g_string_append_printf (details, "<b>%s</b> %i/7\n",
						_("Design charge:"),
						unit->charge_design);
		}
	}
	/* remove the last \n */
	g_string_truncate (details, details->len-1);

	return g_string_free (details, FALSE);
}


/**
 * gpm_cell_class_init:
 * @cell: This class instance
 **/
static void
gpm_cell_class_init (GpmCellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_cell_finalize;
	g_type_class_add_private (klass, sizeof (GpmCellPrivate));

	signals [PERCENT_CHANGED] =
		g_signal_new ("percentage-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellClass, percent_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [PERCENT_CHANGED] =
		g_signal_new ("status-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellClass, status_changed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 2, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	signals [LOW_CAPACITY] =
		g_signal_new ("low-capacity",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellClass, low_capacity),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [PERHAPS_RECALL] =
		g_signal_new ("perhaps-recall",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellClass, perhaps_recall),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);
}

/**
 * gpm_cell_init:
 * @cell: This class instance
 **/
static void
gpm_cell_init (GpmCell *cell)
{
	cell->priv = GPM_CELL_GET_PRIVATE (cell);

	cell->priv->hal_device = hal_gdevice_new ();
	cell->priv->profile = gpm_profile_new ();
	cell->priv->product = NULL;
	cell->priv->vendor = NULL;
	cell->priv->technology = NULL;
	cell->priv->serial = NULL;
	cell->priv->model = NULL;
	gpm_cell_unit_init (&cell->priv->unit);
}

/**
 * gpm_cell_finalize:
 * @object: This class instance
 **/
static void
gpm_cell_finalize (GObject *object)
{
	GpmCell *cell;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CELL (object));

	cell = GPM_CELL (object);
	cell->priv = GPM_CELL_GET_PRIVATE (cell);

	g_free (cell->priv->product);
	g_free (cell->priv->vendor);
	g_free (cell->priv->technology);
	g_free (cell->priv->serial);
	g_free (cell->priv->model);
	g_object_unref (cell->priv->profile);
	g_object_unref (cell->priv->hal_device);
}

/**
 * gpm_cell_new:
 * Return value: new class instance.
 **/
GpmCell *
gpm_cell_new (void)
{
	GpmCell *cell;
	cell = g_object_new (GPM_TYPE_CELL, NULL);
	return GPM_CELL (cell);
}

