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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"

/************************ FILTER LIST *******************/
// extern VikDataSourceInterface vik_datasource_gps_interface;
// extern VikDataSourceInterface vik_datasource_google_interface;

/*** Input is TRWLayer ***/
extern VikDataSourceInterface vik_datasource_bfilter_simplify_interface;
extern VikDataSourceInterface vik_datasource_bfilter_dup_interface;

/*** Input is a track and a TRWLayer ***/
extern VikDataSourceInterface vik_datasource_bfilter_polygon_interface;
extern VikDataSourceInterface vik_datasource_bfilter_exclude_polygon_interface;

/*** Input is a track ***/

const VikDataSourceInterface *filters[] = {
  &vik_datasource_bfilter_simplify_interface,
  &vik_datasource_bfilter_dup_interface,
  &vik_datasource_bfilter_polygon_interface,
  &vik_datasource_bfilter_exclude_polygon_interface,
};

const guint N_FILTERS = sizeof(filters) / sizeof(filters[0]);

VikTrack *filter_track = NULL;
gchar *filter_track_name = NULL;

/********************************************************/

/* passed along to worker thread */
typedef struct {
  acq_dialog_widgets_t *w;
  gchar *cmd;
  gchar *extra;
} w_and_interface_t;


/*********************************************************
 * Definitions and routines for acquiring data from Data Sources in general
 *********************************************************/

static void progress_func ( BabelProgressCode c, gpointer data, acq_dialog_widgets_t *w )
{
  gdk_threads_enter ();
  if (!w->ok) {
    if ( w->source_interface->cleanup_func )
      w->source_interface->cleanup_func( w->user_data );
    g_free ( w );
    gdk_threads_leave();
    g_thread_exit ( NULL );
  }
  gdk_threads_leave ();

  if ( w->source_interface->progress_func )
    w->source_interface->progress_func ( (gpointer) c, data, w );
}


/* this routine is the worker thread.  there is only one simultaneous download allowed */
static void get_from_anything ( w_and_interface_t *wi )
{
  gchar *cmd = wi->cmd;
  gchar *extra = wi->extra;
  gboolean result = TRUE;
  VikTrwLayer *vtl;

  gboolean creating_new_layer = TRUE;

  acq_dialog_widgets_t *w = wi->w;
  VikDataSourceInterface *source_interface = wi->w->source_interface;
  g_free ( wi );
  wi = NULL;

  gdk_threads_enter();
  if (source_interface->mode == VIK_DATASOURCE_ADDTOLAYER) {
    VikLayer *current_selected = vik_layers_panel_get_selected ( w->vlp );
    if ( IS_VIK_TRW_LAYER(current_selected) ) {
      vtl = VIK_TRW_LAYER(current_selected);
      creating_new_layer = FALSE;
    }
  }
  if ( creating_new_layer ) {
    vtl = VIK_TRW_LAYER ( vik_layer_create ( VIK_LAYER_TRW, w->vvp, NULL, FALSE ) );
    vik_layer_rename ( VIK_LAYER ( vtl ), _(source_interface->layer_title) );
    gtk_label_set_text ( GTK_LABEL(w->status), _("Working...") );
  }
  gdk_threads_leave();

  switch ( source_interface->type ) {
  case VIK_DATASOURCE_GPSBABEL_DIRECT:
    result = a_babel_convert_from (vtl, cmd, (BabelStatusFunc) progress_func, extra, w);
    break;
  case VIK_DATASOURCE_URL:
    result = a_babel_convert_from_url (vtl, cmd, extra, (BabelStatusFunc) progress_func, w);
    break;
  case VIK_DATASOURCE_SHELL_CMD:
    result = a_babel_convert_from_shellcommand ( vtl, cmd, extra, (BabelStatusFunc) progress_func, w);
    break;
  default:
    g_critical("Houston, we've had a problem.");
  }

  g_free ( cmd );
  g_free ( extra );

  if (!result) {
    gdk_threads_enter();
    gtk_label_set_text ( GTK_LABEL(w->status), _("Error: acquisition failed.") );
    if ( creating_new_layer )
      g_object_unref ( G_OBJECT ( vtl ) );
    gdk_threads_leave();
  } 
  else {
    gdk_threads_enter();
    if (w->ok) {
      gtk_label_set_text ( GTK_LABEL(w->status), _("Done.") );
      if ( creating_new_layer ) {
	/* Only create the layer if it actually contains anything useful */
	if ( g_hash_table_size (vik_trw_layer_get_tracks(vtl)) ||
	     g_hash_table_size (vik_trw_layer_get_waypoints(vtl)) )
	  vik_aggregate_layer_add_layer( vik_layers_panel_get_top_layer(w->vlp), VIK_LAYER(vtl));
	else
	  gtk_label_set_text ( GTK_LABEL(w->status), _("No data.") );
      }
      if ( source_interface->keep_dialog_open ) {
        gtk_dialog_set_response_sensitive ( GTK_DIALOG(w->dialog), GTK_RESPONSE_ACCEPT, TRUE );
        gtk_dialog_set_response_sensitive ( GTK_DIALOG(w->dialog), GTK_RESPONSE_REJECT, FALSE );
      } else {
        gtk_dialog_response ( GTK_DIALOG(w->dialog), GTK_RESPONSE_ACCEPT );     
      }
    } else {
      /* canceled */
      if ( creating_new_layer )
	g_object_unref(vtl);
    }
  }
  if ( source_interface->cleanup_func )
    source_interface->cleanup_func ( w->user_data );

  if ( w->ok ) {
    w->ok = FALSE;
  } else {
    g_free ( w );
  }

  gdk_threads_leave();
  g_thread_exit ( NULL );
}


