/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006, Alex Foobarian <foobarian@gmail.com>
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
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"

static gboolean gps_acquire_in_progress = FALSE;

static gint last_active = -1;
static gboolean last_get_tracks = TRUE;
static gboolean last_get_waypoints = TRUE;

static gpointer datasource_gps_init_func ( );
static void datasource_gps_get_cmd_string ( gpointer add_widgets_data_not_used, gchar **babelargs, gchar **input_file );
static void datasource_gps_cleanup ( gpointer user_data );
static void datasource_gps_progress ( BabelProgressCode c, gpointer data, acq_dialog_widgets_t *w );
static void datasource_gps_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_gps_add_progress_widgets ( GtkWidget *dialog, gpointer user_data );
static void datasource_gps_off ( gpointer add_widgets_data_not_used, gchar **babelargs, gchar **input_file );

VikDataSourceInterface vik_datasource_gps_interface = {
  N_("Acquire from GPS"),
  N_("Acquired from GPS"),
  VIK_DATASOURCE_GPSBABEL_DIRECT,
  VIK_DATASOURCE_CREATENEWLAYER,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  TRUE,
  TRUE,
  (VikDataSourceInitFunc)		datasource_gps_init_func,
  (VikDataSourceCheckExistenceFunc)	NULL,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_gps_add_setup_widgets,
  (VikDataSourceGetCmdStringFunc)	datasource_gps_get_cmd_string,
  (VikDataSourceProcessFunc)		NULL,
  (VikDataSourceProgressFunc)		datasource_gps_progress,
  (VikDataSourceAddProgressWidgetsFunc)	datasource_gps_add_progress_widgets,
  (VikDataSourceCleanupFunc)		datasource_gps_cleanup,
  (VikDataSourceOffFunc)                datasource_gps_off
};

/*********************************************************
 * Definitions and routines for acquiring data from GPS
 *********************************************************/

/* widgets in setup dialog specific to GPS */
/* widgets in progress dialog specific to GPS */
/* also counts needed for progress */
typedef struct {
  /* setup dialog */
  GtkWidget *proto_l;
  GtkComboBox *proto_b;
  GtkWidget *ser_l;
  GtkComboBox *ser_b;
  GtkWidget *off_request_l;
  GtkCheckButton *off_request_b;
  GtkWidget *get_tracks_l;
  GtkCheckButton *get_tracks_b;
  GtkWidget *get_waypoints_l;
  GtkCheckButton *get_waypoints_b;

  /* progress dialog */
  GtkWidget *gps_label;
  GtkWidget *ver_label;
  GtkWidget *id_label;
  GtkWidget *wp_label;
  GtkWidget *trk_label;
  GtkWidget *progress_label;

  /* state */
  int total_count;
  int count;
} gps_user_data_t;

static gpointer datasource_gps_init_func ()
{
  return g_malloc (sizeof(gps_user_data_t));
}

static void datasource_gps_get_cmd_string ( gpointer user_data, gchar **babelargs, gchar **input_file )
{
  char *ser = NULL;
  char *device = NULL;
  char *tracks = NULL;
  char *waypoints = NULL;
  gps_user_data_t *w = (gps_user_data_t *)user_data;

  if (gps_acquire_in_progress) {
    *babelargs = *input_file = NULL;
  }
  
  gps_acquire_in_progress = TRUE;

  last_active = gtk_combo_box_get_active(GTK_COMBO_BOX(w->proto_b));
  device = ((BabelDevice*)g_list_nth_data(a_babel_device_list, last_active))->name;

  last_get_tracks = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->get_tracks_b));
  if (last_get_tracks)
    tracks = "-t";
  else
    tracks = "";
  last_get_waypoints = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->get_waypoints_b));
  if (last_get_waypoints)
    waypoints = "-w";
  else
    waypoints = "";

  *babelargs = g_strdup_printf("-D 9 %s %s -i %s", tracks, waypoints, device);
  /* device points to static content => no free */
  device = NULL;
  tracks = NULL;
  waypoints = NULL;

  ser = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w->ser_b));
  *input_file = g_strdup(ser);

  g_debug(_("using cmdline '%s' and file '%s'\n"), *babelargs, *input_file);
}


static void datasource_gps_off ( gpointer user_data, gchar **babelargs, gchar **input_file )
{
  char *ser = NULL;
  char *device = NULL;
  gps_user_data_t *w = (gps_user_data_t *)user_data;

  if (gps_acquire_in_progress) {
    *babelargs = *input_file = NULL;
  }

  /* See if we should turn off the device */
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->off_request_b))) {
    return;
  }
  
  last_active = gtk_combo_box_get_active(GTK_COMBO_BOX(w->proto_b));
  device = ((BabelDevice*)g_list_nth_data(a_babel_device_list, last_active))->name;
  if (!strcmp(device, "garmin")) {
    device = "garmin,power_off";
  }
  else if (!strcmp(device, "navilink")) {
    device = "navilink,power_off";
  }
  else {
    return;
  }

  *babelargs = g_strdup_printf("-i %s", device);
  /* device points to static content => no free */
  device = NULL;
  
  ser = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w->ser_b));
  *input_file = g_strdup(ser);
}


