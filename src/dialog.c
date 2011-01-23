/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2010, Rob Norris <rw_norris@hotmail.com>
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
#include "thumbnails.h"
#include "garminsymbols.h"
#include "degrees_converters.h"
#include "authors.h"
#include "documenters.h"
#include "vikgoto.h"
#include "util.h"

#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void a_dialog_msg ( GtkWindow *parent, gint type, const gchar *info, const gchar *extra )
{
  GtkWidget *msgbox = gtk_message_dialog_new ( parent, GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_OK, info, extra );
  gtk_dialog_run ( GTK_DIALOG(msgbox) );
  gtk_widget_destroy ( msgbox );
}

gboolean a_dialog_goto_latlon ( GtkWindow *parent, struct LatLon *ll, const struct LatLon *old )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Go to Lat/Lon"),
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  GtkWidget *latlabel, *lonlabel;
  GtkWidget *lat, *lon;
  gchar *tmp_lat, *tmp_lon;

  latlabel = gtk_label_new (_("Latitude:"));
  lat = gtk_entry_new ();
  tmp_lat = g_strdup_printf ( "%f", old->lat );
  gtk_entry_set_text ( GTK_ENTRY(lat), tmp_lat );
  g_free ( tmp_lat );

  lonlabel = gtk_label_new (_("Longitude:"));
  lon = gtk_entry_new ();
  tmp_lon = g_strdup_printf ( "%f", old->lon );
  gtk_entry_set_text ( GTK_ENTRY(lon), tmp_lon );
  g_free ( tmp_lon );

  gtk_widget_show ( latlabel );
  gtk_widget_show ( lonlabel );
  gtk_widget_show ( lat );
  gtk_widget_show ( lon );

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), latlabel,  FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), lat, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), lonlabel,  FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), lon,  FALSE, FALSE, 0);

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    ll->lat = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(lat) ) );
    ll->lon = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(lon) ) );
    gtk_widget_destroy ( dialog );
    return TRUE;
  }

  gtk_widget_destroy ( dialog );
  return FALSE;
}

gboolean a_dialog_goto_utm ( GtkWindow *parent, struct UTM *utm, const struct UTM *old )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Go to UTM"),
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  GtkWidget *norlabel, *easlabel, *nor, *eas;
  GtkWidget *zonehbox, *zonespin, *letterentry;
  gchar *tmp_eas, *tmp_nor;
  gchar tmp_letter[2];

  norlabel = gtk_label_new (_("Northing:"));
  nor = gtk_entry_new ();
  tmp_nor = g_strdup_printf("%ld", (long) old->northing );
  gtk_entry_set_text ( GTK_ENTRY(nor), tmp_nor );
  g_free ( tmp_nor );

  easlabel = gtk_label_new (_("Easting:"));
  eas = gtk_entry_new ();
  tmp_eas = g_strdup_printf("%ld", (long) old->easting );
  gtk_entry_set_text ( GTK_ENTRY(eas), tmp_eas );
  g_free ( tmp_eas );

  zonehbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(zonehbox), gtk_label_new ( _("Zone:") ), FALSE, FALSE, 5 );
  zonespin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( old->zone, 1, 60, 1, 5, 0 ), 1, 0 );
  gtk_box_pack_start ( GTK_BOX(zonehbox), zonespin, TRUE, TRUE, 5 );
  gtk_box_pack_start ( GTK_BOX(zonehbox), gtk_label_new ( _("Letter:") ), FALSE, FALSE, 5 );
  letterentry = gtk_entry_new ();
  gtk_entry_set_max_length ( GTK_ENTRY(letterentry), 1 );
  gtk_entry_set_width_chars ( GTK_ENTRY(letterentry), 2 );
  tmp_letter[0] = old->letter;
  tmp_letter[1] = '\0';
  gtk_entry_set_text ( GTK_ENTRY(letterentry), tmp_letter );
  gtk_box_pack_start ( GTK_BOX(zonehbox), letterentry, FALSE, FALSE, 5 );

  gtk_widget_show ( norlabel );
  gtk_widget_show ( easlabel );
  gtk_widget_show ( nor );
  gtk_widget_show ( eas );

  gtk_widget_show_all ( zonehbox );

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), norlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), nor, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), easlabel,  FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), eas,  FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), zonehbox,  FALSE, FALSE, 0);

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    const gchar *letter;
    utm->northing = atof ( gtk_entry_get_text ( GTK_ENTRY(nor) ) );
    utm->easting = atof ( gtk_entry_get_text ( GTK_ENTRY(eas) ) );
    utm->zone = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(zonespin) );
    letter = gtk_entry_get_text ( GTK_ENTRY(letterentry) );
    if (*letter)
       utm->letter = toupper(*letter);
    gtk_widget_destroy ( dialog );
    return TRUE;
  }

  gtk_widget_destroy ( dialog );
  return FALSE;
}

