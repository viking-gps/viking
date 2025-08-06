/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2015, Rob Norris <rw_norris@hotmail.com>
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
#include "viking.h"
#include "vikmaplibrelayer.h"
#include <ctype.h>

#include "map_ids.h"
#include "maputils.h"
#include "mapcoord.h"
#include "mapcache.h"
#include "dir.h"
#include "maplibre_interface.h"
#include "background.h"

#include "vikmapslayer.h"

#define MAPLIBRE_FIXED_NAME "Maplibre Rendering"
#define MAPLIBRE_TILE_SIZE 256

struct _VikMaplibreLayerClass
{
	VikLayerClass object_class;
};

static VikLayerParamData file_default ( void )
{
	VikLayerParamData data;
	data.s = "";
	return data;
}

static VikLayerParamData cache_default ( void )
{
	VikLayerParamData data;
	data.s = "tile_cache.sqlite";
	return data;
}

static VikLayerParamData api_key_default ( void )
{
	VikLayerParamData data;
	data.s = "";
	return data;
}

static VikLayerParamData alpha_default ( void ) { return VIK_LPD_UINT ( 255 ); }

static VikLayerParamScale scales[] = {
	{ 0, 255, 5, 0 }, // Alpha
};

static void reset_cb ( GtkWidget *widget, gpointer ptr )
{
    // TODO: reset cache
	a_layer_defaults_reset_show ( MAPLIBRE_FIXED_NAME, ptr, VIK_LAYER_GROUP_NONE );
}

static VikLayerParamData reset_default ( void ) { return VIK_LPD_PTR(reset_cb); }

