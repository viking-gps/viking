/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2018, Rob Norris <rw_norris@hotmail.com>
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
#include <gdk/gdkkeysyms.h>

enum {
  VLP_UPDATE_SIGNAL,
  VLP_DELETE_LAYER_SIGNAL,
  VLP_LAST_SIGNAL
};

// Calendar markup
typedef enum {
  VLP_CAL_MU_NONE = 0,
  VLP_CAL_MU_MARKED,
  VLP_CAL_MU_DETAIL,
  VLP_CAL_MU_AUTO,
  VLP_CAL_MU_LAST,
} calendar_mu_t;

static guint layers_panel_signals[VLP_LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;

struct _VikLayersPanel {
  GtkVBox vbox;
  GtkWidget *hbox;
  GtkWidget *calendar;
  // Could use gtk_widget_get_visible () instead, but needs Gtk2.18
  // ATM still using Gtk2.16 - so manually track the state for now
  // NB Doesn't consider if the layers panel is shown or not.
  gboolean cal_shown;
  calendar_mu_t cal_markup;

  VikAggregateLayer *toplayer;
  GtkTreeIter toplayer_iter;

  VikTreeview *vt;
  VikViewport *vvp; /* reference */
};

static GtkActionEntry entries[] = {
  { "Cut",    GTK_STOCK_CUT,    N_("C_ut"),       NULL, NULL, (GCallback) vik_layers_panel_cut_selected },
  { "Copy",   GTK_STOCK_COPY,   N_("_Copy"),      NULL, NULL, (GCallback) vik_layers_panel_copy_selected },
  { "Paste",  GTK_STOCK_PASTE,  N_("_Paste"),     NULL, NULL, (GCallback) vik_layers_panel_paste_selected },
  { "Delete", GTK_STOCK_DELETE, N_("_Delete"),    NULL, NULL, (GCallback) vik_layers_panel_delete_selected },
};

static void layers_item_toggled (VikLayersPanel *vlp, GtkTreeIter *iter);
static void layers_item_edited (VikLayersPanel *vlp, GtkTreeIter *iter, const gchar *new_text);
static void menu_popup_cb (VikLayersPanel *vlp);
static void layers_popup_cb (VikLayersPanel *vlp);
static void layers_popup ( VikLayersPanel *vlp, GtkTreeIter *iter, gint mouse_button );
static gboolean layers_button_press_cb (VikLayersPanel *vlp, GdkEventButton *event);
static gboolean layers_key_press_cb (VikLayersPanel *vlp, GdkEventKey *event);
static void layers_move_item ( VikLayersPanel *vlp, gboolean up );
static void layers_move_item_up ( VikLayersPanel *vlp );
static void layers_move_item_down ( VikLayersPanel *vlp );
static void layers_panel_finalize ( GObject *gob );

G_DEFINE_TYPE (VikLayersPanel, vik_layers_panel, GTK_TYPE_VBOX)

static void vik_layers_panel_class_init ( VikLayersPanelClass *klass )
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = layers_panel_finalize;

  parent_class = g_type_class_peek_parent (klass);

  layers_panel_signals[VLP_UPDATE_SIGNAL] = g_signal_new ( "update", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikLayersPanelClass, update), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  layers_panel_signals[VLP_DELETE_LAYER_SIGNAL] = g_signal_new ( "delete_layer", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikLayersPanelClass, update), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  }

VikLayersPanel *vik_layers_panel_new ()
{
  return VIK_LAYERS_PANEL ( g_object_new ( VIK_LAYERS_PANEL_TYPE, NULL ) );
}

void vik_layers_panel_set_viewport ( VikLayersPanel *vlp, VikViewport *vvp )
{
  vlp->vvp = vvp;
  /* TODO: also update GCs (?) */
}

VikViewport *vik_layers_panel_get_viewport ( VikLayersPanel *vlp )
{
  return vlp->vvp;
}

static gboolean layers_panel_new_layer ( gpointer lpnl[2] )
{
  return vik_layers_panel_new_layer ( lpnl[0], GPOINTER_TO_INT(lpnl[1]) );
}

/**
 * Create menu popup on demand
 * @full: offer cut/copy options as well - not just the new layer options
 */
static GtkWidget* layers_panel_create_popup ( VikLayersPanel *vlp, gboolean full )
{
  GtkWidget *menu = gtk_menu_new ();
  GtkWidget *menuitem;
  guint ii;

  if ( full ) {
    (void)vu_menu_add_item ( GTK_MENU(menu), NULL, GTK_STOCK_PROPERTIES, G_CALLBACK(vik_layers_panel_properties), vlp );
    for ( ii = 0; ii < G_N_ELEMENTS(entries); ii++ ) {
      (void)vu_menu_add_item ( GTK_MENU(menu), entries[ii].label, entries[ii].stock_id, G_CALLBACK(entries[ii].callback), vlp );
    }
  }

  GtkWidget *submenu = gtk_menu_new();
  menuitem = vu_menu_add_item ( GTK_MENU(menu), _("New Layer"), GTK_STOCK_NEW, NULL, NULL );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu );

  // Static: so memory accessible yet not continually allocated
  static gpointer lpnl[VIK_LAYER_NUM_TYPES][2];

  for ( ii = 0; ii < VIK_LAYER_NUM_TYPES; ii++ ) {

    if ( vik_layer_get_interface(ii)->icon ) {
      menuitem = gtk_image_menu_item_new_with_mnemonic ( _(vik_layer_get_interface(ii)->name) );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)menuitem, gtk_image_new_from_pixbuf ( vik_layer_load_icon (ii) ) );
    }
    else
      menuitem = gtk_menu_item_new_with_mnemonic ( _(vik_layer_get_interface(ii)->name) );

    lpnl[ii][0] = vlp;
    lpnl[ii][1] = GINT_TO_POINTER(ii);

    g_signal_connect_swapped ( G_OBJECT(menuitem), "activate", G_CALLBACK(layers_panel_new_layer), lpnl[ii] );
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
  }

  gtk_widget_show_all ( menu );
  return menu;
}

