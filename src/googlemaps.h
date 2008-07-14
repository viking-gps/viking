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

#ifndef __VIKING_GOOGLEMAPS_H
#define __VIKING_GOOGLEMAPS_H

#include <glib.h>

#include "vikcoord.h"
#include "mapcoord.h"

void googlemaps_init ();

guint8 googlemaps_zoom ( gdouble mpp );

/* a bit misleading, this is the "mpp" (really just set zoom level, very
 * roughly equivalent so you can easily switch between maps) of
 * google maps 1, the second google maps level (1st is 0). */
#define GOOGLEMAPS_ZOOM_ONE_MPP 2.0

gboolean googlemaps_coord_to_mapcoord ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
void googlemaps_mapcoord_to_center_coord ( MapCoord *src, VikCoord *dest );
int googlemaps_download ( MapCoord *src, const gchar *dest_fn );

#endif
