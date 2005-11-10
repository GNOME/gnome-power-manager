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
#include "compiler.h"

static sysDev mysystem[BATT_LAST];

/** Converts a DeviceType to a string
 *
 *  @param	type		The device type
 *  @return			The string, e.g. "battery"
 */
gchar *
sysDevToString (DeviceType type)
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
sysDevGet (DeviceType type)
{
	if (type == BATT_PRIMARY ||
	    type == BATT_MOUSE ||
	    type == BATT_KEYBOARD ||
	    type == BATT_PDA ||
	    type == BATT_UPS)
		return &mysystem[type];
	g_error ("sysDevGet!");
	return NULL;
}

/** Initialises the system device object of a specified type
 *
 *  @param	type		The device type
 */
void
sysDevInit (DeviceType type)
{
	sysDev *sd = sysDevGet (type);
	sd->devices = g_ptr_array_new ();
	sd->percentageCharge = 0;
	sd->minutesRemaining = 0;
}

/** Frees the system device object of a specified type
 *
 *  @param	type		The device type
 */
void
sysDevFree (DeviceType type)
{
	sysDev *sd = sysDevGet (type);
	g_ptr_array_free (sd->devices, TRUE);
	sd->devices = NULL;
	sd->percentageCharge = 0;
	sd->minutesRemaining = 0;
}

/** Prints the system device object of a specified type
 *
 *  @param	type		The device type
 */
void
sysDevPrint (DeviceType type)
{
	sysDev *sd = sysDevGet (type);
	g_print ("Printing %s device parameters:\n", sysDevToString (type));
	g_print ("percentageCharge = %i\n", sd->percentageCharge);
	g_print ("numberDevices    = %i\n", sd->numberDevices);
	if (type != BATT_MOUSE && type != BATT_KEYBOARD) {
		g_print ("minutesRemaining = %i\n", sd->minutesRemaining);
		g_print ("isCharging       = %i\n", sd->isCharging);
		g_print ("isDischarging    = %i\n", sd->isDischarging);
	}
}

/** Initialises all the system device objects
 */
void
sysDevInitAll ()
{
	g_debug ("Initing all device types");
	sysDevInit (BATT_PRIMARY);
	sysDevInit (BATT_UPS);
	sysDevInit (BATT_MOUSE);
	sysDevInit (BATT_KEYBOARD);
	sysDevInit (BATT_PDA);
}

/** Frees all the system device objects
 */
void
sysDevFreeAll (void)
{
	g_debug ("Freeing all device types");
	sysDevFree (BATT_PRIMARY);
	sysDevFree (BATT_UPS);
	sysDevFree (BATT_MOUSE);
	sysDevFree (BATT_KEYBOARD);
	sysDevFree (BATT_PDA);
}

/** Prints all the system device objects
 */
void
sysDevPrintAll (void)
{
	sysDev *sd = NULL;
	g_debug ("Printing all device types");
	/* only print if we have at least one device type */
	sd = sysDevGet (BATT_PRIMARY);
	if (sd->numberDevices > 0)
		sysDevPrint (BATT_PRIMARY);
	sd = sysDevGet (BATT_UPS);
	if (sd->numberDevices > 0)
		sysDevPrint (BATT_UPS);
	sd = sysDevGet (BATT_MOUSE);
	if (sd->numberDevices > 0)
		sysDevPrint (BATT_MOUSE);
	sd = sysDevGet (BATT_KEYBOARD);
	if (sd->numberDevices > 0)
		sysDevPrint (BATT_KEYBOARD);
	sd = sysDevGet (BATT_PDA);
	if (sd->numberDevices > 0)
		sysDevPrint (BATT_PDA);
}

/** Updates all the system device objects
 */
void
sysDevUpdateAll (void)
{
	g_debug ("Updating all device types");
	sysDevUpdate (BATT_PRIMARY);
	sysDevUpdate (BATT_UPS);
	sysDevUpdate (BATT_MOUSE);
	sysDevUpdate (BATT_KEYBOARD);
	sysDevUpdate (BATT_PDA);
}


/** Adds a system struct to a system device
 *
 *  @param	type		The device type
 *  @param	sds		The valid device struct
 *  @return			Success
 */
void
sysDevAdd (DeviceType type, sysDevStruct *sds)
{
	g_assert (sds);

	/* need to check if already exists */
	sysDev *sd = sysDevGet (type);
	g_assert (sd);
	g_assert (sd->devices);

	/* provide link to parent */
	sds->sd = sd;

	/* add to array */
	g_ptr_array_add (sd->devices, (gpointer) sds);
	sd->type = type;

	/* increment number of devices in this struct */
	sd->numberDevices++;
}

/** Prints all the system structs of a specified system device
 *
 *  @param	type		The device type
 *  @return			Success
 */
