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
#include <string.h>
#include "coords.h"
#include "vikcoord.h"
#include "viktrack.h"
#include "viktrwlayer_propwin.h"
#include "vikwaypoint.h"
#include "dialog.h"
#include "globals.h"

#include "vikviewport.h" /* ugh */
#include "viktreeview.h" /* ugh */
#include <gdk-pixbuf/gdk-pixdata.h>
#include "viklayer.h" /* ugh */
#include "vikaggregatelayer.h"
#include "viklayerspanel.h" /* ugh */

#define PROFILE_WIDTH 600
#define PROFILE_HEIGHT 300
#define MIN_ALT_DIFF 100.0
#define MIN_SPEED_DIFF 20.0

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

#define MARGIN 50
#define LINES 5
void track_profile_click( GtkWidget *image, GdkEventButton *event, gpointer *pass_along )
{
  VikTrack *tr = pass_along[0];
  VikCoord *coord = vik_track_get_closest_tp_by_percentage_dist ( tr, (gdouble) (event->x - MARGIN - 2) / PROFILE_WIDTH );
  if ( coord ) {
    VikLayersPanel *vlp = pass_along[1];
    vik_viewport_set_center_coord ( vik_layers_panel_get_viewport(vlp), coord );
    vik_layers_panel_emit_update ( vlp );
    g_free ( coord );
  }
}

GtkWidget *vik_trw_layer_create_profile ( GtkWidget *window, VikTrack *tr, gdouble *min_alt, gdouble *max_alt, gpointer vlp )
{
  GdkPixmap *pix;
  GtkWidget *image;
  GtkWidget *eventbox;
  gdouble *altitudes = vik_track_make_elevation_map ( tr, PROFILE_WIDTH );
  gdouble mina, maxa;
  gpointer *pass_along;
  guint i;

  if ( altitudes == NULL ) {
    *min_alt = *max_alt = VIK_DEFAULT_ALTITUDE;
    return NULL;
  }

  pix = gdk_pixmap_new( window->window, PROFILE_WIDTH + MARGIN, PROFILE_HEIGHT, -1 );
  image = gtk_image_new_from_pixmap ( pix, NULL );

  GdkGC *no_alt_info = gdk_gc_new ( window->window );
  GdkColor color;

  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( no_alt_info, &color);


  minmax_alt(altitudes, min_alt, max_alt);
  mina = *min_alt; 
  maxa = *max_alt;
  if  (maxa-mina < MIN_ALT_DIFF) {
    maxa = mina + MIN_ALT_DIFF;
  }
  
  /* clear the image */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->bg_gc[0], 
		     TRUE, 0, 0, MARGIN, PROFILE_HEIGHT);
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->mid_gc[0], 
		     TRUE, MARGIN, 0, PROFILE_WIDTH, PROFILE_HEIGHT);

  /* draw grid */
#define LABEL_FONT "Sans 8"
  for (i=0; i<=LINES; i++) {
    PangoFontDescription *pfd;
    PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(image), NULL);
    gchar s[32];

    pfd = pango_font_description_from_string (LABEL_FONT);
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);
    sprintf(s, "%8dm", (int)(mina + (LINES-i)*(maxa-mina)/LINES));
    pango_layout_set_text(pl, s, -1);
    gdk_draw_layout(GDK_DRAWABLE(pix), window->style->fg_gc[0], 0, 
		    CLAMP((int)i*PROFILE_HEIGHT/LINES - 5, 0, PROFILE_HEIGHT-15), pl);

    gdk_draw_line (GDK_DRAWABLE(pix), window->style->dark_gc[0], 
		   MARGIN, PROFILE_HEIGHT/LINES * i, MARGIN + PROFILE_WIDTH, PROFILE_HEIGHT/LINES * i);
  }

  /* draw elevations */
  for ( i = 0; i < PROFILE_WIDTH; i++ )
    if ( altitudes[i] == VIK_DEFAULT_ALTITUDE ) 
      gdk_draw_line ( GDK_DRAWABLE(pix), no_alt_info, 
		      i + MARGIN, 0, i + MARGIN, PROFILE_HEIGHT );
    else 
      gdk_draw_line ( GDK_DRAWABLE(pix), window->style->dark_gc[3], 
		      i + MARGIN, PROFILE_HEIGHT, i + MARGIN, PROFILE_HEIGHT-PROFILE_HEIGHT*(altitudes[i]-mina)/(maxa-mina) );

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->black_gc, FALSE, MARGIN, 0, PROFILE_WIDTH-1, PROFILE_HEIGHT-1);



  g_object_unref ( G_OBJECT(pix) );
  g_free ( altitudes );
  g_object_unref ( G_OBJECT(no_alt_info) );

  pass_along = g_malloc ( sizeof(gpointer) * 2 );
  pass_along[0] = tr;
  pass_along[1] = vlp;

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_profile_click), pass_along );
  g_signal_connect_swapped ( G_OBJECT(eventbox), "destroy", G_CALLBACK(g_free), pass_along );
  gtk_container_add ( GTK_CONTAINER(eventbox), image );

  return eventbox;
}

