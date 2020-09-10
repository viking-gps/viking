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

static VikWaypoint *edit_wp = NULL;
static gulong direction_signal_id = 0;
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
  gtk_button_set_relief ( GTK_BUTTON(widget), GTK_RELIEF_NONE );
  g_signal_handler_disconnect ( G_OBJECT(widget), direction_signal_id );
  direction_signal_id = 0;
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

struct _VikTrwLayerWpwin {
  GtkWidget *dialog;
  GtkWindow *parent;
  VikTrwLayer *vtl;
  VikWaypoint *wpt;
  VikCoordMode coord_mode;
  GtkWidget *tabs;
  GtkWidget *latentry, *lonentry, *nameentry, *nameshowcb, *altentry;
  GtkWidget *commentlabel, *commententry, *descriptionlabel, *descriptionentry, *imagelabel, *imageentry, *symbolentry;
  GtkWidget *sourcelabel, *sourceentry, *typeentry;
  GtkWidget *timevaluebutton;
  GtkWidget *hasGeotagCB;
  GtkWidget *consistentGeotagCB;
  GtkWidget *direction_sb;
  GtkWidget *direction_hb;
  GtkWidget *direction_ref;
  GtkListStore *store;
  GtkWidget *crsentry;
  GtkWidget *speedentry;
  GtkWidget *magvarvalue;
  GtkWidget *geoidhgtentry;
  GtkWidget *urlbutton;
  GtkWidget *urlentry;
  GtkWidget *urlnameentry;
  GtkWidget *fixvalue;
  GtkWidget *satvalue;
  GtkWidget *hdopvalue;
  GtkWidget *vdopvalue;
  GtkWidget *pdopvalue;
  GtkWidget *agedvalue;
  GtkWidget *dgpsidvalue;
  GtkWidget *extlab;
  GtkWidget *extsw;
  gboolean is_new;
};

static VikTrwLayerWpwin *VikTrwLayerWpwin_new()
{
  VikTrwLayerWpwin *widgets = g_malloc0(sizeof(VikTrwLayerWpwin));
  return widgets;
}

static void VikTrwLayerWpwin_free ( VikTrwLayerWpwin *widgets )
{
  direction_signal_id = 0;
  g_free ( widgets );
}

static void trw_layer_wpwin_response ( VikTrwLayerWpwin *ww, gint response );

static void set_button_url ( GtkWidget *widget, gchar *str )
{
  // GTK too stupid and shows a blank tooltip when ""
  //  and also shows a tooltip even when insensitized, so turn it off
  gboolean is_url = FALSE;
  if ( str ) {
    char *scheme = g_uri_parse_scheme ( str );
    if ( scheme ) {
      gtk_link_button_set_uri ( GTK_LINK_BUTTON(widget), str );
      is_url = TRUE;
    } else {
      gtk_link_button_set_uri ( GTK_LINK_BUTTON(widget), "" );
    }
    g_free ( scheme );
  }
  g_object_set ( widget, "has-tooltip", is_url, NULL );
}

static void set_text_uint ( GtkWidget *widget, guint value )
{
  char msg[64];
  if ( value )
    g_snprintf ( msg, sizeof(msg), "%d", value );
  else
    g_snprintf ( msg, sizeof(msg), "--" );
  gtk_label_set_text ( GTK_LABEL(widget), msg );
}

static void set_widget_double ( GtkWidget *widget, gdouble value, gchar *format, vik_units_height_t height_units )
{
  char tmp_str[64];
  if ( isnan(value) )
    tmp_str[0] = '\0';
  else {
    switch (height_units) {
    case VIK_UNITS_HEIGHT_FEET:
      g_snprintf ( tmp_str, sizeof(tmp_str), format, VIK_METERS_TO_FEET(value) );
      break;
      // METRES:
    default:
      g_snprintf ( tmp_str, sizeof(tmp_str), format, value );
      break;
    }
  }
  if ( GTK_IS_ENTRY(widget) )
    ui_entry_set_text ( widget, tmp_str );
  else {
    if ( tmp_str[0] )
      gtk_label_set_text ( GTK_LABEL(widget), tmp_str );
    else
      gtk_label_set_text ( GTK_LABEL(widget), "--" );
  }
}

