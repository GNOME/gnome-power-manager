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
#include <libhal-gmanager.h>

#include "gpm-ac-adapter.h"
#include "gpm-common.h"
#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-profile.h"
#include "gpm-marshal.h"
#include "gpm-engine.h"
#include "gpm-cell-unit.h"
#include "gpm-cell-array.h"
#include "gpm-cell.h"
#include "egg-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-warnings.h"
#include "gpm-prefs-server.h"

static void     gpm_engine_class_init (GpmEngineClass *klass);
static void     gpm_engine_init       (GpmEngine      *engine);
static void     gpm_engine_finalize   (GObject	  *object);

#define GPM_ENGINE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_ENGINE, GpmEnginePrivate))
#define GPM_ENGINE_RESUME_DELAY		2*1000
#define GPM_ENGINE_WARN_ACCURACY	20


struct GpmEnginePrivate
{
	GpmConf			*conf;
	GpmWarnings		*warnings;
	GpmIconPolicy		 icon_policy;
	GpmProfile		*profile;
	GpmControl		*control;
	GpmAcAdapter		*ac_adapter;
	GpmEngineCollection	 collection;
	HalGManager		*hal_manager;
	gboolean		 hal_connected;
	gboolean		 during_coldplug;
	gchar			*previous_icon;
	gchar			*previous_summary;
};

enum {
	ICON_CHANGED,
	SUMMARY_CHANGED,
	FULLY_CHARGED,
	CHARGE_LOW,
	CHARGE_CRITICAL,
	CHARGE_ACTION,
	DISCHARGING,
	LOW_CAPACITY,
	PERHAPS_RECALL,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmEngine, gpm_engine, G_TYPE_OBJECT)

/**
 * ac_adaptor_changed_cb:
 **/
static void
ac_adaptor_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean      on_ac,
		       GpmEngine    *engine)
{
	GpmEngineCollection *collection;

	/* grab a reference to the collection */
	collection = &engine->priv->collection;

	/* refresh, as we may be hitting the
	 * "battery not identifies itself as charging rule" */
	gpm_cell_array_refresh (collection->primary);
}

/**
 * gpm_engine_delayed_refresh:
 */
static gboolean
gpm_engine_delayed_refresh (GpmEngine *engine)
{
	GpmEngineCollection *collection;

	/* grab a reference to the collection */
	collection = &engine->priv->collection;

	gpm_cell_array_refresh (collection->primary);
	gpm_cell_array_refresh (collection->ups);
	gpm_cell_array_refresh (collection->mouse);
	gpm_cell_array_refresh (collection->keyboard);
	gpm_cell_array_refresh (collection->pda);
	gpm_cell_array_refresh (collection->phone);
	return FALSE;
}

/**
 * control_resume_cb:
 * @control: The control class instance
 * @engine: This engine class instance
 *
 * We have to update the caches on resume
 **/
static void
control_resume_cb (GpmControl *control,
		   GpmControlAction action,
		   GpmEngine   *engine)
{
	/* we have to delay this at resume to counteract races */
	g_timeout_add (GPM_ENGINE_RESUME_DELAY, (GSourceFunc) gpm_engine_delayed_refresh, engine);
}

/**
 * gpm_engine_class_init:
 * @engine: This class instance
 **/
static void
gpm_engine_class_init (GpmEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_engine_finalize;
	g_type_class_add_private (klass, sizeof (GpmEnginePrivate));

	signals [ICON_CHANGED] =
		g_signal_new ("icon-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, icon_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [SUMMARY_CHANGED] =
		g_signal_new ("summary-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, summary_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [LOW_CAPACITY] =
		g_signal_new ("low-capacity",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, low_capacity),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__UINT_UINT,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
	signals [PERHAPS_RECALL] =
		g_signal_new ("perhaps-recall",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, perhaps_recall),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__UINT_STRING_STRING,
			      G_TYPE_NONE,
			      3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);
	signals [FULLY_CHARGED] =
		g_signal_new ("fully-charged",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, fully_charged),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [DISCHARGING] =
		g_signal_new ("discharging",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, discharging),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [CHARGE_ACTION] =
		g_signal_new ("charge-action",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_action),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__UINT_UINT,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
	signals [CHARGE_LOW] =
		g_signal_new ("charge-low",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_low),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__UINT_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
	signals [CHARGE_CRITICAL] =
		g_signal_new ("charge-critical",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_critical),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__UINT_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
}

/**
 * gpm_engine_get_summary:
 * @engine: This engine class instance
 * @string: The returned string
 *
 * Returns the complete tooltip ready for display. Text logic is done here :-).
 **/
GpmEngineCollection *
gpm_engine_get_collection (GpmEngine *engine)
{
	g_return_val_if_fail (engine != NULL, NULL);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), NULL);

	return &engine->priv->collection;
}

