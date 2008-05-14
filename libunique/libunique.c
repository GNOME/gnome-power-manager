/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <glib.h>

#if HAVE_UNIQUE
#include <unique/unique.h>
#endif

#include "libunique.h"

static void     libunique_class_init (LibUniqueClass *klass);
static void     libunique_init       (LibUnique      *unique);
static void     libunique_finalize   (GObject        *object);

#define LIBUNIQUE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), LIBUNIQUE_TYPE, LibUniquePrivate))

struct LibUniquePrivate
{
	gboolean		 dummy;
#if HAVE_UNIQUE
	UniqueApp		*uniqueapp;
#endif
};

enum {
	ACTIVATED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (LibUnique, libunique, G_TYPE_OBJECT)

#if HAVE_UNIQUE
/**
 * libunique_message_cb:
 **/
static void
libunique_message_cb (UniqueApp *app, UniqueCommand command, UniqueMessageData *message_data,
		      guint time, LibUnique *libunique)
{
	g_return_if_fail (IS_LIBUNIQUE (libunique));
	if (command == UNIQUE_ACTIVATE) {
		g_signal_emit (libunique, signals [ACTIVATED], 0);
	}
}

/**
 * libunique_assign:
 * @libunique: This class instance
 * @service: The service name
 * Return value: %FALSE if we should exit as another instance is running
 **/
gboolean
libunique_assign (LibUnique *libunique, const gchar *service)
{
	g_return_val_if_fail (IS_LIBUNIQUE (libunique), FALSE);
	g_return_val_if_fail (service != NULL, FALSE);

	if (libunique->priv->uniqueapp != NULL) {
		g_warning ("already assigned!");
		return FALSE;
	}

	/* check to see if the user has another instance open */
	libunique->priv->uniqueapp = unique_app_new (service, NULL);
	if (unique_app_is_running (libunique->priv->uniqueapp)) {
		g_warning ("You have another instance running. This program will now close");
		unique_app_send_message (libunique->priv->uniqueapp, UNIQUE_ACTIVATE, NULL);
		return FALSE;
	}

	/* Listen for messages from another instances */
	g_signal_connect (G_OBJECT (libunique->priv->uniqueapp), "message-received",
			  G_CALLBACK (libunique_message_cb), libunique);
	return TRUE;
}
#else

/**
 * libunique_assign:
 * @libunique: This class instance
 * @service: The service name
 * Return value: always %TRUE
 **/
gboolean
libunique_assign (LibUnique *libunique, const gchar *service)
{
	g_return_val_if_fail (IS_LIBUNIQUE (libunique), FALSE);
	return TRUE;
}
#endif

/**
 * libunique_class_init:
 * @libunique: This class instance
 **/
static void
libunique_class_init (LibUniqueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = libunique_finalize;
	g_type_class_add_private (klass, sizeof (LibUniquePrivate));

	signals [ACTIVATED] =
		g_signal_new ("activated",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LibUniqueClass, activated),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * libunique_init:
 * @libunique: This class instance
 **/
static void
libunique_init (LibUnique *libunique)
{
	libunique->priv = LIBUNIQUE_GET_PRIVATE (libunique);
	libunique->priv->uniqueapp = NULL;
}

/**
 * libunique_finalize:
 * @object: This class instance
 **/
static void
libunique_finalize (GObject *object)
{
	LibUnique *libunique;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LIBUNIQUE (object));

	libunique = LIBUNIQUE_OBJECT (object);
	libunique->priv = LIBUNIQUE_GET_PRIVATE (libunique);

	if (libunique->priv->uniqueapp != NULL) {
		g_object_unref (libunique->priv->uniqueapp);
	}
	G_OBJECT_CLASS (libunique_parent_class)->finalize (object);
}

/**
 * libunique_new:
 * Return value: new class instance.
 **/
LibUnique *
libunique_new (void)
{
	LibUnique *libunique;
	libunique = g_object_new (LIBUNIQUE_TYPE, NULL);
	return LIBUNIQUE_OBJECT (libunique);
}

