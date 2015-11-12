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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viking.h"
#include "vikutils.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

#include "map_ids.h"
#include "maputils.h"
#include "mapcoord.h"
#include "mapcache.h"
#include "dir.h"
#include "util.h"
#include "ui_util.h"
#include "preferences.h"
#include "icons/icons.h"
#include "mapnik_interface.h"
#include "background.h"

#include "vikmapslayer.h"

#if !GLIB_CHECK_VERSION(2,26,0)
typedef struct stat GStatBuf;
#endif

struct _VikMapnikLayerClass
{
	VikLayerClass object_class;
};

static VikLayerParamData file_default ( void )
{
	VikLayerParamData data;
	data.s = "";
	return data;
}

static VikLayerParamData size_default ( void ) { return VIK_LPD_UINT ( 256 ); }
static VikLayerParamData alpha_default ( void ) { return VIK_LPD_UINT ( 255 ); }

static VikLayerParamData cache_dir_default ( void )
{
	VikLayerParamData data;
	data.s = g_strconcat ( maps_layer_default_dir(), "MapnikRendering", NULL );
	return data;
}

static VikLayerParamScale scales[] = {
	{ 0, 255, 5, 0 }, // Alpha
	{ 64, 1024, 8, 0 }, // Tile size
	{ 0, 1024, 12, 0 }, // Rerender timeout hours
};

VikLayerParam mapnik_layer_params[] = {
  { VIK_LAYER_MAPNIK, "config-file-mml", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("CSS (MML) Config File:"), VIK_LAYER_WIDGET_FILEENTRY, GINT_TO_POINTER(VF_FILTER_CARTO), NULL,
    N_("CartoCSS configuration file"), file_default, NULL, NULL },
  { VIK_LAYER_MAPNIK, "config-file-xml", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("XML Config File:"), VIK_LAYER_WIDGET_FILEENTRY, GINT_TO_POINTER(VF_FILTER_XML), NULL,
    N_("Mapnik XML configuration file"), file_default, NULL, NULL },
  { VIK_LAYER_MAPNIK, "alpha", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Alpha:"), VIK_LAYER_WIDGET_HSCALE, &scales[0], NULL,
    NULL, alpha_default, NULL, NULL },
  { VIK_LAYER_MAPNIK, "use-file-cache", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use File Cache:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_MAPNIK, "file-cache-dir", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("File Cache Directory:"), VIK_LAYER_WIDGET_FOLDERENTRY, NULL, NULL,
    NULL, cache_dir_default, NULL, NULL },
};

enum {
  PARAM_CONFIG_CSS = 0,
  PARAM_CONFIG_XML,
  PARAM_ALPHA,
  PARAM_USE_FILE_CACHE,
  PARAM_FILE_CACHE_DIR,
  NUM_PARAMS };

static const gchar* mapnik_layer_tooltip ( VikMapnikLayer *vml );
static void mapnik_layer_marshall( VikMapnikLayer *vml, guint8 **data, gint *len );
static VikMapnikLayer *mapnik_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean mapnik_layer_set_param ( VikMapnikLayer *vml, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation );
static VikLayerParamData mapnik_layer_get_param ( VikMapnikLayer *vml, guint16 id, gboolean is_file_operation );
static VikMapnikLayer *mapnik_layer_new ( VikViewport *vvp );
static VikMapnikLayer *mapnik_layer_create ( VikViewport *vp );
static void mapnik_layer_free ( VikMapnikLayer *vml );
static void mapnik_layer_draw ( VikMapnikLayer *vml, VikViewport *vp );
static void mapnik_layer_add_menu_items ( VikMapnikLayer *vml, GtkMenu *menu, gpointer vlp );

static gpointer mapnik_feature_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean mapnik_feature_release ( VikMapnikLayer *vml, GdkEventButton *event, VikViewport *vvp );

// See comment in viktrwlayer.c for advice on values used
// FUTURE:
static VikToolInterface mapnik_tools[] = {
	// Layer Info
	// Zoom All?
  { { "MapnikFeatures", GTK_STOCK_INFO, N_("_Mapnik Features"), NULL, N_("Mapnik Features"), 0 },
    (VikToolConstructorFunc) mapnik_feature_create,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    (VikToolMouseFunc) mapnik_feature_release,
    NULL,
    FALSE,
    GDK_LEFT_PTR, NULL, NULL },
};

static void mapnik_layer_post_read (VikLayer *vl, VikViewport *vvp, gboolean from_file);

