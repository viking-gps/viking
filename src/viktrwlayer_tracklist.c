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
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_propwin.h"

// Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here!
#define TRACK_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"

/**
 * track_close_cb:
 *
 */
static void track_close_cb ( GtkWidget *dialog, gint resp, GList *data )
{
	g_list_foreach ( data, (GFunc) g_free, NULL );
	g_list_free ( data );

	gtk_widget_destroy (dialog);
}

/**
 * format_1f_cell_data_func:
 *
 * General purpose column double formatting
 *
 */
static void format_1f_cell_data_func ( GtkTreeViewColumn *col,
                                       GtkCellRenderer   *renderer,
                                       GtkTreeModel      *model,
                                       GtkTreeIter       *iter,
                                       gpointer           user_data )
{
	gdouble value;
	gchar buf[20];
	gint column = GPOINTER_TO_INT (user_data);
	gtk_tree_model_get ( model, iter, column, &value, -1 );
	g_snprintf ( buf, sizeof(buf), "%.1f", value );
	g_object_set ( renderer, "text", buf, NULL );
}

#define TRK_LIST_COLS 11
#define TRK_COL_NUM TRK_LIST_COLS-1
#define TRW_COL_NUM TRK_COL_NUM-1

/*
 * trw_layer_track_tooltip_cb:
 *
 * Show a tooltip when the mouse is over a track list entry.
 * The tooltip contains the comment or description.
 */
static gboolean trw_layer_track_tooltip_cb ( GtkWidget  *widget,
                                             gint        x,
                                             gint        y,
                                             gboolean    keyboard_tip,
                                             GtkTooltip *tooltip,
                                             gpointer    data )
{
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
	GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

	if ( !gtk_tree_view_get_tooltip_context ( tree_view, &x, &y,
	                                          keyboard_tip,
	                                          &model, &path, &iter ) )
		return FALSE;

	VikTrack *trk;
	gtk_tree_model_get ( model, &iter, TRK_COL_NUM, &trk, -1 );
	if ( !trk ) return FALSE;

	gboolean tooltip_set = TRUE;
	if ( trk->comment )
		gtk_tooltip_set_text ( tooltip, trk->comment );
	else if ( trk->description )
		gtk_tooltip_set_text ( tooltip, trk->description );
	else
		tooltip_set = FALSE;

	if ( tooltip_set )
		gtk_tree_view_set_tooltip_row ( tree_view, tooltip, path );

	gtk_tree_path_free ( path );

	return tooltip_set;
}

/*
static void trw_layer_track_select_cb ( GtkTreeSelection *selection, gpointer data )
{
	GtkTreeIter iter;
	if ( !gtk_tree_selection_get_selected (selection, NULL, &iter) )
		return;

	GtkTreeView *tree_view = GTK_TREE_VIEW ( data );
	GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

	VikTrack *trk;
	gtk_tree_model_get ( model, &iter, TRK_COL_NUM, &trk, -1 );
	if ( !trk ) return;

	VikTrwLayer *vtl;
	gtk_tree_model_get ( model, &iter, TRW_COL_NUM, &vtl, -1 );
	if ( !IS_VIK_TRW_LAYER(vtl) ) return;

	//vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->track_iters, uuid ), TRUE );
}
*/

// A slightly better way of defining the menu callback information
// This should be much easier to extend/rework compared to the current trw_layer menus
typedef enum {
  MA_VTL = 0,
  MA_TRK,
  MA_TRK_UUID,
  MA_VVP,
  MA_TREEVIEW,
  MA_TRKS_LIST,
  MA_LAST
} menu_array_index;

typedef gpointer menu_array_values[MA_LAST];

