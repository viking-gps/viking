/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2010-2018, Rob Norris <rw_norris@hotmail.com>
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
#include "viktrwlayer_wpwin.h"
#include "degrees_converters.h"
#include "garminsymbols.h"
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif
#include "thumbnails.h"
#include "viking.h"
#include "vikdatetime_edit_dialog.h"
#include "vikgoto.h"

static void update_time ( GtkWidget *widget, VikWaypoint *wp )
{
  time_t tt = (time_t)wp->timestamp;
  gchar *msg = vu_get_time_string ( &tt, "%c", &(wp->coord), NULL );
  gtk_button_set_label ( GTK_BUTTON(widget), msg );
  g_free ( msg );
}

static VikWaypoint *edit_wp;
static gulong direction_signal_id;
static gchar *last_sym = NULL;

/**
 * time_remove_cb:
 */
static void time_remove_cb ( GtkWidget* widget )
{
  edit_wp->timestamp = NAN;
  gtk_button_set_label ( GTK_BUTTON(widget), NULL );
  GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU );
  gtk_button_set_image ( GTK_BUTTON(widget), img );
}

/**
 * remove_menu:
 */
static void remove_menu ( GtkWidget *widget, guint button )
{
  GtkWidget *menu = gtk_menu_new();
  (void)vu_menu_add_item ( GTK_MENU(menu), NULL, GTK_STOCK_REMOVE, G_CALLBACK(time_remove_cb), widget );
  gtk_widget_show_all ( GTK_WIDGET(menu) );
  gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, button, gtk_get_current_event_time() );
}

/**
 * time_edit_click:
 */
static void time_edit_click ( GtkWidget* widget, GdkEventButton *event, VikWaypoint *wp )
{
  if ( event->button == 3 ) {
    // On right click and when a time is available, allow a method to copy the displayed time as text
    if ( !gtk_button_get_image ( GTK_BUTTON(widget) ) ) {
      vu_copy_label_menu ( widget, event->button );
    }
    return;
  }
  else if ( event->button == 2 ) {
    if ( !gtk_button_get_image ( GTK_BUTTON(widget) ) ) {
      remove_menu ( widget, event->button );
    }
    return;
  }

  // Initially use current time or otherwise whatever the last value used was
  if ( isnan(wp->timestamp) ) {
    time_t tt;
    time (&tt);
    wp->timestamp = (gdouble)tt;
  }

  GTimeZone *gtz = g_time_zone_new_local ();
  gdouble mytime = vik_datetime_edit_dialog ( GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                              _("Date/Time Edit"),
                                              wp->timestamp,
                                              gtz );
  g_time_zone_unref ( gtz );

  // Was the dialog cancelled?
  if ( isnan(mytime) )
    return;

  // Otherwise use new value in the edit buffer
  edit_wp->timestamp = mytime;

  // Clear the previous 'Add' image as now a time is set
  if ( gtk_button_get_image ( GTK_BUTTON(widget) ) )
    gtk_button_set_image ( GTK_BUTTON(widget), NULL );

  update_time ( widget, edit_wp );
}

/**
 * direction_edit_click:
 */
static void direction_add_click ( GtkWidget* widget, GdkEventButton *event, GtkWidget *direction )
{
  // Replace 'Add' with text and stop further callbacks
  if ( gtk_button_get_image ( GTK_BUTTON(widget) ) )
    gtk_button_set_image ( GTK_BUTTON(widget), NULL );
  gtk_button_set_label ( GTK_BUTTON(widget), _("True") );
  g_signal_handler_disconnect ( G_OBJECT(widget), direction_signal_id );

  // Enable direction value
  gtk_widget_set_sensitive ( direction, TRUE );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(direction), 0.0 );
}

static void symbol_entry_changed_cb(GtkWidget *combo, GtkListStore *store)
{
  GtkTreeIter iter;
  gchar *sym;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
    return;

  gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1 );
  /* Note: symm is NULL when "(none)" is select (first cell is empty) */
  gtk_widget_set_tooltip_text(combo, sym);
  g_free(sym);
}

