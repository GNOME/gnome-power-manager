/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMSRV_CPUFREQ_H
#define __GPMSRV_CPUFREQ_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_SRV_CPUFREQ		(gpm_srv_cpufreq_get_type ())
#define GPM_SRV_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_SRV_CPUFREQ, GpmSrvCpuFreq))
#define GPM_SRV_CPUFREQ_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_SRV_CPUFREQ, GpmSrvCpuFreqClass))
#define GPM_IS_SRV_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_SRV_CPUFREQ))
#define GPM_IS_SRV_CPUFREQ_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_SRV_CPUFREQ))
#define GPM_SRV_CPUFREQ_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_SRV_CPUFREQ, GpmSrvCpuFreqClass))

typedef struct GpmSrvCpuFreqPrivate GpmSrvCpuFreqPrivate;

typedef struct
{
	GObject			 parent;
	GpmSrvCpuFreqPrivate	*priv;
} GpmSrvCpuFreq;

typedef struct
{
	GObjectClass	parent_class;
} GpmSrvCpuFreqClass;

GType		 gpm_srv_cpufreq_get_type		(void);
GpmSrvCpuFreq	*gpm_srv_cpufreq_new			(void);

G_END_DECLS

#endif	/* __GPMSRV_CPUFREQ_H */
