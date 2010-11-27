/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _VIK_WMSC_MAP_SOURCE_H
#define _VIK_WMSC_MAP_SOURCE_H

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapsourcedefault.h"

G_BEGIN_DECLS

#define VIK_TYPE_WMSC_MAP_SOURCE             (vik_wmsc_map_source_get_type ())
#define VIK_WMSC_MAP_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_WMSC_MAP_SOURCE, VikWmscMapSource))
#define VIK_WMSC_MAP_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_WMSC_MAP_SOURCE, VikWmscMapSourceClass))
#define VIK_IS_WMSC_MAP_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_WMSC_MAP_SOURCE))
#define VIK_IS_WMSC_MAP_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_WMSC_MAP_SOURCE))
#define VIK_WMSC_MAP_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_WMSC_MAP_SOURCE, VikWmscMapSourceClass))

typedef struct _VikWmscMapSourceClass VikWmscMapSourceClass;
typedef struct _VikWmscMapSource VikWmscMapSource;

struct _VikWmscMapSourceClass
{
	VikMapSourceDefaultClass parent_class;
};

struct _VikWmscMapSource
{
	VikMapSourceDefault parent_instance;
};

GType vik_wmsc_map_source_get_type (void) G_GNUC_CONST;

VikWmscMapSource * vik_wmsc_map_source_new_with_id (guint8 id, const gchar *label, const gchar *hostname, const gchar *url);

G_END_DECLS

#endif /* _VIK_WMSC_MAP_SOURCE_H_ */