static void calendar_mark_layer_in_month ( VikLayersPanel *vlp, VikTrwLayer *vtl )
{
  guint year, month, day;
  gtk_calendar_get_date ( GTK_CALENDAR(vlp->calendar), &year, &month, &day );
  GDate *gd = g_date_new();
  GHashTable *trks = vik_trw_layer_get_tracks ( vtl ); 
  GHashTableIter iter;
  gpointer key, value;
  // Foreach Track
  g_hash_table_iter_init ( &iter, trks );
  while ( g_hash_table_iter_next ( &iter, &key, &value ) ) {
    VikTrack *trk = VIK_TRACK(value);
    // First trackpoint
    if ( trk->trackpoints && !isnan(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp) ) {
      // Not worried about subsecond resolution here!
      g_date_set_time_t ( gd, (time_t)VIK_TRACKPOINT(trk->trackpoints->data)->timestamp );
      // Is in selected month?
      if ( g_date_get_year(gd) == year && (g_date_get_month(gd) == (month+1)) ) {
	gtk_calendar_mark_day ( GTK_CALENDAR(vlp->calendar), g_date_get_day(gd) );
	break;
      }
    }
  }
  g_date_free ( gd );
}

/**
 *
 */
void layers_panel_calendar_update ( VikLayersPanel *vlp )
{
  // Skip if not shown...
  if ( !vlp->cal_shown )
    return;

  gtk_calendar_clear_marks ( GTK_CALENDAR(vlp->calendar) );

  GList *layers = vik_layers_panel_get_all_layers_of_type ( vlp, VIK_LAYER_TRW, TRUE );
  if ( !layers )
    return;
  
  while ( layers ) {
    VikTrwLayer *vtl = VIK_TRW_LAYER(layers->data);
    calendar_mark_layer_in_month ( vlp, vtl );
    layers = g_list_next ( layers );
  }
  g_list_free ( layers );    
}

/**
 *
 */
void vik_layers_panel_calendar_update ( VikLayersPanel *vlp )
{
  if ( vlp->cal_markup == VLP_CAL_MU_NONE )
    return;

  clock_t begin = clock();
  layers_panel_calendar_update ( vlp );
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f", __FUNCTION__, time_spent );
  // Downgrade automatic detail if taking too long
  // Since ATM the detail method invokes x30 searches,
  //  it could get noticably slow
  if ( vlp->cal_markup == VLP_CAL_MU_AUTO )
    if ( time_spent > (0.5/30.0) )
      vlp->cal_markup = VLP_CAL_MU_MARKED;
}

static void layers_calendar_day_selected_dc_cb ( VikLayersPanel *vlp )
{
  // Ideally only need to search if the day is marked...
  guint year, month, day;
  gtk_calendar_get_date ( GTK_CALENDAR(vlp->calendar), &year, &month, &day );
  gchar *date_str = g_strdup_printf ( "%d-%02d-%02d", year, month+1, day );
  (void)vik_aggregate_layer_search_date ( vlp->toplayer, date_str );
  g_free ( date_str );
}

/**
 *
 */
gboolean layers_calendar_forward ( VikLayersPanel *vlp )
{
  guint year, month, day;
  gtk_calendar_get_date ( GTK_CALENDAR(vlp->calendar), &year, &month, &day );
  // Limit the search into the future
  time_t now = time (NULL);
  if (now == (time_t) -1) {
    g_critical ( "%s: current time unavailable", __FUNCTION__ );
    return TRUE;
  }
  GDate *gd_now = g_date_new ();
  g_date_set_time_t ( gd_now, now );

  GDate *gd = g_date_new_dmy ( day, month+1, year );
  gboolean found = FALSE;
  gchar *date_str = NULL;
  while ( !found && (g_date_compare(gd, gd_now) < 0) ) {
    // Free previous allocation
    g_free ( date_str );
    // Search on the next day
    g_date_add_days ( gd, 1 );
    year = g_date_get_year ( gd );
    month = g_date_get_month ( gd );
    day = g_date_get_day ( gd );
    date_str = g_strdup_printf ( "%d-%02d-%02d", year, month, day );
    found = vik_aggregate_layer_search_date ( vlp->toplayer, date_str );
  }
  if ( found ) {
    gtk_calendar_select_day ( GTK_CALENDAR(vlp->calendar), day );
    gtk_calendar_select_month ( GTK_CALENDAR(vlp->calendar), month-1, year );
  } else {
    if ( !date_str ) {
      date_str = g_malloc0_n ( sizeof(gchar*), 64 );
      (void)g_date_strftime ( date_str, 64, "%x", gd_now );
    }
    a_dialog_info_msg_extra ( GTK_WINDOW(VIK_WINDOW_FROM_WIDGET(vlp)), _("Nothing found up to %s"), date_str );
  }
  g_debug ( "%s: %s %d", __FUNCTION__, date_str, found );
  g_free ( date_str );
  g_date_free ( gd_now );
  g_date_free ( gd );
  return TRUE;
}

/**
 *
 */