VikLayerInterface vik_mapnik_layer_interface = {
	"Mapnik Rendering",
	N_("Mapnik Rendering"),
	NULL,
	&vikmapniklayer_pixbuf, // icon

	mapnik_tools,
	sizeof(mapnik_tools) / sizeof(VikToolInterface),

	mapnik_layer_params,
	NUM_PARAMS,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	(VikLayerFuncCreate)                  mapnik_layer_create,
	(VikLayerFuncRealize)                 NULL,
	(VikLayerFuncPostRead)                mapnik_layer_post_read,
	(VikLayerFuncFree)                    mapnik_layer_free,

	(VikLayerFuncProperties)              NULL,
	(VikLayerFuncDraw)                    mapnik_layer_draw,
	(VikLayerFuncChangeCoordMode)         NULL,

	(VikLayerFuncGetTimestamp)            NULL,

	(VikLayerFuncSetMenuItemsSelection)   NULL,
	(VikLayerFuncGetMenuItemsSelection)   NULL,

	(VikLayerFuncAddMenuItems)            mapnik_layer_add_menu_items,
	(VikLayerFuncSublayerAddMenuItems)    NULL,

	(VikLayerFuncSublayerRenameRequest)   NULL,
	(VikLayerFuncSublayerToggleVisible)   NULL,
	(VikLayerFuncSublayerTooltip)         NULL,
	(VikLayerFuncLayerTooltip)            mapnik_layer_tooltip,
	(VikLayerFuncLayerSelected)           NULL,

	(VikLayerFuncMarshall)                mapnik_layer_marshall,
	(VikLayerFuncUnmarshall)              mapnik_layer_unmarshall,

	(VikLayerFuncSetParam)                mapnik_layer_set_param,
	(VikLayerFuncGetParam)                mapnik_layer_get_param,
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
};

struct _VikMapnikLayer {
	VikLayer vl;
	gchar *filename_css; // CartoCSS MML File - use 'carto' to convert into xml
	gchar *filename_xml;
	guint8 alpha;

	guint tile_size_x; // Y is the same as X ATM
	gboolean loaded;
	MapnikInterface* mi;
	guint rerender_timeout;

	gboolean use_file_cache;
	gchar *file_cache_dir;

	VikCoord rerender_ul;
	VikCoord rerender_br;
	gdouble rerender_zoom;
	GtkWidget *right_click_menu;
};

#define MAPNIK_PREFS_GROUP_KEY "mapnik"
#define MAPNIK_PREFS_NAMESPACE "mapnik."

static VikLayerParamData plugins_default ( void )
{
	VikLayerParamData data;
#ifdef WINDOWS
	data.s = g_strdup ( "input" );
#else
	if ( g_file_test ( "/usr/lib/mapnik/input", G_FILE_TEST_EXISTS ) )
		data.s = g_strdup ( "/usr/lib/mapnik/input" );
		// Current Debian locations
	else if ( g_file_test ( "/usr/lib/mapnik/3.0/input", G_FILE_TEST_EXISTS ) )
		data.s = g_strdup ( "/usr/lib/mapnik/3.0/input" );
	else if ( g_file_test ( "/usr/lib/mapnik/2.2/input", G_FILE_TEST_EXISTS ) )
		data.s = g_strdup ( "/usr/lib/mapnik/2.2/input" );
	else
		data.s = g_strdup ( "" );
#endif
	return data;
}

static VikLayerParamData fonts_default ( void )
{
	// Possibly should be string list to allow loading from multiple directories
	VikLayerParamData data;
#ifdef WINDOWS
	data.s = g_strdup ( "C:\\Windows\\Fonts" );
#elif defined __APPLE__
	data.s = g_strdup ( "/Library/Fonts" );
#else
	data.s = g_strdup ( "/usr/share/fonts" );
#endif
	return data;
}

