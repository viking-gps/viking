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
static int _download ( VikMapSource *self, MapCoord *src, const gchar *dest_fn );

static gchar *_get_uri( VikSlippyMapSource *self, MapCoord *src );
static gchar *_get_hostname( VikSlippyMapSource *self );
static DownloadOptions *_get_download_options( VikSlippyMapSource *self );

/* FIXME Huge gruik */
static DownloadOptions slippy_options = { NULL, 0, a_check_map_file };

typedef struct _VikSlippyMapSourcePrivate VikSlippyMapSourcePrivate;
struct _VikSlippyMapSourcePrivate
{
  gchar *hostname;
  gchar *url;
};

#define VIK_SLIPPY_MAP_SOURCE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_SLIPPY_MAP_SOURCE, VikSlippyMapSourcePrivate))

G_DEFINE_TYPE_EXTENDED (VikSlippyMapSource, vik_slippy_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT, (GTypeFlags)0,);

static void
vik_slippy_map_source_init (VikSlippyMapSource *object)
{
	/* initialize the object here */
	// FIXME VIK_MAP_SOURCE_DEFAULT(self)->tilesize_x = 256;
	// FIXME VIK_MAP_SOURCE_DEFAULT(self)->tilesize_y = 256;
	// FIXME VIK_MAP_SOURCE_DEFAULT(self)->drawmode = VIK_VIEWPORT_DRAWMODE_MERCATOR;
}

static void
vik_slippy_map_source_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (vik_slippy_map_source_parent_class)->finalize (object);
}

static void
vik_slippy_map_source_class_init (VikSlippyMapSourceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* parent_class = VIK_MAP_SOURCE_CLASS (klass);

	/* Overiding methods */
	parent_class->coord_to_mapcoord =        _coord_to_mapcoord;
	parent_class->mapcoord_to_center_coord = _mapcoord_to_center_coord;
	parent_class->download =                 _download;
	
	/* Default implementation of methods */
	klass->get_uri = _get_uri;
	klass->get_hostname = _get_hostname;
	klass->get_download_options = _get_download_options;
	
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
_download ( VikMapSource *self, MapCoord *src, const gchar *dest_fn )
{
   int res;
   gchar *uri = vik_slippy_map_source_get_uri(VIK_SLIPPY_MAP_SOURCE(self), src);
   gchar *host = vik_slippy_map_source_get_hostname(VIK_SLIPPY_MAP_SOURCE(self));
   DownloadOptions *options = vik_slippy_map_source_get_download_options(VIK_SLIPPY_MAP_SOURCE(self));
   res = a_http_download_get_url ( host, uri, dest_fn, options );
   g_free ( uri );
   g_free ( host );
   return res;
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
	
	return &slippy_options;
}

VikSlippyMapSource *
vik_slippy_map_source_new_with_id (guint8 id, const gchar *hostname, const gchar *url)
{
	VikSlippyMapSource *ret = g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE, NULL);
	
    VikSlippyMapSourcePrivate *priv = VIK_SLIPPY_MAP_SOURCE_PRIVATE(ret);
	// FIXME VIK_MAP_SOURCE_DEFAULT(ret)->uniq_id = id;
	priv->hostname = g_strdup(hostname);
	priv->url = g_strdup(url);
	return ret;
}