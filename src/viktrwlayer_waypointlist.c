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
#include "viktrwlayer_waypointlist.h"

// Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here!
#define WAYPOINT_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"

/**
 * waypoint_close_cb:
 *
 */
static void waypoint_close_cb ( GtkWidget *dialog, gint resp, GList *data )
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
 */

#define WPT_LIST_COLS 9
#define WPT_COL_NUM WPT_LIST_COLS-1
#define TRW_COL_NUM WPT_COL_NUM-1

/*
 * trw_layer_waypoint_tooltip_cb:
 *
 * Show a tooltip when the mouse is over a waypoint list entry.
 * The tooltip contains the description.
 */
static gboolean trw_layer_waypoint_tooltip_cb ( GtkWidget  *widget,
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

	VikWaypoint *wpt;
	gtk_tree_model_get ( model, &iter, WPT_COL_NUM, &wpt, -1 );
	if ( !wpt ) return FALSE;

	gboolean tooltip_set = TRUE;
	if ( wpt->description )
		gtk_tooltip_set_text ( tooltip, wpt->description );
	else
		tooltip_set = FALSE;

	if ( tooltip_set )
		gtk_tree_view_set_tooltip_row ( tree_view, tooltip, path );

	gtk_tree_path_free ( path );

	return tooltip_set;
}

/*
static void trw_layer_waypoint_select_cb ( GtkTreeSelection *selection, gpointer data )
{
	GtkTreeIter iter;
	if ( !gtk_tree_selection_get_selected (selection, NULL, &iter) )
		return;

	GtkTreeView *tree_view = GTK_TREE_VIEW ( data );
	GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

	VikWaypoint *wpt;
	gtk_tree_model_get ( model, &iter, WPT_COL_NUM, &wpt, -1 );
	if ( !wpt ) return;

	VikTrwLayer *vtl;
	gtk_tree_model_get ( model, &iter, TRW_COL_NUM, &vtl, -1 );
	if ( !IS_VIK_TRW_LAYER(vtl) ) return;

	//vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->waypoint_iters, uuid ), TRUE );
}
*/

// A slightly better way of defining the menu callback information
// This should be much easier to extend/rework compared to the current trw_layer menus
typedef enum {
  MA_VTL = 0,
  MA_WPT,
  MA_WPT_UUID,
  MA_VVP,
  MA_TREEVIEW,
  MA_WPTS_LIST,
  MA_LAST
} menu_array_index;

typedef gpointer menu_array_values[MA_LAST];

// Instead of hooking automatically on treeview item selection
// This is performed on demand via the specific menu request
static void trw_layer_waypoint_select ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

	if ( values[MA_WPT_UUID] ) {
		GtkTreeIter *iter = NULL;
		iter = g_hash_table_lookup ( vik_trw_layer_get_waypoints_iters(vtl), values[MA_WPT_UUID] );

		if ( iter )
			vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, iter, TRUE );
	}
}

static void trw_layer_waypoint_properties ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
	VikWaypoint *wpt = VIK_WAYPOINT(values[MA_WPT]);

	if ( wpt && wpt->name ) {
		// Kill off this dialog to allow interaction with properties window
		//  since the properties also allows waypoint manipulations it won't cause conflicts here.
		GtkWidget *gw = gtk_widget_get_toplevel ( values[MA_TREEVIEW] );
		waypoint_close_cb ( gw, 0, values[MA_WPTS_LIST] );

		gboolean updated = FALSE;
		gchar *new_name = a_dialog_waypoint ( VIK_GTK_WINDOW_FROM_LAYER(vtl), wpt->name, vtl, wpt, vik_trw_layer_get_coord_mode(vtl), FALSE, &updated );
		if ( new_name )
			trw_layer_waypoint_rename ( vtl, wpt, new_name );

		if ( updated )
			trw_layer_waypoint_reset_icon ( vtl, wpt );

		if ( updated && VIK_LAYER(vtl)->visible )
			vik_layer_emit_update ( VIK_LAYER(vtl) );
	}
}

static void trw_layer_waypoint_view ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
	VikWaypoint *wpt = VIK_WAYPOINT(values[MA_WPT]);
	VikViewport *vvp = VIK_VIEWPORT(values[MA_VVP]);

	vik_viewport_set_center_coord ( vvp, &(wpt->coord) );

	trw_layer_waypoint_select (values);

	vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_show_picture ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
	VikWaypoint *wpt = VIK_WAYPOINT(values[MA_WPT]);

#ifdef WINDOWS
	ShellExecute(NULL, "open", wpt->image, NULL, NULL, SW_SHOWNORMAL);