static VikLayerParam prefs[] = {
	// Changing these values only applies before first mapnik layer is 'created'
	{ VIK_LAYER_NUM_TYPES, MAPNIK_PREFS_NAMESPACE"plugins_directory", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Plugins Directory:"), VIK_LAYER_WIDGET_FOLDERENTRY, NULL, NULL, N_("You need to restart Viking for a change to this value to be used"), plugins_default, NULL, NULL },
	{ VIK_LAYER_NUM_TYPES, MAPNIK_PREFS_NAMESPACE"fonts_directory", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Fonts Directory:"), VIK_LAYER_WIDGET_FOLDERENTRY, NULL, NULL, N_("You need to restart Viking for a change to this value to be used"), fonts_default, NULL, NULL },
	{ VIK_LAYER_NUM_TYPES, MAPNIK_PREFS_NAMESPACE"recurse_fonts_directory", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Recurse Fonts Directory:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, N_("You need to restart Viking for a change to this value to be used"), vik_lpd_true_default, NULL, NULL },
	{ VIK_LAYER_NUM_TYPES, MAPNIK_PREFS_NAMESPACE"rerender_after", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Rerender Timeout (hours):"), VIK_LAYER_WIDGET_SPINBUTTON, &scales[2], NULL, N_("You need to restart Viking for a change to this value to be used"), NULL, NULL, NULL },
	// Changeable any time
	{ VIK_LAYER_NUM_TYPES, MAPNIK_PREFS_NAMESPACE"carto", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("CartoCSS:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL,  N_("The program to convert CartoCSS files into Mapnik XML"), NULL, NULL, NULL },
};

static time_t planet_import_time;

static GMutex *tp_mutex;
static GHashTable *requests = NULL;

/**
 * vik_mapnik_layer_init:
 *
 * Just initialize preferences
 */
void vik_mapnik_layer_init (void)
{
	a_preferences_register_group ( MAPNIK_PREFS_GROUP_KEY, "Mapnik" );

	guint i = 0;
	VikLayerParamData tmp = plugins_default();
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp = fonts_default();
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp.b = TRUE;
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp.u = 168; // One week
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp.s = "carto";
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);
}

/**
 * vik_mapnik_layer_post_init:
 *
 * Initialize data structures - now that reading preferences is OK to perform
 */
void vik_mapnik_layer_post_init (void)
{
	tp_mutex = vik_mutex_new();

	// Just storing keys only
	requests = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );

	guint hours = a_preferences_get (MAPNIK_PREFS_NAMESPACE"rerender_after")->u;
	GDateTime *now = g_date_time_new_now_local ();
	GDateTime *then = g_date_time_add_hours (now, -hours);
	planet_import_time = g_date_time_to_unix (then);
	g_date_time_unref ( now );
	g_date_time_unref ( then );

	GStatBuf gsb;
	// Similar to mod_tile method to mark DB has been imported/significantly changed to cause a rerendering of all tiles
	gchar *import_time_file = g_strconcat ( a_get_viking_dir(), G_DIR_SEPARATOR_S, "planet-import-complete", NULL );
	if ( g_stat ( import_time_file, &gsb ) == 0 ) {
		// Only update if newer
		if ( planet_import_time > gsb.st_mtime )
			planet_import_time = gsb.st_mtime;
	}
	g_free ( import_time_file );
}

void vik_mapnik_layer_uninit ()
{
	vik_mutex_free (tp_mutex);
}

// NB Only performed once per program run
static void mapnik_layer_class_init ( VikMapnikLayerClass *klass )
{
	VikLayerParamData *pd = a_preferences_get (MAPNIK_PREFS_NAMESPACE"plugins_directory");
	VikLayerParamData *fd = a_preferences_get (MAPNIK_PREFS_NAMESPACE"fonts_directory");
	VikLayerParamData *rfd = a_preferences_get (MAPNIK_PREFS_NAMESPACE"recurse_fonts_directory");

	if ( pd && fd && rfd )
		mapnik_interface_initialize ( pd->s, fd->s, rfd->b );
	else
		g_critical ( "Unable to initialize mapnik interface from preferences" );
}

GType vik_mapnik_layer_get_type ()
{
	static GType vml_type = 0;

	if (!vml_type) {
		static const GTypeInfo vml_info = {
			sizeof (VikMapnikLayerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) mapnik_layer_class_init, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikMapnikLayer),
			0,
			NULL /* instance init */
		};
		vml_type = g_type_register_static ( VIK_LAYER_TYPE, "VikMapnikLayer", &vml_info, 0 );
	}

	return vml_type;
}

static const gchar* mapnik_layer_tooltip ( VikMapnikLayer *vml )
{
	return vml->filename_xml;
}

static void mapnik_layer_set_file_xml ( VikMapnikLayer *vml, const gchar *name )
{
	if ( vml->filename_xml )
		g_free (vml->filename_xml);
	// Mapnik doesn't seem to cope with relative filenames
	if ( g_strcmp0 (name, "" ) )
		vml->filename_xml = vu_get_canonical_filename ( VIK_LAYER(vml), name);
	else
		vml->filename_xml = g_strdup (name);
}

static void mapnik_layer_set_file_css ( VikMapnikLayer *vml, const gchar *name )
{
	if ( vml->filename_css )
		g_free (vml->filename_css);
	vml->filename_css = g_strdup (name);
}

static void mapnik_layer_set_cache_dir ( VikMapnikLayer *vml, const gchar *name )
{
	if ( vml->file_cache_dir )
		g_free (vml->file_cache_dir);
	vml->file_cache_dir = g_strdup (name);
}

static void mapnik_layer_marshall( VikMapnikLayer *vml, guint8 **data, gint *len )
{
	vik_layer_marshall_params ( VIK_LAYER(vml), data, len );
}

static VikMapnikLayer *mapnik_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
	VikMapnikLayer *rv = mapnik_layer_new ( vvp );
	vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
	return rv;
}

static gboolean mapnik_layer_set_param ( VikMapnikLayer *vml, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
	switch ( id ) {
		case PARAM_CONFIG_CSS: mapnik_layer_set_file_css (vml, data.s); break;
		case PARAM_CONFIG_XML: mapnik_layer_set_file_xml (vml, data.s); break;
		case PARAM_ALPHA: if ( data.u <= 255 ) vml->alpha = data.u; break;
		case PARAM_USE_FILE_CACHE: vml->use_file_cache = data.b; break;
		case PARAM_FILE_CACHE_DIR: mapnik_layer_set_cache_dir (vml, data.s); break;
		default: break;
	}
	return TRUE;
}

static VikLayerParamData mapnik_layer_get_param ( VikMapnikLayer *vml, guint16 id, gboolean is_file_operation )
{
	VikLayerParamData data;
	switch ( id ) {
		case PARAM_CONFIG_CSS: {
			data.s = vml->filename_css;
			gboolean set = FALSE;
			if ( is_file_operation ) {
				if ( a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE ) {
					gchar *cwd = g_get_current_dir();
					if ( cwd ) {
						data.s = file_GetRelativeFilename ( cwd, vml->filename_css );
						if ( !data.s ) data.s = "";
						set = TRUE;
					}
				}
			}
			if ( !set )
				data.s = vml->filename_css ? vml->filename_css : "";
			break;
		}
		case PARAM_CONFIG_XML: {
			data.s = vml->filename_xml;
			gboolean set = FALSE;
			if ( is_file_operation ) {
				if ( a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE ) {
					gchar *cwd = g_get_current_dir();
					if ( cwd ) {
						data.s = file_GetRelativeFilename ( cwd, vml->filename_xml );
						if ( !data.s ) data.s = "";
						set = TRUE;
					}
				}
			}
			if ( !set )
				data.s = vml->filename_xml ? vml->filename_xml : "";
			break;
		}
		case PARAM_ALPHA: data.u = vml->alpha; break;
		case PARAM_USE_FILE_CACHE: data.b = vml->use_file_cache; break;
		case PARAM_FILE_CACHE_DIR: data.s = vml->file_cache_dir; break;
		default: break;
	}
	return data;
}

/**
 *
 */
static VikMapnikLayer *mapnik_layer_new ( VikViewport *vvp )
{
	VikMapnikLayer *vml = VIK_MAPNIK_LAYER ( g_object_new ( VIK_MAPNIK_LAYER_TYPE, NULL ) );
	vik_layer_set_type ( VIK_LAYER(vml), VIK_LAYER_MAPNIK );
	vik_layer_set_defaults ( VIK_LAYER(vml), vvp );
	vml->tile_size_x = size_default().u; // FUTURE: Is there any use in this being configurable?
	vml->loaded = FALSE;
	vml->mi = mapnik_interface_new();
	return vml;
}

/**
 * Run carto command
 * ATM don't have any version issues AFAIK
 * Tested with carto 0.14.0
 */
gboolean carto_load ( VikMapnikLayer *vml, VikViewport *vvp )
{
	gchar *mystdout = NULL;
	gchar *mystderr = NULL;
	GError *error = NULL;

	VikLayerParamData *vlpd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"carto");
	gchar *command = g_strdup_printf ( "%s %s", vlpd->s, vml->filename_css );

	gboolean answer = TRUE;
	//gchar *args[2]; args[0] = vlpd->s; args[1] = vml->filename_css;
	//GPid pid;
	//if ( g_spawn_async_with_pipes ( NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, NULL, &carto_stdout, &carto_error, &error ) ) {
	// cf code in babel.c to handle stdout

	// NB Running carto may take several seconds
	//  especially for large style sheets like the default OSM Mapnik style (~6 seconds on my system)
	VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_WIDGET(vvp));
	if ( vw ) {
		gchar *msg = g_strdup_printf ( "%s: %s", _("Running"), command );
		vik_window_statusbar_update ( vw, msg, VIK_STATUSBAR_INFO );
		vik_window_set_busy_cursor ( vw );
	}

	gint64 tt1 = 0;
	gint64 tt2 = 0;
	// You won't get a sensible timing measurement if running too old a GLIB