void a_dialog_response_accept ( GtkDialog *dialog )
{
  gtk_dialog_response ( dialog, GTK_RESPONSE_ACCEPT );
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

/* todo: less on this side, like add track */
gboolean a_dialog_new_waypoint ( GtkWindow *parent, gchar **dest, VikWaypoint *wp, GHashTable *waypoints, VikCoordMode coord_mode )
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
  GtkWidget *latlabel, *lonlabel, *namelabel, *latentry, *lonentry, *altentry, *altlabel, *nameentry=NULL, *commentlabel, 
    *commententry, *imagelabel, *imageentry, *symbollabel, *symbolentry;
  GtkListStore *store;




  gchar *lat, *lon, *alt;

  vik_coord_to_latlon ( &(wp->coord), &ll );

  lat = g_strdup_printf ( "%f", ll.lat );
  lon = g_strdup_printf ( "%f", ll.lon );
  vik_units_height_t height_units = a_vik_get_units_height ();
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

  if ( dest != NULL )
  {
    namelabel = gtk_label_new (_("Name:"));
    nameentry = gtk_entry_new ();
    if ( *dest ) {
      gtk_entry_set_text( GTK_ENTRY(nameentry), *dest );
      g_free ( *dest );
      *dest = NULL;
    }
    g_signal_connect_swapped ( nameentry, "activate", G_CALLBACK(a_dialog_response_accept), GTK_DIALOG(dialog) );
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), namelabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), nameentry, FALSE, FALSE, 0);
  }

  latlabel = gtk_label_new (_("Latitude:"));
  latentry = gtk_entry_new ();
  gtk_entry_set_text ( GTK_ENTRY(latentry), lat );
  g_free ( lat );

  lonlabel = gtk_label_new (_("Longitude:"));
  lonentry = gtk_entry_new ();
  gtk_entry_set_text ( GTK_ENTRY(lonentry), lon );
  g_free ( lon );

  altlabel = gtk_label_new (_("Altitude:"));
  altentry = gtk_entry_new ();
  gtk_entry_set_text ( GTK_ENTRY(altentry), alt );
  g_free ( alt );

  commentlabel = gtk_label_new (_("Comment:"));
  commententry = gtk_entry_new ();
  gchar *cmt =  NULL;
  cmt = a_vik_goto_get_search_string_for_this_place(VIK_WINDOW(parent));
  if (cmt)
    gtk_entry_set_text(GTK_ENTRY(commententry), cmt);

  imagelabel = gtk_label_new (_("Image:"));
  imageentry = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN);

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

    if ( dest == NULL && wp->symbol ) {
      gboolean ok;
      gchar *sym;
      for (ok = gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(store), &iter ); ok; ok = gtk_tree_model_iter_next ( GTK_TREE_MODEL(store), &iter)) {
	gtk_tree_model_get ( GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1 );
	if (sym && !strcmp(sym, wp->symbol)) {
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

  if ( dest == NULL && wp->comment )
    gtk_entry_set_text ( GTK_ENTRY(commententry), wp->comment );

  if ( dest == NULL && wp->image )
    vik_file_entry_set_filename ( VIK_FILE_ENTRY(imageentry), wp->image );


  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), latlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), latentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), lonlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), lonentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), altlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), altentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), commentlabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), commententry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), imagelabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), imageentry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), symbollabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(symbolentry), FALSE, FALSE, 0);

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  gtk_widget_show_all ( GTK_DIALOG(dialog)->vbox );

  while ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    if ( dest )
    {
      const gchar *constname = gtk_entry_get_text ( GTK_ENTRY(nameentry) );
      if ( strlen(constname) == 0 ) /* TODO: other checks (isalpha or whatever ) */
        a_dialog_info_msg ( parent, _("Please enter a name for the waypoint.") );
      else {
        gchar *name = g_strdup ( constname );

        if ( g_hash_table_lookup ( waypoints, name ) && !a_dialog_yes_or_no ( parent, _("The waypoint \"%s\" exists, do you want to overwrite it?"), name ) )
          g_free ( name );
        else
        {
          /* Do It */
          *dest = name;
          ll.lat = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(latentry) ) );
          ll.lon = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(lonentry) ) );
          vik_coord_load_from_latlon ( &(wp->coord), coord_mode, &ll );
	  // Always store in metres
	  switch (height_units) {
	  case VIK_UNITS_HEIGHT_METRES:
	    wp->altitude = atof ( gtk_entry_get_text ( GTK_ENTRY(altentry) ) );
	    break;
	  case VIK_UNITS_HEIGHT_FEET:
	    wp->altitude = VIK_FEET_TO_METERS(atof ( gtk_entry_get_text ( GTK_ENTRY(altentry) ) ));
	    break;
	  default:
	    wp->altitude = atof ( gtk_entry_get_text ( GTK_ENTRY(altentry) ) );
	    g_critical("Houston, we've had a problem. height=%d", height_units);
	  }
          vik_waypoint_set_comment ( wp, gtk_entry_get_text ( GTK_ENTRY(commententry) ) );
          vik_waypoint_set_image ( wp, vik_file_entry_get_filename ( VIK_FILE_ENTRY(imageentry) ) );
          if ( wp->image && *(wp->image) && (!a_thumbnails_exists(wp->image)) )
            a_thumbnails_create ( wp->image );

	  {
	    GtkTreeIter iter, first;
	    gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(store), &first );
	    if ( !gtk_combo_box_get_active_iter ( GTK_COMBO_BOX(symbolentry), &iter ) || !memcmp(&iter, &first, sizeof(GtkTreeIter)) ) {
	      vik_waypoint_set_symbol ( wp, NULL );
	    } else {
	      gchar *sym;
	      gtk_tree_model_get ( GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1 );
	      vik_waypoint_set_symbol ( wp, sym );
	      g_free(sym);
	    }
	  }		

          gtk_widget_destroy ( dialog );
          return TRUE;
        }
      } /* else (valid name) */
    }
    else
    {
      ll.lat = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(latentry) ) );
      ll.lon = convert_dms_to_dec ( gtk_entry_get_text ( GTK_ENTRY(lonentry) ) );
      vik_coord_load_from_latlon ( &(wp->coord), coord_mode, &ll );
      switch (height_units) {
      case VIK_UNITS_HEIGHT_METRES:
	wp->altitude = atof ( gtk_entry_get_text ( GTK_ENTRY(altentry) ) );
	break;
      case VIK_UNITS_HEIGHT_FEET:
	wp->altitude = VIK_FEET_TO_METERS(atof ( gtk_entry_get_text ( GTK_ENTRY(altentry) ) ));
	break;
      default:
	wp->altitude = atof ( gtk_entry_get_text ( GTK_ENTRY(altentry) ) );
	g_critical("Houston, we've had a problem. height=%d", height_units);
      }
      if ( (! wp->comment) || strcmp ( wp->comment, gtk_entry_get_text ( GTK_ENTRY(commententry) ) ) != 0 )
        vik_waypoint_set_comment ( wp, gtk_entry_get_text ( GTK_ENTRY(commententry) ) );
      if ( (! wp->image) || strcmp ( wp->image, vik_file_entry_get_filename ( VIK_FILE_ENTRY ( imageentry ) ) ) != 0 )
      {
        vik_waypoint_set_image ( wp, vik_file_entry_get_filename ( VIK_FILE_ENTRY(imageentry) ) );
        if ( wp->image && *(wp->image) && (!a_thumbnails_exists(wp->image)) )
          a_thumbnails_create ( wp->image );
      }

      {
	GtkTreeIter iter, first;
	gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(store), &first );
	if ( !gtk_combo_box_get_active_iter ( GTK_COMBO_BOX(symbolentry), &iter ) || !memcmp(&iter, &first, sizeof(GtkTreeIter)) ) {
	  vik_waypoint_set_symbol ( wp, NULL );
	} else {
	  gchar *sym;
	  gtk_tree_model_get ( GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1 );
	  vik_waypoint_set_symbol ( wp, sym );
	  g_free(sym);
	}
      }		

      gtk_widget_destroy ( dialog );

      return TRUE;
    }
  }
  gtk_widget_destroy ( dialog );
  return FALSE;
}

