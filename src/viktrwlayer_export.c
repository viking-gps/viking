
/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <unistd.h>

#include "babel.h"
#include "babel_ui.h"
#include "viking.h"
#include "viktrwlayer_export.h"
#include "gpx.h"
#include "preferences.h"

static gchar *last_folder_uri = NULL;

#define VIK_PREFS_EXPORT_DEVICE_SIMPLIFY VIKING_PREFERENCES_IO_NAMESPACE"external_auto_device_gpx_simplify"

#define VIK_SETTINGS_EXPORT_DEVICE_PATH "export_device_path"
#define VIK_SETTINGS_EXPORT_DEVICE_SIMPLIFY_TRACKPOINT_LIMIT "export_device_trackpoint_limit"
#define VIK_SETTINGS_EXPORT_DEVICE_SIMPLIFY_ROUTEPOINT_LIMIT "export_device_routepoint_limit"

static VikLayerParam io_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIK_PREFS_EXPORT_DEVICE_SIMPLIFY, VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Auto Device GPX Simplify:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
     N_("GPX saves to certain devices will be simplified for device compatibility."), NULL, NULL, NULL },
};

void vik_trw_layer_export_init ()
{
  VikLayerParamData tmp;
  tmp.b = FALSE;
  a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
}

static gboolean gpx_export_simplify_layer ( VikTrwLayer *vtl, const gchar *fn, VikTrack* trk )
{
  // Note GPSBabel simplify has single value applied to tracks and routes
  // Try to use the best value
  // However this means if there is a mixture of tracks and routes,
  //  the tracks will be over-simplified
  // The defaults are for Garmin Edge series
  gint tmp;
  guint limit = 10000; // ETrex 20 is 500
  if ( a_settings_get_integer ( VIK_SETTINGS_EXPORT_DEVICE_SIMPLIFY_TRACKPOINT_LIMIT, &tmp ) )
    limit = tmp;

  // If any routes - apply route limit as that's normally alot lower
  gboolean got_route = FALSE;
  if ( trk )
    got_route = trk->is_route;

  if ( !got_route ) {
    if ( g_hash_table_size (vik_trw_layer_get_routes(vtl)) ) {
      gint tmp;
      if ( a_settings_get_integer ( VIK_SETTINGS_EXPORT_DEVICE_SIMPLIFY_ROUTEPOINT_LIMIT, &tmp ) )
         limit = tmp;
      else
         limit = 250; // Route capacity is relatively limited
    }
    // NB If there are tracks and trackpoints much > (route)limit,
    //  could warn that tracks may be overily simplified
    // and ask to continue...
  }

  gboolean need_simplify = FALSE;
  if ( trk ) {
    if ( vik_track_get_tp_count(trk) > limit )
      need_simplify = TRUE;
  }
  else {
    // Check all tracks & routes in the layer
    gpointer key, value;
    GHashTableIter ght_iter;
    // See if any tracks exceed point number limit
    g_hash_table_iter_init ( &ght_iter, vik_trw_layer_get_tracks ( vtl ) );
    while ( g_hash_table_iter_next (&ght_iter, &key, &value) ) {
      if ( vik_track_get_tp_count (VIK_TRACK(value)) > limit ) {
        need_simplify = TRUE;
        break;
      }
    }
    if ( !need_simplify ) {
      // See if any routes exceed point number limit
      g_hash_table_iter_init ( &ght_iter, vik_trw_layer_get_routes ( vtl ) );
      while ( g_hash_table_iter_next (&ght_iter, &key, &value) ) {
        if ( vik_track_get_tp_count (VIK_TRACK(value)) > limit ) {
          need_simplify = TRUE;
          break;
        }
      }
    }
  }

  gboolean ans = FALSE;

  if ( need_simplify ) {
    if ( a_babel_available() ) {
      gchar *filter = g_strdup_printf ( "-x simplify,count=%d -o gpx", limit );
      ans = a_babel_convert_to ( vtl, trk, filter, fn, NULL, NULL );
      g_free ( filter );
      if ( ans ) {
         gchar *msg = g_strdup_printf ( _("Export of GPX file simplified using point limit: %d"), limit ); 
         a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), msg );
         g_free ( msg );
      }
    }
  }

  return ans;
}

