/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 *
 * Some formulas or perhaps even code derived from GPSDrive
 * GPSDrive Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>
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
#include "globals.h"
#include "coords.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "download.h"
#include "vikmapslayer.h"

#include "expedia.h"

static gboolean expedia_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void expedia_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest );
static int expedia_download ( MapCoord *src, const gchar *dest_fn );

static DownloadOptions expedia_options = { NULL, 2, a_check_map_file };

void expedia_init() {
  VikMapsLayer_MapType map_type = { 5, 0, 0, VIK_VIEWPORT_DRAWMODE_EXPEDIA, expedia_coord_to_mapcoord, expedia_mapcoord_to_center_coord, expedia_download };
  maps_layer_register_type(_("Expedia Street Maps"), 5, &map_type);
}

#define EXPEDIA_SITE "expedia.com"
#define MPP_MARGIN_OF_ERROR 0.01
#define DEGREES_TO_RADS 0.0174532925
#define HEIGHT_OF_LAT_DEGREE (111318.84502/ALTI_TO_MPP)
#define HEIGHT_OF_LAT_MINUTE (1855.3140837/ALTI_TO_MPP)
#define WIDTH_BUFFER 0
#define HEIGHT_BUFFER 25
#define REAL_WIDTH_BUFFER 1
#define REAL_HEIGHT_BUFFER 26

/* first buffer is to cut off the expedia/microsoft logo. Annoying little buggers ;) */
/* second is to allow for a 1-pixel overlap on each side. this is a good thing (tm) */

static const guint expedia_altis[]              = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };
/* square this number to find out how many per square degree. */
static const gdouble expedia_altis_degree_freq[]  = { 120, 60, 30, 15, 8, 4, 2, 1, 1, 1 };
static const guint expedia_altis_count = sizeof(expedia_altis) / sizeof(expedia_altis[0]);

gdouble expedia_altis_freq ( gint alti )
{
  static gint i;
  for ( i = 0; i < expedia_altis_count; i++ )
    if ( expedia_altis[i] == alti )
      return expedia_altis_degree_freq [ i ];

  g_error ( _("Invalid expedia altitude") );
  return 0;
}

/* returns -1 if none of the above. */
gint expedia_zoom_to_alti ( gdouble zoom )
{
  guint i;
  for ( i = 0; i < expedia_altis_count; i++ )
    if ( fabs(expedia_altis[i] - zoom) / zoom < MPP_MARGIN_OF_ERROR )
      return expedia_altis[i];
  return -1;
}

/*
gint expedia_pseudo_zone ( gint alti, gint x, gint y )
{
  return (int) (x/expedia_altis_freq(alti)*180) + (int) (y/expedia_altis_freq(alti)*90);
}
*/

void expedia_snip ( const gchar *file )
{
  /* Load the pixbuf */
  GError *gx = NULL;
  GdkPixbuf *old, *cropped;
  gint width, height;

  old = gdk_pixbuf_new_from_file ( file, &gx );
  if (gx)
  {
    g_warning ( _("Couldn't open EXPEDIA image file (right after successful download! Please report and delete image file!): %s"), gx->message );
    g_error_free ( gx );
    return;
  }

  width = gdk_pixbuf_get_width ( old );
  height = gdk_pixbuf_get_height ( old );

  cropped = gdk_pixbuf_new_subpixbuf ( old, WIDTH_BUFFER, HEIGHT_BUFFER,
                              width - 2*WIDTH_BUFFER, height - 2*HEIGHT_BUFFER );

  gdk_pixbuf_save ( cropped, file, "png", &gx, NULL );
  if ( gx ) {
    g_warning ( _("Couldn't save EXPEDIA image file (right after successful download! Please report and delete image file!): %s"), gx->message );
    g_error_free ( gx );
  }

  g_object_unref ( cropped );
  g_object_unref ( old );
}

/* if degree_freeq = 60 -> nearest minute (in middle) */
/* everything starts at -90,-180 -> 0,0. then increments by (1/degree_freq) */
static gboolean expedia_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  gint alti;

  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  alti = expedia_zoom_to_alti ( xzoom );
  if ( alti != -1 )
  {
    dest->scale = alti;
    dest->x = (int) (((src->east_west+180) * expedia_altis_freq(alti))+0.5);
    dest->y = (int) (((src->north_south+90) * expedia_altis_freq(alti))+0.5);
    /* + 0.5 to round off and not floor */

    /* just to space out tiles on the filesystem */
    dest->z = 0;
    return TRUE;
  }
  return FALSE;
}

void expedia_xy_to_latlon_middle ( gint alti, gint x, gint y, struct LatLon *ll )
{
  ll->lon = (((gdouble)x) / expedia_altis_freq(alti)) - 180;
  ll->lat = (((gdouble)y) / expedia_altis_freq(alti)) - 90;
}

static void expedia_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest )
{
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = (((gdouble)src->x) / expedia_altis_freq(src->scale)) - 180;
  dest->north_south = (((gdouble)src->y) / expedia_altis_freq(src->scale)) - 90;
}

static int expedia_download ( MapCoord *src, const gchar *dest_fn )
{
  gint height, width;
  struct LatLon ll;
  gchar *uri;
  int res = -1;

  expedia_xy_to_latlon_middle ( src->scale, src->x, src->y, &ll );

  height = HEIGHT_OF_LAT_DEGREE / expedia_altis_freq(src->scale) / (src->scale);
  width = height * cos ( ll.lat * DEGREES_TO_RADS );

  height += 2*REAL_HEIGHT_BUFFER;
  width  += 2*REAL_WIDTH_BUFFER;

  uri = g_strdup_printf ( "/pub/agent.dll?qscr=mrdt&ID=3XNsF.&CenP=%lf,%lf&Lang=%s&Alti=%d&Size=%d,%d&Offs=0.000000,0.000000&BCheck&tpid=1",
               ll.lat, ll.lon, (ll.lon > -30) ? "EUR0809" : "USA0409", src->scale, width, height );

  if ((res = a_http_download_get_url ( EXPEDIA_SITE, uri, dest_fn, &expedia_options )) == 0)	/* All OK */
  	expedia_snip ( dest_fn );
  g_free(uri);
  return(res);
}

