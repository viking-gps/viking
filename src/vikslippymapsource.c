/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * 
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
 /**
  * SECTION:vikslippymapsource
  * @short_description: the class for SlippyMap oriented map sources
  * 
  * The #VikSlippyMapSource class handles slippy map oriented map sources.
  * The related service is tile oriented, à la Google.
  * 
  * The tiles are in 'google spherical mercator', which is
  * basically a mercator projection that assumes a spherical earth.
  * http://docs.openlayers.org/library/spherical_mercator.html
  * 
  * Such service is also a type of TMS (Tile Map Service) as defined in
  * OSGeo's wiki.
  * http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
  * But take care that the Y axis is inverted, ie the origin is at top-left
  * corner.
  * Following this specification, the protocol handled by this class
  * follows the global-mercator profile.
  * 
  * You can also find many interesting information on the OSM's wiki.
  * http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  * http://wiki.openstreetmap.org/wiki/Setting_up_TMS
  */
  
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vikslippymapsource.h"
#include "maputils.h"

static gboolean _coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void _mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest );

static gboolean _is_direct_file_access (VikMapSource *self );
static gboolean _is_mbtiles (VikMapSource *self );
static gboolean _is_osm_meta_tiles (VikMapSource *self );
static gboolean _supports_download_only_new (VikMapSource *self );
static guint8 _get_zoom_min(VikMapSource *self );
static guint8 _get_zoom_max(VikMapSource *self );
static gdouble _get_lat_min(VikMapSource *self );
static gdouble _get_lat_max(VikMapSource *self );
static gdouble _get_lon_min(VikMapSource *self );
static gdouble _get_lon_max(VikMapSource *self );

static gchar *_get_uri( VikMapSourceDefault *self, MapCoord *src );
static gchar *_get_hostname( VikMapSourceDefault *self );
static DownloadMapOptions *_get_download_options( VikMapSourceDefault *self );

typedef struct _VikSlippyMapSourcePrivate VikSlippyMapSourcePrivate;
struct _VikSlippyMapSourcePrivate
{
  gchar *hostname;
  gchar *url;
  DownloadMapOptions options;
  // NB Probably best to keep the above fields in same order to be common across Slippy, TMS & WMS map definitions
  guint zoom_min; // TMS Zoom level: 0 = Whole World // http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  guint zoom_max; // TMS Zoom level: Often 18 for zoomed in.
  gdouble lat_min; // Degrees
  gdouble lat_max; // Degrees
  gdouble lon_min; // Degrees
  gdouble lon_max; // Degrees
  gboolean is_direct_file_access;
  gboolean is_mbtiles;
  gboolean is_osm_meta_tiles; // http://wiki.openstreetmap.org/wiki/Meta_tiles as used by tirex or renderd
  // Mainly for ARCGIS Tile Server URL Layout // http://help.arcgis.com/EN/arcgisserver/10.0/apis/rest/tile.html
  gboolean switch_xy;
};

#define VIK_SLIPPY_MAP_SOURCE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_SLIPPY_MAP_SOURCE, VikSlippyMapSourcePrivate))

/* properties */
enum
{
  PROP_0,

  PROP_HOSTNAME,
  PROP_URL,
  PROP_ZOOM_MIN,
  PROP_ZOOM_MAX,
  PROP_LAT_MIN,
  PROP_LAT_MAX,
  PROP_LON_MIN,
  PROP_LON_MAX,
  PROP_REFERER,
  PROP_FOLLOW_LOCATION,
  PROP_CHECK_FILE_SERVER_TIME,
  PROP_USE_ETAG,
  PROP_IS_DIRECT_FILE_ACCESS,
  PROP_IS_MBTILES,
  PROP_IS_OSM_META_TILES,
  PROP_SWITCH_XY,
};

G_DEFINE_TYPE (VikSlippyMapSource, vik_slippy_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT);

