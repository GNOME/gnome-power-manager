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

#ifndef __GPMCPUFREQ_H
#define __GPMCPUFREQ_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_CPUFREQ		(gpm_cpufreq_get_type ())
#define GPM_CPUFREQ(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_CPUFREQ, GpmCpufreq))
#define GPM_CPUFREQ_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_CPUFREQ, GpmCpufreqClass))
#define GPM_IS_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_CPUFREQ))
#define GPM_IS_CPUFREQ_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_CPUFREQ))
#define GPM_CPUFREQ_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_CPUFREQ, GpmCpufreqClass))

typedef struct GpmCpufreqPrivate GpmCpufreqPrivate;

typedef struct
{
	GObject			 parent;
	GpmCpufreqPrivate	*priv;
} GpmCpufreq;

typedef struct
{
	GObjectClass	parent_class;
} GpmCpufreqClass;

GType		 gpm_cpufreq_get_type				(void);
GpmCpufreq	*gpm_cpufreq_new				(void);

G_END_DECLS

#endif	/* __GPMCPUFREQ_H */