void
sysDevList (DeviceType type)
{
	int a;
	sysDev *sd = sysDevGet (type);
	sysDevStruct *temp;

	g_print ("Printing %s device list:\n", sysDevToString (type));
	for (a=0; a < sd->devices->len; a++) {
		temp = (sysDevStruct *) g_ptr_array_index (sd->devices, a);
		g_print ("%s (%i)\n", temp->udi, a);
		g_print (" percentageCharge : %i\n", temp->percentageCharge);
		g_print (" minutesRemaining    : %i\n", temp->minutesRemaining);
		g_print (" present        : %i\n", temp->present);
	}
}

/** Finds all the system structs of a specified system device
 *
 *  @param	type		The device type
 *  @param	udi		The HAL UDI
 *  @return			A sysDevStruct
 */
sysDevStruct *
sysDevFind (DeviceType type, const gchar *udi)
{
	int a;
	sysDev *sd = sysDevGet (type);
	sysDevStruct *temp;

	if (!udi) {
		g_warning ("sysDevFind uni is NULL");
		return NULL;
	}

	for (a=0; a < sd->devices->len; a++) {
		temp = (sysDevStruct *) g_ptr_array_index (sd->devices, a);
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
sysDevFindAll (const gchar *udi)
{
	sysDevStruct *temp;
	/* ordered in order of likeliness */
	temp = sysDevFind (BATT_PRIMARY, udi);
	if (temp)
		return temp;
	temp = sysDevFind (BATT_MOUSE, udi);
	if (temp)
		return temp;
	temp = sysDevFind (BATT_KEYBOARD, udi);
	if (temp)
		return temp;
	temp = sysDevFind (BATT_UPS, udi);
	if (temp)
		return temp;
	temp = sysDevFind (BATT_PDA, udi);
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
sysDevRemove (DeviceType type, const char *udi)
{
	sysDev *sd = sysDevGet (type);
	sysDevStruct *device = sysDevFind (type, udi);
	if (!device) {
		g_warning ("'%s' not found!", udi);
		return;
	}
	sd->numberDevices--;
	g_ptr_array_remove (sd->devices, device);
	g_free (device);
}

/** Removes the system struct from any system device
 *
 *  @param	udi		The HAL UDI
 */
void
sysDevRemoveAll (const char *udi)
{
	sysDevRemove (BATT_PRIMARY, udi);
	sysDevRemove (BATT_UPS, udi);
	sysDevRemove (BATT_MOUSE, udi);
	sysDevRemove (BATT_KEYBOARD, udi);
	sysDevRemove (BATT_PDA, udi);
}

/** Updates the system device object of a specified type
 *
 *  @param	type		The device type
 */
void
sysDevUpdate (DeviceType type)
{
	int a;
	int numPresent = 0;
	int numDischarging = 0;
	sysDev *sd = sysDevGet (type);
	sysDevStruct *sds;

	/* clear old values */
	sd->minutesRemaining = 0;
	sd->percentageCharge = 0;
	sd->isCharging = FALSE;
	sd->isDischarging = FALSE;

	/* find the number of batteries present, and set charge states */
	for (a=0; a < sd->devices->len; a++) {
		sds = (sysDevStruct *) g_ptr_array_index (sd->devices, a);
		if (sds->present) {
			numPresent++;
			/*
			 * Only one device has to be charging or discharging
			 * for the general case to be valid.
			 */
			if (sds->isCharging)
				sd->isCharging = TRUE;
			if (sds->isDischarging) {
				sd->isDischarging = TRUE;
				numDischarging++;
			}
		}
	}
	/* sanity check */
	if (sd->isDischarging && sd->isCharging) {
		g_warning ("sysDevUpdate: Sanity check kicked in! "
			   "Multiple device object cannot be charging and "
			   "discharging simultaneously!");
		sd->isCharging = FALSE;
	}
	/* no point working out average if no devices */
	if (numPresent == 0) {
		g_debug ("no devices of type %s", sysDevToString(type));
		return;
	}
	g_debug ("%i devices of type %s", numPresent, sysDevToString(type));
	/* do the shortcut for a single device, and return */
	if (sd->numberDevices == 1) {
		sds = (sysDevStruct *) g_ptr_array_index (sd->devices, 0);
		sd->minutesRemaining = sds->minutesRemaining;
		sd->percentageCharge = sds->percentageCharge;
		return;
	}
	/* iterate thru all the devices (multiple battery scenario) */
	for (a=0; a < sd->devices->len; a++) {
		sds = (sysDevStruct *) g_ptr_array_index (sd->devices, a);
		if (sds->present) {
			/* for now, just add */
			sd->minutesRemaining += sds->minutesRemaining;
			/* for now, just average */
			sd->percentageCharge += (sds->percentageCharge / numPresent);
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
	if (sd->isDischarging && numDischarging != numPresent) {
		g_warning ("doubling minutesRemaining as sequential");
		/* for now, just double the result */
		sd->minutesRemaining *= numPresent;
	}
}
