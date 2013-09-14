/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

/**
 * SECTION:vikroutingwebengine
 * @short_description: A generic class for WEB based routing engine
 * 
 * The #VikRoutingWebEngine class handles WEB based
 * routing engine.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "babel.h"

#include "vikroutingwebengine.h"

static void vik_routing_web_engine_finalize ( GObject *gob );

static int vik_routing_web_engine_find ( VikRoutingEngine *self, VikTrwLayer *vtl, struct LatLon start, struct LatLon end );
static gchar *vik_routing_web_engine_get_cmd_from_directions(VikRoutingEngine *self, const gchar *start, const gchar *end);
static gboolean vik_routing_web_engine_supports_direction(VikRoutingEngine *self);
static int vik_routing_web_engine_refine ( VikRoutingEngine *self, VikTrwLayer *vtl, VikTrack *vt );
static gboolean vik_routing_web_engine_supports_refine ( VikRoutingEngine *self );

typedef struct _VikRoutingWebEnginePrivate VikRoutingWebEnginePrivate;
struct _VikRoutingWebEnginePrivate
{
	gchar *url_base;
	
	/* LatLon */
	gchar *url_start_ll_fmt;
	gchar *url_stop_ll_fmt;
	gchar *url_via_ll_fmt;

	/* Directions */
	gchar *url_start_dir_fmt;
	gchar *url_stop_dir_fmt;

	DownloadMapOptions options;
};

#define VIK_ROUTING_WEB_ENGINE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_ROUTING_WEB_ENGINE_TYPE, VikRoutingWebEnginePrivate))

/* properties */
enum
{
  PROP_0,

  PROP_URL_BASE,
  
  /* LatLon */
  PROP_URL_START_LL,
  PROP_URL_STOP_LL,
  PROP_URL_VIA_LL,

  /* Direction */
  PROP_URL_START_DIR,
  PROP_URL_STOP_DIR,

  PROP_REFERER,
  PROP_FOLLOW_LOCATION,
};

G_DEFINE_TYPE (VikRoutingWebEngine, vik_routing_web_engine, VIK_ROUTING_ENGINE_TYPE)

