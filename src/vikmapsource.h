/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2009-2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _VIK_MAP_SOURCE_H_
#define _VIK_MAP_SOURCE_H_

#include <glib-object.h>

#include "vikviewport.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "bbox.h"

G_BEGIN_DECLS

#define VIK_TYPE_MAP_SOURCE             (vik_map_source_get_type ())
#define VIK_MAP_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_MAP_SOURCE, VikMapSource))
#define VIK_MAP_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_MAP_SOURCE, VikMapSourceClass))
#define VIK_IS_MAP_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_MAP_SOURCE))
#define VIK_IS_MAP_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_MAP_SOURCE))
#define VIK_MAP_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_MAP_SOURCE, VikMapSourceClass))

typedef struct _VikMapSourceClass VikMapSourceClass;
typedef struct _VikMapSource VikMapSource;

struct _VikMapSourceClass
{
	GObjectClass parent_class;

	/* Legal info */
	void (* get_copyright) (VikMapSource * self, LatLonBBox bbox, gdouble zoom, void (*fct)(VikViewport*,const gchar*), void *data);
	const gchar *(* get_license) (VikMapSource * self);
	const gchar *(* get_license_url) (VikMapSource * self);
	const GdkPixbuf *(* get_logo) (VikMapSource * self);

	const gchar *(* get_name) (VikMapSource * self);
	guint16 (* get_uniq_id) (VikMapSource * self);
	const gchar * (* get_label) (VikMapSource * self);
	guint16 (* get_tilesize_x) (VikMapSource * self);
	guint16 (* get_tilesize_y) (VikMapSource * self);
	VikViewportDrawMode (* get_drawmode) (VikMapSource * self);
	gboolean (* is_direct_file_access) (VikMapSource * self);
	gboolean (* is_mbtiles) (VikMapSource * self);
	gboolean (* is_osm_meta_tiles) (VikMapSource * self);
	gboolean (* supports_download_only_new) (VikMapSource * self);
	guint8 (* get_zoom_min) (VikMapSource * self);
	guint8 (* get_zoom_max) (VikMapSource * self);
	gdouble (* get_lat_min) (VikMapSource * self);
	gdouble (* get_lat_max) (VikMapSource * self);
	gdouble (* get_lon_min) (VikMapSource * self);
	gdouble (* get_lon_max) (VikMapSource * self);
	const gchar * (* get_file_extension) (VikMapSource * self);
	gboolean (* coord_to_mapcoord) (VikMapSource * self, const VikCoord * src, gdouble xzoom, gdouble yzoom, MapCoord * dest);
	void (* mapcoord_to_center_coord) (VikMapSource * self, MapCoord * src, VikCoord * dest);
	int (* download) (VikMapSource * self, MapCoord * src, const gchar * dest_fn, void * handle);
	void * (* download_handle_init) (VikMapSource * self);
	void (* download_handle_cleanup) (VikMapSource * self, void * handle);
};

struct _VikMapSource
{
	GObject parent_instance;
};

GType vik_map_source_get_type (void) G_GNUC_CONST;

void vik_map_source_get_copyright (VikMapSource * self, LatLonBBox bbox, gdouble zoom, void (*fct)(VikViewport*,const gchar*), void *data);
const gchar *vik_map_source_get_license (VikMapSource * self);
const gchar *vik_map_source_get_license_url (VikMapSource * self);
const GdkPixbuf *vik_map_source_get_logo (VikMapSource * self);

const gchar *vik_map_source_get_name (VikMapSource * self);
guint16 vik_map_source_get_uniq_id (VikMapSource * self);
const gchar *vik_map_source_get_label (VikMapSource * self);
guint16 vik_map_source_get_tilesize_x (VikMapSource * self);
guint16 vik_map_source_get_tilesize_y (VikMapSource * self);
VikViewportDrawMode vik_map_source_get_drawmode (VikMapSource * self);
gboolean vik_map_source_is_direct_file_access (VikMapSource * self);
gboolean vik_map_source_is_mbtiles (VikMapSource * self);
gboolean vik_map_source_is_osm_meta_tiles (VikMapSource * self);
gboolean vik_map_source_supports_download_only_new (VikMapSource * self);
guint8 vik_map_source_get_zoom_min (VikMapSource * self);
guint8 vik_map_source_get_zoom_max (VikMapSource * self);
gdouble vik_map_source_get_lat_min (VikMapSource * self);
gdouble vik_map_source_get_lat_max (VikMapSource * self);
gdouble vik_map_source_get_lon_min (VikMapSource * self);
gdouble vik_map_source_get_lon_max (VikMapSource * self);
const gchar * vik_map_source_get_file_extension (VikMapSource * self);
gboolean vik_map_source_coord_to_mapcoord (VikMapSource * self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
void vik_map_source_mapcoord_to_center_coord (VikMapSource * self, MapCoord *src, VikCoord *dest);
int vik_map_source_download (VikMapSource * self, MapCoord * src, const gchar * dest_fn, void * handle);
void * vik_map_source_download_handle_init (VikMapSource * self);
void vik_map_source_download_handle_cleanup (VikMapSource * self, void * handle);

G_END_DECLS

#endif /* _VIK_MAP_SOURCE_H_ */