static gchar *write_tmp_trwlayer ( VikTrwLayer *vtl )
{
  int fd_src;
  gchar *name_src;
  FILE *f;
  g_assert ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) >= 0);
  f = fdopen(fd_src, "w");
  a_gpx_write_file(vtl, f);
  fclose(f);
  f = NULL;
  return name_src;
}

/* TODO: write with name of old track */
static gchar *write_tmp_track ( VikTrack *track )
{
  int fd_src;
  gchar *name_src;
  FILE *f;
  g_assert ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) >= 0);
  f = fdopen(fd_src, "w");
  a_gpx_write_track_file("track", track, f); /* Thank you Guilhem! Just when I needed this function... -- Evan */
  fclose(f);
  f = NULL;
  return name_src;
}

/* TODO: cleanup, getr rid of redundancy */

/* depending on type of filter, often only vtl or track will be given.
 * the other can be NULL.
 */
static void acquire ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikDataSourceInterface *source_interface,
		      VikTrwLayer *vtl, VikTrack *track )
{
  /* for manual dialogs */
  GtkWidget *dialog = NULL;
  GtkWidget *status;
  gchar *cmd, *extra;
  gchar *cmd_off, *extra_off;
  acq_dialog_widgets_t *w;
  gpointer user_data;

  /* for UI builder */
  gpointer pass_along_data;
  VikLayerParamData *paramdatas = NULL;

  w_and_interface_t *wi;

  /*** INIT AND CHECK EXISTENCE ***/
  if ( source_interface->init_func )
    user_data = source_interface->init_func();
  else
    user_data = NULL;
  pass_along_data = user_data;

  if ( source_interface->check_existence_func ) {
    gchar *error_str = source_interface->check_existence_func();
    if ( error_str ) {
      a_dialog_error_msg ( GTK_WINDOW(vw), error_str );
      g_free ( error_str );
      return;
    }
  }    

  /* BUILD UI & GET OPTIONS IF NECESSARY. */

  /* POSSIBILITY 0: NO OPTIONS. DO NOTHING HERE. */
  /* POSSIBILITY 1: ADD_SETUP_WIDGETS_FUNC */
  if ( source_interface->add_setup_widgets_func ) {
    dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );

    source_interface->add_setup_widgets_func(dialog, vvp, user_data);
    gtk_window_set_title ( GTK_WINDOW(dialog), _(source_interface->window_title) );

    if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
      source_interface->cleanup_func(user_data);
      gtk_widget_destroy(dialog);
      return;
    }
  }
  /* POSSIBILITY 2: UI BUILDER */
  else if ( source_interface->params ) {
    paramdatas = a_uibuilder_run_dialog ( source_interface->window_title, GTK_WINDOW(vw),
			source_interface->params, source_interface->params_count,
			source_interface->params_groups, source_interface->params_groups_count,
			source_interface->params_defaults );
    if ( paramdatas )
      pass_along_data = paramdatas;
    else
      return; /* TODO: do we have to free anything here? */
  }

  /* CREATE INPUT DATA & GET COMMAND STRING */

  if ( source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRWLAYER ) {
    gchar *name_src = write_tmp_trwlayer ( vtl );

    ((VikDataSourceGetCmdStringFuncWithInput) source_interface->get_cmd_string_func)
	( pass_along_data, &cmd, &extra, name_src );

    g_free ( name_src );
    /* TODO: delete the tmp file? or delete it only after we're done with it? */
  } else if ( source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK ) {
    gchar *name_src = write_tmp_trwlayer ( vtl );
    gchar *name_src_track = write_tmp_track ( track );

    ((VikDataSourceGetCmdStringFuncWithInputInput) source_interface->get_cmd_string_func)
	( pass_along_data, &cmd, &extra, name_src, name_src_track );

    g_free ( name_src );
    g_free ( name_src_track );
  } else if ( source_interface->inputtype == VIK_DATASOURCE_INPUTTYPE_TRACK ) {
    gchar *name_src_track = write_tmp_track ( track );

    ((VikDataSourceGetCmdStringFuncWithInput) source_interface->get_cmd_string_func)
	( pass_along_data, &cmd, &extra, name_src_track );

    g_free ( name_src_track );
  } else
    source_interface->get_cmd_string_func ( pass_along_data, &cmd, &extra );

  /* Get data for Off command */
  if ( source_interface->off_func ) {
    source_interface->off_func ( pass_along_data, &cmd_off, &extra_off );
  }

  /* cleanup for option dialogs */
  if ( source_interface->add_setup_widgets_func ) {
    gtk_widget_destroy(dialog);
    dialog = NULL;
  } else if ( source_interface->params ) {
    a_uibuilder_free_paramdatas ( paramdatas, source_interface->params, source_interface->params_count );
  }

  /*** LET'S DO IT! ***/

  if ( ! cmd )
    return;

  w = g_malloc(sizeof(*w));
  wi = g_malloc(sizeof(*wi));
  wi->w = w;
  wi->w->source_interface = source_interface;
  wi->cmd = cmd;
  wi->extra = extra; /* usually input data type (?) */

  dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_dialog_set_response_sensitive ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, FALSE );
  gtk_window_set_title ( GTK_WINDOW(dialog), _(source_interface->window_title) );


  w->dialog = dialog;
  w->ok = TRUE;
  status = gtk_label_new (_("Status: detecting gpsbabel"));
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), status, FALSE, FALSE, 5 );
  gtk_widget_show_all(status);
  w->status = status;

  w->vw = vw;
  w->vlp = vlp;
  w->vvp = vvp;
  if ( source_interface->add_progress_widgets_func ) {
    source_interface->add_progress_widgets_func ( dialog, user_data );
  }
  w->user_data = user_data;


  g_thread_create((GThreadFunc)get_from_anything, wi, FALSE, NULL );

  gtk_dialog_run ( GTK_DIALOG(dialog) );
  if ( w->ok )
    w->ok = FALSE; /* tell thread to stop. TODO: add mutex */
  else {
    if ( cmd_off ) {
      /* Turn off */
      a_babel_convert_from (NULL, cmd_off, NULL, extra_off, NULL);
    }
    g_free ( w ); /* thread has finished; free w */
  }
  gtk_widget_destroy ( dialog );
}

