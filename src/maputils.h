/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _MAP_UTILS_H
#define _MAP_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

/* 1 << (x) is like a 2**(x) */
#define VIK_GZ(x) ((1<<(x)))
// Not sure what GZ stands for probably Google Zoom

gint map_utils_mpp_to_scale ( gdouble mpp );

guint8 map_utils_mpp_to_zoom_level ( gdouble mpp );

G_BEGIN_DECLS

#endif
