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

#include <math.h>
#include <gtk/gtk.h>
#include <time.h>
#include <string.h>
#include "coords.h"
#include "vikcoord.h"
#include "viktrack.h"
#include "viktrwlayer.h"
#include "viktrwlayer_propwin.h"
#include "vikwaypoint.h"
#include "dialog.h"
#include "globals.h"
#include "dems.h"

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

typedef struct _propsaved {
  gboolean saved;
  gint pos;
  GdkImage *img;
} PropSaved;

typedef struct _propwidgets {
  VikTrwLayer *vtl;
  VikTrack *tr;
  VikLayersPanel *vlp;
  gchar *track_name;
  GtkWidget *w_comment;
  GtkWidget *w_track_length;
  GtkWidget *w_tp_count;
  GtkWidget *w_segment_count;
  GtkWidget *w_duptp_count;
  GtkWidget *w_max_speed;
  GtkWidget *w_avg_speed;
  GtkWidget *w_avg_dist;
  GtkWidget *w_elev_range;
  GtkWidget *w_elev_gain;
  GtkWidget *w_time_start;
  GtkWidget *w_time_end;
  GtkWidget *w_time_dur;
  GtkWidget *w_dist_time;
  PropSaved elev_graph_saved_img;
  PropSaved speed_graph_saved_img;
} PropWidgets;

static PropWidgets *prop_widgets_new()
{
  PropWidgets *widgets = g_malloc0(sizeof(PropWidgets));

  return widgets;
}

static void prop_widgets_free(PropWidgets *widgets)
{

  if (widgets->elev_graph_saved_img.img)
    g_object_unref(widgets->elev_graph_saved_img.img);
  if (widgets->speed_graph_saved_img.img)
    g_object_unref(widgets->speed_graph_saved_img.img);
  g_free(widgets);
}

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
static void set_center_at_graph_position(gdouble event_x, gint img_width, VikLayersPanel *vlp, VikTrack *tr, gboolean time_base)
{
  VikTrackpoint *trackpoint;
  gdouble x = event_x - img_width / 2 + PROFILE_WIDTH / 2 - MARGIN / 2;
  if (x < 0)
    x = 0;
  if (x > PROFILE_WIDTH)
    x = PROFILE_WIDTH;

  if (time_base)
    trackpoint = vik_track_get_closest_tp_by_percentage_time ( tr, (gdouble) x / PROFILE_WIDTH, NULL );
  else
    trackpoint = vik_track_get_closest_tp_by_percentage_dist ( tr, (gdouble) x / PROFILE_WIDTH, NULL );

  if ( trackpoint ) {
    VikCoord coord = trackpoint->coord;
    vik_viewport_set_center_coord ( vik_layers_panel_get_viewport(vlp), &coord );
    vik_layers_panel_emit_update ( vlp );
  }
}

static void draw_graph_mark(GtkWidget *image, gdouble x, GdkGC *gc, PropSaved *saved_img)
{
  GdkPixmap *pix;
  const int saved_width = 5;

  gtk_image_get_pixmap(GTK_IMAGE(image), &pix, NULL);
  if (saved_img->saved) {
    gdk_draw_image(GDK_DRAWABLE(pix), gc, saved_img->img, 0, 0,
        saved_img->pos, 0, -1, -1);
    saved_img->saved = FALSE;
    gtk_widget_queue_draw_area(image, saved_img->pos, 0,
        saved_img->img->width, saved_img->img->height);
  }
  if ((x >= MARGIN) && (x < (PROFILE_WIDTH + MARGIN))) {
    if (saved_img->img)
      gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img,
          x - (saved_width/2), 0, 0, 0, saved_img->img->width, saved_img->img->height);
    else
      saved_img->img = gdk_drawable_copy_to_image(GDK_DRAWABLE(pix),
          saved_img->img, x - (saved_width/2), 0, 0, 0, saved_width, PROFILE_HEIGHT);
    saved_img->pos = x - (saved_width/2);
    saved_img->saved = TRUE;
    gdk_draw_line (GDK_DRAWABLE(pix), gc, x, 0, x, image->allocation.height);
    /* redraw the area which contains the line, saved_width is just convenient */
    gtk_widget_queue_draw_area(image, x - saved_width/2, 0, saved_width, PROFILE_HEIGHT);
  }
}

