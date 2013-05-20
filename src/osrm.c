/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

/**
 * SECTION:osrm
 * @short_description: the class for OSRM
 * 
 * The #OsrmRouting class handles OSRM
 * service as routing engine.
 * 
 * Technical details are available here:
 * http://project-osrm.org/
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "osrm.h"

/* See API references: https://github.com/DennisOSRM/Project-OSRM/wiki/Server-api */
#define OSRM_DIRECTIONS_STRING "router.project-osrm.org/viaroute?loc=%s,%s&loc=%s,%s&output=gpx"

static DownloadMapOptions osrms_routing_options = { FALSE, FALSE, "http://map.project-osrm.org/", 0, NULL };

static void osrm_routing_finalize ( GObject *gob );

static gchar *osrm_routing_get_url_for_coords ( VikRoutingEngine *self, struct LatLon start, struct LatLon end );
static DownloadMapOptions *osrm_routing_get_download_options ( VikRoutingEngine *self );

G_DEFINE_TYPE (OsrmRouting, osrm_routing, VIK_ROUTING_ENGINE_TYPE)

static void osrm_routing_class_init ( OsrmRoutingClass *klass )
{
  GObjectClass *object_class;
  VikRoutingEngineClass *parent_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = osrm_routing_finalize;

  parent_class = VIK_ROUTING_ENGINE_CLASS (klass);

  parent_class->get_url_for_coords = osrm_routing_get_url_for_coords;
  parent_class->get_download_options = osrm_routing_get_download_options;
}

OsrmRouting *osrm_routing_new ()
{
  return OSRM_ROUTING ( g_object_new ( OSRM_ROUTING_TYPE,
                                       "id", "osrm",
                                       "label", "OSRM",
                                       "format", "gpx",
                                       NULL ) );
}

static void osrm_routing_init ( OsrmRouting *vlp )
{
}

static void osrm_routing_finalize ( GObject *gob )
{
  G_OBJECT_GET_CLASS(gob)->finalize(gob);
}

static DownloadMapOptions *
osrm_routing_get_download_options ( VikRoutingEngine *self )
{
	return &osrms_routing_options;
}

static gchar *
osrm_routing_get_url_for_coords ( VikRoutingEngine *self, struct LatLon start, struct LatLon end )
{
    gchar startlat[G_ASCII_DTOSTR_BUF_SIZE], startlon[G_ASCII_DTOSTR_BUF_SIZE];
    gchar endlat[G_ASCII_DTOSTR_BUF_SIZE], endlon[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *url;
    url = g_strdup_printf(OSRM_DIRECTIONS_STRING,
                          g_ascii_dtostr (startlat, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) start.lat),
                          g_ascii_dtostr (startlon, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) start.lon),
                          g_ascii_dtostr (endlat, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) end.lat),
                          g_ascii_dtostr (endlon, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) end.lon));

    return url;
}
