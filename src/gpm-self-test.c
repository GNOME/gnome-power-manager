/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <math.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "egg-debug.h"

#include "gpm-screensaver.h"
#include "gpm-dpms.h"
#include "gpm-phone.h"
#include "gpm-idle.h"
#include "gpm-common.h"
#include "gpm-idletime.h"
#include "gpm-array-float.h"


/** ver:1.0 ***********************************************************/
static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

static gboolean
_g_test_hang_check_cb (gpointer user_data)
{
	guint timeout_ms = *((guint*) user_data);
	g_main_loop_quit (_test_loop);
	g_warning ("loop not completed in %ims", timeout_ms);
	g_assert_not_reached ();
	return FALSE;
}

/**
 * _g_test_loop_run_with_timeout:
 **/
static void
_g_test_loop_run_with_timeout (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, _g_test_hang_check_cb, &timeout_ms);
	g_main_loop_run (_test_loop);
}

#if 0
static gboolean
_g_test_hang_wait_cb (gpointer user_data)
{
	g_main_loop_quit (_test_loop);
	_test_loop_timeout_id = 0;
	return FALSE;
}

/**
 * _g_test_loop_wait:
 **/
static void
_g_test_loop_wait (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, _g_test_hang_wait_cb, &timeout_ms);
	g_main_loop_run (_test_loop);
}
#endif

/**
 * _g_test_loop_quit:
 **/
static void
_g_test_loop_quit (void)
{
	if (_test_loop_timeout_id > 0) {
		g_source_remove (_test_loop_timeout_id);
		_test_loop_timeout_id = 0;
	}
	if (_test_loop != NULL) {
		g_main_loop_quit (_test_loop);
		g_main_loop_unref (_test_loop);
		_test_loop = NULL;
	}
}

/**********************************************************************/

static void
gpm_test_dpms_func (void)
{
	GpmDpms *dpms;
	gboolean ret;
	GError *error = NULL;

	dpms = gpm_dpms_new ();
	g_assert (dpms != NULL);

	/* set on */
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_ON, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_usleep (2*1000*1000);

	/* set STANDBY */
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_STANDBY, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_usleep (2*1000*1000);

	/* set SUSPEND */
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_SUSPEND, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_usleep (2*1000*1000);

	/* set OFF */
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_OFF, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_usleep (2*1000*1000);

	/* set on */
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_ON, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_usleep (2*1000*1000);

	g_object_unref (dpms);
}

static GpmIdleMode _mode = 0;

static void
gpm_test_idle_func_idle_changed_cb (GpmIdle *idle, GpmIdleMode mode, gpointer user_data)
{
	_mode = mode;
	egg_debug ("idle-changed %s", gpm_idle_mode_to_string (mode));
	_g_test_loop_quit ();
}

static void
gpm_test_idle_func (void)
{
	GpmIdle *idle;
	gboolean ret;
	GpmDpms *dpms;

	idle = gpm_idle_new ();
	g_assert (idle != NULL);

	/* set up defaults */
	gpm_idle_set_check_cpu (idle, FALSE);
	gpm_idle_set_timeout_dim (idle, 4);
	gpm_idle_set_timeout_blank (idle, 5);
	gpm_idle_set_timeout_sleep (idle, 15);
	g_signal_connect (idle, "idle-changed",
			  G_CALLBACK (gpm_test_idle_func_idle_changed_cb), NULL);

	/* check normal at startup */
	g_assert_cmpint (gpm_idle_get_mode (idle), ==, GPM_IDLE_MODE_NORMAL);

	g_print ("*****************************\n");
	g_print ("*** DO NOT MOVE THE MOUSE ***\n");
	g_print ("*****************************\n");
	_g_test_loop_run_with_timeout (2000 + 10000);

	/* check callback mode */
	g_assert_cmpint (_mode, ==, GPM_IDLE_MODE_DIM);

	/* check current mode */
	g_assert_cmpint (gpm_idle_get_mode (idle), ==, GPM_IDLE_MODE_DIM);

	_g_test_loop_run_with_timeout (5000 + 1000);

	/* check callback mode */
	g_assert_cmpint (_mode, ==, GPM_IDLE_MODE_BLANK);

	/* check current mode */
	g_assert_cmpint (gpm_idle_get_mode (idle), ==, GPM_IDLE_MODE_BLANK);

	g_print ("**********************\n");
	g_print ("*** MOVE THE MOUSE ***\n");
	g_print ("**********************\n");
	_g_test_loop_run_with_timeout (G_MAXUINT);

	/* check callback mode */
	g_assert_cmpint (_mode, ==, GPM_IDLE_MODE_NORMAL);

	/* check current mode */
	g_assert_cmpint (gpm_idle_get_mode (idle), ==, GPM_IDLE_MODE_NORMAL);

	g_print ("*****************************\n");
	g_print ("*** DO NOT MOVE THE MOUSE ***\n");
	g_print ("*****************************\n");
	_g_test_loop_run_with_timeout (4000 + 1500);

	/* check current mode */
	g_assert_cmpint (gpm_idle_get_mode (idle), ==, GPM_IDLE_MODE_DIM);

	_g_test_loop_run_with_timeout (15000);

	/* check current mode */
	g_assert_cmpint (gpm_idle_get_mode (idle), ==, GPM_IDLE_MODE_BLANK);

	/* set dpms off */
	dpms = gpm_dpms_new ();
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_OFF, NULL);
	g_assert (ret);

	/* wait for normal event to be suppressed */
