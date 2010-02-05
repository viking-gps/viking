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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "terraserver.h"
#include "terraservermapsource.h"
#include "vikmapslayer.h"

void terraserver_init () {
  VikMapSource *map_type_1 = VIK_MAP_SOURCE(terraserver_map_source_new_with_id( 2, "Terraserver Topos", 2 ));
  VikMapSource *map_type_2 = VIK_MAP_SOURCE(terraserver_map_source_new_with_id( 1, "Terraserver Aerials", 1 ));
  VikMapSource *map_type_3 = VIK_MAP_SOURCE(terraserver_map_source_new_with_id( 4, "Terraserver Urban Areas", 4 ));

  maps_layer_register_map_source (map_type_1);
  maps_layer_register_map_source (map_type_2);
  maps_layer_register_map_source (map_type_3);
}
