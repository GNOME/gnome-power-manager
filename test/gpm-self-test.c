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
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-ui-init.h>
#include <dbus/dbus-glib.h>

#include <libhal-gpower.h>
#include <libhal-gdevice.h>
#include <libhal-gdevicestore.h>
#include <libhal-gmanager.h>

#include "../src/gpm-common.h"
#include "../src/gpm-debug.h"
#include "../src/gpm-powermanager.h"
#include "../src/gpm-proxy.h"
#include "../src/gpm-array.h"

guint test_total = 0;
guint test_suc = 0;
gchar *test_type = NULL;

static void
test_title (const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("> check #%u\t%s: \t%s...", test_total+1, test_type, va_args_buffer);
	test_total++;
}

static void
test_success (const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	test_suc++;
	if (format == NULL) {
		g_print ("...OK\n");
		return;
	}
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("...OK [%s]\n", va_args_buffer);
}

static void
test_failed (const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	g_print ("...FAILED [%s]\n", va_args_buffer);
}

static void
test_gpm_array (GpmPowermanager *powermanager)
{
	GpmArray *array;
	gboolean ret;
	guint size;
	GpmArrayPoint *point;
	gint svalue;
	test_type = "GpmArray         ";
	guint i;

	/************************************************************/
	test_title ("make sure we get a non null array");
	array = gpm_array_new ();
	if (array != NULL) {
		test_success ("got GpmArray");
	} else {
		test_failed ("could not get GpmArray");
	}

	/************** FIXED SIZE TESTS ****************************/
	test_title ("set fixed size of 10");
	ret = gpm_array_set_fixed_size (array, 10);
	if (ret == TRUE) {
		test_success ("set size");
	} else {
		test_failed ("set size failed");
	}

	/************************************************************/
	test_title ("get fixed size");
	size = gpm_array_get_size (array);
	if (size == 10) {
		test_success ("get size passed");
	} else {
		test_failed ("get size failed");
	}

	/************************************************************/
	test_title ("add some data (should fail as fixed size)");
	ret = gpm_array_add (array, 1, 2, 3);
	if (ret == FALSE) {
		test_success ("could not append to fixed size");
	} else {
		test_failed ("appended to fixed size array");
	}

	/************************************************************/
	test_title ("get valid element (should be zero)");
	point = gpm_array_get (array, 0);
	if (point != NULL && point->x == 0 && point->y == 0 && point->data == 0) {
		test_success ("got blank data");
	} else {
		test_failed ("did not get blank data");
	}

	/************************************************************/
	test_title ("get out of range element (should fail)");
	point = gpm_array_get (array, 10);
	if (point == NULL) {
		test_success ("got NULL as OOB");
	} else {
		test_failed ("did not NULL for OOB");
	}

	g_object_unref (array);
	array = gpm_array_new ();

	/************* VARIABLE SIZED TESTS *************************/
	test_title ("add some data (should pass as variable size)");
	ret = gpm_array_add (array, 1, 2, 3);
	if (ret == TRUE) {
		test_success ("appended to variable size");
	} else {
		test_failed ("did not append to variable size array");
	}

	/************************************************************/
	test_title ("get variable size");
	size = gpm_array_get_size (array);
	if (size == 1) {
		test_success ("get size passed");
	} else {
		test_failed ("get size failed");
	}

	/************************************************************/
	test_title ("get out of range element (should fail)");
	point = gpm_array_get (array, 1);
	if (point == NULL) {
		test_success ("got NULL as OOB");
	} else {
		test_failed ("did not NULL for OOB");
	}

	/************************************************************/
	test_title ("clear array");
	ret = gpm_array_clear (array);
	if (ret == TRUE) {
		test_success ("cleared");
	} else {
		test_failed ("did not clear");
	}

	/************************************************************/
	test_title ("get cleared size");
	size = gpm_array_get_size (array);
	if (size == 0) {
		test_success ("get size passed");
	} else {
		test_failed ("get size failed");
	}

	/************************************************************/
	test_title ("save to disk");
	for (i=0;i<100;i++) {
		gpm_array_add (array, i, i, i);
	}
	ret = gpm_array_save_to_file (array, "/tmp/gpm-self-test.txt");
	if (ret == TRUE) {
		test_success ("saved to disk");
	} else {
		test_failed ("could not save to disk");
	}

	/************************************************************/
	test_title ("load from disk");
	gpm_array_clear (array);
	ret = gpm_array_append_from_file (array, "/tmp/gpm-self-test.txt");
	if (ret == TRUE) {
		test_success ("loaded from disk");
	} else {
		test_failed ("could not load from disk");
	}

	/************************************************************/
	test_title ("get file appended size");
	size = gpm_array_get_size (array);
	if (size == 99) {
		test_success ("get size passed");
	} else {
		test_failed ("get size failed: %i", size);
	}

	/************************************************************/
	test_title ("interpolate data");
	gpm_array_clear (array);
	gpm_array_add (array, 1, 2, 0);
	gpm_array_add (array, 3, 9, 0);
	svalue = gpm_array_interpolate (array, 2);
	if (svalue == 6) {
		test_success ("interpolated");
	} else {
		test_failed ("interpolated incorrect: %i", svalue);
	}

	g_object_unref (array);
}