// returns in m/s
gdouble get_speed_from_text ( vik_units_speed_t speed_units, gchar const *text )
{
  gdouble speed = NAN;
  // Handle input of speed pace types in the form of XX:XX...
  gchar **components = g_strsplit ( text, ":", -1 );
  guint nn = g_strv_length ( components );
  if ( nn >= 1 )
    speed = atof ( components[0] );
  if ( nn == 2 ) {
    speed = speed + ( atof(components[1])/60.0 );
  }
  g_strfreev ( components );
  if ( !isnan(speed) )
    speed = vu_speed_deconvert ( speed_units, speed );
  return speed;
}

/* Specify if a new waypoint or not */
/* If a new waypoint then it uses the default_name for the suggested name allowing the user to change it.
    The name to use is returned
 */
/* todo: less on this side, like add track */
gchar *a_dialog_waypoint ( GtkWindow *parent, gchar *default_name, VikTrwLayer *vtl, VikWaypoint *wp, VikCoordMode coord_mode, gboolean is_new, gboolean *updated )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Waypoint Properties"),
                                                   parent,
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_STOCK_CANCEL,
                                                   GTK_RESPONSE_REJECT,
                                                   GTK_STOCK_OK,
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
  struct LatLon ll;
  GtkWidget *latlabel, *lonlabel, *namelabel, *latentry, *lonentry, *altentry, *altlabel, *nameentry=NULL;
  GtkWidget *commentlabel, *commententry, *descriptionlabel, *descriptionentry, *imagelabel, *imageentry, *symbollabel, *symbolentry;
  GtkWidget *sourcelabel = NULL, *sourceentry = NULL;
  GtkWidget *typelabel = NULL, *typeentry = NULL;
  GtkWidget *timelabel = NULL;
  GtkWidget *timevaluebutton = NULL;
  GtkWidget *hasGeotagCB = NULL;
  GtkWidget *consistentGeotagCB = NULL;
  GtkWidget *direction_sb = NULL;
  GtkWidget *direction_hb = NULL;
  GtkListStore *store;
  GtkWidget *tabs = gtk_notebook_new();
  GtkWidget *basic = gtk_vbox_new ( FALSE, 0 );

  gchar *lat, *lon, *alt;

  vik_coord_to_latlon ( &(wp->coord), &ll );

  lat = g_strdup_printf ( "%f", ll.lat );
  lon = g_strdup_printf ( "%f", ll.lon );
  vik_units_height_t height_units = a_vik_get_units_height ();
  if ( isnan(wp->altitude) ) {
    alt = NULL;
  } else {
    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      alt = g_strdup_printf ( "%f", wp->altitude );
      break;
    case VIK_UNITS_HEIGHT_FEET:
      alt = g_strdup_printf ( "%f", VIK_METERS_TO_FEET(wp->altitude) );
      break;
    default:
      alt = g_strdup_printf ( "%f", wp->altitude );
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }
  }

  *updated = FALSE;

  namelabel = gtk_label_new (_("Name:"));
  gtk_box_pack_start (GTK_BOX(basic), namelabel, FALSE, FALSE, 0);
  // Name is now always changeable
  nameentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
  if ( default_name )
    gtk_entry_set_text( GTK_ENTRY(nameentry), default_name );
  g_signal_connect_swapped ( nameentry, "activate", G_CALLBACK(a_dialog_response_accept), GTK_DIALOG(dialog) );
  gtk_box_pack_start (GTK_BOX(basic), nameentry, FALSE, FALSE, 0);

  latlabel = gtk_label_new (_("Latitude:"));
  latentry = ui_entry_new ( lat, GTK_ENTRY_ICON_SECONDARY );
  g_free ( lat );

  lonlabel = gtk_label_new (_("Longitude:"));
  lonentry = ui_entry_new ( lon, GTK_ENTRY_ICON_SECONDARY );
  g_free ( lon );

  altlabel = gtk_label_new (_("Altitude:"));
  altentry = ui_entry_new ( alt, GTK_ENTRY_ICON_SECONDARY );
  g_free ( alt );

  if ( wp->comment && !strncmp(wp->comment, "http", 4) )
    commentlabel = gtk_link_button_new_with_label (wp->comment, _("Comment:") );
  else
    commentlabel = gtk_label_new (_("Comment:"));
  commententry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
  gchar *cmt =  NULL;
  // Auto put in some kind of 'name' as a comment if one previously 'goto'ed this exact location
  cmt = a_vik_goto_get_search_string_for_this_place(VIK_WINDOW(parent));
  if (cmt)
    gtk_entry_set_text(GTK_ENTRY(commententry), cmt);

  if ( wp->description && !strncmp(wp->description, "http", 4) )
    descriptionlabel = gtk_link_button_new_with_label (wp->description, _("Description:") );
  else
    descriptionlabel = gtk_label_new (_("Description:"));
  descriptionentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

  sourcelabel = gtk_label_new (_("Source:"));
  sourceentry = ui_entry_new ( wp->source, GTK_ENTRY_ICON_SECONDARY );

  typelabel = gtk_label_new (_("Type:"));
  typeentry = ui_entry_new ( wp->type, GTK_ENTRY_ICON_SECONDARY );

  imagelabel = gtk_label_new (_("Image:"));
  imageentry = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, VF_FILTER_IMAGE, NULL, NULL);

  {
    GtkCellRenderer *r;
    symbollabel = gtk_label_new (_("Symbol:"));
    GtkTreeIter iter;

    store = gtk_list_store_new(3, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING);
    symbolentry = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(symbolentry), 6);

    g_signal_connect(symbolentry, "changed", G_CALLBACK(symbol_entry_changed_cb), store);
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0, NULL, 1, NULL, 2, _("(none)"), -1);
    a_populate_sym_list(store);

    r = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (symbolentry), r, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (symbolentry), r, "pixbuf", 1, NULL);

    r = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (symbolentry), r, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (symbolentry), r, "text", 2, NULL);

    if ( wp->symbol || (is_new && last_sym) ) {
      gboolean ok;
      gchar *sym;
      for (ok = gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(store), &iter ); ok; ok = gtk_tree_model_iter_next ( GTK_TREE_MODEL(store), &iter)) {
        gtk_tree_model_get ( GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1 );
        if (sym && (!g_strcmp0(sym, wp->symbol) || !g_strcmp0(sym, last_sym)) ) {
	  g_free(sym);
	  break;
	} else {
	  g_free(sym);
	}
      }
      // Ensure is it a valid symbol in the given symbol set (large vs small)
      // Not all symbols are available in both
      // The check prevents a Gtk Critical message
      if ( iter.stamp )
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(symbolentry), &iter);
    }
  }

  if ( !is_new && wp->comment )
    gtk_entry_set_text ( GTK_ENTRY(commententry), wp->comment );

  if ( !is_new && wp->description )
    gtk_entry_set_text ( GTK_ENTRY(descriptionentry), wp->description );

  if ( !edit_wp )
    edit_wp = vik_waypoint_new ();
  edit_wp = vik_waypoint_copy ( wp );

  if ( !is_new && wp->image ) {
    vik_file_entry_set_filename ( VIK_FILE_ENTRY(imageentry), wp->image );

#ifdef VIK_CONFIG_GEOTAG
    // Geotag Info [readonly]
    hasGeotagCB = gtk_check_button_new_with_label ( _("Has Geotag") );
    gtk_widget_set_sensitive ( hasGeotagCB, FALSE );
    gboolean hasGeotag;
    gchar *ignore = a_geotag_get_exif_date_from_file ( wp->image, &hasGeotag );
    g_free ( ignore );
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(hasGeotagCB), hasGeotag );

    consistentGeotagCB = gtk_check_button_new_with_label ( _("Consistent Position") );
    gtk_widget_set_sensitive ( consistentGeotagCB, FALSE );
    if ( hasGeotag ) {
      struct LatLon ll = a_geotag_get_position ( wp->image );
      VikCoord coord;
      vik_coord_load_from_latlon ( &coord, coord_mode, &ll );
      gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(consistentGeotagCB), vik_coord_equalish(&coord, &wp->coord) );
    }

    // ATM the direction value box is always shown, even when there is no information.
    // It would be nice to be able to hide it until the 'Add' has been performed,
    //  however I've not been able to achieve this.
    // Thus simply sensistizing it instead.
    GtkWidget *direction_label = gtk_label_new ( _("Image Direction:") );
    direction_hb = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start (GTK_BOX(direction_hb), direction_label, FALSE, FALSE, 0);
    direction_sb = gtk_spin_button_new ( (GtkAdjustment*)gtk_adjustment_new (0, 0.0, 359.9, 5.0, 1, 0 ), 1, 1 );

    if ( !is_new && !isnan(wp->image_direction) ) {
      GtkWidget *direction_ref = gtk_label_new ( NULL );
      if ( wp->image_direction_ref == WP_IMAGE_DIRECTION_REF_MAGNETIC )
        gtk_label_set_label ( GTK_LABEL(direction_ref), _("Magnetic") );
      else
        gtk_label_set_label ( GTK_LABEL(direction_ref), _("True") );

      gtk_box_pack_start (GTK_BOX(direction_hb), direction_ref, TRUE, FALSE, 0);
      gtk_spin_button_set_value ( GTK_SPIN_BUTTON(direction_sb), wp->image_direction );
    }
    else {
      GtkWidget *direction_ref_button = gtk_button_new ();
      gtk_button_set_relief ( GTK_BUTTON(direction_ref_button), GTK_RELIEF_NONE );
      GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU );
      gtk_button_set_image ( GTK_BUTTON(direction_ref_button), img );
      gtk_box_pack_start (GTK_BOX(direction_hb), direction_ref_button, TRUE, FALSE, 0);
      gtk_widget_set_sensitive ( direction_sb, FALSE );
      direction_signal_id = g_signal_connect ( G_OBJECT(direction_ref_button), "button-release-event", G_CALLBACK(direction_add_click), direction_sb );
    }

