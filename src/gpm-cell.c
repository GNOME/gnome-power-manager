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
#include "gpm-marshal.h"
#include "gpm-cell.h"
#include "gpm-cell-unit.h"
#include "egg-debug.h"
#include "gpm-phone.h"

static void     gpm_cell_class_init (GpmCellClass *klass);
static void     gpm_cell_init       (GpmCell      *cell);
static void     gpm_cell_finalize   (GObject	  *object);

#define GPM_CELL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CELL, GpmCellPrivate))

struct GpmCellPrivate
{
	HalGDevice	*hal_device;
	GpmCellUnit	 unit;
	GpmPhone	*phone;
	gchar		*product;
	gchar		*vendor;
	gchar		*technology;
	gchar		*serial;
	gchar		*model;
	gulong		 sig_device_refresh;
};

enum {
	PERCENT_CHANGED,
	CHARGING_CHANGED,
	DISCHARGING_CHANGED,
	PERHAPS_RECALL,
	LOW_CAPACITY,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

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
 * gpm_cell_refresh_hal_all:
 **/
static gboolean
gpm_cell_refresh_hal_all (GpmCell *cell)
{
	HalGDevice *device;
	GpmCellUnit *unit;
	gboolean exists;
	gboolean is_recalled;

	device = cell->priv->hal_device;
	unit = gpm_cell_get_unit (cell);

	/* batteries might be missing */
	hal_gdevice_get_bool (device, "battery.present", &unit->is_present, NULL);
	if (unit->is_present == FALSE) {
		egg_debug ("Battery not present, so not filling up values");
		return FALSE;
	}

	hal_gdevice_get_uint (device, "battery.charge_level.design",
				 &unit->charge_design, NULL);
	hal_gdevice_get_uint (device, "battery.charge_level.last_full",
				 &unit->charge_last_full, NULL);
	hal_gdevice_get_uint (device, "battery.charge_level.current",
				 &unit->charge_current, NULL);
	hal_gdevice_get_uint (device, "battery.reporting.design",
				 &unit->reporting_design, NULL);

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
		} else {
			/* HAL isn't being helpful here... */
			unit->is_discharging = TRUE;
			unit->is_charging = FALSE;
		}
	} else {
		/* devices cannot charge, well, at least not while being used */
		unit->is_discharging = TRUE;
		unit->is_charging = FALSE;
	}

	/* sanity check that charge_level.rate exists (if it should) */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		exists = hal_gdevice_get_uint (device, "battery.charge_level.rate",
						  &unit->rate, NULL);
		if (exists == FALSE && (unit->is_discharging || unit->is_charging == TRUE)) {
			egg_warning ("could not read your battery's charge rate");
		}
		/* sanity check to less than 100W */
		if (unit->rate > 100*1000) {
			egg_warning ("sanity checking rate from %i to 0", unit->rate);
			unit->rate = 0;
		}
		/* The ACPI spec only allows this for primary cells, but in the real
		   world we also see it for rechargeables. Sigh */
		if (unit->rate == 0) {
			guint raw_last_charge;
			hal_gdevice_get_uint (device, "battery.reporting.last_full",
					      &raw_last_charge, NULL);
			if (raw_last_charge == 100)
				unit->reports_percentage = TRUE;
		}
	}

	/* sanity check that charge_level.percentage exists (if it should) */
	guint percentage;
	exists = hal_gdevice_get_uint (device, "battery.charge_level.percentage", &percentage, NULL);
	unit->percentage = (gfloat) percentage;
	if (exists == FALSE) {
		egg_warning ("could not read your battery's percentage charge.");
	}

	/* sanity check that remaining time exists (if it should) */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY ||
	    unit->kind == GPM_CELL_UNIT_KIND_UPS) {
		exists = hal_gdevice_get_uint (device,"battery.remaining_time",
						  &unit->time_charge, NULL);
		if (exists == FALSE && (unit->is_discharging || unit->is_charging == TRUE)) {
			egg_warning ("could not read your battery's remaining time");
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
					egg_debug ("rounding down capacity from "
						   "%i to 100", unit->capacity);
					unit->capacity = 100;
				}
				if (unit->capacity < 0) {
					egg_debug ("rounding up capacity from "
						   "%i to 0", unit->capacity);
					unit->capacity = 0;
				}
				if (unit->capacity < 50 && !unit->reports_percentage) {
					egg_warning ("battery has a low capacity");
					egg_debug ("** EMIT: low-capacity");
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
	if (is_recalled) {
		gchar *oem_vendor;
		gchar *website;
		hal_gdevice_get_string (device, "info.recall.vendor", &oem_vendor, NULL);
		hal_gdevice_get_string (device, "info.recall.website_url", &website, NULL);
		egg_warning ("battery is recalled");
		egg_debug ("** EMIT: perhaps-recall");
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
	GpmCellUnit *unit;
	const gchar *udi = hal_gdevice_get_udi (device);
	guint time_hal;

	unit = gpm_cell_get_unit (cell);

	egg_debug ("udi=%s, key=%s, added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	/* only match battery* values */
	if (strncmp (key, "battery", 7) != 0) {
		egg_debug ("not battery key");
		return;
	}

	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		hal_gdevice_get_bool (device, key, &unit->is_present, NULL);
		gpm_cell_refresh_hal_all (cell);

	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		hal_gdevice_get_bool (device, key, &unit->is_charging, NULL);
		egg_debug ("** EMIT: charging-changed: %i", unit->is_charging);
		g_signal_emit (cell, signals [CHARGING_CHANGED], 0, unit->is_charging);
		/* reset the time, as we really can't guess this without profiling */
		if (unit->is_charging) {
			unit->time_discharge = 0;
		}

	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		hal_gdevice_get_bool (device, key, &unit->is_discharging, NULL);
		egg_debug ("** EMIT: discharging-changed: %i", unit->is_discharging);
		g_signal_emit (cell, signals [DISCHARGING_CHANGED], 0, unit->is_discharging);
		/* reset the time, as we really can't guess this without profiling */
		if (unit->is_discharging) {
			unit->time_charge = 0;
		}

	} else if (strcmp (key, "battery.charge_level.design") == 0) {
		hal_gdevice_get_uint (device, key, &unit->charge_design, NULL);

	} else if (strcmp (key, "battery.charge_level.last_full") == 0) {
		hal_gdevice_get_uint (device, key, &unit->charge_last_full, NULL);

	} else if (strcmp (key, "battery.charge_level.current") == 0) {
		hal_gdevice_get_uint (device, key, &unit->charge_current, NULL);

	} else if (strcmp (key, "battery.charge_level.rate") == 0) {
		hal_gdevice_get_uint (device, key, &unit->rate, NULL);
		/* sanity check to less than 100W */
		if (unit->rate > 100*1000) {
			egg_warning ("sanity checking rate from %i to 0", unit->rate);
			unit->rate = 0;
		}

	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		guint percent;
		hal_gdevice_get_uint (device, key, &percent, NULL);
		unit->percentage = (gfloat) percent;
		egg_debug ("** EMIT: percent-changed: %f", unit->percentage);
		g_signal_emit (cell, signals [PERCENT_CHANGED], 0, unit->percentage);

	} else if (strcmp (key, "battery.remaining_time") == 0) {
		hal_gdevice_get_uint (device, key, &time_hal, NULL);
		/* Gahh. We have to multiplex the time as HAL shares a key. */
		if (unit->is_charging) {
			unit->time_charge = time_hal;
		}
		if (unit->is_discharging) {
			unit->time_discharge = time_hal;
		}

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
			gpm_cell_refresh_hal_all (cell);
		}
	}
}

/**
 * gpm_cell_set_type:
 **/
gboolean
gpm_cell_set_type (GpmCell *cell, GpmCellUnitKind kind)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CELL (cell), FALSE);

	unit = gpm_cell_get_unit (cell);
	unit->kind = kind;
	return TRUE;
}

