/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Format Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Format Public License for more details.
 *
 * You should have received a copy of the GNU Format Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vikwebtoolformat.h"

#include <string.h>
#include <glib.h>

#include "util.h"
#include "globals.h"
#include "maputils.h"

static GObjectClass *parent_class;

static void webtool_format_finalize ( GObject *gob );

static guint8 webtool_format_mpp_to_zoom ( VikWebtool *self, gdouble mpp );
static gchar *webtool_format_get_url ( VikWebtool *vw, VikWindow *vwindow );
static gchar *webtool_format_get_url_at_position ( VikWebtool *vw, VikWindow *vwindow, VikCoord *vc );

typedef struct _VikWebtoolFormatPrivate VikWebtoolFormatPrivate;

struct _VikWebtoolFormatPrivate
{
	gchar *url;
	gchar *url_format_code;
};

#define WEBTOOL_FORMAT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                       VIK_WEBTOOL_FORMAT_TYPE,          \
                                       VikWebtoolFormatPrivate))

G_DEFINE_TYPE (VikWebtoolFormat, vik_webtool_format, VIK_WEBTOOL_TYPE)

enum
{
	PROP_0,

	PROP_URL,
	PROP_URL_FORMAT_CODE,
};

static void
webtool_format_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	VikWebtoolFormat *self = VIK_WEBTOOL_FORMAT (object);
	VikWebtoolFormatPrivate *priv = WEBTOOL_FORMAT_GET_PRIVATE (self);

	switch (property_id) {
	case PROP_URL:
		g_free (priv->url);
		priv->url = g_value_dup_string (value);
		g_debug ("VikWebtoolFormat.url: %s", priv->url);
		break;
	case PROP_URL_FORMAT_CODE:
		g_free ( priv->url_format_code );
		priv->url_format_code = g_value_dup_string ( value );
		g_debug ( "VikWebtoolFormat.url_format_code: %s", priv->url_format_code );
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
webtool_format_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	VikWebtoolFormat *self = VIK_WEBTOOL_FORMAT (object);
	VikWebtoolFormatPrivate *priv = WEBTOOL_FORMAT_GET_PRIVATE (self);

	switch (property_id) {
	case PROP_URL:
		g_value_set_string (value, priv->url);
		break;

	case PROP_URL_FORMAT_CODE:
		g_value_set_string ( value, priv->url_format_code );
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
vik_webtool_format_class_init ( VikWebtoolFormatClass *klass )
{
	GObjectClass *gobject_class;
	VikWebtoolClass *base_class;
	GParamSpec *pspec;

	gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = webtool_format_finalize;
	gobject_class->set_property = webtool_format_set_property;
	gobject_class->get_property = webtool_format_get_property;

	pspec = g_param_spec_string ("url",
	                             "Template Url",
	                             "Set the template url",
	                             VIKING_URL /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
	                                 PROP_URL,
	                                 pspec);

	pspec = g_param_spec_string ("url_format_code",
	                             "Template URL Format Code",
	                             "Set the template URL format code",
	                             "AOZ", // default value Lat, Long, Zoom
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
	                                 PROP_URL_FORMAT_CODE,
	                                 pspec);

	parent_class = g_type_class_peek_parent (klass);

	base_class = VIK_WEBTOOL_CLASS ( klass );
	base_class->get_url = webtool_format_get_url;
	base_class->get_url_at_position = webtool_format_get_url_at_position;

	klass->mpp_to_zoom = webtool_format_mpp_to_zoom;

	g_type_class_add_private (klass, sizeof (VikWebtoolFormatPrivate));
}

VikWebtoolFormat *vik_webtool_format_new ()
{
	return VIK_WEBTOOL_FORMAT ( g_object_new ( VIK_WEBTOOL_FORMAT_TYPE, NULL ) );
}

VikWebtoolFormat *vik_webtool_format_new_with_members ( const gchar *label,
                                                        const gchar *url,
                                                        const gchar *url_format_code )
{
	VikWebtoolFormat *result = VIK_WEBTOOL_FORMAT ( g_object_new ( VIK_WEBTOOL_FORMAT_TYPE,
	                                                               "label", label,
	                                                               "url", url,
	                                                               "url_format_code", url_format_code,
	                                                               NULL ) );

	return result;
}

static void
vik_webtool_format_init ( VikWebtoolFormat *self )
{
	VikWebtoolFormatPrivate *priv = WEBTOOL_FORMAT_GET_PRIVATE (self);
	priv->url = NULL;
	priv->url_format_code = NULL;
}

static void webtool_format_finalize ( GObject *gob )
{
	VikWebtoolFormatPrivate *priv = WEBTOOL_FORMAT_GET_PRIVATE ( gob );
	g_free ( priv->url ); priv->url = NULL;
	g_free ( priv->url_format_code ); priv->url_format_code = NULL;
	G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static guint8 webtool_format_mpp_to_zoom ( VikWebtool *self, gdouble mpp ) {
	return map_utils_mpp_to_zoom_level ( mpp );
}

#define MAX_NUMBER_CODES 9

static gchar *webtool_format_get_url_at_position ( VikWebtool *self, VikWindow *vw, VikCoord *vc )
{
	VikWebtoolFormatPrivate *priv = NULL;
	priv = WEBTOOL_FORMAT_GET_PRIVATE (self);

	VikViewport *viewport = vik_window_viewport ( vw );

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

	// Center values
	const VikCoord *coord = vik_viewport_get_center ( viewport );
	struct LatLon ll;
	vik_coord_to_latlon ( coord, &ll );

	gchar scenterlat[G_ASCII_DTOSTR_BUF_SIZE];
	gchar scenterlon[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr (scenterlat, G_ASCII_DTOSTR_BUF_SIZE, ll.lat);
	g_ascii_dtostr (scenterlon, G_ASCII_DTOSTR_BUF_SIZE, ll.lon);

	struct LatLon llpt;
	llpt.lat = 0.0;
	llpt.lon = 0.0;
	if ( vc )
	  vik_coord_to_latlon ( vc, &ll );
	gchar spointlat[G_ASCII_DTOSTR_BUF_SIZE];
	gchar spointlon[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr (spointlat, G_ASCII_DTOSTR_BUF_SIZE, llpt.lat);
	g_ascii_dtostr (spointlon, G_ASCII_DTOSTR_BUF_SIZE, llpt.lon);

	guint8 zoom = 17; // A zoomed in default
	// zoom - ideally x & y factors need to be the same otherwise use the default
	if ( vik_viewport_get_xmpp ( viewport ) == vik_viewport_get_ympp ( viewport ) )
		zoom = map_utils_mpp_to_zoom_level ( vik_viewport_get_zoom ( viewport ) );

	gchar szoom[G_ASCII_DTOSTR_BUF_SIZE];
	g_snprintf ( szoom, G_ASCII_DTOSTR_BUF_SIZE, "%d", zoom );

	gint len = 0;
	if ( priv->url_format_code )
		len = strlen ( priv->url_format_code );
	if ( len > MAX_NUMBER_CODES )
		len = MAX_NUMBER_CODES;

	gchar* values[MAX_NUMBER_CODES];
	int i;
	for ( i = 0; i < MAX_NUMBER_CODES; i++ ) {
		values[i] = '\0';
	}

	for ( i = 0; i < len; i++ ) {
		switch ( g_ascii_toupper ( priv->url_format_code[i] ) ) {
		case 'L': values[i] = g_strdup ( sminlon ); break;
		case 'R': values[i] = g_strdup ( smaxlon ); break;
		case 'B': values[i] = g_strdup ( sminlat ); break;
		case 'T': values[i] = g_strdup ( smaxlat ); break;
		case 'A': values[i] = g_strdup ( scenterlat ); break;
		case 'O': values[i] = g_strdup ( scenterlon ); break;
		case 'Z': values[i] = g_strdup ( szoom ); break;
		case 'P': values[i] = g_strdup ( spointlat ); break;
		case 'N': values[i] = g_strdup ( spointlon ); break;
		default: break;
		}
	}

	gchar *url = g_strdup_printf ( priv->url, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8] );

	for ( i = 0; i < MAX_NUMBER_CODES; i++ ) {
		if ( values[i] != '\0' )
			g_free ( values[i] );
	}

	g_debug ("%s %s", __FUNCTION__, url);
	return url;
}

static gchar *webtool_format_get_url ( VikWebtool *self, VikWindow *vw )
{
  return webtool_format_get_url_at_position ( self, vw, NULL );
}

guint8 vik_webtool_format_mpp_to_zoom (VikWebtool *self, gdouble mpp)
{
	return VIK_WEBTOOL_FORMAT_GET_CLASS( self )->mpp_to_zoom( self, mpp );
}