//	g_timeout_add (2000, (GSourceFunc) gpm_test_idle_func_delay_cb, NULL);
	_g_test_loop_run_with_timeout (G_MAXUINT);

	/* check current mode */
	g_assert_cmpint (gpm_idle_get_mode (idle), ==, GPM_IDLE_MODE_BLANK);

	gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_ON, NULL);

	g_object_unref (idle);
	g_object_unref (dpms);
}

static gboolean _test_got_refresh = FALSE;

static void
gpm_test_phone_device_refresh_cb (GpmPhone *phone, guint idx, gpointer *data)
{
	g_debug ("idx refresh = %i", idx);
	if (idx == 0 && GPOINTER_TO_UINT (data) == 44)
		_test_got_refresh = TRUE;
}

static void
gpm_test_phone_func (void)
{
	GpmPhone *phone;
	guint value;
	gboolean ret;

	/* make sure we get a non null phone */
	phone = gpm_phone_new ();
	g_assert (phone != NULL);

	/* connect signals */
	g_signal_connect (phone, "device-refresh",
			  G_CALLBACK (gpm_test_phone_device_refresh_cb), GUINT_TO_POINTER(44));

	/* coldplug the data */
	ret = gpm_phone_coldplug (phone);
	g_assert (ret);

	_g_test_loop_run_with_timeout (500);

	/* got refresh */
	g_assert (_test_got_refresh);

	/* check the connected phones */
	value = gpm_phone_get_num_batteries (phone);
	g_assert_cmpint (value, ==, 1);

	/* check the present value */
	ret = gpm_phone_get_present (phone, 0);
	g_assert (ret);

	/* check the percentage */
	value = gpm_phone_get_percentage (phone, 0);
	g_assert_cmpint (value, !=, 0);

	/* check the ac value */
	ret = gpm_phone_get_on_ac (phone, 0);
	g_assert (ret);
//out:
	g_object_unref (phone);
}

#if 0
static void
gpm_test_screensaver_func_auth_request_cb (GpmScreensaver *screensaver, gboolean auth, gpointer user_data)
{
	egg_debug ("auth request = %i", auth);
	test_got_request = auth;

	_g_test_loop_quit ();
}
#endif

static void
gpm_test_screensaver_func (void)
{
	GpmScreensaver *screensaver;
//	guint value;
	gboolean ret;

	screensaver = gpm_screensaver_new ();
	g_assert (screensaver != NULL);

#if 0
	/* connect signals */
	g_signal_connect (screensaver, "auth-request",
			  G_CALLBACK (gpm_test_screensaver_func_auth_request_cb), NULL);
#endif
	/* lock */
	ret = gpm_screensaver_lock (screensaver);
	g_assert (ret);

	/* poke */
	ret = gpm_screensaver_poke (screensaver);
	g_assert (ret);

	g_object_unref (screensaver);
}