/**
 * gpm_engine_get_summary:
 * @engine: This engine class instance
 * @string: The returned string
 *
 * Returns the complete tooltip ready for display. Text logic is done here :-).
 **/
gchar *
gpm_engine_get_summary (GpmEngine *engine)
{
	GString *tooltip = NULL;
	GpmCellUnit *unit;
	GpmEngineCollection *collection;
	guint accuracy;
	gboolean on_ac;
	gchar *part;

	g_return_val_if_fail (engine != NULL, NULL);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), NULL);

	/* grab a reference to the collection */
	collection = &engine->priv->collection;

	/* get the ac state */
	on_ac = gpm_ac_adapter_is_present (engine->priv->ac_adapter);

	unit = gpm_cell_array_get_unit (collection->ups);

	if (unit->is_present && unit->is_discharging == TRUE) {
		/* only enable this if discharging on UPS  */
		tooltip = g_string_new (_("Computer is running on backup power\n"));

	} else if (on_ac) {
		tooltip = g_string_new (_("Computer is running on AC power\n"));

	} else {
		tooltip = g_string_new (_("Computer is running on battery power\n"));
	}

	/* do each device type we know about, in the correct visual order */
	part = gpm_cell_array_get_description (collection->primary);
	if (part != NULL) {
		tooltip = g_string_append (tooltip, part);
	}
	g_free (part);

	/* if we have limited accuracy, add this to the tooltip */
	unit = gpm_cell_array_get_unit (collection->primary);
	accuracy = gpm_profile_get_accuracy_average (engine->priv->profile,
						     unit->is_discharging);

	if (unit->is_present) {
		if (accuracy == 0) {
			if (unit->is_discharging) {
				tooltip = g_string_append (tooltip, _("Battery discharge time is currently unknown\n"));
			} else {
				tooltip = g_string_append (tooltip, _("Battery charge time is currently unknown\n"));
			}
		} else if (accuracy < GPM_PROFILE_GOOD_TRUST) {
			if (unit->is_discharging) {
				tooltip = g_string_append (tooltip, _("Battery discharge time is estimated\n"));
			} else {
				tooltip = g_string_append (tooltip, _("Battery charge time is estimated\n"));
			}
		}
	}

	part = gpm_cell_array_get_description (collection->ups);
	if (part != NULL) {
		tooltip = g_string_append (tooltip, part);
	}
	g_free (part);
	part = gpm_cell_array_get_description (collection->mouse);
	if (part != NULL) {
		tooltip = g_string_append (tooltip, part);
	}
	g_free (part);
	part = gpm_cell_array_get_description (collection->keyboard);
	if (part != NULL) {
		tooltip = g_string_append (tooltip, part);
	}
	g_free (part);
	part = gpm_cell_array_get_description (collection->phone);
	if (part != NULL) {
		tooltip = g_string_append (tooltip, part);
	}
	g_free (part);
	part = gpm_cell_array_get_description (collection->pda);
	if (part != NULL) {
		tooltip = g_string_append (tooltip, part);
	}
	g_free (part);

	/* remove the last \n */
	g_string_truncate (tooltip, tooltip->len-1);

	egg_debug ("tooltip: %s", tooltip->str);

	return g_string_free (tooltip, FALSE);
}

/**
 * gpm_engine_get_icon:
 *
 * Returns the icon. Maybe need to g_free.
 **/
