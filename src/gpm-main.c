/***************************************************************************
 *
 * gpm-main.c : GNOME Power Manager
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 *
 * Taken in part from:
 * - lshal   (C) 2003 David Zeuthen, <david@fubar.dk>
 * - notibat (C) 2004 Benjamin Kahn, <xkahn@zoned.net>
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

#include <glib.h>
#include <dbus/dbus-glib.h>

#include <gdk/gdkx.h>
#include <gnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#if HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include <libhal.h>
#include "gpm-common.h"
#include "gpm-main.h"
#include "gpm-notification.h"
#include "gpm-dbus-server.h"
#include "gpm-idle.h"
#include "gpm-screensaver.h"

#include "dbus-common.h"
#include "glibhal-main.h"
#include "glibhal-callback.h"
#include "glibhal-extras.h"

typedef struct GPMObject GPMObject;
typedef struct GPMObjectClass GPMObjectClass;
GType gpm_object_get_type (void);
struct GPMObject {GObject parent;};
struct GPMObjectClass {GObjectClass parent;};
GPMObject *obj;

enum
{
	MAINS_CHANGED,
	ACTION_ABOUT_TO_HAPPEN,
	PERFORMING_ACTION,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

#define GPM_TYPE_OBJECT			(gpm_object_get_type ())
#define GPM_OBJECT(object)		(G_TYPE_CHECK_INSTANCE_CAST ((object), GPM_TYPE_OBJECT, GPMObject))
#define GPM_OBJECT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GPM_TYPE_OBJECT, GPMObjectClass))
#define GPM_IS_OBJECT(object)		(G_TYPE_CHECK_INSTANCE_TYPE ((object), GPM_TYPE_OBJECT))
#define GPM_IS_OBJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GPM_TYPE_OBJECT))
#define GPM_OBJECT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GPM_TYPE_OBJECT, GPMObjectClass))
G_DEFINE_TYPE(GPMObject, gpm_object, G_TYPE_OBJECT)

gboolean gpm_object_ack (GPMObject *obj, gint value, gboolean *ret, GError **error);
gboolean gpm_object_nack (GPMObject *obj, gint value, gchar *reason, gboolean *ret, GError **error);
gboolean gpm_object_is_user_idle (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_battery (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_ups (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_is_on_ac (GPMObject *obj, gboolean *ret, GError **error);
gboolean gpm_object_action_register (GPMObject *obj, gint value, gchar *reason, gboolean *ret, GError **error);
gboolean gpm_object_action_unregister (GPMObject *obj, gint value, gboolean *ret, GError **error);

#include "gnome-power-glue.h"

static void gpm_object_init (GPMObject *obj) { }
static void gpm_object_class_init (GPMObjectClass *klass)
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

/* static */
#if !defined(GLIBHAL)
static LibHalContext *hal_ctx;
#endif

/* no need for IPC with globals */
StateData state_data;
GPtrArray *objectData = NULL;
GPtrArray *registered = NULL;
gboolean isVerbose;

gboolean
gpm_object_is_user_idle (GPMObject *obj, gboolean *ret, GError **error)
{
	g_warning ("STUB: gpm_object_is_user_idle ()");
	return TRUE;
}

/** Find out if we are on battery power
 *
 *  @param  ret			The returned data value
 *  @return			Success.
 */
gboolean
gpm_object_is_on_battery (GPMObject *obj, gboolean *ret, GError **error)
{
	g_debug ("gpm_object_is_on_mains ()");
	*ret = state_data.onBatteryPower;
	return TRUE;
}

/** Find out if we are on ac power
 *
 *  @param  ret			The returned data value
 *  @return			Success.
 */
gboolean
gpm_object_is_on_ac (GPMObject *obj, gboolean *ret, GError **error)
{
	g_debug ("gpm_object_is_on_ac ()");
	*ret = !state_data.onBatteryPower & !state_data.onUPSPower;
	return TRUE;
}

/** Find out if we are on ups power
 *
 *  @param  ret			The returned data value
 *  @return			Success.
 */
gboolean
gpm_object_is_on_ups (GPMObject *obj, gboolean *ret, GError **error)
{
	g_debug ("gpm_object_is_on_ups ()");
	*ret = state_data.onUPSPower;
	return TRUE;
}

/** emits org.gnome.GnomePowerManager.actionAboutToHappen
 *
 */
gboolean
gpm_emit_about_to_happen (const gint value)
{
	g_signal_emit (obj, signals[ACTION_ABOUT_TO_HAPPEN], 0, value);
	return TRUE;
}

/** emits org.gnome.GnomePowerManager.performingAction
 *
 */
gboolean
gpm_emit_performing_action (const gint value)
{
	g_signal_emit (obj, signals[PERFORMING_ACTION], 0, value);
	return TRUE;
}

/** emits org.gnome.GnomePowerManager.mainsStatusChanged
 *
 */
gboolean
gpm_emit_mains_changed (const gboolean value)
{
	g_signal_emit (obj, signals[MAINS_CHANGED], 0, value);
	return TRUE;
}