static void track_graph_click( GtkWidget *event_box, GdkEventButton *event, gpointer *pass_along, gboolean is_vt_graph )
{
  VikTrack *tr = pass_along[0];
  VikLayersPanel *vlp = pass_along[1];
  PropWidgets *widgets = pass_along[2];
  GList *child = gtk_container_get_children(GTK_CONTAINER(event_box));
  GtkWidget *image = GTK_WIDGET(child->data);
  GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(event_box));


  set_center_at_graph_position(event->x, event_box->allocation.width, vlp, tr, is_vt_graph);
  draw_graph_mark(image, event->x, window->style->black_gc,
      is_vt_graph ? &widgets->speed_graph_saved_img : &widgets->elev_graph_saved_img);
  g_list_free(child);

}

static gboolean track_profile_click( GtkWidget *event_box, GdkEventButton *event, gpointer *pass_along )
{
  track_graph_click(event_box, event, pass_along, FALSE);
  return TRUE;  /* don't call other (further) callbacks */
}

static gboolean track_vt_click( GtkWidget *event_box, GdkEventButton *event, gpointer *pass_along )
{
  track_graph_click(event_box, event, pass_along, TRUE);
  return TRUE;  /* don't call other (further) callbacks */
}

void track_profile_move( GtkWidget *image, GdkEventMotion *event, gpointer *pass_along )
{
  VikTrack *tr = pass_along[0];
  PropWidgets *widgets = pass_along[2];
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  gdouble x = mouse_x - image->allocation.width / 2 + PROFILE_WIDTH / 2 - MARGIN / 2;
  if (x < 0)
    x = 0;
  if (x > PROFILE_WIDTH)
    x = PROFILE_WIDTH;

  gdouble meters_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_dist ( tr, (gdouble) x / PROFILE_WIDTH, &meters_from_start );
  if (trackpoint && widgets->w_dist_time) {
    static gchar tmp_buf[20];
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m", meters_from_start);
    gtk_label_set_text(GTK_LABEL(widgets->w_dist_time), tmp_buf);
  }
}

void track_vt_move( GtkWidget *image, GdkEventMotion *event, gpointer *pass_along )
{
  VikTrack *tr = pass_along[0];
  PropWidgets *widgets = pass_along[2];
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  gdouble x = mouse_x - image->allocation.width / 2 + PROFILE_WIDTH / 2 - MARGIN / 2;
  if (x < 0)
    x = 0;
  if (x > PROFILE_WIDTH)
    x = PROFILE_WIDTH;

  time_t seconds_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_time ( tr, (gdouble) x / PROFILE_WIDTH, &seconds_from_start );
  if (trackpoint && widgets->w_dist_time) {
    static gchar tmp_buf[20];
    guint h, m, s;
    h = seconds_from_start/3600;
    m = (seconds_from_start - h*3600)/60;
    s = seconds_from_start - (3600*h) - (60*m);
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%02d:%02d:%02d", h, m, s);

    gtk_label_set_text(GTK_LABEL(widgets->w_dist_time), tmp_buf);
  }
}

static void draw_dem_alt_speed_dist(VikTrack *tr, GdkDrawable *pix, GdkGC *alt_gc, GdkGC *speed_gc, gdouble alt_offset, gdouble alt_diff, gint width, gint height, gint margin)
{
  GList *iter;
  gdouble dist = 0;
  gdouble max_speed = 0;
  gdouble total_length = vik_track_get_length_including_gaps(tr);

  for (iter = tr->trackpoints->next; iter; iter = iter->next) {
    if (!isnan(VIK_TRACKPOINT(iter->data)->speed))
      max_speed = MAX(max_speed, VIK_TRACKPOINT(iter->data)->speed);
  }
  max_speed = max_speed * 110 / 100;

  for (iter = tr->trackpoints->next; iter; iter = iter->next) {
    int x, y_alt, y_speed;
    gint16 elev = a_dems_get_elev_by_coord(&(VIK_TRACKPOINT(iter->data)->coord), VIK_DEM_INTERPOL_BEST);
    elev -= alt_offset;
    dist += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
      &(VIK_TRACKPOINT(iter->prev->data)->coord) );
    x = (width * dist)/total_length + margin;
    if ( elev != VIK_DEM_INVALID_ELEVATION ) {
      y_alt = height - (height * elev)/alt_diff;
      gdk_draw_rectangle(GDK_DRAWABLE(pix), alt_gc, TRUE, x-2, y_alt-2, 4, 4);
    }
    if (!isnan(VIK_TRACKPOINT(iter->data)->speed)) {
      y_speed = height - (height * VIK_TRACKPOINT(iter->data)->speed)/max_speed;
      gdk_draw_rectangle(GDK_DRAWABLE(pix), speed_gc, TRUE, x-2, y_speed-2, 4, 4);
    }
  }
}

