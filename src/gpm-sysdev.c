/** @file	gpm-sysdev.c
 *  @brief	The system device store
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-11-05
 *
 * This module handles the abstraction of the 'system struct' into
 * 'system devices'. This complexity is required as a user may have
 * more than one laptop battery or UPS (or misc. wireless periperial).
 * This is required as we show the icons and do the events as averaged
 * over all battery devices of the same type.
 *
 * In code that incldes gpm-sysdev.h, you will see two types of devices:
 *  - sd	These are system devices and are the overview of all
 *		devices of the specific type.
 *  - sds	These are device structs that contain the indervidual
 *		cached data for each specific device.
 */
/*
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gnome.h>
#include <string.h>
#include "gpm-sysdev.h"

static sysDev mysystem[BATT_LAST];

/** Converts a DeviceType to a string
 *
 *  @param	type		The device type
 *  @return			The string, e.g. "battery"
 */
gchar *
gpm_sysdev_to_string (DeviceType type)
{
	if (type == BATT_PRIMARY)
		return _("Laptop battery");
	else if (type == BATT_UPS)
		return _("UPS");
	else if (type == BATT_MOUSE)
		return _("Wireless mouse");
	else if (type == BATT_KEYBOARD)
		return _("Wireless keyboard");
	else if (type == BATT_PDA)
		return _("Misc PDA");
	g_error ("Type '%i' not recognised", type);
	return NULL;
}

/** Returns the sysDev from the internal static device store.
 *
 *  @param	type		The device type
 *  @return			The sysDev
 *
 *  @note	Nowhere in the program other than this should
 *		the mysystem[type] store be accessed.
 */
sysDev *
gpm_sysdev_get (DeviceType type)
{
	if (type == BATT_PRIMARY ||
	    type == BATT_MOUSE ||
	    type == BATT_KEYBOARD ||
	    type == BATT_PDA ||
	    type == BATT_UPS)
		return &mysystem[type];
	g_error ("gpm_sysdev_get!");
	return NULL;
}

/** Initialises the system device object of a specified type
 *
 *  @param	type		The device type
 */
void
gpm_sysdev_init (DeviceType type)
{
	sysDev *sd = gpm_sysdev_get (type);
	sd->devices = g_ptr_array_new ();
	sd->is_present = FALSE;
	sd->percentage_charge = 0;
	sd->minutes_remaining = 0;
}

/** Frees the system device object of a specified type
 *
 *  @param	type		The device type
 */
static void
gpm_sysdev_free (DeviceType type)
{
	sysDev *sd = gpm_sysdev_get (type);
	g_ptr_array_free (sd->devices, TRUE);
	sd->devices = NULL;
	sd->is_present = FALSE;
	sd->percentage_charge = 0;
	sd->minutes_remaining = 0;
}

/** Prints the system device object of a specified type
 *
 *  @param	type		The device type
 */
void
gpm_sysdev_debug_print (DeviceType type)
{
	sysDev *sd = gpm_sysdev_get (type);
	g_debug ("Printing %s device parameters:", gpm_sysdev_to_string (type));
	g_debug ("percentage_charge = %i", sd->percentage_charge);
	g_debug ("number_devices    = %i", sd->number_devices);
	if (type != BATT_MOUSE && type != BATT_KEYBOARD) {
		g_debug ("is_present        = %i", sd->is_present);
		g_debug ("minutes_remaining = %i", sd->minutes_remaining);
		g_debug ("is_charging       = %i", sd->is_charging);
		g_debug ("is_discharging    = %i", sd->is_discharging);
	}
}

/** Initialises all the system device objects
 */
void
gpm_sysdev_init_all ()
{
	gpm_sysdev_init (BATT_PRIMARY);
	gpm_sysdev_init (BATT_UPS);
	gpm_sysdev_init (BATT_MOUSE);
	gpm_sysdev_init (BATT_KEYBOARD);
	gpm_sysdev_init (BATT_PDA);
}

/** Frees all the system device objects
 */
void
gpm_sysdev_free_all (void)
{
	g_debug ("Freeing all device types");
	gpm_sysdev_free (BATT_PRIMARY);
	gpm_sysdev_free (BATT_UPS);
	gpm_sysdev_free (BATT_MOUSE);
	gpm_sysdev_free (BATT_KEYBOARD);
	gpm_sysdev_free (BATT_PDA);
}

/** Prints all the system device objects
 */