/*
 * I know these don't belong here, but I have a problem:
 *
 * I need a way to get the connection name, like we used to using
 *    dbus_message_get_sender (message);
 * but glib bindings abstract away the message.
 * walters to fix :-)
 */
gboolean
gpm_object_ack (GPMObject *obj, gint value, gboolean *ret, GError **error)
{
	g_warning ("STUB: gpm_object_ack (%i)", value);
	return TRUE;
}

gboolean
gpm_object_nack (GPMObject *obj, gint value, gchar *reason, gboolean *ret, GError **error)
{
	g_warning ("STUB: gpm_object_nack (%i, '%s')", value, reason);
	return TRUE;
}

gboolean
gpm_object_action_register (GPMObject *obj, gint value, gchar *name, gboolean *ret, GError **error)
{
	g_warning ("STUB: gpm_object_action_register (%i, '%s')", value, name);
	return TRUE;
}

gboolean
gpm_object_action_unregister (GPMObject *obj, gint value, gboolean *ret, GError **error)
{
	g_warning ("STUB: gpm_object_action_unregister (%i)", value);
	return TRUE;
}

/** If set to lock on screensave, instruct gnome-screensaver to lock screen
 *  and return TRUE.
 *  if set not to lock, then do nothing, and return FALSE.
 */
gboolean
check_gscreensaver_lock (void)
{
	GConfClient *client = gconf_client_get_default ();
	gboolean should_lock;
	should_lock = gconf_client_get_bool (client, "/apps/gnome-screensaver/lock", NULL);
	if (!should_lock)
		return FALSE;
	gscreensaver_lock ();
	return TRUE;
}

/** Convenience function to call libnotify
 *
 *  @param  content		The content text, e.g. "Battery low"
 *  @param  value		The urgency, e.g NOTIFY_URGENCY_CRITICAL
 */
static void
use_libnotify (const char *content, const int urgency)
{
#if HAVE_LIBNOTIFY
	NotifyHandle *n;
	NotifyIcon *icon;
	NotifyHints *hints;
	char *summary;
	gint x, y;
	gboolean use_hints;
	use_hints = get_icon_position (&x, &y);
	icon = notify_icon_new_from_uri (GPM_DATA "gnome-power.png");
	hints = NULL;
	if (use_hints) {
		hints = notify_hints_new();
		notify_hints_set_int (hints, "x", x);
		notify_hints_set_int (hints, "y", y);
		if (urgency == NOTIFY_URGENCY_CRITICAL)
			notify_hints_set_string (hints, "sound-file", GPM_DATA "critical.wav");
		else
			notify_hints_set_string (hints, "sound-file", GPM_DATA "normal.wav");
	}
	summary = NICENAME;
	n = notify_send_notification (NULL, /* replaces nothing 	*/
			   NULL,
			   urgency,
			   summary, content,
			   icon, /* no icon 			*/
			   TRUE, NOTIFY_TIMEOUT,
			   hints,
			   NULL, /* no user data 		*/
			   0);   /* no actions 			*/
	notify_icon_destroy(icon);
	if (!n)
		g_warning ("failed to send notification (%s)", content);
#else
	GtkWidget *widget;
	widget = gnome_message_box_new (content,
			GNOME_MESSAGE_BOX_WARNING,
			GNOME_STOCK_BUTTON_OK,
			NULL);
	gtk_window_set_title (GTK_WINDOW (widget), NICENAME);
	gtk_widget_show (widget);
#endif
}

/** Gets policy from gconf
 *
 *  @param  name		gconf policy name
 *  @return 			the int gconf value of the policy
 */
gint
get_policy_string (const gchar *gconfpath)
{
	GConfClient *client;
	gchar *valuestr;
	gint value;

	g_return_val_if_fail (gconfpath, -1);

	client = gconf_client_get_default ();
	valuestr = gconf_client_get_string (client, gconfpath, NULL);
	value = convert_string_to_policy (valuestr);
	g_free (valuestr);
	return value;
}

static gboolean
dbus_action (gint action)
{
	gpm_emit_about_to_happen (action);
#if 0
	RegProgram *regprog = NULL;
	gint a;
	const int maxwait = 5;
	gboolean retval;

	gboolean allACK = FALSE;
	gboolean anyNACK = FALSE;

	if (registered->len == 0) {
		g_debug ("No connected clients");
		retval = TRUE;
		goto unref;
	}
	g_debug ("Registered clients = %i", registered->len);

	GTimer* gt = g_timer_new ();
	do {

		g_main_context_iteration (NULL, TRUE);
		if (!g_main_context_pending (NULL))
			g_usleep (100*1000);

		allACK = TRUE;
		for (a=0;a<registered->len;a++) {
			regprog = (RegProgram *) g_ptr_array_index (registered, a);
			if (!regprog->isACK && !regprog->isNACK) {
				allACK = FALSE;
				break;
			}
			if (regprog->isACK)
				g_debug ("ACK!");
			if (regprog->isNACK) {
				g_debug ("NACK!");
				anyNACK = TRUE;
				break;
			}
		}
		if (allACK || anyNACK)
			break;
	} while (g_timer_elapsed (gt, NULL) < maxwait);

	regprog->isACK = FALSE;
	regprog->isNACK = FALSE;

	if (anyNACK) {
		GString *gs = g_string_new ("");
		gchar *actionstr = convert_dbus_enum_to_string (action);
		g_string_printf (gs, _("The program '%s' is preventing the %s "
				       "from occurring.\n\n"
				       "The explanation given is: %s"),
				     regprog->appName->str, actionstr, regprog->reason->str);
		g_message ("%s", gs->str);
		use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
		g_string_free (gs, TRUE);
		retval = FALSE;
		goto unref;
	}
	if (!allACK) {
		GString *gs = g_string_new ("");
		gchar *actionstr = convert_policy_to_string (action);
		g_string_printf (gs, _("The program '%s' has not returned data that "
				     "is preventing the %s from occurring."),
				     regprog->appName->str, actionstr);
		g_message ("%s", gs->str);
		use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
		g_string_free (gs, TRUE);
		retval = FALSE;
		goto unref;
	}
#endif
	gpm_emit_performing_action (action);
	return TRUE;
}