void a_acquire ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikDataSourceInterface *source_interface ) {
  acquire ( vw, vlp, vvp, source_interface, NULL, NULL );
}

static void acquire_trwlayer_callback ( GObject *menuitem, gpointer *pass_along )
{
  VikDataSourceInterface *iface = g_object_get_data ( menuitem, "vik_acq_iface" );
  VikWindow *vw =	pass_along[0];
  VikLayersPanel *vlp =	pass_along[1];
  VikViewport *vvp =	pass_along[2];
  VikTrwLayer *vtl =	pass_along[3];
  VikTrack *tr =	pass_along[4];

  acquire ( vw, vlp, vvp, iface, vtl, tr );
}

static GtkWidget *acquire_build_menu ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp,
				VikTrwLayer *vtl, VikTrack *track, /* both passed to acquire, although for many filters only one ness */
				const gchar *menu_title, vik_datasource_inputtype_t inputtype )
{
  static gpointer pass_along[5];
  GtkWidget *menu_item=NULL, *menu=NULL;
  GtkWidget *item=NULL;
  int i;

  pass_along[0] = vw;
  pass_along[1] = vlp;
  pass_along[2] = vvp;
  pass_along[3] = vtl;
  pass_along[4] = track;

  for ( i = 0; i < N_FILTERS; i++ ) {
    if ( filters[i]->inputtype == inputtype ) {
      if ( ! menu_item ) { /* do this just once, but return NULL if no filters */
        menu = gtk_menu_new();
        menu_item = gtk_menu_item_new_with_label ( menu_title );
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu );
      }

      item = gtk_menu_item_new_with_label ( filters[i]->window_title );
      g_object_set_data ( G_OBJECT(item), "vik_acq_iface", (gpointer) filters[i] );
      g_signal_connect ( G_OBJECT(item), "activate", G_CALLBACK(acquire_trwlayer_callback), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
    }
  }

  return menu_item;
}

GtkWidget *a_acquire_trwlayer_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl)
{
  return acquire_build_menu ( vw, vlp, vvp, vtl, NULL, "Filter", VIK_DATASOURCE_INPUTTYPE_TRWLAYER );
}

GtkWidget *a_acquire_trwlayer_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl)
{
  if ( filter_track == NULL )
    return NULL;
  else {
    gchar *menu_title = g_strdup_printf ( "Filter with %s", filter_track_name );
    GtkWidget *rv = acquire_build_menu ( vw, vlp, vvp, vtl, filter_track,
			menu_title, VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK );
    g_free ( menu_title );
    return rv;
  }
}

GtkWidget *a_acquire_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrack *tr)
{
  return acquire_build_menu ( vw, vlp, vvp, NULL, tr, "Filter", VIK_DATASOURCE_INPUTTYPE_TRACK );
}

void a_acquire_set_filter_track ( VikTrack *tr, const gchar *name )
{
  if ( filter_track )
    vik_track_free ( filter_track );
  if ( filter_track_name )
    g_free ( filter_track_name );

  filter_track = tr;
  vik_track_ref ( tr );

  filter_track_name = g_strdup(name);
}