gchar *
gpm_engine_get_icon (GpmEngine *engine)
{
	GpmCellUnit *unit_primary;
	GpmCellUnit *unit_ups;
	GpmCellUnit *unit_mouse;
	GpmCellUnit *unit_keyboard;
	GpmCellUnit *unit_pda;
	GpmCellUnit *unit_phone;
	GpmEngineCollection *collection;
	GpmWarningsState state;

	/* grab a reference to the collection */
	collection = &engine->priv->collection;

	g_return_val_if_fail (engine != NULL, NULL);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), NULL);

	if (engine->priv->icon_policy == GPM_ICON_POLICY_NEVER) {
		egg_debug ("no icon allowed, so no icon will be displayed.");
		return NULL;
	}

	/* gets units for eash lookup */
	unit_primary = gpm_cell_array_get_unit (collection->primary);
	unit_ups = gpm_cell_array_get_unit (collection->ups);
	unit_mouse = gpm_cell_array_get_unit (collection->mouse);
	unit_keyboard = gpm_cell_array_get_unit (collection->keyboard);
	unit_pda = gpm_cell_array_get_unit (collection->pda);
	unit_phone = gpm_cell_array_get_unit (collection->phone);

	/* we try CRITICAL: PRIMARY, UPS, MOUSE, KEYBOARD */
	egg_debug ("Trying CRITICAL icon: primary, ups, mouse, keyboard");
	if (unit_primary->is_present) {
		state = gpm_warnings_get_state (engine->priv->warnings, unit_primary);
		if (state == GPM_WARNINGS_CRITICAL) {
			return gpm_cell_array_get_icon (collection->primary);
		}
	}
	if (unit_ups->is_present) {
		state = gpm_warnings_get_state (engine->priv->warnings, unit_ups); 
		if (state == GPM_WARNINGS_CRITICAL) {
			return gpm_cell_array_get_icon (collection->ups);
		}
	}
	if (unit_keyboard->is_present) {
		state = gpm_warnings_get_state (engine->priv->warnings, unit_phone);
		if (state == GPM_WARNINGS_CRITICAL) {
			return gpm_cell_array_get_icon (collection->phone);
		}
	}
	if (unit_mouse->is_present) {
		state = gpm_warnings_get_state (engine->priv->warnings, unit_mouse);
		if (state == GPM_WARNINGS_CRITICAL) {
			return gpm_cell_array_get_icon (collection->mouse);
		}
	}
	if (unit_keyboard->is_present) {
		state = gpm_warnings_get_state (engine->priv->warnings, unit_keyboard);
		if (state == GPM_WARNINGS_CRITICAL) {
			return gpm_cell_array_get_icon (collection->keyboard);
		}
	}
	if (unit_keyboard->is_present) {
		state = gpm_warnings_get_state (engine->priv->warnings, unit_pda);
		if (state == GPM_WARNINGS_CRITICAL) {
			return gpm_cell_array_get_icon (collection->pda);
		}
	}

	if (engine->priv->icon_policy == GPM_ICON_POLICY_CRITICAL) {
		egg_debug ("no devices critical, so no icon will be displayed.");
		return NULL;
	}

	/* we try (DIS)CHARGING: PRIMARY, UPS */
	egg_debug ("Trying CHARGING icon: primary, ups");
	if (unit_primary->is_present &&
	    (unit_primary->is_charging || unit_primary->is_discharging == TRUE)) {
		return gpm_cell_array_get_icon (collection->primary);

	} else if (unit_ups->is_present &&
		   (unit_ups->is_charging || unit_ups->is_discharging == TRUE)) {
		return gpm_cell_array_get_icon (collection->ups);
	}

	/* Check if we should just show the icon all the time */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_CHARGE) {
		egg_debug ("no devices (dis)charging, so no icon will be displayed.");
		return NULL;
	}

	/* we try PRESENT: PRIMARY, UPS */
	egg_debug ("Trying PRESENT icon: primary, ups");
	if (unit_primary->is_present) {
		return gpm_cell_array_get_icon (collection->primary);

	} else if (unit_ups->is_present) {
		return gpm_cell_array_get_icon (collection->ups);
	}

	/* Check if we should just fallback to the ac icon */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_PRESENT) {
		egg_debug ("no devices present, so no icon will be displayed.");
		return NULL;
	}

	/* we fallback to the ac_adapter icon */
	egg_debug ("Using fallback");
	return g_strdup_printf (GPM_STOCK_AC_ADAPTER);
}

