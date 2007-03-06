/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "libhal-marshal.h"
#include "libhal-gpower.h"
#include "libhal-gdevice.h"
#include "libhal-gmanager.h"
#include "../src/gpm-debug.h"
#include "../src/gpm-proxy.h"

static void     hal_gpower_class_init (HalGPowerClass *klass);
static void     hal_gpower_init       (HalGPower      *hal_gpower);
static void     hal_gpower_finalize   (GObject	      *object);

#define LIBHAL_GPOWER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), LIBHAL_TYPE_GPOWER, HalGPowerPrivate))

struct HalGPowerPrivate
{
	HalGDevice		*computer;
	GpmProxy		*gproxy;
};

static gpointer hal_gpower_object = NULL;
G_DEFINE_TYPE (HalGPower, hal_gpower, G_TYPE_OBJECT)

/**
 * hal_gpower_class_init:
 * @klass: This class instance
 **/
static void
hal_gpower_class_init (HalGPowerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = hal_gpower_finalize;
	g_type_class_add_private (klass, sizeof (HalGPowerPrivate));
}

/**
 * hal_gpower_init:
 *
 * @hal_gpower: This class instance
 **/
static void
hal_gpower_init (HalGPower *hal_gpower)
{
	hal_gpower->priv = LIBHAL_GPOWER_GET_PRIVATE (hal_gpower);

	/* get the power connection */
	hal_gpower->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (hal_gpower->priv->gproxy,
			  GPM_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  HAL_ROOT_COMPUTER,
			  HAL_DBUS_INTERFACE_POWER);
	if (hal_gpower->priv->gproxy == NULL) {
		gpm_warning ("HAL does not support power management!");
	}

	hal_gpower->priv->computer = hal_gdevice_new ();
	hal_gdevice_set_udi (hal_gpower->priv->computer, HAL_ROOT_COMPUTER);
}

/**
 * hal_gpower_is_laptop:
 *
 * @hal_gpower: This class instance
 * Return value: TRUE is computer is identified as a laptop
 *
 * Returns true if system.formfactor is "laptop"
 **/