static void
run_gconf_script (const char *path)
{
	GConfClient *client;
	gchar *command;
	g_return_if_fail (path);
	client = gconf_client_get_default ();
	command = gconf_client_get_string (client, path, NULL);
	if (command) {
		g_debug ("Executing '%s'", command);
		if (!g_spawn_command_line_async (command, NULL))
			g_warning ("Couldn't execute '%s'.", command);
		g_free (command);
	} else
		g_warning ("'%s' is missing!", path);
}

/** Do the action dictated by policy from gconf
 *
 *  @param  policy_number	What to do!
 */
void
action_policy_do (gint policy_number)
{
#if GPM_SIMULATE
	g_warning ("Ignoring action_policy_do event as simulating!");
	return;
#endif
	gint value;
	GConfClient *client = gconf_client_get_default ();
	if (policy_number == ACTION_NOTHING) {
		g_debug ("*ACTION* Doing nothing");
	} else if (policy_number == ACTION_WARNING) {
		g_warning ("*ACTION* Send warning should be done locally!");
	} else if (policy_number == ACTION_REBOOT && dbus_action (GPM_DBUS_SHUTDOWN)) {
		g_debug ("*ACTION* Reboot");
		run_gconf_script (GCONF_ROOT "general/cmd_reboot");
	} else if (policy_number == ACTION_SUSPEND && dbus_action (GPM_DBUS_SUSPEND)) {
		g_debug ("*ACTION* Suspend");
		check_gscreensaver_lock ();
		hal_suspend (0);
	} else if (policy_number == ACTION_HIBERNATE && dbus_action (GPM_DBUS_HIBERNATE)) {
		g_debug ("*ACTION* Hibernate");
		check_gscreensaver_lock ();
		hal_hibernate ();
	} else if (policy_number == ACTION_SHUTDOWN && dbus_action (GPM_DBUS_SHUTDOWN)) {
		g_debug ("*ACTION* Shutdown");
		run_gconf_script (GCONF_ROOT "general/cmd_shutdown");
	} else if (policy_number == ACTION_BATTERY_CHARGE) {
		g_debug ("*ACTION* Battery Charging");
	} else if (policy_number == ACTION_BATTERY_DISCHARGE) {
		g_debug ("*ACTION* Battery Discharging");
	} else if (policy_number == ACTION_NOW_BATTERYPOWERED) {
		g_debug ("*DBUS* Now battery powered");
		/* set brightness and lowpower mode */
		value = gconf_client_get_int (client,
			GCONF_ROOT "policy/battery/brightness", NULL);
		hal_set_brightness (value);
		hal_setlowpowermode (TRUE);
		/* set gnome screensaver dpms_suspend to our value */
		value = gconf_client_get_int (client,
			GCONF_ROOT "policy/battery/sleep_display", NULL);
		gconf_client_set_int (client,
			"/apps/gnome-screensaver/dpms_suspend", value, NULL);
		/*
		 * make sure gnome-screensaver disables screensaving,
		 * and enables monitor shut-off instead
		 */
		gscreensaver_set_throttle (TRUE);
		/* emit siganal */
		gpm_emit_mains_changed (FALSE);
	} else if (policy_number == ACTION_NOW_MAINSPOWERED) {
		g_debug ("*DBUS* Now mains powered");
		/* set brightness and lowpower mode */
		value = gconf_client_get_int (client,
			GCONF_ROOT "policy/ac/brightness", NULL);
		hal_set_brightness (value);
		hal_setlowpowermode (TRUE);
		/* set dpms_suspend to our value */
		value = gconf_client_get_int (client,
			GCONF_ROOT "policy/ac/sleep_display", NULL);
		gconf_client_set_int (client,
			"/apps/gnome-screensaver/dpms_suspend", value, NULL);
		/* make sure gnome-screensaver enables screensaving */
		gscreensaver_set_throttle (FALSE);
		/* emit siganal */
		gpm_emit_mains_changed (TRUE);
	} else
		g_warning ("action_policy_do called with unknown action %i",
			policy_number);
}