#else
	GError *err = NULL;
	gchar *quoted_file = g_shell_quote ( wpt->image );
	gchar *cmd = g_strdup_printf ( "%s %s", a_vik_get_image_viewer(), quoted_file );
	g_free ( quoted_file );
	if ( ! g_spawn_command_line_async ( cmd, &err ) ) {
		a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Could not launch %s to open file."), a_vik_get_image_viewer() );
		g_error_free ( err );
	}
	g_free ( cmd );
#endif
}


static gboolean add_menu_items ( GtkMenu *menu, VikTrwLayer *vtl, VikWaypoint *wpt, gpointer wpt_uuid, VikViewport *vvp, GtkWidget *tree_view, gpointer data )
{
	static menu_array_values values;
	GtkWidget *item;

	values[MA_VTL]       = vtl;
	values[MA_WPT]       = wpt;
	values[MA_WPT_UUID]  = wpt_uuid;
	values[MA_VVP]       = vvp;
	values[MA_TREEVIEW]  = tree_view;
	values[MA_WPTS_LIST] = data;

	/*
	item = gtk_image_menu_item_new_with_mnemonic ( _("_Select") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_select), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	*/

	// AUTO SELECT NOT TRUE YET...
	// ATM view auto selects, so don't bother with separate select menu entry
	item = gtk_image_menu_item_new_with_mnemonic ( _("_View") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_view), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_PROPERTIES, NULL );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_properties), values );
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("_Show Picture...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Show Picture", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_show_picture), values );
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );
	gtk_widget_set_sensitive ( item, GPOINTER_TO_INT(wpt->image) );

	return TRUE;
}

static gboolean trw_layer_waypoint_menu_popup ( GtkWidget *tree_view,
                                                GdkEventButton *event,
                                                gpointer data )
{
	static GtkTreeIter iter;

	// Use selected item to get a single iterator ref
	// This relies on an row being selected as part of the right click
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(tree_view) );
	if ( gtk_tree_selection_count_selected_rows (selection) != 1 )
		return FALSE;

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

	VikWaypoint *wpt;
	gtk_tree_model_get ( model, &iter, WPT_COL_NUM, &wpt, -1 );
	if ( !wpt ) return FALSE;

	VikTrwLayer *vtl;
	gtk_tree_model_get ( model, &iter, TRW_COL_NUM, &vtl, -1 );
	if ( !IS_VIK_TRW_LAYER(vtl) ) return FALSE;

	wpu_udata udataU;
	udataU.wp   = wpt;
	udataU.uuid = NULL;

	gpointer *wptf;
	wptf = g_hash_table_find ( vik_trw_layer_get_waypoints(vtl), (GHRFunc) trw_layer_waypoint_find_uuid, &udataU );

	if ( wptf && udataU.uuid ) {
		VikViewport *vvp = vik_window_viewport((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)));

		GtkWidget *menu = gtk_menu_new();

		// Originally started to reuse the trw_layer menu items
		//  however these offer too many ways to edit the waypoint data
		//  so without an easy way to distinguish read only operations,
		//  create a very minimal new set of operations
		add_menu_items ( GTK_MENU(menu),
		                 vtl,
		                 wpt,
		                 udataU.uuid,
		                 vvp,
		                 tree_view,
		                 data );

		gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
		return TRUE;
	}
	return FALSE;
}

static gboolean trw_layer_waypoint_button_pressed ( GtkWidget *tree_view,
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
	return trw_layer_waypoint_menu_popup ( tree_view, event, data );
}

/*
 * Foreach entry we copy the various individual waypoint properties into the tree store
 *  formatting & converting the internal values into something for display
 */
static void trw_layer_waypoint_list_add ( vik_trw_waypoint_list_t *vtdl,
                                          GtkTreeStore *store,
                                          vik_units_distance_t dist_units,
                                          vik_units_height_t height_units )
{
	GtkTreeIter t_iter;
	VikWaypoint *wpt = vtdl->wpt;
	VikTrwLayer *vtl = vtdl->vtl;

	// Get start date
	gchar time_buf[32];
	time_buf[0] = '\0';
	if ( wpt->has_timestamp ) {

#if GLIB_CHECK_VERSION(2,26,0)
		GDateTime* gdt = g_date_time_new_from_unix_utc ( wpt->timestamp );
		gchar *time = g_date_time_format ( gdt, WAYPOINT_LIST_DATE_FORMAT );
		strncpy ( time_buf, time, sizeof(time_buf) );
		g_free ( time );
		g_date_time_unref ( gdt);
#else
		GDate* gdate_start = g_date_new ();
		g_date_set_time_t ( gdate_start, wpt->timestamp );
		g_date_strftime ( time_buf, sizeof(time_buf), WAYPOINT_LIST_DATE_FORMAT, gdate_start );
		g_date_free ( gdate_start );
#endif
	}

	// NB: doesn't include aggegrate visibility
	gboolean visible = VIK_LAYER(vtl)->visible && wpt->visible;
	visible = visible && vik_trw_layer_get_waypoints_visibility(vtl);

	gdouble alt = wpt->altitude;
	switch (height_units) {
	case VIK_UNITS_HEIGHT_FEET: alt = VIK_METERS_TO_FEET(alt); break;
	default:
		// VIK_UNITS_HEIGHT_METRES: no need to convert
		break;
	}

	gtk_tree_store_append ( store, &t_iter, NULL );
	gtk_tree_store_set ( store, &t_iter,
	                     0, VIK_LAYER(vtl)->name,
	                     1, wpt->name,
	                     2, time_buf,
	                     3, visible,
	                     4, wpt->comment,
	                     5, (gint)round(alt),
	                     6, get_wp_sym_small (wpt->symbol),
	                     TRW_COL_NUM, vtl,
	                     WPT_COL_NUM, wpt,
	                     -1 );
}