/**
 * gpm_cell_perhaps_recall_cb:
 */
static void
gpm_cell_array_perhaps_recall_cb (GpmCellArray *cell_array, gchar *oem_vendor, gchar *website, GpmEngine *engine)
{
	gboolean show_recall;
	GpmCellUnitKind kind;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* find the kind for easy multiplexing */
	kind = gpm_cell_array_get_kind (cell_array);

	/* only emit this if specified in gconf */
	gpm_conf_get_bool (engine->priv->conf, GPM_CONF_NOTIFY_PERHAPS_RECALL, &show_recall);

	if (show_recall) {
		/* just proxy it to the GUI layer */
		egg_debug ("** EMIT: perhaps-recall");
		g_signal_emit (engine, signals [PERHAPS_RECALL], 0, kind, oem_vendor, website);
	}
}

/**
 * gpm_cell_low_capacity_cb:
 */
static void
gpm_cell_array_low_capacity_cb (GpmCellArray *cell_array, guint capacity, GpmEngine *engine)
{
	gboolean show_capacity;
	GpmCellUnitKind kind;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* find the kind for easy multiplexing */
	kind = gpm_cell_array_get_kind (cell_array);

	/* only emit this if specified in gconf */
	gpm_conf_get_bool (engine->priv->conf, GPM_CONF_NOTIFY_LOW_CAPACITY, &show_capacity);

	/* only emit this once per startup */
	if (show_capacity) {
		/* just proxy it to the GUI layer */
		egg_debug ("** EMIT: low-capacity");
		g_signal_emit (engine, signals [LOW_CAPACITY], 0, kind, capacity);
	}
}

/**
 * gpm_engine_icon_clear_delay:
 *
 * We don't send the action for the icon to clear for a few seconds so that
 * any notification can be shown pointing to the icon.
 *
 * Return value: FALSE, as we don't want to repeat this action.
 **/
static gboolean
gpm_engine_icon_clear_delay (GpmEngine *engine)
{
	g_signal_emit (engine, signals [ICON_CHANGED], 0, NULL);
	return FALSE;
}

/**
 * gpm_engine_recalculate_state_icon:
 */
static gboolean
gpm_engine_recalculate_state_icon (GpmEngine *engine)
{
	gchar *icon;

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), FALSE);

	/* show a different icon if we are disconnected */
	if (engine->priv->hal_connected) {
		/* check the icon */
		icon = gpm_engine_get_icon (engine);
	} else {
		/* show a AC icon */
		icon = g_strdup (GPM_STOCK_AC_ADAPTER);
	}

	if (icon == NULL) {
		/* none before, now none */
		if (engine->priv->previous_icon == NULL) {
			return FALSE;
		}
		/* icon before, now none */
		egg_debug ("** EMIT: icon-changed: none");

		/* we let the icon stick around for a couple of seconds */
		g_timeout_add (1000*2, (GSourceFunc) gpm_engine_icon_clear_delay, engine);

		g_free (engine->priv->previous_icon);
		engine->priv->previous_icon = NULL;
		return TRUE;
	}

	/* no icon before, now icon */
	if (engine->priv->previous_icon == NULL) {
		egg_debug ("** EMIT: icon-changed: %s", icon);
		g_signal_emit (engine, signals [ICON_CHANGED], 0, icon);
		engine->priv->previous_icon = icon;
		return TRUE;
	}

	/* icon before, now different */
	if (strcmp (engine->priv->previous_icon, icon) != 0) {
		g_free (engine->priv->previous_icon);
		engine->priv->previous_icon = icon;
		egg_debug ("** EMIT: icon-changed: %s", icon);
		g_signal_emit (engine, signals [ICON_CHANGED], 0, icon);
		return TRUE;
	}

	egg_debug ("no change");
	/* nothing to do... */
	g_free (icon);
	return FALSE;
}

/**
 * gpm_engine_recalculate_state_summary:
 */