static void
gpm_test_precision_func (void)
{
	g_assert_cmpint (gpm_precision_round_down (0, 10), ==, 0);
	g_assert_cmpint (gpm_precision_round_down (4, 10), ==, 0);
	g_assert_cmpint (gpm_precision_round_down (11, 10), ==, 10);
	g_assert_cmpint (gpm_precision_round_down (201, 2), ==, 200);
	g_assert_cmpint (gpm_precision_round_down (100, 10), ==, 100);
	g_assert_cmpint (gpm_precision_round_up (0, 10), ==, 0);
	g_assert_cmpint (gpm_precision_round_up (4, 10), ==, 10);
	g_assert_cmpint (gpm_precision_round_up (11, 10), ==, 20);
	g_assert_cmpint (gpm_precision_round_up (201, 2), ==, 202);
	g_assert_cmpint (gpm_precision_round_up (100, 10), ==, 100);
}

static void
gpm_test_discrete_func (void)
{
	gfloat fvalue;

	/* convert discrete levels */
	g_assert_cmpint (gpm_discrete_to_percent (0, 10), ==, 0);
	g_assert_cmpint (gpm_discrete_to_percent (9, 10), ==, 100);

	/* convert discrete 20/10 levels */
	g_assert_cmpint (gpm_discrete_to_percent (20, 10), ==, 100);

	/* convert discrete 0/10 levels */
	fvalue = gpm_discrete_to_fraction (0, 10);
	g_assert_cmpfloat (fvalue, >, -0.01);
	g_assert_cmpfloat (fvalue, <, 0.01);

	/* convert discrete 9/10 levels */
	fvalue = gpm_discrete_to_fraction (9, 10);
	g_assert_cmpfloat (fvalue, >, -1.01);
	g_assert_cmpfloat (fvalue, <, 1.01);
}

static void
gpm_test_color_func (void)
{
	guint8 r, g, b;
	guint32 color;

	/* get red */
	gpm_color_to_rgb (0xff0000, &r, &g, &b);
	g_assert_cmpint (r, ==, 255);
	g_assert_cmpint (g, ==, 0);
	g_assert_cmpint (b, ==, 0);

	/* get green */
	gpm_color_to_rgb (0x00ff00, &r, &g, &b);
	g_assert_cmpint (r, ==, 0);
	g_assert_cmpint (g, ==, 255);
	g_assert_cmpint (b, ==, 0);

	/* get blue */
	gpm_color_to_rgb (0x0000ff, &r, &g, &b);
	g_assert_cmpint (r, ==, 0);
	g_assert_cmpint (g, ==, 0);
	g_assert_cmpint (b, ==, 255);

	/* get black */
	gpm_color_to_rgb (0x000000, &r, &g, &b);
	g_assert_cmpint (r, ==, 0);
	g_assert_cmpint (g, ==, 0);
	g_assert_cmpint (b, ==, 0);

	/* get white */
	gpm_color_to_rgb (0xffffff, &r, &g, &b);
	g_assert_cmpint (r, ==, 255);
	g_assert_cmpint (g, ==, 255);
	g_assert_cmpint (b, ==, 255);

	/* set red */
	color = gpm_color_from_rgb (0xff, 0x00, 0x00);
	g_assert_cmpint (color, ==, 0xff0000);

	/* set green */
	color = gpm_color_from_rgb (0x00, 0xff, 0x00);
	g_assert_cmpint (color, ==, 0x00ff00);

	/* set blue */
	color = gpm_color_from_rgb (0x00, 0x00, 0xff);
	g_assert_cmpint (color, ==, 0x0000ff);

	/* set white */
	color = gpm_color_from_rgb (0xff, 0xff, 0xff);
	g_assert_cmpint (color, ==, 0xffffff);
}