/** Recalculate logic of StateData, without any DBUS, all cached internally
 *  Exported DBUS interface values goes here :-)
 *  @param  coldplug		If set, send events even if they are the same
 */
static void
update_state_logic (GPtrArray *parray, gboolean coldplug)
{
	gint a;
	GenericObject *slotData;
	/* set up temp. state */
	StateData state_datanew;
	gboolean hasBatteries;
	gboolean hasAcAdapter;
	gint policy;

	g_return_if_fail (parray);

	state_datanew.onBatteryPower = FALSE;
	state_datanew.onUPSPower = FALSE;
	hasBatteries = FALSE;
	hasAcAdapter = FALSE;

	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		if (slotData->powerDevice == POWER_PRIMARY_BATTERY)
			if (slotData->present)
				hasBatteries = TRUE;
			else
				g_debug ("Battery '%s' missing?!?", slotData->udi);
		else if (slotData->powerDevice == POWER_AC_ADAPTER)
			hasAcAdapter = TRUE;
		if (slotData->powerDevice == POWER_UPS && slotData->isDischarging)
			state_datanew.onUPSPower = TRUE;
	}

	/* get old value */
	if (hasBatteries) {
		/* Reverse logic as only one ac_adapter is needed to be "on mains power" */
		for (a=0;a<parray->len;a++) {
			slotData = (GenericObject *) g_ptr_array_index (parray, a);
			if (slotData->powerDevice == POWER_AC_ADAPTER && !slotData->present) {
				g_debug ("onBatteryPower TRUE as ac_adapter not present");
				state_datanew.onBatteryPower = TRUE;
				break;
				}
		}
	} else {
		g_debug ("Cannot be on batteries if have none...");
		state_datanew.onBatteryPower = FALSE;
	}
	g_debug ("onBatteryPower = %i (coldplug=%i)", state_datanew.onBatteryPower, coldplug);

	if (coldplug || state_datanew.onBatteryPower != state_data.onBatteryPower) {
		state_data.onBatteryPower = state_datanew.onBatteryPower;
		if (state_data.onBatteryPower) {
			action_policy_do (ACTION_NOW_BATTERYPOWERED);
			policy = get_policy_string (GCONF_ROOT "policy/ac_fail");
			/* only do notification if not coldplug */
			if (!coldplug) {
				if (policy == ACTION_WARNING)
					use_libnotify (_("AC Adapter has been removed"), NOTIFY_URGENCY_NORMAL);
				else
					action_policy_do (policy);
				}
		} else {
			action_policy_do (ACTION_NOW_MAINSPOWERED);
		}
	}

	if (coldplug || state_datanew.onUPSPower != state_data.onUPSPower) {
		state_data.onUPSPower = state_datanew.onUPSPower;
		g_debug ("DBUS: %s = %i", "onUPSPower", state_data.onUPSPower);
	}
}

/** Generic exit
 *
 */
static void
gpm_exit (void)
{
	g_debug ("Quitting!");
	gpn_icon_destroy ();
	exit (0);
}

/** Finds a device from the objectData table
 *
 *  @param  parray		pointer array to GenericObject
 *  @param  udi			HAL UDI
 */
static gint
find_udi_parray_index (GPtrArray *parray, const char *udi)
{
	GenericObject *slotData;
	gint a;

	g_return_val_if_fail (parray, -1);
	g_return_val_if_fail (udi, -1);

	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		g_return_val_if_fail (slotData, -1);
		if (strcmp (slotData->udi, udi) == 0)
			return a;
	}
	return -1;
}

/** Finds a device from the objectData table
 *
 *  @param  parray		pointer array to GenericObject
 *  @param  udi			HAL UDI
 */
static GenericObject *
genericobject_find (GPtrArray *parray, const char *udi)
{
	gint a;

	g_return_val_if_fail (parray, NULL);
	g_return_val_if_fail (udi, NULL);

	a = find_udi_parray_index (parray, udi);
	if (a != -1)
		return (GenericObject *) g_ptr_array_index (parray, a);
	return NULL;
}

/** Adds a device to the objectData table *IF DOES NOT EXIST*
 *
 *  @param  parray		pointer array to GenericObject
 *  @param  udi			HAL UDI
 *  @return			TRUE if we added to the table
 */
static GenericObject *
genericobject_add (GPtrArray *parray, const char *udi)
{
	GenericObject *slotData;
	gint a;

	g_return_val_if_fail (parray, NULL);
	g_return_val_if_fail (udi, NULL);

	a = find_udi_parray_index (parray, udi);
	if (a != -1)
		return NULL;

	slotData = g_new (GenericObject, 1);
	strcpy (slotData->udi, udi);
	slotData->powerDevice = POWER_UNKNOWN;
	slotData->slot = a;
	g_ptr_array_add (parray, (gpointer) slotData);
	return slotData;
}

/** Adds a ac_adapter device. Also sets up properties on cached object
 *
 *  @param  udi			UDI
 */