static gboolean
gpm_engine_recalculate_state_summary (GpmEngine *engine)
{
	gchar *summary;

	/* show a different icon if we are disconnected */
	if (engine->priv->hal_connected) {
		/* check the summary */
		summary = gpm_engine_get_summary (engine);
	} else {
		/* show a AC icon */
		summary = g_strdup (_("Unable to get data..."));
	}

	if (engine->priv->previous_summary == NULL) {
		engine->priv->previous_summary = summary;
		egg_debug ("** EMIT: summary-changed(1): %s", summary);
		g_signal_emit (engine, signals [SUMMARY_CHANGED], 0, summary);
		return TRUE;
	}	
	
	if (strcmp (engine->priv->previous_summary, summary) != 0) {
		g_free (engine->priv->previous_summary);
		engine->priv->previous_summary = summary;
		egg_debug ("** EMIT: summary-changed(2): %s", summary);
		g_signal_emit (engine, signals [SUMMARY_CHANGED], 0, summary);
		return TRUE;
	}
	egg_debug ("no change");
	/* nothing to do... */
	g_free (summary);
	return FALSE;
}

/**
 * gpm_engine_recalculate_state:
 */
static void
gpm_engine_recalculate_state (GpmEngine *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* we want to make this quicker */
	if (engine->priv->during_coldplug) {
		egg_debug ("ignoring due to coldplug");
		return;
	}

	gpm_engine_recalculate_state_icon (engine);
	gpm_engine_recalculate_state_summary (engine);
}

/**
 * hal_daemon_start_cb:
 **/
static void
hal_daemon_start_cb (HalGManager *hal_manager,
		     GpmEngine   *engine)
{
	/* change icon or summary */
	engine->priv->hal_connected = TRUE;
	gpm_engine_recalculate_state (engine);
}

/**
 * hal_daemon_stop_cb:
 **/
static void
hal_daemon_stop_cb (HalGManager *hal_manager,
		    GpmEngine   *engine)
{
	/* change icon or summary */
	engine->priv->hal_connected = FALSE;
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_cell_charging_changed_cb:
 */
static void
gpm_cell_array_charging_changed_cb (GpmCellArray *cell_array,
				    gboolean      charging,
				    GpmEngine    *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_cell_discharging_changed_cb:
 */
static void
gpm_cell_array_discharging_changed_cb (GpmCellArray *cell_array,
				       gboolean      discharging,
				       GpmEngine    *engine)
{
	GpmCellUnitKind kind;
	gboolean show_discharging;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);

	/* do we need to send discharging warning? */
	if (discharging) {

		/* only emit this if specified in gconf */
		gpm_conf_get_bool (engine->priv->conf, GPM_CONF_NOTIFY_DISCHARGING, &show_discharging);

		/* don't show warning */
		if (show_discharging == FALSE) {
			return;
		}

		/* find the kind for easy multiplexing */
		kind = gpm_cell_array_get_kind (cell_array);

		/* just proxy it to the GUI layer */
		egg_debug ("** EMIT: discharging");
		g_signal_emit (engine, signals [DISCHARGING], 0, kind);
	}
}

/**
 * gpm_cell_status_changed_cb:
 */
static void
gpm_cell_array_percent_changed_cb (GpmCellArray *cell_array,
				   gfloat        percent,
				   GpmEngine    *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_cell_array_collection_changed_cb:
 */
static void
gpm_cell_array_collection_changed_cb (GpmCellArray *cell_array,
				      GpmEngine    *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_cell_fully_charged_cb:
 */
static void
gpm_cell_array_fully_charged_cb (GpmCellArray *cell_array,
				 GpmEngine    *engine)
{
	gboolean show_fully_charged;
	GpmCellUnitKind kind;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);

	/* only emit this if specified in gconf */
	gpm_conf_get_bool (engine->priv->conf, GPM_CONF_NOTIFY_FULLY_CHARGED, &show_fully_charged);

	/* only emit if in GConf */
	if (show_fully_charged) {
		/* find the kind for easy multiplexing */
		kind = gpm_cell_array_get_kind (cell_array);

		/* just proxy it to the GUI layer */
		egg_debug ("** EMIT: fully-charged");
		g_signal_emit (engine, signals [FULLY_CHARGED], 0, kind);
	}
}

/**
 * gpm_cell_charge_low_cb:
 */
static void
gpm_cell_array_charge_low_cb (GpmCellArray *cell_array,
			      gfloat        percent,
			      GpmEngine    *engine)
{
	GpmCellUnitKind kind;
	GpmCellUnit *unit;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);

	/* find the kind for easy multiplexing */
	kind = gpm_cell_array_get_kind (cell_array);
	unit = gpm_cell_array_get_unit (cell_array);

	/* just proxy it to the GUI layer */
	egg_debug ("** EMIT: charge-low");
	g_signal_emit (engine, signals [CHARGE_LOW], 0, kind, unit);
}

/**
 * gpm_cell_charge_critical_cb:
 */
static void
gpm_cell_array_charge_critical_cb (GpmCellArray *cell_array,
				   gfloat        percent,
				   GpmEngine    *engine)
{
	GpmCellUnitKind kind;
	GpmCellUnit *unit;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);

	/* find the kind for easy multiplexing */
	kind = gpm_cell_array_get_kind (cell_array);
	unit = gpm_cell_array_get_unit (cell_array);

	/* just proxy it to the GUI layer */
	egg_debug ("** EMIT: charge-critical");
	g_signal_emit (engine, signals [CHARGE_CRITICAL], 0, kind, unit);
}

