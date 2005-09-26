/***************************************************************************
 *
 * glibhal-extras.c : GLIB replacement for libhal, the extra stuff
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "dbus-common.h"
#include "glibhal-main.h"
#include "glibhal-extras.h"

#define DIM_INTERVAL		20

/** Uses org.freedesktop.Hal.Device.LaptopPanel.SetBrightness ()
 *
 *  @param  brightness		LCD Brightness to set to
 *  @return			Success
 */
gboolean
hal_get_brightness_item (const char *udi, int *brightness)
{
	GError *error = NULL;
	gboolean retval;
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;

	g_return_val_if_fail (udi, FALSE);

	dbus_get_system_connection (&system_connection);
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal",
		udi,
		"org.freedesktop.Hal.Device.LaptopPanel");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetBrightness", &error,
			G_TYPE_INVALID,
			G_TYPE_UINT, brightness, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning ("org.freedesktop.Hal.Device.LaptopPanel.GetBrightness failed (HAL error)");
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Uses org.freedesktop.Hal.Device.LaptopPanel.SetBrightness ()
 *
 *  @param  brightness		LCD Brightness to set to
 *  @return			Success
 */
static gboolean
hal_set_brightness_item (const char *udi, int brightness)
{
	GError *error = NULL;
	gint ret;
	int levels;
	gboolean retval;
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;

	g_return_val_if_fail (udi, FALSE);

	hal_device_get_int (udi, "laptop_panel.num_levels", &levels);
	if (brightness >= levels || brightness < 0 ) {
		g_warning ("Tried to set brightness %i outside range (0..%i)", brightness, levels - 1);
		return FALSE;
	}
	g_debug ("Setting %s to brightness %i", udi, brightness);

	dbus_get_system_connection (&system_connection);
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal",
		udi,
		"org.freedesktop.Hal.Device.LaptopPanel");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "SetBrightness", &error,
			G_TYPE_INT, brightness, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning ("org.freedesktop.Hal.Device.LaptopPanel.SetBrightness failed (HAL error)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning ("org.freedesktop.Hal.Device.LaptopPanel.SetBrightness call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Sets *all* the laptop_panel objects to the required brightness level
 *
 *  @param  brightness		LCD Brightness to set to
 *  @return			Success, true if *all* adaptors changed
 */
gboolean
hal_set_brightness (int brightness)
{
	gint i;
	gchar **names;
	gboolean retval;

	hal_find_device_capability ("laptop_panel", &names);
	if (!names) {
		g_debug ("No devices of capability laptop_panel");
		return FALSE;
	}
	retval = TRUE;
	/* iterate to seteach laptop_panel object */
	for (i = 0; names[i]; i++)
		if (!hal_set_brightness_item (names[i], brightness))
			retval = FALSE;
	hal_free_capability (names);
	return retval;
}

/** Sets *all* the laptop_panel objects to the required brightness level
 ** And use the fancy new dimming functionality (going through all steps)
 *
 *  @param  brightness		LCD Brightness to set to
 *  @return			Success, true if *all* adaptors changed
 */
gboolean
hal_set_brightness_dim (int brightness)
{
	gint i, a;
	gchar **names;
	gboolean retval;
	int levels, old_level;

	hal_find_device_capability ("laptop_panel", &names);
	if (!names) {
		g_debug ("No devices of capability laptop_panel");
		return FALSE;
	}
	retval = TRUE;
	/* iterate to seteach laptop_panel object */
	for (i = 0; names[i]; i++) {
		/* get old values so we can dim */
		hal_get_brightness_item (names[i], &old_level);
		hal_device_get_int (names[i], "laptop_panel.num_levels", &levels);
		g_debug ("old_level=%i, levels=%i, new=%i", old_level, levels, brightness);
		/* do each step down to the new value */
		if (brightness < old_level) {
			for (a=old_level-1; a >= brightness; a--) {
				if (!hal_set_brightness_item (names[i], a))
					break;
				g_usleep (1000 * DIM_INTERVAL);
			}
		} else {
			for (a=old_level+1; a <= brightness; a++) {
				if (!hal_set_brightness_item (names[i], a))
					break;
				g_usleep (1000 * DIM_INTERVAL);
			}
		}
	}
	hal_free_capability (names);
	return retval;
}


/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 *
 *  @param  wakeup		Seconds to wakeup, currently unsupported
 */
gboolean
hal_suspend (int wakeup)
{
	gint ret;
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	gboolean retval;

	dbus_get_system_connection (&system_connection);
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer", "org.freedesktop.Hal.Device.SystemPowerManagement");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "Suspend", &error,
			G_TYPE_INT, wakeup, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning ("org.freedesktop.Hal.Device.SystemPowerManagement" ".Suspend failed (HAL error)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning ("org.freedesktop.Hal.Device.SystemPowerManagement" ".Suspend call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 *
 */
gboolean
hal_hibernate (void)
{
	gint ret;
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	gboolean retval;

	dbus_get_system_connection (&system_connection);
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer", "org.freedesktop.Hal.Device.SystemPowerManagement");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "Hibernate", &error,
			G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning ("org.freedesktop.Hal.Device.SystemPowerManagement" ".Hibernate failed (HAL error)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning ("org.freedesktop.Hal.Device.SystemPowerManagement" ".Hibernate call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.SetPowerSave ()
 *
 *  @param  set		Set for low power mode
 */
gboolean
hal_setlowpowermode (gboolean set)
{
	gint ret;
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	gboolean retval;
	gchar *formfactor;

	/* always present */
	hal_device_get_string ("/org/freedesktop/Hal/devices/computer", "system.formfactor", &formfactor);
/* TODO: DO NOT NEED after 0.5.5 is released */
	if (!formfactor) {
		g_debug ("system.formfactor not set! If you have PMU, please update HAL to get the latest fixes.");
		return FALSE;
	}
	if (strcmp (formfactor, "laptop") != 0) {
		g_debug ("This machine is not identified as a laptop. system.formfactor is %s.", formfactor);
		g_free (formfactor);
		return FALSE;
	}
	g_free (formfactor);

	dbus_get_system_connection (&system_connection);
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer", "org.freedesktop.Hal.Device.SystemPowerManagement");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "SetPowerSave", &error,
			G_TYPE_BOOLEAN, set, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning ("org.freedesktop.Hal.Device.SystemPowerManagement" ".SetPowerSave failed (HAL error)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning ("org.freedesktop.Hal.Device.SystemPowerManagement" ".SetPowerSave call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Gets the number of brightness steps the (first) LCD adaptor supports
 *
 *  @return  			Number of steps
 */
gint
hal_get_brightness_steps (void)
{
	gchar **names;
	gint levels = 0;
	hal_find_device_capability ("laptop_panel", &names);
	if (!names) {
		g_debug ("No devices of capability laptop_panel");
		return 0;
	}
	/* only use the first one */
	hal_device_get_int (names[0], "laptop_panel.num_levels", &levels);
	hal_free_capability (names);
	return levels;
}
