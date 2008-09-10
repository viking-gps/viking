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
#include "vikmapslayer.h"

#include "osm.h"
#include "osm-map-type.h"

/* initialisation */
void osm_init () {
  VikMapType *osmarender_type = VIK_MAP_TYPE(osm_map_type_new_with_id(12, "tah.openstreetmap.org", "/Tiles/tile/%d/%d/%d.png"));
  VikMapType *mapnik_type = VIK_MAP_TYPE(osm_map_type_new_with_id( 13, "tile.openstreetmap.org", "/%d/%d/%d.png"));
  VikMapType *maplint_type = VIK_MAP_TYPE(osm_map_type_new_with_id( 14, "tah.openstreetmap.org", "/Tiles/maplint.php/%d/%d/%d.png"));
  VikMapType *cycle_type = VIK_MAP_TYPE(osm_map_type_new_with_id( 17, "thunderflames.org/tiles/cycle/", "%d/%d/%d.png" ));

  VikMapType *bluemarble_type = VIK_MAP_TYPE(osm_map_type_new_with_id( 15, "s3.amazonaws.com", "/com.modestmaps.bluemarble/%d-r%3$d-c%2$d.jpg" ));

  VikMapType *openaerialmap_type = VIK_MAP_TYPE(osm_map_type_new_with_id( 20, "tile.openaerialmap.org", "/tiles/1.0.0/openaerialmap-900913/%d/%d/%d.jpg" ));

  maps_layer_register_type("OpenStreetMap (Osmarender)", 12, osmarender_type);
  maps_layer_register_type("OpenStreetMap (Mapnik)", 13, mapnik_type);
  maps_layer_register_type("OpenStreetMap (Maplint)", 14, maplint_type);
  maps_layer_register_type("OpenStreetMap (Cycle)", 17, cycle_type);

  maps_layer_register_type("BlueMarble", 15, bluemarble_type);
  maps_layer_register_type("OpenAerialMap", 20, openaerialmap_type);
}

