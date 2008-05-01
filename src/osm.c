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

#include <gtk/gtk.h>
#include <math.h>
#include "viking.h"
#include "coords.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapslayer.h"

#include "osm.h"

static guint8 osm_zoom ( gdouble mpp );

static gboolean osm_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void osm_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest );
static int osm_maplint_download ( MapCoord *src, const gchar *dest_fn );
static int osm_mapnik_download ( MapCoord *src, const gchar *dest_fn );
static int osm_osmarender_download ( MapCoord *src, const gchar *dest_fn );
static int bluemarble_download ( MapCoord *src, const gchar *dest_fn );

static DownloadOptions osm_options = { NULL, 0, a_check_map_file };

/* initialisation */
void osm_init () {
  VikMapsLayer_MapType osmarender_type = { 12, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, osm_coord_to_mapcoord, osm_mapcoord_to_center_coord, osm_osmarender_download };
  VikMapsLayer_MapType mapnik_type = { 13, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, osm_coord_to_mapcoord, osm_mapcoord_to_center_coord, osm_mapnik_download };  VikMapsLayer_MapType maplint_type = { 14, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, osm_coord_to_mapcoord, osm_mapcoord_to_center_coord, osm_maplint_download };

  VikMapsLayer_MapType bluemarble_type = { 15, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, osm_coord_to_mapcoord, osm_mapcoord_to_center_coord, bluemarble_download };

  maps_layer_register_type("OpenStreetMap (Osmarender)", 12, &osmarender_type);
  maps_layer_register_type("OpenStreetMap (Mapnik)", 13, &mapnik_type);
  maps_layer_register_type("OpenStreetMap (Maplint)", 14, &maplint_type);

  maps_layer_register_type("BlueMarble", 15, &bluemarble_type);
}

/* 1 << (x) is like a 2**(x) */
#define GZ(x) (1<<(x))

static const gdouble scale_mpps[] = { GZ(0), GZ(1), GZ(2), GZ(3), GZ(4), GZ(5), GZ(6), GZ(7), GZ(8), GZ(9),
                                      GZ(10), GZ(11), GZ(12), GZ(13), GZ(14), GZ(15), GZ(16), GZ(17) };

static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

#define ERROR_MARGIN 0.01
guint8 osm_zoom ( gdouble mpp ) {
  gint i;
  for ( i = 0; i < num_scales; i++ ) {
    if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN )
      return i;
  }
  return 255;
}

static gboolean osm_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = osm_zoom ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;

  dest->x = (src->east_west + 180) / 360 * GZ(17) / xzoom;
  dest->y = (180 - MERCLAT(src->north_south)) / 360 * GZ(17) / xzoom;
  dest->z = 0;
  return TRUE;
}

static void osm_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest )
{
  gdouble socalled_mpp = GZ(src->scale);
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = ((src->x+0.5) / GZ(17) * socalled_mpp * 360) - 180;
  dest->north_south = DEMERCLAT(180 - ((src->y+0.5) / GZ(17) * socalled_mpp * 360));
}

/* Maplint tiles
 * Ex: http://dev.openstreetmap.org/~ojw/Tiles/maplint.php/10/517/375.png
 */
static int osm_maplint_download ( MapCoord *src, const gchar *dest_fn )
{
   int res = -1;
   gchar *uri = g_strdup_printf ( "/Tiles/maplint.php/%d/%d/%d.png", 17-src->scale, src->x, src->y );
   res = a_http_download_get_url ( "tah.openstreetmap.org", uri, dest_fn, &osm_options );
   g_free ( uri );
   return res;
}

static int osm_mapnik_download ( MapCoord *src, const gchar *dest_fn )
{
   int res = -1;
   gchar *uri = g_strdup_printf ( "/%d/%d/%d.png", 17-src->scale, src->x, src->y );
   res = a_http_download_get_url ( "tile.openstreetmap.org", uri, dest_fn, &osm_options );
   g_free ( uri );
   return res;
}

static int osm_osmarender_download ( MapCoord *src, const gchar *dest_fn )
{
   int res = -1;
   gchar *uri = g_strdup_printf ( "/Tiles/tile/%d/%d/%d.png", 17-src->scale, src->x, src->y );
   res = a_http_download_get_url ( "tah.openstreetmap.org", uri, dest_fn, &osm_options );
   g_free ( uri );
   return res;
}

static int bluemarble_download ( MapCoord *src, const gchar *dest_fn )
{
   int res = -1;
   gchar *uri = g_strdup_printf ( "/com.modestmaps.bluemarble/%d-r%d-c%d.jpg", 17-src->scale, src->y, src->x );
   res = a_http_download_get_url ( "s3.amazonaws.com", uri, dest_fn, &osm_options );

   g_free ( uri );
   return res;
}
