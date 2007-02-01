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
#include "http.h"
#include "vikmapslayer.h"

#include "terraserver.h"

void terraserver_init () {
  VikMapsLayer_MapType map_type_1 = { 2, 200, 200, VIK_VIEWPORT_DRAWMODE_UTM, terraserver_topo_coord_to_mapcoord, terraserver_mapcoord_to_center_coord, terraserver_topo_download };
  VikMapsLayer_MapType map_type_2 = { 1, 200, 200, VIK_VIEWPORT_DRAWMODE_UTM, terraserver_aerial_coord_to_mapcoord, terraserver_mapcoord_to_center_coord, terraserver_aerial_download };
  VikMapsLayer_MapType map_type_3 = { 4, 200, 200, VIK_VIEWPORT_DRAWMODE_UTM, terraserver_urban_coord_to_mapcoord, terraserver_mapcoord_to_center_coord, terraserver_urban_download };

  maps_layer_register_type("Terraserver Topos", 2, &map_type_1);
  maps_layer_register_type("Terraserver Aerials", 1, &map_type_2);
  maps_layer_register_type("Terraserver Urban Areas", 4, &map_type_3);
}

#define TERRASERVER_SITE "terraserver-usa.com"
#define MARGIN_OF_ERROR 0.001

static int mpp_to_scale ( gdouble mpp, guint8 type )
{
  mpp *= 4;
  gint t = (gint) mpp;
  if ( ABS(mpp - t) > MARGIN_OF_ERROR )
    return FALSE;

  switch ( t ) {
    case 1: return (type == 4) ? 8 : 0;
    case 2: return (type == 4) ? 9 : 0;
    case 4: return (type != 2) ? 10 : 0;
    case 8: return 11;
    case 16: return 12;
    case 32: return 13;
    case 64: return 14;
    case 128: return 15;
    case 256: return 16;
    case 512: return 17;
    case 1024: return 18;
    case 2048: return 19;
    default: return 0;
  }
}

static gdouble scale_to_mpp ( gint scale )
{
  return pow(2,scale - 10);
}

static gboolean terraserver_coord_to_mapcoord ( const VikCoord *src, gdouble xmpp, gdouble ympp, MapCoord *dest, guint8 type )
{
  g_assert ( src->mode == VIK_COORD_UTM );

  if ( xmpp != ympp )
    return FALSE;

  dest->scale = mpp_to_scale ( xmpp, type );
  if ( ! dest->scale )
    return FALSE;

  dest->x = (gint)(((gint)(src->east_west))/(200*xmpp));
  dest->y = (gint)(((gint)(src->north_south))/(200*xmpp));
  dest->z = src->utm_zone;
  return TRUE;
}

gboolean terraserver_topo_coord_to_mapcoord ( const VikCoord *src, gdouble xmpp, gdouble ympp, MapCoord *dest )
{ return terraserver_coord_to_mapcoord ( src, xmpp, ympp, dest, 2 ); }
gboolean terraserver_aerial_coord_to_mapcoord ( const VikCoord *src, gdouble xmpp, gdouble ympp, MapCoord *dest )
{ return terraserver_coord_to_mapcoord ( src, xmpp, ympp, dest, 1 ); }
gboolean terraserver_urban_coord_to_mapcoord ( const VikCoord *src, gdouble xmpp, gdouble ympp, MapCoord *dest )
{ return terraserver_coord_to_mapcoord ( src, xmpp, ympp, dest, 4 ); }

void terraserver_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest )
{
  // FIXME: slowdown here!
  gdouble mpp = scale_to_mpp ( src->scale );
  dest->mode = VIK_COORD_UTM;
  dest->utm_zone = src->z;
  dest->east_west = ((src->x * 200) + 100) * mpp;
  dest->north_south = ((src->y * 200) + 100) * mpp;
}

static void terraserver_download ( MapCoord *src, const gchar *dest_fn, guint8 type )
{
  gchar *uri = g_strdup_printf ( "/tile.ashx?T=%d&S=%d&X=%d&Y=%d&Z=%d", type,
                                  src->scale, src->x, src->y, src->z );
  a_http_download_get_url_nohostname ( TERRASERVER_SITE, uri, dest_fn );
  g_free ( uri );
}

void terraserver_topo_download ( MapCoord *src, const gchar *dest_fn )
{ terraserver_download ( src, dest_fn, 2 ); }
void terraserver_aerial_download ( MapCoord *src, const gchar *dest_fn )
{ terraserver_download ( src, dest_fn, 1 ); }
void terraserver_urban_download ( MapCoord *src, const gchar *dest_fn )
{ terraserver_download ( src, dest_fn, 4 ); }