GtkWidget *vik_trw_layer_create_profile ( GtkWidget *window, VikTrack *tr, gpointer vlp, PropWidgets *widgets, gdouble *min_alt, gdouble *max_alt)
{
  GdkPixmap *pix;
  GtkWidget *image;
  gdouble *altitudes = vik_track_make_elevation_map ( tr, PROFILE_WIDTH );
  gdouble mina, maxa;
  GtkWidget *eventbox;
  gpointer *pass_along;
  guint i;

  if ( altitudes == NULL ) {
    *min_alt = *max_alt = VIK_DEFAULT_ALTITUDE;
    return NULL;
  }

  pix = gdk_pixmap_new( window->window, PROFILE_WIDTH + MARGIN, PROFILE_HEIGHT, -1 );
  image = gtk_image_new_from_pixmap ( pix, NULL );

  GdkGC *no_alt_info = gdk_gc_new ( window->window );
  GdkGC *dem_alt_gc = gdk_gc_new ( window->window );
  GdkGC *gps_speed_gc = gdk_gc_new ( window->window );
  GdkColor color;

  gdk_color_parse ( "yellow", &color );
  gdk_gc_set_rgb_fg_color ( no_alt_info, &color);
  gdk_color_parse ( "green", &color );
  gdk_gc_set_rgb_fg_color ( dem_alt_gc, &color);
  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);


  minmax_alt(altitudes, min_alt, max_alt);
  mina = *min_alt;
  maxa = *max_alt * 110 / 100;
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

  draw_dem_alt_speed_dist(tr, GDK_DRAWABLE(pix), dem_alt_gc, gps_speed_gc, mina, maxa - mina, PROFILE_WIDTH, PROFILE_HEIGHT, MARGIN);

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->black_gc, FALSE, MARGIN, 0, PROFILE_WIDTH-1, PROFILE_HEIGHT-1);



  g_object_unref ( G_OBJECT(pix) );
  g_free ( altitudes );
  g_object_unref ( G_OBJECT(no_alt_info) );
  g_object_unref ( G_OBJECT(dem_alt_gc) );
  g_object_unref ( G_OBJECT(gps_speed_gc) );

  pass_along = g_malloc ( sizeof(gpointer) * 3 ); /* FIXME: mem leak -- never be freed */
  pass_along[0] = tr;
  pass_along[1] = vlp;
  pass_along[2] = widgets;

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_profile_click), pass_along );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_profile_move), pass_along );
  g_signal_connect_swapped ( G_OBJECT(eventbox), "destroy", G_CALLBACK(g_free), pass_along );
  gtk_container_add ( GTK_CONTAINER(eventbox), image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK
                         | GDK_POINTER_MOTION_MASK
                         | GDK_POINTER_MOTION_HINT_MASK);

  return eventbox;
}

#define METRIC 1
#ifdef METRIC 
#define MTOK(v) ( (v)*3.6) /* m/s to km/h */
#else
#define MTOK(v) ( (v)*3600.0/1000.0 * 0.6214) /* m/s to mph - we'll handle this globally eventually but for now ...*/
#endif