gboolean layers_calendar_back ( VikLayersPanel *vlp )
{
  guint year, month, day;
  gtk_calendar_get_date ( GTK_CALENDAR(vlp->calendar), &year, &month, &day );

  GDate *gd = g_date_new_dmy ( day, month+1, year );
  gboolean found = FALSE;
  const guint limit = 3650; // Search up to 10 years ago from selected date
  guint ii = 0;
  gchar *date_str = NULL;
  while ( !found && ii < limit ) {
    // Free previous allocation
    g_free ( date_str );
    // Search on the previous day
    g_date_subtract_days ( gd, 1 );
    year = g_date_get_year ( gd );
    month = g_date_get_month ( gd );
    day = g_date_get_day ( gd );
    date_str = g_strdup_printf ( "%d-%02d-%02d", year, month, day );
    found = vik_aggregate_layer_search_date ( vlp->toplayer, date_str );
    ii++;
  }
  if ( found ) {
    gtk_calendar_select_day ( GTK_CALENDAR(vlp->calendar), day );
    gtk_calendar_select_month ( GTK_CALENDAR(vlp->calendar), month-1, year );
  } else {
    a_dialog_info_msg_extra ( GTK_WINDOW(VIK_WINDOW_FROM_WIDGET(vlp)), _("Nothing found back to %s"), date_str );
  }
  g_debug ( "%s: %s %d", __FUNCTION__, date_str, found );
  g_free ( date_str );
  g_date_free ( gd );
  return TRUE;
}

/**
 *
 */
gboolean layers_calendar_today ( VikLayersPanel *vlp )
{
  vu_calendar_set_to_today ( vlp->calendar );
  return TRUE;
}

/**
 * Enable a right click menu on the calendar
 */
