/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2014, Rob Norris <rw_norris@hotmail.com>
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
#include "maputils.h"
#include "globals.h"
#include <math.h>

// World Scale: VIK_GZ(17)
//  down to
// Submeter scale: 1/VIK_GZ(5)
// No map provider is going to have tiles at the highest zoom in level - but we can interpolate to that.

static const gdouble scale_mpps[] = { VIK_GZ(0), VIK_GZ(1), VIK_GZ(2), VIK_GZ(3), VIK_GZ(4), VIK_GZ(5),
                                      VIK_GZ(6), VIK_GZ(7), VIK_GZ(8), VIK_GZ(9), VIK_GZ(10), VIK_GZ(11),
                                      VIK_GZ(12), VIK_GZ(13), VIK_GZ(14), VIK_GZ(15), VIK_GZ(16), VIK_GZ(17) };
static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

static const gdouble scale_neg_mpps[] = { 1.0/VIK_GZ(0), 1.0/VIK_GZ(1), 1.0/VIK_GZ(2),
                                          1.0/VIK_GZ(3), 1.0/VIK_GZ(4), 1.0/VIK_GZ(5) };
static const gint num_scales_neg = (sizeof(scale_neg_mpps) / sizeof(scale_neg_mpps[0]));

#define ERROR_MARGIN 0.01
/**
 * map_utils_mpp_to_scale:
 * @mpp: The so called 'mpp'
 *
 * Returns: the zoom scale value which may be negative.
 */
gint map_utils_mpp_to_scale ( gdouble mpp ) {
	gint i;
	for ( i = 0; i < num_scales; i++ ) {
		if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN ) {
			return i;
		}
	}
	for ( i = 0; i < num_scales_neg; i++ ) {
		if ( ABS(scale_neg_mpps[i] - mpp) < 0.000001 ) {
			return -i;
		}
	}

	return 255;
}

/**
 * map_utils_mpp_to_zoom_level:
 * @mpp: The so called 'mpp'
 *
 * Returns: a Zoom Level
 *  See: http://wiki.openstreetmap.org/wiki/Zoom_levels
 */
guint8 map_utils_mpp_to_zoom_level ( gdouble mpp )
{
	gint answer = 17 - map_utils_mpp_to_scale ( mpp );
	if ( answer < 0 )
		answer = 17;
	return answer;
}

/**
 * SECTION:maputils
 * @short_description: Notes about TMS / Spherical Mercator conversion
 *
 * VikCoords are in Spherical Mercator projection (#VIK_COORD_LATLON)
 * MapCoords are in Inverse TMS
 *
 * See: http://docs.openlayers.org/library/spherical_mercator.html
 * See: http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
 * NB: the Y axis is inverted, ie the origin is at top-left corner.
 */

/**
 * map_utils_vikcoord_to_iTMS:
 * @src:   Original #VikCoord in #VIK_COORD_LATLON format
 * @xzoom: Viking zoom level in x direction
 * @yzoom: Viking zoom level in y direction (actually needs to be same as xzoom)
 * @dest:  The resulting Inverse TMS coordinates in #MapCoord
 *
 * Convert a #VikCoord in VIK_COORD_LATLON format into Inverse TMS coordinates
 *
 * Returns: whether the conversion was performed
 */
gboolean map_utils_vikcoord_to_iTMS ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  if ( src->mode != VIK_COORD_LATLON )
    return FALSE;

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = map_utils_mpp_to_scale ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;

  dest->x = (src->east_west + 180) / 360 * VIK_GZ(17) / xzoom;
  dest->y = (180 - MERCLAT(src->north_south)) / 360 * VIK_GZ(17) / xzoom;
  dest->z = 0;

  return TRUE;
}

// Internal convenience function
static void _to_vikcoord_with_offset ( const MapCoord *src, VikCoord *dest, gdouble offset )
{
  gdouble socalled_mpp;
  if (src->scale >= 0)
    socalled_mpp = VIK_GZ(src->scale);
  else
    socalled_mpp = 1.0/VIK_GZ(-src->scale);
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = ((src->x+offset) / VIK_GZ(17) * socalled_mpp * 360) - 180;
  dest->north_south = DEMERCLAT(180 - ((src->y+offset) / VIK_GZ(17) * socalled_mpp * 360));
}

/**
 * map_utils_iTMS_to_center_vikcoord:
 * @src:   Original #MapCoord in Inverse TMS format
 * @dest:  The resulting Spherical Mercator coordinates in #VikCoord
 *
 * Convert a #MapCoord in Inverse TMS format into Spherical Mercator coordinates for the center of the TMS area
 *
 * Returns: whether the conversion was performed
 */
void map_utils_iTMS_to_center_vikcoord ( const MapCoord *src, VikCoord *dest )
{
	_to_vikcoord_with_offset ( src, dest, 0.5 );
}

/**
 * map_utils_iTMS_to_vikcoord:
 * @src:   Original #MapCoord in Inverse TMS format
 * @dest:  The resulting Spherical Mercator coordinates in #VikCoord
 *
 * Convert a #MapCoord in Inverse TMS format into Spherical Mercator coordinates
 *  (for the top left corner of the Inverse TMS area)
 *
 * Returns: whether the conversion was performed
 */
void map_utils_iTMS_to_vikcoord ( const MapCoord *src, VikCoord *dest )
{
	_to_vikcoord_with_offset ( src, dest, 0.0 );
}