static void
gpm_test_array_float_func (void)
{
	GpmArrayFloat *array;
	GpmArrayFloat *kernel;
	GpmArrayFloat *result;
	gfloat value;
	gfloat sigma;
	guint size;

	/* make sure we get a non null array */
	array = gpm_array_float_new (10);
	g_assert (array != NULL);

	gpm_array_float_print (array);
	gpm_array_float_free (array);

	/* make sure we get the correct length array */
	array = gpm_array_float_new (10);
	g_assert_cmpint (array->len, ==, 10);

	/* make sure we get the correct array sum */
	value = gpm_array_float_sum (array);
	g_assert_cmpfloat (value, ==, 0.0f);

	/* remove outliers */
	gpm_array_float_set (array, 0, 30.0);
	gpm_array_float_set (array, 1, 29.0);
	gpm_array_float_set (array, 2, 31.0);
	gpm_array_float_set (array, 3, 33.0);
	gpm_array_float_set (array, 4, 100.0);
	gpm_array_float_set (array, 5, 27.0);
	gpm_array_float_set (array, 6, 30.0);
	gpm_array_float_set (array, 7, 29.0);
	gpm_array_float_set (array, 8, 31.0);
	gpm_array_float_set (array, 9, 30.0);
	kernel = gpm_array_float_remove_outliers (array, 3, 10.0);
	g_assert (kernel != NULL);
	g_assert_cmpint (kernel->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (kernel);

	/* make sure we removed the outliers */
	value = gpm_array_float_sum (kernel);
	g_assert_cmpfloat (fabs(value - 30*10), <, 1.0f);
	gpm_array_float_free (kernel);

	/* remove outliers step */
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 10.0);
	gpm_array_float_set (array, 8, 20.0);
	gpm_array_float_set (array, 9, 50.0);
	kernel = gpm_array_float_remove_outliers (array, 3, 20.0);
	g_assert (kernel != NULL);
	g_assert_cmpint (kernel->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (kernel);

	/* make sure we removed the outliers */
	value = gpm_array_float_sum (kernel);
	g_assert_cmpfloat (fabs(value - 80), <, 1.0f);
	gpm_array_float_free (kernel);

	/* get gaussian 0.0, sigma 1.1 */
	value = gpm_array_float_guassian_value (0.0, 1.1);
	g_assert_cmpfloat (fabs (value - 0.36267), <, 0.0001f);

	/* get gaussian 0.5, sigma 1.1 */
	value = gpm_array_float_guassian_value (0.5, 1.1);
	g_assert_cmpfloat (fabs (value - 0.32708), <, 0.0001f);

	/* get gaussian 1.0, sigma 1.1 */
	value = gpm_array_float_guassian_value (1.0, 1.1);
	g_assert_cmpfloat (fabs (value - 0.23991), <, 0.0001f);

	/* get gaussian 0.5, sigma 4.5 */
	value = gpm_array_float_guassian_value (0.5, 4.5);
	g_assert_cmpfloat (fabs (value - 0.088108), <, 0.0001f);

	size = 5;
	sigma = 1.1;
	/* get inprecise gaussian array */
	kernel = gpm_array_float_compute_gaussian (size, sigma);
	g_assert (kernel == NULL);

	size = 9;
	sigma = 1.1;
	/* get gaussian-9 array */
	kernel = gpm_array_float_compute_gaussian (size, sigma);
	g_assert (kernel != NULL);
	g_assert_cmpint (kernel->len, ==, size);
	gpm_array_float_print (kernel);

	/* make sure we get an accurate gaussian */
	value = gpm_array_float_sum (kernel);
	g_assert_cmpfloat (fabs(value - 1.0), <, 0.01f);

	/* make sure we get get and set */
	gpm_array_float_set (array, 4, 100.0);
	value = gpm_array_float_get (array, 4);
	g_assert_cmpfloat (value, ==, 100.0f);
	gpm_array_float_print (array);

	/* make sure we get the correct array sum (2) */
	gpm_array_float_set (array, 0, 20.0);
	gpm_array_float_set (array, 1, 44.0);
	gpm_array_float_set (array, 2, 45.0);
	gpm_array_float_set (array, 3, 89.0);
	gpm_array_float_set (array, 4, 100.0);
	gpm_array_float_set (array, 5, 12.0);
	gpm_array_float_set (array, 6, 76.0);
	gpm_array_float_set (array, 7, 78.0);
	gpm_array_float_set (array, 8, 1.20);
	gpm_array_float_set (array, 9, 3.0);
	value = gpm_array_float_sum (array);
	g_assert_cmpfloat (fabs (value - 468.2), <, 0.0001f);

	/* test convolving with kernel #1 */
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 100.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 0.0);
	gpm_array_float_set (array, 8, 0.0);
	gpm_array_float_set (array, 9, 0.0);
	result = gpm_array_float_convolve (array, kernel);
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #1 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 5.0);
	gpm_array_float_free (result);

	/* test convolving with kernel #2 */
	gpm_array_float_set (array, 0, 100.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 0.0);
	gpm_array_float_set (array, 8, 0.0);
	gpm_array_float_set (array, 9, 0.0);
	result = gpm_array_float_convolve (array, kernel);
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #2 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 10.0f);
	gpm_array_float_free (result);

	/* test convolving with kernel #3 */
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 0.0);
	gpm_array_float_set (array, 8, 0.0);
	gpm_array_float_set (array, 9, 100.0);
	result = gpm_array_float_convolve (array, kernel);
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #3 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 10.0f);
	gpm_array_float_free (result);

	/* test convolving with kernel #4 */
	gpm_array_float_set (array, 0, 10.0);
	gpm_array_float_set (array, 1, 10.0);
	gpm_array_float_set (array, 2, 10.0);
	gpm_array_float_set (array, 3, 10.0);
	gpm_array_float_set (array, 4, 10.0);
	gpm_array_float_set (array, 5, 10.0);
	gpm_array_float_set (array, 6, 10.0);
	gpm_array_float_set (array, 7, 10.0);
	gpm_array_float_set (array, 8, 10.0);
	gpm_array_float_set (array, 9, 10.0);
	result = gpm_array_float_convolve (array, kernel);
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #4 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 1.0f);

	/* test convolving with kernel #5 */
	gpm_array_float_set (array, 0, 10.0);
	gpm_array_float_set (array, 1, 10.0);
	gpm_array_float_set (array, 2, 10.0);
	gpm_array_float_set (array, 3, 10.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 10.0);
	gpm_array_float_set (array, 6, 10.0);
	gpm_array_float_set (array, 7, 10.0);
	gpm_array_float_set (array, 8, 10.0);
	gpm_array_float_set (array, 9, 10.0);
	result = gpm_array_float_convolve (array, kernel);
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #5 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 90.0), <, 1.0f);

	/* integration down */
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 1.0);
	gpm_array_float_set (array, 2, 2.0);
	gpm_array_float_set (array, 3, 3.0);
	gpm_array_float_set (array, 4, 4.0);
	gpm_array_float_set (array, 5, 5.0);
	gpm_array_float_set (array, 6, 6.0);
	gpm_array_float_set (array, 7, 7.0);
	gpm_array_float_set (array, 8, 8.0);
	gpm_array_float_set (array, 9, 9.0);
	size = gpm_array_float_compute_integral (array, 0, 4);
	g_assert_cmpint (size, ==, 0+1+2+3+4);

	/* integration up */
	size = gpm_array_float_compute_integral (array, 5, 9);
	g_assert_cmpint (size, ==, 5+6+7+8+9);

	/* integration all */
	size = gpm_array_float_compute_integral (array, 0, 9);
	g_assert_cmpint (size, ==, 0+1+2+3+4+5+6+7+8+9);

	/* average */
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 1.0);
	gpm_array_float_set (array, 2, 2.0);
	gpm_array_float_set (array, 3, 3.0);
	gpm_array_float_set (array, 4, 4.0);
	gpm_array_float_set (array, 5, 5.0);
	gpm_array_float_set (array, 6, 6.0);
	gpm_array_float_set (array, 7, 7.0);
	gpm_array_float_set (array, 8, 8.0);
	gpm_array_float_set (array, 9, 9.0);
	value = gpm_array_float_get_average (array);
	g_assert_cmpfloat (value, ==, 4.5);

	gpm_array_float_free (result);
	gpm_array_float_free (array);
	gpm_array_float_free (kernel);
}