#if GLIB_CHECK_VERSION (2, 28, 0)
	tt1 = g_get_real_time ();
#endif

	if ( g_spawn_command_line_sync ( command, &mystdout, &mystderr, NULL, &error ) ) {
#if GLIB_CHECK_VERSION (2, 28, 0)
		tt2 = g_get_real_time ();
#endif
		if ( mystderr )
			if ( strlen(mystderr) > 1 ) {
				a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(vvp), _("Error running carto command:\n%s"), mystderr );
				answer = FALSE;
			}
		if ( mystdout ) {
			// NB This will overwrite the specified XML file
			if ( ! ( vml->filename_xml && strlen (vml->filename_xml) > 1 ) ) {
				// XML Not specified so try to create based on CSS file name
				GRegex *regex = g_regex_new ( "\\.mml$|\\.mss|\\.css$", G_REGEX_CASELESS, 0, &error );
				if ( error )
					g_critical ("%s: %s", __FUNCTION__, error->message );
				if ( vml->filename_xml )
					g_free (vml->filename_xml);
				vml->filename_xml = g_regex_replace_literal ( regex, vml->filename_css, -1, 0, ".xml", 0, &error );
				if ( error )
					g_warning ("%s: %s", __FUNCTION__, error->message );
				// Prevent overwriting self
				if ( !g_strcmp0 ( vml->filename_xml, vml->filename_css ) ) {
					vml->filename_xml = g_strconcat ( vml->filename_css, ".xml", NULL );
				}
			}
			if ( !g_file_set_contents (vml->filename_xml, mystdout, -1, &error)  ) {
				g_warning ("%s: %s", __FUNCTION__, error->message );
				g_error_free (error);
			}
		}
		g_free ( mystdout );
		g_free ( mystderr );
	}
	else {
		g_warning ("%s: %s", __FUNCTION__, error->message );
		g_error_free (error);
	}
	g_free ( command );

	if ( vw ) {
		gchar *msg = g_strdup_printf ( "%s %s %.1f %s",  vlpd->s, _(" completed in "), (gdouble)(tt2-tt1)/G_USEC_PER_SEC, _("seconds") );
		vik_window_statusbar_update ( vw, msg, VIK_STATUSBAR_INFO );
		g_free ( msg );
		vik_window_clear_busy_cursor ( vw );
	}
	return answer;
}

