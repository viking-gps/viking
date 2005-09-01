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

#include <gtk/gtk.h>
#include <time.h>
#include "coords.h"
#include "vikcoord.h"
#include "viktrack.h"
#include "viktrwlayer_propwin.h"
#include "vikwaypoint.h"
#include "dialog.h"
#include "globals.h"

#define PROFILE_WIDTH 600
#define PROFILE_HEIGHT 300

static void minmax_alt(const gdouble *altitudes, gdouble *min, gdouble *max)
{
  *max = -1000;
  *min = 20000;
  guint i;
  for ( i=0; i < PROFILE_WIDTH; i++ ) {
    if ( altitudes[i] != VIK_DEFAULT_ALTITUDE ) {
      if ( altitudes[i] > *max )
        *max = altitudes[i];
      if ( altitudes[i] < *min )
        *min = altitudes[i];
    }
  }

}

GtkWidget *vik_trw_layer_create_profile ( GtkWidget *window, VikTrack *tr, gdouble *min_alt, gdouble *max_alt )
{
  GdkPixmap *pix = gdk_pixmap_new( window->window, PROFILE_WIDTH, PROFILE_HEIGHT, -1 );
  GtkWidget *image = gtk_image_new_from_pixmap ( pix, NULL );
  gdouble *altitudes = vik_track_make_elevation_map ( tr, PROFILE_WIDTH );

  guint i;

  GdkGC *no_alt_info = gdk_gc_new ( window->window );
  GdkColor color;

  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( no_alt_info, &color);

  minmax_alt(altitudes, min_alt, max_alt);
  
  for ( i = 0; i < PROFILE_WIDTH; i++ )
    if ( altitudes[i] == VIK_DEFAULT_ALTITUDE )
      gdk_draw_line ( GDK_DRAWABLE(pix), no_alt_info, i, 0, i, PROFILE_HEIGHT );
    else
      gdk_draw_line ( GDK_DRAWABLE(pix), window->style->white_gc, i, 0, i, PROFILE_HEIGHT-PROFILE_HEIGHT*(altitudes[i]-*min_alt)/(*max_alt-*min_alt) );

  g_object_unref ( G_OBJECT(pix) );
  g_free ( altitudes );
  g_object_unref ( G_OBJECT(no_alt_info) );
  return image;
}

gint vik_trw_layer_propwin_run ( GtkWindow *parent, VikTrack *tr )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons ("Track Properties",
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  "Split Segments",
                                                  VIK_TRW_LAYER_PROPWIN_SPLIT,
                                                  "Reverse",
                                                  VIK_TRW_LAYER_PROPWIN_REVERSE,
                                                  "Delete Dupl.",
                                                  VIK_TRW_LAYER_PROPWIN_DEL_DUP,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  GtkWidget *left_vbox, *right_vbox, *hbox;
  GtkWidget *e_cmt;
  GtkWidget *l_len, *l_tps, *l_segs, *l_dups, *l_maxs, *l_avgs, *l_avgd, *l_elev, *l_galo;
  gdouble tr_len;
  guint32 tp_count, seg_count;
  gint resp;

  gdouble min_alt, max_alt;
  GtkWidget *profile = vik_trw_layer_create_profile(GTK_WIDGET(parent),tr,&min_alt,&max_alt);

  static gchar *label_texts[] = { "<b>Comment:</b>", "<b>Track Length:</b>", "<b>Trackpoints:</b>", "<b>Segments:</b>", "<b>Duplicate Points</b>", "<b>Max Speed</b>", "<b>Avg. Speed</b>", "<b>Avg. Dist. Between TPs</b>", "<b>Elevation Range</b>", "<b>Total Elevation Gain/Loss:</b>" };
  static gchar tmp_buf[20];

  left_vbox = a_dialog_create_label_vbox ( label_texts, sizeof(label_texts) / sizeof(label_texts[0]) );
  right_vbox = gtk_vbox_new ( TRUE, 3 );

  e_cmt = gtk_entry_new ();
  if ( tr->comment )
    gtk_entry_set_text ( GTK_ENTRY(e_cmt), tr->comment );
  g_signal_connect_swapped ( e_cmt, "activate", G_CALLBACK(a_dialog_response_accept), GTK_DIALOG(dialog) );

  tr_len = vik_track_get_length(tr);
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", tr_len );
  l_len = gtk_label_new ( tmp_buf );

  tp_count = vik_track_get_tp_count(tr);
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%u", tp_count );
  l_tps = gtk_label_new ( tmp_buf );

  seg_count = vik_track_get_segment_count(tr) ;
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count );
  l_segs = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%lu", vik_track_get_dup_point_count(tr) );
  l_dups = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", vik_track_get_max_speed(tr) );
  l_maxs = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", vik_track_get_average_speed(tr) );
  l_avgs = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / ( tp_count - seg_count ) );
  l_avgd = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt );
  l_elev = gtk_label_new ( tmp_buf );

  vik_track_get_total_elevation_gain(tr, &max_alt, &min_alt );
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m / %.0f m", max_alt, min_alt );
  l_galo = gtk_label_new ( tmp_buf );

  gtk_box_pack_start (GTK_BOX(right_vbox), e_cmt, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_len, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_tps, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_segs, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_dups, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_maxs, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_avgs, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_avgd, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_elev, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_galo, FALSE, FALSE, 0);

  hbox = gtk_hbox_new ( TRUE, 0 );
  gtk_box_pack_start (GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), right_vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), profile, FALSE, FALSE, 0);


  gtk_widget_show_all ( dialog );
  resp = gtk_dialog_run (GTK_DIALOG (dialog));
  if ( resp == GTK_RESPONSE_ACCEPT )
    vik_track_set_comment ( tr, gtk_entry_get_text ( GTK_ENTRY(e_cmt) ) );

  gtk_widget_destroy ( dialog );
  return resp;
}
