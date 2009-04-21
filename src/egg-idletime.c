/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2007-2009 William Jon McCann <mccann@jhu.edu>
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

#include <time.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include "egg-idletime.h"
#include "egg-debug.h"

static void egg_idletime_finalize	(GObject		*object);

#define EGG_IDLETIME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_TYPE_IDLETIME, EggIdletimePrivate))

struct EggIdletimePrivate
{
	GHashTable	*watches;
	int		 sync_event_base;
	XSyncCounter	 counter;
};

typedef struct
{
	guint		 id;
	XSyncValue	 interval;
	XSyncAlarm	 xalarm_positive;
	XSyncAlarm	 xalarm_negative;
} EggIdletimeWatch;

enum {
	SIGNAL_ALARM_EXPIRED,
	SIGNAL_RESET,
	LAST_SIGNAL
};

static guint32 watch_serial = 1;
static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EggIdletime, egg_idletime, G_TYPE_OBJECT)

/**
 * egg_idletime_xsyncvalue_to_int64:
 */
static gint64
egg_idletime_xsyncvalue_to_int64 (XSyncValue value)
{
	return ((guint64) XSyncValueHigh32 (value)) << 32
		| (guint64) XSyncValueLow32 (value);
}

/**
 * egg_idletime_int64_to_xsyncvalue:
 */
static XSyncValue
egg_idletime_int64_to_xsyncvalue (gint64 value)
{
	XSyncValue ret;
	XSyncIntsToValue (&ret, value, ((guint64)value) >> 32);
	return ret;
}

/**
 * egg_idletime_find_alarm:
 */
static gboolean
egg_idletime_find_alarm (gpointer key, EggIdletimeWatch *watch, XSyncAlarm *alarm)
{
	egg_debug ("Searching for %d in %d,%d", (int)*alarm, (int)watch->xalarm_positive, (int)watch->xalarm_negative);
	if (watch->xalarm_positive == *alarm ||
	    watch->xalarm_negative == *alarm) {
		return TRUE;
	}
	return FALSE;
}

/**
 * egg_idletime_find_watch_for_alarm:
 */
static EggIdletimeWatch *
egg_idletime_find_watch_for_alarm (EggIdletime *idletime, XSyncAlarm alarm)
{
	EggIdletimeWatch *watch;
	watch = g_hash_table_find (idletime->priv->watches,
				   (GHRFunc)egg_idletime_find_alarm, &alarm);
	return watch;
}

/**
 * egg_idletime_handle_alarm_notify_event:
 */
static void
egg_idletime_handle_alarm_notify_event (EggIdletime *idletime, XSyncAlarmNotifyEvent *alarm_event)
{
	EggIdletimeWatch *watch;

	if (alarm_event->state == XSyncAlarmDestroyed)
		return;

	watch = egg_idletime_find_watch_for_alarm (idletime, alarm_event->alarm);

	if (watch == NULL) {
		egg_warning ("Unable to find watch for alarm %d", (int)alarm_event->alarm);
		return;
	}

	egg_debug ("Watch %d fired, idle time = %" G_GINT64_FORMAT,
		   watch->id, egg_idletime_xsyncvalue_to_int64 (alarm_event->counter_value));

	if (alarm_event->alarm == watch->xalarm_positive) {
		g_signal_emit (idletime, signals [SIGNAL_ALARM_EXPIRED], 0, watch->id);
	} else {
		g_signal_emit (idletime, signals [SIGNAL_RESET], 0);
	}
}

/**
 * egg_idletime_xevent_filter:
 */
static GdkFilterReturn
egg_idletime_xevent_filter (GdkXEvent *xevent, GdkEvent *event, EggIdletime *idletime)
{
	XEvent *ev;
	XSyncAlarmNotifyEvent *alarm_event;

	ev = xevent;
	if (ev->xany.type != idletime->priv->sync_event_base + XSyncAlarmNotify)
		return GDK_FILTER_CONTINUE;

	alarm_event = xevent;
	egg_idletime_handle_alarm_notify_event (idletime, alarm_event);

	return GDK_FILTER_CONTINUE;
}

/**
 * egg_idletime_init_xsync:
 */
static gboolean
egg_idletime_init_xsync (EggIdletime *idletime)
{
	int sync_error_base;
	int res;
	int major;
	int minor;
	int i;
	int ncounters;
	XSyncSystemCounter *counters;

	res = XSyncQueryExtension (GDK_DISPLAY (),
				   &idletime->priv->sync_event_base,
				   &sync_error_base);
	if (res == 0) {
		egg_warning ("EggIdletime: Sync extension not present");
		return FALSE;
	}

	res = XSyncInitialize (GDK_DISPLAY (), &major, &minor);
	if (res == 0) {
		egg_warning ("EggIdletime: Unable to initialize Sync extension");
		return FALSE;
	}

	counters = XSyncListSystemCounters (GDK_DISPLAY (), &ncounters);
	for (i = 0; i < ncounters; i++) {
		if (counters[i].name != NULL &&
		    g_strcmp0 (counters[i].name, "IDLETIME") == 0) {
			idletime->priv->counter = counters[i].counter;
			break;
		}
	}
	XSyncFreeSystemCounterList (counters);

	if (idletime->priv->counter == None) {
		egg_warning ("EggIdletime: IDLETIME counter not found");
		return FALSE;
	}

	gdk_window_add_filter (NULL, (GdkFilterFunc) egg_idletime_xevent_filter, idletime);

	return TRUE;
}

