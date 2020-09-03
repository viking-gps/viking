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

#include "coords.h"
#include "vikcoord.h"
#include "globals.h"
#include <math.h>

/* all coord operations MUST BE ABSTRACTED!!! */

void vik_coord_convert(VikCoord *coord, VikCoordMode dest_mode)
{
  VikCoord tmp;
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
  struct LatLon a, b;
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

/**
 * vik_coord_equalish:
 * A more sensible comparsion that allows for some small tolerance in comparing floating point numbers
 */
gboolean vik_coord_equalish ( const VikCoord *coord1, const VikCoord *coord2 )
{
  static const gdouble TOLERANCE = 0.000005; // ATM use for both coordinate modes
  if ( coord1->mode != coord2->mode )
    return FALSE;
  if ( coord1->mode == VIK_COORD_LATLON )
    return coord1->north_south >= coord2->north_south - TOLERANCE &&
           coord1->north_south <= coord2->north_south + TOLERANCE &&
	   coord1->east_west >= coord2->east_west - TOLERANCE &&
	   coord1->east_west <= coord2->east_west + TOLERANCE;
  else /* VIK_COORD_UTM */
    return coord1->utm_zone == coord2->utm_zone &&
           coord1->north_south >= coord2->north_south - TOLERANCE &&
           coord1->north_south <= coord2->north_south + TOLERANCE &&
           coord1->east_west >= coord2->east_west - TOLERANCE &&
           coord1->east_west <= coord2->east_west + TOLERANCE;
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

/**
 * vik_coord_angle:
 *
 * Get angle of the second coord from the first one in degrees
 *
 */
gdouble vik_coord_angle (const VikCoord *vc1, const VikCoord *vc2)
{
  struct LatLon ll1, ll2;
  vik_coord_to_latlon ( vc1, &ll1 );
  vik_coord_to_latlon ( vc2, &ll2 );

  // Convert to radians for use in the algorithm
  gdouble rlat1 = DEG2RAD(ll1.lat);
  gdouble rlong1 = DEG2RAD(ll1.lon);
  gdouble rlat2 = DEG2RAD(ll2.lat);
  gdouble rlong2 = DEG2RAD(ll2.lon);

  // This formula is for the initial bearing (ometimes referred to as forward azimuth)
  //θ = atan2( sin Δλ ⋅ cos φ2 , cos φ1 ⋅ sin φ2 − sin φ1 ⋅ cos φ2 ⋅ cos Δλ )
  // in -pi to +pi radians
  gdouble dLon = (rlong2 - rlong1);
  gdouble y = sin(dLon) * cos(rlat2);
  gdouble x = (cos(rlat1) * sin(rlat2)) - (sin(rlat1) * cos(rlat2) * cos(dLon));
  gdouble angle = atan2(y, x);

  // Bring into range 0..360 degrees
  angle = fmod (RAD2DEG(angle) + 360.0, 360);
  return angle;
}

/**
 * vik_coord_angle_end:
 *
 * Get angle of first coord from the second one in degrees (final bearing). Basically the inverse of vik_coord_angle.
 *
 */
gdouble vik_coord_angle_end (const VikCoord *vc1, const VikCoord *vc2)
{
  struct LatLon ll1, ll2;
  vik_coord_to_latlon ( vc1, &ll1 );
  vik_coord_to_latlon ( vc2, &ll2 );

  // Convert to radians for use in the algorithm
  gdouble rlat1 = DEG2RAD(ll1.lat);
  gdouble rlong1 = DEG2RAD(ll1.lon);
  gdouble rlat2 = DEG2RAD(ll2.lat);
  gdouble rlong2 = DEG2RAD(ll2.lon);

  // This formula is for the final bearing
  //θ = atan2( sin Δλ ⋅ cos φ2 , cos φ1 ⋅ sin φ2 − sin φ1 ⋅ cos φ2 ⋅ cos Δλ )
  // in -pi to +pi radians
  gdouble dLon = (rlong2 - rlong1);
  gdouble y = sin(dLon) * cos(rlat1);
  gdouble x = (-cos(rlat2) * sin(rlat1)) + (sin(rlat2) * cos(rlat1) * cos(dLon));
  gdouble angle = atan2(y, x);

  // Bring into range 0..360 degrees
  angle = fmod (RAD2DEG(angle) + 360.0, 360);
  return angle;
}

/**
 * vik_coord_geodesic_coord:
 *
 * Calculate the geodesic coordinate at angular distance ratio n between vc1 and vc2. n = 0 returns vc1, n = 1 returns vc2.
 * Used for drawing great circles for the ruler tool.
 * For the relevant formulas, refer to https://en.wikipedia.org/wiki/Great-circle_navigation
 *
 */
void vik_coord_geodesic_coord (const VikCoord *vc1, const VikCoord *vc2, gdouble n, VikCoord *rvc)
{
  struct LatLon ll, ll1, ll2;
  vik_coord_to_latlon ( vc1, &ll1 );
  vik_coord_to_latlon ( vc2, &ll2 );

  gdouble rlat1 = DEG2RAD(ll1.lat);
  gdouble rlong1 = DEG2RAD(ll1.lon);
  gdouble rlat2 = DEG2RAD(ll2.lat);
  gdouble rlong2 = DEG2RAD(ll2.lon);
  gdouble dLon = rlong2 - rlong1;

  gdouble bearing1 = DEG2RAD(vik_coord_angle ( vc1, vc2 ));


  gdouble bearing0 = atan2(sin(bearing1) * cos(rlat1), sqrt(pow(cos(bearing1), 2) + pow(sin(bearing1)*sin(rlat1), 2)));

  gdouble sigma_01 = atan2(tan(rlat1), cos(bearing1));
  gdouble sigma_12 = atan2(sqrt(pow(cos(rlat1)*sin(rlat2) - sin(rlat1)*cos(rlat2)*cos(dLon), 2) + pow(cos(rlat2)*sin(dLon), 2)),
                           sin(rlat1)*sin(rlat2) + cos(rlat1)*cos(rlat2)*cos(dLon));

  gdouble sigma = sigma_01 + n * sigma_12;
  gdouble rlong0 = rlong1 - atan2(sin(bearing0)*sin(sigma_01), cos(sigma_01));

  ll.lat = atan(cos(bearing0)*sin(sigma) / (sqrt(pow(cos(sigma), 2) + pow(sin(bearing0)*sin(sigma), 2))));
  ll.lon = rlong0 + atan2(sin(bearing0)*sin(sigma), cos(sigma));

  ll.lon = fmod(ll.lon + 2*M_PI, 2*M_PI);
  if (ll.lon > M_PI) {
    ll.lon -= 2*M_PI;
  }

  VikCoord coord;
  coord.north_south = RAD2DEG(ll.lat);
  coord.east_west = RAD2DEG(ll.lon);
  coord.mode = VIK_COORD_LATLON;
  vik_coord_copy_convert ( &coord, vc1->mode, rvc );
}
