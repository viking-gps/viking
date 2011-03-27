/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * 
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VIK_SLIPPY_MAP_SOURCE_H
#define _VIK_SLIPPY_MAP_SOURCE_H

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapsourcedefault.h"

G_BEGIN_DECLS

#define VIK_TYPE_SLIPPY_MAP_SOURCE             (vik_slippy_map_source_get_type ())
#define VIK_SLIPPY_MAP_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_SLIPPY_MAP_SOURCE, VikSlippyMapSource))
#define VIK_SLIPPY_MAP_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_SLIPPY_MAP_SOURCE, VikSlippyMapSourceClass))
#define VIK_IS_SLIPPY_MAP_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_SLIPPY_MAP_SOURCE))
#define VIK_IS_SLIPPY_MAP_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_SLIPPY_MAP_SOURCE))
#define VIK_SLIPPY_MAP_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_SLIPPY_MAP_SOURCE, VikSlippyMapSourceClass))

typedef struct _VikSlippyMapSourceClass VikSlippyMapSourceClass;
typedef struct _VikSlippyMapSource VikSlippyMapSource;

struct _VikSlippyMapSourceClass
{
	VikMapSourceDefaultClass parent_class;
};

struct _VikSlippyMapSource
{
	VikMapSourceDefault parent_instance;
};

GType vik_slippy_map_source_get_type (void) G_GNUC_CONST;

VikSlippyMapSource * vik_slippy_map_source_new_with_id (guint8 id, const gchar *label, const gchar *hostname, const gchar *url);
gint vik_slippy_map_source_zoom_to_scale ( gdouble mpp );


G_END_DECLS

#endif /* _VIK_SLIPPY_MAP_SOURCE_H_ */