gboolean
hal_gpower_is_laptop (HalGPower *hal_gpower)
{
	gboolean ret = TRUE;
	gchar *formfactor = NULL;

	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	/* always present */
	hal_gdevice_get_string (hal_gpower->priv->computer, "system.formfactor", &formfactor, NULL);
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
 * hal_gpower_has_support:
 *
 * @hal_gpower: This class instance
 * Return value: TRUE if haldaemon has power management capability
 *
 * Finds out if power management functions are running (only ACPI, PMU, APM)
 **/
gboolean
hal_gpower_has_support (HalGPower *hal_gpower)
{
	gchar *type = NULL;

	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	hal_gdevice_get_string (hal_gpower->priv->computer, "power_management.type", &type, NULL);
	/* this key only has to exist to be pm okay */
	if (type != NULL) {
		gpm_debug ("Power management type : %s", type);
		g_free (type);
		return TRUE;
	}
	return FALSE;
}

/**
 * hal_gpower_can_suspend:
 *
 * @hal_gpower: This class instance
 * Return value: TRUE if kernel suspend support is compiled in
 *
 * Finds out if HAL indicates that we can suspend
 **/
gboolean
hal_gpower_can_suspend (HalGPower *hal_gpower)
{
	gboolean exists;
	gboolean can_suspend;

	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	/* TODO: Change to can_suspend when rely on newer HAL */
	exists = hal_gdevice_get_bool (hal_gpower->priv->computer,
					  "power_management.can_suspend",
					  &can_suspend, NULL);
	if (exists == FALSE) {
		gpm_warning ("Key can_suspend missing");
		return FALSE;
	}
	return can_suspend;
}

/**
 * hal_gpower_can_hibernate:
 *
 * @hal_gpower: This class instance
 * Return value: TRUE if kernel hibernation support is compiled in
 *
 * Finds out if HAL indicates that we can hibernate
 **/
gboolean
hal_gpower_can_hibernate (HalGPower *hal_gpower)
{
	gboolean exists;
	gboolean can_hibernate;

	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	/* TODO: Change to can_hibernate when rely on newer HAL */
	exists = hal_gdevice_get_bool (hal_gpower->priv->computer,
					  "power_management.can_hibernate",
					  &can_hibernate, NULL);
	if (exists == FALSE) {
		gpm_warning ("Key can_hibernate missing");
		return FALSE;
	}
	return can_hibernate;
}

/**
 * hal_gpower_filter_error:
 *
 * We have to ignore dbus timeouts sometimes
 **/
static gboolean
hal_gpower_filter_error (GError **error)
{
	/* short cut for speed, no error */
	if (*error == NULL) {
		return FALSE;
	}

	/* DBUS might time out, which is okay. We can remove this code
	   when the dbus glib bindings are fixed. See #332888 */
	if (g_error_matches (*error, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
		gpm_syslog ("DBUS timed out, but recovering");
		g_error_free (*error);
		*error = NULL;
		return TRUE;
	}
	gpm_warning ("Method failed\n(%s)",
		     (*error)->message);
	gpm_syslog ("%s code='%i' quark='%s'", (*error)->message,
		    (*error)->code, g_quark_to_string ((*error)->domain));
	return FALSE;
}

/**
 * hal_gpower_suspend:
 *
 * @hal_gpower: This class instance
 * @wakeup: Seconds to wakeup, currently unsupported
 * Return value: Success, true if we suspended OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 **/
gboolean
hal_gpower_suspend (HalGPower *hal_gpower, guint wakeup)
{
	guint retval = 0;
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	gpm_debug ("Try to suspend...");

	proxy = gpm_proxy_get_proxy (hal_gpower->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "Suspend", &error,
				 G_TYPE_INT, wakeup,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &retval,
				 G_TYPE_INVALID);
	/* we might have to ignore the error */
	if (hal_gpower_filter_error (&error)) {
		return TRUE;
	}
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE || retval != 0) {
		/* abort as the DBUS method failed */
		gpm_warning ("Suspend failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_gpower_pm_method_void:
 *
 * @hal_gpower: This class instance
 * @method: The method name, e.g. "Hibernate"
 * Return value: Success, true if we did OK
 *
 * Do a method on org.freedesktop.Hal.Device.SystemPowerManagement.*
 * with no arguments.
 **/
static gboolean
hal_gpower_pm_method_void (HalGPower *hal_gpower, const gchar *method)
{
	guint retval = 0;
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);
	g_return_val_if_fail (method != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (hal_gpower->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, method, &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &retval,
				 G_TYPE_INVALID);
	/* we might have to ignore the error */
	if (hal_gpower_filter_error (&error)) {
		return TRUE;
	}
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE || retval != 0) {
		/* abort as the DBUS method failed */
		gpm_warning ("%s failed!", method);
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_gpower_hibernate:
 *
 * @hal_gpower: This class instance
 * Return value: Success, true if we hibernated OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 **/
gboolean
hal_gpower_hibernate (HalGPower *hal_gpower)
{
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);
	gpm_debug ("Try to hibernate...");
	return hal_gpower_pm_method_void (hal_gpower, "Hibernate");
}

/**
 * hal_gpower_shutdown:
 *
 * Return value: Success, true if we shutdown OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Shutdown ()
 **/
gboolean
hal_gpower_shutdown (HalGPower *hal_gpower)
{
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);
	gpm_debug ("Try to shutdown...");
	return hal_gpower_pm_method_void (hal_gpower, "Shutdown");
}

/**
 * hal_gpower_reboot:
 *
 * @hal_gpower: This class instance
 * Return value: Success, true if we shutdown OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Reboot ()
 **/
gboolean
hal_gpower_reboot (HalGPower *hal_gpower)
{
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);
	gpm_debug ("Try to reboot...");
	return hal_gpower_pm_method_void (hal_gpower, "Reboot");
}

/**
 * hal_gpower_enable_power_save:
 *
 * @hal_gpower: This class instance
 * @enable: True to enable low power mode
 * Return value: Success, true if we set the mode
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.SetPowerSave ()
 **/
gboolean
hal_gpower_enable_power_save (HalGPower *hal_gpower, gboolean enable)
{
	gint retval = 0;
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (hal_gpower != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	proxy = gpm_proxy_get_proxy (hal_gpower->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}

	/* abort if we are not a "qualified" laptop */
	if (hal_gpower_is_laptop (hal_gpower) == FALSE) {
		gpm_debug ("We are not a laptop, so not even trying");
		return FALSE;
	}

	gpm_debug ("Doing SetPowerSave (%i)", enable);
	ret = dbus_g_proxy_call (proxy, "SetPowerSave", &error,
				 G_TYPE_BOOLEAN, enable,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &retval,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE || retval != 0) {
		/* abort as the DBUS method failed */
		gpm_warning ("SetPowerSave failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_gpower_has_suspend_error:
 *
 * @hal_gpower: This class instance
 * @enable: Return true if there was a suspend error
 * Return value: Success
 *
 * TODO: should call a method on HAL and also return the ouput of the file
 **/
gboolean
hal_gpower_has_suspend_error (HalGPower *hal_gpower, gboolean *state)
{
	g_return_val_if_fail (hal_gpower != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);
	*state = g_file_test ("/var/lib/hal_gpower/system-power-suspend-output", G_FILE_TEST_EXISTS);
	return TRUE;
}

/**
 * hal_gpower_has_hibernate_error:
 *
 * @hal_gpower: This class instance
 * @enable: Return true if there was a hibernate error
 * Return value: Success
 *
 * TODO: should call a method on HAL and also return the ouput of the file
 **/
gboolean
hal_gpower_has_hibernate_error (HalGPower *hal_gpower, gboolean *state)
{
	g_return_val_if_fail (hal_gpower != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);
	*state = g_file_test ("/var/lib/hal_gpower/system-power-hibernate-output", G_FILE_TEST_EXISTS);
	return TRUE;
}

/**
 * hal_gpower_clear_suspend_error:
 *
 * @hal_gpower: This class instance
 * Return value: Success
 *
 * Tells HAL to try and clear the suspend error as we appear to be okay
 **/
gboolean
hal_gpower_clear_suspend_error (HalGPower *hal_gpower, GError **error)
{
#if HAVE_HAL_NEW
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (hal_gpower != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	proxy = gpm_proxy_get_proxy (hal_gpower->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}

	gpm_debug ("Doing SuspendClearError");
	ret = dbus_g_proxy_call (proxy, "SuspendClearError", error,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	return ret;
#endif
	return TRUE;
}

/**
 * hal_gpower_clear_hibernate_error:
 *
 * @hal_gpower: This class instance
 * Return value: Success
 *
 * Tells HAL to try and clear the hibernate error as we appear to be okay
 **/
gboolean
hal_gpower_clear_hibernate_error (HalGPower *hal_gpower, GError **error)
{
#if HAVE_HAL_NEW
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (hal_gpower != NULL, FALSE);
	g_return_val_if_fail (LIBHAL_IS_GPOWER (hal_gpower), FALSE);

	proxy = gpm_proxy_get_proxy (hal_gpower->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}

	gpm_debug ("Doing HibernateClearError");
	ret = dbus_g_proxy_call (proxy, "HibernateClearError", error,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	return ret;
#endif
	return TRUE;
}

/**
 * hal_gpower_finalize:
 * @object: This class instance
 **/
static void
hal_gpower_finalize (GObject *object)
{
	HalGPower *hal_gpower;
	g_return_if_fail (object != NULL);
	g_return_if_fail (LIBHAL_IS_GPOWER (object));

	hal_gpower = LIBHAL_GPOWER (object);
	hal_gpower->priv = LIBHAL_GPOWER_GET_PRIVATE (hal_gpower);

	g_object_unref (hal_gpower->priv->gproxy);
	g_object_unref (hal_gpower->priv->computer);

	G_OBJECT_CLASS (hal_gpower_parent_class)->finalize (object);
}

/**
 * hal_gpower_new:
 * Return value: new HalGPower instance.
 **/
HalGPower *
hal_gpower_new (void)
{
	if (hal_gpower_object != NULL) {
		g_object_ref (hal_gpower_object);
	} else {
		hal_gpower_object = g_object_new (LIBHAL_TYPE_GPOWER, NULL);
		g_object_add_weak_pointer (hal_gpower_object, &hal_gpower_object);
	}
	return LIBHAL_GPOWER (hal_gpower_object);
}
