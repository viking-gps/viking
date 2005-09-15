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
#include "coords.h"
#include "vikcoord.h"
#include "mapcoord.h"
#include "http.h"

#define MARGIN_OF_ERROR 0.0001
#define REALLYCLOSE(x,y) (ABS((x)-(y)) < MARGIN_OF_ERROR)

#define USGS_SCALE_TO_MPP .1016
#define TZ(x) ((x)*USGS_SCALE_TO_MPP)

/* defined as "2" = "25k" */
static gint mpp_to_scale ( gdouble mpp )
{
  if ( REALLYCLOSE(mpp,TZ(10)) || REALLYCLOSE(mpp,TZ(24)) || REALLYCLOSE(mpp,TZ(25)) || REALLYCLOSE(mpp,TZ(50)) || REALLYCLOSE(mpp,TZ(100)) || REALLYCLOSE(mpp,TZ(200)) || REALLYCLOSE(mpp,TZ(250)) || REALLYCLOSE(mpp,TZ(500)) )
    return (gint) (mpp / .1016);
  return 0;
}

gboolean usgs_coord_to_mapcoord ( const VikCoord *src, gdouble xmpp, gdouble ympp, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_UTM );

  if ( xmpp != ympp )
    return FALSE;

  dest->scale = mpp_to_scale ( xmpp );
  if ( ! dest->scale )
    return FALSE;

  dest->x = (gint)(((gint)(src->east_west))/(800*xmpp));
  dest->y = (gint)(((gint)(src->north_south))/(600*xmpp));
  dest->z = src->utm_zone;
  return TRUE;
}

void usgs_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest )
{
  gdouble mpp = TZ ( src->scale );
  dest->mode = VIK_COORD_UTM;
  dest->utm_zone = src->z;
  dest->east_west = ((src->x * 800) + 400) * mpp;
  dest->north_south = ((src->y * 600) + 300) * mpp;
}

gint usgs_scale_to_drg ( gint scale )
{
  switch ( scale ) {
    case 10:
    case 25:
    case 50:
      return 25;
    case 100:
    case 200:
      return 100;
    default:
      return 250;
  }
}

  static const char *usgs_scale_factor() {
    static char str[11];
    static int i = 0;
    snprintf(str,sizeof(str),"%d%d%d", 044, 393, 0xA573);
    return str;
  }


void usgs_download ( MapCoord *src, const gchar *dest_fn )
{
  /* find center as above */
  gdouble mpp = TZ ( src->scale );
  gint easting = ((src->x * 800) + 400) * mpp;
  gint northing = ((src->y * 600) + 300) * mpp;
  gchar *uri = g_strdup_printf ( "/map.asp?z=%d&e=%d&n=%d&datum=NAD83&u=4&size=l&s=%d&chkDRG=DRG%d",
                                 src->z, easting, northing, src->scale, usgs_scale_to_drg(src->scale) );
  usgs_hack ( usgs_scale_factor(), uri, dest_fn );
  g_free ( uri );
}