/**
 * gpm_cell_set_device_id:
 **/
gboolean
gpm_cell_set_device_id (GpmCell *cell, const gchar *udi)
{
	gboolean ret;
	HalGDevice *device;
	gchar *battery_kind_str;
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CELL (cell), FALSE);

	unit = gpm_cell_get_unit (cell);
	device = cell->priv->hal_device;

	ret = hal_gdevice_set_udi (device, udi);
	if (!ret) {
		egg_warning ("cannot set udi");
		return FALSE;
	}

	/* watch for changes */
	hal_gdevice_watch_property_modified (device);
	g_signal_connect (device, "property-modified",
			  G_CALLBACK (hal_device_property_modified_cb), cell);

	hal_gdevice_get_string (device, "battery.type", &battery_kind_str, NULL);
	if (battery_kind_str == NULL) {
		egg_warning ("cannot obtain battery type");
		return FALSE;
	}

	ret = gpm_cell_unit_set_kind (unit, battery_kind_str);
	if (!ret) {
		egg_warning ("battery type %s unknown", battery_kind_str);
		g_free (battery_kind_str);
		return FALSE;
	}
	g_free (battery_kind_str);

	gpm_cell_refresh_hal_all (cell);

	return TRUE;
}

/**
 * gpm_cell_set_phone_index:
 **/
