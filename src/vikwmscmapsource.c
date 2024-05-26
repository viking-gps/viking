/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
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
  * SECTION:vikwmscmapsource
  * @short_description: the class for WMS/WMS-C oriented map sources
  *
  * The #VikWmscMapSource class handles WMS/WMS-C oriented map sources.
  *
  * http://wiki.osgeo.org/wiki/WMS_Tile_Caching
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "globals.h"
#include "vikwmscmapsource.h"
#include "maputils.h"

static gboolean _coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void _mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest );
static gboolean _supports_download_only_new ( VikMapSource *self );
static gboolean _is_direct_file_access ( VikMapSource *self );
static gboolean _is_mbtiles ( VikMapSource *self );
static gboolean _is_osm_meta_tiles (VikMapSource *self );
static guint8 _get_zoom_min(VikMapSource *self );
static guint8 _get_zoom_max(VikMapSource *self );
static gdouble _get_lat_min(VikMapSource *self );
static gdouble _get_lat_max(VikMapSource *self );
static gdouble _get_lon_min(VikMapSource *self );
static gdouble _get_lon_max(VikMapSource *self );

static gchar *_get_uri( VikMapSourceDefault *self, MapCoord *src );
static gchar *_get_hostname( VikMapSourceDefault *self );
static DownloadFileOptions *_get_download_options( VikMapSourceDefault *self, MapCoord *src );
static void _set_expiry_age( VikMapSourceDefault *self, guint age );

typedef struct _VikWmscMapSourcePrivate VikWmscMapSourcePrivate;
struct _VikWmscMapSourcePrivate
{
  gchar *hostname;
  gchar *url;
  gchar *custom_http_headers;
  DownloadFileOptions options;
  guint zoom_min; // TMS Zoom level: 0 = Whole World // http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  guint zoom_max; // TMS Zoom level: Often 18 for zoomed in.
  gdouble lat_min; // Degrees
  gdouble lat_max; // Degrees
  gdouble lon_min; // Degrees
  gdouble lon_max; // Degrees
};

G_DEFINE_TYPE_WITH_PRIVATE (VikWmscMapSource, vik_wmsc_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT);
#define VIK_WMSC_MAP_SOURCE_PRIVATE(o)  (vik_wmsc_map_source_get_instance_private (VIK_WMSC_MAP_SOURCE(o)))

/* properties */
enum
{
  PROP_0,

  PROP_HOSTNAME,
  PROP_URL,
  PROP_CUSTOM_HTTP_HEADERS,
  PROP_REFERER,
  PROP_USER_AGENT,
  PROP_FOLLOW_LOCATION,
  PROP_CHECK_FILE_SERVER_TIME,
  PROP_ZOOM_MIN,
  PROP_ZOOM_MAX,
  PROP_LAT_MIN,
  PROP_LAT_MAX,
  PROP_LON_MIN,
  PROP_LON_MAX,
};

static void
vik_wmsc_map_source_init (VikWmscMapSource *self)
{
  /* initialize the object here */
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

  priv->hostname = NULL;
  priv->url = NULL;
  priv->custom_http_headers = NULL;
  priv->options.referer = NULL;
  priv->options.user_agent = NULL;
  priv->options.follow_location = 0;
  priv->options.expiry_age = ONE_WEEK_SECS;
  priv->options.check_file = a_check_map_file;
  priv->options.check_file_server_time = TRUE;
  priv->options.custom_http_headers = NULL;
  priv->zoom_min = 0;
  priv->zoom_max = 18;
  priv->lat_min = -90.0;
  priv->lat_max = 90.0;
  priv->lon_min = -180.0;
  priv->lon_max = 180.0;

  g_object_set (G_OBJECT (self),
                "tilesize-x", 256,
                "tilesize-y", 256,
                "drawmode", VIK_VIEWPORT_DRAWMODE_LATLON,
                NULL);
}

static void
vik_wmsc_map_source_finalize (GObject *object)
{
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (object);

  g_free (priv->hostname);
  priv->hostname = NULL;
  g_free (priv->url);
  priv->url = NULL;
  g_free (priv->custom_http_headers);
  priv->custom_http_headers = NULL;
  g_free (priv->options.referer);
  priv->options.referer = NULL;
  g_free (priv->options.user_agent);
  priv->options.user_agent = NULL;
  g_free (priv->options.custom_http_headers);
  priv->options.custom_http_headers = NULL;

  G_OBJECT_CLASS (vik_wmsc_map_source_parent_class)->finalize (object);
}

