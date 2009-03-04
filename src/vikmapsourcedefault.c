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

#include "vikmapsourcedefault.h"

static guint8 map_source_get_uniq_id (VikMapSource *self);
static guint16 map_source_get_tilesize_x (VikMapSource *self);
static guint16 map_source_get_tilesize_y (VikMapSource *self);
static VikViewportDrawMode map_source_get_drawmode (VikMapSource *self);

typedef struct _VikMapSourceDefaultPrivate VikMapSourceDefaultPrivate;
struct _VikMapSourceDefaultPrivate
{
	guint8 uniq_id;
	guint16 tilesize_x;
	guint16 tilesize_y;
	VikViewportDrawMode drawmode;
};

#define VIK_MAP_SOURCE_DEFAULT_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_MAP_SOURCE_DEFAULT, VikMapSourceDefaultPrivate))


G_DEFINE_TYPE_EXTENDED (VikMapSourceDefault, vik_map_source_default, VIK_TYPE_MAP_SOURCE, (GTypeFlags)G_TYPE_FLAG_ABSTRACT,);

static void
vik_map_source_default_init (VikMapSourceDefault *object)
{
	/* TODO: Add initialization code here */
}

static void
vik_map_source_default_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (vik_map_source_default_parent_class)->finalize (object);
}

static void
vik_map_source_default_class_init (VikMapSourceDefaultClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* parent_class = VIK_MAP_SOURCE_CLASS (klass);

	/* Overiding methods */
	parent_class->get_uniq_id =    map_source_get_uniq_id;
	parent_class->get_tilesize_x = map_source_get_tilesize_x;
	parent_class->get_tilesize_y = map_source_get_tilesize_y;
	parent_class->get_drawmode =   map_source_get_drawmode;

	g_type_class_add_private (klass, sizeof (VikMapSourceDefaultPrivate));

	object_class->finalize = vik_map_source_default_finalize;
}

static guint8
map_source_get_uniq_id (VikMapSource *self)
{
    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
	g_return_val_if_fail (priv != NULL, (guint8 )0);

	return priv->uniq_id;
}

static guint16
map_source_get_tilesize_x (VikMapSource *self)
{
    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
	g_return_val_if_fail (self != NULL, (guint16 )0);

	return priv->tilesize_x;
}

static guint16
map_source_get_tilesize_y (VikMapSource *self)
{
    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
	g_return_val_if_fail (self != NULL, (guint16 )0);

	return priv->tilesize_y;
}

static VikViewportDrawMode
map_source_get_drawmode (VikMapSource *self)
{
    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
	g_return_val_if_fail (self != NULL, (VikViewportDrawMode )0);

	return priv->drawmode;
}