/**
 * vik_trw_layer_wpwin_show:
 * @cur_wpwin: If a pointer to an existing dialog is specified than that dialog will be updated.
 *
 * Show or update the Waypoint Properties dialog.
 *
 * When on an existing waypoint the dialog is kept open and a pointer containing this dialog is returned.
 * When for a new waypoint the dialog is closed and a boolean is returned (via overloading the pointer)
 *  indicating whether the waypoint is truly wanted or else cancelled.
 *
 */
VikTrwLayerWpwin *vik_trw_layer_wpwin_show ( GtkWindow *parent, VikTrwLayerWpwin *cur_wpwin, gchar *default_name, VikTrwLayer *vtl, VikWaypoint *wp, VikCoordMode coord_mode, gboolean is_new )
{
  VikTrwLayerWpwin *ww = NULL;

  GtkWidget *dialog = NULL;
  vik_units_height_t height_units = a_vik_get_units_height ();
  vik_units_speed_t speed_units = a_vik_get_units_speed ();

  if ( !edit_wp )
    edit_wp = vik_waypoint_new ();
  else {
    vik_waypoint_free ( edit_wp );
    edit_wp = vik_waypoint_copy ( wp );
  }

  if ( cur_wpwin ) {
    ww = cur_wpwin;
    ww->coord_mode = coord_mode;
    ww->wpt = wp;
    ww->vtl = vtl;
    ww->is_new = is_new; // Of course shouldn't be TRUE with an existing dialog
    dialog = ww->dialog;
  }
  else {
    ww = VikTrwLayerWpwin_new();
    ww->coord_mode = coord_mode;
    ww->wpt = wp;
    ww->vtl = vtl;
    ww->is_new = is_new;

    dialog = gtk_dialog_new_with_buttons ( _("Waypoint Properties"),
                                           parent, 0,
                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                           NULL );
    if ( !is_new )
      (void)gtk_dialog_add_button ( GTK_DIALOG(dialog), GTK_STOCK_APPLY, GTK_RESPONSE_APPLY );
    (void)gtk_dialog_add_button ( GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT );

    ww->parent = parent;
    ww->dialog = dialog;

    ww->tabs = gtk_notebook_new();
    GtkWidget *basic = gtk_vbox_new ( FALSE, 0 );

    GtkWidget *namelabel = gtk_label_new (_("Name:"));
    GtkWidget *namehb = gtk_hbox_new ( FALSE, 0 );
    ww->nameshowcb = gtk_check_button_new_with_label ( _("Show Name") );
    gtk_box_pack_start ( GTK_BOX(namehb), namelabel, TRUE, TRUE, 0 );
    gtk_box_pack_start ( GTK_BOX(namehb), ww->nameshowcb, TRUE, TRUE, 0 );
    gtk_box_pack_start ( GTK_BOX(basic), namehb, FALSE, FALSE, 0 );
    // Name is now always changeable
    ww->nameentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
    g_signal_connect_swapped ( ww->nameentry, "activate", G_CALLBACK(a_dialog_response_accept), GTK_DIALOG(dialog) );
    gtk_box_pack_start (GTK_BOX(basic), ww->nameentry, FALSE, FALSE, 0);

    GtkWidget *latlabel = gtk_label_new (_("Latitude:"));
    ww->latentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    GtkWidget *lonlabel = gtk_label_new (_("Longitude:"));
    ww->lonentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    GtkWidget *altlabel = gtk_label_new (_("Altitude:"));
    ww->altentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    ww->commentlabel = gtk_link_button_new_with_label ( "", _("Comment:") );
    ww->commententry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    ww->descriptionlabel = gtk_link_button_new_with_label ( "", _("Description:") );
    ww->descriptionentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    ww->sourcelabel = gtk_link_button_new_with_label ( "", _("Source:") );
    ww->sourceentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    GtkWidget *typelabel = gtk_label_new (_("Type:"));
    ww->typeentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    ww->imagelabel = gtk_link_button_new_with_label ( "", _("Image:") );
    ww->imageentry = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, VF_FILTER_IMAGE, NULL, NULL);

    GtkCellRenderer *r;
    GtkWidget *symbollabel = gtk_label_new (_("Symbol:"));
    GtkTreeIter iter;

    GtkListStore *store = gtk_list_store_new(3, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING);
    ww->store = store;
    ww->symbolentry = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(ww->symbolentry), 6);

    g_signal_connect(ww->symbolentry, "changed", G_CALLBACK(symbol_entry_changed_cb), store);
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0, NULL, 1, NULL, 2, _("(none)"), -1);
    a_populate_sym_list(store);

    r = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ww->symbolentry), r, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (ww->symbolentry), r, "pixbuf", 1, NULL);

    r = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ww->symbolentry), r, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (ww->symbolentry), r, "text", 2, NULL);