static void datasource_gps_cleanup ( gpointer user_data )
{
  g_free ( user_data );
  gps_acquire_in_progress = FALSE;
}

static void set_total_count(gint cnt, acq_dialog_widgets_t *w)
{
  gchar *s = NULL;
  gdk_threads_enter();
  if (w->ok) {
    gps_user_data_t *gps_data = (gps_user_data_t *)w->user_data;
    const gchar *tmp_str;
    if (gps_data->progress_label == gps_data->wp_label)
      tmp_str = ngettext("Downloading %d waypoint...", "Downloading %d waypoints...", cnt);
    else
      tmp_str = ngettext("Downloading %d trackpoint...", "Downloading %d trackpoints...", cnt);
    s = g_strdup_printf(tmp_str, cnt);
    gtk_label_set_text ( GTK_LABEL(gps_data->progress_label), s );
    gtk_widget_show ( gps_data->progress_label );
    gps_data->total_count = cnt;
  }
  g_free(s); s = NULL;
  gdk_threads_leave();
}

static void set_current_count(gint cnt, acq_dialog_widgets_t *w)
{
  gchar *s = NULL;
  gdk_threads_enter();
  if (w->ok) {
    gps_user_data_t *gps_data = (gps_user_data_t *)w->user_data;

    if (cnt < gps_data->total_count) {
      s = g_strdup_printf(_("Downloaded %d out of %d %s..."), cnt, gps_data->total_count, (gps_data->progress_label == gps_data->wp_label) ? "waypoints" : "trackpoints");
    } else {
      s = g_strdup_printf(_("Downloaded %d %s."), cnt, (gps_data->progress_label == gps_data->wp_label) ? "waypoints" : "trackpoints");
    }	  
    gtk_label_set_text ( GTK_LABEL(gps_data->progress_label), s );
  }
  g_free(s); s = NULL;
  gdk_threads_leave();
}