static void
gpm_test_idletime_wait (guint time_ms)
{
	GTimer *ltimer = g_timer_new ();
	gfloat goal = time_ms / (gfloat) 1000.0f;
	do {
		g_main_context_iteration (NULL, FALSE);
	} while (g_timer_elapsed (ltimer, NULL) < goal);
	g_timer_destroy (ltimer);
}

static guint last_alarm = 0;
static guint event_time;
GTimer *timer;

static void
gpm_alarm_expired_cb (GpmIdletime *idletime, guint alarm_id, gpointer data)
{
	last_alarm = alarm_id;
	event_time = g_timer_elapsed (timer, NULL) * (gfloat) 1000.0f;
//	g_print ("[evt %i in %ims]\n", alarm_id, event_time);
}

static void
wait_until_alarm (void)
{
	g_print ("*****************************\n");
	g_print ("*** DO NOT MOVE THE MOUSE ***\n");
	g_print ("*****************************\n");
	while (last_alarm == 0)
		g_main_context_iteration (NULL, FALSE);
}

static void
wait_until_reset (void)
{
	if (last_alarm == 0)
		return;
	g_print ("*****************************\n");
	g_print ("***     MOVE THE MOUSE    ***\n");
	g_print ("*****************************\n");
	while (last_alarm != 0)
		g_main_context_iteration (NULL, FALSE);
	gpm_test_idletime_wait (1000);
}