/**
 * gpm_cell_charge_action_cb:
 */
static void
gpm_cell_array_charge_action_cb (GpmCellArray *cell_array,
				 gfloat        percent,
				 GpmEngine    *engine)
{
	GpmCellUnitKind kind;
	GpmCellUnit *unit;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	/* change icon or summary */
	gpm_engine_recalculate_state (engine);

	/* find the kind for easy multiplexing */
	kind = gpm_cell_array_get_kind (cell_array);
	unit = gpm_cell_array_get_unit (cell_array);

	/* just proxy it to the GUI layer */
	egg_debug ("** EMIT: charge-action");
	g_signal_emit (engine, signals [CHARGE_ACTION], 0, kind, unit);
}

/**
 * conf_key_changed_cb:
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmEngine   *engine)
{
	gchar *icon_policy;
	if (strcmp (key, GPM_CONF_UI_ICON_POLICY) == 0) {

		/* do we want to display the icon in the tray */
		gpm_conf_get_string (conf, GPM_CONF_UI_ICON_POLICY, &icon_policy);
		engine->priv->icon_policy = gpm_tray_icon_mode_from_string (icon_policy);
		g_free (icon_policy);

		/* perhaps change icon */
		gpm_engine_recalculate_state_icon (engine);
	}
}

/**
 * gpm_engine_init:
 * @engine: This class instance
 **/
