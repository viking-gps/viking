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
#include "util.h"
#include "ui_util.h"
#include "preferences.h"
#include "icons/icons.h"
#include "mapnik_interface.h"

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

static VikLayerParamScale scales[] = {
	{ 0, 255, 5, 0 }, // Alpha
	{ 64, 1024, 8, 0 }, // Tile size
};

VikLayerParam mapnik_layer_params[] = {
  { VIK_LAYER_MAPNIK, "config-file-xml", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("XML Config File:"), VIK_LAYER_WIDGET_FILEENTRY, GINT_TO_POINTER(VF_FILTER_XML), NULL,
    N_("Mapnik XML configuration file"), file_default, NULL, NULL },
  { VIK_LAYER_MAPNIK, "alpha", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Alpha:"), VIK_LAYER_WIDGET_HSCALE, &scales[0], NULL,
    NULL, alpha_default, NULL, NULL },
};

enum {
	PARAM_CONFIG_XML = 0,
	PARAM_ALPHA,
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

// See comment in viktrwlayer.c for advice on values used
// FUTURE:
static VikToolInterface mapnik_tools[] = {
	// Layer Info
	// Zoom All?
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
	gchar *filename_xml;
	guint8 alpha;

	guint tile_size_x; // Y is the same as X ATM
	gboolean loaded;
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
	else if ( g_file_test ( "/usr/lib/mapnik/2.2/input", G_FILE_TEST_EXISTS ) )
		// Current Debian location
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
};

// NB Only performed once per program run
static void mapnik_layer_class_init ( VikMapnikLayerClass *klass )
{
	mapnik_interface_init ( a_preferences_get (MAPNIK_PREFS_NAMESPACE"plugins_directory")->s,
	                        a_preferences_get (MAPNIK_PREFS_NAMESPACE"fonts_directory")->s,
	                        a_preferences_get (MAPNIK_PREFS_NAMESPACE"recurse_fonts_directory")->b );
}

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
	vml->filename_xml = g_strdup (name);
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
		case PARAM_CONFIG_XML: mapnik_layer_set_file_xml (vml, data.s); break;
		case PARAM_ALPHA: if ( data.u <= 255 ) vml->alpha = data.u; break;
		default: break;
	}
	return TRUE;
}

static VikLayerParamData mapnik_layer_get_param ( VikMapnikLayer *vml, guint16 id, gboolean is_file_operation )
{
	VikLayerParamData data;
	switch ( id ) {
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
		//case PARAM_TILE_X: data.u = vml->tile_size_x; break;
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
	return vml;
}

/**
 *
 */
static void mapnik_layer_post_read (VikLayer *vl, VikViewport *vvp, gboolean from_file)
{
	VikMapnikLayer *vml = VIK_MAPNIK_LAYER(vl);

	if ( mapnik_interface_load_map_file ( vml->filename_xml, vml->tile_size_x, vml->tile_size_x ) ) {
		if ( !from_file )
			a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(vvp),
			                         _("Mapnik error loading configuration file: %s"),
			                         vml->filename_xml );
		else
			g_warning ( _("Mapnik error loading configuration file: %s"), vml->filename_xml );
	}
	else {
		vml->loaded = TRUE;
		if ( !from_file )
			ui_add_recent_file ( vml->filename_xml );
	}
}

/**
 *
 */
static GdkPixbuf *get_pixbuf ( VikMapnikLayer *vml, MapCoord *ulm, MapCoord *brm )
{
	VikCoord ul; VikCoord br;
	GdkPixbuf *pixbuf;

	map_utils_iTMS_to_vikcoord (ulm, &ul);
	map_utils_iTMS_to_vikcoord (brm, &br);

	pixbuf = a_mapcache_get ( ulm->x, ulm->y, ulm->z, MAP_ID_MAPNIK_RENDER, ulm->scale, vml->alpha, 0.0, 0.0, vml->filename_xml );

	if ( ! pixbuf ) {
		pixbuf = mapnik_interface_render ( vml->mi, ul.north_south, ul.east_west, br.north_south, br.east_west );
		if ( pixbuf ) {
			// NB Mapnik can apply alpha, but use our own function for now
			if ( vml->alpha < 255 )
				pixbuf = ui_pixbuf_set_alpha ( pixbuf, vml->alpha );
			a_mapcache_add ( pixbuf, ulm->x, ulm->y, ulm->z, MAPNIK_LAYER_MAP_TYPE, ulm->scale, vml->alpha, 0.0, 0.0, vml->filename_xml );
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
		// TODO: Also potentially allows using multi threads for each individual tile
		//   this is more important for complicated stylesheets/datasources as each rendering may take some time
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
}