static void get_selected_foreach_func(GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
  GList **list = data;
  gchar *name;
  gtk_tree_model_get (model, iter, 0, &name, -1);
  *list = g_list_prepend(*list, name);
}

GList *a_dialog_select_from_list ( GtkWindow *parent, GList *names, gboolean multiple_selection_allowed, const gchar *title, const gchar *msg )
{
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkWidget *view;

  GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  /* When something is selected then OK */
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  /* Default to not apply - as initially nothing is selected! */
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_REJECT );
#endif
  GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);

  GtkWidget *scrolledwindow;

  GList *runner = names;
  while (runner)
  {
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, runner->data, -1);
    runner = g_list_next(runner);
  }

  view = gtk_tree_view_new();
  renderer = gtk_cell_renderer_text_new();
  // Use the column header to display the message text,
  // this makes the overall widget allocation simple as treeview takes up all the space
  gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(view), -1, msg, renderer, "text", 0, NULL);
  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
  gtk_tree_view_set_headers_visible( GTK_TREE_VIEW(view), TRUE);
  gtk_tree_selection_set_mode( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
      multiple_selection_allowed ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_BROWSE );
  g_object_unref(store);

  scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
  gtk_container_add ( GTK_CONTAINER(scrolledwindow), view );

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, TRUE, TRUE, 0 );
  // Ensure a reasonable number of items are shown, but let the width be automatically sized
  gtk_widget_set_size_request ( dialog, -1, 400) ;

  gtk_widget_show_all ( dialog );

  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  while ( gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT )
  {
    GList *names = NULL;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_selected_foreach(selection, get_selected_foreach_func, &names);
    if (names)
    {
      gtk_widget_destroy ( dialog );
      return (names);
    }
    a_dialog_error_msg(parent, _("Nothing was selected"));
  }
  gtk_widget_destroy ( dialog );
  return NULL;
}