VikLayerParam maplibre_layer_params[] = {
  { VIK_LAYER_MAPLIBRE, "style-file", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("JSON Style File:"), VIK_LAYER_WIDGET_FILEENTRY, GINT_TO_POINTER(VF_FILTER_JSON), NULL,
    N_("Maplibre JSON style file"), file_default, NULL, NULL },
  { VIK_LAYER_MAPLIBRE, "cache-file", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Tile Cache File:"), VIK_LAYER_WIDGET_FILEENTRY, GINT_TO_POINTER(VF_FILTER_SQLITE), NULL,
    N_("Maplibre tile cache file (sqlite3)"), cache_default, NULL, NULL },
  { VIK_LAYER_MAPLIBRE, "api-key", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("API Key (if needed):"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL,
    N_("API Key if needed by Maplibre"), api_key_default, NULL, NULL },
  { VIK_LAYER_MAPLIBRE, "alpha", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Alpha:"), VIK_LAYER_WIDGET_HSCALE, &scales[0], NULL,
    NULL, alpha_default, NULL, NULL }
};

enum {
  PARAM_STYLE_JSON,
  PARAM_CACHE,
  PARAM_API_KEY,
  PARAM_ALPHA,
  NUM_PARAMS };

static const gchar* maplibre_layer_tooltip ( VikMaplibreLayer *vml );
static void maplibre_layer_marshall( VikMaplibreLayer *vml, guint8 **data, guint *len );
static VikMaplibreLayer *maplibre_layer_unmarshall( guint8 *data, guint len, VikViewport *vvp );
static gboolean maplibre_layer_set_param ( VikMaplibreLayer *vml, VikLayerSetParam *vlsp );
static VikLayerParamData maplibre_layer_get_param ( VikMaplibreLayer *vml, guint16 id, gboolean is_file_operation );
static VikMaplibreLayer *maplibre_layer_new ( VikViewport *vvp );
static VikMaplibreLayer *maplibre_layer_create ( VikViewport *vp );
static void maplibre_layer_free ( VikMaplibreLayer *vml );
static void maplibre_layer_draw ( VikMaplibreLayer *vml, VikViewport *vp );
static void maplibre_layer_add_menu_items ( VikMaplibreLayer *vml, GtkMenu *menu, gpointer vlp, VikStdLayerMenuItem selection );

static gpointer maplibre_feature_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static VikLayerToolFuncStatus maplibre_feature_release ( VikMaplibreLayer *vml, GdkEventButton *event, VikViewport *vvp );

// See comment in viktrwlayer.c for advice on values used
// FUTURE:
static VikToolInterface maplibre_tools[] = {
	// Layer Info
	// Zoom All?
  { NULL,
    { "MaplibreFeatures", GTK_STOCK_INFO, N_("_Maplibre Features"), NULL, N_("Maplibre Features"), 0 },
    (VikToolConstructorFunc) maplibre_feature_create,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    (VikToolMouseFunc) maplibre_feature_release,
    NULL,
    NULL,
    FALSE,
    GDK_LEFT_PTR, NULL, NULL },
};

static void maplibre_layer_post_read (VikLayer *vl, VikViewport *vvp, gboolean from_file);

VikLayerInterface vik_maplibre_layer_interface = {
	MAPLIBRE_FIXED_NAME,
	N_("Maplibre Rendering"),
	NULL,
	"vikmaplibrelayer", // Icon name

	maplibre_tools,
	G_N_ELEMENTS(maplibre_tools),

	maplibre_layer_params,
	NUM_PARAMS,
	0,
	0,
	0,

	VIK_MENU_ITEM_ALL,

	(VikLayerFuncCreate)                  maplibre_layer_create,
	(VikLayerFuncGetNewName)              NULL,
	(VikLayerFuncRealize)                 NULL,
	(VikLayerFuncPostRead)                maplibre_layer_post_read,
	(VikLayerFuncFree)                    maplibre_layer_free,

	(VikLayerFuncProperties)              NULL,
	(VikLayerFuncDraw)                    maplibre_layer_draw,
	(VikLayerFuncConfigure)               NULL,
	(VikLayerFuncChangeCoordMode)         NULL,

	(VikLayerFuncGetTimestamp)            NULL,

	(VikLayerFuncSetMenuItemsSelection)   NULL,
	(VikLayerFuncGetMenuItemsSelection)   NULL,

	(VikLayerFuncAddMenuItems)            maplibre_layer_add_menu_items,
	(VikLayerFuncSublayerAddMenuItems)    NULL,

	(VikLayerFuncSublayerRenameRequest)   NULL,
	(VikLayerFuncSublayerToggleVisible)   NULL,
	(VikLayerFuncSublayerTooltip)         NULL,
	(VikLayerFuncLayerTooltip)            maplibre_layer_tooltip,
	(VikLayerFuncLayerSelected)           NULL,
	(VikLayerFuncLayerToggleVisible)      NULL,

	(VikLayerFuncMarshall)                maplibre_layer_marshall,
	(VikLayerFuncUnmarshall)              maplibre_layer_unmarshall,

	(VikLayerFuncSetParam)                maplibre_layer_set_param,
	(VikLayerFuncGetParam)                maplibre_layer_get_param,
	(VikLayerFuncChangeParam)             NULL,

	(VikLayerFuncReadFileData)            NULL,
	(VikLayerFuncWriteFileData)           NULL,

	(VikLayerFuncDeleteItem)              NULL,
	(VikLayerFuncCutItem)                 NULL,
	(VikLayerFuncCopyItem)                NULL,
	(VikLayerFuncPasteItem)               NULL,
	(VikLayerFuncFreeCopiedItem)          NULL,
	(VikLayerFuncDragDropRequest)         NULL,

	(VikLayerFuncSelectClick)             NULL,
	(VikLayerFuncSelectMove)              NULL,
	(VikLayerFuncSelectRelease)           NULL,
	(VikLayerFuncSelectedViewportMenu)    NULL,

	(VikLayerFuncRefresh)                 NULL,
};

struct _VikMaplibreLayer {
	VikLayer vl;
	gchar *filename_json;
	gchar *cache_file;
	gchar *api_key;
	guint8 alpha;

	gboolean loaded;
	MaplibreInterface* mi;
	guint rerender_timeout;

	VikCoord rerender_ul;
	VikCoord rerender_br;
	gdouble rerender_zoom;
	GtkWidget *right_click_menu;
};

#define MAPLIBRE_PREFS_GROUP_KEY "maplibre"
#define MAPLIBRE_PREFS_NAMESPACE "maplibre."

static VikLayerParamData rr_to_default ( void ) { return VIK_LPD_UINT(168); } // One week in hours

static VikLayerParam prefs[] = {
};

static GMutex *tp_mutex;
static GHashTable *requests = NULL;

static GdkColor black_color;

/**
 * vik_maplibre_layer_init:
 *
 * Just initialize preferences
 */
void vik_maplibre_layer_init (void)
{
	a_preferences_register_group ( MAPLIBRE_PREFS_GROUP_KEY, _("Maplibre") );

	for ( guint ii = 0; ii < G_N_ELEMENTS(prefs); ii++ )
	  a_preferences_register ( &prefs[ii], (VikLayerParamData){0}, MAPLIBRE_PREFS_GROUP_KEY );

		// TODO: fix deprecation
        gdk_color_parse ( "#000000", &black_color );
}

/**
 * vik_maplibre_layer_post_init:
 *
 * Initialize data structures
 */
void vik_maplibre_layer_post_init (void)
{
	tp_mutex = vik_mutex_new();

	// Just storing keys only
	requests = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );
}

