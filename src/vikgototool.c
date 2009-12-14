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

#include "vikgototool.h"

#include <string.h>

#include <glib/gi18n.h>

static void goto_tool_class_init ( VikGotoToolClass *klass );
static void goto_tool_init ( VikGotoTool *vlp );

static GObjectClass *parent_class;

static void goto_tool_finalize ( GObject *gob );
static gchar *goto_tool_get_label ( VikGotoTool *vw );

typedef struct _VikGotoToolPrivate VikGotoToolPrivate;

struct _VikGotoToolPrivate
{
  gint   id;
  gchar *label;
};

#define GOTO_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                    VIK_GOTO_TOOL_TYPE,          \
                                    VikGotoToolPrivate))

GType vik_goto_tool_get_type()
{
  static GType w_type = 0;

  if (!w_type)
  {
    static const GTypeInfo w_info = 
    {
      sizeof (VikGotoToolClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) goto_tool_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikGotoTool),
      0,
      (GInstanceInitFunc) goto_tool_init,
    };
    w_type = g_type_register_static ( G_TYPE_OBJECT, "VikGotoTool", &w_info, G_TYPE_FLAG_ABSTRACT );
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
goto_tool_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  VikGotoTool *self = VIK_GOTO_TOOL (object);
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      priv->id = g_value_get_uint (value);
      g_debug ("VikGotoTool.id: %d", priv->id);
      break;

    case PROP_LABEL:
      g_free (priv->label);
      priv->label = g_value_dup_string (value);
      g_debug ("VikGotoTool.label: %s", priv->label);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
goto_tool_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  VikGotoTool *self = VIK_GOTO_TOOL (object);
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE (self);

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

static void goto_tool_class_init ( VikGotoToolClass *klass )
{
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = goto_tool_finalize;
  gobject_class->set_property = goto_tool_set_property;
  gobject_class->get_property = goto_tool_get_property;

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

  klass->get_label = goto_tool_get_label;

  parent_class = g_type_class_peek_parent (klass);

  g_type_class_add_private (klass, sizeof (VikGotoToolPrivate));
}

VikGotoTool *vik_goto_tool_new ()
{
  return VIK_GOTO_TOOL ( g_object_new ( VIK_GOTO_TOOL_TYPE, NULL ) );
}

static void goto_tool_init ( VikGotoTool *self )
{
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE (self);
  priv->label = NULL;
}

static void goto_tool_finalize ( GObject *gob )
{
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE ( gob );
  g_free ( priv->label ); priv->label = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static gchar *goto_tool_get_label ( VikGotoTool *self )
{
  VikGotoToolPrivate *priv = NULL;
  priv = GOTO_TOOL_GET_PRIVATE (self);
  return g_strdup ( priv->label );
}

gchar *vik_goto_tool_get_label ( VikGotoTool *w )
{
  return VIK_GOTO_TOOL_GET_CLASS( w )->get_label( w );
}

int vik_goto_tool_get_coord ( VikGotoTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord )
{
  return VIK_GOTO_TOOL_GET_CLASS( self )->get_coord( self, vw, vvp, srch_str, coord );
}
