/** @file	gpm-dbus-server.c
 *  @brief	DBUS listener and signal abstraction
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module handles all th low-level glib DBUS API, and provides
 * the high level hooks into the gpm_object to send signals,
 * and call methods on the gpm_object.
 *
 * @todo	Get the DBUS G-P-M API sorted, perhaps using GConversation.
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
/**
 * @addtogroup	dbus
 * @{
 */

#include <glib.h>
#include <gnome.h>
#include <dbus/dbus-glib.h>
#include "gpm-dbus-server.h"
#include "gpm-common.h"
#include "gpm-dbus-common.h"
#include "gpm-hal.h"

G_DEFINE_TYPE(GPMObject, gpm_object, G_TYPE_OBJECT)

guint signals[LAST_SIGNAL] = { 0 };

GPMObject *obj;

static void
gpm_object_init (GPMObject *obj) { }

static void
gpm_object_class_init (GPMObjectClass *klass)
{
	signals[MAINS_CHANGED] =
		g_signal_new ("mains_status_changed",
			G_OBJECT_CLASS_TYPE (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			0, NULL, NULL,
			g_cclosure_marshal_VOID__BOOLEAN,
			G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals[ACTION_ABOUT_TO_HAPPEN] =
		g_signal_new ("action_about_to_happen",
			G_OBJECT_CLASS_TYPE (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			0, NULL, NULL,
			g_cclosure_marshal_VOID__BOOLEAN,
			G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals[PERFORMING_ACTION] =
		g_signal_new ("performing_action",
			G_OBJECT_CLASS_TYPE (klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			0, NULL, NULL,
			g_cclosure_marshal_VOID__BOOLEAN,
			G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/** registers org.gnome.GnomePowerManager on a connection
 *
 *  @return			If we successfully registered the object
 *
 *  @note	This function MUST be called before DBUS service will work.
 */
gboolean
gpm_object_register (DBusGConnection *connection)
{
	if (!gpm_dbus_get_service (connection, GPM_DBUS_SERVICE))
		return FALSE;
	obj = g_object_new (gpm_object_get_type (), NULL);
	dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH, G_OBJECT (obj));
	return TRUE;
}

/** emits org.gnome.GnomePowerManager.actionAboutToHappen
 *
 *  @param	value		The value we should sent with the signal
 *  @return			If we successfully emmitted the signal
 */
gboolean
gpm_emit_about_to_happen (const gint value)
{
	g_signal_emit (obj, signals[ACTION_ABOUT_TO_HAPPEN], 0, value);
	return TRUE;
}

/** emits org.gnome.GnomePowerManager.performingAction
 *
 *  @param	value		The value we should sent with the signal
 *  @return			If we successfully emmitted the signal
 */
gboolean
gpm_emit_performing_action (const gint value)
{
	g_signal_emit (obj, signals[PERFORMING_ACTION], 0, value);
	return TRUE;
}

/** emits org.gnome.GnomePowerManager.mainsStatusChanged
 *
 *  @param	value		The value we should sent with the signal
 *  @return			If we successfully emmitted the signal
 */
gboolean
gpm_emit_mains_changed (const gboolean value)
{
	g_signal_emit (obj, signals[MAINS_CHANGED], 0, value);
	return TRUE;
}

/** Find out if we are on battery power
 *
 *  @param	obj		The GPM DBUS object
 *  @param	ret		The returned data value
 *  @param	error		Any error value to return, by ref.
 *  @return			Query success
 */
gboolean
gpm_object_is_on_battery (GPMObject *obj, gboolean *ret, GError **error)
{
	g_debug ("gpm_object_is_on_mains ()");
	*ret = !gpm_hal_is_on_ac();
	return TRUE;
}

/** Find out if we are on ac power
 *
 *  @param	obj		The GPM DBUS object
 *  @param	ret		The returned data value
 *  @param	error		Any error value to return, by ref.
 *  @return			Query success
 */
gboolean
gpm_object_is_on_ac (GPMObject *obj, gboolean *ret, GError **error)
{
	g_debug ("gpm_object_is_on_ac ()");
	*ret = gpm_hal_is_on_ac ();
	return TRUE;
}

/** Find out if we are on ups power
 *
 *  @param	obj		The GPM DBUS object
 *  @param	ret		The returned data value
 *  @param	error		Any error value to return, by ref.
 *  @return			Query success
 */
gboolean
gpm_object_is_on_ups (GPMObject *obj, gboolean *ret, GError **error)
{
	g_warning ("gpm_object_is_on_ups ()");
	*ret = FALSE;
	return TRUE;
}

/** @} */
