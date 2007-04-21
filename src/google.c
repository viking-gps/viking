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
#include <string.h>
#include "coords.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "download.h"
#include "globals.h"
#include "google.h"
#include "vikmapslayer.h"

#define GOOGLE_VERSION "w2.43"
#define GOOGLE_TRANS_VERSION "w2t.40"
#define GOOGLE_KH_VERSION "17"

void google_init () {
  VikMapsLayer_MapType google_1 = { 7, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_download };
  VikMapsLayer_MapType google_2 = { 10, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_trans_download };
  VikMapsLayer_MapType google_3 = { 11, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_kh_download };
  VikMapsLayer_MapType slippy = { 12, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_slippy_download };

  maps_layer_register_type("Google Maps", 7, &google_1);
  maps_layer_register_type("Transparent Google Maps", 10, &google_2);
  maps_layer_register_type("Google Satellite Images", 11, &google_3);
  maps_layer_register_type("OpenStreetMap Slippy Maps", 12, &slippy);
}

/* 1 << (x) is like a 2**(x) */
#define GZ(x) ((1<<x))

static const gdouble scale_mpps[] = { GZ(0), GZ(1), GZ(2), GZ(3), GZ(4), GZ(5), GZ(6), GZ(7), GZ(8), GZ(9),
                                           GZ(10), GZ(11), GZ(12), GZ(13), GZ(14), GZ(15), GZ(16), GZ(17) };

static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

#define ERROR_MARGIN 0.01
guint8 google_zoom ( gdouble mpp ) {
  gint i;
  for ( i = 0; i < num_scales; i++ ) {
    if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN )
      return i;
  }
  return 255;
}

gboolean google_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = google_zoom ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;

  dest->x = (src->east_west + 180) / 360 * GZ(17) / xzoom;
  dest->y = (180 - MERCLAT(src->north_south)) / 360 * GZ(17) / xzoom;
  dest->z = 0;

  return TRUE;
}

void google_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest )
{
  gdouble socalled_mpp = GZ(src->scale);
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = ((src->x+0.5) / GZ(17) * socalled_mpp * 360) - 180;
  dest->north_south = DEMERCLAT(180 - ((src->y+0.5) / GZ(17) * socalled_mpp * 360));
}

static void real_google_download ( MapCoord *src, const gchar *dest_fn, const char *verstr )
{
   gchar *uri = g_strdup_printf ( "/mt?v=%s&x=%d&y=%d&zoom=%d", verstr, src->x, src->y, src->scale );
   a_http_download_get_url ( "mt.google.com", uri, dest_fn );
   g_free ( uri );
}

void google_download ( MapCoord *src, const gchar *dest_fn )
{
   real_google_download ( src, dest_fn, GOOGLE_VERSION );
}

void google_trans_download ( MapCoord *src, const gchar *dest_fn )
{
   real_google_download ( src, dest_fn, GOOGLE_TRANS_VERSION );
}

static char *kh_encode(guint32 x, guint32 y, guint8 scale)
{
  gchar *buf = g_malloc ( (20-scale)*sizeof(gchar) );
  guint32 ya = 1 << (17 - scale);
  gint8 i, j;

  if (y < 0 || (ya-1 < y)) {
    strcpy(buf,"tqq"); /* BAD */
    return buf;
  }
  if (x < 0 || ya-1 < x) {
    x %= ya;
    if (x < 0)
      x += ya;
  }

  buf[0] = 't';
  for (j = 1, i = 16; i >= scale; i--, j++) {
    ya /= 2;
    if (y < ya) {
      if (x < ya)
        buf[j]='q';
      else {
        buf[j]='r';
        x -= ya;
      }
    } else {
      if (x < ya) {
        buf[j] = 't';
        y -= ya;
      } else {
        buf[j] = 's';
        x -= ya;
        y -= ya;
      }
    }
  }
  buf[j] = '\0';
  return buf;
}

void google_kh_download ( MapCoord *src, const gchar *dest_fn )
{
   gchar *khenc = kh_encode( src->x, src->y, src->scale );
   gchar *uri = g_strdup_printf ( "/kh?v=%s&t=%s", GOOGLE_KH_VERSION, khenc );
   g_free ( khenc );
   a_http_download_get_url ( "kh.google.com", uri, dest_fn );
   g_free ( uri );
}

void google_slippy_download ( MapCoord *src, const gchar *dest_fn )
{
   gchar *uri = g_strdup_printf ( "/%d/%d/%d.png", 17-src->scale, src->x, src->y );
   a_http_download_get_url ( "tile.openstreetmap.org", uri, dest_fn );
   g_free ( uri );
}