#ifdef VIK_CONFIG_GEOTAG
    // Geotag Info [readonly]
    ww->hasGeotagCB = gtk_check_button_new_with_label ( _("Has Geotag") );
    gtk_widget_set_sensitive ( ww->hasGeotagCB, FALSE );

    ww->consistentGeotagCB = gtk_check_button_new_with_label ( _("Consistent Position") );
    gtk_widget_set_sensitive ( ww->consistentGeotagCB, FALSE );

    GtkWidget *direction_label = gtk_label_new ( _("Image Direction:") );
    ww->direction_hb = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start (GTK_BOX(ww->direction_hb), direction_label, FALSE, FALSE, 0);
    ww->direction_sb = gtk_spin_button_new ( (GtkAdjustment*)gtk_adjustment_new (0, 0.0, 359.9, 5.0, 1, 0 ), 1, 1 );

    ww->direction_ref = gtk_button_new ();
    gtk_box_pack_start (GTK_BOX(ww->direction_hb), ww->direction_ref, TRUE, FALSE, 0);    
#endif

    GtkWidget *timelabel = gtk_label_new ( _("Time:") );
    ww->timevaluebutton = gtk_button_new();
    gtk_button_set_relief ( GTK_BUTTON(ww->timevaluebutton), GTK_RELIEF_NONE );

    g_signal_connect ( G_OBJECT(ww->timevaluebutton), "button-release-event", G_CALLBACK(time_edit_click), edit_wp );

    gtk_box_pack_start (GTK_BOX(basic), latlabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->latentry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), lonlabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->lonentry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), timelabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->timevaluebutton, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), altlabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->altentry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->commentlabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->commententry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->descriptionlabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->descriptionentry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->sourcelabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->sourceentry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), typelabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->typeentry, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->imagelabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), ww->imageentry, FALSE, FALSE, 0);

    if ( ww->hasGeotagCB ) {
      GtkWidget *hbox =  gtk_hbox_new ( FALSE, 0 );
      gtk_box_pack_start (GTK_BOX(hbox), ww->hasGeotagCB, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX(hbox), ww->consistentGeotagCB, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX(basic), hbox, FALSE, FALSE, 0);
    }
    if ( ww->direction_hb )
      gtk_box_pack_start (GTK_BOX(basic), ww->direction_hb, FALSE, FALSE, 0);
    if ( ww->direction_sb )
      gtk_box_pack_start (GTK_BOX(basic), ww->direction_sb, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), symbollabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(basic), GTK_WIDGET(ww->symbolentry), FALSE, FALSE, 0);

    // 'Extra' level of Waypoint properties
    GtkWidget *extra = gtk_vbox_new ( FALSE, 0 );

    GtkWidget *crslabel = gtk_label_new ( _("Course:") );
    ww->crsentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    gtk_box_pack_start ( GTK_BOX(extra), crslabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->crsentry, FALSE, FALSE, 0 );

    char tmp_lab[64];
    g_snprintf ( tmp_lab, sizeof(tmp_lab), _("Speed: (%s)"), vu_speed_units_text(speed_units) );
    GtkWidget *speedlabel = gtk_label_new ( tmp_lab );
    ww->speedentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    gtk_box_pack_start ( GTK_BOX(extra), speedlabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->speedentry, FALSE, FALSE, 0 );

    GtkWidget *magvarlabel = gtk_label_new ( _("Magnetic Variance:") );
    ww->magvarvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), magvarlabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->magvarvalue, FALSE, FALSE, 0 );

    GtkWidget *geoidhgtlabel = gtk_label_new ( _("Geoid Height:") );
    ww->geoidhgtentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
    gtk_box_pack_start ( GTK_BOX(extra), geoidhgtlabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->geoidhgtentry, FALSE, FALSE, 0 );

    ww->urlbutton = gtk_link_button_new_with_label ( "", _("URL:") );
    ww->urlentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
    GtkWidget *urlnamelabel = gtk_label_new (_("URL Name:"));
    ww->urlnameentry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

    gtk_box_pack_start ( GTK_BOX(extra), ww->urlbutton, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->urlentry, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), urlnamelabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->urlnameentry, FALSE, FALSE, 0 );

    // Combine fix & satellites into one line??
    GtkWidget *fixlabel = gtk_label_new ( _("Fix Mode:") );
    ww->fixvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), fixlabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->fixvalue, FALSE, FALSE, 0 );

    GtkWidget *satlabel = gtk_label_new ( _("Satellites:") );
    ww->satvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), satlabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->satvalue, FALSE, FALSE, 0 );

    GtkWidget *hdoplabel = gtk_label_new ( _("HDOP:") );
    ww->hdopvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), hdoplabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->hdopvalue, FALSE, FALSE, 0 );

    GtkWidget *vdoplabel = gtk_label_new ( _("VDOP:") );
    ww->vdopvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), vdoplabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->vdopvalue, FALSE, FALSE, 0 );

    GtkWidget *pdoplabel = gtk_label_new ( _("PDOP:") );
    ww->pdopvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), pdoplabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->pdopvalue, FALSE, FALSE, 0 );

    GtkWidget *agedlabel = gtk_label_new ( _("Age of DGPS Data:") );
    ww->agedvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), agedlabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->agedvalue, FALSE, FALSE, 0 );

    GtkWidget *dgpsidlabel = gtk_label_new ( _("DGPS Station Id:") );
    ww->dgpsidvalue = ui_label_new_selectable ( NULL ); // NB No edit ATM
    gtk_box_pack_start ( GTK_BOX(extra), dgpsidlabel, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX(extra), ww->dgpsidvalue, FALSE, FALSE, 0 );

    gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

    gtk_notebook_append_page ( GTK_NOTEBOOK(ww->tabs), GTK_WIDGET(basic), gtk_label_new(_("Basic")) );
    gtk_notebook_append_page ( GTK_NOTEBOOK(ww->tabs), GTK_WIDGET(extra), gtk_label_new(_("Extra")) );

    ww->extsw = gtk_scrolled_window_new ( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(ww->extsw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    ww->extlab = ui_label_new_selectable ( NULL );
    gtk_widget_set_can_focus ( ww->extlab, FALSE );  // Don't let notebook autofocus on it
    gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW(ww->extsw), ww->extlab );
    gtk_notebook_append_page ( GTK_NOTEBOOK(ww->tabs), ww->extsw, gtk_label_new(_("GPX Extensions")) );

    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), GTK_WIDGET(ww->tabs), FALSE, FALSE, 0);
  }

  gchar *alt;
  struct LatLon ll;
  vik_coord_to_latlon ( &(wp->coord), &ll );

  gchar *lat = g_strdup_printf ( "%f", ll.lat );
  gchar *lon = g_strdup_printf ( "%f", ll.lon );
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

  gtk_entry_set_text ( GTK_ENTRY(ww->latentry), lat );
  gtk_entry_set_text ( GTK_ENTRY(ww->lonentry), lon );
  ui_entry_set_text ( ww->altentry, alt );

  g_free ( lat );
  g_free ( lon );
  g_free ( alt );

  ui_entry_set_text ( ww->nameentry, default_name );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(ww->nameshowcb), !wp->hide_name );

  if ( is_new ) {
    // Auto put in some kind of 'name' as a comment if one previously 'goto'ed this exact location
    gchar *cmt = a_vik_goto_get_search_string_for_this_place(VIK_WINDOW(parent));
    ui_entry_set_text ( ww->commententry, cmt );
    set_button_url ( ww->commentlabel, NULL );
  } else {
    ui_entry_set_text ( ww->commententry, wp->comment );
    set_button_url ( ww->commentlabel, wp->comment );
  }

  ui_entry_set_text ( ww->descriptionentry, wp->description );
  set_button_url ( ww->descriptionlabel, wp->description );

  ui_entry_set_text ( ww->sourceentry, wp->source );
  set_button_url ( ww->sourcelabel, wp->source );

  ui_entry_set_text ( ww->typeentry, wp->type );

  if ( !is_new && wp->image )
    vik_file_entry_set_filename ( VIK_FILE_ENTRY(ww->imageentry), wp->image );
  set_button_url ( ww->imagelabel, wp->image );

  if ( wp->symbol || (is_new && last_sym) ) {
    gboolean ok;
    gchar *sym;
    GtkTreeIter iter;
    for (ok = gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(ww->store), &iter ); ok; ok = gtk_tree_model_iter_next ( GTK_TREE_MODEL(ww->store), &iter)) {
      gtk_tree_model_get ( GTK_TREE_MODEL(ww->store), &iter, 0, (void *)&sym, -1 );
      if (sym && ((!is_new && !g_strcmp0(sym, wp->symbol)) || (is_new && !g_strcmp0(sym, last_sym))) ) {
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
      gtk_combo_box_set_active_iter(GTK_COMBO_BOX(ww->symbolentry), &iter);
  } else {
    // Reset to first in the store - the blank symbol
    GtkTreeIter iter;
    if ( gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ww->store), &iter) )
      gtk_combo_box_set_active_iter ( GTK_COMBO_BOX(ww->symbolentry), &iter );
  }