// Instead of hooking automatically on treeview item selection
// This is performed on demand via the specific menu request
static void trw_layer_track_select ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
	VikTrack *trk = VIK_TRACK(values[MA_TRK]);

	if ( values[MA_TRK_UUID] ) {
		GtkTreeIter *iter = NULL;
		if ( trk->is_route )
			iter = g_hash_table_lookup ( vik_trw_layer_get_routes_iters(vtl), values[MA_TRK_UUID] );
		else
			iter = g_hash_table_lookup ( vik_trw_layer_get_tracks_iters(vtl), values[MA_TRK_UUID] );

		if ( iter )
			vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, iter, TRUE );
	}
}

static void trw_layer_track_stats ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
	VikTrack *trk = VIK_TRACK(values[MA_TRK]);
	VikViewport *vvp = VIK_VIEWPORT(values[MA_VVP]);

	if ( trk && trk->name ) {
		// Kill off this dialog to allow interaction with properties window
		//  since the properties also allows track manipulations it won't cause conflicts here.
		GtkWidget *gw = gtk_widget_get_toplevel ( values[MA_TREEVIEW] );
		track_close_cb ( gw, 0, values[MA_TRKS_LIST] );

		vik_trw_layer_propwin_run ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
		                            vtl,
		                            trk,
		                            NULL, // vlp
		                            vvp,
		                            TRUE );
    }
}

static void trw_layer_track_view ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
	VikTrack *trk = VIK_TRACK(values[MA_TRK]);
	VikViewport *vvp = VIK_VIEWPORT(values[MA_VVP]);

	// TODO create common function to convert between LatLon[2] and LatLonBBox or even change LatLonBBox to be 2 LatLons!
	struct LatLon maxmin[2];
	maxmin[0].lat = trk->bbox.north;
	maxmin[1].lat = trk->bbox.south;
	maxmin[0].lon = trk->bbox.east;
	maxmin[1].lon = trk->bbox.west;

    trw_layer_zoom_to_show_latlons ( vtl, vvp, maxmin );

	trw_layer_track_select (values);
}

typedef struct {
  gboolean has_layer_names;
  GString *str;
} copy_data_t;

static void copy_selection (GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data)
{
	copy_data_t *cd = (copy_data_t*) data;

	gchar* layername; gtk_tree_model_get ( model, iter, 0, &layername, -1 );
	gchar* name; gtk_tree_model_get ( model, iter, 1, &name, -1 );
	gchar* date; gtk_tree_model_get ( model, iter, 2, &date, -1 );
	gdouble d1; gtk_tree_model_get ( model, iter, 4, &d1, -1 );
	guint d2; gtk_tree_model_get ( model, iter, 5, &d2, -1 );
	gdouble d3; gtk_tree_model_get ( model, iter, 6, &d3, -1 );
	gdouble d4; gtk_tree_model_get ( model, iter, 7, &d4, -1 );
	gint d5; gtk_tree_model_get ( model, iter, 8, &d5, -1 );
	gchar sep = '\t'; // Could make this configurable - but simply always make it a tab character for now
	// NB Even if the columns have been reordered - this copies it out only in the original default order
	// if col 0 is displayed then also copy the layername
	if ( cd->has_layer_names )
		g_string_append_printf ( cd->str, "%s%c%s%c%s%c%.1f%c%d%c%.1f%c%.1f%c%d\n", layername, sep, name, sep, date, sep, d1, sep, d2, sep, d3, sep, d4, sep, d5 );
	else
		g_string_append_printf ( cd->str, "%s%c%s%c%.1f%c%d%c%.1f%c%.1f%c%d\n", name, sep, date, sep, d1, sep, d2, sep, d3, sep, d4, sep, d5 );
	g_free ( layername );
	g_free ( name );
	g_free ( date );
}

static void trw_layer_copy_selected ( GtkWidget *tree_view )
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(tree_view) );
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList *gl = gtk_tree_view_get_columns ( GTK_TREE_VIEW(tree_view) );
	guint count = g_list_length ( gl );
	g_list_free ( gl );
	copy_data_t cd;
	cd.has_layer_names = (count > TRK_LIST_COLS-3);
	// Or use gtk_tree_view_column_get_visible()?
	cd.str = g_string_new ( NULL );
	gtk_tree_selection_selected_foreach ( selection, copy_selection, &cd );

	a_clipboard_copy ( VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, cd.str->str, NULL );

	g_string_free ( cd.str, TRUE );
}

