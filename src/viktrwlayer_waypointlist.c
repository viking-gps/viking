/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2020, Rob Norris <rw_norris@hotmail.com>
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
#include "viktrwlayer_waypointlist.h"
#include "viktrwlayer_wpwin.h"
#include "dem.h"

// Long formatted date+basic time - listing this way ensures the string comparison sort works - so no local type format %x or %c here!
#define WAYPOINT_LIST_DATE_FORMAT "%Y-%m-%d %H:%M"

// For simplicity these are global values
//  (i.e. would get shared between multiple windows)
static gint width = 0;
static gint height = 400;
static gboolean configured = FALSE;

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

static gboolean configure_event ( GtkWidget *widget, GdkEventConfigure *event, gpointer notused )
{
	if ( !configured ) {
		configured = TRUE;

		// Allow sizing back down to a minimum
		GdkGeometry geom = { event->width, event->height, 0, 0, 0, 0, 0, 0, 0, 0, GDK_GRAVITY_STATIC };
		gtk_window_set_geometry_hints ( GTK_WINDOW(widget), widget, &geom, GDK_HINT_BASE_SIZE );

		// Restore previous size
		gtk_window_resize ( GTK_WINDOW(widget), width, height );
	} else {
		width = event->width;
		height = event->height;
	}
	return FALSE;
}

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

		trw_layer_wpwin_set ( vtl, wpt, vik_trw_layer_wpwin_show ( VIK_GTK_WINDOW_FROM_LAYER(vtl), NULL, wpt->name, vtl, wpt, vik_trw_layer_get_coord_mode(vtl), FALSE ) );
	}
}

static void trw_layer_waypoint_view ( menu_array_values values )
{
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
	VikWaypoint *wpt = VIK_WAYPOINT(values[MA_WPT]);
	VikViewport *vvp = VIK_VIEWPORT(values[MA_VVP]);

	vik_viewport_set_center_coord ( vvp, &(wpt->coord), TRUE );

	trw_layer_waypoint_select (values);

	vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_show_picture ( menu_array_values values )
{
	VikWaypoint *wpt = VIK_WAYPOINT(values[MA_WPT]);
#ifdef WINDOWS
	ShellExecute(NULL, "open", wpt->image, NULL, NULL, SW_SHOWNORMAL);
#else
	VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
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


typedef struct {
  gboolean has_layer_names;
  gboolean include_positions;
  GString *str;
} copy_data_t;

/**
 * At the moment allow copying the data displayed** with or without the positions
 *  (since the position data is not shown in the list but is useful in copying to external apps)
 *
 * ** ATM The visibility flag is not copied and neither is a text representation of the waypoint symbol
 */
static void copy_selection (GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data)
{
	copy_data_t *cd = (copy_data_t*) data;

	gchar* layername; gtk_tree_model_get ( model, iter, 0, &layername, -1 );
	gchar* name; gtk_tree_model_get ( model, iter, 1, &name, -1 );
	gchar* date; gtk_tree_model_get ( model, iter, 2, &date, -1 );
	gchar* comment; gtk_tree_model_get ( model, iter, 4, &comment, -1 );
	if ( comment == NULL )
		comment = g_strdup ( "" );
	gint hh; gtk_tree_model_get ( model, iter, 5, &hh, -1 );

	VikWaypoint *wpt; gtk_tree_model_get ( model, iter, WPT_COL_NUM, &wpt, -1 );
	struct LatLon ll;
	if ( wpt ) {
		vik_coord_to_latlon ( &wpt->coord, &ll );
	}
	gchar sep = '\t'; // Could make this configurable - but simply always make it a tab character for now
	// NB Even if the columns have been reordered - this copies it out only in the original default order
	// if col 0 is displayed then also copy the layername
	// Note that the lat/lon data copy is using the users locale
	if ( cd->has_layer_names ) {
		if ( cd->include_positions )
			g_string_append_printf ( cd->str, "%s%c%s%c%s%c%s%c%d%c%.6f%c%.6f\n", layername, sep, name, sep, date, sep, comment, sep, hh, sep, ll.lat, sep, ll.lon );
		else
			g_string_append_printf ( cd->str, "%s%c%s%c%s%c%s%c%d\n", layername, sep, name, sep, date, sep, comment, sep, hh );
	}
	else {
		if ( cd->include_positions )
			g_string_append_printf ( cd->str, "%s%c%s%c%s%c%d%c%.6f%c%.6f\n", name, sep, date, sep, comment, sep, hh, sep, ll.lat, sep, ll.lon );
		else
			g_string_append_printf ( cd->str, "%s%c%s%c%s%c%d\n", name, sep, date, sep, comment, sep, hh );
	}
	g_free ( layername );
	g_free ( name );
	g_free ( date );
	g_free ( comment );
}

static void trw_layer_copy_selected ( GtkWidget *tree_view, gboolean include_positions )
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(tree_view) );
	// NB GTK3 has gtk_tree_view_get_n_columns() but we're GTK2 ATM
	GList *gl = gtk_tree_view_get_columns ( GTK_TREE_VIEW(tree_view) );
	guint count = g_list_length ( gl );
	g_list_free ( gl );
	copy_data_t cd;
	cd.has_layer_names = (count > WPT_LIST_COLS-3);
	cd.str = g_string_new ( NULL );
	cd.include_positions = include_positions;
	gtk_tree_selection_selected_foreach ( selection, copy_selection, &cd );

	a_clipboard_copy ( VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, cd.str->str, NULL );

	g_string_free ( cd.str, TRUE );
}