/**
 *
 */
static void mapnik_layer_post_read (VikLayer *vl, VikViewport *vvp, gboolean from_file)
{
	VikMapnikLayer *vml = VIK_MAPNIK_LAYER(vl);

	// Determine if carto needs to be run
	gboolean do_carto = FALSE;
	if ( vml->filename_css && strlen(vml->filename_css) > 1 ) {
		if ( vml->filename_xml && strlen(vml->filename_xml) > 1) {
			// Compare timestamps
			GStatBuf gsb1;
			if ( g_stat ( vml->filename_xml, &gsb1 ) == 0 ) {
				GStatBuf gsb2;
				if ( g_stat ( vml->filename_css, &gsb2 ) == 0 ) {
					// Is CSS file newer than the XML file
					if ( gsb2.st_mtime > gsb1.st_mtime )
						do_carto = TRUE;
					else
						g_debug ( "No need to run carto" );
				}
			}
			else {
				// XML file doesn't exist
				do_carto = TRUE;
			}
		}
		else {
			// No XML specified thus need to generate
			do_carto = TRUE;
		}
	}

	if ( do_carto )
		// Don't load the XML config if carto load fails
		if ( !carto_load ( vml, vvp ) )
			return;

	gchar* ans = mapnik_interface_load_map_file ( vml->mi, vml->filename_xml, vml->tile_size_x, vml->tile_size_x );
	if ( ans ) {
		a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(vvp),
		                           _("Mapnik error loading configuration file:\n%s"),
		                           ans );
		g_free ( ans );
	}
	else {
		vml->loaded = TRUE;
		if ( !from_file )
			ui_add_recent_file ( vml->filename_xml );
	}
}

#define MAPNIK_LAYER_FILE_CACHE_LAYOUT "%s"G_DIR_SEPARATOR_S"%d"G_DIR_SEPARATOR_S"%d"G_DIR_SEPARATOR_S"%d.png"

// Free returned string after use
static gchar *get_filename ( gchar *dir, guint x, guint y, guint z)
{
	return g_strdup_printf ( MAPNIK_LAYER_FILE_CACHE_LAYOUT, dir, (17-z), x, y );
}

static void possibly_save_pixbuf ( VikMapnikLayer *vml, GdkPixbuf *pixbuf, MapCoord *ulm )
{
	if ( vml->use_file_cache ) {
		if ( vml->file_cache_dir ) {
			GError *error = NULL;
			gchar *filename = get_filename ( vml->file_cache_dir, ulm->x, ulm->y, ulm->scale );

			gchar *dir = g_path_get_dirname ( filename );
			if ( !g_file_test ( filename, G_FILE_TEST_EXISTS ) )
				if ( g_mkdir_with_parents ( dir , 0777 ) != 0 )
					g_warning ("%s: Failed to mkdir %s", __FUNCTION__, dir );
			g_free ( dir );

			if ( !gdk_pixbuf_save (pixbuf, filename, "png", &error, NULL ) ) {
				g_warning ("%s: %s", __FUNCTION__, error->message );
				g_error_free (error);
			}
			g_free (filename);
		}
	}
}

