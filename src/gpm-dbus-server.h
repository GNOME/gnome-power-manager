/** @file	gpm-dbus-server.h
 *  @brief	DBUS listener and signal abstraction
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
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

#ifndef _GPMDBUSSERVER_H
#define _GPMDBUSSERVER_H

#include "compiler.h"

/** The dbus signal type
 */
enum
{
	MAINS_CHANGED,		/**< Signal mainsStatusChanged		*/
	ACTION_ABOUT_TO_HAPPEN,	/**< Signal actionAboutToHappen		*/
	PERFORMING_ACTION,	/**< Signal performingAction		*/
	LAST_SIGNAL		/**< The last signal...			*/
};

typedef struct GPMObject GPMObject;
typedef struct GPMObjectClass GPMObjectClass;

/** The GObject type reference */
GType gpm_object_get_type (void);

/** The dbus GPM GObject type
 */
struct GPMObject {GObject parent;};
/** The dbus GPM GObject class type
 */
struct GPMObjectClass {GObjectClass parent;};

gboolean gpm_object_register (void) G_GNUC_WARNUNCHECKED;
gboolean gpm_emit_about_to_happen (const gint value);
gboolean gpm_emit_performing_action (const gint value);
gboolean gpm_emit_mains_changed (const gboolean value);

gboolean gpm_object_is_on_battery (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_ups (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_ac (GPMObject *obj, gboolean *ret, GError **error);

#endif	/* _GPMDBUSSERVER_H */
/** @} */
