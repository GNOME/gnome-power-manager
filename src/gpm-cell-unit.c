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

#include "gpm-cell-unit.h"
#include "egg-debug.h"

/**
 * gpm_unit_init:
 **/
gboolean
gpm_cell_unit_init (GpmCellUnit *unit)
{
	g_return_val_if_fail (unit != NULL, FALSE);

	unit->charge_design = 0;
	unit->charge_last_full = 0;
	unit->charge_current = 0;
	unit->rate = 0;
	unit->percentage = 0.0f;
	unit->time_charge = 0;
	unit->time_discharge = 0;
	unit->capacity = 0;
	unit->voltage = 0;

	unit->is_rechargeable = FALSE;
	unit->is_present = FALSE;
	unit->is_charging = FALSE;
	unit->is_discharging = FALSE;
	unit->reports_percentage = FALSE;

	return TRUE;
}

/**
 * gpm_unit_print:
 **/
gboolean
gpm_cell_unit_print (GpmCellUnit *unit)
{
	g_return_val_if_fail (unit != NULL, FALSE);

	egg_debug ("device         %s", gpm_cell_unit_get_kind_localised (unit, FALSE));
	egg_debug ("present        %i", unit->is_present);
	egg_debug ("percent        %.1f", unit->percentage);
	egg_debug ("is charging    %i", unit->is_charging);
	egg_debug ("is discharging %i", unit->is_discharging);
	if (unit->charge_current > 0) {
		egg_debug ("charge current %i", unit->charge_current);
	}
	if (unit->charge_last_full > 0) {
		egg_debug ("charge last    %i", unit->charge_last_full);
	}
	if (unit->charge_design > 0) {
		egg_debug ("charge design  %i", unit->charge_design);
	}
	if (unit->rate > 0) {
		egg_debug ("rate           %i", unit->rate);
	}
	if (unit->time_charge > 0) {
		egg_debug ("time charge    %i", unit->time_charge);
	}
	if (unit->time_discharge > 0) {
		egg_debug ("time discharge %i", unit->time_discharge);
	}
	if (unit->capacity > 0) {
		egg_debug ("capacity       %i", unit->capacity);
	}
	if (unit->voltage > 0) {
		egg_debug ("voltage        %i", unit->voltage);
	}
	return TRUE;
}

/**
 * gpm_cell_unit_get_icon_index:
 * @percent: The charge of the device
 *
 * The index value depends on the percentage charge:
 *	00-10  = 000
 *	10-30  = 020
 *	30-50  = 040
 *	50-70  = 060
 *	70-90  = 080
 *	90-100 = 100
 *
 * Return value: The character string for the filename suffix.
 **/
static const gchar *
gpm_cell_unit_get_icon_index (GpmCellUnit *unit)
{
	if (unit->percentage < 10) {
		return "000";
	} else if (unit->percentage < 30) {
		return "020";
	} else if (unit->percentage < 50) {
		return "040";
	} else if (unit->percentage < 70) {
		return "060";
	} else if (unit->percentage < 90) {
		return "080";
	}
	return "100";
}

/**
 * gpm_cell_unit_is_charged:
 *
 * We have to be clever here, as there are lots of broken batteries.
 * Some batteries have a reduced charge capacity and cannot charge to
 * 100% anymore (although they *should* update thier own last_full
 * values, it appears most do not...)
 * Return value: If the battery is considered "charged".
 **/
gboolean
gpm_cell_unit_is_charged (GpmCellUnit *unit)
{
	g_return_val_if_fail (unit != NULL, FALSE);

	if (unit->is_charging == FALSE &&
	    unit->is_discharging == FALSE &&
	    unit->percentage > GPM_CELL_UNIT_MIN_CHARGED_PERCENTAGE) {
		return TRUE;
	}
	return FALSE;
}

/**
 * gpm_cell_get_icon:
 *
 * Need to g_free the return value
 *
 **/
gchar *
gpm_cell_unit_get_icon (GpmCellUnit *unit)
{
	gchar *filename = NULL;
	const gchar *prefix = NULL;
	const gchar *index_str;

	g_return_val_if_fail (unit != NULL, NULL);

	/* get correct icon prefix */
	prefix = gpm_cell_unit_get_kind_string (unit);

	/* get the icon from some simple rules */
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY ||
	    unit->kind == GPM_CELL_UNIT_KIND_UPS) {
		if (unit->is_present == FALSE) {
			/* battery missing */
			filename = g_strdup_printf ("gpm-%s-missing", prefix);

		} else if (gpm_cell_unit_is_charged (unit)) {
			filename = g_strdup_printf ("gpm-%s-charged", prefix);

		} else if (unit->is_charging) {
			index_str = gpm_cell_unit_get_icon_index (unit);
			filename = g_strdup_printf ("gpm-%s-%s-charging", prefix, index_str);

		} else {
			index_str = gpm_cell_unit_get_icon_index (unit);
			filename = g_strdup_printf ("gpm-%s-%s", prefix, index_str);
		}
	} else if (unit->kind == GPM_CELL_UNIT_KIND_MOUSE ||
		   unit->kind == GPM_CELL_UNIT_KIND_KEYBOARD) {
		if (unit->charge_current < 2) {
			index_str = "000";
		} else if (unit->charge_current < 4) {
			index_str = "030";
		} else if (unit->charge_current < 6) {
			index_str = "060";
		} else {
			index_str = "100";
		}
		filename = g_strdup_printf ("gpm-%s-%s", prefix, index_str);
	} else if (unit->kind == GPM_CELL_UNIT_KIND_PHONE) {
		if (unit->percentage < 26) {
			index_str = "000";
		} else if (unit->percentage < 51) {
			index_str = "030";
		} else if (unit->percentage < 76) {
			index_str = "060";
		} else {
			index_str = "100";
		}
		filename = g_strdup_printf ("gpm-%s-%s", prefix, index_str);
	}
	egg_debug ("got filename: %s", filename);
	return filename;
}