static void add_copy_menu_item ( GtkMenu *menu, GtkWidget *tree_view )
{
	GtkWidget *item = gtk_image_menu_item_new_with_mnemonic ( _("_Copy Data") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_selected), tree_view );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
}

static gboolean add_menu_items ( GtkMenu *menu, VikTrwLayer *vtl, VikTrack *trk, gpointer trk_uuid, VikViewport *vvp, GtkWidget *tree_view, gpointer data )
{
	static menu_array_values values;
	GtkWidget *item;

	values[MA_VTL]       = vtl;
	values[MA_TRK]       = trk;
	values[MA_TRK_UUID]  = trk_uuid;
	values[MA_VVP]       = vvp;
	values[MA_TREEVIEW]  = tree_view;
	values[MA_TRKS_LIST] = data;

	/*
	item = gtk_image_menu_item_new_with_mnemonic ( _("_Select") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_select), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	*/

	// ATM view auto selects, so don't bother with separate select menu entry
	item = gtk_image_menu_item_new_with_mnemonic ( _("_View") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_view), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("_Statistics") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_stats), values );
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );

	add_copy_menu_item ( menu, tree_view );

	return TRUE;
}

static gboolean trw_layer_track_menu_popup_multi  ( GtkWidget *tree_view,
                                                    GdkEventButton *event,
                                                    gpointer data )
{
	GtkWidget *menu = gtk_menu_new();

	add_copy_menu_item ( GTK_MENU(menu), tree_view );

	gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );

	return TRUE;
}

static gboolean trw_layer_track_menu_popup ( GtkWidget *tree_view,
                                             GdkEventButton *event,
                                             gpointer data )
{
	static GtkTreeIter iter;

	// Use selected item to get a single iterator ref
	// This relies on an row being selected as part of the right click
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(tree_view) );
	if ( gtk_tree_selection_count_selected_rows (selection) != 1 )
		return trw_layer_track_menu_popup_multi ( tree_view, event, data );

	GtkTreePath *path;
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(tree_view) );

	// All this just to get the iter
	if ( gtk_tree_view_get_path_at_pos ( GTK_TREE_VIEW(tree_view),
	                                     (gint) event->x,
	                                     (gint) event->y,
	                                     &path, NULL, NULL, NULL)) {
		gtk_tree_model_get_iter_from_string ( model, &iter, gtk_tree_path_to_string (path) );
		gtk_tree_path_free ( path );
	}
	else
		return FALSE;

	VikTrack *trk;
	gtk_tree_model_get ( model, &iter, TRK_COL_NUM, &trk, -1 );
	if ( !trk ) return FALSE;

	VikTrwLayer *vtl;
	gtk_tree_model_get ( model, &iter, TRW_COL_NUM, &vtl, -1 );
	if ( !IS_VIK_TRW_LAYER(vtl) ) return FALSE;

	trku_udata udataU;
	udataU.trk  = trk;
	udataU.uuid = NULL;

	gpointer *trkf;
	if ( trk->is_route )
		trkf = g_hash_table_find ( vik_trw_layer_get_routes(vtl), (GHRFunc) trw_layer_track_find_uuid, &udataU );
	else
		trkf = g_hash_table_find ( vik_trw_layer_get_tracks(vtl), (GHRFunc) trw_layer_track_find_uuid, &udataU );

	if ( trkf && udataU.uuid ) {
		VikViewport *vvp = vik_window_viewport((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)));

		GtkWidget *menu = gtk_menu_new();

		// Originally started to reuse the trw_layer menu items
		//  however these offer too many ways to edit the track data
		//  so without an easy way to distinguish read only operations,
		//  create a very minimal new set of operations
		add_menu_items ( GTK_MENU(menu),
		                 vtl,
		                 trk,
		                 udataU.uuid,
		                 vvp,
		                 tree_view,
		                 data );

		gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
		return TRUE;
	}
	return FALSE;
}

