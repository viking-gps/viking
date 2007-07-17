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
#include "coords.h"
#include "vikcoord.h"

/* all coord operations MUST BE ABSTRACTED!!! */

void vik_coord_convert(VikCoord *coord, VikCoordMode dest_mode)
{
  static VikCoord tmp;
  if ( coord->mode != dest_mode )
  {
    if ( dest_mode == VIK_COORD_LATLON ) {
      a_coords_utm_to_latlon ( (struct UTM *)coord, (struct LatLon *)&tmp );
      *((struct LatLon *)coord) = *((struct LatLon *)&tmp);
    } else {
      a_coords_latlon_to_utm ( (struct LatLon *)coord, (struct UTM *)&tmp );
      *((struct UTM *)coord) = *((struct UTM *)&tmp);
    }
    coord->mode = dest_mode;
  }
}

void vik_coord_copy_convert(const VikCoord *coord, VikCoordMode dest_mode, VikCoord *dest)
{
  if ( coord->mode == dest_mode ) {
    *dest = *coord;
  } else {
    if ( dest_mode == VIK_COORD_LATLON )
      a_coords_utm_to_latlon ( (struct UTM *)coord, (struct LatLon *)dest );
    else
      a_coords_latlon_to_utm ( (struct LatLon *)coord, (struct UTM *)dest );
    dest->mode = dest_mode;
  }
}

static gdouble vik_coord_diff_safe(const VikCoord *c1, const VikCoord *c2)
{
  static struct LatLon a, b;
  vik_coord_to_latlon ( c1, &a );
  vik_coord_to_latlon ( c2, &b );
  return a_coords_latlon_diff ( &a, &b );
}

gdouble vik_coord_diff(const VikCoord *c1, const VikCoord *c2)
{
  if ( c1->mode == c2->mode )
    return vik_coord_diff_safe ( c1, c2 );
  if ( c1->mode == VIK_COORD_UTM )
    return a_coords_utm_diff ( (const struct UTM *) c1, (const struct UTM *) c2 );
  else
    return a_coords_latlon_diff ( (const struct LatLon *) c1, (const struct LatLon *) c2 );
}

void vik_coord_load_from_latlon ( VikCoord *coord, VikCoordMode mode, const struct LatLon *ll )
{
  if ( mode == VIK_COORD_LATLON )
    *((struct LatLon *)coord) = *ll;
  else
    a_coords_latlon_to_utm ( ll, (struct UTM *) coord );
  coord->mode = mode;
}

void vik_coord_load_from_utm ( VikCoord *coord, VikCoordMode mode, const struct UTM *utm )
{
  if ( mode == VIK_COORD_UTM )
    *((struct UTM *)coord) = *utm;
  else
    a_coords_utm_to_latlon ( utm, (struct LatLon *) coord );
  coord->mode = mode;
}

void vik_coord_to_latlon ( const VikCoord *coord, struct LatLon *dest )
{
  if ( coord->mode == VIK_COORD_LATLON )
    *dest = *((const struct LatLon *)coord);
  else
    a_coords_utm_to_latlon ( (const struct UTM *) coord, dest );
}

void vik_coord_to_utm ( const VikCoord *coord, struct UTM *dest )
{
  if ( coord->mode == VIK_COORD_UTM )
    *dest = *((const struct UTM *)coord);
  else
    a_coords_latlon_to_utm ( (const struct LatLon *) coord, dest );
}

gboolean vik_coord_equals ( const VikCoord *coord1, const VikCoord *coord2 )
{
  if ( coord1->mode != coord2->mode )
    return FALSE;
  if ( coord1->mode == VIK_COORD_LATLON )
    return coord1->north_south == coord2->north_south && coord1->east_west == coord2->east_west;
  else /* VIK_COORD_UTM */
    return coord1->utm_zone == coord2->utm_zone && coord1->north_south == coord2->north_south && coord1->east_west == coord2->east_west;
}

static void get_north_west(struct LatLon *center, struct LatLon *dist, struct LatLon *nw) 
{
  nw->lat = center->lat + dist->lat;
  nw->lon = center->lon - dist->lon;
  if (nw->lon < -180)
    nw->lon += 360.0;
  if (nw->lat > 90.0) {  /* over north pole */
    nw->lat = 180 - nw->lat;
    nw->lon = nw->lon - 180;
  }
}

static void get_south_east(struct LatLon *center, struct LatLon *dist, struct LatLon *se) 
{
  se->lat = center->lat - dist->lat;
  se->lon = center->lon + dist->lon;
  if (se->lon > 180.0)
    se->lon -= 360.0;
  if (se->lat < -90.0) {  /* over south pole */
    se->lat += 180;
    se->lon = se->lon - 180;
  }
}

void vik_coord_set_area(const VikCoord *coord, const struct LatLon *wh, VikCoord *tl, VikCoord *br)
{
  struct LatLon center, nw, se;
  struct LatLon dist;

  dist.lat = wh->lat/2;
  dist.lon = wh->lon/2;

  vik_coord_to_latlon(coord, &center);
  get_north_west(&center, &dist, &nw);
  get_south_east(&center, &dist, &se);

  *((struct LatLon *)tl) = nw;
  *((struct LatLon *)br) = se;
  tl->mode = br->mode = VIK_COORD_LATLON;
}

gboolean vik_coord_inside(const VikCoord *coord, const VikCoord *tl, const VikCoord *br)
{
  struct LatLon ll, tl_ll, br_ll;

  vik_coord_to_latlon(coord, &ll);
  vik_coord_to_latlon(tl, &tl_ll);
  vik_coord_to_latlon(br, &br_ll);

  if ((ll.lat > tl_ll.lat) || (ll.lon < tl_ll.lon))
    return FALSE;
  if ((ll.lat  < br_ll.lat) || (ll.lon > br_ll.lon))
    return FALSE;
  return TRUE;
}