/**
 * gpm_cell_unit_set_measure:
 **/
gboolean
gpm_cell_unit_set_measure (GpmCellUnit *unit)
{
	g_return_val_if_fail (unit != NULL, FALSE);

	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		/* true as not reporting, but charge_level */
		unit->measure = GPM_CELL_UNIT_MWH;
	} else if (unit->kind == GPM_CELL_UNIT_KIND_UPS) {
		/* is this always correct? */
		unit->measure = GPM_CELL_UNIT_PERCENT;
	} else if (unit->kind == GPM_CELL_UNIT_KIND_MOUSE ||
		   unit->kind == GPM_CELL_UNIT_KIND_KEYBOARD) {
		unit->measure = GPM_CELL_UNIT_CSR;
	}
	return TRUE;
}

/**
 * gpm_cell_unit_get_localised_kind:
 **/
const gchar *
gpm_cell_unit_get_kind_localised (GpmCellUnit *unit, gboolean plural)
{
	const gchar *str = NULL;

	g_return_val_if_fail (unit != NULL, NULL);

	if (plural) {
		if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
	 		str = _("Laptop batteries");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_UPS) {
	 		str = _("UPSs");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_MOUSE) {
	 		str = _("Wireless mice");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_KEYBOARD) {
	 		str = _("Wireless keyboards");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_PDA) {
	 		str = _("PDAs");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_PHONE) {
	 		str = _("Cell phones");
	 	} else {
	 		str = _("Unknown");
		}
	} else {
		if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
	 		str = _("Laptop battery");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_UPS) {
	 		str = _("UPS");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_MOUSE) {
	 		str = _("Wireless mouse");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_KEYBOARD) {
	 		str = _("Wireless keyboard");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_PDA) {
	 		str = _("PDA");
		} else if (unit->kind == GPM_CELL_UNIT_KIND_PHONE) {
	 		str = _("Cell phone");
	 	} else {
	 		str = _("Unknown");
		}
	}
	return str;
}

/**
 * gpm_cell_unit_get_kind_string:
 **/
const gchar *
gpm_cell_unit_get_kind_string (GpmCellUnit *unit)
{
	const gchar *str;

	g_return_val_if_fail (unit != NULL, NULL);

	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
 		str = "primary";
	} else if (unit->kind == GPM_CELL_UNIT_KIND_UPS) {
 		str = "ups";
	} else if (unit->kind == GPM_CELL_UNIT_KIND_MOUSE) {
 		str = "mouse";
	} else if (unit->kind == GPM_CELL_UNIT_KIND_KEYBOARD) {
 		str = "keyboard";
	} else if (unit->kind == GPM_CELL_UNIT_KIND_PDA) {
 		str = "pda";
	} else if (unit->kind == GPM_CELL_UNIT_KIND_PHONE) {
 		str = "phone";
 	} else {
 		str = "unknown";
	}
	return str;
}

/**
 * gpm_cell_unit_set_kind:
 **/