void vik_maplibre_layer_uninit ()
{
	vik_mutex_free (tp_mutex);
	g_hash_table_destroy ( requests );
}

// NB Only performed once per program run
static void maplibre_layer_class_init ( VikMaplibreLayerClass *klass )
{
	// TODO?
}

GType vik_maplibre_layer_get_type ()
{
	static GType vml_type = 0;

	if (!vml_type) {
		static const GTypeInfo vml_info = {
			sizeof (VikMaplibreLayerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) maplibre_layer_class_init, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikMaplibreLayer),
			0,
			NULL /* instance init */
		};
		vml_type = g_type_register_static ( VIK_LAYER_TYPE, "VikMaplibreLayer", &vml_info, 0 );
	}

	return vml_type;
}

static const gchar* maplibre_layer_tooltip ( VikMaplibreLayer *vml )
{
	// TODO: this is the tooltip?
	return vml->filename_json;
}

static void maplibre_layer_set_file_json ( VikMaplibreLayer *vml, const gchar *name )
{
	if ( vml->filename_json )
		g_free (vml->filename_json);
	vml->filename_json = g_strdup (name);
}

static void maplibre_layer_set_cache_file ( VikMaplibreLayer *vml, const gchar *name )
{
	if ( vml->cache_file )
		g_free (vml->cache_file);
	vml->cache_file = g_strdup (name);
}

static void maplibre_layer_set_api_key ( VikMaplibreLayer *vml, const gchar *key )
{
	if ( vml->api_key )
		g_free (vml->api_key );
	vml->api_key = g_strdup (key);
}
static void maplibre_layer_marshall( VikMaplibreLayer *vml, guint8 **data, guint *len )
{
	vik_layer_marshall_params ( VIK_LAYER(vml), data, len );
}

static VikMaplibreLayer *maplibre_layer_unmarshall( guint8 *data, guint len, VikViewport *vvp )
{
	VikMaplibreLayer *rv = maplibre_layer_new ( vvp );
	vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
	return rv;
}