gchar *a_dialog_new_track ( GtkWindow *parent, GHashTable *tracks, gchar *default_name )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Add Track"),
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  GtkWidget *label = gtk_label_new ( _("Track Name:") );
  GtkWidget *entry = gtk_entry_new ();

  if (default_name)
    gtk_entry_set_text ( GTK_ENTRY(entry), default_name );

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), entry, FALSE, FALSE, 0);

  g_signal_connect_swapped ( entry, "activate", G_CALLBACK(a_dialog_response_accept), GTK_DIALOG(dialog) );

  gtk_widget_show ( label );
  gtk_widget_show ( entry );

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  while ( gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT )
  {
    const gchar *constname = gtk_entry_get_text ( GTK_ENTRY(entry) );
    if ( *constname == '\0' )
      a_dialog_info_msg ( parent, _("Please enter a name for the track.") );
    else {
      gchar *name = g_strdup ( constname );

      if ( g_hash_table_lookup( tracks, name ) && !a_dialog_yes_or_no ( parent, _("The track \"%s\" exists, do you want to overwrite it?"), gtk_entry_get_text ( GTK_ENTRY(entry) ) ) )
      {
        g_free ( name );
      }
      else
      {
        gtk_widget_destroy ( dialog );
        return name;
      }
    }
  }
  gtk_widget_destroy ( dialog );
  return NULL;
}

/* creates a vbox full of labels */
GtkWidget *a_dialog_create_label_vbox ( gchar **texts, int label_count )
{
  GtkWidget *vbox, *label;
  int i;
  vbox = gtk_vbox_new( TRUE, 3 );

  for ( i = 0; i < label_count; i++ )
  {
    label = gtk_label_new(NULL);
    gtk_label_set_markup ( GTK_LABEL(label), _(texts[i]) );
    gtk_box_pack_start ( GTK_BOX(vbox), label, FALSE, TRUE, 5 );
  }
  return vbox;
}

gboolean a_dialog_yes_or_no ( GtkWindow *parent, const gchar *message, const gchar *extra )
{
  GtkWidget *dia;
  dia = gtk_message_dialog_new ( parent,
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_QUESTION,
                                 GTK_BUTTONS_YES_NO,
                                 message, extra );

  if ( gtk_dialog_run ( GTK_DIALOG(dia) ) == GTK_RESPONSE_YES )
  {
    gtk_widget_destroy ( dia );
    return TRUE;
  }
  else
  {
    gtk_widget_destroy ( dia );
    return FALSE;
  }
}