static void
test_hal_power (GpmPowermanager *powermanager)
{
	HalGPower *power;
	gboolean ret;
	test_type = "HalGPower        ";

	/************************************************************/
	test_title ("make sure we get a non null device");
	power = hal_gpower_new ();
	if (power != NULL) {
		test_success ("got HalGPower");
	} else {
		test_failed ("could not get HalGPower");
	}

	/************************************************************/
	test_title ("make sure we have pm support");
	ret = hal_gpower_has_support (power);
	if (ret == TRUE) {
		test_success ("has pm support");
	} else {
		test_failed ("does not have pm support");
	}

	g_object_unref (power);
}

static void
test_hal_manager (GpmPowermanager *powermanager)
{
	HalGManager *manager;
	gboolean ret;
	test_type = "HalGManager      ";

	/************************************************************/
	test_title ("make sure we get a non null device");
	manager = hal_gmanager_new ();
	if (manager != NULL) {
		test_success ("got HalGManager");
	} else {
		test_failed ("could not get HalGManager");
	}

	/************************************************************/
	test_title ("check if we are a laptop");
	ret = hal_gmanager_is_laptop (manager);
	if (ret == TRUE) {
		test_success ("identified as a laptop");
	} else {
		test_failed ("not identified as a laptop");
	}


	/************************************************************/
	gchar **value;
	test_title ("search for battery devices");
	ret = hal_gmanager_find_capability (manager, "battery", &value, NULL);
	if (ret == TRUE && value != NULL && value[0] != NULL) {
		test_success ("found battery device");
	} else {
		test_failed ("did not find battery device");
	}
	hal_gmanager_free_capability (value);

	g_object_unref (manager);
}