#define METRIC 1
#ifdef METRIC 
#define MTOK(v) ( (v)*3600.0/1000.0) /* m/s to km/h */
#else
#define MTOK(v) ( (v)*3600.0/1000.0 * 0.6214) /* m/s to mph - we'll handle this globally eventually but for now ...*/
#endif

GtkWidget *vik_trw_layer_create_vtdiag ( GtkWidget *window, VikTrack *tr)
{
  GdkPixmap *pix;
  GtkWidget *image;
  gdouble mins, maxs;
  guint i;


  gdouble *speeds = vik_track_make_speed_map ( tr, PROFILE_WIDTH );
  if ( speeds == NULL )
    return NULL;

  pix = gdk_pixmap_new( window->window, PROFILE_WIDTH + MARGIN, PROFILE_HEIGHT, -1 );
  image = gtk_image_new_from_pixmap ( pix, NULL );

  for (i=0; i<PROFILE_WIDTH; i++) {
    speeds[i] = MTOK(speeds[i]);
  }

  minmax_alt(speeds, &mins, &maxs);
  if  (maxs-mins < MIN_SPEED_DIFF) {
    maxs = mins + MIN_SPEED_DIFF;
  }
  
  /* clear the image */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->bg_gc[0], 
		     TRUE, 0, 0, MARGIN, PROFILE_HEIGHT);
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->mid_gc[0], 
		     TRUE, MARGIN, 0, PROFILE_WIDTH, PROFILE_HEIGHT);

#if 0
  /* XXX this can go out, it's just a helpful dev tool */
  {
    int j;
    GdkGC **colors[8] = { window->style->bg_gc, window->style->fg_gc, 
			 window->style->light_gc, 
			 window->style->dark_gc, window->style->mid_gc, 
			 window->style->text_gc, window->style->base_gc,
			 window->style->text_aa_gc };
    for (i=0; i<5; i++) {
      for (j=0; j<8; j++) {
	gdk_draw_rectangle(GDK_DRAWABLE(pix), colors[j][i],
			   TRUE, i*20, j*20, 20, 20);
	gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->black_gc,
			   FALSE, i*20, j*20, 20, 20);
      }
    }
  }
#else

  /* draw grid */
#define LABEL_FONT "Sans 8"
  for (i=0; i<=LINES; i++) {
    PangoFontDescription *pfd;
    PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(image), NULL);
    gchar s[32];

    pfd = pango_font_description_from_string (LABEL_FONT);
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);
#ifdef METRIC 
    sprintf(s, "%5dkm/h", (int)(mins + (LINES-i)*(maxs-mins)/LINES));
#else
    sprintf(s, "%8dmph", (int)(mins + (LINES-i)*(maxs-mins)/LINES));
#endif
    pango_layout_set_text(pl, s, -1);
    gdk_draw_layout(GDK_DRAWABLE(pix), window->style->fg_gc[0], 0, 
		    CLAMP((int)i*PROFILE_HEIGHT/LINES - 5, 0, PROFILE_HEIGHT-15), pl);

    gdk_draw_line (GDK_DRAWABLE(pix), window->style->dark_gc[0], 
		   MARGIN, PROFILE_HEIGHT/LINES * i, MARGIN + PROFILE_WIDTH, PROFILE_HEIGHT/LINES * i);
  }

  /* draw speeds */
  for ( i = 0; i < PROFILE_WIDTH; i++ )
      gdk_draw_line ( GDK_DRAWABLE(pix), window->style->dark_gc[3], 
		      i + MARGIN, PROFILE_HEIGHT, i + MARGIN, PROFILE_HEIGHT-PROFILE_HEIGHT*(speeds[i]-mins)/(maxs-mins) );