static void set_gps_info(const gchar *info, acq_dialog_widgets_t *w)
{
  gchar *s = NULL;
  gdk_threads_enter();
  if (w->ok) {
    s = g_strdup_printf(_("GPS Device: %s"), info);
    gtk_label_set_text ( GTK_LABEL(((gps_user_data_t *)w->user_data)->gps_label), s );
  }
  g_free(s); s = NULL;
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
  gps_user_data_t *gps_data = (gps_user_data_t *)w->user_data;

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
      int n_tokens = 0;

      while (tokens[n_tokens])
        n_tokens++;

      if (n_tokens > 8) {
        for (i=8; tokens[i] && ilen < sizeof(info)-2 && strcmp(tokens[i], "00"); i++) {
	  guint ch;
	  sscanf(tokens[i], "%x", &ch);
	  info[ilen++] = ch;
        }
        info[ilen++] = 0;
        set_gps_info(info, w);
      }
      g_strfreev(tokens);
    }
    /* eg: "Unit:\teTrex Legend HCx Software Version 2.90\n" */
    if (strstr(line, "Unit:")) {
      gchar **tokens = g_strsplit(line, "\t", 0);
      int n_tokens = 0;
      while (tokens[n_tokens])
        n_tokens++;

      if (n_tokens > 1) {
        set_gps_info(tokens[1], w);
      }
      g_strfreev(tokens);
    }
    if (strstr(line, "RECORD")) { 
      int lsb, msb, cnt;

      if (strlen(line) > 20) {
       sscanf(line+17, "%x", &lsb); 
       sscanf(line+20, "%x", &msb);
       cnt = lsb + msb * 256;
       set_total_count(cnt, w);
       gps_data->count = 0;
      }
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

void append_element (gpointer elem, gpointer user_data)
{
  GtkComboBox *combo = GTK_COMBO_BOX (user_data);
  const gchar *text = ((BabelDevice*)elem)->label;
  gtk_combo_box_append_text (combo, text);
}

static gint find_entry = -1;
static gint garmin_entry = -1;

static void find_garmin (gpointer elem, gpointer user_data)
{
  const gchar *name = ((BabelDevice*)elem)->name;
  find_entry++;
  if (!strcmp(name, "garmin")) {
    garmin_entry = find_entry;
  }
}

void datasource_gps_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
  gps_user_data_t *w = (gps_user_data_t *)user_data;
  GtkTable *box, *data_type_box;

  w->proto_l = gtk_label_new (_("GPS Protocol:"));
  w->proto_b = GTK_COMBO_BOX(gtk_combo_box_new_text ());
  g_list_foreach (a_babel_device_list, append_element, w->proto_b);

  // Maintain default to Garmin devices (assumed most popular/numerous device)
  if ( last_active < 0 ) {
    find_entry = -1;
    g_list_foreach (a_babel_device_list, find_garmin, NULL);
    if ( garmin_entry < 0 )
      // Not found - so set it to the first entry
      last_active = 0;
    else
      // Found
      last_active = garmin_entry;
  }

  gtk_combo_box_set_active (w->proto_b, last_active);
  g_object_ref(w->proto_b);

  w->ser_l = gtk_label_new (_("Serial Port:"));
  w->ser_b = GTK_COMBO_BOX(gtk_combo_box_entry_new_text ());
#ifdef WINDOWS
  gtk_combo_box_append_text (w->ser_b, "com1");
#else
  /* Here just try to see if the device is available which gets passed onto gpsbabel
     List USB devices first as these will generally only be present if autogenerated by udev or similar
     User is still able to set their own free text entry */
  if (g_access ("/dev/ttyUSB0", R_OK) == 0)
    gtk_combo_box_append_text (w->ser_b, "/dev/ttyUSB0");
  if (g_access ("/dev/ttyUSB1", R_OK) == 0)
    gtk_combo_box_append_text (w->ser_b, "/dev/ttyUSB1");
  if (g_access ("/dev/ttyS0", R_OK) == 0)
    gtk_combo_box_append_text (w->ser_b, "/dev/ttyS0");
  if (g_access ("/dev/ttyS1", R_OK) == 0)
    gtk_combo_box_append_text (w->ser_b, "/dev/ttyS1");
#endif
  gtk_combo_box_append_text (w->ser_b, "usb:");
  gtk_combo_box_set_active (w->ser_b, 0);
  g_object_ref(w->ser_b);

  w->off_request_l = gtk_label_new (_("Turn Off After Transfer\n(Garmin/NAViLink Only)"));
  w->off_request_b = GTK_CHECK_BUTTON ( gtk_check_button_new () );

  w->get_tracks_l = gtk_label_new (_("Tracks:"));
  w->get_tracks_b = GTK_CHECK_BUTTON ( gtk_check_button_new () );
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->get_tracks_b), last_get_tracks);

  w->get_waypoints_l = gtk_label_new (_("Waypoints:"));
  w->get_waypoints_b = GTK_CHECK_BUTTON ( gtk_check_button_new () );
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->get_waypoints_b), last_get_waypoints);

  box = GTK_TABLE(gtk_table_new(2, 4, FALSE));
  data_type_box = GTK_TABLE(gtk_table_new(4, 1, FALSE));

  gtk_table_attach_defaults(box, GTK_WIDGET(w->proto_l), 0, 1, 0, 1);
  gtk_table_attach_defaults(box, GTK_WIDGET(w->proto_b), 1, 2, 0, 1);
  gtk_table_attach_defaults(box, GTK_WIDGET(w->ser_l), 0, 1, 1, 2);
  gtk_table_attach_defaults(box, GTK_WIDGET(w->ser_b), 1, 2, 1, 2);
  gtk_table_attach_defaults(data_type_box, GTK_WIDGET(w->get_tracks_l), 0, 1, 0, 1);
  gtk_table_attach_defaults(data_type_box, GTK_WIDGET(w->get_tracks_b), 1, 2, 0, 1);
  gtk_table_attach_defaults(data_type_box, GTK_WIDGET(w->get_waypoints_l), 2, 3, 0, 1);
  gtk_table_attach_defaults(data_type_box, GTK_WIDGET(w->get_waypoints_b), 3, 4, 0, 1);
  gtk_table_attach_defaults(box, GTK_WIDGET(data_type_box), 0, 2, 2, 3);
  gtk_table_attach_defaults(box, GTK_WIDGET(w->off_request_l), 0, 1, 3, 4);
  gtk_table_attach_defaults(box, GTK_WIDGET(w->off_request_b), 1, 3, 3, 4);
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(box), FALSE, FALSE, 5 );

  gtk_widget_show_all ( dialog );
}

void datasource_gps_add_progress_widgets ( GtkWidget *dialog, gpointer user_data )
{
  GtkWidget *gpslabel, *verlabel, *idlabel, *wplabel, *trklabel;

  gps_user_data_t *w_gps = (gps_user_data_t *)user_data;

  gpslabel = gtk_label_new (_("GPS device: N/A"));
  verlabel = gtk_label_new ("");
  idlabel = gtk_label_new ("");
  wplabel = gtk_label_new ("");
  trklabel = gtk_label_new ("");

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), gpslabel, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), wplabel, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), trklabel, FALSE, FALSE, 5 );

  gtk_widget_show_all ( dialog );

  w_gps->gps_label = gpslabel;
  w_gps->id_label = idlabel;
  w_gps->ver_label = verlabel;
  w_gps->progress_label = w_gps->wp_label = wplabel;
  w_gps->trk_label = trklabel;
  w_gps->total_count = -1;
}
