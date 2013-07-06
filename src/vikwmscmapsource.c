/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "globals.h"
#include "vikwmscmapsource.h"

static gboolean _coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void _mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest );
static gboolean _supports_download_only_new ( VikMapSource *self );
static gboolean _is_direct_file_access ( VikMapSource *self );

static gchar *_get_uri( VikMapSourceDefault *self, MapCoord *src );
static gchar *_get_hostname( VikMapSourceDefault *self );
static DownloadMapOptions *_get_download_options( VikMapSourceDefault *self );

typedef struct _VikWmscMapSourcePrivate VikWmscMapSourcePrivate;
struct _VikWmscMapSourcePrivate
{
  gchar *hostname;
  gchar *url;
  DownloadMapOptions options;
};

#define VIK_WMSC_MAP_SOURCE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_WMSC_MAP_SOURCE, VikWmscMapSourcePrivate))

/* properties */
enum
{
  PROP_0,

  PROP_HOSTNAME,
  PROP_URL,
  PROP_REFERER,
  PROP_FOLLOW_LOCATION,
  PROP_CHECK_FILE_SERVER_TIME,
};

G_DEFINE_TYPE (VikWmscMapSource, vik_wmsc_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT);

static void
vik_wmsc_map_source_init (VikWmscMapSource *self)
{
  /* initialize the object here */
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

  priv->hostname = NULL;
  priv->url = NULL;
  priv->options.referer = NULL;
  priv->options.follow_location = 0;
  priv->options.check_file = a_check_map_file;
  priv->options.check_file_server_time = FALSE;

  g_object_set (G_OBJECT (self),
                "tilesize-x", 256,
                "tilesize-y", 256,
                "drawmode", VIK_VIEWPORT_DRAWMODE_LATLON,
                NULL);
}

static void
vik_wmsc_map_source_finalize (GObject *object)
{
  VikWmscMapSource *self = VIK_WMSC_MAP_SOURCE (object);
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

  g_free (priv->hostname);
  priv->hostname = NULL;
  g_free (priv->url);
  priv->url = NULL;
  g_free (priv->options.referer);
  priv->options.referer = NULL;

  G_OBJECT_CLASS (vik_wmsc_map_source_parent_class)->finalize (object);
}

static void
vik_wmsc_map_source_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  VikWmscMapSource *self = VIK_WMSC_MAP_SOURCE (object);
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

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
  VikWmscMapSource *self = VIK_WMSC_MAP_SOURCE (object);
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

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
	
	parent_class->get_uri = _get_uri;
	parent_class->get_hostname = _get_hostname;
	parent_class->get_download_options = _get_download_options;

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

	g_type_class_add_private (klass, sizeof (VikWmscMapSourcePrivate));
	
	object_class->finalize = vik_wmsc_map_source_finalize;
}

/* 1 << (x) is like a 2**(x) */
#define GZ(x) ((1<<x))

static const gdouble scale_mpps[] = { GZ(0), GZ(1), GZ(2), GZ(3), GZ(4), GZ(5), GZ(6), GZ(7), GZ(8), GZ(9),
                                           GZ(10), GZ(11), GZ(12), GZ(13), GZ(14), GZ(15), GZ(16), GZ(17) };

static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

static const gdouble scale_neg_mpps[] = { 1.0/GZ(0), 1.0/GZ(1), 1.0/GZ(2), 1.0/GZ(3) };
static const gint num_scales_neg = (sizeof(scale_neg_mpps) / sizeof(scale_neg_mpps[0]));

#define ERROR_MARGIN 0.01
static gint wmsc_zoom ( gdouble mpp ) {
  gint i;
  for ( i = 0; i < num_scales; i++ ) {
    if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN ) {
      return i;
    }
  }
  for ( i = 0; i < num_scales_neg; i++ ) {
    if ( ABS(scale_neg_mpps[i] - mpp) < 0.000001 ) {
      return -i;
    }
  }

  return 255;
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
_coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = wmsc_zoom ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;

  /* Note : GZ(17) / xzoom / 2 = number of tile on Y axis */
  g_debug("%s: xzoom=%f yzoom=%f -> %f", __FUNCTION__,
          xzoom, yzoom, GZ(17) / xzoom / 2);
  dest->x = floor((src->east_west + 180) / 180 * GZ(17) / xzoom / 2);
  /* We should restore logic of viking:
   * tile index on Y axis follow a screen logic (top -> down)
   */
  dest->y = floor((180 - (src->north_south + 90)) / 180 * GZ(17) / xzoom / 2);
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
    socalled_mpp = GZ(src->scale);
  else
    socalled_mpp = 1.0/GZ(-src->scale);
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = (src->x+0.5) * 180 / GZ(17) * socalled_mpp * 2 - 180;
  /* We should restore logic of viking:
   * tile index on Y axis follow a screen logic (top -> down)
   */
  dest->north_south = -((src->y+0.5) * 180 / GZ(17) * socalled_mpp * 2 - 90);
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
		socalled_mpp = GZ(src->scale);
	else
		socalled_mpp = 1.0/GZ(-src->scale);
	gdouble minx = (gdouble)src->x * 180 / GZ(17) * socalled_mpp * 2 - 180;
	gdouble maxx = (gdouble)(src->x + 1) * 180 / GZ(17) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
     * tile index on Y axis follow a screen logic (top -> down)
     */
	gdouble miny = -((gdouble)(src->y + 1) * 180 / GZ(17) * socalled_mpp * 2 - 90);
	gdouble maxy = -((gdouble)(src->y) * 180 / GZ(17) * socalled_mpp * 2 - 90);
	
	gchar sminx[G_ASCII_DTOSTR_BUF_SIZE];
	gchar smaxx[G_ASCII_DTOSTR_BUF_SIZE];
	gchar sminy[G_ASCII_DTOSTR_BUF_SIZE];
	gchar smaxy[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr (sminx, G_ASCII_DTOSTR_BUF_SIZE, minx);
	g_ascii_dtostr (smaxx, G_ASCII_DTOSTR_BUF_SIZE, maxx);
	g_ascii_dtostr (sminy, G_ASCII_DTOSTR_BUF_SIZE, miny);
	g_ascii_dtostr (smaxy, G_ASCII_DTOSTR_BUF_SIZE, maxy);

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

static DownloadMapOptions *
_get_download_options( VikMapSourceDefault *self )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), NULL);
	
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return &(priv->options);
}

VikWmscMapSource *
vik_wmsc_map_source_new_with_id (guint16 id, const gchar *label, const gchar *hostname, const gchar *url)
{
	return g_object_new(VIK_TYPE_WMSC_MAP_SOURCE,
	                    "id", id, "label", label, "hostname", hostname, "url", url, NULL);
}
