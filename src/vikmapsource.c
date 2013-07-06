/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2009-2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
  * SECTION:vikmapsource
  * @short_description: the base class to describe map source
  * 
  * The #VikMapSource class is both the interface and the base class
  * for the hierarchie of map source.
  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vikviewport.h"
#include "vikcoord.h"
#include "mapcoord.h"

#include "vikmapsource.h"

static void vik_map_source_init (VikMapSource *object);
static void vik_map_source_finalize (GObject *object);
static void vik_map_source_class_init (VikMapSourceClass *klass);

static gboolean _supports_download_only_new (VikMapSource *object);

G_DEFINE_ABSTRACT_TYPE (VikMapSource, vik_map_source, G_TYPE_OBJECT);

static void
vik_map_source_init (VikMapSource *object)
{
	/* TODO: Add initialization code here */
}

static void
vik_map_source_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (vik_map_source_parent_class)->finalize (object);
}

static void
vik_map_source_class_init (VikMapSourceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	klass->get_copyright = NULL;
	klass->get_license = NULL;
	klass->get_license_url = NULL;
	klass->get_logo = NULL;
	klass->get_uniq_id = NULL;
	klass->get_label = NULL;
	klass->get_tilesize_x = NULL;
	klass->get_tilesize_y = NULL;
	klass->get_drawmode = NULL;
	klass->is_direct_file_access = NULL;
	klass->supports_download_only_new = _supports_download_only_new;
	klass->coord_to_mapcoord = NULL;
	klass->mapcoord_to_center_coord = NULL;
	klass->download = NULL;
	klass->download_handle_init = NULL;
	klass->download_handle_cleanup = NULL;
	
	object_class->finalize = vik_map_source_finalize;
}

gboolean
_supports_download_only_new (VikMapSource *self)
{
	// Default feature: does not support
	return FALSE;
}

/**
 * vik_map_source_get_copyright:
 * @self: the VikMapSource of interest.
 * @bbox: bounding box of interest.
 * @zoom: the zoom level of interest.
 * @fct: the callback function to use to return matching copyrights.
 * @data: the user data to use to call the callbaack function.
 *
 * Retrieve copyright(s) for the corresponding bounding box and zoom level.
 */
void
vik_map_source_get_copyright (VikMapSource *self, LatLonBBox bbox, gdouble zoom, void (*fct)(VikViewport*,const gchar*), void *data)
{
	VikMapSourceClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (VIK_IS_MAP_SOURCE (self));
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_if_fail (klass->get_copyright != NULL);

	(*klass->get_copyright)(self, bbox, zoom, fct, data);
}

const gchar *
vik_map_source_get_license (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), NULL);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_license != NULL, NULL);

	return (*klass->get_license)(self);
}

const gchar *
vik_map_source_get_license_url (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), NULL);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_license_url != NULL, NULL);

	return (*klass->get_license_url)(self);
}

const GdkPixbuf *
vik_map_source_get_logo (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), NULL);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_logo != NULL, NULL);

	return (*klass->get_logo)(self);
}

guint16
vik_map_source_get_uniq_id (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, (guint16 )0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), (guint16 )0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_uniq_id != NULL, (guint16 )0);

	return (*klass->get_uniq_id)(self);
}

const gchar *
vik_map_source_get_label (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), NULL);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_label != NULL, NULL);

	return (*klass->get_label)(self);
}

guint16
vik_map_source_get_tilesize_x (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, (guint16 )0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), (guint16 )0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_tilesize_x != NULL, (guint16 )0);

	return (*klass->get_tilesize_x)(self);
}

guint16
vik_map_source_get_tilesize_y (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, (guint16 )0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), (guint16 )0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_tilesize_y != NULL, (guint16 )0);

	return (*klass->get_tilesize_y)(self);
}

VikViewportDrawMode
vik_map_source_get_drawmode (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, (VikViewportDrawMode )0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), (VikViewportDrawMode )0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_drawmode != NULL, (VikViewportDrawMode )0);

	return (*klass->get_drawmode)(self);
}

/**
 * vik_map_source_is_direct_file_access:
 * @self: the VikMapSource of interest.
 *
 *   Return true when we can bypass all this download malarky
 *   Treat the files as a pre generated data set in OSM tile server layout: <tiledir>/%d/%d/%d.png
 */
gboolean
vik_map_source_is_direct_file_access (VikMapSource * self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), 0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->is_direct_file_access != NULL, 0);

	return (*klass->is_direct_file_access)(self);
}

gboolean
vik_map_source_supports_download_only_new (VikMapSource * self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), 0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->supports_download_only_new != NULL, 0);

	return (*klass->supports_download_only_new)(self);
}

gboolean
vik_map_source_coord_to_mapcoord (VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), FALSE);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->coord_to_mapcoord != NULL, FALSE);

	return (*klass->coord_to_mapcoord)(self, src, xzoom, yzoom, dest);
}

void
vik_map_source_mapcoord_to_center_coord (VikMapSource *self, MapCoord *src, VikCoord *dest)
{
	VikMapSourceClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (VIK_IS_MAP_SOURCE (self));
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_if_fail (klass->mapcoord_to_center_coord != NULL);

	(*klass->mapcoord_to_center_coord)(self, src, dest);
}

int
vik_map_source_download (VikMapSource * self, MapCoord * src, const gchar * dest_fn, void *handle)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), 0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->download != NULL, 0);

	return (*klass->download)(self, src, dest_fn, handle);
}

void *
vik_map_source_download_handle_init (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), 0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->download_handle_init != NULL, 0);

	return (*klass->download_handle_init)(self);
}

void
vik_map_source_download_handle_cleanup (VikMapSource * self, void * handle)
{
	VikMapSourceClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (VIK_IS_MAP_SOURCE (self));
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_if_fail (klass->download_handle_cleanup != NULL);

	(*klass->download_handle_cleanup)(self, handle);
}