static gboolean maplibre_layer_set_param ( VikMaplibreLayer *vml, VikLayerSetParam *vlsp )
{
	gboolean changed = FALSE;
	switch ( vlsp->id ) {
		case PARAM_STYLE_JSON: {
			gchar* old = g_strdup ( vml->filename_json );
			maplibre_layer_set_file_json ( vml, vlsp->data.s );
			changed = g_strcmp0 ( old, vlsp->data.s );
			g_free ( old );
			break;
		}
		case PARAM_CACHE: {
			gchar* old = g_strdup ( vml->cache_file );
			maplibre_layer_set_cache_file ( vml, vlsp->data.s );
			changed = g_strcmp0 ( old, vlsp->data.s );
			g_free ( old );
			break;
		}
		case PARAM_API_KEY: {
			gchar* old = g_strdup ( vml->api_key );
			maplibre_layer_set_api_key ( vml, vlsp->data.s );
			changed = g_strcmp0 ( old, vlsp->data.s );
			g_free ( old );
			break;
		}
		case PARAM_ALPHA:
			if ( vlsp->data.u <= 255 )
				changed = vik_layer_param_change_uint8 ( vlsp->data, &vml->alpha );
			break;
		default: break;
	}
	if ( vik_debug && changed )
		g_debug ( "%s: Detected change on param %d", __FUNCTION__, vlsp->id );
	return TRUE;
}

static VikLayerParamData maplibre_layer_get_param ( VikMaplibreLayer *vml, guint16 id, gboolean is_file_operation )
{
	VikLayerParamData data;
	switch ( id ) {
		case PARAM_STYLE_JSON: {
			data.s = vml->filename_json;
			gboolean set = FALSE;
			if ( is_file_operation ) {
				if ( a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE ) {
					gchar *cwd = g_get_current_dir();
					if ( cwd ) {
						data.s = file_GetRelativeFilename ( cwd, vml->filename_json );
						if ( !data.s ) data.s = "";
						set = TRUE;
					}
				}
			}
			if ( !set )
				data.s = vml->filename_json ? vml->filename_json : "";
			break;
		}
		case PARAM_CACHE: {
			data.s = vml->cache_file;
			gboolean set = FALSE;
			if ( is_file_operation ) {
				if ( a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE ) {
					gchar *cwd = g_get_current_dir();
					if ( cwd ) {
						data.s = file_GetRelativeFilename ( cwd, vml->cache_file );
						if ( !data.s ) data.s = "";
						set = TRUE;
					}
				}
			}
			if ( !set )
				data.s = vml->cache_file ? vml->cache_file : "";
			break;
		}
		case PARAM_API_KEY: {
			data.s = vml->api_key ? vml->api_key : "";;
			break;
		}
		case PARAM_ALPHA: data.u = vml->alpha; break;
		default: break;
	}
	return data;
}

/**
 *
 */
static VikMaplibreLayer *maplibre_layer_new ( VikViewport *vvp )
{
	VikMaplibreLayer *vml = VIK_MAPLIBRE_LAYER ( g_object_new ( VIK_MAPLIBRE_LAYER_TYPE, NULL ) );
	vik_layer_set_type ( VIK_LAYER(vml), VIK_LAYER_MAPLIBRE );
	vik_layer_set_defaults ( VIK_LAYER(vml), vvp );
	vml->loaded = FALSE;
	return vml;
}

/**
 *
 */
static void maplibre_layer_post_read (VikLayer *vl, VikViewport *vvp, gboolean from_file)
{
	VikMaplibreLayer *vml = VIK_MAPLIBRE_LAYER(vl);

	vml->mi = maplibre_interface_new( MAPLIBRE_TILE_SIZE, MAPLIBRE_TILE_SIZE, vml->cache_file, vml->api_key );

	gchar* ans = maplibre_interface_load_style_file ( vml->mi, vml->filename_json );
	if ( ans ) {
		a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(vvp),
		                           _("Maplibre error loading configuration file:\n%s"),
		                           ans );
		g_free ( ans );
	}
	else {
		// TODO: interesting -- open json from menu?
		vml->loaded = TRUE;
		if ( !from_file )
			ui_add_recent_file ( vml->filename_json );
	}
}

typedef struct
{
	VikMaplibreLayer *vml;
	VikCoord *ul;
	VikCoord *br;
	MapCoord *ulmc;
	const gchar* request;
} RenderInfo;