static gboolean trw_layer_track_button_pressed ( GtkWidget *tree_view,
                                                 GdkEventButton *event,
                                                 gpointer data )
{
	// Only on right clicks...
	if ( ! (event->type == GDK_BUTTON_PRESS && event->button == 3) )
		return FALSE;

	// ATM Force a selection...
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(tree_view) );
	if ( gtk_tree_selection_count_selected_rows (selection) <= 1 ) {
		GtkTreePath *path;
		/* Get tree path for row that was clicked */
		if ( gtk_tree_view_get_path_at_pos ( GTK_TREE_VIEW(tree_view),
		                                     (gint) event->x,
	                                         (gint) event->y,
		                                     &path, NULL, NULL, NULL)) {
			gtk_tree_selection_unselect_all ( selection );
			gtk_tree_selection_select_path ( selection, path );
			gtk_tree_path_free ( path );
		}
	}
	return trw_layer_track_menu_popup ( tree_view, event, data );
}

/*
 * Foreach entry we copy the various individual track properties into the tree store
 *  formatting & converting the internal values into something for display
 */
static void trw_layer_track_list_add ( vik_trw_track_list_t *vtdl,
                                       GtkTreeStore *store,
                                       vik_units_distance_t dist_units,
                                       vik_units_speed_t speed_units,
                                       vik_units_height_t height_units )
{
	GtkTreeIter t_iter;
	VikTrack *trk = vtdl->trk;
	VikTrwLayer *vtl = vtdl->vtl;

	gdouble trk_dist = vik_track_get_length ( trk );
	// Store unit converted value
	switch ( dist_units ) {
	case VIK_UNITS_DISTANCE_MILES:
		trk_dist = VIK_METERS_TO_MILES(trk_dist);
		break;
	default:
		trk_dist = trk_dist/1000.0;
		break;
	}

	// Get start date
	gchar time_buf[32];
	time_buf[0] = '\0';
	if ( trk->trackpoints && VIK_TRACKPOINT(trk->trackpoints->data)->has_timestamp ) {

#if GLIB_CHECK_VERSION(2,26,0)
		GDateTime* gdt = g_date_time_new_from_unix_utc ( VIK_TRACKPOINT(trk->trackpoints->data)->timestamp );
		gchar *time = g_date_time_format ( gdt, TRACK_LIST_DATE_FORMAT );
		strncpy ( time_buf, time, sizeof(time_buf) );
		g_free ( time );
		g_date_time_unref ( gdt);
#else
		GDate* gdate_start = g_date_new ();
		g_date_set_time_t ( gdate_start, VIK_TRACKPOINT(trk->trackpoints->data)->timestamp );
		g_date_strftime ( time_buf, sizeof(time_buf), TRACK_LIST_DATE_FORMAT, gdate_start );
		g_date_free ( gdate_start );
#endif
	}

	// NB: doesn't include aggegrate visibility
	gboolean visible = VIK_LAYER(vtl)->visible && trk->visible;
	visible = visible && (trk->is_route ? vik_trw_layer_get_routes_visibility(vtl) : vik_trw_layer_get_tracks_visibility(vtl));

	guint trk_len_time = 0;
	if ( trk->trackpoints ) {
		time_t t1, t2;
		t1 = VIK_TRACKPOINT(g_list_first(trk->trackpoints)->data)->timestamp;
		t2 = VIK_TRACKPOINT(g_list_last(trk->trackpoints)->data)->timestamp;
		trk_len_time = (int)round (labs(t2-t1)/60);
	}

	gdouble av_speed = 0.0;
	gdouble max_speed = 0.0;
	gdouble max_alt = 0.0;

	av_speed = vik_track_get_average_speed ( trk );
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: av_speed = VIK_MPS_TO_KPH(av_speed); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      av_speed = VIK_MPS_TO_MPH(av_speed); break;
	case VIK_UNITS_SPEED_KNOTS:               av_speed = VIK_MPS_TO_KNOTS(av_speed); break;
	default: // VIK_UNITS_SPEED_METRES_PER_SECOND therefore no change
		break;
	}

	max_speed = vik_track_get_max_speed ( trk );
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: max_speed = VIK_MPS_TO_KPH(max_speed); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      max_speed = VIK_MPS_TO_MPH(max_speed); break;
	case VIK_UNITS_SPEED_KNOTS:               max_speed = VIK_MPS_TO_KNOTS(max_speed); break;
	default: // VIK_UNITS_SPEED_METRES_PER_SECOND therefore no change
		break;
	}

	// TODO - make this a function to get min / max values?
	gdouble *altitudes = NULL;
	altitudes = vik_track_make_elevation_map ( trk, 500 );
	if ( altitudes ) {
		max_alt = -1000;
		guint i;
		for ( i=0; i < 500; i++ ) {
			if ( altitudes[i] != VIK_DEFAULT_ALTITUDE ) {
				if ( altitudes[i] > max_alt )
					max_alt = altitudes[i];
			}
		}
	}
	g_free ( altitudes );

	switch (height_units) {
	case VIK_UNITS_HEIGHT_FEET: max_alt = VIK_METERS_TO_FEET(max_alt); break;
	default:
		// VIK_UNITS_HEIGHT_METRES: no need to convert
		break;
	}

	gtk_tree_store_append ( store, &t_iter, NULL );
	gtk_tree_store_set ( store, &t_iter,
	                     0, VIK_LAYER(vtl)->name,
	                     1, trk->name,
	                     2, time_buf,
	                     3, visible,
	                     4, trk_dist,
	                     5, trk_len_time,
	                     6, av_speed,
	                     7, max_speed,
	                     8, (gint)round(max_alt),
	                     TRW_COL_NUM, vtl,
	                     TRK_COL_NUM, trk,
	                     -1 );
}