GtkWidget *vik_trw_layer_create_vtdiag ( GtkWidget *window, VikTrack *tr, gpointer vlp, PropWidgets *widgets)
{
  GdkPixmap *pix;
  GtkWidget *image;
  gdouble mins, maxs;
  guint i;
  GtkWidget *eventbox;
  gpointer *pass_along;

  pass_along = g_malloc ( sizeof(gpointer) * 3 ); /* FIXME: mem leak -- never be freed */
  pass_along[0] = tr;
  pass_along[1] = vlp;
  pass_along[2] = widgets;

  gdouble *speeds = vik_track_make_speed_map ( tr, PROFILE_WIDTH );
  if ( speeds == NULL )
    return NULL;

  pix = gdk_pixmap_new( window->window, PROFILE_WIDTH + MARGIN, PROFILE_HEIGHT, -1 );
  image = gtk_image_new_from_pixmap ( pix, NULL );

  for (i=0; i<PROFILE_WIDTH; i++) {
    speeds[i] = MTOK(speeds[i]);
  }

  minmax_alt(speeds, &mins, &maxs);
  mins = 0; /* splines sometimes give negative speeds */
  maxs = maxs * 110 / 100;
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


  GdkGC *gps_speed_gc = gdk_gc_new ( window->window );
  GdkColor color;

  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

  time_t beg_time = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  time_t dur =  VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp - beg_time;
  GList *iter;
  for (iter = tr->trackpoints; iter; iter = iter->next) {
    gdouble gps_speed = VIK_TRACKPOINT(iter->data)->speed;
    if (isnan(gps_speed))
        continue;
    int x = MARGIN + PROFILE_WIDTH * (VIK_TRACKPOINT(iter->data)->timestamp - beg_time) / dur;
    int y = PROFILE_HEIGHT - PROFILE_HEIGHT*(MTOK(gps_speed) - mins)/(maxs - mins);
    gdk_draw_rectangle(GDK_DRAWABLE(pix), gps_speed_gc, TRUE, x-2, y-2, 4, 4);
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->black_gc, FALSE, MARGIN, 0, PROFILE_WIDTH-1, PROFILE_HEIGHT-1);

  g_object_unref ( G_OBJECT(pix) );
  g_object_unref ( G_OBJECT(gps_speed_gc) );
  g_free ( speeds );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_vt_click), pass_along );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_vt_move), pass_along );
  g_signal_connect_swapped ( G_OBJECT(eventbox), "destroy", G_CALLBACK(g_free), pass_along );
  gtk_container_add ( GTK_CONTAINER(eventbox), image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK
                         | GDK_POINTER_MOTION_MASK
                         | GDK_POINTER_MOTION_HINT_MASK);

  return eventbox;
}
#undef MARGIN
#undef LINES

/* Notes: first and third arguments are swapped around compared to the manuals */
//static void propwin_response_cb( GtkDialog *widgets, gint resp, PropWidgets *widgets)
static void propwin_response_cb( gpointer p_widgets, gint resp, gpointer p_dialog)
{
  PropWidgets *widgets = (PropWidgets *) p_widgets;
  GtkDialog * dialog = p_dialog;

  VikTrack *tr = widgets->tr;
  VikTrwLayer *vtl = widgets->vtl;

  /* FIXME: check and make sure the track still exists before doing anything to it */
  /* Note: destroying diaglog (eg, parent window exit) won't give "response" */
  switch (resp) {
    case GTK_RESPONSE_DELETE_EVENT: /* received delete event (not from buttons) */
    case GTK_RESPONSE_REJECT:
      break;
    case GTK_RESPONSE_ACCEPT:
      vik_track_set_comment(tr, gtk_entry_get_text(GTK_ENTRY(widgets->w_comment)));
      break;
    case VIK_TRW_LAYER_PROPWIN_REVERSE:
      vik_track_reverse(tr);
      vik_layer_emit_update ( VIK_LAYER(vtl) );
      break;
    case VIK_TRW_LAYER_PROPWIN_DEL_DUP:
      vik_track_remove_dup_points(tr);
      /* above operation could have deleted current_tp or last_tp */
      trw_layer_cancel_tps_of_track ( vtl, widgets->track_name );
      vik_layer_emit_update ( VIK_LAYER(vtl) );
      break;
    case VIK_TRW_LAYER_PROPWIN_SPLIT:
      {
        /* get new tracks, add them, resolve naming conflicts (free if cancel), and delete old. old can still exist on clipboard. */
        guint ntracks;
        VikTrack **tracks = vik_track_split_into_segments(tr, &ntracks);
        gchar *new_tr_name;
        guint i;
        for ( i = 0; i < ntracks; i++ )
        {
          g_assert ( tracks[i] );
          new_tr_name = g_strdup_printf("%s #%d", widgets->track_name, i+1);
          /* if ( (wp_exists) && (! overwrite) ) */
          /* don't need to upper case new_tr_name because old tr name was uppercase */
          if ( vik_trw_layer_get_track(vtl, new_tr_name ) && 
             ( ! a_dialog_overwrite ( VIK_GTK_WINDOW_FROM_LAYER(vtl), "The track \"%s\" exists, do you wish to overwrite it?", new_tr_name ) ) )
          {
            gchar *new_new_tr_name = a_dialog_new_track ( VIK_GTK_WINDOW_FROM_LAYER(vtl), vik_trw_layer_get_tracks(vtl) );
            g_free ( new_tr_name );
            if (new_new_tr_name)
              new_tr_name = new_new_tr_name;
            else
            {
              new_tr_name = NULL;
              vik_track_free ( tracks[i] );
            }
          }
          if ( new_tr_name )
            vik_trw_layer_add_track ( vtl, new_tr_name, tracks[i] );
        }
        if ( tracks )
        {
          g_free ( tracks );
          /* Don't let track destroy this dialog */
          vik_track_clear_property_dialog(tr);
          vik_trw_layer_delete_track ( vtl, widgets->track_name );
          vik_layer_emit_update ( VIK_LAYER(vtl) ); /* chase thru the hoops */
        }
      }
      break;
    default:
      fprintf(stderr, "DEBUG: unknown response\n");
      return;
  }

  /* Keep same behaviour for now: destroy dialog if click on any button */
  prop_widgets_free(widgets);
  vik_track_clear_property_dialog(tr);
  gtk_widget_destroy ( GTK_WIDGET(dialog) );
}

