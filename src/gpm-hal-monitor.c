/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
 *
 * Authors:
 *          William Jon McCann <mccann@jhu.edu>
 *          Richard Hughes <richard@hughsie.com>
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "gpm-common.h"
#include "gpm-core.h"
#include "gpm-prefs.h"
#include "gpm-hal.h"
#include "gpm-hal-callback.h"
#include "gpm-sysdev.h"
#include "gpm-marshal.h"

#include "gpm-hal-monitor.h"

static void     gpm_hal_monitor_class_init (GpmHalMonitorClass *klass);
static void     gpm_hal_monitor_init       (GpmHalMonitor      *hal_monitor);
static void     gpm_hal_monitor_finalize   (GObject      *object);

#define GPM_HAL_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_MONITOR, GpmHalMonitorPrivate))

struct GpmHalMonitorPrivate
{
        gboolean enabled;
        gboolean has_power_management;

};

enum {
        POWER_BUTTON,
        SUSPEND_BUTTON,
        LID_BUTTON,
        HIBERNATE,
        SUSPEND,
        LOCK,
        AC_POWER_CHANGED,
        BATTERY_POWER_CHANGED,
        DEVICE_ADDED,
        DEVICE_REMOVED,
        LAST_SIGNAL
};

enum {
        PROP_0,
        PROP_MODE
};

static GObjectClass *parent_class = NULL;
static guint         signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmHalMonitor, gpm_hal_monitor, G_TYPE_OBJECT)

