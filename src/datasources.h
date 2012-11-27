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
#ifndef __VIK_DATASOURCES_H
#define __VIK_DATASOURCES_H

#include "acquire.h"

G_BEGIN_DECLS

extern VikDataSourceInterface vik_datasource_gps_interface;
extern VikDataSourceInterface vik_datasource_file_interface;
#ifdef VIK_CONFIG_GOOGLE
extern VikDataSourceInterface vik_datasource_google_interface;
#endif
#ifdef VIK_CONFIG_OPENSTREETMAP
extern VikDataSourceInterface vik_datasource_osm_interface;
extern VikDataSourceInterface vik_datasource_osm_my_traces_interface;
#endif
#ifdef VIK_CONFIG_GEOCACHES
extern VikDataSourceInterface vik_datasource_gc_interface;
#endif
#ifdef VIK_CONFIG_GEOTAG
extern VikDataSourceInterface vik_datasource_geotag_interface;
#endif
#ifdef VIK_CONFIG_GEONAMES
extern VikDataSourceInterface vik_datasource_wikipedia_interface;
#endif

G_BEGIN_DECLS

#endif
