/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "gpm-hal.h"
#include "gpm-hal-power.h"
#include "gpm-proxy.h"
#include "gpm-debug.h"

static void     gpm_hal_power_class_init (GpmHalPowerClass *klass);
static void     gpm_hal_power_init       (GpmHalPower      *hal_power);
static void     gpm_hal_power_finalize   (GObject	   *object);

#define GPM_HAL_POWER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_POWER, GpmHalPowerPrivate))

struct GpmHalPowerPrivate
{
	GpmProxy		*gproxy;
	GpmHal			*hal;
};

G_DEFINE_TYPE (GpmHalPower, gpm_hal_power, G_TYPE_OBJECT)

/**
 * gpm_hal_power_is_on_ac:
 *
 * @hal: This hal class instance
 * Return value: TRUE is computer is running on AC
 **/
gboolean
gpm_hal_power_is_on_ac (GpmHalPower *hal_power)
{
	gboolean is_on_ac;
	gchar **device_names = NULL;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);

	/* find ac_adapter */
	gpm_hal_device_find_capability (hal_power->priv->hal, "ac_adapter", &device_names);
	if (device_names == NULL || device_names[0] == NULL) {
		gpm_debug ("Couldn't obtain list of ac_adapters");
		/* If we do not have an AC adapter, then assume we are a
		 * desktop and return true */
		return TRUE;
	}
	/* assume only one */
	gpm_hal_device_get_bool (hal_power->priv->hal, device_names[0], "ac_adapter.present", &is_on_ac);
	gpm_hal_free_capability (hal_power->priv->hal, device_names);
	return is_on_ac;
}

/**
 * gpm_hal_power_is_laptop:
 *
 * @hal: This hal class instance
 * Return value: TRUE is computer is identified as a laptop
 *
 * Returns true if system.formfactor is "laptop"
 **/
gboolean
gpm_hal_power_is_laptop (GpmHalPower *hal_power)
{
	gboolean ret = TRUE;
	gchar *formfactor = NULL;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);

	/* always present */
	gpm_hal_device_get_string (hal_power->priv->hal, HAL_ROOT_COMPUTER, "system.formfactor", &formfactor);
	if (formfactor == NULL) {
		gpm_debug ("system.formfactor not set!");
		/* no need to free */
		return FALSE;
	}
	if (strcmp (formfactor, "laptop") != 0) {
		gpm_debug ("This machine is not identified as a laptop."
			   "system.formfactor is %s.", formfactor);
		ret = FALSE;
	}
	g_free (formfactor);
	return ret;
}

/**
 * gpm_hal_power_has_power_management:
 *
 * @hal: This hal class instance
 * Return value: TRUE if haldaemon has power management capability
 *
 * Finds out if power management functions are running (only ACPI, PMU, APM)
 **/
gboolean
gpm_hal_power_has_power_management (GpmHalPower *hal_power)
{
	gchar *ptype = NULL;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);

	gpm_hal_device_get_string (hal_power->priv->hal, HAL_ROOT_COMPUTER, "power_management.type", &ptype);
	/* this key only has to exist to be pm okay */
	if (ptype) {
		gpm_debug ("Power management type : %s", ptype);
		g_free (ptype);
		return TRUE;
	}
	return FALSE;
}

/**
 * gpm_hal_power_can_suspend:
 *
 * @hal: This hal class instance
 * Return value: TRUE if kernel suspend support is compiled in
 *
 * Finds out if HAL indicates that we can suspend
 **/
gboolean
gpm_hal_power_can_suspend (GpmHalPower *hal_power)
{
	gboolean exists;
	gboolean can_suspend;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);

	/* TODO: Change to can_suspend when rely on newer HAL */
	exists = gpm_hal_device_get_bool (hal_power->priv->hal, HAL_ROOT_COMPUTER,
					  "power_management.can_suspend_to_ram",
					  &can_suspend);
	if (exists == FALSE) {
		gpm_warning ("gpm_hal_can_suspend: Key can_suspend_to_ram missing");
		return FALSE;
	}
	return can_suspend;
}

/**
 * gpm_hal_power_can_hibernate:
 *
 * @hal: This hal class instance
 * Return value: TRUE if kernel hibernation support is compiled in
 *
 * Finds out if HAL indicates that we can hibernate
 **/
gboolean
gpm_hal_power_can_hibernate (GpmHalPower *hal_power)
{
	gboolean exists;
	gboolean can_hibernate;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);

	/* TODO: Change to can_hibernate when rely on newer HAL */
	exists = gpm_hal_device_get_bool (hal_power->priv->hal, HAL_ROOT_COMPUTER,
					  "power_management.can_suspend_to_disk",
					  &can_hibernate);
	if (exists == FALSE) {
		gpm_warning ("gpm_hal_can_hibernate: Key can_suspend_to_disk missing");
		return FALSE;
	}
	return can_hibernate;
}

/* we have to be clever, as hal can pass back two types of errors, and we have
   to ignore dbus timeouts */