gboolean
gpm_cell_set_phone_index (GpmCell *cell, guint index)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CELL (cell), FALSE);

	unit = gpm_cell_get_unit (cell);
	unit->kind = GPM_CELL_UNIT_KIND_PHONE;

	unit->is_discharging = TRUE;
	unit->is_present = TRUE;
	unit->percentage = (gfloat) gpm_phone_get_percentage (cell->priv->phone, 0);
	unit->is_charging = gpm_phone_get_on_ac (cell->priv->phone, 0);
	unit->is_discharging = !unit->is_charging;
	return TRUE;
}

/**
 * gpm_cell_get_device_id:
 **/
const gchar *
gpm_cell_get_device_id (GpmCell *cell)
{
	g_return_val_if_fail (cell != NULL, NULL);
	g_return_val_if_fail (GPM_IS_CELL (cell), NULL);

	return hal_gdevice_get_udi (cell->priv->hal_device);
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

	unit = gpm_cell_get_unit (cell);
	return gpm_cell_unit_get_icon (unit);
}

/**
 * gpm_cell_get_id:
 **/
gchar *
gpm_cell_get_id (GpmCell *cell)
{
	GString *string;
	gchar *id = NULL;

	g_return_val_if_fail (cell != NULL, NULL);
	g_return_val_if_fail (GPM_IS_CELL (cell), NULL);

	string = g_string_new ("");

	/* in an ideal world, model-capacity-serial */
	if (cell->priv->model != NULL && strlen (cell->priv->model) > 2) {
		g_string_append (string, cell->priv->model);
		g_string_append_c (string, '-');
	}
	if (cell->priv->unit.reporting_design > 0) {
		g_string_append_printf (string, "%i", cell->priv->unit.reporting_design);
		g_string_append_c (string, '-');
	}
	if (cell->priv->serial != NULL && strlen (cell->priv->serial) > 2) {
		g_string_append (string, cell->priv->serial);
		g_string_append_c (string, '-');
	}

	/* make sure we are sane */
	if (string->len == 0) {
		/* just use something generic */
		g_string_append (string, "generic_id");
	} else {
		/* remove trailing '-' */
		g_string_set_size (string, string->len - 1);
	}

	/* the id may have invalid chars that need to be replaced */
	id = g_string_free (string, FALSE);
	g_strdelimit (id, "\\\t\"' /", '_');

	return id;
}

/**
 * gpm_cell_print:
 **/
