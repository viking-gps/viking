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
#include <string.h>
#include <math.h>
#include "viking.h"
#include "coords.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "download.h"
#include "vikmapslayer.h"

#include "khmaps.h"

static DownloadOptions khmaps_options = { NULL, 0, a_check_map_file };

void khmaps_init () {
  VikMapsLayer_MapType map_type = { 8, 256, 256, VIK_VIEWPORT_DRAWMODE_KH, khmaps_coord_to_mapcoord, khmaps_mapcoord_to_center_coord, khmaps_download };

  maps_layer_register_type("Old KH Satellite Images", 8, &map_type);
}

/* 1 << (x-1) is like a 2**(x-1) */
#define KH(x) ((1<<(x-1)))

static const gdouble scale_mpps[] = { KH(1), KH(2), KH(3), KH(4), KH(5), KH(6), KH(7), KH(8), KH(9),
                                           KH(10), KH(11), KH(12), KH(13), KH(14), KH(15) };

static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0])) - 1;

#define ERROR_MARGIN 0.01
guint8 khmaps_zoom ( gdouble mpp ) {
  gint i;
  for ( i = 0; i <= num_scales; i++ ) {
    if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN )
      return i;
  }
  return 255;
}

gboolean khmaps_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = khmaps_zoom ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;


  dest->x = (gint) ((1<<(16 - dest->scale)) + (src->east_west / 180 * (1<<(16-dest->scale))));
  dest->y = (gint) ((1<<(16 - dest->scale)) - (src->north_south / 180 * (1<<(16-dest->scale))));
  dest->z = 0;
  return TRUE;
}

void khmaps_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest )
{
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = (src->x+0.5 - (1<<(16 - src->scale))) * 180 / (1<<(16-src->scale));
  dest->north_south = -((src->y+0.5 - (1<<(16 - src->scale))) * 180 / (1<<(16-src->scale)));
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

int khmaps_download ( MapCoord *src, const gchar *dest_fn )
{
   gchar *tmp = kh_encode(src->x, src->y, src->scale);
   gchar *uri = g_strdup_printf ( "/kh?v=2&t=%s", tmp );
   g_print("%d %d %d = %s\n", src->x, src->y, src->scale, uri);
   g_free ( tmp );
   a_http_download_get_url ( "kh.google.com", uri, dest_fn, &khmaps_options );
   g_free ( uri );
   return 1;
}

/* Popularity has its disadvantages ... */