static void zoom_spin_changed ( GtkSpinButton *spin, GtkWidget *pass_along[3] )
{
  if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(pass_along[2]) ) )
    gtk_spin_button_set_value ( 
        GTK_SPIN_BUTTON(pass_along[GTK_WIDGET(spin) == pass_along[0] ? 1 : 0]),
        gtk_spin_button_get_value ( spin ) );
}

gboolean a_dialog_custom_zoom ( GtkWindow *parent, gdouble *xmpp, gdouble *ympp )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Zoom Factors..."),
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  GtkWidget *table, *label, *xlabel, *xspin, *ylabel, *yspin, *samecheck;
  GtkWidget *pass_along[3];

  table = gtk_table_new ( 4, 2, FALSE );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0 );

  label = gtk_label_new ( _("Zoom factor (in meters per pixel):") );
  xlabel = gtk_label_new ( _("X (easting): "));
  ylabel = gtk_label_new ( _("Y (northing): "));

  pass_along[0] = xspin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( *xmpp, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0 ), 1, 8 );
  pass_along[1] = yspin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( *ympp, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0 ), 1, 8 );

  pass_along[2] = samecheck = gtk_check_button_new_with_label ( _("X and Y zoom factors must be equal") );
  /* TODO -- same factor */
  /*  samecheck = gtk_check_button_new_with_label ( "Same x/y zoom factor" ); */

  if ( *xmpp == *ympp )
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(samecheck), TRUE );

  gtk_table_attach_defaults ( GTK_TABLE(table), label, 0, 2, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table), xlabel, 0, 1, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table), xspin, 1, 2, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table), ylabel, 0, 1, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table), yspin, 1, 2, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table), samecheck, 0, 2, 3, 4 );

  gtk_widget_show_all ( table );

  g_signal_connect ( G_OBJECT(xspin), "value-changed", G_CALLBACK(zoom_spin_changed), pass_along );
  g_signal_connect ( G_OBJECT(yspin), "value-changed", G_CALLBACK(zoom_spin_changed), pass_along );

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    *xmpp = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(xspin) );
    *ympp = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(yspin) );
    gtk_widget_destroy ( dialog );
    return TRUE;
  }
  gtk_widget_destroy ( dialog );
  return FALSE;
}

static void split_spin_focused ( GtkSpinButton *spin, GtkWidget *pass_along[1] )
{
  gtk_toggle_button_set_active    (GTK_TOGGLE_BUTTON(pass_along[0]), 1);
}

gboolean a_dialog_time_threshold ( GtkWindow *parent, gchar *title_text, gchar *label_text, guint *thr )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons (title_text, 
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  GtkWidget *table, *t1, *t2, *t3, *t4, *spin, *label;
  GtkWidget *pass_along[1];

  table = gtk_table_new ( 4, 2, FALSE );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0 );

  label = gtk_label_new (label_text);

  t1 = gtk_radio_button_new_with_label ( NULL, _("1 min") );
  t2 = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(t1), _("1 hour") );
  t3 = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(t2), _("1 day") );
  t4 = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(t3), _("Custom (in minutes):") );

  pass_along[0] = t4;

  spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( *thr, 0, 65536, 1, 5, 0 ), 1, 0 );

  gtk_table_attach_defaults ( GTK_TABLE(table), label, 0, 2, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table), t1, 0, 1, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table), t2, 0, 1, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table), t3, 0, 1, 3, 4 );
  gtk_table_attach_defaults ( GTK_TABLE(table), t4, 0, 1, 4, 5 );
  gtk_table_attach_defaults ( GTK_TABLE(table), spin, 1, 2, 4, 5 );

  gtk_widget_show_all ( table );

  g_signal_connect ( G_OBJECT(spin), "grab-focus", G_CALLBACK(split_spin_focused), pass_along );

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t1))) {
      *thr = 1;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t2))) {
      *thr = 60;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t3))) {
      *thr = 60 * 24;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t4))) {
      *thr = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(spin) );
    }
    gtk_widget_destroy ( dialog );
    return TRUE;
  }
  gtk_widget_destroy ( dialog );
  return FALSE;
}

static void about_url_hook (GtkAboutDialog *about,
                            const gchar    *link,
                            gpointer        data)
{
  open_url (GTK_WINDOW(about), link);
}

static void about_email_hook (GtkAboutDialog *about,
                              const gchar    *email,
                              gpointer        data)
{
  new_email (GTK_WINDOW(about), email);
}