static void
test_hal_device (GpmPowermanager *powermanager)
{
	HalGDevice *device;
	const char *udi;
	char *retstr;
	gboolean ret;
	test_type = "HalGDevice       ";

	/************************************************************/
	test_title ("make sure we get a non null device");
	device = hal_gdevice_new ();
	if (device != NULL) {
		test_success ("got HalGDevice");
	} else {
		test_failed ("could not get HalGDevice");
	}

	/************************************************************/
	test_title ("make sure we get a null UDI");
	udi = hal_gdevice_get_udi (device);
	if (udi == NULL) {
		test_success ("got NULL UDI");
	} else {
		test_failed ("got non-null UDI: %s", udi);
	}

	/************************************************************/
	test_title ("make sure we can set a UDI");
	ret = hal_gdevice_set_udi (device, HAL_ROOT_COMPUTER);
	if (ret == TRUE) {
		test_success ("set UDI");
	} else {
		test_failed ("could not set UDI");
	}

	/************************************************************/
	test_title ("make sure we can get the UDI");
	udi = hal_gdevice_get_udi (device);
	if (udi && strcmp (udi, HAL_ROOT_COMPUTER) == 0) {
		test_success ("got correct UDI: %s", udi);
	} else {
		test_failed ("got incorrect UDI: %s", udi);
	}

	/************************************************************/
	test_title ("make sure we cannot set another UDI");
	ret = hal_gdevice_set_udi (device, "foo");
	if (ret == FALSE) {
		test_success ("Cannot set another UDI");
	} else {
		test_failed ("Able to overwrite UDI");
	}

	/************************************************************/
	test_title ("make sure we can get a string key");
	ret = hal_gdevice_get_string (device, "info.product", &retstr, NULL);
	if (ret == TRUE && retstr && strcmp (retstr, "Computer") == 0) {
		test_success ("got correct key");
	} else {
		test_failed ("got invalid key");
	}

	/************************************************************/
	test_title ("try to get property modified events");
	ret = hal_gdevice_watch_property_modified (device);
	if (ret == TRUE) {
		test_success ("got notification");
	} else {
		test_failed ("could not get notification");
	}

	/************************************************************/
	test_title ("try to get duplicate property modified events");
	ret = hal_gdevice_watch_property_modified (device);
	if (ret == FALSE) {
		test_success ("duplicate notification refused");
	} else {
		test_failed ("got duplicate notification");
	}

	/************************************************************/
	test_title ("try to cancel property modified events");
	ret = hal_gdevice_remove_property_modified (device);
	if (ret == TRUE) {
		test_success ("cancel notification");
	} else {
		test_failed ("could not cancel notification");
	}

	/************************************************************/
	test_title ("try to get duplicate property modified cancel");
	ret = hal_gdevice_remove_property_modified (device);
	if (ret == FALSE) {
		test_success ("duplicate cancel refused");
	} else {
		test_failed ("did duplicate cancel");
	}

	g_object_unref (device);
}

static void
test_hal_devicestore (GpmPowermanager *powermanager)
{
	HalGDevicestore *devicestore;
	HalGDevice *device;
	HalGDevice *device2;
	HalGDevice *device3;
	gboolean ret;
	test_type = "HalGDevicestore  ";

	/************************************************************/
	test_title ("make sure we get a non null devicestore");
	devicestore = hal_gdevicestore_new ();
	if (devicestore != NULL) {
		test_success ("got HalGDevicestore");
	} else {
		test_failed ("could not get HalGDevicestore");
	}

	/************************************************************/
	test_title ("make sure device not in devicestore");
	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, HAL_ROOT_COMPUTER);
	ret = hal_gdevicestore_present (devicestore, device);
	if (ret == FALSE) {
		test_success ("could not get different device");
	} else {
		test_failed ("got computer in empty store");
	}

	/************************************************************/
	test_title ("insert device");
	ret = hal_gdevicestore_insert (devicestore, device);
	if (ret == TRUE) {
		test_success ("inserted device");
	} else {
		test_failed ("could not insert device");
	}

	/************************************************************/
	test_title ("insert duplicate device");
	ret = hal_gdevicestore_insert (devicestore, device);
	if (ret == FALSE) {
		test_success ("cannot insert duplicate device");
	} else {
		test_failed ("inserted duplicate device");
	}

	/************************************************************/
	test_title ("make sure device in store");
	ret = hal_gdevicestore_present (devicestore, device);
	if (ret == TRUE) {
		test_success ("found device");
	} else {
		test_failed ("could not find device");
	}

	/************************************************************/
	test_title ("find device by UDI");
	device3 = hal_gdevicestore_find_udi (devicestore, HAL_ROOT_COMPUTER);
	if (device3 == device) {
		test_success ("found device");
	} else {
		test_failed ("could not find device");
	}

	/************************************************************/
	test_title ("make sure we can match on UDI");
	device2 = hal_gdevice_new ();
	hal_gdevice_set_udi (device2, HAL_ROOT_COMPUTER);
	ret = hal_gdevicestore_present (devicestore, device2);
	if (ret == TRUE) {
		test_success ("found device");
	} else {
		test_failed ("could not find device");
	}

	/************************************************************/
	test_title ("remove device");
	ret = hal_gdevicestore_remove (devicestore, device);
	if (ret == TRUE) {
		test_success ("removed device");
	} else {
		test_failed ("could not remove device");
	}

	/************************************************************/
	test_title ("make sure device not in devicestore");
	ret = hal_gdevicestore_present (devicestore, device);
	if (ret == FALSE) {
		test_success ("could not get device from empty devicestore");
	} else {
		test_failed ("got computer in empty store");
	}

	g_object_unref (device);
	g_object_unref (device2);
	g_object_unref (devicestore);
}

