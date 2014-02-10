/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007,2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2012, Rob Norris <rw_norris@hotmail.com>
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
#include "vikmapslayer.h"
#include "vikslippymapsource.h"
#include "vikwmscmapsource.h"
#include "vikwebtoolcenter.h"
#include "vikwebtoolbounds.h"
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
                                "id", 13,
                                "label", "OpenStreetMap (Mapnik)",
                                "hostname", "tile.openstreetmap.org",
                                "url", "/%d/%d/%d.png",
                                "check-file-server-time", FALSE,
                                "use-etag", TRUE,
                                "copyright", "© OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));
  VikMapSource *cycle_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 17,
                                "label", "OpenStreetMap (Cycle)",
                                "hostname", "b.tile.opencyclemap.org",
                                "url", "/cycle/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "copyright", "Tiles courtesy of Andy Allan © OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));
  VikMapSource *transport_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 20,
                                "label", "OpenStreetMap (Transport)",
                                "hostname", "c.tile2.opencyclemap.org",
                                "url", "/transport/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "copyright", "Tiles courtesy of Andy Allan © OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));

  VikMapSource *mapquest_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 19,
                                "label", "OpenStreetMap (MapQuest)",
                                "hostname", "otile1.mqcdn.com",
                                "url", "/tiles/1.0.0/osm/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "copyright", "Tiles Courtesy of MapQuest © OpenStreetMap contributors",
                                "license", "MapQuest Specific",
                                "license-url", "http://developer.mapquest.com/web/info/terms-of-use",
                                NULL));
  VikMapSource *hot_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 22,
                                "label", "OpenStreetMap (Humanitarian)",
                                "hostname", "c.tile.openstreetmap.fr",
                                "url", "/hot/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "copyright", "© OpenStreetMap contributors. Tiles courtesy of Humanitarian OpenStreetMap Team",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));

  // NB no cache needed for this type!!
  VikMapSource *direct_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 21,
                                "label", _("On Disk OSM Tile Format"),
                                // For using your own generated data assumed you know the license already!
                                "copyright", "© OpenStreetMap contributors", // probably
                                "use-direct-file-access", TRUE,
                                NULL));

  maps_layer_register_map_source (mapquest_type);
  maps_layer_register_map_source (mapnik_type);
  maps_layer_register_map_source (cycle_type);
  maps_layer_register_map_source (transport_type);
  maps_layer_register_map_source (hot_type);
  maps_layer_register_map_source (direct_type);

  // Webtools
  VikWebtoolCenter *webtool = NULL;
  webtool = vik_webtool_center_new_with_members ( _("OSM (view)"), "http://openstreetmap.org/?lat=%s&lon=%s&zoom=%d" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  webtool = vik_webtool_center_new_with_members ( _("OSM (edit)"), "http://www.openstreetmap.org/edit?lat=%s&lon=%s&zoom=%d" );
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

  // Datasource
  VikWebtoolDatasource *vwtds = NULL;
  vwtds = vik_webtool_datasource_new_with_members ( _("OpenStreetMap Notes"), "http://api.openstreetmap.org/api/0.6/notes.gpx?bbox=%s,%s,%s,%s&amp;closed=0", "LBRT", NULL );
  vik_ext_tool_datasources_register ( VIK_EXT_TOOL ( vwtds ) );
  g_object_unref ( vwtds );

  // Goto
  VikGotoXmlTool *nominatim = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM Nominatim",
    "url-format", "http://nominatim.openstreetmap.org/search?q=%s&format=xml",
    "lat-path", "/searchresults/place",
    "lat-attr", "lat",
    "lon-path", "/searchresults/place",
    "lon-attr", "lon",
    NULL ) );
    vik_goto_register ( VIK_GOTO_TOOL ( nominatim ) );
    g_object_unref ( nominatim );

  VikGotoXmlTool *namefinder = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM Name finder",
    "url-format", "http://gazetteer.openstreetmap.org/namefinder/search.xml?find=%s&max=1",
    "lat-path", "/searchresults/named",
    "lat-attr", "lat",
    "lon-path", "/searchresults/named",
    "lon-attr", "lon",
    NULL ) );
    vik_goto_register ( VIK_GOTO_TOOL ( namefinder ) );
    g_object_unref ( namefinder );

  // Not really OSM but can't be bothered to create somewhere else to put it...
  webtool = vik_webtool_center_new_with_members ( _("Wikimedia Toolserver GeoHack"), "http://toolserver.org/~geohack/geohack.php?params=%s;%s" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );
  
  /* See API references: https://github.com/DennisOSRM/Project-OSRM/wiki/Server-api */
  VikRoutingEngine *osrm = g_object_new ( VIK_ROUTING_WEB_ENGINE_TYPE,
    "id", "osrm",
    "label", "OSRM",
    "format", "gpx",
    "url-base", "http://router.project-osrm.org/viaroute?output=gpx",
    "url-start-ll", "&loc=%s,%s",
    "url-stop-ll", "&loc=%s,%s",
    "url-via-ll", "&loc=%s,%s",
    NULL);
  vik_routing_register ( VIK_ROUTING_ENGINE ( osrm ) );
  g_object_unref ( osrm );
}