void vik_trw_layer_propwin_run ( GtkWindow *parent, VikTrwLayer *vtl, VikTrack *tr, gpointer vlp, gchar *track_name )
{
  /* FIXME: free widgets when destroy signal received */
  PropWidgets *widgets = prop_widgets_new();
  widgets->vtl = vtl;
  widgets->tr = tr;
  widgets->vlp = vlp;
  widgets->track_name = track_name;
  gchar *title = g_strdup_printf("%s - Track Properties", track_name);
  GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
                         parent,
                         GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                         "Split Segments", VIK_TRW_LAYER_PROPWIN_SPLIT,
                         "Reverse",        VIK_TRW_LAYER_PROPWIN_REVERSE,
                         "Delete Dupl.",   VIK_TRW_LAYER_PROPWIN_DEL_DUP,
                         GTK_STOCK_OK,     GTK_RESPONSE_ACCEPT,
                         NULL);
  g_free(title);
  g_signal_connect_swapped(dialog, "response", G_CALLBACK(propwin_response_cb), widgets);
  //fprintf(stderr, "DEBUG: dialog=0x%p\n", dialog);
  GtkTable *table;
  gdouble tr_len;
  guint32 tp_count, seg_count;

  gdouble min_alt, max_alt;
  GtkWidget *profile = vik_trw_layer_create_profile(GTK_WIDGET(parent),tr, vlp, widgets, &min_alt,&max_alt);
  GtkWidget *vtdiag = vik_trw_layer_create_vtdiag(GTK_WIDGET(parent), tr, vlp, widgets);
  GtkWidget *graphs = gtk_notebook_new();

  GtkWidget *content[20];
  int cnt;
  int i;

  static gchar *label_texts[] = { "<b>Comment:</b>", "<b>Track Length:</b>", "<b>Trackpoints:</b>", "<b>Segments:</b>", "<b>Duplicate Points:</b>", "<b>Max Speed:</b>", "<b>Avg. Speed:</b>", "<b>Avg. Dist. Between TPs:</b>", "<b>Elevation Range:</b>", "<b>Total Elevation Gain/Loss:</b>", "<b>Start:</b>",  "<b>End:</b>",  "<b>Duration:</b>", "<b>Track Distance/Time:</b>" };
  static gchar tmp_buf[50];
  gdouble tmp_speed;

  cnt = 0;
  widgets->w_comment = gtk_entry_new ();
  if ( tr->comment )
    gtk_entry_set_text ( GTK_ENTRY(widgets->w_comment), tr->comment );
  g_signal_connect_swapped ( widgets->w_comment, "activate", G_CALLBACK(a_dialog_response_accept), GTK_DIALOG(dialog) );
  content[cnt++] = widgets->w_comment;

  tr_len = vik_track_get_length(tr);
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", tr_len );
  widgets->w_track_length = content[cnt++] = gtk_label_new ( tmp_buf );

  tp_count = vik_track_get_tp_count(tr);
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%u", tp_count );
  widgets->w_tp_count = content[cnt++] = gtk_label_new ( tmp_buf );

  seg_count = vik_track_get_segment_count(tr) ;
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count );
  widgets->w_segment_count = content[cnt++] = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%lu", vik_track_get_dup_point_count(tr) );
  widgets->w_duptp_count = content[cnt++] = gtk_label_new ( tmp_buf );

  tmp_speed = vik_track_get_max_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s   (%.0f km/h)", tmp_speed, MTOK(tmp_speed) );
  widgets->w_max_speed = content[cnt++] = gtk_label_new ( tmp_buf );

  tmp_speed = vik_track_get_average_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s   (%.0f km/h)", tmp_speed, MTOK(tmp_speed) );
  widgets->w_avg_speed = content[cnt++] = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / ( tp_count - seg_count ) );
  widgets->w_avg_dist = content[cnt++] = gtk_label_new ( tmp_buf );

  if ( min_alt == VIK_DEFAULT_ALTITUDE )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt );
  widgets->w_elev_range = content[cnt++] = gtk_label_new ( tmp_buf );

  vik_track_get_total_elevation_gain(tr, &max_alt, &min_alt );
  if ( min_alt == VIK_DEFAULT_ALTITUDE )
    g_snprintf(tmp_buf, sizeof(tmp_buf), "No Data");
  else
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m / %.0f m", max_alt, min_alt );
  widgets->w_elev_gain = content[cnt++] = gtk_label_new ( tmp_buf );

