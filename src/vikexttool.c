/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "vikexttool.h"

#include <string.h>

#include <glib/gi18n.h>

static GObjectClass *parent_class;

static void ext_tool_finalize ( GObject *gob );
static gchar *ext_tool_get_label ( VikExtTool *vw );

typedef struct _VikExtToolPrivate VikExtToolPrivate;

struct _VikExtToolPrivate
{
  gint   id;
  gchar *label;
};

#define EXT_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                VIK_EXT_TOOL_TYPE,          \
                                VikExtToolPrivate))

G_DEFINE_ABSTRACT_TYPE (VikExtTool, vik_ext_tool, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_ID,
  PROP_LABEL,
};

static void
ext_tool_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  VikExtTool *self = VIK_EXT_TOOL (object);
  VikExtToolPrivate *priv = EXT_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      priv->id = g_value_get_uint (value);
      g_debug ("VikExtTool.id: %d", priv->id);
      break;

    case PROP_LABEL:
      g_free (priv->label);
      priv->label = g_value_dup_string (value);
      g_debug ("VikExtTool.label: %s", priv->label);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ext_tool_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  VikExtTool *self = VIK_EXT_TOOL (object);
  VikExtToolPrivate *priv = EXT_TOOL_GET_PRIVATE (self);

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

static void vik_ext_tool_class_init ( VikExtToolClass *klass )
{
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ext_tool_finalize;
  gobject_class->set_property = ext_tool_set_property;
  gobject_class->get_property = ext_tool_get_property;

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

  klass->get_label = ext_tool_get_label;

  parent_class = g_type_class_peek_parent (klass);

  g_type_class_add_private (klass, sizeof (VikExtToolPrivate));
}

static void vik_ext_tool_init ( VikExtTool *self )
{
  VikExtToolPrivate *priv = EXT_TOOL_GET_PRIVATE (self);
  priv->label = NULL;
}

static void ext_tool_finalize ( GObject *gob )
{
  VikExtToolPrivate *priv = EXT_TOOL_GET_PRIVATE ( gob );
  g_free ( priv->label ); priv->label = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static gchar *ext_tool_get_label ( VikExtTool *self )
{
  VikExtToolPrivate *priv = NULL;
  priv = EXT_TOOL_GET_PRIVATE (self);
  return g_strdup ( priv->label );
}

gchar *vik_ext_tool_get_label ( VikExtTool *w )
{
  return VIK_EXT_TOOL_GET_CLASS( w )->get_label( w );
}

void vik_ext_tool_open ( VikExtTool *self, VikWindow *vwindow )
{
  VIK_EXT_TOOL_GET_CLASS( self )->open( self, vwindow );
}

void vik_ext_tool_open_at_position ( VikExtTool *self, VikWindow *vwindow, VikCoord *vc )
{
  if ( VIK_EXT_TOOL_GET_CLASS( self )->open_at_position )
    VIK_EXT_TOOL_GET_CLASS( self )->open_at_position( self, vwindow, vc );
}