/**
 * render:
 *
 * Common render function which can run in separate thread
 */
static void render ( VikMaplibreLayer *vml, VikCoord *ul, VikCoord *br, MapCoord *ulm )
{
	gint64 tt1 = g_get_real_time ();
	GdkPixbuf *pixbuf = maplibre_interface_render ( vml->mi, ul->north_south, ul->east_west, br->north_south, br->east_west );
	gint64 tt2 = g_get_real_time ();
	gdouble tt = (gdouble)(tt2-tt1)/1000000;
	g_debug ( "Maplibre rendering completed in %.3f seconds", tt );
	if ( !pixbuf ) {
		// A pixbuf to stick into cache incase of an unrenderable area - otherwise will get continually re-requested
		pixbuf = gdk_pixbuf_scale_simple ( ui_get_icon("vikmaplibrelayer", 16), MAPLIBRE_TILE_SIZE, MAPLIBRE_TILE_SIZE, GDK_INTERP_BILINEAR );
	}

	if ( vml->alpha < 255 )
		pixbuf = ui_pixbuf_scale_alpha ( pixbuf, vml->alpha );
	a_mapcache_add ( pixbuf, (mapcache_extra_t){ tt, 0 }, ulm->x, ulm->y, ulm->z, MAP_ID_MAPLIBRE_RENDER, ulm->scale, vml->alpha, 0.0, 0.0, vml->filename_json );
	g_object_unref(pixbuf);
}

static void render_info_free ( RenderInfo *data )
{
	g_free ( data->ul );
	g_free ( data->br );
	g_free ( data->ulmc );
	// NB No need to free the request/key - as this is freed by the hash table destructor
	g_free ( data );
}

static void background ( RenderInfo *data, gpointer threaddata )
{
	int res = a_background_thread_progress ( threaddata, 0 );
	if (res == 0) {
		render ( data->vml, data->ul, data->br, data->ulmc );
	}

	g_mutex_lock(tp_mutex);
	g_hash_table_remove (requests, data->request);
	g_mutex_unlock(tp_mutex);

	if (res == 0)
		vik_layer_emit_update ( VIK_LAYER(data->vml), FALSE ); // NB update display from background
}

static void render_cancel_cleanup (RenderInfo *data)
{
	// Anything?
}

#define REQUEST_HASHKEY_FORMAT "%d-%d-%d-%d-%d"

/**
 * Thread
 */
static void thread_add (VikMaplibreLayer *vml, MapCoord *mul, VikCoord *ul, VikCoord *br, gint x, gint y, gint z, gint zoom, const gchar* name )
{
	// Create request
	guint nn = name ? g_str_hash ( name ) : 0;
	gchar *request = g_strdup_printf ( REQUEST_HASHKEY_FORMAT, x, y, z, zoom, nn );

	g_mutex_lock(tp_mutex);

	if ( g_hash_table_lookup_extended (requests, request, NULL, NULL ) ) {
		g_free ( request );
		g_mutex_unlock (tp_mutex);
		return;
	}

	RenderInfo *ri = g_malloc ( sizeof(RenderInfo) );
	ri->vml = vml;
	ri->ul = g_malloc ( sizeof(VikCoord) );
	ri->br = g_malloc ( sizeof(VikCoord) );
	ri->ulmc = g_malloc ( sizeof(MapCoord) );
	memcpy(ri->ul, ul, sizeof(VikCoord));
	memcpy(ri->br, br, sizeof(VikCoord));
	memcpy(ri->ulmc, mul, sizeof(MapCoord));
	ri->request = request;

	g_hash_table_insert ( requests, request, NULL );

	g_mutex_unlock (tp_mutex);

	gchar *basename = g_path_get_basename (name);
	gchar *description = g_strdup_printf ( _("Maplibre Render %d:%d:%d %s"), zoom, x, y, basename );
	g_free ( basename );
	a_background_thread ( BACKGROUND_POOL_LOCAL_MAPLIBRE,
	                      VIK_GTK_WINDOW_FROM_LAYER(vml),
	                      description,
	                      (vik_thr_func) background,
	                      ri,
	                      (vik_thr_free_func) render_info_free,
	                      (vik_thr_free_func) render_cancel_cleanup,
	                      1 );
	g_free ( description );
}