void
gpm_sysdev_debug_print_all (void)
{
	sysDev *sd = NULL;
	g_debug ("Printing all device types");
	/* only print if we have at least one device type */
	sd = gpm_sysdev_get (BATT_PRIMARY);
	if (sd->number_devices > 0)
		gpm_sysdev_debug_print (BATT_PRIMARY);
	sd = gpm_sysdev_get (BATT_UPS);
	if (sd->number_devices > 0)
		gpm_sysdev_debug_print (BATT_UPS);
	sd = gpm_sysdev_get (BATT_MOUSE);
	if (sd->number_devices > 0)
		gpm_sysdev_debug_print (BATT_MOUSE);
	sd = gpm_sysdev_get (BATT_KEYBOARD);
	if (sd->number_devices > 0)
		gpm_sysdev_debug_print (BATT_KEYBOARD);
	sd = gpm_sysdev_get (BATT_PDA);
	if (sd->number_devices > 0)
		gpm_sysdev_debug_print (BATT_PDA);
}

/** Updates all the system device objects
 */
void
gpm_sysdev_update_all (void)
{
	g_debug ("Updating all device types");
	gpm_sysdev_update (BATT_PRIMARY);
	gpm_sysdev_update (BATT_UPS);
	gpm_sysdev_update (BATT_MOUSE);
	gpm_sysdev_update (BATT_KEYBOARD);
	gpm_sysdev_update (BATT_PDA);
}


/** Adds a system struct to a system device
 *
 *  @param	type		The device type
 *  @param	sds		The valid device struct
 *  @return			Success
 */
void
gpm_sysdev_add (DeviceType type, sysDevStruct *sds)
{
	g_assert (sds);

	if (!sds->is_present)
		g_warning ("Adding missing device, may bug");

	/* need to check if already exists */
	sysDev *sd = gpm_sysdev_get (type);
	g_assert (sd);
	g_assert (sd->devices);

	/* provide link to parent */
	sds->sd = sd;

	/* add to array */
	g_ptr_array_add (sd->devices, (gpointer) sds);
	sd->type = type;

	/* increment number of devices in this struct */
	sd->number_devices++;
}

/** Gets the specified system device from the table by index
 *
 *  @param	type		The device type
 *  @param	index		The number device in the table
 *  @return			A sysDevStruct
 */
static sysDevStruct *
gpm_sysdev_get_index (DeviceType type, int index)
{
	sysDev *sd = gpm_sysdev_get (type);
	if (index < 0 || index > sd->devices->len) {
		g_warning ("gpm_sysdev_get_index: Invalid index %i", index);
		return NULL;
	}
	return (sysDevStruct *) g_ptr_array_index (sd->devices, index);
}

/** Prints all the system structs of a specified system device
 *
 *  @param	type		The device type
 *  @return			Success
 */
void
gpm_sysdev_list (DeviceType type)
{
	int a;
	sysDev *sd = gpm_sysdev_get (type);
	sysDevStruct *temp;

	g_print ("Printing %s device list:\n", gpm_sysdev_to_string (type));
	for (a=0; a < sd->devices->len; a++) {
		temp = gpm_sysdev_get_index (type, a);
		g_print ("%s (%i)\n", temp->udi, a);
		g_print (" percentage_charge : %i\n", temp->percentage_charge);
		g_print (" minutes_remaining : %i\n", temp->minutes_remaining);
		g_print (" is_present        : %i\n", temp->is_present);
	}
}

/** Finds all the system structs of a specified system device
 *
 *  @param	type		The device type
 *  @param	udi		The HAL UDI
 *  @return			A sysDevStruct
 */
sysDevStruct *
gpm_sysdev_find (DeviceType type, const gchar *udi)
{
	int a;
	sysDev *sd = gpm_sysdev_get (type);
	sysDevStruct *temp;

	if (!udi) {
		g_warning ("gpm_sysdev_find UDI is NULL");
		return NULL;
	}

	for (a=0; a < sd->devices->len; a++) {
		temp = gpm_sysdev_get_index (type, a);
		if (strcmp(temp->udi, udi) == 0)
			return temp;
	}
	return NULL;
}

/** Finds all the system structs of any system device
 *
 *  @param	udi		The HAL UDI
 *  @return			A sysDevStruct
 */
