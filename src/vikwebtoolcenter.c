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

#include "vikwebtoolcenter.h"

#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "util.h"
#include "globals.h"
#include "maputils.h"

static GObjectClass *parent_class;

static void webtool_center_finalize ( GObject *gob );

static guint8 webtool_center_mpp_to_zoom ( VikWebtool *self, gdouble mpp );
static gchar *webtool_center_get_url ( VikWebtool *vw, VikWindow *vwindow );
static gchar *webtool_center_get_url_at_position ( VikWebtool *vw, VikWindow *vwindow, VikCoord *vc );

typedef struct _VikWebtoolCenterPrivate VikWebtoolCenterPrivate;

struct _VikWebtoolCenterPrivate
{
  gchar *url;
};

G_DEFINE_TYPE_WITH_PRIVATE (VikWebtoolCenter, vik_webtool_center, VIK_WEBTOOL_TYPE)
#define WEBTOOL_CENTER_GET_PRIVATE(o) (vik_webtool_center_get_instance_private (VIK_WEBTOOL_CENTER(o)))

enum
{
  PROP_0,

  PROP_URL,
};

static void
webtool_center_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_URL:
      g_free (priv->url);
      priv->url = g_value_dup_string (value);
      g_debug ("VikWebtoolCenter.url: %s", priv->url);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
webtool_center_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE (object);

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
vik_webtool_center_class_init ( VikWebtoolCenterClass *klass )
{
  GObjectClass *gobject_class;
  VikWebtoolClass *base_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = webtool_center_finalize;
  gobject_class->set_property = webtool_center_set_property;
  gobject_class->get_property = webtool_center_get_property;

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
  base_class->get_url = webtool_center_get_url;
  base_class->get_url_at_position = webtool_center_get_url_at_position;

  klass->mpp_to_zoom = webtool_center_mpp_to_zoom;
}

VikWebtoolCenter *vik_webtool_center_new ()
{
  return VIK_WEBTOOL_CENTER ( g_object_new ( VIK_WEBTOOL_CENTER_TYPE, NULL ) );
}

VikWebtoolCenter *vik_webtool_center_new_with_members ( const gchar *label, const gchar *url )
{
  VikWebtoolCenter *result = VIK_WEBTOOL_CENTER ( g_object_new ( VIK_WEBTOOL_CENTER_TYPE,
                                                                 "label", label,
                                                                 "url", url,
                                                                 NULL ) );

  return result;
}

static void
vik_webtool_center_init ( VikWebtoolCenter *self )
{
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE (self);
  priv->url = NULL;
}

static void webtool_center_finalize ( GObject *gob )
{
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE ( gob );
  g_free ( priv->url ); priv->url = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static guint8 webtool_center_mpp_to_zoom ( VikWebtool *self, gdouble mpp ) {
  return map_utils_mpp_to_zoom_level ( mpp );
}

static gchar *webtool_center_get_url_at_position ( VikWebtool *self, VikWindow *vwindow, VikCoord *vc )
{
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE (self);
  VikViewport *viewport = vik_window_viewport ( vwindow );
  guint8 zoom = 17;
  struct LatLon ll;
  gchar strlat[COORDS_STR_BUFFER_SIZE], strlon[COORDS_STR_BUFFER_SIZE];

  // Coords
  // Use the provided position otherwise use center of the viewport
  if ( vc )
    vik_coord_to_latlon ( vc, &ll );
  else {
    const VikCoord *coord = NULL;
    coord = vik_viewport_get_center ( viewport );
    vik_coord_to_latlon ( coord, &ll );
  }

  // zoom - ideally x & y factors need to be the same otherwise use the default
  if ( vik_viewport_get_xmpp ( viewport ) == vik_viewport_get_ympp ( viewport ) )
    zoom = vik_webtool_center_mpp_to_zoom ( self, vik_viewport_get_zoom ( viewport ) );

  ll.lat = ROUND_TO_DECIMAL_PLACES ( ll.lat, 6 );
  ll.lon = ROUND_TO_DECIMAL_PLACES ( ll.lon, 6 );

  // Cannot simply use g_strdup_printf and gdouble due to locale.
  // As we compute an URL, we have to think in C locale.
  // Furthermore ensure decimal output (never scientific notation)
  a_coords_dtostr_buffer ( ll.lat, strlat );
  a_coords_dtostr_buffer ( ll.lon, strlon );

  return g_strdup_printf ( priv->url, strlat, strlon, zoom );
}

static gchar *webtool_center_get_url ( VikWebtool *self, VikWindow *vwindow )
{
  return webtool_center_get_url_at_position ( self, vwindow, NULL );
}

guint8 vik_webtool_center_mpp_to_zoom (VikWebtool *self, gdouble mpp)
{
  return VIK_WEBTOOL_CENTER_GET_CLASS( self )->mpp_to_zoom( self, mpp );
}
