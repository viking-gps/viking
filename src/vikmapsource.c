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

#include "vikviewport.h"
#include "vikcoord.h"
#include "mapcoord.h"

#include "vikmapsource.h"

static void vik_map_source_init (VikMapSource *object);
static void vik_map_source_finalize (GObject *object);
static void vik_map_source_class_init (VikMapSourceClass *klass);

static gboolean _supports_if_modified_since (VikMapSource *object);

G_DEFINE_TYPE_EXTENDED (VikMapSource, vik_map_source, G_TYPE_OBJECT, (GTypeFlags)G_TYPE_FLAG_ABSTRACT,);

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

	klass->get_uniq_id = NULL;
	klass->get_label = NULL;
	klass->get_tilesize_x = NULL;
	klass->get_tilesize_y = NULL;
	klass->get_drawmode = NULL;
	klass->supports_if_modified_since = _supports_if_modified_since;
	klass->coord_to_mapcoord = NULL;
	klass->mapcoord_to_center_coord = NULL;
	klass->download = NULL;
	klass->download_handle_init = NULL;
	klass->download_handle_cleanup = NULL;
	
	object_class->finalize = vik_map_source_finalize;
}

gboolean
_supports_if_modified_since (VikMapSource *self)
{
	// Default feature: does not support
	return FALSE;
}

guint8
vik_map_source_get_uniq_id (VikMapSource *self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, (guint8 )0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), (guint8 )0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->get_uniq_id != NULL, (guint8 )0);

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

gboolean
vik_map_source_supports_if_modified_since (VikMapSource * self)
{
	VikMapSourceClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE (self), 0);
	klass = VIK_MAP_SOURCE_GET_CLASS(self);

	g_return_val_if_fail (klass->supports_if_modified_since != NULL, 0);

	return (*klass->supports_if_modified_since)(self);
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

	return (*klass->mapcoord_to_center_coord)(self, src, dest);
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