/**
 * egg_idletime_get_next_watch_serial:
 */
static guint32
egg_idletime_get_next_watch_serial (void)
{
	guint32 serial;

	serial = watch_serial++;
	/* cope with overflow */
	if ((gint32)watch_serial < 0)
		watch_serial = 1;
	return serial;
}

/**
 * egg_idletime_watch_new:
 */
static EggIdletimeWatch *
egg_idletime_watch_new (guint interval)
{
	EggIdletimeWatch *watch;

	watch = g_slice_new0 (EggIdletimeWatch);
	watch->interval = egg_idletime_int64_to_xsyncvalue ((gint64)interval);
	watch->id = egg_idletime_get_next_watch_serial ();
	watch->xalarm_positive = None;
	watch->xalarm_negative = None;

	return watch;
}

/**
 * egg_idletime_watch_free:
 */
static void
egg_idletime_watch_free (EggIdletimeWatch *watch)
{
	if (watch == NULL)
		return;
	if (watch->xalarm_positive != None)
		XSyncDestroyAlarm (GDK_DISPLAY (), watch->xalarm_positive);
	if (watch->xalarm_negative != None)
		XSyncDestroyAlarm (GDK_DISPLAY (), watch->xalarm_negative);
	g_slice_free (EggIdletimeWatch, watch);
}

/**
 * egg_idletime_xsync_alarm_set:
 */
static gboolean
egg_idletime_xsync_alarm_set (EggIdletime *idletime, EggIdletimeWatch *watch)
{
	XSyncAlarmAttributes attr;
	XSyncValue delta;
	guint flags;

	flags = XSyncCACounter
		| XSyncCAValueType
		| XSyncCATestType
		| XSyncCAValue
		| XSyncCADelta
		| XSyncCAEvents;

	XSyncIntToValue (&delta, 0);
	attr.trigger.counter = idletime->priv->counter;
	attr.trigger.value_type = XSyncAbsolute;
	attr.trigger.wait_value = watch->interval;
	attr.delta = delta;
	attr.events = TRUE;

	attr.trigger.test_type = XSyncPositiveTransition;
	if (watch->xalarm_positive != None) {
		egg_debug ("EggIdletime: updating alarm for positive transition wait=%" G_GINT64_FORMAT,
			   egg_idletime_xsyncvalue_to_int64 (attr.trigger.wait_value));
		XSyncChangeAlarm (GDK_DISPLAY (), watch->xalarm_positive, flags, &attr);
	} else {
		egg_debug ("EggIdletime: creating new alarm for positive transition wait=%" G_GINT64_FORMAT,
			   egg_idletime_xsyncvalue_to_int64 (attr.trigger.wait_value));
		watch->xalarm_positive = XSyncCreateAlarm (GDK_DISPLAY (), flags, &attr);
	}

	attr.trigger.test_type = XSyncNegativeTransition;
	if (watch->xalarm_negative != None) {
		egg_debug ("EggIdletime: updating alarm for negative transition wait=%" G_GINT64_FORMAT,
			   egg_idletime_xsyncvalue_to_int64 (attr.trigger.wait_value));
		XSyncChangeAlarm (GDK_DISPLAY (), watch->xalarm_negative, flags, &attr);
	} else {
		egg_debug ("EggIdletime: creating new alarm for negative transition wait=%" G_GINT64_FORMAT,
			   egg_idletime_xsyncvalue_to_int64 (attr.trigger.wait_value));
		watch->xalarm_negative = XSyncCreateAlarm (GDK_DISPLAY (), flags, &attr);
	}

	return TRUE;
}

/**
 * egg_idletime_add_watch:
 */
guint
egg_idletime_add_watch (EggIdletime *idletime, guint interval)
{
	EggIdletimeWatch *watch;

	g_return_val_if_fail (EGG_IS_IDLETIME (idletime), 0);

	watch = egg_idletime_watch_new (interval);

	egg_idletime_xsync_alarm_set (idletime, watch);

	g_hash_table_insert (idletime->priv->watches,
			     GUINT_TO_POINTER (watch->id), watch);
	return watch->id;
}

/**
 * egg_idletime_remove_watch:
 */
void
egg_idletime_remove_watch (EggIdletime *idletime, guint id)
{
	g_return_if_fail (EGG_IS_IDLETIME (idletime));

	g_hash_table_remove (idletime->priv->watches, GUINT_TO_POINTER (id));
}

/**
 * egg_idletime_finalize:
 */
static void
egg_idletime_finalize (GObject *object)
{
	EggIdletime *idletime;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EGG_IS_IDLETIME (object));

	idletime = EGG_IDLETIME (object);

	g_return_if_fail (idletime->priv != NULL);

	G_OBJECT_CLASS (egg_idletime_parent_class)->finalize (object);
}