gboolean
gpm_cell_print (GpmCell *cell)
{
	GpmCellUnit *unit;

	g_return_val_if_fail (cell != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_CELL (cell), FALSE);

	unit = gpm_cell_get_unit (cell);
	gpm_cell_unit_print (unit);
	return TRUE;
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

	unit = gpm_cell_get_unit (cell);

	details = g_string_new ("");
	if (cell->priv->product) {
		g_string_append_printf (details, _("<b>Product:</b> %s\n"), cell->priv->product);
	}
	if (unit->is_present == FALSE) {
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Missing"));
	} else if (gpm_cell_unit_is_charged (unit)) {
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Charged"));
	} else if (unit->is_charging) {
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Charging"));
	} else if (unit->is_discharging) {
		g_string_append_printf (details, _("<b>Status:</b> %s\n"), _("Discharging"));
	}
	if (unit->percentage >= 0) {
		g_string_append_printf (details, _("<b>Percentage charge:</b> %.1f%%\n"), unit->percentage);
	}
	if (cell->priv->vendor) {
		g_string_append_printf (details, _("<b>Vendor:</b> %s\n"), cell->priv->vendor);
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
		} else if (strcasecmp (cell->priv->technology, "unknown") == 0) {
			/* Translators: Unknown is related to the Technology of the battery */
			technology = _("Unknown");
		} else {
			egg_warning ("Battery type %s not translated, please report!",
				     cell->priv->technology);
			technology = cell->priv->technology;
		}
		g_string_append_printf (details, _("<b>Technology:</b> %s\n"), technology);
	}
	if (cell->priv->serial) {
		g_string_append_printf (details, _("<b>Serial number:</b> %s\n"), cell->priv->serial);
	}
	if (cell->priv->model) {
		g_string_append_printf (details, _("<b>Model:</b> %s\n"), cell->priv->model);
	}
	if (unit->time_charge > 0) {
		gchar *time_str;
		time_str = gpm_get_timestring (unit->time_charge);
		g_string_append_printf (details, _("<b>Charge time:</b> %s\n"), time_str);
		g_free (time_str);
	}
	if (unit->time_discharge > 0) {
		gchar *time_str;
		time_str = gpm_get_timestring (unit->time_discharge);
		g_string_append_printf (details, _("<b>Discharge time:</b> %s\n"), time_str);
		g_free (time_str);
	}
	if (unit->capacity > 0) {
		const gchar *condition;
		if (unit->capacity > 99) {
			/* Translators: Excellent, Good, Fair and Poor are all related to battery Capacity */
			condition = _("Excellent");
		} else if (unit->capacity > 90) {
			condition = _("Good");
		} else if (unit->capacity > 70) {
			condition = _("Fair");
		} else {
			condition = _("Poor");
		}
		/* Translators: %i is a percentage and %s the condition (Excellent, Good, ...) */
		g_string_append_printf (details, _("<b>Capacity:</b> %i%% (%s)\n"),
					unit->capacity, condition);
	}
	if (unit->measure == GPM_CELL_UNIT_MWH) {
		if (unit->charge_current > 0) {
			g_string_append_printf (details, _("<b>Current charge:</b> %.1f Wh\n"),
						unit->charge_current / 1000.0f);
		}
		if (unit->charge_last_full > 0 &&
		    unit->charge_design != unit->charge_last_full) {
			g_string_append_printf (details, _("<b>Last full charge:</b> %.1f Wh\n"),
						unit->charge_last_full / 1000.0f);
		}
		if (unit->charge_design > 0) {
			g_string_append_printf (details, _("<b>Design charge:</b> %.1f Wh\n"),
						unit->charge_design / 1000.0f);
		}
		if (unit->rate > 0) {
			g_string_append_printf (details, _("<b>Charge rate:</b> %.1f W\n"),
						unit->rate / 1000.0f);
		}
	}
	if (unit->measure == GPM_CELL_UNIT_CSR) {
		if (unit->charge_current > 0) {
			g_string_append_printf (details, _("<b>Current charge:</b> %i/7\n"),
						unit->charge_current);
		}
		if (unit->charge_design > 0) {
			g_string_append_printf (details, _("<b>Design charge:</b> %i/7\n"),
						unit->charge_design);
		}
	}
	/* remove the last \n */
	g_string_truncate (details, details->len-1);

	return g_string_free (details, FALSE);
}

/**
 * phone_device_refresh_cb:
 **/
