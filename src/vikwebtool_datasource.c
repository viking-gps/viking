/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#include "vikwebtool_datasource.h"

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "globals.h"
#include "acquire.h"
#include "maputils.h"

static GObjectClass *parent_class;

static void webtool_datasource_finalize ( GObject *gob );

static gchar *webtool_datasource_get_url ( VikWebtool *self, VikWindow *vw );

typedef struct _VikWebtoolDatasourcePrivate VikWebtoolDatasourcePrivate;

struct _VikWebtoolDatasourcePrivate
{
	gchar *url;
	gchar *url_format_code;
	gchar *file_type;
};

#define WEBTOOL_DATASOURCE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                           VIK_WEBTOOL_DATASOURCE_TYPE,      \
                                           VikWebtoolDatasourcePrivate))

G_DEFINE_TYPE (VikWebtoolDatasource, vik_webtool_datasource, VIK_WEBTOOL_TYPE)

enum
{
	PROP_0,
	PROP_URL,
	PROP_URL_FORMAT_CODE,
	PROP_FILE_TYPE,
};

static void webtool_datasource_set_property (GObject      *object,
                                             guint         property_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
	VikWebtoolDatasource *self = VIK_WEBTOOL_DATASOURCE ( object );
	VikWebtoolDatasourcePrivate *priv = WEBTOOL_DATASOURCE_GET_PRIVATE ( self );

	switch ( property_id ) {
	case PROP_URL:
		g_free ( priv->url );
		priv->url = g_value_dup_string ( value );
		g_debug ( "VikWebtoolDatasource.url: %s", priv->url );
		break;

	case PROP_URL_FORMAT_CODE:
		g_free ( priv->url_format_code );
		priv->url_format_code = g_value_dup_string ( value );
		g_debug ( "VikWebtoolDatasource.url_format_code: %s", priv->url_format_code );
		break;

	case PROP_FILE_TYPE:
		g_free ( priv->file_type );
		priv->file_type = g_value_dup_string ( value );
		g_debug ( "VikWebtoolDatasource.file_type: %s", priv->url_format_code );
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, property_id, pspec );
		break;
    }
}

static void webtool_datasource_get_property (GObject    *object,
                                             guint       property_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
	VikWebtoolDatasource *self = VIK_WEBTOOL_DATASOURCE ( object );
	VikWebtoolDatasourcePrivate *priv = WEBTOOL_DATASOURCE_GET_PRIVATE ( self );

	switch ( property_id ) {

	case PROP_URL:              g_value_set_string ( value, priv->url ); break;
	case PROP_URL_FORMAT_CODE:	g_value_set_string ( value, priv->url_format_code ); break;
	case PROP_FILE_TYPE:        g_value_set_string ( value, priv->url ); break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, property_id, pspec );
		break;
    }
}

typedef struct {
	VikExtTool *self;
	VikWindow *vw;
	VikViewport *vvp;
} datasource_t;

static gpointer datasource_init ( acq_vik_t *avt )
{
	datasource_t *data = g_malloc(sizeof(*data));
	data->self = avt->userdata;
	data->vw = avt->vw;
	data->vvp = avt->vvp;
	return data;
}

static void datasource_get_cmd_string ( gpointer user_data, gchar **cmd, gchar **extra, DownloadMapOptions *options )
{
	datasource_t *data = (datasource_t*) user_data;

	VikWebtool *vwd = VIK_WEBTOOL ( data->self );
	gchar *url = vik_webtool_get_url ( vwd, data->vw );
	g_debug ("%s: %s", __FUNCTION__, url );

	*cmd = g_strdup ( url );

	// Only use first section of the file_type string
	// One can't use values like 'kml -x transform,rte=wpt' in order to do fancy things
	//  since it won't be in the right order for the overall GPSBabel command
	// So prevent any potentially dangerous behaviour
	VikWebtoolDatasourcePrivate *priv = WEBTOOL_DATASOURCE_GET_PRIVATE ( data->self );
	gchar **parts = NULL;
	if ( priv->file_type )
		parts = g_strsplit ( priv->file_type, " ", 0);
	if ( parts )
		*extra = g_strdup ( parts[0] );
	else
		*extra = NULL;
	g_strfreev ( parts );

	options = NULL;
}

static gboolean datasource_process ( VikTrwLayer *vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw, DownloadMapOptions *options )
{
	//datasource_t *data = (datasource_t *)adw->user_data;
	// Dependent on the ExtTool / what extra has been set to...
	// When extra is NULL - then it interprets results as a GPX
	gboolean result = a_babel_convert_from_url ( vtl, cmd, extra, status_cb, adw, options);
	return result;
}

static void cleanup ( gpointer data )
{
	g_free ( data );
}