static gboolean gpx_export ( VikTrwLayer *vtl, const gchar *fn, VikTrack* trk )
{
  // Note one could attempt to detect which device it is being saved to
  // e.g. using libusb-1.0 - example:
  // https://github.com/libusb/libusb/blob/master/examples/testlibusb.c +
  // http://www.linux-usb.org/usb.ids
  // And then knowledge of which GPSes have differing limits.
  // It might not be possible to differentiate between devices if the USB id is the same
  // e.g. Etrex 20 vs Etrex 30 - but I think these have the same limitations.
  //  and whether file is actually being saved on to the device(s) detected
  gboolean auto_simplify = a_preferences_get (VIK_PREFS_EXPORT_DEVICE_SIMPLIFY)->b;
  if ( auto_simplify ) {
    // Check path of 'fn' for device match
    gchar *device_path = NULL;
    if ( !a_settings_get_string ( VIK_SETTINGS_EXPORT_DEVICE_PATH, &device_path ) ) {
#ifdef WINDOWS
      device_path = g_strdup (":\Garmin\GPX");
#else
      device_path = g_strdup_printf ( "/media/%s/GARMIN/Garmin/GPX", g_getenv("USER") );
#endif
    }
    gboolean simplified = FALSE;
    if ( g_strrstr(fn, device_path) ) {
      simplified = gpx_export_simplify_layer ( vtl, fn, trk );
    }
    g_free ( device_path );
    if ( simplified )
      return TRUE;
  }

  return a_file_export ( vtl, fn, FILE_TYPE_GPX, trk, trk ? TRUE : FALSE );
}

void vik_trw_layer_export ( VikTrwLayer *vtl, const gchar *title, const gchar* default_name, VikTrack* trk, VikFileType_t file_type )
{
  GtkWidget *file_selector;
  gboolean failed = FALSE;
  file_selector = gtk_file_chooser_dialog_new (title,
                                               NULL,
                                               GTK_FILE_CHOOSER_ACTION_SAVE,
                                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                               GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                               NULL);
  if ( last_folder_uri )
    gtk_file_chooser_set_current_folder_uri ( GTK_FILE_CHOOSER(file_selector), last_folder_uri );

  gtk_file_chooser_set_current_name ( GTK_FILE_CHOOSER(file_selector), default_name );

  while ( gtk_dialog_run ( GTK_DIALOG(file_selector) ) == GTK_RESPONSE_ACCEPT )
  {
    gchar *fn = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(file_selector) );
    if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) == FALSE ||
         a_dialog_yes_or_no ( GTK_WINDOW(file_selector), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
    {
      g_free ( last_folder_uri );
      last_folder_uri = gtk_file_chooser_get_current_folder_uri ( GTK_FILE_CHOOSER(file_selector) );

      gtk_widget_hide ( file_selector );
      vik_window_set_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
      // Don't Export invisible items - unless requested on this specific track
      if ( file_type == FILE_TYPE_GPX )
        failed = ! gpx_export ( vtl, fn, trk );
      else
        failed = ! a_file_export ( vtl, fn, file_type, trk, trk ? TRUE : FALSE );
      vik_window_clear_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
      g_free ( fn );
      break;
    }
    g_free ( fn );
  }
  gtk_widget_destroy ( file_selector );
  if ( failed )
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("The filename you requested could not be opened for writing.") );
}


/**
 * Convert the given TRW layer into a temporary GPX file and open it with the specified program
 *
 */
void vik_trw_layer_export_external_gpx ( VikTrwLayer *vtl, const gchar* external_program )
{
  // Don't Export invisible items
  static GpxWritingOptions options = { TRUE, TRUE, FALSE, FALSE };
  gchar *name_used = a_gpx_write_tmp_file ( vtl, &options );

  if ( name_used ) {
    GError *err = NULL;
    gchar *quoted_file = g_shell_quote ( name_used );
    gchar *cmd = g_strdup_printf ( "%s %s", external_program, quoted_file );
    g_free ( quoted_file );
    if ( ! g_spawn_command_line_async ( cmd, &err ) ) {
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER( vtl), _("Could not launch %s."), external_program );
      g_error_free ( err );
    }
    g_free ( cmd );
    util_add_to_deletion_list ( name_used );
    g_free ( name_used );
  }
  else
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Could not create temporary file for export.") );
}

static guint last_mode_index = 0;
static BabelMode last_mode = { 0, 0, 0, 0, 0, 0 };
static gchar* last_suboptions = NULL;

gboolean BabelMode_is_equal ( BabelMode left, BabelMode right )
{
  return ( left.waypointsRead == right.waypointsRead &&
           left.waypointsWrite == right.waypointsWrite &&
           left.tracksRead == right.tracksRead &&
           left.tracksWrite == right.tracksWrite &&
           left.routesRead == right.routesRead &&
           left.routesWrite == right.routesWrite );
}

