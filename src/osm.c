/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007,2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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

#include <glib/gi18n.h>

#include "osm.h"
#include "map_ids.h"
#include "vikmapslayer.h"
#include "vikslippymapsource.h"
#include "vikwmscmapsource.h"
#include "vikwebtoolcenter.h"
#include "vikwebtoolbounds.h"
#include "vikwebtoolformat.h"
#include "vikwebtool_datasource.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "vikgotoxmltool.h"
#include "vikgoto.h"
#include "vikrouting.h"
#include "vikroutingwebengine.h"

/* initialisation */
void osm_init () {
  VikMapSource *mapnik_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_OSM_MAPNIK,
                                "label", _("OpenStreetMap (Mapnik)"),
                                "name", "OSM-Mapnik",
                                "hostname", "tile.openstreetmap.org",
                                "url", "/%d/%d/%d.png",
                                "check-file-server-time", FALSE,
                                "use-etag", TRUE,
                                "zoom-min", 0,
                                "zoom-max", 19,
                                "copyright", "© OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));
  VikMapSource *cycle_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_OSM_CYCLE,
                                "label", _("OpenStreetMap (Cycle)"),
                                "name", "OSM-Cycle",
                                "url", "https://tile.thunderforest.com/cycle/%d/%d/%d.png?apikey="VIK_CONFIG_THUNDERFOREST_KEY,
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "zoom-min", 0,
                                "zoom-max", 18,
                                "copyright", "Tiles courtesy of Andy Allan © OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));
  VikMapSource *transport_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_OSM_TRANSPORT,
                                "label", _("OpenStreetMap (Transport)"),
                                "name", "OSM-Transport",
                                "hostname", "tile2.opencyclemap.org",
                                "url", "/transport/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "zoom-min", 0,
                                "zoom-max", 18,
                                "copyright", "Tiles courtesy of Andy Allan © OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));
  VikMapSource *hot_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_OSM_HUMANITARIAN,
                                "name", "OSM-Humanitarian",
                                "label", _("OpenStreetMap (Humanitarian)"),
                                "hostname", "c.tile.openstreetmap.fr",
                                "url", "/hot/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "zoom-min", 0,
                                "zoom-max", 20, // Super detail!!
                                "copyright", "© OpenStreetMap contributors. Tiles courtesy of Humanitarian OpenStreetMap Team",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));

  // NB no cache needed for this type!!
  VikMapSource *direct_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_OSM_ON_DISK,
                                "label", _("On Disk OSM Tile Format"),
                                // For using your own generated data assumed you know the license already!
                                "copyright", "© OpenStreetMap contributors", // probably
                                "use-direct-file-access", TRUE,
                                NULL));

  // NB no cache needed for this type!!
  VikMapSource *mbtiles_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_MBTILES,
                                "label", _("MBTiles File"),
                                // For using your own generated data assumed you know the license already!
                                "copyright", "© OpenStreetMap contributors", // probably
                                "use-direct-file-access", TRUE,
                                "is-mbtiles", TRUE,
                                NULL));

  // NB no cache needed for this type!!
  VikMapSource *metatiles_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_OSM_METATILES,
                                "label", _("OSM Metatiles"),
                                // For using your own generated data assumed you know the license already!
                                "copyright", "© OpenStreetMap contributors", // probably
                                "use-direct-file-access", TRUE,
                                "is-osm-meta-tiles", TRUE,
                                NULL));

  // Note using a registered token for the Mapbox Tileservice
  // Thus not only will the (free) service allocation limit be reached by normal users
  //  but by anymore who cares to read these sources and use the default themselves.
  VikMapSource *mapbox_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", MAP_ID_MAPBOX_OUTDOORS,
                                "name", "Mapbox-Outdoors",
                                "label", _("Mapbox Outdoors"),
                                "url", "https://api.tiles.mapbox.com/styles/v1/mapbox/outdoors-v9/tiles/256/%d/%d/%d?access_token="VIK_CONFIG_MAPBOX_TOKEN,
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "zoom-min", 0,
                                "zoom-max", 19,
                                "copyright", "© Mapbox © OpenStreetMap",
                                "license", _("Mapbox Specific"),
                                "license-url", "https://www.mapbox.com/tos",
                                NULL));

  // NB The first registered map source is the default
  //  (unless the user has specified Map Layer defaults)
  maps_layer_register_map_source (mapbox_type);
  maps_layer_register_map_source (mapnik_type);
  maps_layer_register_map_source (cycle_type);
  maps_layer_register_map_source (transport_type);
  maps_layer_register_map_source (hot_type);
  maps_layer_register_map_source (direct_type);
  maps_layer_register_map_source (mbtiles_type);
  maps_layer_register_map_source (metatiles_type);

  // Webtools
  VikWebtoolCenter *webtool = NULL;
  webtool = vik_webtool_center_new_with_members ( _("OSM (view)"), "http://www.openstreetmap.org/?lat=%s&lon=%s&zoom=%d" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  webtool = vik_webtool_center_new_with_members ( _("OSM (edit)"), "http://www.openstreetmap.org/edit?lat=%s&lon=%s&zoom=%d" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  // Note the use of positional parameters
  webtool = vik_webtool_center_new_with_members ( _("OSM (query)"), "http://www.openstreetmap.org/query?lat=%1$s&lon=%2$s#map=%3$d/%1$s/%2$s" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  webtool = vik_webtool_center_new_with_members ( _("OSM (render)"), "http://www.informationfreeway.org/?lat=%s&lon=%s&zoom=%d&layers=B0000F000F" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  VikWebtoolBounds *webtoolbounds = NULL;
  // Example: http://127.0.0.1:8111/load_and_zoom?left=8.19&right=8.20&top=48.605&bottom=48.590&select=node413602999
  // JOSM or merkaartor must already be running with remote interface enabled
  webtoolbounds = vik_webtool_bounds_new_with_members ( _("Local port 8111 (eg JOSM)"), "http://localhost:8111/load_and_zoom?left=%s&right=%s&bottom=%s&top=%s" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtoolbounds ) );
  g_object_unref ( webtoolbounds );

  VikWebtoolFormat *vwtf = NULL;
  vwtf = vik_webtool_format_new_with_members ( _("Geofabrik Map Compare"),
                                               "http://tools.geofabrik.de/mc/#%s/%s/%s",
                                               "ZAO" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( vwtf ) );
  g_object_unref ( vwtf );

  // Datasource
  VikWebtoolDatasource *vwtds = NULL;
  vwtds = vik_webtool_datasource_new_with_members ( _("OpenStreetMap Notes"), "https://api.openstreetmap.org/api/0.6/notes.gpx?bbox=%s,%s,%s,%s&amp;closed=0", "LBRT", NULL, NULL, NULL );
  vik_ext_tool_datasources_register ( VIK_EXT_TOOL ( vwtds ) );
  g_object_unref ( vwtds );

  // Goto
  VikGotoXmlTool *nominatim = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM Nominatim",
    "url-format", "https://nominatim.openstreetmap.org/search?q=%s&format=xml",
    "lat-path", "/searchresults/place",
    "lat-attr", "lat",
    "lon-path", "/searchresults/place",
    "lon-attr", "lon",
    "desc-path", "/searchresults/place",
    "desc-attr", "display_name",
    NULL ) );
    vik_goto_register ( VIK_GOTO_TOOL ( nominatim ) );
    g_object_unref ( nominatim );

  // Not really OSM but can't be bothered to create somewhere else to put it...
  webtool = vik_webtool_center_new_with_members ( _("Wikimedia Toolserver GeoHack"), "http://tools.wmflabs.org/geohack/geohack.php?params=%s;%s" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );
}

