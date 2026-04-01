/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#include "gpm-info-item.h"

struct _GpmInfoItem {
	GObject parent_instance;
	gchar *attribute;
	gchar *value;
};

G_DEFINE_TYPE (GpmInfoItem, gpm_info_item, G_TYPE_OBJECT)

static void
gpm_info_item_init (GpmInfoItem *item)
{
}

static void
gpm_info_item_class_init (GpmInfoItemClass *klass)
{
}

GpmInfoItem *
gpm_info_item_new (const gchar *attribute, const gchar *value)
{
	GpmInfoItem *item = g_object_new (GPM_INFO_ITEM_TYPE, NULL);

	item->attribute = g_strdup (attribute);
	item->value = g_strdup (value);

	return item;
}

const gchar *
gpm_info_item_get_attribute (GpmInfoItem *item)
{
	g_return_val_if_fail (GPM_IS_INFO_ITEM (item), NULL);

	return item->attribute;
}

const gchar *
gpm_info_item_get_value (GpmInfoItem *item)
{
	g_return_val_if_fail (GPM_IS_INFO_ITEM (item), NULL);

	return item->value;
}