static void
add_ac_adapter (const gchar *udi)
{
	GenericObject *slotData;
	g_return_if_fail (udi);
	slotData = genericobject_add (objectData, udi);

#if defined(GLIBHAL)
	/* register this with HAL so we get PropertyModified events */
	glibhal_watch_add_device_property_modified (udi);
#endif
	if (!slotData) {
		g_warning ("Cannot add '%s' object to table!", udi);
		return;
	}

	slotData->powerDevice = POWER_AC_ADAPTER;
	slotData->percentageCharge = 0;
	g_debug ("Device '%s' added", udi);
	/* ac_adapter batteries might be missing */
	hal_device_get_bool (udi, "ac_adapter.present", &slotData->present);
	slotData->isRechargeable = FALSE;
	slotData->isCharging = FALSE;
	slotData->isDischarging = FALSE;
	slotData->isRechargeable = 0;
	slotData->percentageCharge = 0;
	slotData->minutesRemaining = 0;
}

static void
read_battery_data (GenericObject *slotData)
{
	gint tempval;

	g_return_if_fail (slotData);

	/* initialise to known defaults */
	slotData->minutesRemaining = 0;
	slotData->isRechargeable = FALSE;
	slotData->isCharging = FALSE;
	slotData->isDischarging = FALSE;

	if (!slotData->present) {
		g_debug ("Battery %s not present!", slotData->udi);
		return;
	}

	/* set cached variables up */
	hal_device_get_int (slotData->udi, "battery.remaining_time", &tempval);
	if (tempval > 0)
		slotData->minutesRemaining = tempval / 60;

	hal_device_get_int (slotData->udi, "battery.charge_level.percentage", &slotData->percentageCharge);
	/* battery might not be rechargeable, have to check */
	hal_device_get_bool (slotData->udi, "battery.is_rechargeable", &slotData->isRechargeable);
	if (slotData->isRechargeable) {
		hal_device_get_bool (slotData->udi, "battery.rechargeable.is_charging", &slotData->isCharging);
		hal_device_get_bool (slotData->udi, "battery.rechargeable.is_discharging", &slotData->isDischarging);
	}
}

/** Adds a battery device, of any type. Also sets up properties on cached object
 *
 *  @param  udi			UDI
 */
static void
add_battery (const gchar *udi)
{
	gchar *type;
	gchar *device;
	GenericObject *slotData;

	g_return_if_fail (udi);

	slotData = genericobject_add (objectData, udi);
	if (!slotData) {
		g_debug ("Cannot add '%s' object to table!", udi);
		return;
	}

#if defined(GLIBHAL)
	/* register this with HAL so we get PropertyModified events */
	glibhal_watch_add_device_property_modified (udi);
#endif

	/* PMU/ACPI batteries might be missing */
	hal_device_get_bool (udi, "battery.present", &slotData->present);

	/* battery is refined using the .type property */
	hal_device_get_string (udi, "battery.type", &type);
	if (!type) {
		g_warning ("Battery %s has no type!", udi);
		return;
	}
	slotData->powerDevice = convert_haltype_to_powerdevice (type);
	g_free (type);

	device = convert_powerdevice_to_string (slotData->powerDevice);
	g_debug ("%s added", device);

	/* read in values */
	read_battery_data (slotData);
}

/** Coldplugs devices of type battery & ups at startup
 *
 */
static void
coldplug_batteries (void)
{
	gint i;
	gchar **device_names;
	/* devices of type battery */
	hal_find_device_capability ("battery", &device_names);
	if (device_names == NULL)
		g_debug (_("Couldn't obtain list of batteries"));
	for (i = 0; device_names[i]; i++)
		add_battery (device_names[i]);
	hal_free_capability (device_names);
}

/** Coldplugs devices of type ac_adaptor at startup
 *
 */
static void
coldplug_acadapter (void)
{
	gint i;
	gchar **device_names;
	/* devices of type ac_adapter */
	hal_find_device_capability ("ac_adapter", &device_names);
	if (device_names == NULL)
		g_debug (_("Couldn't obtain list of ac_adapters"));
	for (i = 0; device_names[i]; i++)
		add_ac_adapter (device_names[i]);
	hal_free_capability (device_names);
}

/** Coldplugs devices of type ac_adaptor at startup
 *
 */
static void
coldplug_buttons (void)
{
	gint i;
	gchar **device_names;
	/* devices of type button */
	hal_find_device_capability ("button", &device_names);
	for (i = 0; device_names[i]; i++) {
		/*
		 * We register this here, as buttons are not present
		 * in object data, and do not need to be added manually.
		*/
#if defined(GLIBHAL)
		glibhal_watch_add_device_condition (device_names[i]);
#endif
	}
	hal_free_capability (device_names);
}

/** Invoked when a device is removed from the Global Device List.
 *  Removes any type of device from the objectData database and removes the
 *  watch on it's UDI.
 *
 *  @param  udi			UDI
 */
