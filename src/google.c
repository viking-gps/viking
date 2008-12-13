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

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "coords.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "download.h"
#include "curl_download.h"
#include "globals.h"
#include "google.h"
#include "vikmapslayer.h"
#include "vikexttools.h"
#include "vikwebtoolcenter.h"


static int google_download ( MapCoord *src, const gchar *dest_fn );
static int google_trans_download ( MapCoord *src, const gchar *dest_fn );
static int google_terrain_download ( MapCoord *src, const gchar *dest_fn );
static void google_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest );
static gboolean google_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );

static DownloadOptions google_options = { "http://maps.google.com/", 0, a_check_map_file };

void google_init () {
  VikMapsLayer_MapType google_1 = { 7, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_download };
  VikMapsLayer_MapType google_2 = { 10, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_trans_download };
  VikMapsLayer_MapType google_4 = { 16, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_terrain_download };

  maps_layer_register_type(_("Google Street Maps"), 7, &google_1);
  maps_layer_register_type(_("Transparent Google Maps"), 10, &google_2);
  maps_layer_register_type(_("Google Terrain Maps"), 16, &google_4);

  // Webtools
  VikWebtoolCenter *webtool = vik_webtool_center_new_with_members ( _("Google"), "http://maps.google.com/maps?f=q&hl=fr&geocode=&ie=UTF8&ll=%s,%s&z=%d&iwloc=addr" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );
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

typedef enum {
	TYPE_GOOGLE_MAPS = 0,
	TYPE_GOOGLE_TRANS,
	TYPE_GOOGLE_TERRAIN,

	TYPE_GOOGLE_NUM
} GoogleType;

static gchar *parse_version_number(gchar *text)
{
  int i;
  gchar *vers;
  gchar *s = text;

  for (i = 0; (s[i] != '\\') && (i < 8); i++)
    ;
  if (s[i] != '\\') {
    return NULL;
  }

  return vers = g_strndup(s, i);
}

static const gchar *google_version_number(MapCoord *mapcoord, GoogleType google_type)
{
  static gboolean first = TRUE;
  static char *vers[] = { "w2.80", "w2t.80", "w2p.81"};
  FILE *tmp_file;
  int tmp_fd;
  gchar *tmpname;
  gchar *uri;
  VikCoord coord;
  gchar coord_north_south[G_ASCII_DTOSTR_BUF_SIZE], coord_east_west[G_ASCII_DTOSTR_BUF_SIZE];
  gchar *text, *pat, *beg;
  GMappedFile *mf;
  gsize len;
  gchar *gvers, *tvers, *terrvers, *tmpvers;
  static DownloadOptions dl_options = { "http://maps.google.com/", 0, a_check_map_file };
  static const char *gvers_pat = "http://mt0.google.com/mt";

  g_assert(google_type < TYPE_GOOGLE_NUM);

  if (!first)
    return (vers[google_type]);


  first = FALSE;
  gvers = tvers = terrvers = NULL;
  if ((tmp_fd = g_file_open_tmp ("vikgvers.XXXXXX", &tmpname, NULL)) == -1) {
    g_critical(_("couldn't open temp file %s"), tmpname);
    exit(1);
  } 

  google_mapcoord_to_center_coord(mapcoord, &coord);
  uri = g_strdup_printf("http://maps.google.com/maps?f=q&hl=en&q=%s,%s",
                        g_ascii_dtostr (coord_north_south, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) coord.north_south),
                        g_ascii_dtostr (coord_east_west, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) coord.east_west));
  tmp_file = fdopen(tmp_fd, "r+");

  if (curl_download_uri(uri, tmp_file, &dl_options)) {  /* error */
    g_warning(_("Failed downloading %s"), tmpname);
  } else {
    if ((mf = g_mapped_file_new(tmpname, FALSE, NULL)) == NULL) {
      g_critical(_("couldn't map temp file"));
      exit(1);
    }
    len = g_mapped_file_get_length(mf);
    text = g_mapped_file_get_contents(mf);

    if ((beg = g_strstr_len(text, len, "GLoadApi")) == NULL) {
      g_warning(_("Failed fetching Google numbers (\"GLoadApi\" not found)"));
      goto failed;
    }

    pat = beg;
    while (!gvers || !tvers ||!terrvers) {
      if ((pat = g_strstr_len(pat, &text[len] - pat, gvers_pat)) != NULL) {
        pat += strlen(gvers_pat);
	if ((pat[0] != '/' && pat[0] != '?') ||
	     pat[1] != 'v' || pat[2] != '\\' ||
	     pat[3] != 'x' || pat[4] != '3' || pat[5] != 'd')
          continue;
        pat += 6;
        if ((tmpvers = parse_version_number(pat)) != NULL) {
          if (strstr(tmpvers, "t."))
            tvers = tmpvers;
          else if (strstr(tmpvers, "p."))
            terrvers = tmpvers;
          else
            gvers = tmpvers;
        }
      }
      else
        break;
    }

    if (gvers && tvers && terrvers) {
      vers[TYPE_GOOGLE_MAPS] = gvers;
      vers[TYPE_GOOGLE_TRANS] = tvers;
      vers[TYPE_GOOGLE_TERRAIN] = terrvers;
    }
    else
      g_warning(_("Failed getting google version numbers"));

    if (gvers)
      fprintf(stderr, "DEBUG gvers=%s\n", gvers);
    if (tvers)
      fprintf(stderr, "DEBUG tvers=%s\n", tvers);
    if (terrvers)
      fprintf(stderr, "DEBUG terrvers=%s\n", terrvers);

failed:
    g_mapped_file_free(mf);
  }

  fclose(tmp_file);
  tmp_file = NULL;
  g_free(tmpname);
  g_free (uri);
  return (vers[google_type]);
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

static int real_google_download ( MapCoord *src, const gchar *dest_fn, const char *verstr )
{
   int res;
   gchar *uri = g_strdup_printf ( "/mt?n=404&v=%s&x=%d&y=%d&zoom=%d", verstr, src->x, src->y, src->scale );
   res = a_http_download_get_url ( "mt.google.com", uri, dest_fn, &google_options );
   g_free ( uri );
   return res;
}

static int google_download ( MapCoord *src, const gchar *dest_fn )
{
   const gchar *vers_str = google_version_number(src, TYPE_GOOGLE_MAPS);
   return(real_google_download ( src, dest_fn, vers_str ));
}

static int google_trans_download ( MapCoord *src, const gchar *dest_fn )
{
   const gchar *vers_str = google_version_number(src, TYPE_GOOGLE_TRANS);
   return(real_google_download ( src, dest_fn, vers_str ));
}

static int google_terrain_download ( MapCoord *src, const gchar *dest_fn )
{
   const gchar *vers_str = google_version_number(src, TYPE_GOOGLE_TERRAIN);
   return(real_google_download ( src, dest_fn, vers_str ));
}