static gboolean layers_calendar_button_press_cb ( VikLayersPanel *vlp, GdkEventButton *event )
{
  if ( event->button == 3 ) {
    GtkMenu *menu = GTK_MENU ( gtk_menu_new () );
    (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_GO_BACK, G_CALLBACK(layers_calendar_back), vlp );
    (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_GO_FORWARD, G_CALLBACK(layers_calendar_forward), vlp );
    (void)vu_menu_add_item ( menu, _("_Today"), GTK_STOCK_HOME, G_CALLBACK(layers_calendar_today), vlp );
    gtk_widget_show_all ( GTK_WIDGET(menu) );
    gtk_menu_popup ( menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
    return TRUE;
  }
  return FALSE;
}

static gchar *calendar_detail ( GtkCalendar *calendar,
                                guint year,
                                guint month,
                                guint day,
                                gpointer user_data )
{
  VikLayersPanel *vlp = (VikLayersPanel*)user_data;
  // Skip if not shown...
  if ( !vlp->cal_shown )
    return NULL;

  // Check that detail is wanted
  if ( vlp->cal_markup < VLP_CAL_MU_DETAIL )
    return NULL;

  guint ayear, amonth, aday;
  gtk_calendar_get_date ( GTK_CALENDAR(vlp->calendar), &ayear, &amonth, &aday );

  // Only bother for this actual month
  // As this event gets fired for the surrounding days too
  if ( amonth != month )
    return NULL;

  GList *layers = vik_layers_panel_get_all_layers_of_type ( vlp, VIK_LAYER_TRW, TRUE );
  if ( !layers )
    return NULL;

  gboolean need_to_break = FALSE;
  VikTrwLayer *vtl = NULL;
  GDate *gd = g_date_new();
  while ( layers ) {
    vtl = VIK_TRW_LAYER(layers->data);

    GHashTable *trks = vik_trw_layer_get_tracks ( vtl ); 
    GHashTableIter iter;
    gpointer key, value;
    // Foreach Track
    g_hash_table_iter_init ( &iter, trks );
    while ( g_hash_table_iter_next ( &iter, &key, &value ) ) {
      VikTrack *trk = VIK_TRACK(value);
      // First trackpoint
      if ( trk->trackpoints && !isnan(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp) ) {
        // Not worried about subsecond resolution here!
        g_date_set_time_t ( gd, (time_t)VIK_TRACKPOINT(trk->trackpoints->data)->timestamp );
        // Is of this day?
        if ( g_date_get_year(gd) == year &&
             g_date_get_month(gd) == (month+1) &&
             g_date_get_day(gd) == day ) {
          need_to_break = TRUE;
	  break;
	}
      }
    }
    // Fully exit
    if ( need_to_break )
      break;
    
    layers = g_list_next ( layers );
    vtl = NULL;
  }
  g_date_free ( gd );
  g_list_free ( layers );    

  if ( vtl)
    return g_strdup (vik_layer_get_name ( VIK_LAYER(vtl) ));

  return NULL;
}

#define VIK_SETTINGS_CAL_MUM "layers_panel_calendar_markup_mode"

static void vik_layers_panel_init ( VikLayersPanel *vlp )
{
  GtkWidget *addbutton, *addimage;
  GtkWidget *removebutton, *removeimage;
  GtkWidget *upbutton, *upimage;
  GtkWidget *downbutton, *downimage;
  GtkWidget *cutbutton, *cutimage;
  GtkWidget *copybutton, *copyimage;
  GtkWidget *pastebutton, *pasteimage;
  GtkWidget *scrolledwindow;

  vlp->vvp = NULL;

  vlp->hbox = gtk_hbox_new ( TRUE, 2 );
  vlp->vt = vik_treeview_new ( );

  vlp->toplayer = vik_aggregate_layer_new ();
  vik_layer_rename ( VIK_LAYER(vlp->toplayer), _("Top Layer"));
  g_signal_connect_swapped ( G_OBJECT(vlp->toplayer), "update", G_CALLBACK(vik_layers_panel_emit_update), vlp );

  vik_treeview_add_layer ( vlp->vt, NULL, &(vlp->toplayer_iter), VIK_LAYER(vlp->toplayer)->name, NULL, TRUE, vlp->toplayer, VIK_LAYER_AGGREGATE, VIK_LAYER_AGGREGATE, 0 );
  vik_layer_realize ( VIK_LAYER(vlp->toplayer), vlp->vt, &(vlp->toplayer_iter) );

  g_signal_connect_swapped ( vlp->vt, "popup_menu", G_CALLBACK(menu_popup_cb), vlp);
  g_signal_connect_swapped ( vlp->vt, "button_press_event", G_CALLBACK(layers_button_press_cb), vlp);
  g_signal_connect_swapped ( vlp->vt, "item_toggled", G_CALLBACK(layers_item_toggled), vlp);
  g_signal_connect_swapped ( vlp->vt, "item_edited", G_CALLBACK(layers_item_edited), vlp);
  g_signal_connect_swapped ( vlp->vt, "key_press_event", G_CALLBACK(layers_key_press_cb), vlp);

  /* Add button */
  addimage = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_SMALL_TOOLBAR );
  addbutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(addbutton), addimage );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(addbutton), _("Add new layer"));
  gtk_box_pack_start ( GTK_BOX(vlp->hbox), addbutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(addbutton), "clicked", G_CALLBACK(layers_popup_cb), vlp );
  /* Remove button */
  removeimage = gtk_image_new_from_stock ( GTK_STOCK_REMOVE, GTK_ICON_SIZE_SMALL_TOOLBAR );
  removebutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(removebutton), removeimage );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(removebutton), _("Remove selected layer"));
  gtk_box_pack_start ( GTK_BOX(vlp->hbox), removebutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(removebutton), "clicked", G_CALLBACK(vik_layers_panel_delete_selected), vlp );
  /* Up button */
  upimage = gtk_image_new_from_stock ( GTK_STOCK_GO_UP, GTK_ICON_SIZE_SMALL_TOOLBAR );
  upbutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(upbutton), upimage );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(upbutton), _("Move selected layer up"));
  gtk_box_pack_start ( GTK_BOX(vlp->hbox), upbutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(upbutton), "clicked", G_CALLBACK(layers_move_item_up), vlp );
  /* Down button */
  downimage = gtk_image_new_from_stock ( GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_SMALL_TOOLBAR );
  downbutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(downbutton), downimage );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(downbutton), _("Move selected layer down"));
  gtk_box_pack_start ( GTK_BOX(vlp->hbox), downbutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(downbutton), "clicked", G_CALLBACK(layers_move_item_down), vlp );
  /* Cut button */
  cutimage = gtk_image_new_from_stock ( GTK_STOCK_CUT, GTK_ICON_SIZE_SMALL_TOOLBAR );
  cutbutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(cutbutton), cutimage );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(cutbutton), _("Cut selected layer"));
  gtk_box_pack_start ( GTK_BOX(vlp->hbox), cutbutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(cutbutton), "clicked", G_CALLBACK(vik_layers_panel_cut_selected), vlp );
  /* Copy button */
  copyimage = gtk_image_new_from_stock ( GTK_STOCK_COPY, GTK_ICON_SIZE_SMALL_TOOLBAR );
  copybutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(copybutton), copyimage );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(copybutton), _("Copy selected layer"));
  gtk_box_pack_start ( GTK_BOX(vlp->hbox), copybutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(copybutton), "clicked", G_CALLBACK(vik_layers_panel_copy_selected), vlp );
  /* Paste button */
  pasteimage = gtk_image_new_from_stock ( GTK_STOCK_PASTE, GTK_ICON_SIZE_SMALL_TOOLBAR );
  pastebutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(pastebutton),pasteimage );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(pastebutton), _("Paste layer into selected container layer or otherwise above selected layer"));
  gtk_box_pack_start ( GTK_BOX(vlp->hbox), pastebutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(pastebutton), "clicked", G_CALLBACK(vik_layers_panel_paste_selected), vlp );

  scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
  gtk_container_add ( GTK_CONTAINER(scrolledwindow), GTK_WIDGET(vlp->vt) );

  int cal_markup;
  if ( a_settings_get_integer ( VIK_SETTINGS_CAL_MUM, &cal_markup ) ) {
    if ( cal_markup >= VLP_CAL_MU_NONE && cal_markup < VLP_CAL_MU_LAST )
      vlp->cal_markup = cal_markup;
    else
      vlp->cal_markup = VLP_CAL_MU_AUTO;
  }
  else
    vlp->cal_markup = VLP_CAL_MU_AUTO;

  vlp->calendar = gtk_calendar_new();
  g_signal_connect_swapped ( vlp->calendar, "month-changed", G_CALLBACK(vik_layers_panel_calendar_update), vlp);
  // Use double click event, rather than just the day-selected event,
  //  since changing month/year causes the day-selected event to occur
  // (and then possibly this results in selecting a track) which would be counter intuitive
  g_signal_connect_swapped ( vlp->calendar, "day-selected-double-click", G_CALLBACK(layers_calendar_day_selected_dc_cb), vlp);
  g_signal_connect_swapped ( vlp->calendar, "button-press-event", G_CALLBACK(layers_calendar_button_press_cb), vlp );
  vlp->cal_shown = TRUE;

  // Ensure any detail results in a tooltip rather than embedded in the calendar display
  GValue sd = { 0 };
  g_value_init ( &sd, G_TYPE_BOOLEAN );
  g_value_set_boolean ( &sd, FALSE );
  g_object_set_property ( G_OBJECT(vlp->calendar), "show-details", &sd );

  gtk_calendar_set_detail_func ( GTK_CALENDAR(vlp->calendar), calendar_detail, vlp, NULL );
  vik_layers_panel_set_preferences ( vlp );
  
  gtk_box_pack_start ( GTK_BOX(vlp), scrolledwindow, TRUE, TRUE, 0 );
  gtk_box_pack_start ( GTK_BOX(vlp), vlp->calendar, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(vlp), vlp->hbox, FALSE, FALSE, 0 );
}

