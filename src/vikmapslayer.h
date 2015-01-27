/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _VIKING_MAPSLAYER_H
#define _VIKING_MAPSLAYER_H

#include "vikcoord.h"
#include "viklayer.h"
#include "vikviewport.h"
#include "vikmapsource.h"
#include "mapcoord.h"
#include "vikmapslayer_compat.h"

G_BEGIN_DECLS

#define VIK_MAPS_LAYER_TYPE            (vik_maps_layer_get_type ())
#define VIK_MAPS_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_MAPS_LAYER_TYPE, VikMapsLayer))
#define VIK_MAPS_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_MAPS_LAYER_TYPE, VikMapsLayerClass))
#define IS_VIK_MAPS_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_MAPS_LAYER_TYPE))
#define IS_VIK_MAPS_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_MAPS_LAYER_TYPE))

typedef struct _VikMapsLayerClass VikMapsLayerClass;
struct _VikMapsLayerClass
{
  VikLayerClass object_class;
};

GType vik_maps_layer_get_type ();

typedef struct _VikMapsLayer VikMapsLayer;

typedef enum {
  VIK_MAPS_CACHE_LAYOUT_VIKING=0, // CacheDir/t<MapId>s<VikingZoom>z0/X/Y (NB no file extension) - Legacy default layout
  VIK_MAPS_CACHE_LAYOUT_OSM,      // CacheDir/<OptionalMapName>/OSMZoomLevel/X/Y.ext (Default ext=png)
  VIK_MAPS_CACHE_LAYOUT_NUM       // Last enum
} VikMapsCacheLayout;

// OSM definition is a TMS derivative, (Global Mercator profile with Flipped Y)
//  http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
//  http://wiki.openstreetmap.org/wiki/TMS
//  http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification

void maps_layer_init ();
void maps_layer_set_autodownload_default ( gboolean autodownload );
void maps_layer_set_cache_default ( VikMapsCacheLayout layout );
guint vik_maps_layer_get_default_map_type ();
void maps_layer_register_map_source ( VikMapSource *map );
void vik_maps_layer_download_section ( VikMapsLayer *vml, VikViewport *vvp, VikCoord *ul, VikCoord *br, gdouble zoom );
guint vik_maps_layer_get_map_type(VikMapsLayer *vml);
void vik_maps_layer_set_map_type(VikMapsLayer *vml, guint map_type);
gchar *vik_maps_layer_get_map_label(VikMapsLayer *vml);
gchar *maps_layer_default_dir ();
void vik_maps_layer_download ( VikMapsLayer *vml, VikViewport *vvp, gboolean only_new );

G_END_DECLS

#endif
