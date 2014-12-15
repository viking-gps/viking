/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
#ifndef __MAP_IDS_H
#define __MAP_IDS_H
 
// OLD Terraserver ids - listed for compatibility
#define MAP_ID_TERRASERVER_AERIAL 1
#define MAP_ID_TERRASERVER_TOPO 2
#define MAP_ID_TERRASERVER_URBAN 4
 
#define MAP_ID_EXPEDIA 5

#define MAP_ID_MAPNIK_RENDER 7
 
// Mostly OSM related - except the Blue Marble value
#define MAP_ID_OSM_MAPNIK 13
#define MAP_ID_BLUE_MARBLE 15
#define MAP_ID_OSM_CYCLE 17
#define MAP_ID_MAPQUEST_OSM 19
#define MAP_ID_OSM_TRANSPORT 20
#define MAP_ID_OSM_ON_DISK 21
#define MAP_ID_OSM_HUMANITARIAN 22
#define MAP_ID_MBTILES 23
#define MAP_ID_OSM_METATILES 24
 
#define MAP_ID_BING_AERIAL 212
 
// Unfortunately previous ID allocations have been a little haphazard,
//  but hopefully future IDs can be follow this scheme:
//   0 to 31 are intended for hard coded internal defaults
//   32-127 are intended for XML configuration map supplied defaults: see data/maps.xml
//   128 and above are intended for end user configurations.

#endif