/**
 * Invoke the actual drawing via signal method
 */
static gboolean idle_draw_panel ( VikLayersPanel *vlp )
{
  g_signal_emit ( G_OBJECT(vlp), layers_panel_signals[VLP_UPDATE_SIGNAL], 0 );
  return FALSE; // Nothing else to do
}

void vik_layers_panel_emit_update ( VikLayersPanel *vlp )
{
  GThread *thread = vik_window_get_thread (VIK_WINDOW(VIK_GTK_WINDOW_FROM_WIDGET(vlp)));
  if ( !thread )
    // Do nothing
    return;

  // Only ever draw when there is time to do so
  if ( g_thread_self() != thread )
    // Drawing requested from another (background) thread, so handle via the gdk thread method
    (void)gdk_threads_add_idle ( (GSourceFunc)idle_draw_panel, vlp );
  else
    (void)g_idle_add ( (GSourceFunc)idle_draw_panel, vlp );
}

static void layers_item_toggled (VikLayersPanel *vlp, GtkTreeIter *iter)
{
  gboolean visible;
  gpointer p;
  gint type;

  /* get type and data */
  type = vik_treeview_item_get_type ( vlp->vt, iter );
  p = vik_treeview_item_get_pointer ( vlp->vt, iter );

  switch ( type )
  {
    case VIK_TREEVIEW_TYPE_LAYER:
      visible = (VIK_LAYER(p)->visible ^= 1);
      vik_layer_layer_toggle_visible ( VIK_LAYER(p) );
      vik_layer_emit_update_although_invisible ( VIK_LAYER(p) ); /* set trigger for half-drawn */
      break;
    case VIK_TREEVIEW_TYPE_SUBLAYER: {
      VikLayer *vl = VIK_LAYER(vik_treeview_item_get_parent ( vlp->vt, iter ));
      visible = vik_layer_sublayer_toggle_visible ( vl, vik_treeview_item_get_data(vlp->vt, iter), p);
      vik_layer_emit_update_although_invisible ( vl );
      break;
    }
    default: return;
  }

  vik_treeview_item_set_visible ( vlp->vt, iter, visible );
}

static void layers_item_edited (VikLayersPanel *vlp, GtkTreeIter *iter, const gchar *new_text)
{
  if ( !new_text )
    return;

  if ( new_text[0] == '\0' ) {
    a_dialog_error_msg ( GTK_WINDOW(VIK_WINDOW_FROM_WIDGET(vlp)), _("New name can not be blank.") );
    return;
  }

  if ( vik_treeview_item_get_type ( vlp->vt, iter ) == VIK_TREEVIEW_TYPE_LAYER )
  {
    VikLayer *l;

    /* get iter and layer */
    l = VIK_LAYER ( vik_treeview_item_get_pointer ( vlp->vt, iter ) );

    if ( strcmp ( l->name, new_text ) != 0 )
    {
      vik_layer_rename ( l, new_text );
      vik_treeview_item_set_name ( vlp->vt, iter, l->name );
    }
  }
  else
  {
    const gchar *name = vik_layer_sublayer_rename_request ( vik_treeview_item_get_parent ( vlp->vt, iter ), new_text, vlp, vik_treeview_item_get_data ( vlp->vt, iter ), vik_treeview_item_get_pointer ( vlp->vt, iter ), iter );
    if ( name )
      vik_treeview_item_set_name ( vlp->vt, iter, name);
  }
}

static gboolean layers_button_press_cb ( VikLayersPanel *vlp, GdkEventButton *event )
{
  if (event->button == 3)
  {
    static GtkTreeIter iter;
    if ( vik_treeview_get_iter_at_pos ( vlp->vt, &iter, event->x, event->y ) )
    {
      layers_popup ( vlp, &iter, 3 );
      vik_treeview_item_select ( vlp->vt, &iter );
    }
    else
      layers_popup ( vlp, NULL, 3 );
    return TRUE;
  }
  return FALSE;
}

static gboolean layers_key_press_cb ( VikLayersPanel *vlp, GdkEventKey *event )
{
  // Accept all forms of delete keys
  if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete || event->keyval == GDK_BackSpace) {
    vik_layers_panel_delete_selected (vlp);
    return TRUE;
 }
 return FALSE;
}