static void
vik_slippy_map_source_init (VikSlippyMapSource *self)
{
  /* initialize the object here */
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE (self);

  priv->hostname = NULL;
  priv->url = NULL;
  priv->zoom_min = 0;
  priv->zoom_max = 18;
  priv->lat_min = -90.0;
  priv->lat_max = 90.0;
  priv->lon_min = -180.0;
  priv->lon_max = 180.0;
  priv->options.referer = NULL;
  priv->options.follow_location = 0;
  priv->options.check_file = a_check_map_file;
  priv->options.check_file_server_time = FALSE;
  priv->options.use_etag = FALSE;
  priv->is_direct_file_access = FALSE;
  priv->is_mbtiles = FALSE;
  priv->is_osm_meta_tiles = FALSE;
  priv->switch_xy = FALSE;

  g_object_set (G_OBJECT (self),
                "tilesize-x", 256,
                "tilesize-y", 256,
                "drawmode", VIK_VIEWPORT_DRAWMODE_MERCATOR,
                NULL);
}

static void
vik_slippy_map_source_finalize (GObject *object)
{
  VikSlippyMapSource *self = VIK_SLIPPY_MAP_SOURCE (object);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE (self);

  g_free (priv->hostname);
  priv->hostname = NULL;
  g_free (priv->url);
  priv->url = NULL;
  g_free (priv->options.referer);
  priv->options.referer = NULL;

  G_OBJECT_CLASS (vik_slippy_map_source_parent_class)->finalize (object);
}

