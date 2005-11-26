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

static gboolean gps_acquire_in_progress = FALSE;

static void datasource_gps_get_cmd_string ( gpointer add_widgets_data_not_used, gchar **babelargs, gchar **input_file );
static void datasource_gps_cleanup ( gpointer data );
static void datasource_gps_progress ( BabelProgressCode c, gpointer data, acq_dialog_widgets_t *w );
gpointer datasource_gps_add_progress_widgets ( GtkWidget *dialog );

VikDataSourceInterface vik_datasource_gps_interface = {
  "Acquire from GPS",
  VIK_DATASOURCE_GPSBABEL_DIRECT,
  VIK_DATASOURCE_CREATENEWLAYER,
  (VikDataSourceAddWidgetsFunc)		NULL,
  (VikDataSourceGetCmdStringFunc)	datasource_gps_get_cmd_string,
  (VikDataSourceFirstCleanupFunc)	NULL,
  (VikDataSourceProgressFunc)		datasource_gps_progress,
  (VikDataSourceAddProgressWidgetsFunc)	datasource_gps_add_progress_widgets,
  (VikDataSourceCleanupFunc)		datasource_gps_cleanup
};

/*********************************************************
 * Definitions and routines for acquiring data from GPS
 *********************************************************/

/* widgets in progress dialog specific to GPS */
/* also counts needed for progress */
typedef struct {
  GtkWidget *gps_label;
  GtkWidget *ver_label;
  GtkWidget *id_label;
  GtkWidget *wp_label;
  GtkWidget *trk_label;
  GtkWidget *progress_label;
  int total_count;
  int count;
} gps_acq_dialog_widgets_t;

static void datasource_gps_get_cmd_string ( gpointer add_widgets_data_not_used, gchar **babelargs, gchar **input_file )
{
  if (gps_acquire_in_progress) {
    *babelargs = *input_file = NULL;
  }

 gps_acquire_in_progress = TRUE;
 *babelargs = g_strdup_printf("%s", "-D 9 -t -w -i garmin");
 *input_file = g_strdup_printf("%s", "/dev/ttyS1" );
}

static void datasource_gps_cleanup ( gpointer data )
{
  g_free ( data );
  gps_acquire_in_progress = FALSE;
}

static void set_total_count(gint cnt, acq_dialog_widgets_t *w)
{
  gchar s[128];
  gdk_threads_enter();
  if (w->ok) {
    gps_acq_dialog_widgets_t *gps_data = (gps_acq_dialog_widgets_t *)w->specific_data;
    g_sprintf(s, "Downloading %d %s...", cnt, (gps_data->progress_label == gps_data->wp_label) ? "waypoints" : "trackpoints");
    gtk_label_set_text ( GTK_LABEL(gps_data->progress_label), s );
    gtk_widget_show ( gps_data->progress_label );
    gps_data->total_count = cnt;
  }
  gdk_threads_leave();
}

static void set_current_count(gint cnt, acq_dialog_widgets_t *w)
{
  gchar s[128];
  gdk_threads_enter();
  if (w->ok) {
    gps_acq_dialog_widgets_t *gps_data = (gps_acq_dialog_widgets_t *)w->specific_data;

    if (cnt < gps_data->total_count) {
      g_sprintf(s, "Downloaded %d out of %d %s...", cnt, gps_data->total_count, (gps_data->progress_label == gps_data->wp_label) ? "waypoints" : "trackpoints");
    } else {
      g_sprintf(s, "Downloaded %d %s.", cnt, (gps_data->progress_label == gps_data->wp_label) ? "waypoints" : "trackpoints");
    }	  
    gtk_label_set_text ( GTK_LABEL(gps_data->progress_label), s );
  }
  gdk_threads_leave();
}

static void set_gps_info(const gchar *info, acq_dialog_widgets_t *w)
{
  gchar s[256];
  gdk_threads_enter();
  if (w->ok) {
    g_sprintf(s, "GPS Device: %s", info);
    gtk_label_set_text ( GTK_LABEL(((gps_acq_dialog_widgets_t *)w->specific_data)->gps_label), s );
  }
  gdk_threads_leave();
}

/* 
 * This routine relies on gpsbabel's diagnostic output to display the progress information. 
 * These outputs differ when different GPS devices are used, so we will need to test
 * them on several and add the corresponding support.
 */
static void datasource_gps_progress ( BabelProgressCode c, gpointer data, acq_dialog_widgets_t *w )
{
  gchar *line;
  gps_acq_dialog_widgets_t *gps_data = (gps_acq_dialog_widgets_t *)w->specific_data;

  switch(c) {
  case BABEL_DIAG_OUTPUT:
    line = (gchar *)data;

    /* tells us how many items there will be */
    if (strstr(line, "Xfer Wpt")) { 
      gps_data->progress_label = gps_data->wp_label;
    }
    if (strstr(line, "Xfer Trk")) { 
      gps_data->progress_label = gps_data->trk_label;
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
      set_gps_info(info, w);
    }
    if (strstr(line, "RECORD")) { 
      int lsb, msb, cnt;

      sscanf(line+17, "%x", &lsb); 
      sscanf(line+20, "%x", &msb);
      cnt = lsb + msb * 256;
      set_total_count(cnt, w);
      gps_data->count = 0;
    }
    if ( strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") ) {
      gps_data->count++;
      set_current_count(gps_data->count, w);
    }
    break;
  case BABEL_DONE:
    break;
  default:
    break;
  }
}

gpointer datasource_gps_add_progress_widgets ( GtkWidget *dialog )
{
  GtkWidget *gpslabel, *verlabel, *idlabel, *wplabel, *trklabel;

  gps_acq_dialog_widgets_t *w_gps = g_malloc(sizeof(*w_gps));

  gpslabel = gtk_label_new ("GPS device: N/A");
  verlabel = gtk_label_new ("");
  idlabel = gtk_label_new ("");
  wplabel = gtk_label_new ("");
  trklabel = gtk_label_new ("");

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), gpslabel, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), wplabel, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), trklabel, FALSE, FALSE, 5 );

  gtk_window_set_title ( GTK_WINDOW(dialog), "Acquire data from GPS" );

  gtk_widget_show_all ( dialog );

  w_gps->gps_label = gpslabel;
  w_gps->id_label = idlabel;
  w_gps->ver_label = verlabel;
  w_gps->progress_label = w_gps->wp_label = wplabel;
  w_gps->trk_label = trklabel;
  w_gps->total_count = -1;
  return w_gps;
}