#ifdef VIK_CONFIG_GEOTAG
  if ( !is_new && wp->image ) {
    gboolean hasGeotag;
    gchar *ignore = a_geotag_get_exif_date_from_file ( wp->image, &hasGeotag );
    g_free ( ignore );
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(ww->hasGeotagCB), hasGeotag );

    if ( hasGeotag ) {
      struct LatLon ll = a_geotag_get_position ( wp->image );
      VikCoord coord;
      vik_coord_load_from_latlon ( &coord, ww->coord_mode, &ll );
      gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(ww->consistentGeotagCB), vik_coord_equalish(&coord, &wp->coord) );
    }
  }

  if ( !is_new && !isnan(wp->image_direction) ) {
    if ( gtk_button_get_image ( GTK_BUTTON(ww->direction_ref) ) )
      gtk_button_set_image ( GTK_BUTTON(ww->direction_ref), NULL );
    gtk_button_set_relief ( GTK_BUTTON(ww->direction_ref), GTK_RELIEF_NONE );
    if ( wp->image_direction_ref == WP_IMAGE_DIRECTION_REF_MAGNETIC )
      gtk_button_set_label ( GTK_BUTTON(ww->direction_ref), _("Magnetic") );
    else
      gtk_button_set_label ( GTK_BUTTON(ww->direction_ref), _("True") );
    if ( direction_signal_id ) {
      g_signal_handler_disconnect ( G_OBJECT(ww->direction_ref), direction_signal_id );
      direction_signal_id = 0;
    }

    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(ww->direction_sb), wp->image_direction );
    gtk_widget_set_sensitive ( ww->direction_sb, TRUE );
  } else {
    if ( !gtk_button_get_image ( GTK_BUTTON(ww->direction_ref) ) ) {
      if ( direction_signal_id == 0 )
        direction_signal_id = g_signal_connect ( G_OBJECT(ww->direction_ref), "button-release-event", G_CALLBACK(direction_add_click), ww->direction_sb );
      gtk_button_set_relief ( GTK_BUTTON(ww->direction_ref), GTK_RELIEF_NORMAL );
      gtk_button_set_label ( GTK_BUTTON(ww->direction_ref), NULL );
      GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU );
      gtk_button_set_image ( GTK_BUTTON(ww->direction_ref), img );
    }
    gtk_widget_set_sensitive ( ww->direction_sb, FALSE );
  }