static void
gpm_engine_init (GpmEngine *engine)
{
	gchar *icon_policy;
	GpmEngineCollection *collection;

	engine->priv = GPM_ENGINE_GET_PRIVATE (engine);

	engine->priv->conf = gpm_conf_new ();
	g_signal_connect (engine->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), engine);

	engine->priv->warnings = gpm_warnings_new ();
	engine->priv->profile = gpm_profile_new ();

	engine->priv->previous_icon = NULL;
	engine->priv->previous_summary = NULL;
	engine->priv->hal_connected = TRUE;
	engine->priv->during_coldplug = TRUE;

	/* do we want to display the icon in the tray */
	gpm_conf_get_string (engine->priv->conf, GPM_CONF_UI_ICON_POLICY, &icon_policy);
	engine->priv->icon_policy = gpm_tray_icon_mode_from_string (icon_policy);
	g_free (icon_policy);

	engine->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (engine->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adaptor_changed_cb), engine);

	engine->priv->hal_manager = hal_gmanager_new ();
	g_signal_connect (engine->priv->hal_manager, "daemon-start",
			  G_CALLBACK (hal_daemon_start_cb), engine);
	g_signal_connect (engine->priv->hal_manager, "daemon-stop",
			  G_CALLBACK (hal_daemon_stop_cb), engine);

	engine->priv->control = gpm_control_new ();
	g_signal_connect (engine->priv->control, "resume",
			  G_CALLBACK (control_resume_cb), engine);

	/* grab a reference to the collection */
	collection = &engine->priv->collection;

	collection->primary = gpm_cell_array_new ();
	collection->ups = gpm_cell_array_new ();
	collection->mouse = gpm_cell_array_new ();
	collection->keyboard = gpm_cell_array_new ();
	collection->pda = gpm_cell_array_new ();
	collection->phone = gpm_cell_array_new ();
	engine->priv->ac_adapter = gpm_ac_adapter_new ();

	g_signal_connect (collection->primary, "collection-changed",
			  G_CALLBACK (gpm_cell_array_collection_changed_cb), engine);
	g_signal_connect (collection->primary, "perhaps-recall",
			  G_CALLBACK (gpm_cell_array_perhaps_recall_cb), engine);
	g_signal_connect (collection->primary, "low-capacity",
			  G_CALLBACK (gpm_cell_array_low_capacity_cb), engine);
	g_signal_connect (collection->primary, "charging-changed",
			  G_CALLBACK (gpm_cell_array_charging_changed_cb), engine);
	g_signal_connect (collection->primary, "discharging-changed",
			  G_CALLBACK (gpm_cell_array_discharging_changed_cb), engine);
	g_signal_connect (collection->primary, "percent-changed",
			  G_CALLBACK (gpm_cell_array_percent_changed_cb), engine);
	g_signal_connect (collection->primary, "fully-charged",
			  G_CALLBACK (gpm_cell_array_fully_charged_cb), engine);
	g_signal_connect (collection->primary, "charge-low",
			  G_CALLBACK (gpm_cell_array_charge_low_cb), engine);
	g_signal_connect (collection->primary, "charge-critical",
			  G_CALLBACK (gpm_cell_array_charge_critical_cb), engine);
	g_signal_connect (collection->primary, "charge-action",
			  G_CALLBACK (gpm_cell_array_charge_action_cb), engine);

	g_signal_connect (collection->mouse, "collection-changed",
			  G_CALLBACK (gpm_cell_array_collection_changed_cb), engine);
	g_signal_connect (collection->mouse, "percent-changed",
			  G_CALLBACK (gpm_cell_array_percent_changed_cb), engine);
	g_signal_connect (collection->mouse, "charge-low",
			  G_CALLBACK (gpm_cell_array_charge_low_cb), engine);
	g_signal_connect (collection->mouse, "charge-critical",
			  G_CALLBACK (gpm_cell_array_charge_critical_cb), engine);

	g_signal_connect (collection->phone, "collection-changed",
			  G_CALLBACK (gpm_cell_array_collection_changed_cb), engine);
	g_signal_connect (collection->phone, "percent-changed",
			  G_CALLBACK (gpm_cell_array_percent_changed_cb), engine);
	g_signal_connect (collection->phone, "charge-low",
			  G_CALLBACK (gpm_cell_array_charge_low_cb), engine);
	g_signal_connect (collection->phone, "charge-critical",
			  G_CALLBACK (gpm_cell_array_charge_critical_cb), engine);

	g_signal_connect (collection->keyboard, "collection-changed",
			  G_CALLBACK (gpm_cell_array_collection_changed_cb), engine);
	g_signal_connect (collection->keyboard, "percent-changed",
			  G_CALLBACK (gpm_cell_array_percent_changed_cb), engine);
	g_signal_connect (collection->keyboard, "charge-low",
			  G_CALLBACK (gpm_cell_array_charge_low_cb), engine);
	g_signal_connect (collection->keyboard, "charge-critical",
			  G_CALLBACK (gpm_cell_array_charge_critical_cb), engine);

	g_signal_connect (collection->pda, "collection-changed",
			  G_CALLBACK (gpm_cell_array_collection_changed_cb), engine);
	g_signal_connect (collection->pda, "percent-changed",
			  G_CALLBACK (gpm_cell_array_percent_changed_cb), engine);
	g_signal_connect (collection->pda, "charge-low",
			  G_CALLBACK (gpm_cell_array_charge_low_cb), engine);
	g_signal_connect (collection->pda, "charge-critical",
			  G_CALLBACK (gpm_cell_array_charge_critical_cb), engine);

	g_signal_connect (collection->ups, "collection-changed",
			  G_CALLBACK (gpm_cell_array_collection_changed_cb), engine);
	g_signal_connect (collection->ups, "charging-changed",
			  G_CALLBACK (gpm_cell_array_charging_changed_cb), engine);
	g_signal_connect (collection->ups, "discharging-changed",
			  G_CALLBACK (gpm_cell_array_discharging_changed_cb), engine);
	g_signal_connect (collection->ups, "percent-changed",
			  G_CALLBACK (gpm_cell_array_percent_changed_cb), engine);
	g_signal_connect (collection->ups, "charge-low",
			  G_CALLBACK (gpm_cell_array_charge_low_cb), engine);
	g_signal_connect (collection->ups, "charge-critical",
			  G_CALLBACK (gpm_cell_array_charge_critical_cb), engine);
	g_signal_connect (collection->ups, "charge-action",
			  G_CALLBACK (gpm_cell_array_charge_action_cb), engine);
}