static void trw_layer_copy_selected_only_visible_columns ( GtkWidget *tree_view )
{
	trw_layer_copy_selected ( tree_view, FALSE );
}

static void trw_layer_copy_selected_with_position ( GtkWidget *tree_view )
{
	trw_layer_copy_selected ( tree_view, TRUE );
}

static void add_copy_menu_items ( GtkMenu *menu, GtkWidget *tree_view )
{
	(void)vu_menu_add_item ( menu, _("_Copy Data"), GTK_STOCK_COPY,
	                         G_CALLBACK(trw_layer_copy_selected_only_visible_columns), tree_view );
	(void)vu_menu_add_item ( menu, _("Copy Data (with _positions)"), GTK_STOCK_COPY,
	                         G_CALLBACK(trw_layer_copy_selected_with_position), tree_view );
}

static gboolean add_menu_items ( GtkMenu *menu, VikTrwLayer *vtl, VikWaypoint *wpt, gpointer wpt_uuid, VikViewport *vvp, GtkWidget *tree_view, gpointer data )
{
	static menu_array_values values;
	values[MA_VTL]       = vtl;
	values[MA_WPT]       = wpt;
	values[MA_WPT_UUID]  = wpt_uuid;
	values[MA_VVP]       = vvp;
	values[MA_TREEVIEW]  = tree_view;
	values[MA_WPTS_LIST] = data;

	//(void)vu_menu_add_item ( menu, _("_Select"), GTK_STOCK_FIND, G_CALLBACK(trw_layer_waypoint_select), values );
	// AUTO SELECT NOT TRUE YET...
	// ATM view auto selects, so don't bother with separate select menu entry

	(void)vu_menu_add_item ( menu, _("_View"), GTK_STOCK_ZOOM_FIT, G_CALLBACK(trw_layer_waypoint_view), values );
	(void)vu_menu_add_item ( menu, NULL, GTK_STOCK_PROPERTIES, G_CALLBACK(trw_layer_waypoint_properties), values );
	// Own icon - see stock_icons in vikwindow.c
	GtkWidget *item = vu_menu_add_item ( menu, _("_Show Picture..."), VIK_ICON_SHOW_PICTURE, G_CALLBACK(trw_layer_show_picture), values );
	gtk_widget_set_sensitive ( item, GPOINTER_TO_INT(wpt->image) );

	add_copy_menu_items ( menu, tree_view );

	return TRUE;
}

