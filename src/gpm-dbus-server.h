/***************************************************************************
 *
 * gpm-dbus-server.h : DBUS listener
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

#ifndef _GPMDBUSSERVER_H
#define _GPMDBUSSERVER_H

#if !defined (G_GNUC_WARNUNCHECKED)
#if    __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define G_GNUC_WARNUNCHECKED 		__attribute__((warn_unused_result))
#else
#define G_GNUC_WARNUNCHECKED
#endif /* __GNUC__ */
#endif

enum
{
	MAINS_CHANGED,
	ACTION_ABOUT_TO_HAPPEN,
	PERFORMING_ACTION,
	LAST_SIGNAL
};

typedef struct GPMObject GPMObject;
typedef struct GPMObjectClass GPMObjectClass;
GType gpm_object_get_type (void);
struct GPMObject {GObject parent;};
struct GPMObjectClass {GObjectClass parent;};

gboolean gpm_object_register (void) G_GNUC_WARNUNCHECKED;
gboolean gpm_emit_about_to_happen (const gint value);
gboolean gpm_emit_performing_action (const gint value);
gboolean gpm_emit_mains_changed (const gboolean value);

gboolean gpm_object_is_user_idle (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_battery (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_ups (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_ac (GPMObject *obj, gboolean *ret, GError **error);

gboolean gpm_object_ack (GPMObject *obj, gint value, gboolean *ret, GError **error);
gboolean gpm_object_nack (GPMObject *obj, gint value, gchar *reason, gboolean *ret, GError **error);
gboolean gpm_object_action_register (GPMObject *obj, gint value, gchar *reason, gboolean *ret, GError **error);
gboolean gpm_object_action_unregister (GPMObject *obj, gint value, gboolean *ret, GError **error);

#if 0
typedef struct {
	GString *dbusName;
	GString *appName;
	GString *reason;
	gint flags;
	gint timeout;
	gboolean isNACK;
	gboolean isACK;
} RegProgram;
#endif

#endif	/* _GPMDBUSSERVER_H */