static void
test_gpm_proxy (GpmPowermanager *powermanager)
{
	GpmProxy *gproxy = NULL;
	DBusGProxy *proxy = NULL;

	test_type = "GpmProxy         ";

	/************************************************************/
	test_title ("make sure we can get a new gproxy");
	gproxy = gpm_proxy_new ();
	if (gproxy != NULL) {
		test_success ("got gproxy");
	} else {
		test_failed ("could not get gproxy");
	}

	/************************************************************/
	test_title ("make sure proxy if NULL when no assign");
	proxy = gpm_proxy_get_proxy (gproxy);
	if (proxy == NULL) {
		test_success ("got NULL proxy");
	} else {
		test_failed ("did not get NULL proxy");
	}

	/************************************************************/
	test_title ("make sure we can assign and connect");
	proxy = gpm_proxy_assign (gproxy,
				  GPM_PROXY_SESSION,
				  GPM_DBUS_SERVICE,
				  GPM_DBUS_PATH_INHIBIT,
				  GPM_DBUS_INTERFACE_INHIBIT);
	if (proxy != NULL) {
		test_success ("got proxy (init)");
	} else {
		test_failed ("could not get proxy (init)");
	}

	/************************************************************/
	test_title ("make sure proxy non NULL when assigned");
	proxy = gpm_proxy_get_proxy (gproxy);
	if (proxy != NULL) {
		test_success ("got valid proxy");
	} else {
		test_failed ("did not get valid proxy");
	}

	g_object_unref (gproxy);	
}

