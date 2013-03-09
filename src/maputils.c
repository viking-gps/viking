/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
