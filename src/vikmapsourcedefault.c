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
#include "vikenumtypes.h"

static guint8 map_source_get_uniq_id (VikMapSource *self);
static const gchar *map_source_get_label (VikMapSource *self);
static guint16 map_source_get_tilesize_x (VikMapSource *self);
static guint16 map_source_get_tilesize_y (VikMapSource *self);
static VikViewportDrawMode map_source_get_drawmode (VikMapSource *self);

typedef struct _VikMapSourceDefaultPrivate VikMapSourceDefaultPrivate;
struct _VikMapSourceDefaultPrivate
{
	guint8 uniq_id;
	gchar *label;
	guint16 tilesize_x;
	guint16 tilesize_y;
	VikViewportDrawMode drawmode;
};

#define VIK_MAP_SOURCE_DEFAULT_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_MAP_SOURCE_DEFAULT, VikMapSourceDefaultPrivate))

/* properties */
enum
{
  PROP_0,

  PROP_ID,
  PROP_LABEL,
  PROP_TILESIZE_X,
  PROP_TILESIZE_Y,
  PROP_DRAWMODE,
};

G_DEFINE_TYPE_EXTENDED (VikMapSourceDefault, vik_map_source_default, VIK_TYPE_MAP_SOURCE, (GTypeFlags)G_TYPE_FLAG_ABSTRACT,);

static void
vik_map_source_default_init (VikMapSourceDefault *object)
{
  VikMapSourceDefault *self = VIK_MAP_SOURCE_DEFAULT (object);
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (self);

  priv->label = NULL;
}

static void
vik_map_source_default_finalize (GObject *object)
{
  VikMapSourceDefault *self = VIK_MAP_SOURCE_DEFAULT (object);
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (self);

  g_free (priv->label);
  priv->label = NULL;
	
  G_OBJECT_CLASS (vik_map_source_default_parent_class)->finalize (object);
}

static void
vik_map_source_default_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  VikMapSourceDefault *self = VIK_MAP_SOURCE_DEFAULT (object);
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      priv->uniq_id = g_value_get_uint (value);
      break;

	case PROP_LABEL:
      g_free (priv->label);
      priv->label = g_strdup(g_value_get_string (value));
      break;

    case PROP_TILESIZE_X:
      priv->tilesize_x = g_value_get_uint (value);
      break;

    case PROP_TILESIZE_Y:
      priv->tilesize_y = g_value_get_uint (value);
      break;

    case PROP_DRAWMODE:
      priv->drawmode = g_value_get_enum(value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_map_source_default_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  VikMapSourceDefault *self = VIK_MAP_SOURCE_DEFAULT (object);
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_uint (value, priv->uniq_id);
      break;

    case PROP_LABEL:
      g_value_set_string (value, priv->label);
      break;

    case PROP_TILESIZE_X:
      g_value_set_uint (value, priv->tilesize_x);
      break;

    case PROP_TILESIZE_Y:
      g_value_set_uint (value, priv->tilesize_y);
      break;

    case PROP_DRAWMODE:
      g_value_set_enum (value, priv->drawmode);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_map_source_default_class_init (VikMapSourceDefaultClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* parent_class = VIK_MAP_SOURCE_CLASS (klass);
    GParamSpec *pspec = NULL;
	
	object_class->set_property = vik_map_source_default_set_property;
    object_class->get_property = vik_map_source_default_get_property;
	
	/* Overiding methods */
	parent_class->get_uniq_id =    map_source_get_uniq_id;
	parent_class->get_label =      map_source_get_label;
	parent_class->get_tilesize_x = map_source_get_tilesize_x;
	parent_class->get_tilesize_y = map_source_get_tilesize_y;
	parent_class->get_drawmode =   map_source_get_drawmode;

	pspec = g_param_spec_uint ("id",
	                           "Id of the tool",
                               "Set the id",
                               0  /* minimum value */,
                               G_MAXUINT8 /* maximum value */,
                               0  /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	pspec = g_param_spec_string ("label",
	                             "Label",
	                             "The label of the map source",
	                             "<no-set>" /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LABEL, pspec);

	pspec = g_param_spec_uint ("tilesize-x",
	                           "TileSizeX",
                               "Set the size of the tile (x)",
                               0  /* minimum value */,
                               G_MAXUINT16 /* maximum value */,
                               0  /* default value */,
                               G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TILESIZE_X, pspec);

	pspec = g_param_spec_uint ("tilesize-y",
	                           "TileSizeY",
                               "Set the size of the tile (y)",
                               0  /* minimum value */,
                               G_MAXUINT16 /* maximum value */,
                               0  /* default value */,
                               G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TILESIZE_Y, pspec);

	pspec = g_param_spec_enum("drawmode",
                              "Drawmode",
                              "The mode used to draw map",
                              VIK_TYPE_VIEWPORT_DRAW_MODE,
                              VIK_VIEWPORT_DRAWMODE_UTM,
                              G_PARAM_READWRITE);
    g_object_class_install_property(object_class, PROP_DRAWMODE, pspec);                                    

	g_type_class_add_private (klass, sizeof (VikMapSourceDefaultPrivate));

	object_class->finalize = vik_map_source_default_finalize;
}

static guint8
map_source_get_uniq_id (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (guint8)0);
	
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->uniq_id;
}

static const gchar *
map_source_get_label (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), NULL);
	
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->label;
}

static guint16
map_source_get_tilesize_x (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (guint16)0);

    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->tilesize_x;
}

static guint16
map_source_get_tilesize_y (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (guint16)0);

    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->tilesize_y;
}

static VikViewportDrawMode
map_source_get_drawmode (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (VikViewportDrawMode)0);

    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->drawmode;
}