/**
 * gpm_engine_start:
 **/
gboolean
gpm_engine_start (GpmEngine *engine)
{
	GpmEngineCollection *collection;
	GpmPrefsServer *prefs_server;

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), FALSE);

	/* grab a reference to the collection */
	collection = &engine->priv->collection;
	gpm_cell_array_set_type (collection->primary, GPM_CELL_UNIT_KIND_PRIMARY);
	gpm_cell_array_set_type (collection->ups, GPM_CELL_UNIT_KIND_UPS);
	gpm_cell_array_set_type (collection->mouse, GPM_CELL_UNIT_KIND_MOUSE);
	gpm_cell_array_set_type (collection->keyboard, GPM_CELL_UNIT_KIND_KEYBOARD);
	gpm_cell_array_set_type (collection->pda, GPM_CELL_UNIT_KIND_PDA);
	gpm_cell_array_set_type (collection->phone, GPM_CELL_UNIT_KIND_PHONE);

	/* only show the battery prefs section if we have batteries */
	prefs_server = gpm_prefs_server_new ();
	if (gpm_cell_array_get_num_cells (collection->primary) > 0) {
		gpm_prefs_server_set_capability (prefs_server, GPM_PREFS_SERVER_BATTERY);
	}
	if (gpm_cell_array_get_num_cells (collection->ups) > 0) {
		gpm_prefs_server_set_capability (prefs_server, GPM_PREFS_SERVER_UPS);
	}
	g_object_unref (prefs_server);

	/* we're done */
	engine->priv->during_coldplug = FALSE;

	gpm_engine_recalculate_state (engine);
	return TRUE;
}

/**
 * gpm_engine_finalize:
 * @object: This class instance
 **/
static void
gpm_engine_finalize (GObject *object)
{
	GpmEngine *engine;
	GpmEngineCollection *collection;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_ENGINE (object));

	engine = GPM_ENGINE (object);
	engine->priv = GPM_ENGINE_GET_PRIVATE (engine);

	/* grab a reference to the collection */
	collection = &engine->priv->collection;

	g_object_unref (collection->primary);
	g_object_unref (collection->ups);
	g_object_unref (collection->mouse);
	g_object_unref (collection->keyboard);
	g_object_unref (collection->pda);
	g_object_unref (collection->phone);

	g_free (engine->priv->previous_icon);
	g_free (engine->priv->previous_summary);

	g_object_unref (engine->priv->hal_manager);
	g_object_unref (engine->priv->profile);
	g_object_unref (engine->priv->warnings);
	g_object_unref (engine->priv->ac_adapter);
	g_object_unref (engine->priv->control);
	G_OBJECT_CLASS (gpm_engine_parent_class)->finalize (object);
}

/**
 * gpm_engine_new:
 * Return value: new class instance.
 **/
GpmEngine *
gpm_engine_new (void)
{
	GpmEngine *engine;
	engine = g_object_new (GPM_TYPE_ENGINE, NULL);
	return GPM_ENGINE (engine);
}