/**
 * egg_idletime_constructor:
 */
static GObject *
egg_idletime_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
	EggIdletime *idletime;

	idletime = EGG_IDLETIME (G_OBJECT_CLASS (egg_idletime_parent_class)->constructor (type, n_construct_properties, construct_properties));

	if (!egg_idletime_init_xsync (idletime)) {
		g_object_unref (idletime);
		return NULL;
	}

	return G_OBJECT (idletime);
}

/**
 * egg_idletime_dispose:
 */
static void
egg_idletime_dispose (GObject *object)
{
	EggIdletime *idletime;

	g_return_if_fail (EGG_IS_IDLETIME (object));

	idletime = EGG_IDLETIME (object);

	if (idletime->priv->watches != NULL) {
		g_hash_table_destroy (idletime->priv->watches);
		idletime->priv->watches = NULL;
	}

	G_OBJECT_CLASS (egg_idletime_parent_class)->dispose (object);
}

/**
 * egg_idletime_class_init:
 */
static void
egg_idletime_class_init (EggIdletimeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = egg_idletime_finalize;
	object_class->dispose = egg_idletime_dispose;
	object_class->constructor = egg_idletime_constructor;

	signals [SIGNAL_ALARM_EXPIRED] =
		g_signal_new ("alarm-expired",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EggIdletimeClass, alarm_expired),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [SIGNAL_RESET] =
		g_signal_new ("reset",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EggIdletimeClass, reset),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (EggIdletimePrivate));
}

/**
 * egg_idletime_init:
 */
static void
egg_idletime_init (EggIdletime *idletime)
{
	idletime->priv = EGG_IDLETIME_GET_PRIVATE (idletime);
	idletime->priv->counter = None;
	idletime->priv->watches = g_hash_table_new_full (NULL, NULL, NULL,
							(GDestroyNotify) egg_idletime_watch_free);
}

/**
 * egg_idletime_new:
 */
EggIdletime *
egg_idletime_new (void)
{
	GObject *idletime;
	idletime = g_object_new (EGG_TYPE_IDLETIME, NULL);
	return EGG_IDLETIME (idletime);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

static guint _last_alarm = 0;

static void
egg_idletime_test_alarm_expired_cb (EggIdletime *idletime, guint alarm, EggTest *test)
{
	_last_alarm = alarm;
	egg_debug ("alarm %i", alarm);
	egg_test_loop_quit (test);
}

static void
egg_idletime_test_reset_cb (EggIdletime *idletime, EggTest *test)
{
	_last_alarm = 0;
	egg_debug ("reset");
	egg_test_loop_quit (test);
}

void
egg_idletime_test (gpointer data)
{
	EggIdletime *idletime;
	EggTest *test = (EggTest *) data;
	guint id;

	if (egg_test_start (test, "EggIdletime") == FALSE)
		return;

	gdk_init (NULL, NULL);

	/************************************************************/
	egg_test_title (test, "make sure we get an object");
	idletime = egg_idletime_new ();
	egg_test_assert (test, (idletime != NULL));
	g_signal_connect (idletime, "alarm-expired",
			  G_CALLBACK (egg_idletime_test_alarm_expired_cb), test);
	g_signal_connect (idletime, "reset",
			  G_CALLBACK (egg_idletime_test_reset_cb), test);

	/************************************************************/
	egg_test_title (test, "add a watch");
	id = egg_idletime_add_watch (idletime, 3000);
	egg_test_assert (test, (id != 0));

	/************************************************************/
	g_print ("*****************************\n");
	g_print ("*** DO NOT MOVE THE MOUSE ***\n");
	g_print ("*****************************\n");
	egg_test_loop_wait (test, 4500);
	egg_test_loop_check (test);

	/************************************************************/
	egg_test_title (test, "check condition");
	egg_test_assert (test, (_last_alarm == 1));

	/************************************************************/
	g_print ("*****************************\n");
	g_print ("***    MOVE THE MOUSE     ***\n");
	g_print ("*****************************\n");
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	/************************************************************/
	egg_test_title (test, "check condition");
	egg_test_assert (test, (_last_alarm == 0));

	/************************************************************/
	g_print ("*****************************\n");
	g_print ("*** DO NOT MOVE THE MOUSE ***\n");
	g_print ("*****************************\n");
	egg_test_loop_wait (test, 4500);
	egg_test_loop_check (test);

	/************************************************************/
	egg_test_title (test, "check condition");
	egg_test_assert (test, (_last_alarm == 1));

	/************************************************************/
	g_print ("*****************************\n");
	g_print ("***    MOVE THE MOUSE     ***\n");
	g_print ("*****************************\n");
	egg_test_loop_wait (test, 10000);
	egg_test_loop_check (test);

	/************************************************************/
	egg_test_title (test, "check condition");
	egg_test_assert (test, (_last_alarm == 0));

	g_object_unref (idletime);

	egg_test_end (test);
}

#endif