static void webtool_datasource_open ( VikExtTool *self, VikWindow *vw )
{
	// Use VikDataSourceInterface to give thready goodness controls of downloading stuff (i.e. can cancel the request)

	// Can now create a 'VikDataSourceInterface' on the fly...
	VikDataSourceInterface *vik_datasource_interface = g_malloc(sizeof(VikDataSourceInterface));

	// An 'easy' way of assigning values
	VikDataSourceInterface data = {
		vik_ext_tool_get_label (self),
		vik_ext_tool_get_label (self),
		VIK_DATASOURCE_ADDTOLAYER,
		VIK_DATASOURCE_INPUTTYPE_NONE,
		FALSE, // Maintain current view - rather than setting it to the acquired points
		TRUE,
		TRUE,
		(VikDataSourceInitFunc)               datasource_init,
		(VikDataSourceCheckExistenceFunc)     NULL,
		(VikDataSourceAddSetupWidgetsFunc)    NULL,
		(VikDataSourceGetCmdStringFunc)       datasource_get_cmd_string,
		(VikDataSourceProcessFunc)            datasource_process,
		(VikDataSourceProgressFunc)           NULL,
		(VikDataSourceAddProgressWidgetsFunc) NULL,
		(VikDataSourceCleanupFunc)            cleanup,
		(VikDataSourceOffFunc)                NULL,
		NULL,
		0,
		NULL,
		NULL,
		0
	};
	memcpy ( vik_datasource_interface, &data, sizeof(VikDataSourceInterface) );

	a_acquire ( vw, vik_window_layers_panel(vw), vik_window_viewport (vw), vik_datasource_interface, self, cleanup );
}

static void vik_webtool_datasource_class_init ( VikWebtoolDatasourceClass *klass )
{
	GObjectClass *gobject_class;
	VikWebtoolClass *base_class;
	GParamSpec *pspec;

	gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = webtool_datasource_finalize;
	gobject_class->set_property = webtool_datasource_set_property;
	gobject_class->get_property = webtool_datasource_get_property;

	pspec = g_param_spec_string ("url",
	                             "Template URL",
	                             "Set the template URL",
	                             VIKING_URL /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
	                                 PROP_URL,
	                                 pspec);

	pspec = g_param_spec_string ("url_format_code",
	                             "Template URL Format Code",
	                             "Set the template URL format code",
	                             "LRBT", // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
	                                 PROP_URL_FORMAT_CODE,
	                                 pspec);

	pspec = g_param_spec_string ("file_type",
	                             "The file type expected",
	                             "Set the file type",
	                             NULL, // default value ~ equates to internal GPX reading
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (gobject_class,
	                                 PROP_FILE_TYPE,
	                                 pspec);

	parent_class = g_type_class_peek_parent (klass);

	base_class = VIK_WEBTOOL_CLASS ( klass );
	base_class->get_url = webtool_datasource_get_url;

	// Override default open function here:
	VikExtToolClass *ext_tool_class = VIK_EXT_TOOL_CLASS ( klass );
	ext_tool_class->open = webtool_datasource_open;

	g_type_class_add_private (klass, sizeof (VikWebtoolDatasourcePrivate));
}

VikWebtoolDatasource *vik_webtool_datasource_new ()
{
	return VIK_WEBTOOL_DATASOURCE ( g_object_new ( VIK_WEBTOOL_DATASOURCE_TYPE, NULL ) );
}

VikWebtoolDatasource *vik_webtool_datasource_new_with_members ( const gchar *label,
                                                                const gchar *url,
                                                                const gchar *url_format_code,
                                                                const gchar *file_type )
{
	VikWebtoolDatasource *result = VIK_WEBTOOL_DATASOURCE ( g_object_new ( VIK_WEBTOOL_DATASOURCE_TYPE,
	                                                        "label", label,
	                                                        "url", url,
	                                                        "url_format_code", url_format_code,
	                                                        "file_type", file_type,
	                                                        NULL ) );

	return result;
}

static void vik_webtool_datasource_init ( VikWebtoolDatasource *self )
{
	VikWebtoolDatasourcePrivate *priv = WEBTOOL_DATASOURCE_GET_PRIVATE (self);
	priv->url = NULL;
	priv->url_format_code = NULL;
	priv->file_type = NULL;
}

static void webtool_datasource_finalize ( GObject *gob )
{
	VikWebtoolDatasourcePrivate *priv = WEBTOOL_DATASOURCE_GET_PRIVATE ( gob );
	g_free ( priv->url ); priv->url = NULL;
	g_free ( priv->url_format_code ); priv->url_format_code = NULL;
	g_free ( priv->file_type ); priv->file_type = NULL;
	G_OBJECT_CLASS(parent_class)->finalize(gob);
}

#define MAX_NUMBER_CODES 7

/**
 * Calculate individual elements (similarly to the VikWebtool Bounds & Center) for *all* potential values
 * Then only values specified by the URL format are used in parameterizing the URL
 */
static gchar *webtool_datasource_get_url ( VikWebtool *self, VikWindow *vw )
{
	VikWebtoolDatasourcePrivate *priv = WEBTOOL_DATASOURCE_GET_PRIVATE ( self );
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
		default: break;
		}
	}

	gchar *url = g_strdup_printf ( priv->url, values[0], values[1], values[2], values[3], values[4], values[5], values[6] );

	for ( i = 0; i < MAX_NUMBER_CODES; i++ ) {
		if ( values[i] != '\0' )
			g_free ( values[i] );
	}
	
	return url;
}
