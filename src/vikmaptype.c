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

#include "vikmaptype.h"
#include "vikmapslayer_compat.h"

static gboolean map_type_coord_to_mapcoord (VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
static void map_type_mapcoord_to_center_coord (VikMapSource *self, MapCoord *src, VikCoord *dest);
static int map_type_download (VikMapSource * self, MapCoord * src, const gchar * dest_fn);

typedef struct _VikMapTypePrivate VikMapTypePrivate;
struct _VikMapTypePrivate
{
	gchar *label;
	VikMapsLayer_MapType map_type;
};

#define VIK_MAP_TYPE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_MAP_TYPE, VikMapTypePrivate))


G_DEFINE_TYPE (VikMapType, vik_map_type, VIK_TYPE_MAP_SOURCE);

static void
vik_map_type_init (VikMapType *object)
{
	VikMapTypePrivate *priv = VIK_MAP_TYPE_PRIVATE(object);
	priv->label = NULL;
}

VikMapType *
vik_map_type_new_with_id (VikMapsLayer_MapType map_type, const char *label)
{
	VikMapType *ret = (VikMapType *)g_object_new(vik_map_type_get_type(), NULL);
	VikMapTypePrivate *priv = VIK_MAP_TYPE_PRIVATE(ret);
	priv->map_type = map_type;
	priv->label = g_strdup (label);
	return ret;
}

static void
vik_map_type_finalize (GObject *object)
{
	VikMapTypePrivate *priv = VIK_MAP_TYPE_PRIVATE(object);
	g_free (priv->label);
	priv->label = NULL;

	G_OBJECT_CLASS (vik_map_type_parent_class)->finalize (object);
}

static void
vik_map_type_class_init (VikMapTypeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* parent_class = VIK_MAP_SOURCE_CLASS (klass);

	/* Overiding methods */
	parent_class->coord_to_mapcoord =        map_type_coord_to_mapcoord;
	parent_class->mapcoord_to_center_coord = map_type_mapcoord_to_center_coord;
	parent_class->download =                 map_type_download;

	g_type_class_add_private (klass, sizeof (VikMapTypePrivate));

	object_class->finalize = vik_map_type_finalize;
}

static gboolean
map_type_coord_to_mapcoord (VikMapSource *self, const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest )
{
    VikMapTypePrivate *priv = VIK_MAP_TYPE_PRIVATE(self);
	g_return_val_if_fail (priv != NULL, FALSE);

	return (priv->map_type.coord_to_mapcoord)(src, xzoom, yzoom, dest);
}

static void
map_type_mapcoord_to_center_coord (VikMapSource *self, MapCoord *src, VikCoord *dest)
{
    VikMapTypePrivate *priv = VIK_MAP_TYPE_PRIVATE(self);
	g_return_if_fail (self != NULL);

	return (priv->map_type.mapcoord_to_center_coord)(src, dest);
}

static int
map_type_download (VikMapSource * self, MapCoord * src, const gchar * dest_fn)
{
    VikMapTypePrivate *priv = VIK_MAP_TYPE_PRIVATE(self);
	g_return_val_if_fail (priv != NULL, 0);

	return (priv->map_type.download)(src, dest_fn);
}

