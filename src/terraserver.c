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

#include "terraserver.h"
#include "terraserver-map-type.h"
#include "vikmapslayer.h"

void terraserver_init () {
  VikMapSource *map_type_1 = VIK_MAP_SOURCE(terraserver_map_type_new_with_id( 2, 2 ));
  VikMapSource *map_type_2 = VIK_MAP_SOURCE(terraserver_map_type_new_with_id( 1, 1 ));
  VikMapSource *map_type_3 = VIK_MAP_SOURCE(terraserver_map_type_new_with_id( 4, 4 ));

  maps_layer_register_map_source ("Terraserver Topos", map_type_1);
  maps_layer_register_map_source ("Terraserver Aerials", map_type_2);
  maps_layer_register_map_source ("Terraserver Urban Areas", map_type_3);
}