gboolean
gpm_cell_unit_set_kind (GpmCellUnit *unit, const gchar *kind)
{
	g_return_val_if_fail (unit != NULL, FALSE);

	if (strcmp (kind, "primary") == 0) {
		unit->kind = GPM_CELL_UNIT_KIND_PRIMARY;
		return TRUE;
	} else if (strcmp (kind, "ups") == 0) {
		unit->kind = GPM_CELL_UNIT_KIND_UPS;
		return TRUE;
	} else if (strcmp (kind, "keyboard") == 0) {
		unit->kind = GPM_CELL_UNIT_KIND_KEYBOARD;
		return TRUE;
	} else if (strcmp (kind, "mouse") == 0) {
		unit->kind = GPM_CELL_UNIT_KIND_MOUSE;
		return TRUE;
	} else if (strcmp (kind, "pda") == 0) {
		unit->kind = GPM_CELL_UNIT_KIND_PDA;
		return TRUE;
	} else if (strcmp (kind, "phone") == 0) {
		unit->kind = GPM_CELL_UNIT_KIND_PHONE;
		return TRUE;
	}

	egg_warning ("battery type %s unknown", kind);
	return FALSE;
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef GPM_BUILD_TESTS
#include "gpm-self-test.h"

void
gpm_st_cell_unit (GpmSelfTest *test)
{
	GpmCellUnit unit_d;
	GpmCellUnit *unit = &unit_d;
	gchar *value;
	const gchar *cvalue;
	gboolean ret;

	if (gpm_st_start (test, "GpmCellUnit") == FALSE) {
		return;
	}

	gpm_cell_unit_init (unit);

	/************************************************************/
	gpm_st_title (test, "make sure hal type set correct");
	gpm_cell_unit_set_kind (unit, "primary");
	if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		gpm_st_success (test, "type correct");
	} else {
		gpm_st_failed (test, "type incorrect");
	}

	/************************************************************/
	gpm_st_title (test, "make sure full battery isn't charged");
	unit->percentage = 100.0f;
	unit->is_charging = FALSE;
	unit->is_discharging = TRUE;
	ret = gpm_cell_unit_is_charged (unit);
	if (!ret) {
		gpm_st_success (test, "not charged");
	} else {
		gpm_st_failed (test, "declaring charged");
	}

	/************************************************************/
	gpm_st_title (test, "make sure string type is okay");
	cvalue = gpm_cell_unit_get_kind_string (unit);
	if (strcmp (cvalue, "primary") == 0) {
		gpm_st_success (test, "string type okay");
	} else {
		gpm_st_failed (test, "string type not okay: %s", cvalue);
	}

	/************************************************************/
	gpm_st_title (test, "make sure single localised type is okay");
	cvalue = gpm_cell_unit_get_kind_localised (unit, FALSE);
	if (strcmp (cvalue, "Laptop battery") == 0) {
		gpm_st_success (test, "localised type okay");
	} else {
		gpm_st_failed (test, "localised type not okay: %s", cvalue);
	}

	/************************************************************/
	gpm_st_title (test, "make sure plural localised type is okay");
	cvalue = gpm_cell_unit_get_kind_localised (unit, TRUE);
	if (strcmp (cvalue, "Laptop batteries") == 0) {
		gpm_st_success (test, "localised type okay");
	} else {
		gpm_st_failed (test, "localised type not okay: %s", cvalue);
	}

	/************************************************************/
	gpm_st_title (test, "make sure charging battery isn't charged");
	unit->percentage = 99.0f;
	unit->is_charging = TRUE;
	unit->is_discharging = FALSE;
	ret = gpm_cell_unit_is_charged (unit);
	if (!ret) {
		gpm_st_success (test, "not charged");
	} else {
		gpm_st_failed (test, "declaring charged");
	}

	/************************************************************/
	gpm_st_title (test, "make sure full battery is charged");
	unit->percentage = 95.0f;
	unit->is_charging = FALSE;
	unit->is_discharging = FALSE;
	ret = gpm_cell_unit_is_charged (unit);
	if (ret) {
		gpm_st_success (test, "charged");
	} else {
		gpm_st_failed (test, "declaring non-charged");
	}

	/************************************************************/
	gpm_st_title (test, "make sure broken battery isn't charged");
	unit->percentage = 30.0f;
	unit->is_charging = FALSE;
	unit->is_discharging = FALSE;
	ret = gpm_cell_unit_is_charged (unit);
	if (!ret) {
		gpm_st_success (test, "not charged");
	} else {
		gpm_st_failed (test, "declaring charged");
	}

	/************************************************************/
	gpm_st_title (test, "get missing icon");
	unit->percentage = 30.0f;
	unit->is_present = FALSE;
	value = gpm_cell_unit_get_icon (unit);
	if (strcmp (value, "gpm-primary-missing") == 0) {
		gpm_st_success (test, "icon correct");
	} else {
		gpm_st_failed (test, "icon not correct: %s", value);
	}
	g_free (value);

	/************************************************************/
	gpm_st_title (test, "get middle icon");
	unit->percentage = 30.0f;
	unit->is_present = TRUE;
	value = gpm_cell_unit_get_icon (unit);
	if (strcmp (value, "gpm-primary-040") == 0) {
		gpm_st_success (test, "icon correct");
	} else {
		gpm_st_failed (test, "icon not correct: %s", value);
	}
	g_free (value);

	/************************************************************/
	gpm_st_title (test, "get charged icon");
	unit->is_charging = FALSE;
	unit->is_discharging = FALSE;
	unit->percentage = 95.0f;
	unit->is_present = TRUE;
	value = gpm_cell_unit_get_icon (unit);
	if (strcmp (value, "gpm-primary-charged") == 0) {
		gpm_st_success (test, "icon correct");
	} else {
		gpm_st_failed (test, "icon not correct: %s", value);
	}
	g_free (value);
	
	/************************************************************/
	gpm_st_title (test, "setting measure");
	gpm_cell_unit_set_measure (unit);
	if (unit->measure == GPM_CELL_UNIT_MWH) {
		gpm_st_success (test, "measure correct");
	} else {
		gpm_st_failed (test, "measre not correct: %s", unit->measure);
	}

	gpm_st_end (test);
}
#endif