static gboolean trw_layer_waypoint_menu_popup_multi  ( GtkWidget *tree_view,
                                                       GdkEventButton *event,
                                                       gpointer data )
{
	GtkWidget *menu = gtk_menu_new();

	add_copy_menu_items ( GTK_MENU(menu), tree_view );
	gtk_widget_show_all ( menu );
	gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );

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
		return trw_layer_waypoint_menu_popup_multi ( tree_view, event, data );

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

	gpointer wptf;
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

		gtk_widget_show_all ( menu );
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
                                          vik_units_height_t height_units,
                                          const gchar* date_format )
{
	GtkTreeIter t_iter;
	VikWaypoint *wpt = vtdl->wpt;
	VikTrwLayer *vtl = vtdl->vtl;

	// Get start date
	gchar time_buf[32];
	time_buf[0] = '\0';
	if ( !isnan(wpt->timestamp) ) {
		time_t tt = wpt->timestamp;
		gchar *time = vu_get_time_string ( &tt, date_format, &wpt->coord, NULL );
		g_strlcpy ( time_buf, time, sizeof(time_buf) );
		g_free ( time );
	}

	gboolean visible = wpt->visible && vik_trw_layer_get_waypoints_visibility ( vtl );
	visible = visible && vik_treeview_item_get_visible_tree ( VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter) );

	gdouble alt = wpt->altitude;
	if ( isnan(alt) ) {
		alt = VIK_DEM_INVALID_ELEVATION;
	} else {
		switch (height_units) {
		case VIK_UNITS_HEIGHT_FEET: alt = VIK_METERS_TO_FEET(alt); break;
		default:
			// VIK_UNITS_HEIGHT_METRES: no need to convert
			break;
		}
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

static gboolean
filter_waypoints_cb (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	const gchar *filter = gtk_entry_get_text ( GTK_ENTRY(data) );

	if ( filter == NULL || strlen ( filter ) == 0 )
		return TRUE;

	gchar *filter_case = g_utf8_casefold ( filter, -1 );

	VikWaypoint *wpt;
	gtk_tree_model_get ( model, iter, WPT_COL_NUM, &wpt, -1 );

	if ( wpt == NULL )
		return FALSE;

	gboolean visible = FALSE;

	gchar *layer_name;
	gtk_tree_model_get ( model, iter, 0, &layer_name, -1 );
	if ( layer_name != NULL ) {
		gchar *layer_name_case = g_utf8_casefold ( layer_name, -1 );
		visible |= ( strstr ( layer_name_case, filter_case ) != NULL );
		g_free ( layer_name_case );
	}
	g_free ( layer_name );

	gchar *wpt_name_case = g_utf8_casefold ( wpt->name, -1 );
	visible |= ( strstr ( wpt_name_case, filter_case ) != NULL );
	g_free ( wpt_name_case );

	gchar *time_buf;
	gtk_tree_model_get ( model, iter, 2, &time_buf, -1 );
	visible |= (time_buf != NULL && strstr ( time_buf, filter_case )  != NULL);
	g_free ( time_buf );

	if ( wpt->comment != NULL ) {
		gchar *wpt_comment_case = g_utf8_casefold ( wpt->comment, -1 );
		visible |= ( strstr ( wpt_comment_case, filter_case ) != NULL );
		g_free ( wpt_comment_case );
	}

	if ( wpt->description != NULL ) {
		gchar *wpt_description_case = g_utf8_casefold ( wpt->description, -1 );
		visible |= ( strstr ( wpt_description_case, filter_case ) != NULL );
		g_free ( wpt_description_case );
	}

	g_free ( filter_case );

	return visible;
}

static void
filter_changed_cb (GtkEntry   *entry,
                   GParamSpec *pspec,
                   gpointer   data)
{
    gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER(data) );
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

/**
 * format_elev_cell_data_func
 *
 * Integer display handling invalid/undefined elevation values
 *
 */
static void format_elev_cell_data_func ( GtkTreeViewColumn *col,
                                         GtkCellRenderer   *renderer,
                                         GtkTreeModel      *model,
                                         GtkTreeIter       *iter,
                                         gpointer           user_data )
{
	gint value;
	gchar buf[20];
	gint column = GPOINTER_TO_INT(user_data);
	gtk_tree_model_get ( model, iter, column, &value, -1 );
	if ( value == VIK_DEM_INVALID_ELEVATION )
		g_snprintf ( buf, sizeof(buf), "--" );
	else
		g_snprintf ( buf, sizeof(buf), "%d", value );
	g_object_set ( renderer, "text", buf, NULL );
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

	vik_units_height_t height_units = a_vik_get_units_height ();

	//GList *gl = get_waypoints_and_layers_cb ( vl, user_data );
	//g_list_foreach ( waypoints_and_layers, (GFunc) trw_layer_waypoint_list_add, store );
	gchar *date_format = NULL;
	if ( !a_settings_get_string ( VIK_SETTINGS_LIST_DATE_FORMAT, &date_format ) )
		date_format = g_strdup ( WAYPOINT_LIST_DATE_FORMAT );

	GList *gl = waypoints_and_layers;
	while ( gl ) {
		trw_layer_waypoint_list_add ( (vik_trw_waypoint_list_t*)gl->data, store, height_units, date_format );
		gl = g_list_next ( gl );
	}
	g_free ( date_format );

	GtkWidget *view = gtk_tree_view_new();
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set (G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	GtkTreeViewColumn *column;
	GtkTreeViewColumn *sort_by_column;

	gint column_runner = 0;
	if ( show_layer_names ) {
		// Insert column for the layer name when viewing multi layers
		column = ui_new_column_text ( _("Layer"), renderer, view, column_runner++ );
		g_object_set (G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
		gtk_tree_view_column_set_expand ( column, TRUE );
		// remember the layer column so we can sort by it later
		sort_by_column = column;
	}
	else
		column_runner++;

	column = ui_new_column_text ( _("Name"), renderer, view, column_runner++ );
	gtk_tree_view_column_set_expand ( column, TRUE );
	if ( !show_layer_names )
		// remember the name column so we can sort by it later
		sort_by_column = column;

	column = ui_new_column_text ( _("Date"), renderer, view, column_runner++ );
	gtk_tree_view_column_set_resizable ( column, TRUE );

	GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes ( _("Visible"), renderer_toggle, "active", column_runner, NULL );
	gtk_tree_view_column_set_sort_column_id ( column, column_runner );
	gtk_tree_view_append_column ( GTK_TREE_VIEW(view), column );
	column_runner++;

	column = ui_new_column_text ( _("Comment"), renderer, view, column_runner++ );
	gtk_tree_view_column_set_expand ( column, TRUE );

	if ( height_units == VIK_UNITS_HEIGHT_FEET )
		column = ui_new_column_text ( _("Max Height\n(Feet)"), renderer, view, column_runner++ );
	else
		column = ui_new_column_text ( _("Max Height\n(Metres)"), renderer, view, column_runner++ );
	// Apply own formatting of the data
	gtk_tree_view_column_set_cell_data_func ( column, renderer, format_elev_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL);


	GtkCellRenderer *renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
	g_object_set (G_OBJECT (renderer_pixbuf), "xalign", 0.5, NULL);
	column = gtk_tree_view_column_new_with_attributes ( _("Symbol"), renderer_pixbuf, "pixbuf", column_runner++, NULL );
	// Special sort required for pixbufs
	gtk_tree_sortable_set_sort_func ( GTK_TREE_SORTABLE(store), column_runner, sort_pixbuf_compare_func, NULL, NULL );
	gtk_tree_view_column_set_sort_column_id ( column, column_runner );
	gtk_tree_view_append_column ( GTK_TREE_VIEW(view), column );

	GtkTreeModelFilter *model = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new ( GTK_TREE_MODEL(store), NULL));
	GtkTreeModelSort *sorted = GTK_TREE_MODEL_SORT(gtk_tree_model_sort_new_with_model ( GTK_TREE_MODEL(model) ));

	gtk_tree_view_set_model ( GTK_TREE_VIEW(view), GTK_TREE_MODEL(sorted) );
	gtk_tree_selection_set_mode ( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE );
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

	GtkWidget *filter_entry = ui_entry_new( NULL, GTK_ENTRY_ICON_SECONDARY );
	g_signal_connect ( filter_entry, "notify::text", G_CALLBACK(filter_changed_cb), model );
	gtk_tree_model_filter_set_visible_func ( model, filter_waypoints_cb, filter_entry, NULL );

	GtkWidget *filter_label = gtk_label_new ( _("Filter") );
	g_object_set ( filter_label, "has-tooltip", TRUE, NULL );
	g_signal_connect ( filter_label, "query-tooltip", G_CALLBACK(ui_tree_model_number_tooltip_cb), model );

	GtkWidget *filter_box = gtk_hbox_new( FALSE, 10 );
	gtk_box_pack_start (GTK_BOX(filter_box), filter_label, FALSE, TRUE, 10);
	gtk_box_pack_start (GTK_BOX(filter_box), filter_entry, TRUE, TRUE, 10);

	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX(GTK_DIALOG(dialog)->vbox), filter_box, FALSE, TRUE, 10);

	// Set ordering of the initial view by one of the name columns
	gtk_tree_view_column_clicked ( sort_by_column );
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

	configured = FALSE;
	g_signal_connect ( G_OBJECT(dialog), "configure-event", G_CALLBACK(configure_event), NULL );

	gtk_widget_show_all ( dialog );

	// Ensure a reasonable number of items are shown
	if ( width == 0 )
		width = show_layer_names ? 800 : 650;

	// ATM lock out on dialog run - to prevent list contents being manipulated in other parts of the GUI whilst shown here.
	gtk_dialog_run (GTK_DIALOG (dialog));
	// Unfortunately seems subsequently opening the Waypoint Properties we can't interact with it until this dialog is closed
	// Thus this dialog is then forcibly closed when opening the properties.

	// Occassionally the 'View' doesn't update the viewport properly
	//  viewport center + zoom is changed but the viewport isn't updated
	// not sure why yet..
}
