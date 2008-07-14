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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include "viking.h"
#include "coords.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "download.h"
#include "vikmapslayer.h"

#include "googlemaps.h"

static DownloadOptions googlemaps_options = { "http://maps.google.com/", 0, a_check_map_file };

/* initialisation */
void googlemaps_init () {
  VikMapsLayer_MapType map_type = { 9, 128, 128, VIK_VIEWPORT_DRAWMODE_GOOGLE, googlemaps_coord_to_mapcoord, googlemaps_mapcoord_to_center_coord, googlemaps_download };
  maps_layer_register_type(_("Old Google Maps"), 9, &map_type);
}

/* 1 << (x-1) is like a 2**(x-1) */
#define GZ(x) ((1<<(x-1))*GOOGLEMAPS_ZOOM_ONE_MPP)

static const gdouble scale_mpps[] = { GZ(1)/2, GZ(1), GZ(2), GZ(3), GZ(4), GZ(5), GZ(6), GZ(7), GZ(8), GZ(9),
                                           GZ(10), GZ(11), GZ(12), GZ(13), GZ(14), GZ(15) };

static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0])) - 1;

#define ERROR_MARGIN 0.01
guint8 googlemaps_zoom ( gdouble mpp ) {
  gint i;
  for ( i = 0; i <= num_scales; i++ ) {
    if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN )
      return i;
  }
  return 255;
}

gboolean googlemaps_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = googlemaps_zoom ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;

  dest->x = (gint) floor ( floor ((src->east_west + 98.35) * (131072 >> dest->scale) * 0.77162458338772) / 128);
  dest->y = (gint) floor ( floor ((39.5 - src->north_south) * (131072 >> dest->scale)) / 128);
  dest->z = 0;
  return TRUE;
}

void googlemaps_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest )
{
  dest->mode = VIK_COORD_LATLON;
  dest->north_south = 39.5 - (1.0/(131072 >> src->scale) * (src->y+0.5) * 128);
  dest->east_west = 1.0 / (131072 >> src->scale) * (src->x+0.5) / 0.77162458338772 * 128 - 98.35;
}

int googlemaps_download ( MapCoord *src, const gchar *dest_fn )
{
   gchar *uri = g_strdup_printf ( "/mt?&x=%d&y=%d&zoom=%d", src->x, src->y, src->scale );
   a_http_download_get_url ( "mt.google.com", uri, dest_fn, &googlemaps_options );
   g_free ( uri );
   return 1;
}