#endif
  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->black_gc, FALSE, MARGIN, 0, PROFILE_WIDTH-1, PROFILE_HEIGHT-1);

  g_object_unref ( G_OBJECT(pix) );
  g_free ( speeds );
  return image;
}
#undef MARGIN
#undef LINES

gint vik_trw_layer_propwin_run ( GtkWindow *parent, VikTrack *tr, gpointer vlp )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons ("Track Properties",
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
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
  GtkWidget *l_len, *l_tps, *l_segs, *l_dups, *l_maxs, *l_avgs, *l_avgd, *l_elev, *l_galo, *l_begin, *l_end, *l_dur;
  gdouble tr_len;
  guint32 tp_count, seg_count;
  gint resp;

  gdouble min_alt, max_alt;
  GtkWidget *profile = vik_trw_layer_create_profile(GTK_WIDGET(parent),tr,&min_alt,&max_alt,vlp);
  GtkWidget *vtdiag = vik_trw_layer_create_vtdiag(GTK_WIDGET(parent), tr);
  GtkWidget *graphs = gtk_notebook_new();

  static gchar *label_texts[] = { "<b>Comment:</b>", "<b>Track Length:</b>", "<b>Trackpoints:</b>", "<b>Segments:</b>", "<b>Duplicate Points:</b>", "<b>Max Speed:</b>", "<b>Avg. Speed:</b>", "<b>Avg. Dist. Between TPs:</b>", "<b>Elevation Range:</b>", "<b>Total Elevation Gain/Loss:</b>", "<b>Start:</b>",  "<b>End:</b>",  "<b>Duration:</b>" };
  static gchar tmp_buf[25];
  gdouble tmp_speed;

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

  tmp_speed = vik_track_get_max_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", tmp_speed );
  l_maxs = gtk_label_new ( tmp_buf );

  tmp_speed = vik_track_get_average_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", tmp_speed );
  l_avgs = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / ( tp_count - seg_count ) );
  l_avgd = gtk_label_new ( tmp_buf );

  if ( min_alt == VIK_DEFAULT_ALTITUDE )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt );
  l_elev = gtk_label_new ( tmp_buf );

  vik_track_get_total_elevation_gain(tr, &max_alt, &min_alt );
  if ( max_alt == VIK_DEFAULT_ALTITUDE )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
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

  if ( tr->trackpoints && VIK_TRACKPOINT(tr->trackpoints->data)->timestamp )
  {
    time_t t1, t2;
    t1 = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
    t2 = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;

    strncpy(tmp_buf, ctime(&t1), sizeof(tmp_buf));
    tmp_buf[strlen(tmp_buf)-1] = 0;
    l_begin = gtk_label_new(tmp_buf);

    strncpy(tmp_buf, ctime(&t2), sizeof(tmp_buf));
    tmp_buf[strlen(tmp_buf)-1] = 0;
    l_end = gtk_label_new(tmp_buf);

    g_snprintf(tmp_buf, sizeof(tmp_buf), "%d minutes", (int)(t2-t1)/60);
    l_dur = gtk_label_new(tmp_buf);
  } else {
    l_begin = gtk_label_new("No Data");
    l_end = gtk_label_new("No Data");
    l_dur = gtk_label_new("No Data");
  }
  gtk_box_pack_start (GTK_BOX(right_vbox), l_begin, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_end, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), l_dur, FALSE, FALSE, 0);

  hbox = gtk_hbox_new ( TRUE, 0 );
  gtk_box_pack_start (GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), right_vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 0);

  if ( profile )
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), profile, gtk_label_new("Elevation-distance"));

  if ( vtdiag )
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), vtdiag, gtk_label_new("Speed-time"));

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), graphs, FALSE, FALSE, 0);
  
  gtk_widget_show_all ( dialog );
  resp = gtk_dialog_run (GTK_DIALOG (dialog));
  if ( resp == GTK_RESPONSE_ACCEPT )
    vik_track_set_comment ( tr, gtk_entry_get_text ( GTK_ENTRY(e_cmt) ) );

  gtk_widget_destroy ( dialog );
  return resp;
}
