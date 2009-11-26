/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viksearchtool.h"

#include <string.h>

#include <glib/gi18n.h>

static void search_tool_class_init ( VikSearchToolClass *klass );
static void search_tool_init ( VikSearchTool *vlp );

static GObjectClass *parent_class;

static void search_tool_finalize ( GObject *gob );
static gchar *search_tool_get_label ( VikSearchTool *vw );

typedef struct _VikSearchToolPrivate VikSearchToolPrivate;

struct _VikSearchToolPrivate
{
  gint   id;
  gchar *label;
};

#define SEARCH_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                    VIK_SEARCH_TOOL_TYPE,          \
                                    VikSearchToolPrivate))

GType vik_search_tool_get_type()
{
  static GType w_type = 0;

  if (!w_type)
  {
    static const GTypeInfo w_info = 
    {
      sizeof (VikSearchToolClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) search_tool_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikSearchTool),
      0,
      (GInstanceInitFunc) search_tool_init,
    };
    w_type = g_type_register_static ( G_TYPE_OBJECT, "VikSearchTool", &w_info, G_TYPE_FLAG_ABSTRACT );
  }

  return w_type;
}

enum
{
  PROP_0,

  PROP_ID,
  PROP_LABEL,
};

static void
search_tool_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  VikSearchTool *self = VIK_SEARCH_TOOL (object);
  VikSearchToolPrivate *priv = SEARCH_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      priv->id = g_value_get_uint (value);
      g_debug ("VikSearchTool.id: %d", priv->id);
      break;

    case PROP_LABEL:
      g_free (priv->label);
      priv->label = g_value_dup_string (value);
      g_debug ("VikSearchTool.label: %s", priv->label);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
search_tool_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  VikSearchTool *self = VIK_SEARCH_TOOL (object);
  VikSearchToolPrivate *priv = SEARCH_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_uint (value, priv->id);
      break;

    case PROP_LABEL:
      g_value_set_string (value, priv->label);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void search_tool_class_init ( VikSearchToolClass *klass )
{
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = search_tool_finalize;
  gobject_class->set_property = search_tool_set_property;
  gobject_class->get_property = search_tool_get_property;

  pspec = g_param_spec_string ("label",
                               "Label",
                               "Set the label",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   pspec);

  pspec = g_param_spec_uint ("id",
                             "Id of the tool",
                             "Set the id",
                             0  /* minimum value */,
                             G_MAXUINT16 /* maximum value */,
                             0  /* default value */,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   pspec);

  klass->get_label = search_tool_get_label;

  parent_class = g_type_class_peek_parent (klass);

  g_type_class_add_private (klass, sizeof (VikSearchToolPrivate));
}

VikSearchTool *vik_search_tool_new ()
{
  return VIK_SEARCH_TOOL ( g_object_new ( VIK_SEARCH_TOOL_TYPE, NULL ) );
}

static void search_tool_init ( VikSearchTool *self )
{
  VikSearchToolPrivate *priv = SEARCH_TOOL_GET_PRIVATE (self);
  priv->label = NULL;
}

static void search_tool_finalize ( GObject *gob )
{
  VikSearchToolPrivate *priv = SEARCH_TOOL_GET_PRIVATE ( gob );
  g_free ( priv->label ); priv->label = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static gchar *search_tool_get_label ( VikSearchTool *self )
{
  VikSearchToolPrivate *priv = NULL;
  priv = SEARCH_TOOL_GET_PRIVATE (self);
  return g_strdup ( priv->label );
}

gchar *vik_search_tool_get_label ( VikSearchTool *w )
{
  return VIK_SEARCH_TOOL_GET_CLASS( w )->get_label( w );
}

int vik_search_tool_get_coord ( VikSearchTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord )
{
  return VIK_SEARCH_TOOL_GET_CLASS( self )->get_coord( self, vw, vvp, srch_str, coord );
}