static void layers_popup ( VikLayersPanel *vlp, GtkTreeIter *iter, gint mouse_button )
{
  GtkMenu *menu = NULL;

  if ( iter )
  {
    if ( vik_treeview_item_get_type ( vlp->vt, iter ) == VIK_TREEVIEW_TYPE_LAYER )
    {
      VikLayer *layer = VIK_LAYER(vik_treeview_item_get_pointer ( vlp->vt, iter ));

      if ( layer->type == VIK_LAYER_AGGREGATE )
        menu = GTK_MENU ( layers_panel_create_popup ( vlp, TRUE ) );
      else
      {
	VikStdLayerMenuItem menu_selection = vik_layer_get_menu_items_selection(layer);

        menu = GTK_MENU ( gtk_menu_new () );

	if (menu_selection & VIK_MENU_ITEM_PROPERTY) {
          (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_PROPERTIES, G_CALLBACK(vik_layers_panel_properties), vlp );
	}

	if (menu_selection & VIK_MENU_ITEM_CUT) {
          (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_CUT, G_CALLBACK(vik_layers_panel_cut_selected), vlp );
	}

	if (menu_selection & VIK_MENU_ITEM_COPY) {
          (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_COPY, G_CALLBACK(vik_layers_panel_copy_selected), vlp );
	}

	if (menu_selection & VIK_MENU_ITEM_PASTE) {
          (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_PASTE, G_CALLBACK(vik_layers_panel_paste_selected), vlp );
	}

	if (menu_selection & VIK_MENU_ITEM_DELETE) {
          (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_DELETE, G_CALLBACK(vik_layers_panel_delete_selected), vlp );
	}
      }
      vik_layer_add_menu_items ( layer, menu, vlp );
      gtk_widget_show_all ( GTK_WIDGET(menu) );
    }
    else
    {
      menu = GTK_MENU ( gtk_menu_new () );
      if ( ! vik_layer_sublayer_add_menu_items ( vik_treeview_item_get_parent ( vlp->vt, iter ), menu, vlp, vik_treeview_item_get_data ( vlp->vt, iter ), vik_treeview_item_get_pointer ( vlp->vt, iter ), iter, vlp->vvp ) )
      {
        gtk_widget_destroy ( GTK_WIDGET(menu) );
        return;
      }
      /* TODO: specific things for different types */
    }
  }
  else
  {
    menu = GTK_MENU ( layers_panel_create_popup ( vlp, FALSE ) );
  }
  gtk_menu_popup ( menu, NULL, NULL, NULL, NULL, mouse_button, gtk_get_current_event_time() );
}

static void menu_popup_cb ( VikLayersPanel *vlp )
{
  GtkTreeIter iter;
  layers_popup ( vlp, vik_treeview_get_selected_iter ( vlp->vt, &iter ) ? &iter : NULL, 0 );
}

static void layers_popup_cb ( VikLayersPanel *vlp )
{
  layers_popup ( vlp, NULL, 0 );
}

#define VIK_SETTINGS_LAYERS_TRW_CREATE_DEFAULT "layers_create_trw_auto_default"
/**
 * vik_layers_panel_new_layer:
 * @type: type of the new layer
 * 
 * Create a new layer and add to panel.
 */
gboolean vik_layers_panel_new_layer ( VikLayersPanel *vlp, VikLayerTypeEnum type )
{
  VikLayer *l;
  g_assert ( vlp->vvp );
  gboolean ask_user = FALSE;
  if ( type == VIK_LAYER_TRW )
    (void)a_settings_get_boolean ( VIK_SETTINGS_LAYERS_TRW_CREATE_DEFAULT, &ask_user );
  ask_user = !ask_user;
  l = vik_layer_create ( type, vlp->vvp, ask_user );
  if ( l )
  {
    vik_layers_panel_add_layer ( vlp, l );
    return TRUE;
  }
  return FALSE;
}

/**
 * vik_layers_panel_add_layer:
 * @l: existing layer
 * 
 * Add an existing layer to panel.
 */
void vik_layers_panel_add_layer ( VikLayersPanel *vlp, VikLayer *l )
{
  GtkTreeIter iter;
  GtkTreeIter *replace_iter = NULL;

  /* could be something different so we have to do this */
  vik_layer_change_coord_mode ( l, vik_viewport_get_coord_mode(vlp->vvp) );

  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    vik_aggregate_layer_add_layer ( vlp->toplayer, l, TRUE );
  else
  {
    VikAggregateLayer *addtoagg;
    if (vik_treeview_item_get_type ( vlp->vt, &iter ) == VIK_TREEVIEW_TYPE_LAYER )
    {
      if ( IS_VIK_AGGREGATE_LAYER(vik_treeview_item_get_pointer ( vlp->vt, &iter )) )
         addtoagg = VIK_AGGREGATE_LAYER(vik_treeview_item_get_pointer ( vlp->vt, &iter ));
      else {
       VikLayer *vl = VIK_LAYER(vik_treeview_item_get_parent ( vlp->vt, &iter ));
       while ( ! IS_VIK_AGGREGATE_LAYER(vl) ) {
         iter = vl->iter;
         vl = VIK_LAYER(vik_treeview_item_get_parent ( vlp->vt, &vl->iter ));
         g_assert ( vl->realized );
       }
       addtoagg = VIK_AGGREGATE_LAYER(vl);
       replace_iter = &iter;
      }
    }
    else
    {
      /* a sublayer is selected, first get its parent (layer), then find the layer's parent (aggr. layer) */
      VikLayer *vl = VIK_LAYER(vik_treeview_item_get_parent ( vlp->vt, &iter ));
      replace_iter = &(vl->iter);
      g_assert ( vl->realized );
      VikLayer *grandpa = (vik_treeview_item_get_parent ( vlp->vt, &(vl->iter) ) );
      if (IS_VIK_AGGREGATE_LAYER(grandpa))
        addtoagg = VIK_AGGREGATE_LAYER(grandpa);
      else {
        addtoagg = vlp->toplayer;
        replace_iter = &grandpa->iter;
      }
    }
    if ( replace_iter )
      vik_aggregate_layer_insert_layer ( addtoagg, l, replace_iter );
    else
      vik_aggregate_layer_add_layer ( addtoagg, l, TRUE );
  }

  vik_layers_panel_emit_update ( vlp );
}

