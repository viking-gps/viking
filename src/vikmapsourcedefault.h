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

#ifndef _VIK_MAP_SOURCE_DEFAULT_H_
#define _VIK_MAP_SOURCE_DEFAULT_H_

#include <glib-object.h>

#include "vikmapsource.h"
#include "download.h"

G_BEGIN_DECLS

#define VIK_TYPE_MAP_SOURCE_DEFAULT             (vik_map_source_default_get_type ())
#define VIK_MAP_SOURCE_DEFAULT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_MAP_SOURCE_DEFAULT, VikMapSourceDefault))
#define VIK_MAP_SOURCE_DEFAULT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_MAP_SOURCE_DEFAULT, VikMapSourceDefaultClass))
#define VIK_IS_MAP_SOURCE_DEFAULT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_MAP_SOURCE_DEFAULT))
#define VIK_IS_MAP_SOURCE_DEFAULT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_MAP_SOURCE_DEFAULT))
#define VIK_MAP_SOURCE_DEFAULT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_MAP_SOURCE_DEFAULT, VikMapSourceDefaultClass))

typedef struct _VikMapSourceDefaultClass VikMapSourceDefaultClass;
typedef struct _VikMapSourceDefault VikMapSourceDefault;

struct _VikMapSourceDefaultClass
{
	VikMapSourceClass parent_class;

	gchar * (*get_uri) ( VikMapSourceDefault *self, MapCoord *src );
	gchar * (*get_hostname) ( VikMapSourceDefault *self );
	DownloadMapOptions * (*get_download_options) ( VikMapSourceDefault *self );
};

struct _VikMapSourceDefault
{
	VikMapSource parent_instance;
};

GType vik_map_source_default_get_type (void) G_GNUC_CONST;
gchar * vik_map_source_default_get_uri( VikMapSourceDefault *self, MapCoord *src );
gchar * vik_map_source_default_get_hostname( VikMapSourceDefault *self );
DownloadMapOptions * vik_map_source_default_get_download_options( VikMapSourceDefault *self );

G_END_DECLS

#endif /* _VIK_MAP_SOURCE_DEFAULT_H_ */
