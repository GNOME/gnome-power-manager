/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#include "gpm-device-item.h"

struct _GpmDeviceItem {
	GObject parent_instance;
	GIcon *icon;
	gchar *text;
	gchar *id;
};

G_DEFINE_TYPE (GpmDeviceItem, gpm_device_item, G_TYPE_OBJECT)

static void
gpm_device_item_init (GpmDeviceItem *item)
{
}

static void
gpm_device_item_class_init (GpmDeviceItemClass *klass)
{
}

GpmDeviceItem *
gpm_device_item_new (GIcon *icon, const gchar *text, const gchar *id)
{
	GpmDeviceItem *item = g_object_new (GPM_TYPE_DEVICE_ITEM, NULL);

	item->icon = g_object_ref (icon);
	item->text = g_strdup (text);
	item->id = g_strdup (id);

	return item;
}

GIcon *
gpm_device_item_get_icon (GpmDeviceItem *item)
{
	g_return_val_if_fail (GPM_IS_DEVICE_ITEM (item), NULL);

	return item->icon;
}

const gchar *
gpm_device_item_get_text (GpmDeviceItem *item)
{
	g_return_val_if_fail (GPM_IS_DEVICE_ITEM (item), NULL);

	return item->text;
}

const gchar *
gpm_device_item_get_id (GpmDeviceItem *item)
{
	g_return_val_if_fail (GPM_IS_DEVICE_ITEM (item), NULL);

	return item->id;
}