static GtkTreeViewColumn *my_new_column_text ( const gchar *title, GtkCellRenderer *renderer, GtkWidget *view, gint column_runner )
{
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ( title, renderer, "text", column_runner, NULL );
	gtk_tree_view_column_set_sort_column_id ( column, column_runner );
	gtk_tree_view_append_column ( GTK_TREE_VIEW(view), column );
	gtk_tree_view_column_set_reorderable ( column, TRUE );
	gtk_tree_view_column_set_resizable ( column, TRUE );
	return column;
}

/**
 * vik_trw_layer_track_list_internal:
 * @dialog:            The dialog to create the widgets in
 * @tracks_and_layers: The list of tracks (and it's layer) to be shown
 * @show_layer_names:  Show the layer names that each track belongs to
 *
 * Create a table of tracks with corresponding track information
 * This table does not support being actively updated
 */
static void vik_trw_layer_track_list_internal ( GtkWidget *dialog,
                                                GList *tracks_and_layers,
                                                gboolean show_layer_names )
{
	if ( !tracks_and_layers )
		return;

	// It's simple storing the gdouble values in the tree store as the sort works automatically
	// Then apply specific cell data formatting (rather default double is to 6 decimal places!)
	GtkTreeStore *store = gtk_tree_store_new ( TRK_LIST_COLS,
	                                           G_TYPE_STRING,    // 0: Layer Name
	                                           G_TYPE_STRING,    // 1: Track Name
	                                           G_TYPE_STRING,    // 2: Date
	                                           G_TYPE_BOOLEAN,   // 3: Visible
	                                           G_TYPE_DOUBLE,    // 4: Distance
	                                           G_TYPE_UINT,      // 5: Length in time
	                                           G_TYPE_DOUBLE,    // 6: Av. Speed
	                                           G_TYPE_DOUBLE,    // 7: Max Speed
	                                           G_TYPE_INT,       // 8: Max Height
	                                           G_TYPE_POINTER,   // 9: TrackWaypoint Layer pointer
	                                           G_TYPE_POINTER ); // 10: Track pointer

	//gtk_tree_selection_set_select_function ( gtk_tree_view_get_selection (GTK_TREE_VIEW(vt)), vik_treeview_selection_filter, vt, NULL );

	vik_units_distance_t dist_units = a_vik_get_units_distance ();
	vik_units_speed_t speed_units = a_vik_get_units_speed ();
	vik_units_height_t height_units = a_vik_get_units_height ();

	//GList *gl = get_tracks_and_layers_cb ( vl, user_data );
	//g_list_foreach ( tracks_and_layers, (GFunc) trw_layer_track_list_add, store );
	GList *gl = tracks_and_layers;
	while ( gl ) {
		trw_layer_track_list_add ( (vik_trw_track_list_t*)gl->data, store, dist_units, speed_units, height_units );
		gl = g_list_next ( gl );
	}

	GtkWidget *view = gtk_tree_view_new();
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set (G_OBJECT (renderer),
	              "xalign", 0.0,
	              "ellipsize", PANGO_ELLIPSIZE_END,
	              NULL);

	GtkTreeViewColumn *column;
	GtkTreeViewColumn *sort_by_column;

	gint column_runner = 0;
	if ( show_layer_names ) {
		// Insert column for the layer name when viewing multi layers
		column = my_new_column_text ( _("Layer"), renderer, view, column_runner++ );
		gtk_tree_view_column_set_expand ( column, TRUE );
		// remember the layer column so we can sort by it later
		sort_by_column = column;
	}
	else
		column_runner++;

	column = my_new_column_text ( _("Name"), renderer, view, column_runner++ );
	gtk_tree_view_column_set_expand ( column, TRUE );
	if ( !show_layer_names )
		// remember the name column so we can sort by it later
		sort_by_column = column;

	column = my_new_column_text ( _("Date"), renderer, view, column_runner++ );
	gtk_tree_view_column_set_expand ( column, TRUE );

	GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes ( _("Visible"), renderer_toggle, "active", column_runner, NULL );
	gtk_tree_view_column_set_reorderable ( column, TRUE );
	gtk_tree_view_column_set_sort_column_id ( column, column_runner );
	gtk_tree_view_append_column ( GTK_TREE_VIEW(view), column );
	column_runner++;

	switch ( dist_units ) {
	case VIK_UNITS_DISTANCE_MILES:
		column = my_new_column_text ( _("Distance\n(miles)"), renderer, view, column_runner++ );
		break;
	default:
		column = my_new_column_text ( _("Distance\n(km)"), renderer, view, column_runner++ );
		break;
	}
	// Apply own formatting of the data
	gtk_tree_view_column_set_cell_data_func ( column, renderer, format_1f_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL);

	column = my_new_column_text ( _("Length\n(minutes)"), renderer, view, column_runner++ );

	gchar *spd_units = NULL;
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: spd_units = g_strdup (_("km/h")); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      spd_units = g_strdup (_("mph")); break;
	case VIK_UNITS_SPEED_KNOTS:               spd_units = g_strdup (_("knots")); break;
	// VIK_UNITS_SPEED_METRES_PER_SECOND:
	default:                                  spd_units = g_strdup (_("m/s")); break;
	}

	gchar *title = g_strdup_printf ( _("Av. Speed\n(%s)"), spd_units );
	column = my_new_column_text ( title, renderer, view, column_runner++ );
	g_free ( title );
	gtk_tree_view_column_set_cell_data_func ( column, renderer, format_1f_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL); // Apply own formatting of the data

	title = g_strdup_printf ( _("Max Speed\n(%s)"), spd_units );
	column = my_new_column_text ( title, renderer, view, column_runner++ );
	gtk_tree_view_column_set_cell_data_func ( column, renderer, format_1f_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL); // Apply own formatting of the data

	g_free ( title );
	g_free ( spd_units );

	if ( height_units == VIK_UNITS_HEIGHT_FEET )
		column = my_new_column_text ( _("Max Height\n(Feet)"), renderer, view, column_runner++ );
	else
		column = my_new_column_text ( _("Max Height\n(Metres)"), renderer, view, column_runner++ );

	gtk_tree_view_set_model ( GTK_TREE_VIEW(view), GTK_TREE_MODEL(store) );
	gtk_tree_selection_set_mode ( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE );
	gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW(view), TRUE );

	g_object_unref(store);

	GtkWidget *scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_container_add ( GTK_CONTAINER(scrolledwindow), view );

	g_object_set ( view, "has-tooltip", TRUE, NULL);

	g_signal_connect ( view, "query-tooltip", G_CALLBACK (trw_layer_track_tooltip_cb), NULL );
	//g_signal_connect ( gtk_tree_view_get_selection (GTK_TREE_VIEW(view)), "changed", G_CALLBACK(trw_layer_track_select_cb), view );

	g_signal_connect ( view, "popup-menu", G_CALLBACK(trw_layer_track_menu_popup), tracks_and_layers );
	g_signal_connect ( view, "button-press-event", G_CALLBACK(trw_layer_track_button_pressed), tracks_and_layers );

	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, TRUE, TRUE, 0);

	// Set ordering of the initial view by one of the name columns
	gtk_tree_view_column_clicked ( sort_by_column );

	// Ensure a reasonable number of items are shown
	//  TODO: may be save window size, column order, sorted by between invocations.
	// Gtk too stupid to work out best size so need to tell it.
	gtk_window_set_default_size ( GTK_WINDOW(dialog), show_layer_names ? 900 : 700, 400 );
}