static void
#if defined(GLIBHAL)
hal_device_removed (const char *udi)
#else
device_removed (LibHalContext *ctx, const char *udi)
#endif
{
	int a;

	g_return_if_fail (udi);
	g_debug ("hal_device_removed: udi=%s", udi);
	/*
	 * UPS's/mice/keyboards don't use battery.present
	 * they just disappear from the device tree
	 */
	a = find_udi_parray_index (objectData, udi);
	if (a == -1) {
		g_debug ("Asked to remove '%s' when not present", udi);
		return;
	}
	g_debug ("Removed '%s'", udi);
	g_ptr_array_remove_index (objectData, a);
#if defined(GLIBHAL)
	glibhal_watch_remove_device_property_modified (udi);
#endif
	/* our state has changed, update */
	update_state_logic (objectData, FALSE);
	gpn_icon_update ();
}

/** Invoked when device in the Global Device List acquires a new capability.
 *  Prints the name of the capability to stderr.
 *
 *  @param  udi			UDI
 *  @param  capability		Name of capability
 */

static void
#if defined(GLIBHAL)
hal_device_new_capability (const char *udi, const char *capability)
#else
device_new_capability (LibHalContext *ctx, const char *udi, const char *capability)
#endif
{
	g_return_if_fail (udi);
	g_return_if_fail (capability);
	g_debug ("hal_device_new_capability: udi=%s, capability=%s", udi, capability);
	/*
	 * UPS's/mice/keyboards don't use battery.present
	 * they just appear in the device tree
	 */
	if (strcmp (capability, "battery") == 0) {
		add_battery (udi);
		/* our state has changed, update */
		update_state_logic (objectData, FALSE);
		gpn_icon_update ();
	}
}

/** Invoked when device in the Global Device List acquires a new capability.
 *  Prints the name of the capability to stderr.
 *
 *  @param  slotData		Data structure
 *  @param  newCharge		New charge value (%)
 */