static void
gpm_hal_monitor_set_property (GObject            *object,
                              guint               prop_id,
                              const GValue       *value,
                              GParamSpec         *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gpm_hal_monitor_get_property (GObject            *object,
                              guint               prop_id,
                              GValue             *value,
                              GParamSpec         *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gpm_hal_monitor_class_init (GpmHalMonitorClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize     = gpm_hal_monitor_finalize;
        object_class->get_property = gpm_hal_monitor_get_property;
        object_class->set_property = gpm_hal_monitor_set_property;

        g_type_class_add_private (klass, sizeof (GpmHalMonitorPrivate));

        signals [DEVICE_ADDED] =
                g_signal_new ("device-added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, device_added),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
        signals [DEVICE_REMOVED] =
                g_signal_new ("device-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, device_removed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
        signals [SUSPEND_BUTTON] =
                g_signal_new ("suspend-button",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, suspend_button),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1, G_TYPE_BOOLEAN);
        signals [POWER_BUTTON] =
                g_signal_new ("power-button",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, power_button),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1, G_TYPE_BOOLEAN);
        signals [LID_BUTTON] =
                g_signal_new ("lid-button",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, lid_button),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1, G_TYPE_BOOLEAN);
        signals [HIBERNATE] =
                g_signal_new ("hibernate",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, hibernate),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
        signals [LOCK] =
                g_signal_new ("lock",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, lock),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
        signals [SUSPEND] =
                g_signal_new ("suspend",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, suspend),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
        signals [AC_POWER_CHANGED] =
                g_signal_new ("ac-power-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmHalMonitorClass, ac_power_changed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_BOOLEAN);
	signals [BATTERY_POWER_CHANGED] =
		g_signal_new ("battery-power-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalMonitorClass, battery_power_changed),
			      NULL,
                              NULL,
			      gpm_marshal_VOID__INT_LONG_BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 4, G_TYPE_INT, G_TYPE_LONG, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);


}


/** When we have a device removed
 *
 *  @param	udi		The HAL UDI
 */
static void
hal_device_removed (const gchar *udi,
                    gpointer     user_data)
{
        GpmHalMonitor *monitor;

        monitor = GPM_HAL_MONITOR (user_data);

	if (gpm_device_removed (udi)) {
                g_signal_emit (monitor, signals [DEVICE_REMOVED], 0);
        }
}

/** When we have a new device hot-plugged
 *
 *  @param	udi		UDI
 *  @param	capability	Name of capability
 */
static void
hal_device_new_capability (const gchar *udi,
                           const gchar *capability,
                           gpointer     user_data)
{
        GpmHalMonitor *monitor;

        monitor = GPM_HAL_MONITOR (user_data);

	if (gpm_device_new_capability (udi, capability)) {
                g_signal_emit (monitor, signals [DEVICE_ADDED], 0);
        }
}

static void
monitor_change_on_ac (GpmHalMonitor *monitor,
                      gboolean       on_ac)
{
        g_signal_emit (monitor, signals [AC_POWER_CHANGED], 0, on_ac);
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi		The HAL UDI
 *  @param	key		Property key
 *  @param	is_added	If the key was added
 *  @param	is_removed	If the key was removed
 */
static void
hal_device_property_modified (const gchar *udi,
                              const gchar *key,
                              gboolean     is_added,
                              gboolean     is_removed,
                              gpointer     user_data)
{
	sysDev *sd = NULL;
	sysDevStruct *sds = NULL;
	gchar *type;
	gint old_charge;
	gint new_charge;
        GpmHalMonitor *monitor;
        DeviceType dev;

        monitor = GPM_HAL_MONITOR (user_data);

	g_debug ("hal_device_property_modified: udi=%s, key=%s, added=%i, removed=%i",
                 udi, key, is_added, is_removed);

	/* only process modified entries, not added or removed keys */
	if (is_removed || is_added)
		return;

	if (strcmp (key, "ac_adapter.present") == 0) {
                gboolean on_ac = gpm_hal_is_on_ac ();

                monitor_change_on_ac (monitor, on_ac);

		/* update all states */
		gpm_sysdev_update_all ();

		return;
	}

	/* no point continuing any further if we are never going to match ...*/
	if (strncmp (key, "battery", 7) != 0)
		return;

	sds = gpm_sysdev_find_all (udi);
	/*
	 * if we BUG here then *HAL* has a problem where key modification is
	 * done before capability is present
	 */
	if (!sds) {
		g_warning ("sds is NULL! udi=%s\n"
			   "This is probably a bug in HAL where we are getting "
			   "is_removed=false, is_added=false before the capability "
			   "had been added. In addon-hid-ups this is likely to happen.",
			   udi);
		return;
	}

	/* get battery type so we know what to process */
	gpm_hal_device_get_string (udi, "battery.type", &type);
	if (!type) {
		g_warning ("Battery %s has no type!", udi);
		return;
	}

	dev = hal_to_device_type (type);
	g_free (type);

	/* find old percentage_charge */
	sd = gpm_sysdev_get (dev);
	old_charge = sd->percentage_charge;

	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		gpm_hal_device_get_bool (udi, key, &sds->is_present);
		/* read in values */
		gpm_read_battery_data (sds);

	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		gpm_hal_device_get_bool (udi, key, &sds->is_charging);
		/*
		 * invalidate the remaining time, as we need to wait for
		 * the next HAL update. This is a HAL bug I think.
		 */
		sds->minutes_remaining = 0;
	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		gpm_hal_device_get_bool (udi, key, &sds->is_discharging);
		/* invalidate the remaining time */
		sds->minutes_remaining = 0;
	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		gpm_hal_device_get_int (udi, key, &sds->percentage_charge);

	} else if (strcmp (key, "battery.remaining_time") == 0) {
		gint tempval;
		gpm_hal_device_get_int (udi, key, &tempval);
		if (tempval > 0)
			sds->minutes_remaining = tempval / 60;
	} else {
		/* ignore */
		return;
	}

	gpm_sysdev_update (dev);
	gpm_sysdev_debug_print (dev);

	/* find new percentage_charge  */
	new_charge = sd->percentage_charge;

	g_debug ("new_charge = %i, old_charge = %i", new_charge, old_charge);

	/* do we need to notify the user we are getting low ? */
	if (old_charge != new_charge) {
                gboolean is_primary;

		g_debug ("percentage change %i -> %i", old_charge, new_charge);

                is_primary = (sd->type == BATT_PRIMARY);
                g_signal_emit (monitor,
                               signals [BATTERY_POWER_CHANGED], 0,
                               sd->percentage_charge,
                               sds->minutes_remaining,
                               sds->is_discharging,
                               is_primary);
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi			Univerisal Device Id
 *  @param	name		Name of condition
 *  @param	details	D-BUS message with parameters
 */
static void
hal_device_condition (const gchar *udi,
                      const gchar *name,
                      const gchar *details,
                      gpointer     user_data)
{
	gchar   *type = NULL;
	gboolean value;
        GpmHalMonitor *monitor;

        monitor = GPM_HAL_MONITOR (user_data);

	g_assert (udi);
	g_assert (name);
	g_assert (details);

	g_debug ("hal_device_condition: udi=%s, name=%s, details=%s",
		 udi, name, details);

	if (strcmp (name, "ButtonPressed") == 0) {
		gpm_hal_device_get_string (udi, "button.type", &type);

		if (!type) {
			g_warning ("You must have a button type for %s!", udi);
			return;
		}

		g_debug ("ButtonPressed : %s", type);
		if (strcmp (type, "power") == 0) {
                        gboolean state = TRUE;

                        g_signal_emit (monitor, signals [POWER_BUTTON], 0, state);

		} else if (strcmp (type, "sleep") == 0) {
                        gboolean state = TRUE;
                        g_signal_emit (monitor, signals [SUSPEND_BUTTON], 0, state);

		} else if (strcmp (type, "lid") == 0) {
			gpm_hal_device_get_bool (udi, "button.state.value", &value);

                        g_signal_emit (monitor, signals [LID_BUTTON], 0, value);

		} else if (strcmp (type, "virtual") == 0) {

			if (!details) {
				g_warning ("Virtual buttons must have details for %s!", udi);
				return;
			}

			if (strcmp (details, "BrightnessUp") == 0) {

				gpm_hal_set_brightness_up ();

                        } else if (strcmp (details, "BrightnessDown") == 0) {

				gpm_hal_set_brightness_down ();

                        } else if (strcmp (details, "Suspend") == 0) {

                                g_signal_emit (monitor, signals [SUSPEND], 0);

                        } else if (strcmp (details, "Hibernate") == 0) {

                                g_signal_emit (monitor, signals [HIBERNATE], 0);

                        } else if (strcmp (details, "Lock") == 0) {

                                g_signal_emit (monitor, signals [LOCK], 0);

                        }

		} else {
			g_warning ("Button '%s' unrecognised", type);
		}

		g_free (type);
	}
}

static void
hal_monitor_start (GpmHalMonitor *monitor)
{
	/* initialise all system devices */
	gpm_sysdev_init_all ();

	gpm_hal_callback_init (monitor);
	/* assign the callback functions */
	gpm_hal_method_device_removed (hal_device_removed);
	gpm_hal_method_device_new_capability (hal_device_new_capability);
	gpm_hal_method_device_property_modified (hal_device_property_modified);
	gpm_hal_method_device_condition (hal_device_condition);

	/* sets up these devices and adds watches */
	gpm_coldplug_batteries ();
	gpm_coldplug_acadapter ();
	gpm_coldplug_buttons ();

	gpm_sysdev_update_all ();
	gpm_sysdev_debug_print_all ();
}

static void
hal_monitor_stop (GpmHalMonitor *monitor)
{
	gpm_hal_callback_shutdown ();
	/* cleanup all system devices */
	gpm_sysdev_free_all ();
}

gboolean
gpm_hal_monitor_get_on_ac (GpmHalMonitor *monitor)
{
        gboolean on_ac;

        g_return_val_if_fail (GS_IS_HAL_MONITOR (monitor), FALSE);

        on_ac = gpm_hal_is_on_ac ();

        return on_ac;
}


static void
gpm_hal_monitor_init (GpmHalMonitor *monitor)
{
        monitor->priv = GPM_HAL_MONITOR_GET_PRIVATE (monitor);

        monitor->priv->enabled = gpm_hal_is_running ();

	if (! monitor->priv->enabled) {
		g_warning ("GNOME Power Manager cannot connect to HAL!");
	}

        monitor->priv->has_power_management = gpm_hal_has_power_management ();

	if (! monitor->priv->has_power_management) {
                g_warning ("HAL does not have modern PowerManagement capability");
	}

        if (monitor->priv->enabled
            && monitor->priv->has_power_management) {
                hal_monitor_start (monitor);
        }
}

static void
gpm_hal_monitor_finalize (GObject *object)
{
        GpmHalMonitor *monitor;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GS_IS_HAL_MONITOR (object));

        monitor = GPM_HAL_MONITOR (object);

        g_return_if_fail (monitor->priv != NULL);

        hal_monitor_stop (monitor);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmHalMonitor *
gpm_hal_monitor_new (void)
{
        GpmHalMonitor *monitor;

        monitor = g_object_new (GPM_TYPE_HAL_MONITOR, NULL);

        return GPM_HAL_MONITOR (monitor);
}
