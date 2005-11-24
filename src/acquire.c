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

/*********************************************************
 * Definitions and routines for acquiring data from GPS
 *********************************************************/

/* global data structure used to expose the progress dialog to the worker thread */
typedef struct {
  VikWindow *vw;
  VikLayersPanel *vlp;
  VikViewport *vvp;
  GtkWidget *dialog;
  GtkWidget *status;
  GtkWidget *gps_label;
  GtkWidget *ver_label;
  GtkWidget *id_label;
  GtkWidget *wp_label;
  GtkWidget *trk_label;
  GtkWidget *progress_label;
  gboolean ok;
} acq_dialog_widgets_t;

acq_dialog_widgets_t *w = NULL;
int total_count = -1;
int count;

static void set_total_count(gint cnt)
{
  gchar s[128];
  gdk_threads_enter();
  if (w->ok) {
    g_sprintf(s, "Downloading %d %s...", cnt, (w->progress_label == w->wp_label) ? "waypoints" : "trackpoints");
    gtk_label_set_text ( GTK_LABEL(w->progress_label), s );
    gtk_widget_show ( w->progress_label );
    total_count = cnt;
  }
  gdk_threads_leave();
}

static void set_current_count(gint cnt)
{
  gchar s[128];
  gdk_threads_enter();
  if (w->ok) {
    if (cnt < total_count) {
      g_sprintf(s, "Downloaded %d out of %d %s...", cnt, total_count, (w->progress_label == w->wp_label) ? "waypoints" : "trackpoints");
    } else {
      g_sprintf(s, "Downloaded %d %s.", cnt, (w->progress_label == w->wp_label) ? "waypoints" : "trackpoints");
    }	  
    gtk_label_set_text ( GTK_LABEL(w->progress_label), s );
  }
  gdk_threads_leave();
}

static void set_gps_info(const gchar *info)
{
  gchar s[256];
  gdk_threads_enter();
  if (w->ok) {
    g_sprintf(s, "GPS Device: %s", info);
    gtk_label_set_text ( GTK_LABEL(w->gps_label), s );
  }
  gdk_threads_leave();
}

/* 
 * This routine relies on gpsbabel's diagnostic output to display the progress information. 
 * These outputs differ when different GPS devices are used, so we will need to test
 * them on several and add the corresponding support.
 */
static void progress_func ( BabelProgressCode c, gpointer data )
{
  gchar *line;

  switch(c) {
  case BABEL_DIAG_OUTPUT:
    line = (gchar *)data;

    /* tells us how many items there will be */
    if (strstr(line, "Xfer Wpt")) { 
      w->progress_label = w->wp_label;
    }
    if (strstr(line, "Xfer Trk")) { 
      w->progress_label = w->trk_label;
    }
    if (strstr(line, "PRDDAT")) {
      gchar **tokens = g_strsplit(line, " ", 0);
      gchar info[128];
      int ilen = 0;
      int i;

      for (i=8; tokens[i] && ilen < sizeof(info)-2 && strcmp(tokens[i], "00"); i++) {
	guint ch;
	sscanf(tokens[i], "%x", &ch);
	info[ilen++] = ch;
      }
      info[ilen++] = 0;
      set_gps_info(info);
    }
    if (strstr(line, "RECORD")) { 
      int lsb, msb, cnt;

      sscanf(line+17, "%x", &lsb); 
      sscanf(line+20, "%x", &msb);
      cnt = lsb + msb * 256;
      set_total_count(cnt);
      count = 0;
    }
    if ( strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") ) {
      count++;
      set_current_count(count);
    }
    break;
  case BABEL_DONE:
    break;
  default:
    break;
  }
}

/* this routine is the worker thread.  there is only one simultaneous download allowed */
static void get_from_gps ( gpointer data )
{
  VikTrwLayer *vtl;
	
  if ( w ) {
    /* simultaneous downloads are not allowed, so we return.  */
    g_free(data);
    g_thread_exit ( NULL );
    return;
  }

  w = data;

  gdk_threads_enter();
  vtl = VIK_TRW_LAYER ( vik_layer_create ( VIK_LAYER_TRW, w->vvp, NULL, FALSE ) );
  vik_layer_rename ( VIK_LAYER ( vtl ), "Acquired from GPS" );
  gtk_label_set_text ( GTK_LABEL(w->status), "Working..." );
  gdk_threads_leave();

  if (!a_babel_convert_from (vtl, "-D 9 -t -w -i garmin", progress_func, "/dev/ttyS0")) {
//  if (!a_babel_convert_from (vtl, "-D 9 -t -w -i garmin", progress_func, "/dev/ttyS1")) {
//  if (!a_babel_convert_from_shellcommand (vtl, "(wget -O - \"http://maps.google.com/maps?q=91214 to 94704&output=xml\" 2>/dev/null) | cat ~/vik/tools/temp.ggl | head -3 | tail -1 |  sed 's/.*<page>\\(.*\\)<\\/page>.*/<page>\\1<\\/page>/'", "google", progress_func)) {
    gdk_threads_enter();
    gtk_label_set_text ( GTK_LABEL(w->status), "Error: couldn't find gpsbabel." );
    gdk_threads_leave();
  } 

  gdk_threads_enter();
  if (w->ok) {
    gtk_label_set_text ( GTK_LABEL(w->status), "Done." );
    vik_aggregate_layer_add_layer( vik_layers_panel_get_top_layer(w->vlp), VIK_LAYER(vtl));
    gtk_dialog_set_response_sensitive ( GTK_DIALOG(w->dialog), GTK_RESPONSE_ACCEPT, TRUE );
    gtk_dialog_set_response_sensitive ( GTK_DIALOG(w->dialog), GTK_RESPONSE_REJECT, FALSE );
  } else {
    g_object_unref(vtl);
  }
  g_free ( w );
  w = NULL;
  gdk_threads_leave();
  g_thread_exit ( NULL );
}

void a_acquire_from_gps ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp )
{
  GtkWidget *status, *gpslabel, *verlabel, *idlabel, *wplabel, *trklabel;
  GtkWidget *dialog = NULL;
  acq_dialog_widgets_t *w = g_malloc(sizeof(*w));

  dialog = gtk_dialog_new_with_buttons ( "", NULL, 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_dialog_set_response_sensitive ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, FALSE );

  status = gtk_label_new ("Status: detecting gpsbabel");
  gpslabel = gtk_label_new ("GPS device: N/A");
  verlabel = gtk_label_new ("Version: N/A");
  idlabel = gtk_label_new ("ID: N/A");
  wplabel = gtk_label_new ("WP");
  trklabel = gtk_label_new ("TRK");

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), status, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), gpslabel, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), wplabel, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), trklabel, FALSE, FALSE, 5 );

  gtk_window_set_title ( GTK_WINDOW(dialog), "Acquire data from GPS" );

  gtk_widget_show ( status );
  gtk_widget_show ( gpslabel );
  gtk_widget_show ( dialog );

  w->dialog = dialog;
  w->status = status;
  w->gps_label = gpslabel;
  w->id_label = idlabel;
  w->ver_label = verlabel;
  w->progress_label = w->wp_label = wplabel;
  w->trk_label = trklabel;
  w->vw = vw;
  w->vlp = vlp;
  w->vvp = vvp;
  w->ok = TRUE;
  g_thread_create((GThreadFunc)get_from_gps, w, FALSE, NULL );

  gtk_dialog_run ( GTK_DIALOG(dialog) );
  w->ok = FALSE;
  gtk_widget_destroy ( dialog );
}