static void
vik_slippy_map_source_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  VikSlippyMapSource *self = VIK_SLIPPY_MAP_SOURCE (object);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE (self);

  switch (property_id)
    {
    case PROP_HOSTNAME:
      g_free (priv->hostname);
      priv->hostname = g_value_dup_string (value);
      break;

    case PROP_URL:
      g_free (priv->url);
      priv->url = g_value_dup_string (value);
      break;

    case PROP_ZOOM_MIN:
      priv->zoom_min = g_value_get_uint (value);
      break;

    case PROP_ZOOM_MAX:
      priv->zoom_max = g_value_get_uint (value);
      break;

    case PROP_LAT_MIN:
      priv->lat_min = g_value_get_double (value);
      break;

    case PROP_LAT_MAX:
      priv->lat_max = g_value_get_double (value);
      break;

    case PROP_LON_MIN:
      priv->lon_min = g_value_get_double (value);
      break;

    case PROP_LON_MAX:
      priv->lon_max = g_value_get_double (value);
      break;

    case PROP_REFERER:
      g_free (priv->options.referer);
      priv->options.referer = g_value_dup_string (value);
      break;

    case PROP_FOLLOW_LOCATION:
      priv->options.follow_location = g_value_get_long (value);
      break;

    case PROP_CHECK_FILE_SERVER_TIME:
      priv->options.check_file_server_time = g_value_get_boolean (value);
      break;

    case PROP_USE_ETAG:
      priv->options.use_etag = g_value_get_boolean (value);
      break;

    case PROP_IS_DIRECT_FILE_ACCESS:
      priv->is_direct_file_access = g_value_get_boolean (value);
      break;

    case PROP_IS_MBTILES:
      priv->is_mbtiles = g_value_get_boolean (value);
      break;

    case PROP_IS_OSM_META_TILES:
      priv->is_osm_meta_tiles = g_value_get_boolean (value);
      break;

    case PROP_SWITCH_XY:
      priv->switch_xy = g_value_get_boolean (value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_slippy_map_source_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  VikSlippyMapSource *self = VIK_SLIPPY_MAP_SOURCE (object);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE (self);

  switch (property_id)
    {
    case PROP_HOSTNAME:
      g_value_set_string (value, priv->hostname);
      break;

    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;

    case PROP_ZOOM_MIN:
      g_value_set_uint (value, priv->zoom_min);
      break;

    case PROP_ZOOM_MAX:
      g_value_set_uint (value, priv->zoom_max);
      break;

    case PROP_LON_MIN:
      g_value_set_double (value, priv->lon_min);
      break;

    case PROP_LON_MAX:
      g_value_set_double (value, priv->lon_max);
      break;

    case PROP_LAT_MIN:
      g_value_set_double (value, priv->lat_min);
      break;

    case PROP_LAT_MAX:
      g_value_set_double (value, priv->lat_max);
      break;

    case PROP_REFERER:
      g_value_set_string (value, priv->options.referer);
      break;

    case PROP_FOLLOW_LOCATION:
      g_value_set_long (value, priv->options.follow_location);
      break;

    case PROP_CHECK_FILE_SERVER_TIME:
      g_value_set_boolean (value, priv->options.check_file_server_time);
      break;
	  
    case PROP_USE_ETAG:
      g_value_set_boolean (value, priv->options.use_etag);
      break;

    case PROP_IS_DIRECT_FILE_ACCESS:
      g_value_set_boolean (value, priv->is_direct_file_access);
      break;

    case PROP_IS_MBTILES:
      g_value_set_boolean (value, priv->is_mbtiles);
      break;

    case PROP_IS_OSM_META_TILES:
      g_value_set_boolean (value, priv->is_osm_meta_tiles);
      break;

    case PROP_SWITCH_XY:
      g_value_set_boolean (value, priv->switch_xy);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_slippy_map_source_class_init (VikSlippyMapSourceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* grandparent_class = VIK_MAP_SOURCE_CLASS (klass);
	VikMapSourceDefaultClass* parent_class = VIK_MAP_SOURCE_DEFAULT_CLASS (klass);
	GParamSpec *pspec = NULL;
		
	object_class->set_property = vik_slippy_map_source_set_property;
    object_class->get_property = vik_slippy_map_source_get_property;

	/* Overiding methods */
	grandparent_class->coord_to_mapcoord =        _coord_to_mapcoord;
	grandparent_class->mapcoord_to_center_coord = _mapcoord_to_center_coord;
	grandparent_class->is_direct_file_access = _is_direct_file_access;
	grandparent_class->is_mbtiles = _is_mbtiles;
	grandparent_class->is_osm_meta_tiles = _is_osm_meta_tiles;
	grandparent_class->supports_download_only_new = _supports_download_only_new;
	grandparent_class->get_zoom_min = _get_zoom_min;
	grandparent_class->get_zoom_max = _get_zoom_max;
	grandparent_class->get_lat_min = _get_lat_min;
	grandparent_class->get_lat_max = _get_lat_max;
	grandparent_class->get_lon_min = _get_lon_min;
	grandparent_class->get_lon_max = _get_lon_max;

	parent_class->get_uri = _get_uri;
	parent_class->get_hostname = _get_hostname;
	parent_class->get_download_options = _get_download_options;

	pspec = g_param_spec_string ("hostname",
	                             "Hostname",
	                             "The hostname of the map server",
	                             "<no-set>" /* default value */,
	                             G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_HOSTNAME, pspec);

	pspec = g_param_spec_string ("url",
	                             "URL",
	                             "The template of the tiles' URL",
	                             "<no-set>" /* default value */,
	                             G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_URL, pspec);

	pspec = g_param_spec_uint ("zoom-min",
	                           "Minimum zoom",
	                           "Minimum Zoom level supported by the map provider",
	                           0,  // minimum value,
	                           22, // maximum value
	                           0, // default value
	                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ZOOM_MIN, pspec);

	pspec = g_param_spec_uint ("zoom-max",
	                           "Maximum zoom",
	                           "Maximum Zoom level supported by the map provider",
	                           0,  // minimum value,
	                           22, // maximum value
	                           18, // default value
	                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ZOOM_MAX, pspec);

	pspec = g_param_spec_double ("lat-min",
	                             "Minimum latitude",
	                             "Minimum latitude in degrees supported by the map provider",
	                             -90.0,  // minimum value
	                             90.0, // maximum value
	                             -90.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LAT_MIN, pspec);

	pspec = g_param_spec_double ("lat-max",
	                             "Maximum latitude",
	                             "Maximum latitude in degrees supported by the map provider",
	                             -90.0,  // minimum value
	                             90.0, // maximum value
	                             90.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LAT_MAX, pspec);

	pspec = g_param_spec_double ("lon-min",
	                             "Minimum longitude",
	                             "Minimum longitude in degrees supported by the map provider",
	                             -180.0,  // minimum value
	                             180.0, // maximum value
	                             -180.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LON_MIN, pspec);

	pspec = g_param_spec_double ("lon-max",
	                             "Maximum longitude",
	                             "Maximum longitude in degrees supported by the map provider",
	                             -180.0,  // minimum value
	                             180.0, // maximum value
	                             180.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LON_MAX, pspec);

	pspec = g_param_spec_string ("referer",
	                             "Referer",
	                             "The REFERER string to use in HTTP request",
	                             NULL /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_REFERER, pspec);
	
	pspec = g_param_spec_long ("follow-location",
	                           "Follow location",
                               "Specifies the number of retries to follow a redirect while downloading a page",
                               0  /* minimum value */,
                               G_MAXLONG /* maximum value */,
                               0  /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FOLLOW_LOCATION, pspec);
	
	pspec = g_param_spec_boolean ("check-file-server-time",
	                              "Check file server time",
                                  "Age of current cache before redownloading tile",
                                  FALSE  /* default value */,
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CHECK_FILE_SERVER_TIME, pspec);

	pspec = g_param_spec_boolean ("use-etag",
	                              "Use etag values with server",
                                  "Store etag in a file, and send it to server to check if we have the latest file",
                                  FALSE  /* default value */,
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_USE_ETAG, pspec);

	pspec = g_param_spec_boolean ("use-direct-file-access",
	                              "Use direct file access",
	                              "Use direct file access to OSM like tile images - no need for a webservice",
                                  FALSE  /* default value */,
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_IS_DIRECT_FILE_ACCESS, pspec);

	pspec = g_param_spec_boolean ("is-mbtiles",
	                              "Is an SQL MBTiles File",
	                              "Use an SQL MBTiles File for the tileset - no need for a webservice",
	                              FALSE  /* default value */,
	                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_IS_MBTILES, pspec);

	pspec = g_param_spec_boolean ("is-osm-meta-tiles",
	                              "Is in OSM Meta Tile format",
	                              "Read from OSM Meta Tiles - Should be 'use-direct-file-access' as well",
	                              FALSE  /* default value */,
	                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_IS_OSM_META_TILES, pspec);

	pspec = g_param_spec_boolean ("switch-xy",
	                              "Switch the order of x,y components in the URL",
	                              "Switch the order of x,y components in the URL (such as used by ARCGIS Tile Server",
	                              FALSE  /* default value */,
	                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SWITCH_XY, pspec);

	g_type_class_add_private (klass, sizeof (VikSlippyMapSourcePrivate));
	
	object_class->finalize = vik_slippy_map_source_finalize;
}

static gboolean
_is_direct_file_access (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);

  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);

  return priv->is_direct_file_access;
}

static gboolean
_is_mbtiles (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);

  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);

  return priv->is_mbtiles;
}