/**
 * Caller has to decrease reference counter of returned
 * GdkPixbuf, when buffer is no longer needed.
 */
static GdkPixbuf *get_pixbuf ( VikMaplibreLayer *vml, MapCoord *ulm, MapCoord *brm )
{
	VikCoord ul; VikCoord br;
	GdkPixbuf *pixbuf = NULL;

	map_utils_iTMS_to_vikcoord (ulm, &ul);
	map_utils_iTMS_to_vikcoord (brm, &br);

	pixbuf = a_mapcache_get ( ulm->x, ulm->y, ulm->z, MAP_ID_MAPLIBRE_RENDER, ulm->scale, vml->alpha, 0.0, 0.0, vml->filename_json );

	if ( ! pixbuf ) {
		thread_add (vml, ulm, &ul, &br, ulm->x, ulm->y, ulm->z, ulm->scale, vml->filename_json );
	}

	return pixbuf;
}

/**
 *
 */
static void maplibre_layer_draw ( VikMaplibreLayer *vml, VikViewport *vvp )
{
	if ( !vml->loaded )
		return;

	if ( vik_viewport_get_drawmode(vvp) != VIK_VIEWPORT_DRAWMODE_MERCATOR ) {
		vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vml))),
		                            VIK_STATUSBAR_INFO, _("Maplibre Rendering must be in Mercator mode") );
		return;
	}

	if ( vml->mi ) {
		gchar *copyright = maplibre_interface_get_copyright ( vml->mi );
		if ( copyright ) {
			vik_viewport_add_copyright ( vvp, copyright );
		}
	}

	VikCoord ul, br;
	ul.mode = VIK_COORD_LATLON;
	br.mode = VIK_COORD_LATLON;
	vik_viewport_screen_to_coord ( vvp, 0, 0, &ul );
	vik_viewport_screen_to_coord ( vvp, vik_viewport_get_width(vvp), vik_viewport_get_height(vvp), &br );

	gdouble xzoom = vik_viewport_get_xmpp ( vvp );
	gdouble yzoom = vik_viewport_get_ympp ( vvp );

	MapCoord ulm, brm;

	if ( map_utils_vikcoord_to_iTMS ( &ul, xzoom, yzoom, &ulm ) &&
	     map_utils_vikcoord_to_iTMS ( &br, xzoom, yzoom, &brm ) ) {
		// TODO: Understand if tilesize != 256 does this need to use shrinkfactors??
		GdkPixbuf *pixbuf;
		VikCoord coord;
		gint xx, yy;

		gint xmin = MIN(ulm.x, brm.x), xmax = MAX(ulm.x, brm.x);
		gint ymin = MIN(ulm.y, brm.y), ymax = MAX(ulm.y, brm.y);

		// Split rendering into a grid for the current viewport
		//  thus each individual 'tile' can then be stored in the map cache
		for (gint x = xmin; x <= xmax; x++ ) {
			for (gint y = ymin; y <= ymax; y++ ) {
				ulm.x = x;
				ulm.y = y;
				brm.x = x+1;
				brm.y = y+1;

				pixbuf = get_pixbuf ( vml, &ulm, &brm );

				if ( pixbuf ) {
					map_utils_iTMS_to_vikcoord ( &ulm, &coord );
					vik_viewport_coord_to_screen ( vvp, &coord, &xx, &yy );
					vik_viewport_draw_pixbuf ( vvp, pixbuf, 0, 0, xx, yy, MAPLIBRE_TILE_SIZE, MAPLIBRE_TILE_SIZE );
					g_object_unref(pixbuf);
				}
			}
		}

		// Done after so drawn on top
		// Just a handy guide to tile blocks.
		if ( vik_debug && vik_verbose ) {
			GdkGC *black_gc = vik_viewport_get_black_gc ( vvp );
			gint width = vik_viewport_get_width(vvp);
			gint height = vik_viewport_get_height(vvp);
			gint xx, yy;
			ulm.x = xmin; ulm.y = ymin;
			map_utils_iTMS_to_center_vikcoord ( &ulm, &coord );
			vik_viewport_coord_to_screen ( vvp, &coord, &xx, &yy );
			xx = xx - (MAPLIBRE_TILE_SIZE/2);
			yy = yy - (MAPLIBRE_TILE_SIZE/2); // Yes use X ATM
			for (gint x = xmin; x <= xmax; x++ ) {
				vik_viewport_draw_line ( vvp, black_gc, xx, 0, xx, height, &black_color, 1 );
				xx += MAPLIBRE_TILE_SIZE;
			}
			for (gint y = ymin; y <= ymax; y++ ) {
				vik_viewport_draw_line ( vvp, black_gc, 0, yy, width, yy, &black_color, 1 );
				yy += MAPLIBRE_TILE_SIZE; // Yes use X ATM
			}
		}
	}
}