#if 0
#define PACK(w) gtk_box_pack_start (GTK_BOX(right_vbox), w, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(right_vbox), e_cmt, FALSE, FALSE, 0); 
  PACK(l_len);
  PACK(l_tps);
  PACK(l_segs);
  PACK(l_dups);
  PACK(l_maxs);
  PACK(l_avgs);
  PACK(l_avgd);
  PACK(l_elev);
  PACK(l_galo);
#undef PACK;
#endif

  if ( tr->trackpoints && VIK_TRACKPOINT(tr->trackpoints->data)->timestamp )
  {
    time_t t1, t2;
    t1 = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
    t2 = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;

    strncpy(tmp_buf, ctime(&t1), sizeof(tmp_buf));
    tmp_buf[sizeof(tmp_buf)-1] = 0;
    g_strchomp(tmp_buf);
    widgets->w_time_start = content[cnt++] = gtk_label_new(tmp_buf);

    strncpy(tmp_buf, ctime(&t2), sizeof(tmp_buf));
    tmp_buf[sizeof(tmp_buf)-1] = 0;
    g_strchomp(tmp_buf);
    widgets->w_time_end = content[cnt++] = gtk_label_new(tmp_buf);

    g_snprintf(tmp_buf, sizeof(tmp_buf), "%d minutes", (int)(t2-t1)/60);
    widgets->w_time_dur = content[cnt++] = gtk_label_new(tmp_buf);
  } else {
    widgets->w_time_start = content[cnt++] = gtk_label_new("No Data");
    widgets->w_time_end = content[cnt++] = gtk_label_new("No Data");
    widgets->w_time_dur = content[cnt++] = gtk_label_new("No Data");
  }
  widgets->w_dist_time = content[cnt++] = gtk_label_new("No Data");

  table = GTK_TABLE(gtk_table_new (cnt, 2, FALSE));
  gtk_table_set_col_spacing (table, 0, 10);
  for (i=0; i<cnt; i++) {
    GtkWidget *label;

    label = gtk_label_new(NULL);
    gtk_misc_set_alignment ( GTK_MISC(label), 1, 0 );
    gtk_label_set_markup ( GTK_LABEL(label), label_texts[i] );
    gtk_table_attach_defaults ( table, label, 0, 1, i, i+1 );
    if (GTK_IS_MISC(content[i])) {
      gtk_misc_set_alignment ( GTK_MISC(content[i]), 0, 0 );
    }
    gtk_table_attach_defaults ( table, content[i], 1, 2, i, i+1 );
  }

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(table), FALSE, FALSE, 0);

  if ( profile )
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), profile, gtk_label_new("Elevation-distance"));

  if ( vtdiag )
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), vtdiag, gtk_label_new("Speed-time"));

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), graphs, FALSE, FALSE, 0);

  if (seg_count <= 1)
    gtk_dialog_set_response_sensitive(dialog, VIK_TRW_LAYER_PROPWIN_SPLIT, FALSE);
  if (vik_track_get_dup_point_count(tr) <= 0)
    gtk_dialog_set_response_sensitive(dialog, VIK_TRW_LAYER_PROPWIN_DEL_DUP, FALSE);

  vik_track_set_property_dialog(tr, dialog);
  gtk_widget_show_all ( dialog );
}
