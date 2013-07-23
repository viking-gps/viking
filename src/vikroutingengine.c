/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 */

/**
 * SECTION:vikroutingengine
 * @short_description: the base class to describe routing engine
 * 
 * The #VikRoutingEngine class is both the interface and the base class
 * for the hierarchie of routing engines.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "babel.h"

#include "vikroutingengine.h"

static void vik_routing_engine_finalize ( GObject *gob );
static GObjectClass *parent_class;

typedef struct _VikRoutingPrivate VikRoutingPrivate;
struct _VikRoutingPrivate
{
	gchar *id;
	gchar *label;
	gchar *format;
};

#define VIK_ROUTING_ENGINE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_ROUTING_ENGINE_TYPE, VikRoutingPrivate))

/* properties */
enum
{
  PROP_0,

  PROP_ID,
  PROP_LABEL,
  PROP_FORMAT,
};

G_DEFINE_ABSTRACT_TYPE (VikRoutingEngine, vik_routing_engine, G_TYPE_OBJECT)

static void
vik_routing_engine_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE ( object );

  switch (property_id)
    {
    case PROP_ID:
      g_free (priv->id);
      priv->id = g_strdup(g_value_get_string (value));
      break;

    case PROP_LABEL:
      g_free (priv->label);
      priv->label = g_strdup(g_value_get_string (value));
      break;

    case PROP_FORMAT:
      g_free (priv->format);
      priv->format = g_strdup(g_value_get_string (value));
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_routing_engine_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE ( object );

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;

    case PROP_LABEL:
      g_value_set_string (value, priv->label);
      break;

    case PROP_FORMAT:
      g_value_set_string (value, priv->format);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_routing_engine_class_init ( VikRoutingEngineClass *klass )
{
  GObjectClass *object_class;
  VikRoutingEngineClass *routing_class;
  GParamSpec *pspec = NULL;

  object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = vik_routing_engine_set_property;
  object_class->get_property = vik_routing_engine_get_property;
  object_class->finalize = vik_routing_engine_finalize;

  parent_class = g_type_class_peek_parent (klass);

  routing_class = VIK_ROUTING_ENGINE_CLASS ( klass );
  routing_class->find = NULL;

  routing_class->supports_direction = NULL;
  routing_class->get_cmd_from_directions = NULL;

  routing_class->refine = NULL;
  routing_class->supports_refine = NULL;

  pspec = g_param_spec_string ("id",
                               "Identifier",
                               "The identifier of the routing engine",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ID, pspec);
  
  pspec = g_param_spec_string ("label",
                               "Label",
                               "The label of the routing engine",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_LABEL, pspec);
    
  pspec = g_param_spec_string ("format",
                               "Format",
                               "The format of the output (see gpsbabel)",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FORMAT, pspec);

  g_type_class_add_private (klass, sizeof (VikRoutingPrivate));
}

static void
vik_routing_engine_init ( VikRoutingEngine *self )
{
  VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE (self);

  priv->id = NULL;
  priv->label = NULL;
  priv->format = NULL;
}

static void
vik_routing_engine_finalize ( GObject *self )
{
  VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE (self);

  g_free (priv->id);
  priv->id = NULL;

  g_free (priv->label);
  priv->label = NULL;

  g_free (priv->format);
  priv->format = NULL;

  G_OBJECT_CLASS(parent_class)->finalize(self);
}

/**
 * vik_routing_engine_find:
 * @self: self object
 * @vtl:
 * @start: starting point
 * @end: ending point
 *
 * Retrieve a route between two coordinates.
 * 
 * Returns: indicates success or not.
 */
int
vik_routing_engine_find ( VikRoutingEngine *self, VikTrwLayer *vtl, struct LatLon start, struct LatLon end )
{
	VikRoutingEngineClass *klass;
	
	g_return_val_if_fail ( VIK_IS_ROUTING_ENGINE (self), 0 );
	klass = VIK_ROUTING_ENGINE_GET_CLASS( self );
	g_return_val_if_fail ( klass->find != NULL, 0 );

	return klass->find( self, vtl, start, end );
}

/**
 * vik_routing_engine_get_id:
 * 
 * Returns: the id of self
 */
gchar *
vik_routing_engine_get_id ( VikRoutingEngine *self )
{
  VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE (self);

  return priv->id;
}

/**
 * vik_routing_engine_get_label:
 * 
 * Returns: the label of self
 */
gchar *
vik_routing_engine_get_label ( VikRoutingEngine *self )
{
  VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE (self);

  return priv->label;
}

/**
 * vik_routing_engine_get_format:
 *
 * GPSbabel's Format of result.
 *
 * Returns: the format of self
 */
gchar *
vik_routing_engine_get_format ( VikRoutingEngine *self )
{
  VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE (self);

  return priv->format;
}

/**
 * vik_routing_engine_supports_direction:
 * 
 * Returns: %TRUE if this engine supports the route finding based on directions
 */
gboolean
vik_routing_engine_supports_direction ( VikRoutingEngine *self )
{
  VikRoutingEngineClass *klass;

  g_return_val_if_fail ( VIK_IS_ROUTING_ENGINE (self), FALSE );
  klass = VIK_ROUTING_ENGINE_GET_CLASS( self );
  g_return_val_if_fail ( klass->supports_direction != NULL, FALSE );

  return klass->supports_direction( self );
}

/**
 * vik_routing_engine_get_cmd_from_directions:
 * @self: routing engine
 * @start: the start direction
 * @end: the end direction
 *
 * Compute a "cmd" for acquire framework.
 *
 * Returns: the computed cmd
 */
gchar *
vik_routing_engine_get_cmd_from_directions ( VikRoutingEngine *self, const gchar *start, const gchar *end )
{
  VikRoutingEngineClass *klass;

  g_return_val_if_fail ( VIK_IS_ROUTING_ENGINE (self), NULL );
  klass = VIK_ROUTING_ENGINE_GET_CLASS( self );
  g_return_val_if_fail ( klass->get_cmd_from_directions != NULL, NULL );

  return klass->get_cmd_from_directions( self, start, end );
}

/**
 * vik_routing_engine_refine:
 * @self: self object
 * @vtl: layer where to create new track
 * @vt: the simple route to refine
 *
 * Retrieve a route refining the @vt track/route.
 *
 * A refined route is computed from @vt.
 * The route is computed from first trackpoint to last trackpoint,
 * and going via all intermediate trackpoints.
 *
 * Returns: indicates success or not.
 */
int
vik_routing_engine_refine ( VikRoutingEngine *self, VikTrwLayer *vtl, VikTrack *vt )
{
  VikRoutingEngineClass *klass;

  g_return_val_if_fail ( VIK_IS_ROUTING_ENGINE (self), 0 );
  klass = VIK_ROUTING_ENGINE_GET_CLASS ( self );
  g_return_val_if_fail ( klass->refine != NULL, 0 );

  return klass->refine ( self, vtl, vt );
}

/**
 * vik_routing_engine_supports_refine:
 * @self: routing engine
 *
 * Returns: %TRUE if this engine supports the refine of track
 */
gboolean
vik_routing_engine_supports_refine ( VikRoutingEngine *self )
{
  VikRoutingEngineClass *klass;

  g_return_val_if_fail ( VIK_IS_ROUTING_ENGINE (self), FALSE );
  klass = VIK_ROUTING_ENGINE_GET_CLASS ( self );
  g_return_val_if_fail ( klass->supports_refine != NULL, FALSE );

  return klass->supports_refine ( self );
}