/**
 *
 */
static void maplibre_layer_free ( VikMaplibreLayer *vml )
{
	maplibre_interface_free ( vml->mi );
	if ( vml->filename_json )
		g_free ( vml->filename_json );
}

static VikMaplibreLayer *maplibre_layer_create ( VikViewport *vp )
{
	return maplibre_layer_new ( vp );
}

typedef enum {
	MA_VML = 0,
	MA_VVP,
	MA_LAST
} menu_array_index;

typedef gpointer menu_array_values[MA_LAST];

/**
 *
 */
static void maplibre_layer_flush_memory ( menu_array_values values )
{
	a_mapcache_flush_type ( MAP_ID_MAPLIBRE_RENDER );
}

/**
 *
 */
static void maplibre_layer_reload ( menu_array_values values )
{
	VikMaplibreLayer *vml = values[MA_VML];
	VikViewport *vvp = values[MA_VVP];
	maplibre_layer_post_read (VIK_LAYER(vml), vvp, FALSE);
	maplibre_layer_draw ( vml, vvp );
}

/**
 * Show Maplibre configuration parameters
 */
static void maplibre_layer_information ( menu_array_values values )
{
	VikMaplibreLayer *vml = values[MA_VML];
	if ( !vml->mi )
		return;
	GArray *array = maplibre_interface_get_parameters( vml->mi );
	if ( array->len ) {
		a_dialog_list (  VIK_GTK_WINDOW_FROM_LAYER(vml), _("Maplibre Information"), array, 1 );
		// Free the copied strings
		for ( int i = 0; i < array->len; i++ )
			g_free ( g_array_index(array,gchar*,i) );
	}
	g_array_free ( array, FALSE );
}

/**
 *
 */
static void maplibre_layer_about ( menu_array_values values )
{
	VikMaplibreLayer *vml = values[MA_VML];
	gchar *msg = maplibre_interface_about();
	a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vml),  msg );
	g_free ( msg );
}

/**
 *
 */
