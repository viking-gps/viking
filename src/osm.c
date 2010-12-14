/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include "vikexttools.h"
#include "vikgotoxmltool.h"
#include "vikgoto.h"

/* initialisation */
void osm_init () {
  VikMapSource *osmarender_type = 
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 12,
                                "label", "OpenStreetMap (Osmarender)",
                                "hostname", "tah.openstreetmap.org",
                                "url", "/Tiles/tile/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
                                "copyright", "© OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));
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
  VikMapSource *maplint_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 14,
                                "label", "OpenStreetMap (Maplint)",
                                "hostname", "tah.openstreetmap.org",
                                "url", "/Tiles/maplint.php/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                "use-etag", FALSE,
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
                                "copyright", "© OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));
  VikMapSource *wms_type =
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_WMSC_MAP_SOURCE,
                                "id", 18,
                                "label", "OpenStreetMap (WMS)",
                                "hostname", "full.wms.geofabrik.de",
                                "url", "/std/demo_key?LAYERS=osm-full&FORMAT=image/png&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=&SRS=EPSG:4326&BBOX=%s,%s,%s,%s&WIDTH=256&HEIGHT=256",
                                "check-file-server-time", FALSE,
                                "copyright", "© OpenStreetMap contributors",
                                "license", "CC-BY-SA",
                                "license-url", "http://www.openstreetmap.org/copyright",
                                NULL));

  maps_layer_register_map_source (osmarender_type);
  maps_layer_register_map_source (mapnik_type);
  maps_layer_register_map_source (maplint_type);
  maps_layer_register_map_source (cycle_type);
  maps_layer_register_map_source (wms_type);

  // Webtools
  VikWebtoolCenter *webtool = NULL;
  webtool = vik_webtool_center_new_with_members ( _("OSM (view)"), "http://openstreetmap.org/?lat=%s&lon=%s&zoom=%d&layers=B000FTF" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  webtool = vik_webtool_center_new_with_members ( _("OSM (edit)"), "http://www.openstreetmap.org/edit?lat=%s&lon=%s&zoom=%d" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  webtool = vik_webtool_center_new_with_members ( _("OSM (render)"), "http://www.informationfreeway.org/?lat=%s&lon=%s&zoom=%d&layers=B0000F000F" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

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
}