static void layers_move_item ( VikLayersPanel *vlp, gboolean up )
{
  GtkTreeIter iter;
  VikAggregateLayer *parent;

  /* TODO: deactivate the buttons and stuff */
  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    return;

  vik_treeview_select_iter ( vlp->vt, &iter, FALSE ); /* cancel any layer-name editing going on... */

  if ( vik_treeview_item_get_type ( vlp->vt, &iter ) == VIK_TREEVIEW_TYPE_LAYER )
  {
    parent = VIK_AGGREGATE_LAYER(vik_treeview_item_get_parent ( vlp->vt, &iter ));
    if ( parent ) /* not toplevel */
    {
      vik_aggregate_layer_move_layer ( parent, &iter, up );
      vik_layers_panel_emit_update ( vlp );
    }
  }
}

gboolean vik_layers_panel_properties ( VikLayersPanel *vlp )
{
  GtkTreeIter iter;
  g_assert ( vlp->vvp );

  if ( vik_treeview_get_selected_iter ( vlp->vt, &iter ) && vik_treeview_item_get_type ( vlp->vt, &iter ) == VIK_TREEVIEW_TYPE_LAYER )
  {
    VikLayer *layer = VIK_LAYER( vik_treeview_item_get_pointer ( vlp->vt, &iter ) );
    if ( vik_layer_properties ( layer, vlp->vvp, TRUE ))
      vik_layer_emit_update ( layer );
    return TRUE;
  }
  else
    return FALSE;
}

void vik_layers_panel_draw_all ( VikLayersPanel *vlp )
{
  if ( vlp->vvp && VIK_LAYER(vlp->toplayer)->visible )
    vik_aggregate_layer_draw ( vlp->toplayer, vlp->vvp );
}

void vik_layers_panel_cut_selected ( VikLayersPanel *vlp )
{
  gint type;
  GtkTreeIter iter;
  
  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    /* Nothing to do */
    return;

  type = vik_treeview_item_get_type ( vlp->vt, &iter );

  if ( type == VIK_TREEVIEW_TYPE_LAYER )
  {
    VikAggregateLayer *parent = vik_treeview_item_get_parent ( vlp->vt, &iter );
    if ( parent )
    {
      /* reset trigger if trigger deleted */
      if ( vik_layers_panel_get_selected ( vlp ) == vik_viewport_get_trigger ( vlp->vvp ) )
        vik_viewport_set_trigger ( vlp->vvp, NULL );

      a_clipboard_copy_selected ( vlp );

      if (IS_VIK_AGGREGATE_LAYER(parent)) {

        g_signal_emit ( G_OBJECT(vlp), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0 );

        if ( vik_aggregate_layer_delete ( parent, &iter ) )
          vik_layers_panel_emit_update ( vlp );
      }
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_WIDGET(vlp), _("You cannot cut the Top Layer.") );
  }
  else if (type == VIK_TREEVIEW_TYPE_SUBLAYER) {
    VikLayer *sel = vik_layers_panel_get_selected ( vlp );
    if ( vik_layer_get_interface(sel->type)->cut_item ) {
      gint subtype = vik_treeview_item_get_data( vlp->vt, &iter);
      vik_layer_get_interface(sel->type)->cut_item ( sel, subtype, vik_treeview_item_get_pointer(sel->vt, &iter) );
    }
  }
}

void vik_layers_panel_copy_selected ( VikLayersPanel *vlp )
{
  GtkTreeIter iter;
  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    /* Nothing to do */
    return;
  // NB clipboard contains layer vs sublayer logic, so don't need to do it here
  a_clipboard_copy_selected ( vlp );
}

gboolean vik_layers_panel_paste_selected ( VikLayersPanel *vlp )
{
  GtkTreeIter iter;
  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    /* Nothing to do */
    return FALSE;
  return a_clipboard_paste ( vlp );
}

void vik_layers_panel_delete_selected ( VikLayersPanel *vlp )
{
  gint type;
  GtkTreeIter iter;
  
  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    /* Nothing to do */
    return;

  type = vik_treeview_item_get_type ( vlp->vt, &iter );

  if ( type == VIK_TREEVIEW_TYPE_LAYER )
  {
    // Get confirmation from the user
    if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_WIDGET(vlp),
				_("Are you sure you want to delete %s?"),
				vik_layer_get_name ( VIK_LAYER(vik_treeview_item_get_pointer ( vlp->vt, &iter )) ) ) )
      return;

    VikAggregateLayer *parent = vik_treeview_item_get_parent ( vlp->vt, &iter );
    if ( parent )
    {
      /* reset trigger if trigger deleted */
      if ( vik_layers_panel_get_selected ( vlp ) == vik_viewport_get_trigger ( vlp->vvp ) )
        vik_viewport_set_trigger ( vlp->vvp, NULL );

      if (IS_VIK_AGGREGATE_LAYER(parent)) {

        g_signal_emit ( G_OBJECT(vlp), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0 );

        if ( vik_aggregate_layer_delete ( parent, &iter ) )
	  vik_layers_panel_emit_update ( vlp );	
      }
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_WIDGET(vlp), _("You cannot delete the Top Layer.") );
  }
  else if (type == VIK_TREEVIEW_TYPE_SUBLAYER) {
    VikLayer *sel = vik_layers_panel_get_selected ( vlp );
    if ( vik_layer_get_interface(sel->type)->delete_item ) {
      gint subtype = vik_treeview_item_get_data( vlp->vt, &iter);
      vik_layer_get_interface(sel->type)->delete_item ( sel, subtype, vik_treeview_item_get_pointer(sel->vt, &iter) );
    }
  }
  // Always attempt to update the calendar on any delete (even if not actually deleted anything)
  vik_layers_panel_calendar_update ( vlp );
}

