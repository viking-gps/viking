/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _BING_MAP_SOURCE_H
#define _BING_MAP_SOURCE_H

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikslippymapsource.h"

G_BEGIN_DECLS

#define BING_TYPE_MAP_SOURCE             (bing_map_source_get_type ())
#define BING_MAP_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BING_TYPE_MAP_SOURCE, BingMapSource))
#define BING_MAP_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BING_TYPE_MAP_SOURCE, BingMapSourceClass))
#define BING_IS_MAP_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BING_TYPE_MAP_SOURCE))
#define BING_IS_MAP_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BING_TYPE_MAP_SOURCE))
#define BING_MAP_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BING_TYPE_MAP_SOURCE, BingMapSourceClass))

typedef struct _BingMapSourceClass BingMapSourceClass;
typedef struct _BingMapSource BingMapSource;

struct _BingMapSourceClass
{
	VikSlippyMapSourceClass parent_class;
};

struct _BingMapSource
{
	VikSlippyMapSource parent_instance;
};

GType bing_map_source_get_type (void) G_GNUC_CONST;

BingMapSource * bing_map_source_new_with_id (guint8 id, const gchar *label, const gchar *key);

G_END_DECLS

#endif /* _BING_MAP_SOURCE_H_ */
