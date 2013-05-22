/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 * SECTION:googlerouting
 * @short_description: the class for Google Directions
 * 
 * The #GoogleRouting class handles Google Directions 
 * service as routing engine.
 * 
 * Technical details are available here:
 * https://developers.google.com/maps/documentation/directions/#DirectionsResponses
 *
 * gpsbabel supports this format.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "googlerouting.h"

#define GOOGLE_DIRECTIONS_STRING "maps.google.com/maps?q=from:%s,%s+to:%s,%s&output=js"

static DownloadMapOptions googles_routing_options = { FALSE, FALSE, "http://maps.google.com/", 0, NULL };

static gchar *google_routing_get_url_for_coords ( VikRoutingEngine *self, struct LatLon start, struct LatLon end );
static DownloadMapOptions *google_routing_get_download_options ( VikRoutingEngine *self );

G_DEFINE_TYPE (GoogleRouting, google_routing, VIK_ROUTING_ENGINE_TYPE)

static void google_routing_class_init ( GoogleRoutingClass *klass )
{
  GObjectClass *object_class;
  VikRoutingEngineClass *parent_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = VIK_ROUTING_ENGINE_CLASS (klass);

  parent_class->get_url_for_coords = google_routing_get_url_for_coords;
  parent_class->get_download_options = google_routing_get_download_options;
}

/**
 * google_routing_new:
 * 
 * Create a new instance of GoogleRouting routing engine.
 */
GoogleRouting *google_routing_new ()
{
  return GOOGLE_ROUTING ( g_object_new ( GOOGLE_ROUTING_TYPE,
                                         "id", "google",
                                         "label", "Google",
                                         "format", "google",
                                         NULL ) );
}

static void google_routing_init ( GoogleRouting *vlp )
{
}

static DownloadMapOptions *
google_routing_get_download_options ( VikRoutingEngine *self )
{
	return &googles_routing_options;
}

/*
 * Override VikRoutingEngine:get_download_options()
 */
gchar *
google_routing_get_url_for_coords ( VikRoutingEngine *self, struct LatLon start, struct LatLon end )
{
    gchar startlat[G_ASCII_DTOSTR_BUF_SIZE], startlon[G_ASCII_DTOSTR_BUF_SIZE];
    gchar endlat[G_ASCII_DTOSTR_BUF_SIZE], endlon[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *url;
    url = g_strdup_printf(GOOGLE_DIRECTIONS_STRING,
                          g_ascii_dtostr (startlat, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) start.lat),
                          g_ascii_dtostr (startlon, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) start.lon),
                          g_ascii_dtostr (endlat, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) end.lat),
                          g_ascii_dtostr (endlon, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) end.lon));

    return url;
}