sysDevStruct *
gpm_sysdev_find_all (const gchar *udi)
{
	sysDevStruct *temp;
	/* ordered in order of likeliness */
	temp = gpm_sysdev_find (BATT_PRIMARY, udi);
	if (temp)
		return temp;
	temp = gpm_sysdev_find (BATT_MOUSE, udi);
	if (temp)
		return temp;
	temp = gpm_sysdev_find (BATT_KEYBOARD, udi);
	if (temp)
		return temp;
	temp = gpm_sysdev_find (BATT_UPS, udi);
	if (temp)
		return temp;
	temp = gpm_sysdev_find (BATT_PDA, udi);
	if (temp)
		return temp;
	return NULL;
}

/** Removes the system struct from a specified system device
 *
 *  @param	type		The device type
 *  @param	udi		The HAL UDI
 */
void
gpm_sysdev_remove (DeviceType type, const char *udi)
{
	sysDev *sd = gpm_sysdev_get (type);
	sysDevStruct *device = gpm_sysdev_find (type, udi);
	if (!device) {
		g_warning ("'%s' not found!", udi);
		return;
	}
	sd->number_devices--;
	g_ptr_array_remove (sd->devices, device);
	g_free (device);
}

/** Removes the system struct from any system device
 *
 *  @param	udi		The HAL UDI
 */
void
gpm_sysdev_remove_all (const char *udi)
{
	gpm_sysdev_remove (BATT_PRIMARY, udi);
	gpm_sysdev_remove (BATT_UPS, udi);
	gpm_sysdev_remove (BATT_MOUSE, udi);
	gpm_sysdev_remove (BATT_KEYBOARD, udi);
	gpm_sysdev_remove (BATT_PDA, udi);
}

/** Updates the system device object of a specified type
 *
 *  @param	type		The device type
 */
void
gpm_sysdev_update (DeviceType type)
{
	int a;
	int num_present = 0;
	int num_discharging = 0;
	sysDev *sd = gpm_sysdev_get (type);
	sysDevStruct *sds;

	/* clear old values */
	sd->minutes_remaining = 0;
	sd->percentage_charge = 0;
	sd->is_charging = FALSE;
	sd->is_discharging = FALSE;
	sd->is_present = FALSE;

	/* find the number of devices present, and set charge states */
	for (a=0; a < sd->devices->len; a++) {
		sds = gpm_sysdev_get_index (type, a);
		if (sds->is_present) {
			num_present++;
			/*
			 * Only one device has to be present for the class
			 * to be present.
			 */
			if (sds->is_present)
				sd->is_present = TRUE;
			/*
			 * Only one device has to be charging or discharging
			 * for the class to be valid.
			 */
			if (sds->is_charging)
				sd->is_charging = TRUE;
			if (sds->is_discharging) {
				sd->is_discharging = TRUE;
				num_discharging++;
			}
		}
	}
	/* sanity check */
	if (sd->is_discharging && sd->is_charging) {
		g_warning ("gpm_sysdev_update: Sanity check kicked in! "
			   "Multiple device object cannot be charging and "
			   "discharging simultaneously!");
		sd->is_charging = FALSE;
	}
	/* no point working out average if no devices */
	if (num_present == 0) {
		g_debug ("no devices of type %s", gpm_sysdev_to_string(type));
		return;
	}
	g_debug ("%i devices of type %s", num_present, gpm_sysdev_to_string(type));
	/* do the shortcut for a single device, and return */
	if (sd->number_devices == 1) {
		sds = gpm_sysdev_get_index (type, 0);
		sd->minutes_remaining = sds->minutes_remaining;
		sd->percentage_charge = sds->percentage_charge;
		return;
	}
	/* iterate thru all the devices (multiple battery scenario) */
	for (a=0; a < sd->devices->len; a++) {
		sds = gpm_sysdev_get_index (type, a);
		if (sds->is_present) {
			/* for now, just add */
			sd->minutes_remaining += sds->minutes_remaining;
			/* for now, just average */
			sd->percentage_charge += (sds->percentage_charge / num_present);
		}
	}
	/*
	 * if we are discharging, and the number or batteries
	 * discharging != the number present, then we have a case where the
	 * batteries are discharging one at a time (i.e. not simultanously)
	 * and we have to factor this into the time remaining calculations.
	 * This should effect:
	 *   https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=169158
	 */
	if (sd->is_discharging && num_discharging != num_present) {
		g_warning ("doubling minutes_remaining as sequential");
		/* for now, just double the result */
		sd->minutes_remaining *= num_present;
	}
}
