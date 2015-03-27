/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011-2015, Rob Norris <rw_norris@hotmail.com>
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

#include "vikwebtoolbounds.h"

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "globals.h"

static GObjectClass *parent_class;

static void webtool_bounds_finalize ( GObject *gob );
static gchar *webtool_bounds_get_url ( VikWebtool *vw, VikWindow *vwindow );
static gchar *webtool_bounds_get_url_at_position ( VikWebtool *vw, VikWindow *vwindow, VikCoord *vc );

typedef struct _VikWebtoolBoundsPrivate VikWebtoolBoundsPrivate;

struct _VikWebtoolBoundsPrivate
{
  gchar *url;
};

#define WEBTOOL_BOUNDS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                       VIK_WEBTOOL_BOUNDS_TYPE,          \
                                       VikWebtoolBoundsPrivate))

G_DEFINE_TYPE (VikWebtoolBounds, vik_webtool_bounds, VIK_WEBTOOL_TYPE)

enum
{
  PROP_0,

  PROP_URL,
};

static void
webtool_bounds_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  VikWebtoolBounds *self = VIK_WEBTOOL_BOUNDS (object);
  VikWebtoolBoundsPrivate *priv = WEBTOOL_BOUNDS_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_URL:
      g_free (priv->url);
      priv->url = g_value_dup_string (value);
      g_debug ("VikWebtoolBounds.url: %s", priv->url);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
webtool_bounds_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  VikWebtoolBounds *self = VIK_WEBTOOL_BOUNDS (object);
  VikWebtoolBoundsPrivate *priv = WEBTOOL_BOUNDS_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_webtool_bounds_class_init ( VikWebtoolBoundsClass *klass )
{
  GObjectClass *gobject_class;
  VikWebtoolClass *base_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = webtool_bounds_finalize;
  gobject_class->set_property = webtool_bounds_set_property;
  gobject_class->get_property = webtool_bounds_get_property;

  pspec = g_param_spec_string ("url",
                               "Template Url",
                               "Set the template url",
                               VIKING_URL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_URL,
                                   pspec);

  parent_class = g_type_class_peek_parent (klass);

  base_class = VIK_WEBTOOL_CLASS ( klass );
  base_class->get_url = webtool_bounds_get_url;
  base_class->get_url_at_position = webtool_bounds_get_url_at_position;

  g_type_class_add_private (klass, sizeof (VikWebtoolBoundsPrivate));
}

VikWebtoolBounds *vik_webtool_bounds_new ()
{
  return VIK_WEBTOOL_BOUNDS ( g_object_new ( VIK_WEBTOOL_BOUNDS_TYPE, NULL ) );
}

VikWebtoolBounds *vik_webtool_bounds_new_with_members ( const gchar *label, const gchar *url )
{
  VikWebtoolBounds *result = VIK_WEBTOOL_BOUNDS ( g_object_new ( VIK_WEBTOOL_BOUNDS_TYPE,
                                                                 "label", label,
                                                                 "url", url,
                                                                 NULL ) );

  return result;
}

static void
vik_webtool_bounds_init ( VikWebtoolBounds *self )
{
  VikWebtoolBoundsPrivate *priv = WEBTOOL_BOUNDS_GET_PRIVATE (self);
  priv->url = NULL;
}

static void webtool_bounds_finalize ( GObject *gob )
{
  VikWebtoolBoundsPrivate *priv = WEBTOOL_BOUNDS_GET_PRIVATE ( gob );
  g_free ( priv->url ); priv->url = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static gchar *webtool_bounds_get_url ( VikWebtool *self, VikWindow *vwindow )
{
  VikWebtoolBoundsPrivate *priv = NULL;
  VikViewport *viewport = NULL;

  priv = WEBTOOL_BOUNDS_GET_PRIVATE (self);
  viewport = vik_window_viewport ( vwindow );

  // Get top left and bottom right lat/lon pairs from the viewport
  gdouble min_lat, max_lat, min_lon, max_lon;
  gchar sminlon[G_ASCII_DTOSTR_BUF_SIZE];
  gchar smaxlon[G_ASCII_DTOSTR_BUF_SIZE];
  gchar sminlat[G_ASCII_DTOSTR_BUF_SIZE];
  gchar smaxlat[G_ASCII_DTOSTR_BUF_SIZE];
  vik_viewport_get_min_max_lat_lon ( viewport, &min_lat, &max_lat, &min_lon, &max_lon );

  // Cannot simply use g_strdup_printf and gdouble due to locale.
  // As we compute an URL, we have to think in C locale.
  g_ascii_dtostr (sminlon, G_ASCII_DTOSTR_BUF_SIZE, min_lon);
  g_ascii_dtostr (smaxlon, G_ASCII_DTOSTR_BUF_SIZE, max_lon);
  g_ascii_dtostr (sminlat, G_ASCII_DTOSTR_BUF_SIZE, min_lat);
  g_ascii_dtostr (smaxlat, G_ASCII_DTOSTR_BUF_SIZE, max_lat);

  return g_strdup_printf ( priv->url, sminlon, smaxlon, sminlat, smaxlat );
}

static gchar *webtool_bounds_get_url_at_position ( VikWebtool *self, VikWindow *vwindow, VikCoord *vc )
{
  // TODO: could use zoom level to generate an offset from center lat/lon to get the bounds
  // For now simply use the existing function to use bounds from the viewport
  return webtool_bounds_get_url ( self, vwindow );
}
