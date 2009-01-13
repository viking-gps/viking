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

#include "osm.h"
#include "vikmapslayer.h"
#include "slippy-map-type.h"

/* initialisation */
void osm_init () {
  VikMapType *osmarender_type = VIK_MAP_TYPE(slippy_map_type_new_with_id(12, "tah.openstreetmap.org", "/Tiles/tile/%d/%d/%d.png"));
  VikMapType *mapnik_type = VIK_MAP_TYPE(slippy_map_type_new_with_id( 13, "tile.openstreetmap.org", "/%d/%d/%d.png"));
  VikMapType *maplint_type = VIK_MAP_TYPE(slippy_map_type_new_with_id( 14, "tah.openstreetmap.org", "/Tiles/maplint.php/%d/%d/%d.png"));
  VikMapType *cycle_type = VIK_MAP_TYPE(slippy_map_type_new_with_id( 17, "thunderflames.org/tiles/cycle/", "%d/%d/%d.png" ));

  maps_layer_register_map_type("OpenStreetMap (Osmarender)", osmarender_type);
  maps_layer_register_map_type("OpenStreetMap (Mapnik)", mapnik_type);
  maps_layer_register_map_type("OpenStreetMap (Maplint)", maplint_type);
  maps_layer_register_map_type("OpenStreetMap (Cycle)", cycle_type);
}