/**
 *
 */
static gboolean
_is_osm_meta_tiles (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
  return priv->is_osm_meta_tiles;
}

static gboolean
_supports_download_only_new (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
	
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	
  return priv->options.check_file_server_time || priv->options.use_etag;
}

/**
 *
 */
static guint8
_get_zoom_min (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
  return priv->zoom_min;
}

/**
 *
 */
static guint8
_get_zoom_max (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
  return priv->zoom_max;
}

/**
 *
 */
static gdouble
_get_lat_min (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
  return priv->lat_min;
}

/**
 *
 */
static gdouble
_get_lat_max (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
  return priv->lat_max;
}

/**
 *
 */
static gdouble
_get_lon_min (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
  return priv->lon_min;
}

/**
 *
 */
static gdouble
_get_lon_max (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
  return priv->lon_max;
}

static gboolean
_coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
	return map_utils_vikcoord_to_iTMS ( src, xzoom, yzoom, dest );
}

static void
_mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest )
{
	map_utils_iTMS_to_center_vikcoord ( src, dest );
}

static gchar *
_get_uri( VikMapSourceDefault *self, MapCoord *src )
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), NULL);
	
	VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);

	gchar *uri = NULL;
	if ( priv->switch_xy )
		// 'ARC GIS' Tile Server layout ordering
		uri = g_strdup_printf (priv->url, 17 - src->scale, src->y, src->x);
	else
		// (Default) Standard OSM Tile Server layout ordering
		uri = g_strdup_printf (priv->url, 17 - src->scale, src->x, src->y);

	return uri;
} 

static gchar *
_get_hostname( VikMapSourceDefault *self )
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), NULL);
	
    VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	return g_strdup( priv->hostname );
}

static DownloadMapOptions *
_get_download_options( VikMapSourceDefault *self )
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), NULL);
	
	VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	return &(priv->options);
}

VikSlippyMapSource *
vik_slippy_map_source_new_with_id (guint16 id, const gchar *label, const gchar *hostname, const gchar *url)
{
	return g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
	                    "id", id, "label", label, "hostname", hostname, "url", url, NULL);
}