typedef struct
{
	VikMapnikLayer *vml;
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
static void render ( VikMapnikLayer *vml, VikCoord *ul, VikCoord *br, MapCoord *ulm )
{
	gint64 tt1 = g_get_real_time ();
	GdkPixbuf *pixbuf = mapnik_interface_render ( vml->mi, ul->north_south, ul->east_west, br->north_south, br->east_west );
	gint64 tt2 = g_get_real_time ();
	gdouble tt = (gdouble)(tt2-tt1)/1000000;
	g_debug ( "Mapnik rendering completed in %.3f seconds", tt );
	if ( !pixbuf ) {
		// A pixbuf to stick into cache incase of an unrenderable area - otherwise will get continually re-requested
		pixbuf = gdk_pixbuf_scale_simple ( gdk_pixbuf_from_pixdata(&vikmapniklayer_pixbuf, FALSE, NULL), vml->tile_size_x, vml->tile_size_x, GDK_INTERP_BILINEAR );
	}
	possibly_save_pixbuf ( vml, pixbuf, ulm );

	// NB Mapnik can apply alpha, but use our own function for now
	if ( vml->alpha < 255 )
		pixbuf = ui_pixbuf_scale_alpha ( pixbuf, vml->alpha );
	a_mapcache_add ( pixbuf, (mapcache_extra_t){ tt }, ulm->x, ulm->y, ulm->z, MAP_ID_MAPNIK_RENDER, ulm->scale, vml->alpha, 0.0, 0.0, vml->filename_xml );
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
		vik_layer_emit_update ( VIK_LAYER(data->vml) ); // NB update display from background
}

static void render_cancel_cleanup (RenderInfo *data)
{
	// Anything?
}

#define REQUEST_HASHKEY_FORMAT "%d-%d-%d-%d-%d"

/**
 * Thread
 */
void thread_add (VikMapnikLayer *vml, MapCoord *mul, VikCoord *ul, VikCoord *br, gint x, gint y, gint z, gint zoom, const gchar* name )
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
	gchar *description = g_strdup_printf ( _("Mapnik Render %d:%d:%d %s"), zoom, x, y, basename );
	g_free ( basename );
	a_background_thread ( BACKGROUND_POOL_LOCAL_MAPNIK,
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
 * load_pixbuf:
 *
 * If function returns GdkPixbuf properly, reference counter to this
 * buffer has to be decreased, when buffer is no longer needed.
 */
static GdkPixbuf *load_pixbuf ( VikMapnikLayer *vml, MapCoord *ulm, MapCoord *brm, gboolean *rerender )
{
	*rerender = FALSE;
	GdkPixbuf *pixbuf = NULL;
	gchar *filename = get_filename ( vml->file_cache_dir, ulm->x, ulm->y, ulm->scale );

	GStatBuf gsb;
	if ( g_stat ( filename, &gsb ) == 0 ) {
		// Get from disk
		GError *error = NULL;
		pixbuf = gdk_pixbuf_new_from_file ( filename, &error );
		if ( error ) {
			g_warning ("%s: %s", __FUNCTION__, error->message );
			g_error_free ( error );
		}
		else {
			if ( vml->alpha < 255 )
				pixbuf = ui_pixbuf_set_alpha ( pixbuf, vml->alpha );
			a_mapcache_add ( pixbuf, (mapcache_extra_t) { -42.0 }, ulm->x, ulm->y, ulm->z, MAP_ID_MAPNIK_RENDER, ulm->scale, vml->alpha, 0.0, 0.0, vml->filename_xml );
		}
		// If file is too old mark for rerendering
		if ( planet_import_time < gsb.st_mtime ) {
			*rerender = TRUE;
		}
	}
	g_free ( filename );

	return pixbuf;
}

/**
 * Caller has to decrease reference counter of returned
 * GdkPixbuf, when buffer is no longer needed.
 */
static GdkPixbuf *get_pixbuf ( VikMapnikLayer *vml, MapCoord *ulm, MapCoord *brm )
{
	VikCoord ul; VikCoord br;
	GdkPixbuf *pixbuf = NULL;

	map_utils_iTMS_to_vikcoord (ulm, &ul);
	map_utils_iTMS_to_vikcoord (brm, &br);

	pixbuf = a_mapcache_get ( ulm->x, ulm->y, ulm->z, MAP_ID_MAPNIK_RENDER, ulm->scale, vml->alpha, 0.0, 0.0, vml->filename_xml );

	if ( ! pixbuf ) {
		gboolean rerender = FALSE;
		if ( vml->use_file_cache && vml->file_cache_dir )
			pixbuf = load_pixbuf ( vml, ulm, brm, &rerender );
		if ( ! pixbuf || rerender ) {
			if ( TRUE )
				thread_add (vml, ulm, &ul, &br, ulm->x, ulm->y, ulm->z, ulm->scale, vml->filename_xml );
			else {
				// Run in the foreground
				render ( vml, &ul, &br, ulm );
				vik_layer_emit_update ( VIK_LAYER(vml) );
			}
		}
	}

	return pixbuf;
}

/**
 *
 */
static void mapnik_layer_draw ( VikMapnikLayer *vml, VikViewport *vvp )
{
	if ( !vml->loaded )
		return;

	if ( vik_viewport_get_drawmode(vvp) != VIK_VIEWPORT_DRAWMODE_MERCATOR ) {
		vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vml))),
		                            VIK_STATUSBAR_INFO, _("Mapnik Rendering must be in Mercator mode") );
		return;
	}

	if ( vml->mi ) {
		gchar *copyright = mapnik_interface_get_copyright ( vml->mi );
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
					vik_viewport_draw_pixbuf ( vvp, pixbuf, 0, 0, xx, yy, vml->tile_size_x, vml->tile_size_x );
					g_object_unref(pixbuf);
				}
			}
		}

		// Done after so drawn on top
		// Just a handy guide to tile blocks.
		if ( vik_debug && vik_verbose ) {
			GdkGC *black_gc = GTK_WIDGET(vvp)->style->black_gc;
			gint width = vik_viewport_get_width(vvp);
			gint height = vik_viewport_get_height(vvp);
			gint xx, yy;
			ulm.x = xmin; ulm.y = ymin;
			map_utils_iTMS_to_center_vikcoord ( &ulm, &coord );
			vik_viewport_coord_to_screen ( vvp, &coord, &xx, &yy );
			xx = xx - (vml->tile_size_x/2);
			yy = yy - (vml->tile_size_x/2); // Yes use X ATM
			for (gint x = xmin; x <= xmax; x++ ) {
				vik_viewport_draw_line ( vvp, black_gc, xx, 0, xx, height );
				xx += vml->tile_size_x;
			}
			for (gint y = ymin; y <= ymax; y++ ) {
				vik_viewport_draw_line ( vvp, black_gc, 0, yy, width, yy );
				yy += vml->tile_size_x; // Yes use X ATM
			}
		}
	}
}