void a_dialog_about ( GtkWindow *parent )
{
  const gchar *program_name = PACKAGE_NAME;
  const gchar *version = VIKING_VERSION;
  const gchar *website = VIKING_URL;
  const gchar *copyright = "2003-2008, Evan Battaglia\n2008-2010, Viking's contributors";
  const gchar *comments = _("GPS Data and Topo Analyzer, Explorer, and Manager.");
  const gchar *license = _("This program is free software; you can redistribute it and/or modify "
			"it under the terms of the GNU General Public License as published by "
			"the Free Software Foundation; either version 2 of the License, or "
			"(at your option) any later version."
			"\n\n"
			"This program is distributed in the hope that it will be useful, "
			"but WITHOUT ANY WARRANTY; without even the implied warranty of "
			"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
			"GNU General Public License for more details."
			"\n\n"
			"You should have received a copy of the GNU General Public License "
			"along with this program; if not, write to the Free Software "
			"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA");

  gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);
  gtk_about_dialog_set_email_hook (about_email_hook, NULL, NULL);
  gtk_show_about_dialog (parent,
	/* TODO do not set program-name and correctly set info for g_get_application_name */
  	"program-name", program_name,
	"version", version,
	"website", website,
	"comments", comments,
	"copyright", copyright,
	"license", license,
	"wrap-license", TRUE,
	/* logo automatically retrieved via gtk_window_get_default_icon_list */
	"authors", AUTHORS,
	"documenters", DOCUMENTERS,
	"translator-credits", _("Translation is coordinated on http://launchpad.net/viking"),
	NULL);
}

gboolean a_dialog_map_n_zoom(GtkWindow *parent, gchar *mapnames[], gint default_map, gchar *zoom_list[], gint default_zoom, gint *selected_map, gint *selected_zoom)
{
  gchar **s;

  GtkWidget *dialog = gtk_dialog_new_with_buttons ( _("Download along track"), parent, 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
#endif
  GtkWidget *map_label = gtk_label_new(_("Map type:"));
  GtkComboBox *map_combo = GTK_COMBO_BOX(gtk_combo_box_new_text());
  for (s = mapnames; *s; s++)
    gtk_combo_box_append_text(map_combo, *s);
  gtk_combo_box_set_active (map_combo, default_map);
  GtkWidget *zoom_label = gtk_label_new(_("Zoom level:"));
  GtkComboBox *zoom_combo = GTK_COMBO_BOX(gtk_combo_box_new_text());
  for (s = zoom_list; *s; s++)
    gtk_combo_box_append_text(zoom_combo, *s);
  gtk_combo_box_set_active (zoom_combo, default_zoom);

  GtkTable *box = GTK_TABLE(gtk_table_new(2, 2, FALSE));
  gtk_table_attach_defaults(box, GTK_WIDGET(map_label), 0, 1, 0, 1);
  gtk_table_attach_defaults(box, GTK_WIDGET(map_combo), 1, 2, 0, 1);
  gtk_table_attach_defaults(box, GTK_WIDGET(zoom_label), 0, 1, 1, 2);
  gtk_table_attach_defaults(box, GTK_WIDGET(zoom_combo), 1, 2, 1, 2);

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(box), FALSE, FALSE, 5 );

  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  gtk_widget_show_all ( dialog );
  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
    gtk_widget_destroy(dialog);
    return FALSE;
  }

  *selected_map = gtk_combo_box_get_active(map_combo);
  *selected_zoom = gtk_combo_box_get_active(zoom_combo);

  gtk_widget_destroy(dialog);
  return TRUE;
}

/**
 * Display a dialog presenting the license of a map.
 * Allow to read the license by launching a web browser.
 */
void a_dialog_license ( GtkWindow *parent, const gchar *map, const gchar *license, const gchar *url)
{
  GtkWidget *dialog = gtk_message_dialog_new (parent,
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_INFO,
                                 GTK_BUTTONS_OK,
                                 _("The map data is licensed: %s."),
                                 license);
  gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
    _("The data provided by '<b>%s</b>' are licensed under the following license: <b>%s</b>.\n"
    "Please, read the license before continuing."),
    map, license);
#define RESPONSE_OPEN_LICENSE 600
  if (url != NULL) {
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("Open license"), RESPONSE_OPEN_LICENSE);
  }
  gint response;
  do {
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (response == RESPONSE_OPEN_LICENSE) {
      open_url (parent, url);
    }
  } while (response != GTK_RESPONSE_DELETE_EVENT && response != GTK_RESPONSE_OK);
  gtk_widget_destroy (dialog);
}