void vik_trw_layer_export_gpsbabel ( VikTrwLayer *vtl, const gchar *title, const gchar* default_name )
{
  BabelMode mode = { 0, 0, 0, 0, 0, 0 };
  if ( g_hash_table_size (vik_trw_layer_get_routes(vtl)) ) {
      mode.routesWrite = 1;
  }
  if ( g_hash_table_size (vik_trw_layer_get_tracks(vtl)) ) {
      mode.tracksWrite = 1;
  }
  if ( g_hash_table_size (vik_trw_layer_get_waypoints(vtl)) ) {
      mode.waypointsWrite = 1;
  }

  GtkWidget *file_selector;
  gboolean failed = FALSE;
  file_selector = gtk_file_chooser_dialog_new (title,
                                               NULL,
                                               GTK_FILE_CHOOSER_ACTION_SAVE,
                                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                               GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                               NULL);
  gchar *cwd = g_get_current_dir();
  if ( cwd ) {
    gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER(file_selector), cwd );
    g_free ( cwd );
  }

  /* Build the extra part of the widget */
  GtkWidget *babel_selector = a_babel_ui_file_type_selector_new ( mode );
  GtkWidget *label = gtk_label_new(_("File format:"));
  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start ( GTK_BOX(hbox), label, TRUE, TRUE, 2 );
  gtk_box_pack_start ( GTK_BOX(hbox), babel_selector, TRUE, TRUE, 2 );

  gtk_widget_set_tooltip_text( babel_selector, _("Select the file format.") );

  GtkWidget *babel_modes = a_babel_ui_modes_new(mode.tracksWrite, mode.routesWrite, mode.waypointsWrite);

  gtk_widget_set_tooltip_text( babel_modes, _("Select the information to process.\n"
      "Warning: the behavior of these switches is highly dependent of the file format selected.\n"
      "Please, refer to GPSbabel if unsure.") );

  GtkWidget *hbox2 = gtk_hbox_new(FALSE, 0);
  GtkWidget *link = gtk_link_button_new_with_label ( "https://www.gpsbabel.org/capabilities.html", _("GPSBabel suboptions") );
  GtkWidget *entry = ui_entry_new ( BabelMode_is_equal(mode, last_mode) ? last_suboptions : NULL, GTK_ENTRY_ICON_SECONDARY );
  gtk_box_pack_start ( GTK_BOX(hbox2), link, FALSE, FALSE, 2 );
  gtk_box_pack_start ( GTK_BOX(hbox2), entry, TRUE, TRUE, 2 );

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start ( GTK_BOX(vbox), hbox, TRUE, TRUE, 2 );
  gtk_box_pack_start ( GTK_BOX(vbox), babel_modes, TRUE, TRUE, 2 );
  gtk_box_pack_start ( GTK_BOX(vbox), hbox2, TRUE, TRUE, 2 );

  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER(file_selector), vbox);

  /* Add some dynamic: only allow dialog's validation when format selection is done */
  g_signal_connect (babel_selector, "changed", G_CALLBACK(a_babel_ui_type_selector_dialog_sensitivity_cb), file_selector);
  // NB the selection list changes according to available modes, so only restore selection index if the same mode
  if ( BabelMode_is_equal(mode, last_mode) )
    gtk_combo_box_set_active ( GTK_COMBO_BOX(babel_selector), last_mode_index );
  /* Manually call the callback to fix the state */
  a_babel_ui_type_selector_dialog_sensitivity_cb ( GTK_COMBO_BOX(babel_selector), file_selector);

  /* Set possible name of the file */
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(file_selector), default_name);

  gtk_widget_show_all ( file_selector );

  while ( gtk_dialog_run ( GTK_DIALOG(file_selector) ) == GTK_RESPONSE_ACCEPT )
  {
    gchar *fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(file_selector) );
    if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) == FALSE ||
         a_dialog_yes_or_no ( GTK_WINDOW(file_selector), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
    {
      BabelFile *active = a_babel_ui_file_type_selector_get(babel_selector);
      if (active == NULL) {
          a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("You did not select a valid file format.") );
      } else {
        gtk_widget_hide ( file_selector );
        vik_window_set_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
        gboolean tracks, routes, waypoints;
        a_babel_ui_modes_get( babel_modes, &tracks, &routes, &waypoints );
        failed = ! a_file_export_babel ( vtl, fn, active->name, tracks, routes, waypoints, gtk_entry_get_text(GTK_ENTRY(entry)) );
        vik_window_clear_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
        g_free ( fn );
        // Save selections for usage next time
        last_mode_index = gtk_combo_box_get_active ( GTK_COMBO_BOX(babel_selector) );
	last_mode = mode;
        if ( last_suboptions )
          g_free ( last_suboptions );
        last_suboptions = g_strdup ( gtk_entry_get_text(GTK_ENTRY(entry)) );
        break;
      }
    }
    g_free ( fn );
  }
  //babel_ui_selector_destroy(babel_selector);
  gtk_widget_destroy ( file_selector );
  if ( failed )
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("The filename you requested could not be opened for writing.") );
}
