/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#include "viktrwlayer_tpwin.h"
#include "vikdatetime_edit_dialog.h"
#include "degrees_converters.h"

// For simplicity these are global values
//  (i.e. would get shared between multiple windows)
static gint width = 0;
static gint height = 0;

struct _VikTrwLayerTpwin {
  GtkDialog parent;
  GtkSpinButton *lat, *lon, *alt, *ts;
  GtkWidget *trkpt_name;
  GtkWidget *time;
  GtkLabel *course, *diff_dist, *diff_time, *diff_speed, *speed, *hdop, *vdop, *pdop, *sat;
  GtkWidget *tabs;
  GtkWidget *extsw;
  GtkLabel *extlab;
  // Previously these buttons were in a glist, however I think the ordering behaviour is implicit
  //  thus control manually to ensure operating on the correct button
  GtkWidget *button_close;
  GtkWidget *button_delete;
  GtkWidget *button_insert;
  GtkWidget *button_split;
  GtkWidget *button_back;
  GtkWidget *button_forward;
  VikTrackpoint *cur_tp;
  gboolean sync_to_tp_block;
  gboolean configured;
};

GType vik_trw_layer_tpwin_get_type (void)
{
  static GType tpwin_type = 0;

  if (!tpwin_type)
  {
    static const GTypeInfo tpwin_info = 
    {
      sizeof (VikTrwLayerTpwinClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikTrwLayerTpwin),
      0,
      NULL /* instance init */
    };
    tpwin_type = g_type_register_static ( GTK_TYPE_DIALOG, "VikTrwLayerTpwin", &tpwin_info, 0 );
  }

  return tpwin_type;
}

/**
 *  Just update the display for the time fields
 */
static void tpwin_update_times ( VikTrwLayerTpwin *tpwin, VikTrackpoint *tp )
{
  if ( !isnan(tp->timestamp) ) {
    if ( gtk_button_get_image ( GTK_BUTTON(tpwin->time) ) )
      gtk_button_set_image ( GTK_BUTTON(tpwin->time), NULL );
    gtk_spin_button_set_value ( tpwin->ts, tp->timestamp );
    time_t time = round ( tp->timestamp );
    gchar *msg = vu_get_time_string ( &time, "%c", &(tp->coord), NULL );
    gtk_button_set_label ( GTK_BUTTON(tpwin->time), msg );
    g_free ( msg );
  }
  else {
    gtk_spin_button_set_value ( tpwin->ts, 0 );
    gtk_button_set_label ( GTK_BUTTON(tpwin->time), NULL );
  }
}

static void tpwin_sync_ll_to_tp ( VikTrwLayerTpwin *tpwin )
{
  if ( tpwin->cur_tp && (!tpwin->sync_to_tp_block) )
  {
    struct LatLon ll;
    VikCoord coord;
    ll.lat = gtk_spin_button_get_value ( tpwin->lat );
    ll.lon = gtk_spin_button_get_value ( tpwin->lon );
    vik_coord_load_from_latlon ( &coord, tpwin->cur_tp->coord.mode, &ll );

    /* don't redraw unless we really have to */
    if ( vik_coord_diff(&(tpwin->cur_tp->coord), &coord) > 0.05 ) /* may not be exact due to rounding */
    {
      tpwin->cur_tp->coord = coord;
      gtk_dialog_response ( GTK_DIALOG(tpwin), VIK_TRW_LAYER_TPWIN_DATA_CHANGED );
    }
  }
}

static void tpwin_sync_alt_to_tp ( VikTrwLayerTpwin *tpwin )
{
  if ( tpwin->cur_tp && (!tpwin->sync_to_tp_block) ) {
    // Always store internally in metres
    vik_units_height_t height_units = a_vik_get_units_height ();
    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      tpwin->cur_tp->altitude = gtk_spin_button_get_value ( tpwin->alt );
      break;
    case VIK_UNITS_HEIGHT_FEET:
      tpwin->cur_tp->altitude = VIK_FEET_TO_METERS(gtk_spin_button_get_value ( tpwin->alt ));
      break;
    default:
      tpwin->cur_tp->altitude = gtk_spin_button_get_value ( tpwin->alt );
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }
  }
}