#endif

  if ( !is_new && !isnan(wp->timestamp) ) {
    if ( gtk_button_get_image ( GTK_BUTTON(ww->timevaluebutton) ) )
      gtk_button_set_image ( GTK_BUTTON(ww->timevaluebutton), NULL );
    gtk_button_set_relief ( GTK_BUTTON(ww->timevaluebutton), GTK_RELIEF_NONE );
    update_time ( ww->timevaluebutton, wp );
  }
  else {
    gtk_button_set_relief ( GTK_BUTTON(ww->timevaluebutton), GTK_RELIEF_NORMAL );
    gtk_button_set_label ( GTK_BUTTON(ww->timevaluebutton), NULL );
    GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU );
    gtk_button_set_image ( GTK_BUTTON(ww->timevaluebutton), img );
  }

  gchar *crs = isnan(wp->course) ? NULL : g_strdup_printf ( "%05.2f", wp->course );
  ui_entry_set_text ( ww->crsentry, crs );
  g_free ( crs );

  char tmp_str[64];
  gdouble my_speed = wp->speed;
  if ( isnan(my_speed) )
    tmp_str[0] = '\0';
  else {
    my_speed = vu_speed_convert ( speed_units, my_speed );
    vu_speed_text_value ( tmp_str, sizeof(tmp_str), speed_units, my_speed, "%.2f" );
  }
  ui_entry_set_text ( ww->speedentry, tmp_str );

  ui_entry_set_text ( ww->urlentry, wp->url );
  set_button_url ( ww->urlbutton, wp->url );
  ui_entry_set_text ( ww->urlnameentry, wp->url_name );

  set_widget_double ( ww->geoidhgtentry, wp->geoidheight, "%2f", height_units );
  set_widget_double ( ww->hdopvalue, wp->hdop, "%0.3f", height_units );
  set_widget_double ( ww->vdopvalue, wp->vdop, "%0.3f", height_units );
  set_widget_double ( ww->pdopvalue, wp->pdop, "%0.3f", height_units );
  // Fake the units to ensure values not converted
  set_widget_double ( ww->magvarvalue, wp->magvar, "%05.2f", VIK_UNITS_HEIGHT_METRES );
  set_widget_double ( ww->agedvalue, wp->ageofdgpsdata, "%0.3f", VIK_UNITS_HEIGHT_METRES );

  set_text_uint ( ww->fixvalue, wp->fix_mode );
  set_text_uint ( ww->satvalue, wp->nsats );
  set_text_uint ( ww->dgpsidvalue, wp->dgpsid );

  gtk_label_set_text ( GTK_LABEL(ww->extlab), wp->extensions );
  GtkWidget *ext_tab = gtk_notebook_get_tab_label ( GTK_NOTEBOOK(ww->tabs), ww->extsw );
  gtk_widget_set_sensitive ( ext_tab, wp->extensions ? TRUE : FALSE );

  gtk_widget_show_all ( dialog );

  g_signal_connect_swapped ( dialog, "response", G_CALLBACK(trw_layer_wpwin_response), ww );

  if ( !is_new && !cur_wpwin ) {
    // Shift left<->right to try not to obscure the waypoint.
    trw_layer_dialog_shift ( vtl, GTK_WINDOW(dialog), &(wp->coord), FALSE );
  }

  // Run effectively as a 'modal' dialog when used for waypoint creation
  if ( is_new ) {
    //  NB gtk_dialog_run () always returns GTK_RESPONSE_NONE
    //   after the trw_layer_wpwin_response() has ran
    (void)gtk_dialog_run ( GTK_DIALOG(dialog) );
    // Thus use data passed back via 'ww' to see if the dialog was OKed
    VikTrwLayerWpwin *ptr_hack = GUINT_TO_POINTER (ww->is_new);
    VikTrwLayerWpwin_free ( ww );
    return ptr_hack;
  }
  return ww;
}