static gboolean
gpm_hal_handle_error (guint ret, GError *error, const gchar *method)
{
	gboolean retval = TRUE;

	g_return_val_if_fail (method != NULL, FALSE);

	if (error) {
		/* DBUS might time out, which is okay. We can remove this code
		   when the dbus glib bindings are fixed. See #332888 */
		if (g_error_matches (error, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
			gpm_debug ("DBUS timed out, but recovering");
			retval = TRUE;
		/* We might also get a generic remote exception if we time out */
		} else if (g_error_matches (error, DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION)) {
			gpm_debug ("Remote exception, recovering");
			retval = TRUE;
		} else {
			gpm_warning ("%s failed\n(%s)",
				     method,
				     error->message);
			gpm_syslog ("%s code='%i' quark='%s'", error->message, error->code, g_quark_to_string (error->domain));
			retval = FALSE;
		}
		g_error_free (error);
	} else if (ret != 0) {
		/* we might not get an error set */
		gpm_warning ("%s failed (Unknown error)", method);
		retval = FALSE;
	}
	return retval;
}

/**
 * gpm_hal_power_suspend:
 *
 * @hal: This hal class instance
 * @wakeup: Seconds to wakeup, currently unsupported
 * Return value: Success, true if we suspended OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 **/
gboolean
gpm_hal_power_suspend (GpmHalPower *hal_power, guint wakeup)
{
	guint ret = 0;
	GError *error = NULL;
	gboolean retval;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);

	proxy = gpm_proxy_get_proxy (hal_power->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	dbus_g_proxy_call (proxy, "Suspend", &error,
			   G_TYPE_INT, wakeup, G_TYPE_INVALID,
			   G_TYPE_UINT, &ret, G_TYPE_INVALID);
	retval = gpm_hal_handle_error (ret, error, "suspend");

	return retval;
}

/**
 * hal_power_pm_method_void:
 *
 * @hal: This hal class instance
 * @method: The method name, e.g. "Hibernate"
 * Return value: Success, true if we did OK
 *
 * Do a method on org.freedesktop.Hal.Device.SystemPowerManagement.*
 * with no arguments.
 **/
static gboolean
hal_power_pm_method_void (GpmHalPower *hal_power, const gchar* method)
{
	guint ret = 0;
	GError *error = NULL;
	gboolean retval;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);
	g_return_val_if_fail (method != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (hal_power->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	dbus_g_proxy_call (proxy, method, &error,
			   G_TYPE_INVALID,
			   G_TYPE_UINT, &ret, G_TYPE_INVALID);
	retval = gpm_hal_handle_error (ret, error, method);

	return retval;
}

/**
 * gpm_hal_power_hibernate:
 *
 * @hal: This hal class instance
 * Return value: Success, true if we hibernated OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 **/
gboolean
gpm_hal_power_hibernate (GpmHalPower *hal_power)
{
	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);
	return hal_power_pm_method_void (hal_power, "Hibernate");
}

/**
 * gpm_hal_shutdown:
 *
 * Return value: Success, true if we shutdown OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Shutdown ()
 **/
gboolean
gpm_hal_power_shutdown (GpmHalPower *hal_power)
{
	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);
	return hal_power_pm_method_void (hal_power, "Shutdown");
}

/**
 * gpm_hal_reboot:
 *
 * @hal: This hal class instance
 * Return value: Success, true if we shutdown OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Reboot ()
 **/
gboolean
gpm_hal_power_reboot (GpmHalPower *hal_power)
{
	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);
	return hal_power_pm_method_void (hal_power, "Reboot");
}

/**
 * gpm_hal_enable_power_save:
 *
 * @hal: This hal class instance
 * @enable: True to enable low power mode
 * Return value: Success, true if we set the mode
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.SetPowerSave ()
 **/
gboolean
gpm_hal_power_enable_power_save (GpmHalPower *hal_power, gboolean enable)
{
	gint ret = 0;
	GError *error = NULL;
	gboolean retval;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL_POWER (hal_power), FALSE);

	proxy = gpm_proxy_get_proxy (hal_power->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	/* abort if we are not a "qualified" laptop */
	if (gpm_hal_power_is_laptop (hal_power) == FALSE) {
		gpm_debug ("We are not a laptop, so not even trying");
		return FALSE;
	}

	gpm_debug ("Doing SetPowerSave (%i)", enable);
	dbus_g_proxy_call (proxy, "SetPowerSave", &error,
			   G_TYPE_BOOLEAN, enable, G_TYPE_INVALID,
			   G_TYPE_UINT, &ret, G_TYPE_INVALID);
	retval = gpm_hal_handle_error (ret, error, "power save");

	return retval;
}



/**
 * gpm_hal_power_class_init:
 * @klass: This hal_power class instance
 **/
static void
gpm_hal_power_class_init (GpmHalPowerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_hal_power_finalize;
	g_type_class_add_private (klass, sizeof (GpmHalPowerPrivate));
}

/**
 * gpm_hal_power_init:
 * @hal_power: This hal_power class instance
 **/
static void
gpm_hal_power_init (GpmHalPower *hal_power)
{
	hal_power->priv = GPM_HAL_POWER_GET_PRIVATE (hal_power);

	hal_power->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (hal_power->priv->gproxy,
			  GPM_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  HAL_ROOT_COMPUTER,
			  HAL_DBUS_INTERFACE_POWER);
	hal_power->priv->hal = gpm_hal_new ();
}

/**
 * gpm_hal_power_finalize:
 * @object: This hal_power class instance
 **/
static void
gpm_hal_power_finalize (GObject *object)
{
	GpmHalPower *hal_power;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL_POWER (object));

	hal_power = GPM_HAL_POWER (object);
	hal_power->priv = GPM_HAL_POWER_GET_PRIVATE (hal_power);
	g_object_unref (hal_power->priv->hal);
	g_object_unref (hal_power->priv->gproxy);

	G_OBJECT_CLASS (gpm_hal_power_parent_class)->finalize (object);
}

/**
 * gpm_hal_power_new:
 * Return value: new GpmHalPower instance.
 **/
GpmHalPower *
gpm_hal_power_new (void)
{
	GpmHalPower *hal_power;
	hal_power = g_object_new (GPM_TYPE_HAL_POWER, NULL);
	return GPM_HAL_POWER (hal_power);
}