/**
 *
 */
static void tpwin_sync_ts_to_tp ( VikTrwLayerTpwin *tpwin )
{
  if ( tpwin->cur_tp && (!tpwin->sync_to_tp_block) ) {
    gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->ts), !isnan(tpwin->cur_tp->timestamp) );
    if ( !isnan(tpwin->cur_tp->timestamp) ) {
      tpwin->cur_tp->timestamp = gtk_spin_button_get_value ( tpwin->ts );
      tpwin_update_times ( tpwin, tpwin->cur_tp );
    }
  }
}

static time_t last_edit_time = 0;

/**
 * time_remove_cb:
 */
static void time_remove_cb ( VikTrwLayerTpwin *tpwin )
{
  tpwin->cur_tp->timestamp = NAN;
  GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU );
  gtk_button_set_image ( GTK_BUTTON(tpwin->time), img );
  tpwin_update_times ( tpwin, tpwin->cur_tp );
}

/**
 * remove_menu:
 */
static void remove_menu ( GtkWidget *widget, guint button, VikTrwLayerTpwin *tpwin )
{
  GtkWidget *menu = gtk_menu_new();
  (void)vu_menu_add_item ( GTK_MENU(menu), NULL, GTK_STOCK_REMOVE, G_CALLBACK(time_remove_cb), tpwin );
  gtk_widget_show_all ( GTK_WIDGET(menu) );
  gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, button, gtk_get_current_event_time() );
}

/**
 * tpwin_sync_time_to_tp:
 *
 */
static void tpwin_sync_time_to_tp ( GtkWidget* widget, GdkEventButton *event, VikTrwLayerTpwin *tpwin )
{
  if ( !tpwin->cur_tp || tpwin->sync_to_tp_block )
    return;

  if ( event->button == 3 ) {
    // On right click and when a time is available, allow a method to copy the displayed time as text
    if ( !gtk_button_get_image ( GTK_BUTTON(widget) ) ) {
       vu_copy_label_menu ( widget, event->button );
    }
    return;
  }
  else if ( event->button == 2 ) {
    if ( !gtk_button_get_image ( GTK_BUTTON(widget) ) ) {
      remove_menu ( widget, event->button, tpwin );
    }
    return;
  }

  if ( !tpwin->cur_tp || tpwin->sync_to_tp_block )
    return;

  if ( !isnan(tpwin->cur_tp->timestamp) )
    last_edit_time = tpwin->cur_tp->timestamp;
  else if ( last_edit_time == 0 )
    time ( &last_edit_time );

  GTimeZone *gtz = g_time_zone_new_local ();
  gdouble mytime = vik_datetime_edit_dialog ( GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(&tpwin->parent))),
                                              _("Date/Time Edit"),
                                              last_edit_time,
                                              gtz );
  g_time_zone_unref ( gtz );

  // Was the dialog cancelled?
  if ( isnan(mytime) )
    return;

  // Otherwise use the new value
  tpwin->cur_tp->timestamp = mytime;
  // TODO: consider warning about unsorted times?

  // Clear the previous 'Add' image as now a time is set
  if ( gtk_button_get_image ( GTK_BUTTON(tpwin->time) ) )
    gtk_button_set_image ( GTK_BUTTON(tpwin->time), NULL );

  tpwin_update_times ( tpwin, tpwin->cur_tp );
}

static gboolean tpwin_set_name ( VikTrwLayerTpwin *tpwin )
{
  if ( tpwin->cur_tp && (!tpwin->sync_to_tp_block) ) {
    vik_trackpoint_set_name ( tpwin->cur_tp, gtk_entry_get_text(GTK_ENTRY(tpwin->trkpt_name)) );
  }
  return FALSE;
}