/**
 *
 */
static void mapnik_layer_free ( VikMapnikLayer *vml )
{
	mapnik_interface_free ( vml->mi );
	if ( vml->filename_css )
		g_free ( vml->filename_css );
	if ( vml->filename_xml )
		g_free ( vml->filename_xml );
}

static VikMapnikLayer *mapnik_layer_create ( VikViewport *vp )
{
	return mapnik_layer_new ( vp );
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
static void mapnik_layer_flush_memory ( menu_array_values values )
{
	a_mapcache_flush_type ( MAP_ID_MAPNIK_RENDER );
}

/**
 *
 */
static void mapnik_layer_reload ( menu_array_values values )
{
	VikMapnikLayer *vml = values[MA_VML];
	VikViewport *vvp = values[MA_VVP];
	mapnik_layer_post_read (VIK_LAYER(vml), vvp, FALSE);
	mapnik_layer_draw ( vml, vvp );
}

/**
 * Force carto run
 *
 * Most carto projects will consist of many files
 * ATM don't have a way of detecting when any of the included files have changed
 * Thus allow a manual method to force re-running carto
 */
static void mapnik_layer_carto ( menu_array_values values )
{
	VikMapnikLayer *vml = values[MA_VML];
	VikViewport *vvp = values[MA_VVP];

	// Don't load the XML config if carto load fails
	if ( !carto_load ( vml, vvp ) )
		return;

	gchar* ans = mapnik_interface_load_map_file ( vml->mi, vml->filename_xml, vml->tile_size_x, vml->tile_size_x );
	if ( ans ) {
		a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(vvp),
		                           _("Mapnik error loading configuration file:\n%s"),
		                           ans );
		g_free ( ans );
	}
	else
		mapnik_layer_draw ( vml, vvp );
}

/**
 * Show Mapnik configuration parameters
 */
static void mapnik_layer_information ( menu_array_values values )
{
	VikMapnikLayer *vml = values[MA_VML];
	if ( !vml->mi )
		return;
	GArray *array = mapnik_interface_get_parameters( vml->mi );
	if ( array->len ) {
		a_dialog_list (  VIK_GTK_WINDOW_FROM_LAYER(vml), _("Mapnik Information"), array, 1 );
		// Free the copied strings
		for ( int i = 0; i < array->len; i++ )
			g_free ( g_array_index(array,gchar*,i) );
	}
	g_array_free ( array, FALSE );
}

