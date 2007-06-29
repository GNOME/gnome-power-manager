/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_NOTIFY_H
#define __GPM_NOTIFY_H

#include <glib-object.h>
#include <gtk/gtkstatusicon.h>

G_BEGIN_DECLS

#define GPM_TYPE_NOTIFY		(gpm_notify_get_type ())
#define GPM_NOTIFY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_NOTIFY, GpmNotify))
#define GPM_NOTIFY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_NOTIFY, GpmNotifyClass))
#define GPM_IS_NOTIFY(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_NOTIFY))
#define GPM_IS_NOTIFY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_NOTIFY))
#define GPM_NOTIFY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_NOTIFY, GpmNotifyClass))

typedef enum {
	GPM_NOTIFY_TIMEOUT_NEVER,
	GPM_NOTIFY_TIMEOUT_LONG,
	GPM_NOTIFY_TIMEOUT_SHORT
} GpmNotifyTimeout;

typedef enum {
	GPM_NOTIFY_URGENCY_CRITICAL = 1,
	GPM_NOTIFY_URGENCY_NORMAL   = 2,
	GPM_NOTIFY_URGENCY_LOW      = 3
} GpmNotifyUrgency;

typedef struct GpmNotifyPrivate GpmNotifyPrivate;

typedef struct
{
	GObject		      parent;
	GpmNotifyPrivate *priv;
} GpmNotify;

typedef struct
{
	GObjectClass	parent_class;
//	void		(* notify_changed)	(GpmNotify		*notify,
//						 GpmNotifyState	 	 status);
} GpmNotifyClass;

GType		 gpm_notify_get_type		(void);
GpmNotify	*gpm_notify_new			(void);

gboolean	 gpm_notify_display		(GpmNotify 		*notify,
	 					 const gchar		*title,
						 const gchar		*content,
						 GpmNotifyTimeout	 timeout,
						 const gchar		*msgicon,
						 GpmNotifyUrgency	 urgency);
void		 gpm_notify_cancel		(GpmNotify		*notify);
void		 gpm_notify_use_status_icon	(GpmNotify		*notify,
						 GtkStatusIcon		*status_icon);
gboolean	 gpm_notify_perhaps_recall	(GpmNotify		*notify,
						 const gchar		*oem_vendor,
						 const gchar		*website);
gboolean	 gpm_notify_low_capacity	(GpmNotify		*notify,
						 guint			 capacity);
gboolean	 gpm_notify_inhibit_lid		(GpmNotify		*notify);
gboolean	 gpm_notify_fully_charged_primary (GpmNotify		*notify);
gboolean	 gpm_notify_discharging_primary	(GpmNotify		*notify);
gboolean	 gpm_notify_discharging_ups	(GpmNotify		*notify);
gboolean	 gpm_notify_sleep_failed	(GpmNotify		*notify,
						 gboolean		 hibernate);

G_END_DECLS

#endif /* __GPM_NOTIFY_H */