static void
vik_wmsc_map_source_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (object);

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

    case PROP_CUSTOM_HTTP_HEADERS:
      {
      g_free (priv->custom_http_headers);
      const gchar *str = g_value_get_string ( value );
      // Convert the literal characters '\'+'n' into actual newlines '\n'
      if ( str )
        priv->custom_http_headers = g_strcompress ( str );
      else
        priv->custom_http_headers = NULL;
      }
      break;

    case PROP_REFERER:
      g_free (priv->options.referer);
      priv->options.referer = g_value_dup_string (value);
      break;

    case PROP_USER_AGENT:
      g_free (priv->options.user_agent);
      priv->options.user_agent = g_value_dup_string (value);
      break;

    case PROP_FOLLOW_LOCATION:
      priv->options.follow_location = g_value_get_long (value);
      break;

    case PROP_CHECK_FILE_SERVER_TIME:
      priv->options.check_file_server_time = g_value_get_boolean (value);
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

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_wmsc_map_source_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (object);

  switch (property_id)
    {
    case PROP_HOSTNAME:
      g_value_set_string (value, priv->hostname);
      break;

    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;

    case PROP_CUSTOM_HTTP_HEADERS:
      g_value_set_string (value, priv->custom_http_headers);
	  break;

    case PROP_REFERER:
      g_value_set_string (value, priv->options.referer);
      break;

    case PROP_USER_AGENT:
      g_value_set_string (value, priv->options.user_agent);
      break;

    case PROP_FOLLOW_LOCATION:
      g_value_set_long (value, priv->options.follow_location);
      break;

    case PROP_CHECK_FILE_SERVER_TIME:
      g_value_set_boolean (value, priv->options.check_file_server_time);
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

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_wmsc_map_source_class_init (VikWmscMapSourceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* grandparent_class = VIK_MAP_SOURCE_CLASS (klass);
	VikMapSourceDefaultClass* parent_class = VIK_MAP_SOURCE_DEFAULT_CLASS (klass);
	GParamSpec *pspec = NULL;
		
	object_class->set_property = vik_wmsc_map_source_set_property;
    object_class->get_property = vik_wmsc_map_source_get_property;

	/* Overiding methods */
	grandparent_class->coord_to_mapcoord =        _coord_to_mapcoord;
	grandparent_class->mapcoord_to_center_coord = _mapcoord_to_center_coord;
	grandparent_class->supports_download_only_new = _supports_download_only_new;
	grandparent_class->is_direct_file_access = _is_direct_file_access;
	grandparent_class->is_mbtiles = _is_mbtiles;
	grandparent_class->is_osm_meta_tiles = _is_osm_meta_tiles;
	grandparent_class->get_zoom_min = _get_zoom_min;
	grandparent_class->get_zoom_max = _get_zoom_max;
	grandparent_class->get_lat_min = _get_lat_min;
	grandparent_class->get_lat_max = _get_lat_max;
	grandparent_class->get_lon_min = _get_lon_min;
	grandparent_class->get_lon_max = _get_lon_max;

	parent_class->get_uri = _get_uri;
	parent_class->get_hostname = _get_hostname;
	parent_class->get_download_options = _get_download_options;
	parent_class->set_expiry_age = _set_expiry_age;

	pspec = g_param_spec_string ("hostname",
	                             "Hostname",
	                             "The hostname of the map server",
	                             "<no-set>" /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_HOSTNAME, pspec);

	pspec = g_param_spec_string ("url",
	                             "URL",
	                             "The template of the tiles' URL",
	                             "<no-set>" /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_URL, pspec);

	pspec = g_param_spec_string ("custom-http-headers",
	                             "Custom HTTP Headers",
	                             "Custom HTTP Headers, use '\n' to separate multiple headers",
	                             NULL /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CUSTOM_HTTP_HEADERS, pspec);

	pspec = g_param_spec_string ("referer",
	                             "Referer",
	                             "The REFERER string to use in HTTP request",
	                             NULL /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_REFERER, pspec);

	pspec = g_param_spec_string ("user-agent",
	                             "User Agent",
	                             "The User Agent string to send in the request",
	                             NULL, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_USER_AGENT, pspec);

	pspec = g_param_spec_long ("follow-location",
	                           "Follow location",
                               "Specifies the number of retries to follow a redirect while downloading a page",
	                           -1, // minimum value (unlimited)
                               G_MAXLONG /* maximum value */,
                               0  /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FOLLOW_LOCATION, pspec);

	pspec = g_param_spec_boolean ("check-file-server-time",
	                              "Check file server time",
                                  "Age of current cache before redownloading tile",
                                  TRUE  /* default value */,
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CHECK_FILE_SERVER_TIME, pspec);

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

	object_class->finalize = vik_wmsc_map_source_finalize;
}

static gboolean
_supports_download_only_new ( VikMapSource *self )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), FALSE);
    VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->options.check_file_server_time;
}

static gboolean
_is_direct_file_access ( VikMapSource *self )
{
	return FALSE;
}

static gboolean
_is_mbtiles ( VikMapSource *self )
{
	return FALSE;
}

static gboolean
_is_osm_meta_tiles ( VikMapSource *self )
{
	return FALSE;
}