/*
 * Instead of comparing the pixbufs,
 *  look at the waypoint data and compare the symbol (as text).
 */
gint sort_pixbuf_compare_func ( GtkTreeModel *model,
                                GtkTreeIter  *a,
                                GtkTreeIter  *b,
                                gpointer      userdata )
{
	VikWaypoint *wpt1, *wpt2;
	gtk_tree_model_get ( model, a, WPT_COL_NUM, &wpt1, -1 );
	if ( !wpt1 ) return 0;
	gtk_tree_model_get ( model, b, WPT_COL_NUM, &wpt2, -1 );
	if ( !wpt2 ) return 0;

	return g_strcmp0 ( wpt1->symbol, wpt2->symbol );
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
 * vik_trw_layer_waypoint_list_internal:
 * @dialog:            The dialog to create the widgets in
 * @waypoints_and_layers: The list of waypoints (and it's layer) to be shown
 * @show_layer_names:  Show the layer names that each waypoint belongs to
 *
 * Create a table of waypoints with corresponding waypoint information
 * This table does not support being actively updated
 */
static void vik_trw_layer_waypoint_list_internal ( GtkWidget *dialog,
                                                   GList *waypoints_and_layers,
                                                   gboolean show_layer_names )
{
	if ( !waypoints_and_layers )
		return;

	// It's simple storing the gdouble values in the tree store as the sort works automatically
	// Then apply specific cell data formatting (rather default double is to 6 decimal places!)
	// However not storing any doubles for waypoints ATM
	// TODO: Consider adding the waypoint icon into this store for display in the list
	GtkTreeStore *store = gtk_tree_store_new ( WPT_LIST_COLS,
	                                           G_TYPE_STRING,    // 0: Layer Name
	                                           G_TYPE_STRING,    // 1: Waypoint Name
	                                           G_TYPE_STRING,    // 2: Date
	                                           G_TYPE_BOOLEAN,   // 3: Visible
	                                           G_TYPE_STRING,    // 4: Comment
	                                           G_TYPE_INT,       // 5: Height
	                                           GDK_TYPE_PIXBUF,  // 6: Symbol Icon
	                                           G_TYPE_POINTER,   // 7: TrackWaypoint Layer pointer
	                                           G_TYPE_POINTER ); // 8: Waypoint pointer

	//gtk_tree_selection_set_select_function ( gtk_tree_view_get_selection (GTK_TREE_VIEW(vt)), vik_treeview_selection_filter, vt, NULL );

	vik_units_distance_t dist_units = a_vik_get_units_distance ();
	vik_units_height_t height_units = a_vik_get_units_height ();

	//GList *gl = get_waypoints_and_layers_cb ( vl, user_data );
	//g_list_foreach ( waypoints_and_layers, (GFunc) trw_layer_waypoint_list_add, store );
	GList *gl = waypoints_and_layers;
	while ( gl ) {
		trw_layer_waypoint_list_add ( (vik_trw_waypoint_list_t*)gl->data, store, dist_units, height_units );
		gl = g_list_next ( gl );
	}

	GtkWidget *view = gtk_tree_view_new();
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set (G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	GtkTreeViewColumn *column;
	GtkTreeViewColumn *sort_by_column;

	gint column_runner = 0;
	if ( show_layer_names ) {
		// Insert column for the layer name when viewing multi layers
		column = my_new_column_text ( _("Layer"), renderer, view, column_runner++ );
		g_object_set (G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
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
	gtk_tree_view_column_set_resizable ( column, TRUE );

	GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes ( _("Visible"), renderer_toggle, "active", column_runner, NULL );
	gtk_tree_view_column_set_sort_column_id ( column, column_runner );
	gtk_tree_view_append_column ( GTK_TREE_VIEW(view), column );
	column_runner++;

	column = my_new_column_text ( _("Comment"), renderer, view, column_runner++ );
	gtk_tree_view_column_set_expand ( column, TRUE );

	if ( height_units == VIK_UNITS_HEIGHT_FEET )
		column = my_new_column_text ( _("Max Height\n(Feet)"), renderer, view, column_runner++ );
	else
		column = my_new_column_text ( _("Max Height\n(Metres)"), renderer, view, column_runner++ );

	GtkCellRenderer *renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
	g_object_set (G_OBJECT (renderer_pixbuf), "xalign", 0.5, NULL);
	column = gtk_tree_view_column_new_with_attributes ( _("Symbol"), renderer_pixbuf, "pixbuf", column_runner++, NULL );
	// Special sort required for pixbufs
	gtk_tree_sortable_set_sort_func ( GTK_TREE_SORTABLE(store), column_runner, sort_pixbuf_compare_func, NULL, NULL );
	gtk_tree_view_column_set_sort_column_id ( column, column_runner );
	gtk_tree_view_append_column ( GTK_TREE_VIEW(view), column );

	gtk_tree_view_set_model ( GTK_TREE_VIEW(view), GTK_TREE_MODEL(store) );
	gtk_tree_selection_set_mode ( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_BROWSE ); // GTK_SELECTION_MULTIPLE
	gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW(view), TRUE );

	g_object_unref(store);

	GtkWidget *scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_container_add ( GTK_CONTAINER(scrolledwindow), view );

	g_object_set ( view, "has-tooltip", TRUE, NULL);

	g_signal_connect ( view, "query-tooltip", G_CALLBACK (trw_layer_waypoint_tooltip_cb), NULL );
	//g_signal_connect ( gtk_tree_view_get_selection (GTK_TREE_VIEW(view)), "changed", G_CALLBACK(trw_layer_waypoint_select_cb), view );

	g_signal_connect ( view, "popup-menu", G_CALLBACK(trw_layer_waypoint_menu_popup), waypoints_and_layers );
	g_signal_connect ( view, "button-press-event", G_CALLBACK(trw_layer_waypoint_button_pressed), waypoints_and_layers );

	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, TRUE, TRUE, 0);

	// Set ordering of the initial view by one of the name columns
	gtk_tree_view_column_clicked ( sort_by_column );

	// Ensure a reasonable number of items are shown
	//  TODO: may be save window size, column order, sorted by between invocations.
	gtk_window_set_default_size ( GTK_WINDOW(dialog), show_layer_names ? 700 : 500, 400 );
}


/**
 * vik_trw_layer_waypoint_list_show_dialog:
 * @title:                    The title for the dialog
 * @vl:                       The #VikLayer passed on into get_waypoints_and_layers_cb()
 * @user_data:                Data passed on into get_waypoints_and_layers_cb()
 * @get_waypoints_and_layers_cb: The function to call to construct items to be analysed
 * @show_layer_names:         Normally only set when called from an aggregate level
 *
 * Common method for showing a list of waypoints with extended information
 *
 */
void vik_trw_layer_waypoint_list_show_dialog ( gchar *title,
                                               VikLayer *vl,
                                               gpointer user_data,
                                               VikTrwlayerGetWaypointsAndLayersFunc get_waypoints_and_layers_cb,
                                               gboolean show_layer_names )
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons ( title,
	                                                  VIK_GTK_WINDOW_FROM_LAYER(vl),
	                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
	                                                  GTK_STOCK_CLOSE,
	                                                  GTK_RESPONSE_CLOSE,
	                                                  NULL );

	GList *gl = get_waypoints_and_layers_cb ( vl, user_data );

	vik_trw_layer_waypoint_list_internal ( dialog, gl, show_layer_names );

	// Use response to close the dialog with tidy up
	g_signal_connect ( G_OBJECT(dialog), "response", G_CALLBACK(waypoint_close_cb), gl );

	gtk_widget_show_all ( dialog );
	// Yes - set the size *AGAIN* - this time widgets are expanded nicely
	gtk_window_resize ( GTK_WINDOW(dialog), show_layer_names ? 800 : 600, 400 );

	// ATM lock out on dialog run - to prevent list contents being manipulated in other parts of the GUI whilst shown here.
	gtk_dialog_run (GTK_DIALOG (dialog));
	// Unfortunately seems subsequently opening the Waypoint Properties we can't interact with it until this dialog is closed
	// Thus this dialog is then forcibly closed when opening the properties.

	// Occassionally the 'View' doesn't update the viewport properly
	//  viewport center + zoom is changed but the viewport isn't updated
	// not sure why yet..
}
