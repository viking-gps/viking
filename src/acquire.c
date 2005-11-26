/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
#include <string.h>
#include <glib/gprintf.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"

/* passed along to worker thread */
typedef struct {
  acq_dialog_widgets_t *w;
  VikDataSourceInterface *interface;
  gchar *cmd;
  gchar *extra;
} w_and_interface_t;

extern VikDataSourceInterface vik_datasource_gps_interface;
extern VikDataSourceInterface vik_datasource_google_interface;

/*********************************************************
 * Definitions and routines for acquiring data from GPS
 *********************************************************/

/* this routine is the worker thread.  there is only one simultaneous download allowed */
static void get_from_anything ( w_and_interface_t *wi )
{
  gchar *cmd = wi->cmd;
  gchar *extra = wi->extra;
  gboolean result;
  VikTrwLayer *vtl;

  gboolean creating_new_layer = TRUE;

  acq_dialog_widgets_t *w = wi->w;
  VikDataSourceInterface *interface = wi->interface;
  g_free ( wi );

  gdk_threads_enter();
  if (interface->mode == VIK_DATASOURCE_ADDTOLAYER) {
    VikLayer *current_selected = vik_layers_panel_get_selected ( w->vlp );
    if ( IS_VIK_TRW_LAYER(current_selected) ) {
      vtl = VIK_TRW_LAYER(current_selected);
      creating_new_layer = FALSE;
    }
  }
  if ( creating_new_layer ) {
    vtl = VIK_TRW_LAYER ( vik_layer_create ( VIK_LAYER_TRW, w->vvp, NULL, FALSE ) );
    vik_layer_rename ( VIK_LAYER ( vtl ), interface->layer_title );
    gtk_label_set_text ( GTK_LABEL(w->status), "Working..." );
  }
  gdk_threads_leave();

  if ( interface->type == VIK_DATASOURCE_GPSBABEL_DIRECT )
    result = a_babel_convert_from (vtl, cmd, (BabelStatusFunc) interface->progress_func, extra, w);
  else
    result = a_babel_convert_from_shellcommand ( vtl, cmd, extra, (BabelStatusFunc) interface->progress_func, w);

  g_free ( cmd );
  g_free ( extra );

  if (!result) {
    gdk_threads_enter();
    gtk_label_set_text ( GTK_LABEL(w->status), "Error: couldn't find gpsbabel." );
    if ( creating_new_layer )
      g_object_unref ( G_OBJECT ( vtl ) );
    gdk_threads_leave();
  } 

  gdk_threads_enter();
  if (w->ok) {
    gtk_label_set_text ( GTK_LABEL(w->status), "Done." );
    if ( creating_new_layer )
    vik_aggregate_layer_add_layer( vik_layers_panel_get_top_layer(w->vlp), VIK_LAYER(vtl));
    gtk_dialog_set_response_sensitive ( GTK_DIALOG(w->dialog), GTK_RESPONSE_ACCEPT, TRUE );
    gtk_dialog_set_response_sensitive ( GTK_DIALOG(w->dialog), GTK_RESPONSE_REJECT, FALSE );
  } else {
    /* canceled */
    if ( creating_new_layer )
      g_object_unref(vtl);
  }

  if ( interface->cleanup_func )
    interface->cleanup_func ( w->specific_data );

  if ( w->ok ) {
    w->ok = FALSE;
  } else {
    g_free ( w );
  }

  gdk_threads_leave();
  g_thread_exit ( NULL );
}


void a_acquire ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikDataSourceInterface *interface )
{
  GtkWidget *dialog = NULL;
  GtkWidget *status;
  gchar *cmd, *extra;
  acq_dialog_widgets_t *w;

  w_and_interface_t *wi;

  if ( interface->add_widgets_func ) {
    gpointer first_dialog_data;
    dialog = gtk_dialog_new_with_buttons ( "", NULL, 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
    first_dialog_data = interface->add_widgets_func(dialog);
    if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
      interface->first_cleanup_func(first_dialog_data);
      gtk_widget_destroy(dialog);
      return;
    }
    interface->get_cmd_string_func ( first_dialog_data, &cmd, &extra );
    interface->first_cleanup_func(first_dialog_data);
    gtk_widget_destroy(dialog);
    dialog = NULL;
  } else
    interface->get_cmd_string_func ( NULL, &cmd, &extra );

  if ( ! cmd )
    return;

  w = g_malloc(sizeof(*w));
  wi = g_malloc(sizeof(*wi));
  wi->w = w;
  wi->interface = interface;
  wi->cmd = cmd;
  wi->extra = extra;

  dialog = gtk_dialog_new_with_buttons ( "", NULL, 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_dialog_set_response_sensitive ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, FALSE );


  w->dialog = dialog;
  w->ok = TRUE;

  status = gtk_label_new ("Status: detecting gpsbabel");
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), status, FALSE, FALSE, 5 );
  gtk_widget_show_all(status);
  w->status = status;

  w->vw = vw;
  w->vlp = vlp;
  w->vvp = vvp;
  if ( interface->add_progress_widgets_func )
    w->specific_data = interface->add_progress_widgets_func ( dialog );
  else
    w->specific_data = NULL;

  g_thread_create((GThreadFunc)get_from_anything, wi, FALSE, NULL );

  gtk_dialog_run ( GTK_DIALOG(dialog) );
  if ( w->ok )
    w->ok = FALSE; /* tell thread to stop. TODO: add mutex */
  else {
    g_free ( w ); /* thread has finished; free w */
  }
  gtk_widget_destroy ( dialog );
}