static gboolean
_coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = map_utils_mpp_to_scale ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;

  /* Note : VIK_GZ(17) / xzoom / 2 = number of tile on Y axis */
  g_debug("%s: xzoom=%f yzoom=%f -> %f", __FUNCTION__,
          xzoom, yzoom, VIK_GZ(17) / xzoom / 2);
  dest->x = floor((src->east_west + 180) / 180 * VIK_GZ(17) / xzoom / 2);
  /* We should restore logic of viking:
   * tile index on Y axis follow a screen logic (top -> down)
   */
  dest->y = floor((180 - (src->north_south + 90)) / 180 * VIK_GZ(17) / xzoom / 2);
  dest->z = 0;
  g_debug("%s: %f,%f -> %d,%d", __FUNCTION__,
          src->east_west, src->north_south, dest->x, dest->y);
  return TRUE;
}

static void
_mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest )
{
  gdouble socalled_mpp;
  if (src->scale >= 0)
    socalled_mpp = VIK_GZ(src->scale);
  else
    socalled_mpp = 1.0/VIK_GZ(-src->scale);
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = (src->x+0.5) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 180;
  /* We should restore logic of viking:
   * tile index on Y axis follow a screen logic (top -> down)
   */
  dest->north_south = -((src->y+0.5) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 90);
  g_debug("%s: %d,%d -> %f,%f", __FUNCTION__,
          src->x, src->y, dest->east_west, dest->north_south);
}

static gchar *
_get_uri( VikMapSourceDefault *self, MapCoord *src )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), NULL);
    VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	gdouble socalled_mpp;
	if (src->scale >= 0)
		socalled_mpp = VIK_GZ(src->scale);
	else
		socalled_mpp = 1.0/VIK_GZ(-src->scale);
	gdouble minx = (gdouble)src->x * 180 / VIK_GZ(17) * socalled_mpp * 2 - 180;
	gdouble maxx = (gdouble)(src->x + 1) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
     * tile index on Y axis follow a screen logic (top -> down)
     */
	gdouble miny = -((gdouble)(src->y + 1) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 90);
	gdouble maxy = -((gdouble)(src->y) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 90);

	gchar sminx[COORDS_STR_BUFFER_SIZE];
	gchar smaxx[COORDS_STR_BUFFER_SIZE];
	gchar sminy[COORDS_STR_BUFFER_SIZE];
	gchar smaxy[COORDS_STR_BUFFER_SIZE];

	a_coords_dtostr_buffer ( minx, sminx );
	a_coords_dtostr_buffer ( maxx, smaxx );
	a_coords_dtostr_buffer ( miny, sminy );
	a_coords_dtostr_buffer ( maxy, smaxy );

	if ( !priv->url )
		return NULL;

	gchar *uri = g_strdup_printf (priv->url, sminx, sminy, smaxx, smaxy);

	return uri;
}

static gchar *
_get_hostname( VikMapSourceDefault *self )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), NULL);
    VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return g_strdup( priv->hostname );
}

static DownloadFileOptions *
_get_download_options( VikMapSourceDefault *self, MapCoord *src )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), NULL);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);

	DownloadFileOptions *dfo = g_malloc0 ( sizeof(DownloadFileOptions) );
	dfo->check_file_server_time = priv->options.check_file_server_time;
	dfo->use_etag = priv->options.use_etag;
	dfo->referer = g_strdup ( priv->options.referer );
	dfo->user_agent = g_strdup ( priv->options.user_agent );
	dfo->follow_location = priv->options.follow_location;
	if ( src )
		if ( priv->custom_http_headers )
			dfo->custom_http_headers = g_strdup_printf ( priv->custom_http_headers, 17-src->scale, src->x, src->y );
		else
			dfo->custom_http_headers = NULL;
	else
		dfo->custom_http_headers = g_strdup ( priv->custom_http_headers );
	dfo->check_file = priv->options.check_file;
	dfo->user_pass = g_strdup ( priv->options.user_pass );
	dfo->convert_file = priv->options.convert_file;
	dfo->expiry_age = priv->options.expiry_age;

	return dfo;
}
/**
 *
 */
static guint8
_get_zoom_min (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), FALSE);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->zoom_min;
}

/**
 *
 */
static guint8
_get_zoom_max (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), FALSE);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->zoom_max;
}

/**
 *
 */
static gdouble
_get_lat_min (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), FALSE);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lat_min;
}

/**
 *
 */
static gdouble
_get_lat_max (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), FALSE);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lat_max;
}

/**
 *
 */
static gdouble
_get_lon_min (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), FALSE);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lon_min;
}

/**
 *
 */
static gdouble
_get_lon_max (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), FALSE);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lon_max;
}

static void
_set_expiry_age (VikMapSourceDefault *self, guint age)
{
	g_return_if_fail (VIK_IS_WMSC_MAP_SOURCE(self));
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	priv->options.expiry_age = age;
}

VikWmscMapSource *
vik_wmsc_map_source_new_with_id (guint16 id, const gchar *label, const gchar *hostname, const gchar *url)
{
	return g_object_new(VIK_TYPE_WMSC_MAP_SOURCE,
	                    "id", id, "label", label, "hostname", hostname, "url", url, NULL);
}