static void
phone_device_refresh_cb (GpmPhone     *phone,
		         guint         index,
		         GpmCell      *cell)
{
	GpmCellUnit *unit;
	gboolean is_charging;
	guint percentage;

	g_return_if_fail (cell != NULL);
	g_return_if_fail (GPM_IS_CELL (cell));

	unit = gpm_cell_get_unit (cell);

	/* ignore non-phones */
	if (unit->kind != GPM_CELL_UNIT_KIND_PHONE) {
		return;
	}

	percentage = gpm_phone_get_percentage (cell->priv->phone, 0);
	is_charging = gpm_phone_get_on_ac (cell->priv->phone, 0);

	if (unit->is_charging != is_charging) {
		unit->is_charging = is_charging;
		unit->is_discharging = !is_charging;
		egg_debug ("** EMIT: charging-changed: %i", is_charging);
		g_signal_emit (cell, signals [CHARGING_CHANGED], 0, is_charging);
	}

	if (percentage != unit->percentage) {
		unit->percentage = (gfloat) percentage;
		egg_debug ("** EMIT: percent-changed: %.1f", unit->percentage);
		g_signal_emit (cell, signals [PERCENT_CHANGED], 0, unit->percentage);
	}
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
		g_signal_new ("percent-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellClass, percent_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE, 1, G_TYPE_FLOAT);
	signals [DISCHARGING_CHANGED] =
		g_signal_new ("discharging-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellClass, discharging_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals [CHARGING_CHANGED] =
		g_signal_new ("charging-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmCellClass, charging_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
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
	cell->priv->product = NULL;
	cell->priv->vendor = NULL;
	cell->priv->technology = NULL;
	cell->priv->serial = NULL;
	cell->priv->model = NULL;

	gpm_cell_unit_init (&cell->priv->unit);

	cell->priv->phone = gpm_phone_new ();
	cell->priv->sig_device_refresh =
		g_signal_connect (cell->priv->phone, "device-refresh",
	 			  G_CALLBACK (phone_device_refresh_cb), cell);
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

	/* we need to do this in case the device goes away and comes back */
	g_signal_handler_disconnect (cell->priv->phone, cell->priv->sig_device_refresh);

	g_free (cell->priv->product);
	g_free (cell->priv->vendor);
	g_free (cell->priv->technology);
	g_free (cell->priv->serial);
	g_free (cell->priv->model);
	g_object_unref (cell->priv->phone);
	g_object_unref (cell->priv->hal_device);
	G_OBJECT_CLASS (gpm_cell_parent_class)->finalize (object);
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

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef GPM_BUILD_TESTS
#include "gpm-self-test.h"
#include <libhal-gmanager.h>

guint recall_count = 0;

static void
gpm_cell_perhaps_recall_cb (GpmCell *cell, gchar *oem_vendor, gchar *website, gpointer data)
{
	recall_count++;
}

static gchar *
gpm_cell_get_battery (void)
{
	gchar **value;
	gchar *udi = NULL;
	gboolean ret;
	HalGManager *manager;

	manager = hal_gmanager_new ();
	ret = hal_gmanager_find_capability (manager, "battery", &value, NULL);
	if (ret && value != NULL && value[0] != NULL) {
		udi = g_strdup (value[0]);
	}
	hal_gmanager_free_capability (value);
	g_object_unref (manager);

	return udi;
}


void
gpm_st_cell (GpmSelfTest *test)
{
	GpmCell *cell;
	GpmCellUnit *unit;
	gchar *desc;
	gboolean ret;
	gchar *udi;

	if (gpm_st_start (test, "GpmCell") == FALSE) {
		return;
	}

	/* get battery */
	udi = gpm_cell_get_battery ();
	if (udi == NULL) {
		gpm_st_failed (test, "did not find battery device");
		gpm_st_end (test);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null cell");
	cell = gpm_cell_new ();
	g_signal_connect (cell, "perhaps-recall",
			  G_CALLBACK (gpm_cell_perhaps_recall_cb), NULL);
	if (cell != NULL) {
		gpm_st_success (test, "got GpmCell");
	} else {
		gpm_st_failed (test, "could not get GpmCell");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null unit");
	unit = gpm_cell_get_unit (cell);
	if (unit != NULL) {
		gpm_st_success (test, "got GpmCellUnit");
	} else {
		gpm_st_failed (test, "could not get GpmCellUnit");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a null description for nonassigned cell");
	desc = gpm_cell_get_description (cell);
	if (desc != NULL) {
		gpm_st_success (test, "got description %s", desc);
		g_free (desc);
	} else {
		gpm_st_failed (test, "could not get description");
	}

	/************************************************************/
	gpm_st_title (test, "can we assign device");
	ret = gpm_cell_set_device_id (cell, udi);
	if (ret) {
		gpm_st_success (test, "set type okay");
	} else {
		gpm_st_failed (test, "could not set type");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we got a single recall notice");
	if (recall_count == 1) {
		gpm_st_success (test, "got recall");
	} else {
		gpm_st_failed (test, "did not get recall (install fdi?)");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a full description for set cell");
	desc = gpm_cell_get_description (cell);
	if (desc != NULL) {
		gpm_st_success (test, "got description %s", desc);
		g_free (desc);
	} else {
		gpm_st_failed (test, "could not get description");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a valid unit");
	unit = gpm_cell_get_unit (cell);
	if (unit->voltage > 1000 && unit->voltage < 20000) {
		gpm_st_success (test, "got correct voltage");
	} else {
		gpm_st_failed (test, "could not get correct voltage %i", unit->voltage);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a id");
	desc = gpm_cell_get_id (cell);
	if (desc != NULL) {
		gpm_st_success (test, "got valid id %s", desc);
	} else {
		gpm_st_failed (test, "could not get valid id");
	}

	g_free (udi);
	g_free (desc);
	g_object_unref (cell);

	gpm_st_end (test);
}

#endif