static void trw_layer_wpwin_response ( VikTrwLayerWpwin *ww, gint response )
{
  VikWaypoint *wp = ww->wpt;
  struct LatLon ll;

  if ( response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_APPLY )
  {
    gchar const *crstext = gtk_entry_get_text ( GTK_ENTRY(ww->crsentry) );
    gdouble crsd = NAN;
    if ( crstext && strlen(crstext) )
      crsd = atof ( crstext );

    // Some checks on valid input
    if ( strlen((gchar*)gtk_entry_get_text ( GTK_ENTRY(ww->nameentry) )) == 0 )
      a_dialog_info_msg ( ww->parent, _("Please enter a name for the waypoint.") );
    else if ( !isnan(crsd) && ( crsd < 0.0 || crsd > 360.0 ) )
      a_dialog_warning_msg ( ww->parent, _("Course value must be between 0 and 360.") );
    else {
      // Do It
      vik_units_height_t height_units = a_vik_get_units_height ();
      ll.lat = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(ww->latentry) ) );
      ll.lon = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(ww->lonentry) ) );
      vik_coord_load_from_latlon ( &(wp->coord), ww->coord_mode, &ll );
      gchar const *alttext = gtk_entry_get_text ( GTK_ENTRY(ww->altentry) );
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
      vik_waypoint_set_name ( wp, gtk_entry_get_text ( GTK_ENTRY(ww->nameentry) ) );
      wp->hide_name = !gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(ww->nameshowcb) );
      if ( g_strcmp0 ( wp->comment, gtk_entry_get_text ( GTK_ENTRY(ww->commententry) ) ) )
        vik_waypoint_set_comment ( wp, gtk_entry_get_text ( GTK_ENTRY(ww->commententry) ) );
      if ( g_strcmp0 ( wp->description, gtk_entry_get_text ( GTK_ENTRY(ww->descriptionentry) ) ) )
        vik_waypoint_set_description ( wp, gtk_entry_get_text ( GTK_ENTRY(ww->descriptionentry) ) );
      if ( g_strcmp0 ( wp->image, vik_file_entry_get_filename ( VIK_FILE_ENTRY(ww->imageentry) ) ) )
        vik_waypoint_set_image ( wp, vik_file_entry_get_filename ( VIK_FILE_ENTRY(ww->imageentry) ) );
      if ( ww->sourceentry && g_strcmp0 ( wp->source, gtk_entry_get_text ( GTK_ENTRY(ww->sourceentry) ) ) )
        vik_waypoint_set_source ( wp, gtk_entry_get_text ( GTK_ENTRY(ww->sourceentry) ) );
      if ( ww->typeentry && g_strcmp0 ( wp->type, gtk_entry_get_text ( GTK_ENTRY(ww->typeentry) ) ) )
        vik_waypoint_set_type ( wp, gtk_entry_get_text ( GTK_ENTRY(ww->typeentry) ) );
      if ( wp->image && *(wp->image) && (!a_thumbnails_exists(wp->image)) )
        a_thumbnails_create ( wp->image );

      wp->timestamp = edit_wp->timestamp;

      if ( ww->direction_sb ) {
        if ( gtk_widget_get_sensitive (ww->direction_sb) ) {
          wp->image_direction = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(ww->direction_sb) );
          if ( wp->image && wp->image_direction != edit_wp->image_direction )
            a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, wp->image_direction, wp->image_direction_ref, TRUE );
        }
      }

      g_free ( last_sym );
      GtkTreeIter iter, first;
      gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(ww->store), &first );
      if ( !gtk_combo_box_get_active_iter ( GTK_COMBO_BOX(ww->symbolentry), &iter ) || !memcmp(&iter, &first, sizeof(GtkTreeIter)) ) {
        vik_waypoint_set_symbol ( wp, NULL );
        last_sym = NULL;
      } else {
        gchar *sym;
        gtk_tree_model_get ( GTK_TREE_MODEL(ww->store), &iter, 0, (void *)&sym, -1 );
        vik_waypoint_set_symbol ( wp, sym );
        last_sym = g_strdup ( sym );
        g_free(sym);
      }

      // Extra tab data
      vik_units_speed_t speed_units = a_vik_get_units_speed ();
      wp->course = crsd;

      gchar const *spdtext = gtk_entry_get_text ( GTK_ENTRY(ww->speedentry) );
      if ( spdtext && strlen(spdtext) ) {
        wp->speed = get_speed_from_text ( speed_units, spdtext );
      }
      else {
        wp->speed = NAN;
      }

      gchar const *ghtext = gtk_entry_get_text ( GTK_ENTRY(ww->geoidhgtentry) );
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

      if ( g_strcmp0 ( wp->url, gtk_entry_get_text ( GTK_ENTRY(ww->urlentry) ) ) )
        vik_waypoint_set_url ( wp, gtk_entry_get_text ( GTK_ENTRY(ww->urlentry) ) );
      if ( g_strcmp0 ( wp->url_name, gtk_entry_get_text ( GTK_ENTRY(ww->urlnameentry) ) ) )
        vik_waypoint_set_url_name ( wp, gtk_entry_get_text ( GTK_ENTRY(ww->urlnameentry) ) );

      // Now that the wpt itself has been updated, need to update the display
      //  could use a signal method, but a direct function call is simple
      trw_layer_waypoint_properties_changed ( ww->vtl, ww->wpt );
    }
  }

  // Keep dialog open?
  if ( response == GTK_RESPONSE_APPLY )
    return;

  gtk_widget_destroy ( GTK_WIDGET(ww->dialog) );
   // When a 'new waypoint' freeing is handled by the dialog setup function
  gboolean need_to_free = !ww->is_new;
  if ( ww->is_new && response != GTK_RESPONSE_ACCEPT ) {
    // Thus allowing to set some data for use by the dialog setup function
    ww->is_new = FALSE;
  }
  if ( need_to_free ) {
    trw_layer_wpwin_set ( ww->vtl, NULL, NULL );
    VikTrwLayerWpwin_free ( ww );
  }
}

void vik_trw_layer_wpwin_destroy ( VikTrwLayerWpwin *wpwin )
{
  if ( wpwin ) {
    gtk_widget_destroy ( wpwin->dialog );
    VikTrwLayerWpwin_free ( wpwin );
  }
}