#endif
  }

  timelabel = gtk_label_new ( _("Time:") );
  timevaluebutton = gtk_button_new();
  gtk_button_set_relief ( GTK_BUTTON(timevaluebutton), GTK_RELIEF_NONE );

  if ( !is_new && !isnan(wp->timestamp) ) {
    update_time ( timevaluebutton, wp );
  }
  else {
    GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU );
    gtk_button_set_image ( GTK_BUTTON(timevaluebutton), img );
  }
  g_signal_connect ( G_OBJECT(timevaluebutton), "button-release-event", G_CALLBACK(time_edit_click), edit_wp );

  gtk_box_pack_start (GTK_BOX(basic), latlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), latentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), lonlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), lonentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), timelabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), timevaluebutton, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), altlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), altentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), commentlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), commententry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), descriptionlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), descriptionentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), sourcelabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), sourceentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), typelabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), typeentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), imagelabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), imageentry, FALSE, FALSE, 0);
  if ( hasGeotagCB ) {
    GtkWidget *hbox =  gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start (GTK_BOX(hbox), hasGeotagCB, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(hbox), consistentGeotagCB, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), hbox, FALSE, FALSE, 0);
  }
  if ( direction_hb )
    gtk_box_pack_start (GTK_BOX(basic), direction_hb, FALSE, FALSE, 0);
  if ( direction_sb )
    gtk_box_pack_start (GTK_BOX(basic), direction_sb, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), symbollabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(basic), GTK_WIDGET(symbolentry), FALSE, FALSE, 0);

  // 'Extra' level of Waypoint properties
  GtkWidget *extra = gtk_vbox_new ( FALSE, 0 );

  gchar *crs = isnan(wp->course) ? NULL : g_strdup_printf ( "%05.2f", wp->course );
  GtkWidget *crslabel = gtk_label_new ( _("Course:") );
  GtkWidget *crsentry = ui_entry_new ( crs, GTK_ENTRY_ICON_SECONDARY );
  g_free ( crs );
  
  gtk_box_pack_start ( GTK_BOX(extra), crslabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), crsentry, FALSE, FALSE, 0 );

  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  char tmp_lab[64];
  g_snprintf ( tmp_lab, sizeof(tmp_lab), _("Speed: (%s)"), vu_speed_units_text(speed_units) );
  GtkWidget *speedlabel = gtk_label_new ( tmp_lab );
  char tmp_str[64];
  gdouble my_speed = wp->speed;
  if ( isnan(my_speed) )
    tmp_str[0] = '\0';
  else {
    my_speed = vu_speed_convert ( speed_units, my_speed );
    vu_speed_text_value ( tmp_str, sizeof(tmp_str), speed_units, my_speed, "%.2f" );
  }
  GtkWidget *speedentry = ui_entry_new ( tmp_str, GTK_ENTRY_ICON_SECONDARY );

  gtk_box_pack_start ( GTK_BOX(extra), speedlabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), speedentry, FALSE, FALSE, 0 );

  char msg[64];
  if ( isnan(wp->magvar) )
    g_snprintf ( msg, sizeof(msg), "--" );
  else
    g_snprintf ( msg, sizeof(msg), "%05.2f", wp->magvar );
  GtkWidget *magvarlabel = gtk_label_new ( _("Magnetic Variance:") );
  GtkWidget *magvarvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), magvarlabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), magvarvalue, FALSE, FALSE, 0 );

  GtkWidget *geoidhgtlabel = gtk_label_new ( _("Geoid Height:") );
  if ( isnan(wp->geoidheight) )
    tmp_str[0] = '\0';
  else {
    switch (height_units) {
    case VIK_UNITS_HEIGHT_FEET:
      g_snprintf ( tmp_str, sizeof(tmp_str), "%2f", VIK_METERS_TO_FEET(wp->geoidheight) );
      break;
    default:
      g_snprintf ( tmp_str, sizeof(tmp_str), "%2f", wp->geoidheight );
      break;
    }
  }
  GtkWidget *geoidhgtentry = ui_entry_new ( tmp_str, GTK_ENTRY_ICON_SECONDARY );
  gtk_box_pack_start ( GTK_BOX(extra), geoidhgtlabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), geoidhgtentry, FALSE, FALSE, 0 );

  GtkWidget *urllabel = NULL;
  if ( wp->url && !strncmp(wp->url, "http", 4) )
    urllabel = gtk_link_button_new_with_label (wp->url, _("URL:") );
  else
    urllabel = gtk_label_new (_("URL:"));
  GtkWidget *urlentry = ui_entry_new ( wp->url, GTK_ENTRY_ICON_SECONDARY );
  GtkWidget *urlnamelabel = gtk_label_new (_("URL Name:")); 
  GtkWidget *urlnameentry = ui_entry_new ( wp->url_name, GTK_ENTRY_ICON_SECONDARY );

  gtk_box_pack_start ( GTK_BOX(extra), urllabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), urlentry, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), urlnamelabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), urlnameentry, FALSE, FALSE, 0 );

  // Combine fix & satellites into one line??
  if ( wp->fix_mode )
    g_snprintf ( msg, sizeof(msg), "%d", wp->fix_mode );
  else
    g_snprintf ( msg, sizeof(msg), "--" );
  GtkWidget *fixlabel = gtk_label_new ( _("Fix Mode:") );
  GtkWidget *fixvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), fixlabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), fixvalue, FALSE, FALSE, 0 );

  if ( wp->nsats )
    g_snprintf ( msg, sizeof(msg), "%d", wp->nsats );
  else
    g_snprintf ( msg, sizeof(msg), "--" );
  GtkWidget *satlabel = gtk_label_new ( _("Satellites:") );
  GtkWidget *satvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), satlabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), satvalue, FALSE, FALSE, 0 );

  if ( isnan(wp->hdop) )
    g_snprintf ( msg, sizeof(msg), "--" );
  else
    g_snprintf ( msg, sizeof(msg), "%.3f", wp->hdop ); // TODO feet version...
  GtkWidget *hdoplabel = gtk_label_new ( _("HDOP:") );
  GtkWidget *hdopvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), hdoplabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), hdopvalue, FALSE, FALSE, 0 );

  if ( isnan(wp->vdop) )
    g_snprintf ( msg, sizeof(msg), "--" );
  else
    g_snprintf ( msg, sizeof(msg), "%.3f", wp->vdop ); // TODO feet version...
  GtkWidget *vdoplabel = gtk_label_new ( _("VDOP:") );
  GtkWidget *vdopvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), vdoplabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), vdopvalue, FALSE, FALSE, 0 );

  if ( isnan(wp->pdop) )
    g_snprintf ( msg, sizeof(msg), "--" );
  else
    g_snprintf ( msg, sizeof(msg), "%.3f", wp->pdop ); // TODO feet version...
  GtkWidget *pdoplabel = gtk_label_new ( _("PDOP:") );
  GtkWidget *pdopvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), pdoplabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), pdopvalue, FALSE, FALSE, 0 );

  if ( isnan(wp->ageofdgpsdata) )
    g_snprintf ( msg, sizeof(msg), "--" );
  else
    g_snprintf ( msg, sizeof(msg), "%.3f", wp->ageofdgpsdata ); // TODO feet version...
  GtkWidget *agedlabel = gtk_label_new ( _("Age of DGPS Data:") );
  GtkWidget *agedvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), agedlabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), agedvalue, FALSE, FALSE, 0 );

  if ( wp->dgpsid )
    g_snprintf ( msg, sizeof(msg), "%d", wp->dgpsid );
  else
    g_snprintf ( msg, sizeof(msg), "--" );
  GtkWidget *dgpsidlabel = gtk_label_new ( _("DGPS Station Id:") );
  GtkWidget *dgpsidvalue = ui_label_new_selectable(msg); // NB No edit ATM
  gtk_box_pack_start ( GTK_BOX(extra), dgpsidlabel, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(extra), dgpsidvalue, FALSE, FALSE, 0 );

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  gtk_notebook_append_page ( GTK_NOTEBOOK(tabs), GTK_WIDGET(basic), gtk_label_new(_("Basic")) );
  gtk_notebook_append_page ( GTK_NOTEBOOK(tabs), GTK_WIDGET(extra), gtk_label_new(_("Extra")) );

  if ( wp->extensions ) {
    GtkWidget *sw = gtk_scrolled_window_new ( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    GtkWidget *ext = ui_label_new_selectable ( wp->extensions );
    gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW(sw), ext );
    gtk_notebook_append_page ( GTK_NOTEBOOK(tabs), sw, gtk_label_new(_("GPX Extensions")) );
  }
  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), GTK_WIDGET(tabs), FALSE, FALSE, 0);

  gtk_widget_show_all ( gtk_dialog_get_content_area(GTK_DIALOG(dialog)) );

  if ( !is_new ) {
    // Shift left<->right to try not to obscure the waypoint.
    trw_layer_dialog_shift ( vtl, GTK_WINDOW(dialog), &(wp->coord), FALSE );
  }

  while ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    gchar const *crstext = gtk_entry_get_text ( GTK_ENTRY(crsentry) );
    gdouble crsd = NAN;
    if ( crstext && strlen(crstext) )
      crsd = atof ( crstext );

    if ( strlen((gchar*)gtk_entry_get_text ( GTK_ENTRY(nameentry) )) == 0 ) /* TODO: other checks (isalpha or whatever ) */
      a_dialog_info_msg ( parent, _("Please enter a name for the waypoint.") );
    else if ( !isnan(crsd) && ( crsd < 0.0 || crsd > 360.0 ) )
      a_dialog_warning_msg ( parent, _("Course value must be between 0 and 360.") );
    else {
      // NB: No check for unique names - this allows generation of same named entries.
      gchar *entered_name = g_strdup ( (gchar*)gtk_entry_get_text ( GTK_ENTRY(nameentry) ) );

      /* Do It */
      ll.lat = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(latentry) ) );
      ll.lon = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(lonentry) ) );
      vik_coord_load_from_latlon ( &(wp->coord), coord_mode, &ll );
      gchar const *alttext = gtk_entry_get_text ( GTK_ENTRY(altentry) );
      if ( alttext && strlen(alttext) ) {
        // Always store in metres
        switch (height_units) {
        case VIK_UNITS_HEIGHT_METRES:
          wp->altitude = atof ( alttext );
          break;
        case VIK_UNITS_HEIGHT_FEET:
          wp->altitude = VIK_FEET_TO_METERS(atof ( alttext ));
          break;
        default:
          wp->altitude = atof ( alttext );
          g_critical("Houston, we've had a problem. height=%d", height_units);
        }
      }
      else {
        wp->altitude = NAN;
      }
      if ( g_strcmp0 ( wp->comment, gtk_entry_get_text ( GTK_ENTRY(commententry) ) ) )
        vik_waypoint_set_comment ( wp, gtk_entry_get_text ( GTK_ENTRY(commententry) ) );
      if ( g_strcmp0 ( wp->description, gtk_entry_get_text ( GTK_ENTRY(descriptionentry) ) ) )
        vik_waypoint_set_description ( wp, gtk_entry_get_text ( GTK_ENTRY(descriptionentry) ) );
      if ( g_strcmp0 ( wp->image, vik_file_entry_get_filename ( VIK_FILE_ENTRY(imageentry) ) ) )
        vik_waypoint_set_image ( wp, vik_file_entry_get_filename ( VIK_FILE_ENTRY(imageentry) ) );
      if ( sourceentry && g_strcmp0 ( wp->source, gtk_entry_get_text ( GTK_ENTRY(sourceentry) ) ) )
        vik_waypoint_set_source ( wp, gtk_entry_get_text ( GTK_ENTRY(sourceentry) ) );
      if ( typeentry && g_strcmp0 ( wp->type, gtk_entry_get_text ( GTK_ENTRY(typeentry) ) ) )
        vik_waypoint_set_type ( wp, gtk_entry_get_text ( GTK_ENTRY(typeentry) ) );
      if ( wp->image && *(wp->image) && (!a_thumbnails_exists(wp->image)) )
        a_thumbnails_create ( wp->image );

      wp->timestamp = edit_wp->timestamp;

      if ( direction_sb ) {
        if ( gtk_widget_get_sensitive (direction_sb) ) {
          wp->image_direction = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(direction_sb) );
          if ( wp->image_direction != edit_wp->image_direction )
            a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, wp->image_direction, wp->image_direction_ref, TRUE );
        }
      }

      g_free ( last_sym );
      GtkTreeIter iter, first;
      gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(store), &first );
      if ( !gtk_combo_box_get_active_iter ( GTK_COMBO_BOX(symbolentry), &iter ) || !memcmp(&iter, &first, sizeof(GtkTreeIter)) ) {
        vik_waypoint_set_symbol ( wp, NULL );
        last_sym = NULL;
      } else {
        gchar *sym;
        gtk_tree_model_get ( GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1 );
        vik_waypoint_set_symbol ( wp, sym );
        last_sym = g_strdup ( sym );
        g_free(sym);
      }

      // Extra tab data
      wp->course = crsd;

      gchar const *spdtext = gtk_entry_get_text ( GTK_ENTRY(speedentry) );
      if ( spdtext && strlen(spdtext) ) {
        wp->speed = get_speed_from_text ( speed_units, spdtext );
      }
      else {
        wp->speed = NAN;
      }

      gchar const *ghtext = gtk_entry_get_text ( GTK_ENTRY(geoidhgtentry) );
      if ( ghtext && strlen(ghtext) ) {
        // Always store in metres
        switch (height_units) {
        case VIK_UNITS_HEIGHT_FEET:
          wp->geoidheight = VIK_FEET_TO_METERS(atof(ghtext));
          break;
        default:
          // VIK_UNITS_HEIGHT_METRES:
          wp->geoidheight = atof ( ghtext );
        }
      }
      else {
        wp->geoidheight = NAN;
      }

      if ( g_strcmp0 ( wp->url, gtk_entry_get_text ( GTK_ENTRY(urlentry) ) ) )
        vik_waypoint_set_url ( wp, gtk_entry_get_text ( GTK_ENTRY(urlentry) ) );
      if ( g_strcmp0 ( wp->url_name, gtk_entry_get_text ( GTK_ENTRY(urlnameentry) ) ) )
        vik_waypoint_set_url_name ( wp, gtk_entry_get_text ( GTK_ENTRY(urlnameentry) ) );

      gtk_widget_destroy ( dialog );
      if ( is_new )
        return entered_name;
      else {
        *updated = TRUE;
        // See if name has been changed
        if ( g_strcmp0 (default_name, entered_name ) )
          return entered_name;
        else
          return NULL;
      }
    }
  }
  gtk_widget_destroy ( dialog );
  return NULL;
}