static gboolean configure_event ( GtkWidget *widget, GdkEventConfigure *event, VikTrwLayerTpwin *tpwin )
{
  if ( !tpwin->configured ) {
    tpwin->configured = TRUE;

    // Set defaults
    if ( width == 0 )
      width = event->width;
    if ( height == 0 )
      height = event->height;

    // Allow sizing back down to the minimum
    // Possibly because last row contains a button, using BASE_SIZE rather than MIN_SIZE hint is better
    GdkGeometry geom = { event->width, event->height, 0, 0, 0, 0, 0, 0, 0, 0, GDK_GRAVITY_STATIC };
    gtk_window_set_geometry_hints ( GTK_WINDOW(widget), widget, &geom, GDK_HINT_BASE_SIZE );

    // Restore previous size (if one was set)
    gtk_window_resize ( GTK_WINDOW(widget), width, height );
  }
  return FALSE;
}

#define TPWIN_PAD 1

VikTrwLayerTpwin *vik_trw_layer_tpwin_new ( GtkWindow *parent )
{
  static gchar *left_label_texts[] = { N_("<b>Name:</b>"),
                                       N_("<b>Latitude:</b>"),
                                       N_("<b>Longitude:</b>"),
                                       N_("<b>Altitude:</b>"),
                                       N_("<b>Course:</b>"),
                                       N_("<b>Timestamp:</b>"),
                                       N_("<b>Time:</b>") };
  static gchar *right_label_texts[] = { N_("<b>Distance Difference:</b>"),
                                        N_("<b>Time Difference:</b>"),
                                        N_("<b>\"Speed\" Between:</b>"),
                                        N_("<b>Speed:</b>"),
                                        N_("<b>VDOP:</b>"),
                                        N_("<b>HDOP:</b>"),
                                        N_("<b>PDOP:</b>"),
                                        N_("<b>SAT/FIX:</b>") };

  VikTrwLayerTpwin *tpwin = VIK_TRW_LAYER_TPWIN ( g_object_new ( VIK_TRW_LAYER_TPWIN_TYPE, NULL ) );
  GtkWidget *main_hbox, *left_vbox, *right_vbox;
  GtkWidget *diff_left_vbox, *diff_right_vbox;

  gtk_window_set_transient_for ( GTK_WINDOW(tpwin), parent );
  gtk_window_set_title ( GTK_WINDOW(tpwin), _("Trackpoint") );

  tpwin->button_close = gtk_dialog_add_button ( GTK_DIALOG(tpwin), GTK_STOCK_CLOSE, VIK_TRW_LAYER_TPWIN_CLOSE);
  tpwin->button_insert = gtk_dialog_add_button ( GTK_DIALOG(tpwin), _("_Insert After"), VIK_TRW_LAYER_TPWIN_INSERT);
  tpwin->button_delete = gtk_dialog_add_button ( GTK_DIALOG(tpwin), GTK_STOCK_DELETE, VIK_TRW_LAYER_TPWIN_DELETE);
  tpwin->button_split = gtk_dialog_add_button ( GTK_DIALOG(tpwin), _("Split Here"), VIK_TRW_LAYER_TPWIN_SPLIT);
  tpwin->button_back = gtk_dialog_add_button ( GTK_DIALOG(tpwin), GTK_STOCK_GO_BACK, VIK_TRW_LAYER_TPWIN_BACK);
  tpwin->button_forward = gtk_dialog_add_button ( GTK_DIALOG(tpwin), GTK_STOCK_GO_FORWARD, VIK_TRW_LAYER_TPWIN_FORWARD);

  /* main track info */
  left_vbox = a_dialog_create_label_vbox ( left_label_texts, G_N_ELEMENTS(left_label_texts), 1, TPWIN_PAD );

  tpwin->trkpt_name = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
  g_signal_connect_swapped ( G_OBJECT(tpwin->trkpt_name), "focus-out-event", G_CALLBACK(tpwin_set_name), tpwin );

  tpwin->course = GTK_LABEL(ui_label_new_selectable(NULL));
  tpwin->time = gtk_button_new();
  gtk_button_set_relief ( GTK_BUTTON(tpwin->time), GTK_RELIEF_NONE );
  g_signal_connect ( G_OBJECT(tpwin->time), "button-release-event", G_CALLBACK(tpwin_sync_time_to_tp), tpwin );

  tpwin->lat = GTK_SPIN_BUTTON(gtk_spin_button_new( GTK_ADJUSTMENT(gtk_adjustment_new (
                                 0, -90, 90, 0.00005, 0.01, 0 )), 0.00005, 6));
  tpwin->lon = GTK_SPIN_BUTTON(gtk_spin_button_new( GTK_ADJUSTMENT(gtk_adjustment_new (
                                 0, -180, 180, 0.00005, 0.01, 0 )), 0.00005, 6));

  g_signal_connect_swapped ( G_OBJECT(tpwin->lat), "value-changed", G_CALLBACK(tpwin_sync_ll_to_tp), tpwin );
  g_signal_connect_swapped ( G_OBJECT(tpwin->lon), "value-changed", G_CALLBACK(tpwin_sync_ll_to_tp), tpwin );

  tpwin->alt = GTK_SPIN_BUTTON(gtk_spin_button_new( GTK_ADJUSTMENT(gtk_adjustment_new (
                                 0, -1000, 25000, 10, 100, 0 )), 10, 2));

  g_signal_connect_swapped ( G_OBJECT(tpwin->alt), "value-changed", G_CALLBACK(tpwin_sync_alt_to_tp), tpwin );

  tpwin->ts = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -G_MAXDOUBLE, G_MAXDOUBLE, 0.001, 1, 0 )), 0.001, 3));
  g_signal_connect_swapped ( G_OBJECT(tpwin->ts), "value-changed", G_CALLBACK(tpwin_sync_ts_to_tp), tpwin );

  right_vbox = gtk_vbox_new ( TRUE, 1 );
  gtk_box_pack_start ( GTK_BOX(right_vbox), GTK_WIDGET(tpwin->trkpt_name), TRUE, TRUE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(right_vbox), GTK_WIDGET(tpwin->lat), TRUE, TRUE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(right_vbox), GTK_WIDGET(tpwin->lon), TRUE, TRUE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(right_vbox), GTK_WIDGET(tpwin->alt), TRUE, TRUE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(right_vbox), GTK_WIDGET(tpwin->course), TRUE, TRUE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(right_vbox), GTK_WIDGET(tpwin->ts), TRUE, TRUE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(right_vbox), GTK_WIDGET(tpwin->time), TRUE, TRUE, TPWIN_PAD );

  /* diff info */
  diff_left_vbox = a_dialog_create_label_vbox ( right_label_texts, G_N_ELEMENTS(right_label_texts), 1, TPWIN_PAD );

  tpwin->diff_dist = GTK_LABEL(ui_label_new_selectable(NULL));
  tpwin->diff_time = GTK_LABEL(ui_label_new_selectable(NULL));
  tpwin->diff_speed = GTK_LABEL(ui_label_new_selectable(NULL));
  tpwin->speed = GTK_LABEL(ui_label_new_selectable(NULL));

  tpwin->vdop = GTK_LABEL(ui_label_new_selectable(NULL));
  tpwin->hdop = GTK_LABEL(ui_label_new_selectable(NULL));
  tpwin->pdop = GTK_LABEL(ui_label_new_selectable(NULL));
  tpwin->sat = GTK_LABEL(ui_label_new_selectable(NULL));

  diff_right_vbox = gtk_vbox_new ( TRUE, 1 );
  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->diff_dist), FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->diff_time), FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->diff_speed), FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->speed), FALSE, FALSE, TPWIN_PAD );

  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->vdop), FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->hdop), FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->pdop), FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->sat), FALSE, FALSE, TPWIN_PAD );

  main_hbox = gtk_hbox_new( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(main_hbox), left_vbox, FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(main_hbox), right_vbox, TRUE, TRUE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(main_hbox), diff_left_vbox, FALSE, FALSE, TPWIN_PAD );
  gtk_box_pack_start ( GTK_BOX(main_hbox), diff_right_vbox, FALSE, FALSE, TPWIN_PAD );

  tpwin->extsw = gtk_scrolled_window_new ( NULL, NULL );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(tpwin->extsw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
  tpwin->extlab = GTK_LABEL(ui_label_new_selectable ( NULL ));
  gtk_widget_set_can_focus ( GTK_WIDGET(tpwin->extlab), FALSE ); // Don't let notebook autofocus on it
  gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW(tpwin->extsw), GTK_WIDGET(tpwin->extlab) );

  tpwin->tabs = gtk_notebook_new();
  gtk_notebook_append_page ( GTK_NOTEBOOK(tpwin->tabs), main_hbox, gtk_label_new(_("General")) );
  gtk_notebook_append_page ( GTK_NOTEBOOK(tpwin->tabs), tpwin->extsw, gtk_label_new(_("GPX Extensions")) );
  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(tpwin))), tpwin->tabs, FALSE, FALSE, 0 );
  gtk_notebook_set_show_tabs ( GTK_NOTEBOOK(tpwin->tabs), FALSE ); // Hide the tabs until really needed

  tpwin->cur_tp = NULL;

  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(tpwin), VIK_TRW_LAYER_TPWIN_CLOSE );
