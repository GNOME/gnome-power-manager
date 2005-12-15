/** @file	gpm-sysdev.h
 *  @brief	The system device store
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-11-05
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
#ifndef _GPMSYSDEV_H
#define _GPMSYSDEV_H

/** The different types of battery */
typedef enum {
	BATT_PRIMARY,			/**< devices of type primary (laptop)	*/
	BATT_UPS,			/**< devices of type UPS		*/
	BATT_MOUSE,			/**< devices of type mouse		*/
	BATT_KEYBOARD,			/**< devices of type keyboard		*/
	BATT_PDA,			/**< devices of type PDA		*/
	BATT_LAST			/**< The last device, for the array #	*/
} DeviceType;

/**
 * The 'system device'
 *
 * Multiple batteries percentages are averaged and times added
 * so that a virtual device is presented to the program. This is
 * required as we show the icons and do the events as averaged
 * over all battery devices of the same type.
 */
typedef struct {
	int percentageCharge;	/**< The percentage charge remaining	*/
	int minutesRemaining;	/**< Minutes remaining until charged	*/
	gboolean isCharging;	/**< If general device is charging	*/
	gboolean isDischarging;	/**< If general device is discharging	*/
	int numberDevices;	/**< Number of devices of this type	*/
	GPtrArray* devices;	/**< system struct array		*/
	DeviceType type;	/**< The device type, e.g. BATT_UPS	*/
} sysDev;

/**
 * The 'system struct'
 *
 * Every laptop battery, mouse, ups keyboard has one of these
 * with a unique UDI
 */
typedef struct {
	gchar udi[128];		/**< The HAL UDI			*/
	gboolean isRechargeable;/**< If device is rechargeable		*/
	int percentageCharge;	/**< The percentage charge remaining	*/
	int minutesRemaining;	/**< Minutes remaining until charged	*/
	gboolean present;	/**< If the device is present		*/
	gboolean isCharging;	/**< If device is charging		*/
	gboolean isDischarging;	/**< If device is discharging		*/
	sysDev *sd;		/**< Pointer to parent system device	*/
} sysDevStruct;

gchar *sysDevToString (DeviceType type);

sysDev *sysDevGet (DeviceType type);
sysDevStruct *sysDevFind (DeviceType type, const gchar *udi);
sysDevStruct *sysDevFindAll (const gchar *udi);

void sysDevInitAll ();
void sysDevFreeAll ();
void sysDevDebugPrintAll (void);
void sysDevUpdateAll ();

void sysDevList (DeviceType type);
void sysDevAdd (DeviceType type, sysDevStruct *sds);
void sysDevDebugPrint (DeviceType type);
void sysDevUpdate (DeviceType type);
void sysDevRemove (DeviceType type, const char *udi);
void sysDevRemoveAll (const char *udi);

#endif	/* _GPMSYSDEV_H */