static void maplibre_layer_add_menu_items ( VikMaplibreLayer *vml, GtkMenu *menu, gpointer vlp, VikStdLayerMenuItem selection )
{
	static menu_array_values values;
	values[MA_VML] = vml;
	values[MA_VVP] = vik_layers_panel_get_viewport( VIK_LAYERS_PANEL(vlp) );

	(void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator

	// Typical users shouldn't need to use this functionality - so debug only ATM
	if ( vik_debug ) {
		(void)vu_menu_add_item ( menu, _("_Flush Memory Cache"), GTK_STOCK_REMOVE, G_CALLBACK(maplibre_layer_flush_memory), values );
	}

	(void)vu_menu_add_item ( menu, NULL, GTK_STOCK_REFRESH, G_CALLBACK(maplibre_layer_reload), values );

	(void)vu_menu_add_item ( menu, NULL, GTK_STOCK_INFO, G_CALLBACK(maplibre_layer_information), values );
	(void)vu_menu_add_item ( menu, NULL, GTK_STOCK_ABOUT, G_CALLBACK(maplibre_layer_about), values );
}

/**
 * Rerender a specific tile
 */
static void maplibre_layer_rerender ( VikMaplibreLayer *vml )
{
	MapCoord ulm;
	// Requested position to map coord
	map_utils_vikcoord_to_iTMS ( &vml->rerender_ul, vml->rerender_zoom, vml->rerender_zoom, &ulm );
	// Reconvert back - thus getting the coordinate at the tile *ul corner*
	map_utils_iTMS_to_vikcoord (&ulm, &vml->rerender_ul );
	// Bottom right bound is simply +1 in TMS coords
	MapCoord brm = ulm;
	brm.x = brm.x+1;
	brm.y = brm.y+1;
	map_utils_iTMS_to_vikcoord (&brm, &vml->rerender_br );
	thread_add (vml, &ulm, &vml->rerender_ul, &vml->rerender_br, ulm.x, ulm.y, ulm.z, ulm.scale, vml->filename_json );
}

/**
 * Info
 */
static void maplibre_layer_tile_info ( VikMaplibreLayer *vml )
{
	MapCoord ulm;
	// Requested position to map coord
	map_utils_vikcoord_to_iTMS ( &vml->rerender_ul, vml->rerender_zoom, vml->rerender_zoom, &ulm );

	mapcache_extra_t extra = a_mapcache_get_extra ( ulm.x, ulm.y, ulm.z, MAP_ID_MAPLIBRE_RENDER, ulm.scale, vml->alpha, 0.0, 0.0, vml->filename_json );

	gchar *filemsg = NULL;
	gchar *timemsg = NULL;

	GArray *array = g_array_new (FALSE, TRUE, sizeof(gchar*));
	g_array_append_val ( array, filemsg );
	g_array_append_val ( array, timemsg );

	gchar *rendmsg = NULL;
	// Show the info
	if ( extra.duration > 0.0 ) {
		rendmsg = g_strdup_printf ( _("Rendering time %.2f seconds"), extra.duration );
		g_array_append_val ( array, rendmsg );
	}

	a_dialog_list (  VIK_GTK_WINDOW_FROM_LAYER(vml), _("Tile Information"), array, 5 );
	g_array_free ( array, FALSE );

	g_free ( rendmsg );
	g_free ( timemsg );
	g_free ( filemsg );
}

static VikLayerToolFuncStatus maplibre_feature_release ( VikMaplibreLayer *vml, GdkEventButton *event, VikViewport *vvp )
{
	if ( !vml )
		return VIK_LAYER_TOOL_IGNORED;
	if ( event->button == 3 ) {
		vik_viewport_screen_to_coord ( vvp, MAX(0, event->x), MAX(0, event->y), &vml->rerender_ul );
		vml->rerender_zoom = vik_viewport_get_zoom ( vvp );

		if ( ! vml->right_click_menu ) {
			vml->right_click_menu = gtk_menu_new ();
			GtkMenu *menu = GTK_MENU(vml->right_click_menu); // local rename
			(void)vu_menu_add_item ( menu, _("_Rerender Tile"), GTK_STOCK_REFRESH, G_CALLBACK(maplibre_layer_rerender), vml );
			(void)vu_menu_add_item ( menu, _("_Info"), GTK_STOCK_INFO, G_CALLBACK(maplibre_layer_tile_info), vml );
		}

		gtk_menu_popup ( GTK_MENU(vml->right_click_menu), NULL, NULL, NULL, NULL, event->button, event->time );
		gtk_widget_show_all ( GTK_WIDGET(vml->right_click_menu) );
	}

	return VIK_LAYER_TOOL_IGNORED;
}
