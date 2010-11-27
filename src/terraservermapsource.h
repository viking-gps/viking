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

#ifndef _TERRASERVER_MAP_SOURCE_H
#define _TERRASERVER_MAP_SOURCE_H

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapsourcedefault.h"

G_BEGIN_DECLS

#define TERRASERVER_TYPE_MAP_SOURCE             (terraserver_map_source_get_type ())
#define TERRASERVER_MAP_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TERRASERVER_TYPE_MAP_SOURCE, TerraserverMapSource))
#define TERRASERVER_MAP_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TERRASERVER_TYPE_MAP_SOURCE, TerraserverMapSourceClass))
#define TERRASERVER_IS_MAP_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TERRASERVER_TYPE_MAP_SOURCE))
#define TERRASERVER_IS_MAP_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TERRASERVER_TYPE_MAP_SOURCE))
#define TERRASERVER_MAP_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TERRASERVER_TYPE_MAP_SOURCE, TerraserverMapSourceClass))

typedef struct _TerraserverMapSourceClass TerraserverMapSourceClass;
typedef struct _TerraserverMapSource TerraserverMapSource;

struct _TerraserverMapSourceClass
{
	VikMapSourceDefaultClass parent_class;
};

struct _TerraserverMapSource
{
	VikMapSourceDefault parent_instance;
};

GType terraserver_map_source_get_type (void) G_GNUC_CONST;

TerraserverMapSource * terraserver_map_source_new_with_id (guint8 id, const char *label, int type);

G_END_DECLS

#endif /* _TERRASERVER_MAP_SOURCE_H_ */