VikLayer *vik_layers_panel_get_selected ( VikLayersPanel *vlp )
{
  GtkTreeIter iter, parent;
  gint type;

  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    return NULL;

  type = vik_treeview_item_get_type ( vlp->vt, &iter );

  while ( type != VIK_TREEVIEW_TYPE_LAYER )
  {
    if ( ! vik_treeview_item_get_parent_iter ( vlp->vt, &iter, &parent ) )
      return NULL;
    iter = parent;
    type = vik_treeview_item_get_type ( vlp->vt, &iter );
  }

  return VIK_LAYER( vik_treeview_item_get_pointer ( vlp->vt, &iter ) );
}

static void layers_move_item_up ( VikLayersPanel *vlp )
{
  layers_move_item ( vlp, TRUE );
}

static void layers_move_item_down ( VikLayersPanel *vlp )
{
  layers_move_item ( vlp, FALSE );
}

#if 0
gboolean vik_layers_panel_tool ( VikLayersPanel *vlp, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp )
{
  VikLayer *vl = vik_layers_panel_get_selected ( vlp );
  if ( vl && vl->type == layer_type )
  {
    tool_func ( vl, event, vvp );
    return TRUE;
  }
  else if ( VIK_LAYER(vlp->toplayer)->visible &&
      vik_aggregate_layer_tool ( vlp->toplayer, layer_type, tool_func, event, vvp ) != 1 ) /* either accepted or rejected, but a layer was found */
    return TRUE;
  return FALSE;
}
#endif

VikLayer *vik_layers_panel_get_layer_of_type ( VikLayersPanel *vlp, VikLayerTypeEnum type )
{
  VikLayer *rv = vik_layers_panel_get_selected ( vlp );
  if ( rv == NULL || rv->type != type )
    if ( VIK_LAYER(vlp->toplayer)->visible )
      return vik_aggregate_layer_get_top_visible_layer_of_type ( vlp->toplayer, type );
    else
      return NULL;
  else
    return rv;
}

GList *vik_layers_panel_get_all_layers_of_type(VikLayersPanel *vlp, gint type, gboolean include_invisible)
{
  GList *layers = NULL;

  return (vik_aggregate_layer_get_all_layers_of_type ( vlp->toplayer, layers, type, include_invisible));
}

VikAggregateLayer *vik_layers_panel_get_top_layer ( VikLayersPanel *vlp )
{
  return vlp->toplayer;
}

/**
 * Remove all layers
 */
void vik_layers_panel_clear ( VikLayersPanel *vlp )
{
  if ( ! vik_aggregate_layer_is_empty(vlp->toplayer) ) {
    g_signal_emit ( G_OBJECT(vlp), layers_panel_signals[VLP_DELETE_LAYER_SIGNAL], 0 );
    vik_aggregate_layer_clear ( vlp->toplayer ); /* simply deletes all layers */
    gtk_calendar_clear_marks ( GTK_CALENDAR(vlp->calendar) );
  }
}

void vik_layers_panel_change_coord_mode ( VikLayersPanel *vlp, VikCoordMode mode )
{
  vik_layer_change_coord_mode ( VIK_LAYER(vlp->toplayer), mode );
}

static void layers_panel_finalize ( GObject *gob )
{
  VikLayersPanel *vlp = VIK_LAYERS_PANEL ( gob );
  g_object_unref ( VIK_LAYER(vlp->toplayer) );
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

VikTreeview *vik_layers_panel_get_treeview ( VikLayersPanel *vlp )
{
  return vlp->vt;
}

void vik_layers_panel_show_buttons ( VikLayersPanel *vlp, gboolean show )
{
  if ( show )
    gtk_widget_show ( vlp->hbox );
  else
    gtk_widget_hide ( vlp->hbox );
}

void vik_layers_panel_show_calendar ( VikLayersPanel *vlp, gboolean show )
{
  if ( show ) {
    vlp->cal_shown = TRUE;
    vik_layers_panel_calendar_update ( vlp );
    gtk_widget_show ( vlp->calendar );
  }
  else {
    vlp->cal_shown = FALSE;
    gtk_widget_hide ( vlp->calendar );
  }
}

/**
 *
 */
void vik_layers_panel_calendar_today ( VikLayersPanel *vlp )
{
  vu_calendar_set_to_today (vlp->calendar);
}

void vik_layers_panel_calendar_date ( VikLayersPanel *vlp, time_t timestamp )
{
  GDate *gd = g_date_new();
  g_date_set_time_t ( gd, timestamp );
  gtk_calendar_select_month ( GTK_CALENDAR(vlp->calendar), g_date_get_month(gd)-1, g_date_get_year(gd) );
  gtk_calendar_select_day ( GTK_CALENDAR(vlp->calendar), g_date_get_day(gd) );
  g_date_free ( gd );
}

/**
 * vik_layers_panel_set_preferences:
 *
 * Allow reapplying preferences (i.e from the preferences dialog)
 */
void vik_layers_panel_set_preferences ( VikLayersPanel *vlp )
{
  GValue sd = { 0 };
  g_value_init ( &sd, G_TYPE_BOOLEAN );
  g_value_set_boolean ( &sd, a_vik_get_calendar_show_day_names() );
  g_object_set_property ( G_OBJECT(vlp->calendar), "show-day-names", &sd );
}