#endif
  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  tpwin->configured = FALSE;
  g_signal_connect ( G_OBJECT(tpwin), "configure-event", G_CALLBACK (configure_event), tpwin );

  return tpwin;
}

/**
 *
 */
void vik_trw_layer_tpwin_destroy ( VikTrwLayerTpwin *tpwin )
{
  // Save the size before closing the dialog
  gtk_window_get_size ( GTK_WINDOW(tpwin), &width, &height );
  gtk_widget_destroy ( GTK_WIDGET(tpwin) );
}

void vik_trw_layer_tpwin_set_empty ( VikTrwLayerTpwin *tpwin )
{
  gtk_editable_delete_text ( GTK_EDITABLE(tpwin->trkpt_name), 0, -1 );
  gtk_widget_set_sensitive ( tpwin->trkpt_name, FALSE );

  gtk_button_set_label ( GTK_BUTTON(tpwin->time), "" );
  gtk_label_set_text ( tpwin->course, NULL );

  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->lat), FALSE );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->lon), FALSE );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->alt), FALSE );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->ts), FALSE );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->time), FALSE );

  // Only keep close button enabled
  gtk_widget_set_sensitive ( tpwin->button_insert, FALSE );
  gtk_widget_set_sensitive ( tpwin->button_split, FALSE );
  gtk_widget_set_sensitive ( tpwin->button_delete, FALSE );
  gtk_widget_set_sensitive ( tpwin->button_back, FALSE );
  gtk_widget_set_sensitive ( tpwin->button_forward, FALSE );

  gtk_label_set_text ( tpwin->diff_dist, NULL );
  gtk_label_set_text ( tpwin->diff_time, NULL );
  gtk_label_set_text ( tpwin->diff_speed, NULL );
  gtk_label_set_text ( tpwin->speed, NULL );
  gtk_label_set_text ( tpwin->vdop, NULL );
  gtk_label_set_text ( tpwin->hdop, NULL );
  gtk_label_set_text ( tpwin->pdop, NULL );
  gtk_label_set_text ( tpwin->sat, NULL );

  gtk_label_set_text ( tpwin->extlab, NULL );

  gtk_window_set_title ( GTK_WINDOW(tpwin), _("Trackpoint") );
}