static void
test_gpm_inhibit (GpmPowermanager *powermanager)
{
	gboolean ret;
	gboolean valid;
	guint cookie1 = 0;
	guint cookie2 = 0;
	test_type = "GpmInhibit       ";

	/************************************************************/
	test_title ("make sure we are not inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == FALSE) {
		test_failed ("Already inhibited");
	} else {
		test_success (NULL);
	}

	/************************************************************/
	test_title ("clear an invalid cookie");
	ret = gpm_powermanager_uninhibit (powermanager, 123456);
	if (ret == FALSE) {
		test_success ("invalid cookie failed as expected");
	} else {
		test_failed ("should have rejected invalid cookie");
	}

	/************************************************************/
	test_title ("get auto cookie 1");
	ret = gpm_powermanager_inhibit_auto (powermanager,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie1);
	if (ret == FALSE) {
		test_failed ("Unable to inhibit");
	} else if (cookie1 == 0) {
		test_failed ("Cookie invalid (cookie: %u)", cookie1);
	} else {
		test_success ("cookie: %u", cookie1);
	}

	/************************************************************/
	test_title ("make sure we are auto inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == FALSE) {
		test_success ("inhibited");
	} else {
		test_failed ("inhibit failed");
	}

	/************************************************************/
	test_title ("make sure we are not manual inhibited");
	ret = gpm_powermanager_is_valid (powermanager, TRUE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == TRUE) {
		test_success ("not inhibited for manual action");
	} else {
		test_failed ("inhibited auto but manual not valid");
	}

	/************************************************************/
	test_title ("get cookie 2");
	ret = gpm_powermanager_inhibit_auto (powermanager,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie2);
	if (ret == FALSE) {
		test_failed ("Unable to inhibit");
	} else if (cookie2 == 0) {
		test_failed ("Cookie invalid (cookie: %u)", cookie2);
	} else {
		test_success ("cookie: %u", cookie2);
	}

	/************************************************************/
	test_title ("clear cookie 1");
	ret = gpm_powermanager_uninhibit (powermanager, cookie1);
	if (ret == FALSE) {
		test_failed ("cookie failed to clear");
	} else {
		test_success (NULL);
	}

	/************************************************************/
	test_title ("make sure we are still inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == FALSE) {
		test_success ("inhibited");
	} else {
		test_failed ("inhibit failed");
	}

	/************************************************************/
	test_title ("clear cookie 2");
	ret = gpm_powermanager_uninhibit (powermanager, cookie2);
	if (ret == FALSE) {
		test_failed ("cookie failed to clear");
	} else {
		test_success (NULL);
	}

	/************************************************************/
	test_title ("make sure we are not auto inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == FALSE) {
		test_failed ("Auto inhibited when we shouldn't be");
	} else {
		test_success (NULL);
	}

	/************************************************************/
	test_title ("make sure we are not manual inhibited");
	ret = gpm_powermanager_is_valid (powermanager, TRUE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == FALSE) {
		test_failed ("Manual inhibited when we shouldn't be");
	} else {
		test_success (NULL);
	}

	/************************************************************/
	test_title ("get manual cookie 1");
	ret = gpm_powermanager_inhibit_manual (powermanager,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie1);
	if (ret == FALSE) {
		test_failed ("Unable to manual inhibit");
	} else if (cookie1 == 0) {
		test_failed ("Cookie invalid (cookie: %u)", cookie1);
	} else {
		test_success ("cookie: %u", cookie1);
	}

	/************************************************************/
	test_title ("make sure we are auto inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == FALSE) {
		test_success ("inhibited");
	} else {
		test_failed ("inhibit failed");
	}

	/************************************************************/
	test_title ("make sure we are manual inhibited");
	ret = gpm_powermanager_is_valid (powermanager, TRUE, &valid);
	if (ret == FALSE) {
		test_failed ("Unable to test validity");
	} else if (valid == FALSE) {
		test_success ("inhibited");
	} else {
		test_failed ("inhibit failed");
	}

	/************************************************************/
	test_title ("clear cookie 1");
	ret = gpm_powermanager_uninhibit (powermanager, cookie1);
	if (ret == FALSE) {
		test_failed ("cookie failed to clear");
	} else {
		test_success (NULL);
	}

}

int
main (int argc, char **argv)
{
	GOptionContext  *context;
 	GnomeProgram    *program;
	GpmPowermanager *powermanager;
	int retval;

	context = g_option_context_new ("GNOME Power Manager Self Test");
	program = gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_GOPTION_CONTEXT, context,
			    GNOME_PARAM_HUMAN_READABLE_NAME,
			    "Power Inhibit Test",
			    NULL);
	gpm_debug_init (FALSE);

	powermanager = gpm_powermanager_new ();
	if (powermanager == NULL) {
		g_warning ("Unable to get connection to power manager");
		return 1;
	}

	test_gpm_array (powermanager);
	test_gpm_inhibit (powermanager);
	test_gpm_proxy (powermanager);
	test_hal_device (powermanager);
	test_hal_devicestore (powermanager);
	test_hal_manager (powermanager);
	test_hal_power (powermanager);

	g_print ("test passes (%u/%u) : ", test_suc, test_total);
	if (test_suc == test_total) {
		g_print ("ALL OKAY\n");
		retval = 0;
	} else {
		g_print ("%u FAILURE(S)\n", test_total - test_suc);
		retval = 1;
	}

	g_object_unref (powermanager);

//	g_object_unref (program);
	g_option_context_free (context);
	return retval;
}
