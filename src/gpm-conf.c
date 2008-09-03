/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gconf/gconf-client.h>

#include "gpm-marshal.h"
#include "gpm-conf.h"
#include "egg-debug.h"

static void     gpm_conf_class_init (GpmConfClass *klass);
static void     gpm_conf_init       (GpmConf      *conf);
static void     gpm_conf_finalize   (GObject	*object);

#define GPM_CONF_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CONF, GpmConfPrivate))

struct GpmConfPrivate
{
	GConfClient	*gconf_client;
};

enum {
	VALUE_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_conf_object = NULL;

G_DEFINE_TYPE (GpmConf, gpm_conf, G_TYPE_OBJECT)

/**
 * gpm_conf_get_bool:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_conf_get_bool (GpmConf     *conf,
		   const gchar *key,
		   gboolean    *value)
{
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	*value = gconf_client_get_bool (conf->priv->gconf_client, key, &error);
	if (error) {
		egg_debug ("Error: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	return ret;
}

/**
 * gpm_conf_get_string:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 *
 * You must g_free () the return value.
 **/
gboolean
gpm_conf_get_string (GpmConf     *conf,
		     const gchar *key,
		     gchar      **value)
{
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	*value = gconf_client_get_string (conf->priv->gconf_client, key, &error);
	if (error) {
		egg_debug ("Error: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	return ret;
}

/**
 * gpm_conf_get_int:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_conf_get_int (GpmConf     *conf,
		  const gchar *key,
		  gint        *value)
{
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	*value = gconf_client_get_int (conf->priv->gconf_client, key, &error);
	if (error) {
		egg_debug ("Error: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	return ret;
}

/**
 * gpm_conf_get_int:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_conf_get_uint (GpmConf     *conf,
		   const gchar *key,
		   guint       *value)
{
	gboolean ret = TRUE;
	gint tvalue;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	/* bodge */
	ret = gpm_conf_get_int (conf, key, &tvalue);
	*value = (guint) tvalue;
	return ret;
}

/**
 * gpm_conf_set_bool:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_conf_set_bool (GpmConf     *conf,
		   const gchar *key,
		   gboolean     value)
{
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	ret = gconf_client_key_is_writable (conf->priv->gconf_client, key, NULL);
	if (!ret) {
		egg_debug ("%s not writable", key);
		goto out;
	}
	gconf_client_set_bool (conf->priv->gconf_client, key, value, &error);
	if (error) {
		egg_debug ("Error: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}
out:
	return ret;
}

/**
 * gpm_conf_set_string:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 *
 * You must g_free () the return value.
 **/
gboolean
gpm_conf_set_string (GpmConf     *conf,
		     const gchar *key,
		     const gchar *value)
{
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	ret = gconf_client_key_is_writable (conf->priv->gconf_client, key, NULL);
	if (!ret) {
		egg_debug ("%s not writable", key);
		goto out;
	}
	gconf_client_set_string (conf->priv->gconf_client, key, value, &error);
	if (error) {
		egg_debug ("Error: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}
out:
	return ret;
}

/**
 * gpm_conf_set_int:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_conf_set_int (GpmConf     *conf,
		  const gchar *key,
		  gint         value)
{
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	ret = gconf_client_key_is_writable (conf->priv->gconf_client, key, NULL);
	if (!ret) {
		egg_debug ("%s not writable", key);
		goto out;
	}
	gconf_client_set_int (conf->priv->gconf_client, key, value, &error);
	if (error) {
		egg_debug ("Error: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}
out:
	return ret;
}

/**
 * gpm_conf_set_int:
 *
 * @conf: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_conf_set_uint (GpmConf     *conf,
		   const gchar *key,
		   guint        value)
{
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	/* bodge */
	ret = gpm_conf_set_int (conf, key, (gint) value);
	return ret;
}

/**
 * gpm_conf_is_writable:
 *
 * @conf: This class instance
 * @key: The key to query
 * @writable: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_conf_is_writable (GpmConf     *conf,
		      const gchar *key,
		      gboolean    *writable)
{
	GError *error = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_CONF (conf), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (writable != NULL, FALSE);

	*writable = gconf_client_key_is_writable (conf->priv->gconf_client, key, &error);
	if (error) {
		egg_debug ("Error: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	return ret;
}

/**
 * gconf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
gconf_key_changed_cb (GConfClient *client,
		      guint	   cnxn_id,
		      GConfEntry  *entry,
		      gpointer	   user_data)
{
	GpmConf    *conf = GPM_CONF (user_data);
	if (gconf_entry_get_value (entry) == NULL) {
		return;
	}

	egg_debug ("emitting value-changed : '%s'", entry->key);
	g_signal_emit (conf, signals [VALUE_CHANGED], 0, entry->key);
}

/**
 * gpm_conf_class_init:
 * @klass: This class instance
 **/
static void
gpm_conf_class_init (GpmConfClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_conf_finalize;
	g_type_class_add_private (klass, sizeof (GpmConfPrivate));

	signals [VALUE_CHANGED] =
		g_signal_new ("value-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmConfClass, value_changed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
}

/**
 * gpm_conf_init:
 *
 * @conf: This class instance
 **/
static void
gpm_conf_init (GpmConf *conf)
{
	conf->priv = GPM_CONF_GET_PRIVATE (conf);

	conf->priv->gconf_client = gconf_client_get_default ();

	/* watch gnome-power-manager keys */
	gconf_client_add_dir (conf->priv->gconf_client,
			      GPM_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	gconf_client_notify_add (conf->priv->gconf_client,
				 GPM_CONF_DIR,
				 gconf_key_changed_cb,
				 conf, NULL, NULL);

	/* watch gnome-screensaver keys */
	gconf_client_add_dir (conf->priv->gconf_client,
			      GS_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	gconf_client_notify_add (conf->priv->gconf_client,
				 GS_CONF_DIR,
				 gconf_key_changed_cb,
				 conf, NULL, NULL);
}

/**
 * gpm_conf_finalize:
 * @object: This class instance
 **/
static void
gpm_conf_finalize (GObject *object)
{
	GpmConf *conf;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CONF (object));

	conf = GPM_CONF (object);
	conf->priv = GPM_CONF_GET_PRIVATE (conf);

	g_object_unref (conf->priv->gconf_client);

	G_OBJECT_CLASS (gpm_conf_parent_class)->finalize (object);
}

/**
 * gpm_conf_new:
 * Return value: new GpmConf instance.
 **/
GpmConf *
gpm_conf_new (void)
{
	if (gpm_conf_object != NULL) {
		g_object_ref (gpm_conf_object);
	} else {
		gpm_conf_object = g_object_new (GPM_TYPE_CONF, NULL);
		g_object_add_weak_pointer (gpm_conf_object, &gpm_conf_object);
	}
	return GPM_CONF (gpm_conf_object);
}

