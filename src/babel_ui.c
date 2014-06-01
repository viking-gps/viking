
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
#include <glib/gi18n.h>

#include "babel.h"
#include "babel_ui.h"


static void babel_ui_selector_add_entry_cb ( gpointer data, gpointer user_data )
{
  BabelFile *file = (BabelFile*)data;
  GtkWidget *combo = GTK_WIDGET(user_data);

  GList *formats = g_object_get_data ( G_OBJECT(combo), "formats" );
  formats = g_list_append ( formats, file );
  g_object_set_data ( G_OBJECT(combo), "formats", formats );

  const gchar *label = file->label;
  vik_combo_box_text_append ( combo, label );
}

void a_babel_ui_type_selector_dialog_sensitivity_cb ( GtkComboBox *widget, gpointer user_data )
{
  /* user_data is the GtkDialog */
  GtkDialog *dialog = GTK_DIALOG(user_data);

  /* Retrieve the associated file format descriptor */
  BabelFile *file = a_babel_ui_file_type_selector_get ( GTK_WIDGET(widget) );

  if ( file )
    /* Not NULL => valid selection */
    gtk_dialog_set_response_sensitive ( dialog, GTK_RESPONSE_ACCEPT, TRUE );
  else
    /* NULL => invalid selection */
    gtk_dialog_set_response_sensitive ( dialog, GTK_RESPONSE_ACCEPT, FALSE );
}

/**
 * a_babel_ui_file_type_selector_new:
 * @mode: the mode to filter the file types
 *
 * Create a file type selector.
 *
 * This widget relies on a combo box listing labels of file formats.
 * We store in the "data" of the GtkWidget a list with the BabelFile
 * entries, in order to retrieve the selected file format.
 *
 * Returns: a GtkWidget
 */
GtkWidget *a_babel_ui_file_type_selector_new ( BabelMode mode )
{
  GList *formats = NULL;
  /* Create the combo */
  GtkWidget * combo = vik_combo_box_text_new ();

  /* Add a first label to invite user to select a file format */
  /* We store a NULL pointer to distinguish this entry */
  formats = g_list_append ( formats, NULL );
  vik_combo_box_text_append ( combo, _("Select a file format") );

  /* Prepare space for file format list */
  g_object_set_data ( G_OBJECT(combo), "formats", formats );

  /* Add all known and compatible file formats */
  if ( mode.tracksRead && mode.routesRead && mode.waypointsRead &&
       !mode.tracksWrite && !mode.routesWrite && !mode.waypointsWrite )
    a_babel_foreach_file_read_any ( babel_ui_selector_add_entry_cb, combo );
  else
    a_babel_foreach_file_with_mode ( mode, babel_ui_selector_add_entry_cb, combo );

  /* Initialize the selection with the really first entry */
  gtk_combo_box_set_active ( GTK_COMBO_BOX(combo), 0 );

  return combo;
}

/**
 * a_babel_ui_file_type_selector_destroy:
 * @selector: the selector to destroy
 *
 * Destroy the selector and any related data.
 */
void a_babel_ui_file_type_selector_destroy ( GtkWidget *selector )
{
  GList *formats = g_object_get_data ( G_OBJECT(selector), "formats" );
  g_free ( formats );
}

/**
 * a_babel_ui_file_type_selector_get:
 * @selector: the selector
 *
 * Retrieve the selected file type.
 *
 * Returns: the selected BabelFile or NULL
 */
BabelFile *a_babel_ui_file_type_selector_get ( GtkWidget *selector )
{
  gint active = gtk_combo_box_get_active ( GTK_COMBO_BOX(selector) );
  if (active >= 0) {
    GList *formats = g_object_get_data ( G_OBJECT(selector), "formats" );
    return (BabelFile*)g_list_nth_data ( formats, active );
  } else {
    return NULL;
  }
}

/**
 * a_babel_ui_modes_new:
 * @tracks:
 * @routes:
 * @waypoints:
 *
 * Creates a selector for babel modes.
 * This selector is based on 3 checkboxes.
 *
 * Returns: a GtkWidget packing all checkboxes.
 */
GtkWidget *a_babel_ui_modes_new ( gboolean tracks, gboolean routes, gboolean waypoints )
{
  GtkWidget *hbox = gtk_hbox_new( FALSE, 0 );
  GtkWidget *button = NULL;

  button = gtk_check_button_new_with_label ( _("Tracks") );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(button), tracks );
  gtk_box_pack_start ( GTK_BOX(hbox), button, TRUE, TRUE, 0 );
  gtk_widget_show ( button );

  button = gtk_check_button_new_with_label ( _("Routes") );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(button), routes );
  gtk_box_pack_start ( GTK_BOX(hbox), button, TRUE, TRUE, 0 );
  gtk_widget_show ( button );

  button = gtk_check_button_new_with_label ( _("Waypoints") );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(button), waypoints );
  gtk_box_pack_start ( GTK_BOX(hbox), button, TRUE, TRUE, 0 );
  gtk_widget_show ( button );

  return hbox;
}

/**
 * a_babel_ui_modes_get:
 * @container:
 * @tracks: return value
 * @routes: return value
 * @waypoints: return value
 *
 * Retrieve state of checkboxes.
 */
void a_babel_ui_modes_get ( GtkWidget *container, gboolean *tracks, gboolean *routes, gboolean *waypoints )
{
  GList* children = gtk_container_get_children ( GTK_CONTAINER(container) );
  GtkWidget *child = NULL;

  child = g_list_nth_data ( children, 0 );
  *tracks = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(child) );

  child = g_list_nth_data ( children, 1 );
  *routes = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(child) );

  child = g_list_nth_data ( children, 2 );
  *waypoints = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(child) );
}

