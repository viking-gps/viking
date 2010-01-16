/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) Guilhem Bonnefille 2009 <guilhem.bonnefille@gmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "globals.h"
#include "vikslippymapsource.h"

static gboolean _coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void _mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest );
static int _download ( VikMapSource *self, MapCoord *src, const gchar *dest_fn, void *handle );
static void * _download_handle_init ( VikMapSource *self);
static void _download_handle_cleanup ( VikMapSource *self, void *handle);
static gboolean _supports_if_modified_since (VikMapSource *self );

static gchar *_get_uri( VikSlippyMapSource *self, MapCoord *src );
static gchar *_get_hostname( VikSlippyMapSource *self );
static DownloadOptions *_get_download_options( VikSlippyMapSource *self );

typedef struct _VikSlippyMapSourcePrivate VikSlippyMapSourcePrivate;
struct _VikSlippyMapSourcePrivate
{
  gchar *hostname;
  gchar *url;
  DownloadOptions options;
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
};

G_DEFINE_TYPE_EXTENDED (VikSlippyMapSource, vik_slippy_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT, (GTypeFlags)0,);

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
  priv->options.check_file_server_time = 0;

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
      priv->options.check_file_server_time = g_value_get_uint (value);
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
      g_value_set_uint (value, priv->options.check_file_server_time);
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
	VikMapSourceClass* parent_class = VIK_MAP_SOURCE_CLASS (klass);
	GParamSpec *pspec = NULL;
		
	object_class->set_property = vik_slippy_map_source_set_property;
    object_class->get_property = vik_slippy_map_source_get_property;

	/* Overiding methods */
	parent_class->coord_to_mapcoord =        _coord_to_mapcoord;
	parent_class->mapcoord_to_center_coord = _mapcoord_to_center_coord;
	parent_class->download =                 _download;
	parent_class->download_handle_init =     _download_handle_init;
	parent_class->download_handle_cleanup =  _download_handle_cleanup;
	parent_class->supports_if_modified_since = _supports_if_modified_since;
	
	/* Default implementation of methods */
	klass->get_uri = _get_uri;
	klass->get_hostname = _get_hostname;
	klass->get_download_options = _get_download_options;

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
	
	pspec = g_param_spec_uint ("check-file-server-time",
	                           "Check file server time",
                               "Age of current cache before redownloading tile",
                               0  /* minimum value */,
                               G_MAXUINT16 /* maximum value */,
                               0  /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CHECK_FILE_SERVER_TIME, pspec);

	g_type_class_add_private (klass, sizeof (VikSlippyMapSourcePrivate));
	
	object_class->finalize = vik_slippy_map_source_finalize;
}

/* 1 << (x) is like a 2**(x) */
#define GZ(x) ((1<<x))

static const gdouble scale_mpps[] = { GZ(0), GZ(1), GZ(2), GZ(3), GZ(4), GZ(5), GZ(6), GZ(7), GZ(8), GZ(9),
                                           GZ(10), GZ(11), GZ(12), GZ(13), GZ(14), GZ(15), GZ(16), GZ(17) };

static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

#define ERROR_MARGIN 0.01
static guint8 slippy_zoom ( gdouble mpp ) {
  gint i;
  for ( i = 0; i < num_scales; i++ ) {
    if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN )
      return i;
  }
  return 255;
}

gchar *
vik_slippy_map_source_get_uri( VikSlippyMapSource *self, MapCoord *src )
{
	VikSlippyMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE (self), 0);
	klass = VIK_SLIPPY_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_uri != NULL, 0);

	return (*klass->get_uri)(self, src);
}

gchar *
vik_slippy_map_source_get_hostname( VikSlippyMapSource *self )
{
	VikSlippyMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE (self), 0);
	klass = VIK_SLIPPY_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_hostname != NULL, 0);

	return (*klass->get_hostname)(self);
}

DownloadOptions *
vik_slippy_map_source_get_download_options( VikSlippyMapSource *self )
{
	VikSlippyMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE (self), 0);
	klass = VIK_SLIPPY_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_download_options != NULL, 0);

	return (*klass->get_download_options)(self);
}

gboolean
_supports_if_modified_since (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), FALSE);
	
    VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	
	g_debug ("%s: priv->options.check_file_server_time = %d", __FUNCTION__, priv->options.check_file_server_time);
	
	return priv->options.check_file_server_time != 0;
}
static gboolean
_coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
  g_assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return FALSE;

  dest->scale = slippy_zoom ( xzoom );
  if ( dest->scale == 255 )
    return FALSE;

  dest->x = (src->east_west + 180) / 360 * GZ(17) / xzoom;
  dest->y = (180 - MERCLAT(src->north_south)) / 360 * GZ(17) / xzoom;
  dest->z = 0;

  return TRUE;
}

static void
_mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest )
{
  gdouble socalled_mpp = GZ(src->scale);
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = ((src->x+0.5) / GZ(17) * socalled_mpp * 360) - 180;
  dest->north_south = DEMERCLAT(180 - ((src->y+0.5) / GZ(17) * socalled_mpp * 360));
}

static int
_download ( VikMapSource *self, MapCoord *src, const gchar *dest_fn, void *handle )
{
   int res;
   gchar *uri = vik_slippy_map_source_get_uri(VIK_SLIPPY_MAP_SOURCE(self), src);
   gchar *host = vik_slippy_map_source_get_hostname(VIK_SLIPPY_MAP_SOURCE(self));
   DownloadOptions *options = vik_slippy_map_source_get_download_options(VIK_SLIPPY_MAP_SOURCE(self));
   res = a_http_download_get_url ( host, uri, dest_fn, options, handle );
   g_free ( uri );
   g_free ( host );
   return res;
}

static void *
_download_handle_init ( VikMapSource *self )
{
   return a_download_handle_init ();
}


static void
_download_handle_cleanup ( VikMapSource *self, void *handle )
{
   return a_download_handle_cleanup ( handle );
}

static gchar *
_get_uri( VikSlippyMapSource *self, MapCoord *src )
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), NULL);
	
    VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	gchar *uri = g_strdup_printf (priv->url, 17 - src->scale, src->x, src->y);
	return uri;
} 

static gchar *
_get_hostname( VikSlippyMapSource *self )
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), NULL);
	
    VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	return g_strdup( priv->hostname );
}

static DownloadOptions *
_get_download_options( VikSlippyMapSource *self )
{
	g_return_val_if_fail (VIK_IS_SLIPPY_MAP_SOURCE(self), NULL);
	
	VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(self);
	return &(priv->options);
}

VikSlippyMapSource *
vik_slippy_map_source_new_with_id (guint8 id, const gchar *label, const gchar *hostname, const gchar *url)
{
	return g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
	                    "id", id, "label", label, "hostname", hostname, "url", url, NULL);
}