static void
gpm_test_idletime_func (void)
{
	GpmIdletime *idletime;
	gboolean ret;
	guint i;

	timer = g_timer_new ();
	gdk_init (NULL, NULL);

	/* warn */

	g_timer_start (timer);
	/* check to see if delay works as expected */
	gpm_test_idletime_wait (2000);
	event_time = g_timer_elapsed (timer, NULL) * (gfloat) 1000.0f;
	g_assert_cmpfloat (event_time, >, 1800);
	g_assert_cmpfloat (event_time, <, 2200);

	/* make sure we get a non null device */
	idletime = gpm_idletime_new ();
	g_assert (idletime != NULL);
	g_signal_connect (idletime, "alarm-expired",
			  G_CALLBACK (gpm_alarm_expired_cb), NULL);

	/* check if we are alarm zero with no alarms */
	g_assert_cmpint (last_alarm, ==, 0);

	/* check if we can set an reset alarm */
	ret = gpm_idletime_alarm_set (idletime, 0, 100);
	g_assert (!ret);

	/* check if we can set an alarm timeout of zero */
	ret = gpm_idletime_alarm_set (idletime, 999, 0);
	g_assert (!ret);

	g_timer_start (timer);
	/* check if we can set an alarm */
	ret = gpm_idletime_alarm_set (idletime, 101, 5000);
	g_assert (ret);

	gpm_idletime_alarm_set (idletime, 101, 5000);
	wait_until_alarm ();

	/* loop this two times */
	for (i=0; i<2; i++) {
		/* just let it time out, and wait for human input */
		wait_until_reset ();
		g_timer_start (timer);

			g_timer_start (timer);
		/* check if we can set an alarm */
		ret = gpm_idletime_alarm_set (idletime, 101, 5000);
		g_assert (ret);

		/* wait for alarm to go off */
		wait_until_alarm ();
		g_timer_start (timer);

		/* check if correct alarm has gone off */
		g_assert_cmpint (last_alarm, ==, 101);

		/* check if alarm has gone off in correct time */
		g_assert_cmpint (event_time, >, 3000);
		g_assert_cmpint (event_time, <, 6000);
	}

	/* just let it time out, and wait for human input */
	wait_until_reset ();
	g_timer_start (timer);

	g_timer_start (timer);
	/* check if we can set an existing alarm */
	ret = gpm_idletime_alarm_set (idletime, 101, 10000);
	g_assert (ret);

	/* wait for alarm to go off */
	wait_until_alarm ();
	g_timer_start (timer);

	/* check if alarm has gone off in the old time */
	g_assert_cmpint (event_time, >, 5000);

	/* check if we can remove an invalid alarm */
	ret = gpm_idletime_alarm_remove (idletime, 202);
	g_assert (!ret);

	/* check if we can remove an valid alarm */
	ret = gpm_idletime_alarm_remove (idletime, 101);
	g_assert (ret);

	g_timer_destroy (timer);
	g_object_unref (idletime);
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* needed for DPMS checks */
	gtk_init (&argc, &argv);

	/* tests go here */
	g_test_add_func ("/power/precision", gpm_test_precision_func);
	g_test_add_func ("/power/discrete", gpm_test_discrete_func);
	g_test_add_func ("/power/color", gpm_test_color_func);
	g_test_add_func ("/power/array_float", gpm_test_array_float_func);
	g_test_add_func ("/power/idle", gpm_test_idle_func);
	g_test_add_func ("/power/idletime", gpm_test_idletime_func);
	g_test_add_func ("/power/dpms", gpm_test_dpms_func);
	g_test_add_func ("/power/phone", gpm_test_phone_func);
//	g_test_add_func ("/power/graph-widget", gpm_graph_widget_test);
	g_test_add_func ("/power/screensaver", gpm_test_screensaver_func);

	return g_test_run ();
}