/**
 *
 */
static void mapnik_layer_about ( menu_array_values values )
{
	VikMapnikLayer *vml = values[MA_VML];
	gchar *msg = mapnik_interface_about();
	a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vml),  msg );
	g_free ( msg );
}

/**
 *
 */
static void mapnik_layer_add_menu_items ( VikMapnikLayer *vml, GtkMenu *menu, gpointer vlp )
{
	static menu_array_values values;
	values[MA_VML] = vml;
	values[MA_VVP] = vik_layers_panel_get_viewport( VIK_LAYERS_PANEL(vlp) );

	GtkWidget *item = gtk_menu_item_new();
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );

	// Typical users shouldn't need to use this functionality - so debug only ATM
	if ( vik_debug ) {
		item = gtk_image_menu_item_new_with_mnemonic ( _("_Flush Memory Cache") );
		gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
		g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_flush_memory), values );
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show ( item );
	}

	item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_REFRESH, NULL );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_reload), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	if ( g_strcmp0 ("", vml->filename_css) ) {
		item = gtk_image_menu_item_new_with_mnemonic ( _("_Run Carto Command") );
		gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU) );
		g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_carto), values );
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show ( item );
	}

	item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_INFO, NULL );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_information), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_ABOUT, NULL );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_about), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
}

/**
 * Rerender a specific tile
 */
static void mapnik_layer_rerender ( VikMapnikLayer *vml )
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
	thread_add (vml, &ulm, &vml->rerender_ul, &vml->rerender_br, ulm.x, ulm.y, ulm.z, ulm.scale, vml->filename_xml );
}

/**
 * Info
 */
static void mapnik_layer_tile_info ( VikMapnikLayer *vml )
{
	MapCoord ulm;
	// Requested position to map coord
	map_utils_vikcoord_to_iTMS ( &vml->rerender_ul, vml->rerender_zoom, vml->rerender_zoom, &ulm );

	mapcache_extra_t extra = a_mapcache_get_extra ( ulm.x, ulm.y, ulm.z, MAP_ID_MAPNIK_RENDER, ulm.scale, vml->alpha, 0.0, 0.0, vml->filename_xml );

	gchar *filename = get_filename ( vml->file_cache_dir, ulm.x, ulm.y, ulm.scale );
	gchar *filemsg = NULL;
	gchar *timemsg = NULL;

	if ( g_file_test ( filename, G_FILE_TEST_EXISTS ) ) {
		filemsg = g_strconcat ( "Tile File: ", filename, NULL );
		// Get some timestamp information of the tile
		struct stat stat_buf;
		if ( g_stat ( filename, &stat_buf ) == 0 ) {
			gchar time_buf[64];
			strftime ( time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime) );
			timemsg = g_strdup_printf ( _("Tile File Timestamp: %s"), time_buf );
		}
		else {
			timemsg = g_strdup ( _("Tile File Timestamp: Not Available") );
		}
	}
	else {
		filemsg = g_strdup_printf ( "Tile File: %s [Not Available]", filename );
		timemsg = g_strdup("");
	}

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
	g_free ( filename );
}

static gboolean mapnik_feature_release ( VikMapnikLayer *vml, GdkEventButton *event, VikViewport *vvp )
{
	if ( !vml )
		return FALSE;
	if ( event->button == 3 ) {
		vik_viewport_screen_to_coord ( vvp, MAX(0, event->x), MAX(0, event->y), &vml->rerender_ul );
		vml->rerender_zoom = vik_viewport_get_zoom ( vvp );

		if ( ! vml->right_click_menu ) {
			GtkWidget *item;
			vml->right_click_menu = gtk_menu_new ();

			item = gtk_image_menu_item_new_with_mnemonic ( _("_Rerender Tile") );
			gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
			g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_rerender), vml );
			gtk_menu_shell_append ( GTK_MENU_SHELL(vml->right_click_menu), item );

			item = gtk_image_menu_item_new_with_mnemonic ( _("_Info") );
			gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INFO, GTK_ICON_SIZE_MENU) );
			g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_tile_info), vml );
			gtk_menu_shell_append ( GTK_MENU_SHELL(vml->right_click_menu), item );
		}

		gtk_menu_popup ( GTK_MENU(vml->right_click_menu), NULL, NULL, NULL, NULL, event->button, event->time );
		gtk_widget_show_all ( GTK_WIDGET(vml->right_click_menu) );
	}

	return FALSE;
}
