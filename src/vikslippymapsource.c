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

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "globals.h"
#include "vikslippymapsource.h"
#include "maputils.h"

static gboolean _coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void _mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest );

static gboolean _is_direct_file_access (VikMapSource *self );
static gboolean _supports_download_only_new (VikMapSource *self );

static gchar *_get_uri( VikMapSourceDefault *self, MapCoord *src );
static gchar *_get_hostname( VikMapSourceDefault *self );
static DownloadMapOptions *_get_download_options( VikMapSourceDefault *self );

typedef struct _VikSlippyMapSourcePrivate VikSlippyMapSourcePrivate;
struct _VikSlippyMapSourcePrivate
{
  gchar *hostname;
  gchar *url;
  DownloadMapOptions options;
  gboolean is_direct_file_access;
};

#define VIK_SLIPPY_MAP_SOURCE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_SLIPPY_MAP_SOURCE, VikSlippyMapSourcePrivate))

/* properties */
enum
{
  PROP_0,

  PROP_HOSTNAME,
  PROP_URL,
  PROP_REFERER,
  PROP_FOLLOW_LOCATION,
  PROP_CHECK_FILE_SERVER_TIME,
  PROP_USE_ETAG,
  PROP_IS_DIRECT_FILE_ACCESS,
};

G_DEFINE_TYPE (VikSlippyMapSource, vik_slippy_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT);

static void
vik_slippy_map_source_init (VikSlippyMapSource *self)
{
  /* initialize the object here */
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE (self);

  priv->hostname = NULL;
  priv->url = NULL;
  priv->options.referer = NULL;
  priv->options.follow_location = 0;
  priv->options.check_file = a_check_map_file;
  priv->options.check_file_server_time = FALSE;
  priv->options.use_etag = FALSE;
  priv->is_direct_file_access = FALSE;

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
	grandparent_class->supports_download_only_new = _supports_download_only_new;

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
_supports_download_only_new (VikMapSource *self)
{
  g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
	
  VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	
  return priv->options.check_file_server_time || priv->options.use_etag;
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

  dest->x = (src->east_west + 180) / 360 * VIK_GZ(17) / xzoom;
  dest->y = (180 - MERCLAT(src->north_south)) / 360 * VIK_GZ(17) / xzoom;
  dest->z = 0;

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
  dest->east_west = ((src->x+0.5) / VIK_GZ(17) * socalled_mpp * 360) - 180;
  dest->north_south = DEMERCLAT(180 - ((src->y+0.5) / VIK_GZ(17) * socalled_mpp * 360));
}

static gchar *
_get_uri( VikMapSourceDefault *self, MapCoord *src )
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), NULL);
	
    VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	gchar *uri = g_strdup_printf (priv->url, 17 - src->scale, src->x, src->y);
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