static void
notify_user_low_batt (GenericObject *slotData, gint newCharge)
{
	GConfClient *client;
	gint lowThreshold;
	gint criticalThreshold;
	GString *gs;
	GString *remaining;
	gchar *device;

	g_return_if_fail (slotData);

	if (!slotData->isDischarging)
		return;
	client = gconf_client_get_default ();
	lowThreshold = gconf_client_get_int (client, GCONF_ROOT "general/lowThreshold", NULL);
	criticalThreshold = gconf_client_get_int (client, GCONF_ROOT "general/criticalThreshold", NULL);

	/* critical warning */
	if (newCharge < criticalThreshold) {
		gint policy = get_policy_string (GCONF_ROOT "policy/battery_critical");
		if (policy == ACTION_WARNING) {
			device = convert_powerdevice_to_string (slotData->powerDevice);
			remaining = get_time_string (slotData);;
			gs = g_string_new ("");
			g_string_printf (gs, _("The %s (%i%%) is <b>critically low</b> (%s)"),
				device, newCharge, remaining->str);
			g_message ("%s", gs->str);
			use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
			g_string_free (gs, TRUE);
			g_string_free (remaining, TRUE);
		} else
			action_policy_do (policy);
	/* low warning */
	} else if (newCharge < lowThreshold) {
		device = convert_powerdevice_to_string (slotData->powerDevice);
		remaining = get_time_string (slotData);;
		gs = g_string_new ("");
		g_string_printf (gs, _("The %s (%i%%) is <b>low</b> (%s)"),
			device, newCharge, remaining->str);
		g_message ("%s", gs->str);
		use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
		g_string_free (gs, TRUE);
		g_string_free (remaining, TRUE);
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param  udi                 Univerisal Device Id
 *  @param  key                 Key of property
 */
static void
#if defined(GLIBHAL)
hal_device_property_modified (const char *udi, const char *key, gboolean is_added, gboolean is_removed)
#else
property_modified (LibHalContext *ctx, const char *udi, const char *key,
		   dbus_bool_t is_removed, dbus_bool_t is_added)
#endif
{
	GenericObject *slotData;
	GenericObject slotDataVirt;
	gboolean updateState;
	gint oldCharge;
	gint newCharge;

	g_debug ("hal_device_property_modified: udi=%s, key=%s, a=%i, r=%i", udi, key, is_added, is_removed);
	g_return_if_fail (udi);
	g_return_if_fail (key);

	/* only process modified entries, not added or removed keys */
	if (is_removed||is_added)
		return;

	/* no point continuing any further if we are never going to match ...*/
	if (strncmp (key, "battery", 7) != 0 && strncmp (key, "ac_adapter", 10) != 0)
		return;

	slotData = genericobject_find (objectData, udi);
	/* if we BUG here then *HAL* has a problem where key modification is
	 * done before capability is present
	 */
	if (!slotData) {
		g_warning ("slotData is NULL! udi=%s\n"
				   "This is probably a bug in HAL where we are getting "
				   "is_removed=false, is_added=false before the capability "
				   "had been added. In addon-hid-ups this is likely to happen."
				   , udi);
		return;
	}
	updateState = FALSE;

	if (strcmp (key, "battery.present") == 0) {
		hal_device_get_bool (udi, key, &slotData->present);
		/* read in values */
		read_battery_data (slotData);
		updateState = TRUE;
	} else if (strcmp (key, "ac_adapter.present") == 0) {
		hal_device_get_bool (udi, key, &slotData->present);
		updateState = TRUE;
	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		hal_device_get_bool (udi, key, &slotData->isCharging);
		updateState = TRUE;
	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		hal_device_get_bool (udi, key, &slotData->isDischarging);
		updateState = TRUE;
	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		hal_device_get_int (udi, key, &slotData->percentageCharge);
	} else if (strcmp (key, "battery.remaining_time") == 0) {
		gint tempval;
		hal_device_get_int (udi, key, &tempval);
		if (tempval > 0)
			slotData->minutesRemaining = tempval / 60;
	} else
		/* ignore */
		return;

	if (updateState)
		update_state_logic (objectData, FALSE);

	/* find old (taking into account multi-device machines) */
	if (slotData->isRechargeable) {
		slotDataVirt.percentageCharge = 100;
		create_virtual_of_type (&slotDataVirt, slotData->powerDevice);
		oldCharge = slotDataVirt.percentageCharge;
	} else
		oldCharge = slotData->percentageCharge;

	/* find new (taking into account multi-device machines) */
	if (slotData->isRechargeable) {
		slotDataVirt.percentageCharge = 100;
		create_virtual_of_type (&slotDataVirt, slotData->powerDevice);
		newCharge = slotDataVirt.percentageCharge;
	} else
		newCharge = slotData->percentageCharge;

	gpn_icon_update ();

	/* do we need to notify the user we are getting low ? */
	if (oldCharge != newCharge) {
		g_debug ("percentage change %i -> %i", oldCharge, newCharge);
		notify_user_low_batt (slotData, newCharge);
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param  udi                 Univerisal Device Id
 *  @param  condition_name      Name of condition
 *  @param  message             D-BUS message with parameters
 */
static void
#if defined(GLIBHAL)
hal_device_condition (const char *udi,
		const char *condition_name,
		const char *condition_details)
#else
device_condition (LibHalContext *ctx,
		  const char *udi,
		  const char *condition_name,
		  const char *condition_details)
#endif
{
	gchar *type;
	gint policy;
	gboolean value;

	g_debug ("hal_device_condition: udi=%s, condition_name=%s, condition_details=%s", udi, condition_name, condition_details);
	g_return_if_fail (udi);

	if (strcmp (condition_name, "ButtonPressed") == 0) {
		hal_device_get_string (udi, "button.type", &type);
		g_debug ("ButtonPressed : %s", type);
		if (strcmp (type, "power") == 0) {
			policy = get_policy_string (GCONF_ROOT "policy/button_power");
			if (policy == ACTION_WARNING)
				use_libnotify (_("Power button has been pressed"), NOTIFY_URGENCY_NORMAL);
			else
				action_policy_do (policy);
		} else if (strcmp (type, "sleep") == 0) {
			policy = get_policy_string (GCONF_ROOT "policy/button_suspend");
			if (policy == ACTION_WARNING)
				use_libnotify (_("Sleep button has been pressed"), NOTIFY_URGENCY_NORMAL);
			else
				action_policy_do (policy);
		} else if (strcmp (type, "lid") == 0) {
			/* we only do a lid event when the lid is OPENED */
			hal_device_get_bool (udi, "button.state.value", &value);
			if (value) {
				gint policy = get_policy_string (GCONF_ROOT "policy/button_lid");
				if (policy == ACTION_WARNING)
			use_libnotify (_("Lid has been opened"), NOTIFY_URGENCY_NORMAL);
				else
			action_policy_do (policy);
			}
		} else
			g_warning ("Button '%s' unrecognised", type);
		g_free (type);
	}
}

/** Prints program usage.
 *
 */
static void print_usage (void)
{
	g_print ("usage : gnome-power-manager [options]\n");
	g_print (
		"\n"
		"    --disable        Do not perform the action, e.g. suspend\n"
		"    --verbose        Show extra debugging\n"
		"    --version        Show the installed version and quit\n"
		"    --help           Show this information and exit\n"
		"\n");
}

/** Entry point
 *
 *  @param  argc	Number of arguments given to program
 *  @param  argv	Arguments given to program
 *  @return			Return code
 */
int
main (int argc, char *argv[])
{
	gint a;
	GMainLoop *loop;
	GConfClient *client;
	GnomeClient *master;
	GnomeClientFlags flags;
	DBusGConnection *system_connection;
	DBusGConnection *session_connection;
#if !defined(GLIBHAL)
	DBusError error;
	DBusConnection *connsystem;
#endif

	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();
	dbus_g_object_type_install_info (GPM_TYPE_OBJECT, &dbus_glib_gpm_object_object_info);

	gconf_init (argc, argv, NULL);
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, GCONF_ROOT_SANS_SLASH, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (client, GCONF_ROOT_SANS_SLASH,
		callback_gconf_key_changed, NULL, NULL, NULL);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	isVerbose = FALSE;
	for (a=1; a < argc; a++) {
		if (strcmp (argv[a], "--verbose") == 0)
			isVerbose = TRUE;
		else if (strcmp (argv[a], "--version") == 0) {
			g_print ("%s %s", NICENAME, VERSION);
			return EXIT_SUCCESS;
		}
		else if (strcmp (argv[a], "--help") == 0) {
			print_usage ();
			return EXIT_SUCCESS;
		}
	}

	/* set log level */
	if (!isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, g_log_ignore, NULL);

	/* check dbus connections, exit if not valid */
	if (!dbus_get_system_connection (&system_connection))
		exit (1);
	if (!dbus_get_session_connection (&session_connection))
		exit (1);

	/* initialise gnome */
	gnome_program_init (NICENAME, VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);
	master = gnome_master_client ();
	flags = gnome_client_get_flags (master);
	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		gnome_client_set_restart_style (master, GNOME_RESTART_IMMEDIATELY);
		gnome_client_flush (master);
	}
	g_signal_connect (GTK_OBJECT (master), "die", G_CALLBACK (gpm_exit), NULL);

#if HAVE_LIBNOTIFY
	if (!notify_glib_init(NICENAME, NULL))
		g_error ("Cannot initialise libnotify!");
#endif

	g_print ("%s %s - %s\n", NICENAME, VERSION, NICEDESC);
	g_print (_("Report bugs to richard@hughsie.com\n"));
	g_print (_("Please check the faq page before reporting bugs!\n"));
	g_print ("  * http://gnome-power.sourceforge.net/faq.php *");

	/* see if we can get the unique name */
	if (!dbus_get_service (session_connection, GPM_DBUS_SERVICE)) {
		g_warning ("GNOME Power Manager is already running in this session.");
		return 0;
	}

	loop = g_main_loop_new (NULL, FALSE);
	obj = g_object_new (GPM_TYPE_OBJECT, NULL);
	dbus_g_connection_register_g_object (session_connection, GPM_DBUS_PATH, G_OBJECT (obj));

#if !defined(GLIBHAL)
	/* convert to legacy DBusConnection */
	connsystem = dbus_g_connection_get_connection (system_connection);
	dbus_error_init (&error);

	if (!(hal_ctx = libhal_ctx_new ())) {
		use_libnotify ("GNOME Power Manager cannot connect to HAL!", NOTIFY_URGENCY_CRITICAL);
		g_warning ("HAL error: libhal_ctx_new");
		exit (1);
	}
	if (!libhal_ctx_set_dbus_connection (hal_ctx, connsystem)) {
		use_libnotify ("GNOME Power Manager cannot connect to HAL!", NOTIFY_URGENCY_CRITICAL);
		g_warning ("HAL error: libhal_ctx_set_dbus_connection: %s: %s", error.name, error.message);
		exit (1);
	}
	if (!libhal_ctx_init (hal_ctx, &error)) {
		use_libnotify ("GNOME Power Manager cannot connect to HAL!", NOTIFY_URGENCY_CRITICAL);
		g_warning ("HAL error: libhal_ctx_init: %s: %s", error.name, error.message);
		exit (1);
	}
	libhal_ctx_set_device_property_modified (hal_ctx, property_modified);
	libhal_ctx_set_device_removed (hal_ctx, device_removed);
	libhal_ctx_set_device_new_capability (hal_ctx, device_new_capability);
	libhal_ctx_set_device_condition (hal_ctx, device_condition);
	/* bad, bad - we have to check and filter */
	libhal_device_property_watch_all (hal_ctx, &error);
#else

	if (!is_hald_running ()) {
		use_libnotify ("GNOME Power Manager cannot connect to HAL!", NOTIFY_URGENCY_CRITICAL);
		g_warning ("HAL error: is_hald_running FALSE!");
		exit (1);
	}

	if (!hal_pm_check ()) {
		use_libnotify ("HAL does not have PowerManagement capability!", NOTIFY_URGENCY_CRITICAL);
		g_warning ("HAL error: hal_pm_check FALSE!");
		exit (1);
	}

	glibhal_callback_init ();
	/* assign the callback functions */
	glibhal_method_device_removed (hal_device_removed);
	glibhal_method_device_new_capability (hal_device_new_capability);
	glibhal_method_device_property_modified (hal_device_property_modified);
	glibhal_method_device_condition (hal_device_condition);
#endif

	objectData = g_ptr_array_new ();
	registered = g_ptr_array_new ();

	/* sets up these devices in the objectData and adds watches */
	coldplug_batteries ();
	coldplug_acadapter ();
	coldplug_buttons ();

	update_state_logic (objectData, TRUE);
	gpn_icon_initialise ();
	gpn_icon_update ();

	/* set up idle calculation function */
	g_timeout_add (5000, update_idle_function, NULL);

	g_main_loop_run (loop);

	/* free objectData */
	for (a=0;a<objectData->len;a++)
		g_free (g_ptr_array_index (objectData, a));
	g_ptr_array_free (objectData, TRUE);

	/* free registered */
	for (a=0;a<registered->len;a++)
		g_free (g_ptr_array_index (registered, a));
	g_ptr_array_free (registered, TRUE);

#if !defined(GLIBHAL)
	/* free all old HAL stuff */
	dbus_error_init (&error);
	libhal_ctx_shutdown (hal_ctx, &error);
	libhal_ctx_free (hal_ctx);
#else
	glibhal_callback_shutdown ();
#endif

	gpm_exit ();
	return 0;
}