static void
vik_routing_web_engine_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( object );

  switch (property_id)
    {
    case PROP_URL_BASE:
      g_free (priv->url_base);
      priv->url_base = g_strdup(g_value_get_string (value));
      break;

    case PROP_URL_START_LL:
      g_free (priv->url_start_ll_fmt);
      priv->url_start_ll_fmt = g_strdup(g_value_get_string (value));
      break;

    case PROP_URL_STOP_LL:
      g_free (priv->url_stop_ll_fmt);
      priv->url_stop_ll_fmt = g_strdup(g_value_get_string (value));
      break;

    case PROP_URL_VIA_LL:
      g_free (priv->url_via_ll_fmt);
      priv->url_via_ll_fmt = g_strdup(g_value_get_string (value));
      break;

    case PROP_URL_START_DIR:
      g_free (priv->url_start_dir_fmt);
      priv->url_start_dir_fmt = g_strdup(g_value_get_string (value));
      break;

    case PROP_URL_STOP_DIR:
      g_free (priv->url_stop_dir_fmt);
      priv->url_stop_dir_fmt = g_strdup(g_value_get_string (value));
      break;

    case PROP_REFERER:
      g_free (priv->options.referer);
      priv->options.referer = g_value_dup_string (value);
      break;

    case PROP_FOLLOW_LOCATION:
      priv->options.follow_location = g_value_get_long (value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_routing_web_engine_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( object );

  switch (property_id)
    {
    case PROP_URL_BASE:
      g_value_set_string (value, priv->url_base);
      break;

    case PROP_URL_START_LL:
      g_value_set_string (value, priv->url_start_ll_fmt);
      break;

    case PROP_URL_STOP_LL:
      g_value_set_string (value, priv->url_stop_ll_fmt);
      break;

    case PROP_URL_VIA_LL:
      g_value_set_string (value, priv->url_via_ll_fmt);
      break;

    case PROP_URL_START_DIR:
      g_value_set_string (value, priv->url_start_dir_fmt);
      break;

    case PROP_URL_STOP_DIR:
      g_value_set_string (value, priv->url_stop_dir_fmt);
      break;

    case PROP_REFERER:
      g_value_set_string (value, priv->options.referer);
      break;

    case PROP_FOLLOW_LOCATION:
      g_value_set_long (value, priv->options.follow_location);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void vik_routing_web_engine_class_init ( VikRoutingWebEngineClass *klass )
{
  GObjectClass *object_class;
  VikRoutingEngineClass *parent_class;
  GParamSpec *pspec = NULL;

  object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = vik_routing_web_engine_set_property;
  object_class->get_property = vik_routing_web_engine_get_property;
  object_class->finalize = vik_routing_web_engine_finalize;

  parent_class = VIK_ROUTING_ENGINE_CLASS (klass);

  parent_class->find = vik_routing_web_engine_find;
  parent_class->supports_direction = vik_routing_web_engine_supports_direction;
  parent_class->get_cmd_from_directions = vik_routing_web_engine_get_cmd_from_directions;
  parent_class->refine = vik_routing_web_engine_refine;
  parent_class->supports_refine = vik_routing_web_engine_supports_refine;

  /**
   * VikRoutingWebEngine:url-base:
   *
   * The base URL of the routing engine.
   */
  pspec = g_param_spec_string ("url-base",
                               "URL's base",
                               "The base URL of the routing engine",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_BASE, pspec);
  

  /**
   * VikRoutingWebEngine:url-start-ll:
   *
   * The part of the request hosting the end point.
   */
  pspec = g_param_spec_string ("url-start-ll",
                               "Start part of the URL",
                               "The part of the request hosting the start point",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_START_LL, pspec);
    

  /**
   * VikRoutingWebEngine:url-stop-ll:
   *
   * The part of the request hosting the end point.
   */
  pspec = g_param_spec_string ("url-stop-ll",
                               "Stop part of the URL",
                               "The part of the request hosting the end point",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_STOP_LL, pspec);


  /**
   * VikRoutingWebEngine:url-via-ll:
   *
   * The param of the request for setting a via point.
   */
  pspec = g_param_spec_string ("url-via-ll",
                               "Via part of the URL",
                               "The param of the request for setting a via point",
                               NULL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_VIA_LL, pspec);


  /**
   * VikRoutingWebEngine:url-start-dir:
   *
   * The part of the request hosting the end point.
   */
  pspec = g_param_spec_string ("url-start-dir",
                               "Start part of the URL",
                               "The part of the request hosting the start point",
                               NULL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_START_DIR, pspec);
    

  /**
   * VikRoutingWebEngine:url-stop-dir:
   *
   * The part of the request hosting the end point.
   */
  pspec = g_param_spec_string ("url-stop-dir",
                               "Stop part of the URL",
                               "The part of the request hosting the end point",
                               NULL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_STOP_DIR, pspec);


  /**
   * VikRoutingWebEngine:referer:
   *
   * The REFERER string to use in HTTP request.
   */
  pspec = g_param_spec_string ("referer",
                               "Referer",
                               "The REFERER string to use in HTTP request",
                               NULL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_REFERER, pspec);


  /**
   * VikRoutingWebEngine:follow-location:
   *
   * Specifies the number of retries to follow a redirect while downloading a page.
   */
  pspec = g_param_spec_long ("follow-location",
                             "Follow location",
                             "Specifies the number of retries to follow a redirect while downloading a page",
                             0  /* minimum value */,
                             G_MAXLONG /* maximum value */,
                             2  /* default value */,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FOLLOW_LOCATION, pspec);

  g_type_class_add_private (klass, sizeof (VikRoutingWebEnginePrivate));
}

static void vik_routing_web_engine_init ( VikRoutingWebEngine *self )
{
  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( self );

  priv->url_base = NULL;
  
  /* LatLon */
  priv->url_start_ll_fmt = NULL;
  priv->url_stop_ll_fmt = NULL;
  priv->url_via_ll_fmt = NULL;

  /* Directions */
  priv->url_start_dir_fmt = NULL;
  priv->url_stop_dir_fmt = NULL;

  priv->options.referer = NULL;
  priv->options.follow_location = 0;
  priv->options.check_file = NULL;
  priv->options.check_file_server_time = FALSE;
  priv->options.use_etag = FALSE;
}

static void vik_routing_web_engine_finalize ( GObject *gob )
{
  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( gob );

  g_free (priv->url_base);
  priv->url_base = NULL;
  
  /* LatLon */
  g_free (priv->url_start_ll_fmt);
  priv->url_start_ll_fmt = NULL;
  g_free (priv->url_stop_ll_fmt);
  priv->url_stop_ll_fmt = NULL;
  g_free (priv->url_via_ll_fmt);
  priv->url_via_ll_fmt = NULL;

  /* Directions */
  g_free (priv->url_start_dir_fmt);
  priv->url_start_dir_fmt = NULL;
  g_free (priv->url_stop_dir_fmt);
  priv->url_stop_dir_fmt = NULL;

  g_free (priv->options.referer);
  priv->options.referer = NULL;

  G_OBJECT_CLASS (vik_routing_web_engine_parent_class)->finalize(gob);
}

static DownloadMapOptions *
vik_routing_web_engine_get_download_options ( VikRoutingEngine *self )
{
	g_return_val_if_fail (VIK_IS_ROUTING_WEB_ENGINE(self), NULL);
	
	VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE(self);
	
	return &(priv->options);
}

static gchar *
substitute_latlon ( const gchar *fmt, struct LatLon ll )
{
	gchar lat[G_ASCII_DTOSTR_BUF_SIZE], lon[G_ASCII_DTOSTR_BUF_SIZE];
	gchar *substituted = g_strdup_printf(fmt,
                          g_ascii_dtostr (lat, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) ll.lat),
                          g_ascii_dtostr (lon, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) ll.lon));
	return substituted;
}

static gchar *
vik_routing_web_engine_get_url_for_coords ( VikRoutingEngine *self, struct LatLon start, struct LatLon end )
{
	gchar *startURL;
	gchar *endURL;
	gchar *url;
	
	g_return_val_if_fail ( VIK_IS_ROUTING_WEB_ENGINE (self), NULL);

	VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( self );

	g_return_val_if_fail ( priv->url_base != NULL, NULL);
	g_return_val_if_fail ( priv->url_start_ll_fmt != NULL, NULL);
	g_return_val_if_fail ( priv->url_stop_ll_fmt != NULL, NULL);

	startURL = substitute_latlon ( priv->url_start_ll_fmt, start );
	endURL = substitute_latlon ( priv->url_stop_ll_fmt, end );
	url = g_strconcat ( priv->url_base, startURL, endURL, NULL );

	/* Free memory */
	g_free ( startURL );
	g_free ( endURL );

	return url;
}

static int
vik_routing_web_engine_find ( VikRoutingEngine *self, VikTrwLayer *vtl, struct LatLon start, struct LatLon end )
{
  gchar *uri;
  int ret = 0;  /* OK */

  uri = vik_routing_web_engine_get_url_for_coords(self, start, end);

  DownloadMapOptions *options = vik_routing_web_engine_get_download_options(self);
  
  gchar *format = vik_routing_engine_get_format ( self );
  a_babel_convert_from_url ( vtl, uri, format, NULL, NULL, options );

  g_free(uri);

  return ret;
}

static gchar *
vik_routing_web_engine_get_cmd_from_directions ( VikRoutingEngine *self, const gchar *start, const gchar *end )
{
  g_return_val_if_fail ( VIK_IS_ROUTING_WEB_ENGINE (self), NULL);

  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( self );

  g_return_val_if_fail ( priv->url_base != NULL, NULL);
  g_return_val_if_fail ( priv->url_start_dir_fmt != NULL, NULL);
  g_return_val_if_fail ( priv->url_stop_dir_fmt != NULL, NULL);

  gchar *from_quoted, *to_quoted;
  gchar **from_split, **to_split;
  from_quoted = g_shell_quote ( start );
  to_quoted = g_shell_quote ( end );

  from_split = g_strsplit( from_quoted, " ", 0);
  to_split = g_strsplit( to_quoted, " ", 0);

  from_quoted = g_strjoinv( "%20", from_split);
  to_quoted = g_strjoinv( "%20", to_split);

  gchar *url_fmt = g_strconcat ( priv->url_base, priv->url_start_dir_fmt, priv->url_stop_dir_fmt, NULL );
  gchar *url = g_strdup_printf ( url_fmt, from_quoted, to_quoted );

  g_free ( url_fmt );

  g_free(from_quoted);
  g_free(to_quoted);
  g_strfreev(from_split);
  g_strfreev(to_split);

  return url;
}

static gboolean
vik_routing_web_engine_supports_direction ( VikRoutingEngine *self )
{
  g_return_val_if_fail ( VIK_IS_ROUTING_WEB_ENGINE (self), FALSE);

  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( self );

  return (priv->url_start_dir_fmt) != NULL;
}

struct _append_ctx {
  VikRoutingWebEnginePrivate *priv;
  gchar **urlParts;
  int nb;
};

static void
_append_stringified_coords ( gpointer data, gpointer user_data )
{
  VikTrackpoint *vtp = (VikTrackpoint*)data;
  struct _append_ctx *ctx = (struct _append_ctx*)user_data;
  
  /* Stringify coordinate */
  struct LatLon position;
  vik_coord_to_latlon ( &(vtp->coord), &position );
  gchar *string = substitute_latlon ( ctx->priv->url_via_ll_fmt, position );
  
  /* Append */
  ctx->urlParts[ctx->nb] = string;
  ctx->nb++;
}

static gchar *
vik_routing_web_engine_get_url_for_track ( VikRoutingEngine *self, VikTrack *vt )
{
  gchar **urlParts;
  gchar *url;

  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( self );

  g_return_val_if_fail ( priv->url_base != NULL, NULL );
  g_return_val_if_fail ( priv->url_start_ll_fmt != NULL, NULL );
  g_return_val_if_fail ( priv->url_stop_ll_fmt != NULL, NULL );
  g_return_val_if_fail ( priv->url_via_ll_fmt != NULL, NULL );

  /* Init temporary storage */
  gsize len = 1 + g_list_length ( vt->trackpoints ) + 1; /* base + trackpoints + NULL */
  urlParts = g_malloc ( sizeof(gchar*)*len );
  urlParts[0] = g_strdup ( priv->url_base );
  urlParts[len-1] = NULL;

  struct _append_ctx ctx;
  ctx.priv = priv;
  ctx.urlParts = urlParts;
  ctx.nb = 1; /* First cell available, previous used for base URL */

  /* Append all trackpoints to URL */
  g_list_foreach ( vt->trackpoints, _append_stringified_coords, &ctx );

  /* Override first and last positions with associated formats */
  struct LatLon position;
  VikTrackpoint *vtp;
  g_free ( urlParts[1] );
  vtp = g_list_first ( vt->trackpoints )->data;
  vik_coord_to_latlon ( &(vtp->coord ), &position );
  urlParts[1] = substitute_latlon ( priv->url_start_ll_fmt, position );
  g_free ( urlParts[len-2] );
  vtp = g_list_last ( vt->trackpoints )->data;
  vik_coord_to_latlon ( &(vtp->coord), &position );
  urlParts[len-2] = substitute_latlon ( priv->url_stop_ll_fmt, position );

  /* Concat */
  url = g_strjoinv ( NULL, urlParts );
  g_debug ( "%s: %s", __FUNCTION__, url );

  /* Free */
  g_strfreev ( urlParts );

  return url;
}

static int
vik_routing_web_engine_refine ( VikRoutingEngine *self, VikTrwLayer *vtl, VikTrack *vt )
{
  gchar *uri;
  int ret = 0;  /* OK */

  /* Compute URL */
  uri = vik_routing_web_engine_get_url_for_track ( self, vt );

  /* Download data */
  DownloadMapOptions *options = vik_routing_web_engine_get_download_options ( self );

  /* Convert and insert data in model */
  gchar *format = vik_routing_engine_get_format ( self );
  a_babel_convert_from_url ( vtl, uri, format, NULL, NULL, options );

  g_free(uri);

  return ret;
}

static gboolean
vik_routing_web_engine_supports_refine ( VikRoutingEngine *self )
{
  g_return_val_if_fail ( VIK_IS_ROUTING_WEB_ENGINE (self), FALSE);

  VikRoutingWebEnginePrivate *priv = VIK_ROUTING_WEB_ENGINE_PRIVATE ( self );

  return priv->url_via_ll_fmt != NULL;
}