/**
 * vik_trw_layer_tpwin_set_tp:
 * @tpwin:      The Trackpoint Edit Window
 * @tpl:        The #Glist of trackpoints pointing at the current trackpoint
 * @track_name: The name of the track in which the trackpoint belongs
 * @is_route:   Is the track of the trackpoint actually a route?
 *
 * Sets the Trackpoint Edit Window to the values of the current trackpoint given in @tpl.
 *
 */
void vik_trw_layer_tpwin_set_tp ( VikTrwLayerTpwin *tpwin, GList *tpl, const gchar *track_name, gboolean is_route )
{
  static char tmp_str[64];
  static struct LatLon ll;
  VikTrackpoint *tp = VIK_TRACKPOINT(tpl->data);

  if ( tp->name )
    gtk_entry_set_text ( GTK_ENTRY(tpwin->trkpt_name), tp->name );
  else
    gtk_editable_delete_text ( GTK_EDITABLE(tpwin->trkpt_name), 0, -1 );
  gtk_widget_set_sensitive ( tpwin->trkpt_name, TRUE );

  /* Only can insert if not at the end (otherwise use extend track) */
  gtk_widget_set_sensitive ( tpwin->button_insert, (gboolean) GPOINTER_TO_INT (tpl->next) );
  gtk_widget_set_sensitive ( tpwin->button_delete, TRUE );

  /* We can only split up a track if it's not an endpoint. Makes sense to me. */
  gtk_widget_set_sensitive ( tpwin->button_split, tpl->next && tpl->prev );

  gtk_widget_set_sensitive ( tpwin->button_forward, (gboolean) GPOINTER_TO_INT (tpl->next) );
  gtk_widget_set_sensitive ( tpwin->button_back, (gboolean) GPOINTER_TO_INT (tpl->prev) );

  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->lat), TRUE );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->lon), TRUE );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->alt), TRUE );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->ts), !isnan(tp->timestamp) );
  gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->time), !isnan(tp->timestamp) );
  // Enable adding timestamps - but not on routepoints
  if ( isnan(tp->timestamp) && !is_route ) {
    gtk_widget_set_sensitive ( GTK_WIDGET(tpwin->time), TRUE );
    GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU );
    gtk_button_set_image ( GTK_BUTTON(tpwin->time), img );
  }

  vik_trw_layer_tpwin_set_track_name ( tpwin, track_name );

  tpwin->sync_to_tp_block = TRUE; /* don't update while setting data. */

  vik_coord_to_latlon ( &(tp->coord), &ll );
  gtk_spin_button_set_value ( tpwin->lat, ll.lat );
  gtk_spin_button_set_value ( tpwin->lon, ll.lon );
  vik_units_height_t height_units = a_vik_get_units_height ();
  switch (height_units) {
  case VIK_UNITS_HEIGHT_METRES:
    gtk_spin_button_set_value ( tpwin->alt, tp->altitude );
    break;
  case VIK_UNITS_HEIGHT_FEET:
    gtk_spin_button_set_value ( tpwin->alt, VIK_METERS_TO_FEET(tp->altitude) );
    break;
  default:
    gtk_spin_button_set_value ( tpwin->alt, tp->altitude );
    g_critical("Houston, we've had a problem. height=%d", height_units);
  }
  // Override spin button text if NAN (otherwise it displays 0.0)
  if ( isnan(tp->altitude) ) {
    gtk_entry_set_text ( GTK_ENTRY(tpwin->alt), "--" );
  }
  tpwin_update_times ( tpwin, tp );

  tpwin->sync_to_tp_block = FALSE; // don't update while setting data.

  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  if ( tpwin->cur_tp )
  {
    gdouble diff_dist = vik_coord_diff(&(tp->coord), &(tpwin->cur_tp->coord));
    vu_distance_text_precision ( tmp_str, sizeof(tmp_str), dist_units, diff_dist, "%.2f" );
    gtk_label_set_text ( tpwin->diff_dist, tmp_str );

    if ( !isnan(tp->timestamp) && !isnan(tpwin->cur_tp->timestamp) )
    {
      g_snprintf ( tmp_str, sizeof(tmp_str), "%.3f s", tp->timestamp - tpwin->cur_tp->timestamp);
      gtk_label_set_text ( tpwin->diff_time, tmp_str );
      if ( tp->timestamp == tpwin->cur_tp->timestamp )
        gtk_label_set_text ( tpwin->diff_speed, "--" );
      else
      {
        gdouble tmp_speed = vik_coord_diff(&(tp->coord), &(tpwin->cur_tp->coord)) / ABS(tp->timestamp - tpwin->cur_tp->timestamp);
        vu_speed_text ( tmp_str, sizeof(tmp_str), speed_units, tmp_speed, TRUE, "%.2f", FALSE );
        gtk_label_set_text ( tpwin->diff_speed, tmp_str );
      }
    }
    else
    {
      gtk_label_set_text ( tpwin->diff_time, NULL );
      gtk_label_set_text ( tpwin->diff_speed, NULL );
    }
  }

  if ( isnan(tp->course) )
    g_snprintf ( tmp_str, sizeof(tmp_str), "--" );
  else
    g_snprintf ( tmp_str, sizeof(tmp_str), "%05.1f%s", tp->course, DEGREE_SYMBOL );
  gtk_label_set_text ( tpwin->course, tmp_str );

  vu_speed_text ( tmp_str, sizeof(tmp_str), speed_units, tp->speed, TRUE, "%.2f", FALSE );
  gtk_label_set_text ( tpwin->speed, tmp_str );

  vu_distance_text_precision ( tmp_str, sizeof(tmp_str), dist_units, tp->hdop, "%.5f" );
  gtk_label_set_text ( tpwin->hdop, tmp_str );

  vu_distance_text_precision ( tmp_str, sizeof(tmp_str), dist_units, tp->pdop, "%.5f" );
  gtk_label_set_text ( tpwin->pdop, tmp_str );

  if ( isnan(tp->vdop) )
    g_snprintf ( tmp_str, sizeof(tmp_str), "--" );
  else {
    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      g_snprintf ( tmp_str, sizeof(tmp_str), "%.5f m", tp->vdop );
      break;
    case VIK_UNITS_HEIGHT_FEET:
      g_snprintf ( tmp_str, sizeof(tmp_str), "%.5f feet", VIK_METERS_TO_FEET(tp->vdop) );
      break;
    default:
      g_snprintf ( tmp_str, sizeof(tmp_str), "--" );
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }
  }
  gtk_label_set_text ( tpwin->vdop, tmp_str );

  g_snprintf ( tmp_str, sizeof(tmp_str), "%d / %d", tp->nsats, tp->fix_mode );
  gtk_label_set_text ( tpwin->sat, tmp_str );

  gtk_label_set_text ( tpwin->extlab, tp->extensions );
  GtkWidget *ext_tab = gtk_notebook_get_tab_label ( GTK_NOTEBOOK(tpwin->tabs), tpwin->extsw );
  gtk_widget_set_sensitive ( ext_tab, tp->extensions ? TRUE : FALSE );
  if ( tp->extensions )
    gtk_notebook_set_show_tabs ( GTK_NOTEBOOK(tpwin->tabs), TRUE );

  tpwin->cur_tp = tp;
}

void vik_trw_layer_tpwin_set_track_name ( VikTrwLayerTpwin *tpwin, const gchar *track_name )
{
  gchar *tmp_name = g_strdup_printf ( "%s: %s", track_name, _("Trackpoint") );
  gtk_window_set_title ( GTK_WINDOW(tpwin), tmp_name );
  g_free ( tmp_name );
  //gtk_label_set_text ( tpwin->track_name, track_name );
}