/**
 * vik_trw_layer_track_list_show_dialog:
 * @title:                    The title for the dialog
 * @vl:                       The #VikLayer passed on into get_tracks_and_layers_cb()
 * @user_data:                Data passed on into get_tracks_and_layers_cb()
 * @get_tracks_and_layers_cb: The function to call to construct items to be analysed
 * @show_layer_names:         Normally only set when called from an aggregate level
 *
 * Common method for showing a list of tracks with extended information
 *
 */
void vik_trw_layer_track_list_show_dialog ( gchar *title,
                                            VikLayer *vl,
                                            gpointer user_data,
                                            VikTrwlayerGetTracksAndLayersFunc get_tracks_and_layers_cb,
                                            gboolean show_layer_names )
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons ( title,
	                                                  VIK_GTK_WINDOW_FROM_LAYER(vl),
	                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
	                                                  GTK_STOCK_CLOSE,
	                                                  GTK_RESPONSE_CLOSE,
	                                                  NULL );

	GList *gl = get_tracks_and_layers_cb ( vl, user_data );

	vik_trw_layer_track_list_internal ( dialog, gl, show_layer_names );

	// Use response to close the dialog with tidy up
	g_signal_connect ( G_OBJECT(dialog), "response", G_CALLBACK(track_close_cb), gl );

	gtk_widget_show_all ( dialog );
	// Yes - set the size *AGAIN* - this time widgets are expanded nicely
	gtk_window_resize ( GTK_WINDOW(dialog), show_layer_names ? 1000 : 800, 400 );

	// ATM lock out on dialog run - to prevent list contents being manipulated in other parts of the GUI whilst shown here.
	gtk_dialog_run (GTK_DIALOG (dialog));
	// Unfortunately seems subsequently opening the Track Properties we can't interact with it until this dialog is closed
	// Thus this dialog is then forcibly closed when opening the properties.

	// Occassionally the 'View' doesn't update the viewport properly
	//  viewport center + zoom is changed but the viewport isn't updated
	// not sure why yet..
}
