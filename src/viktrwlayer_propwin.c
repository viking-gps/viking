/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
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

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <time.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
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

#define PROPWIN_PROFILE_WIDTH 600
#define PROPWIN_PROFILE_HEIGHT 300

#define PROPWIN_LABEL_FONT "Sans 7"

#define MIN_ALT_DIFF 100.0
#define MIN_SPEED_DIFF 5.0

typedef struct _propsaved {
  gboolean saved;
  gint pos;
  GdkImage *img;
} PropSaved;

typedef struct _propwidgets {
  gboolean  configure_dialog;
  VikTrwLayer *vtl;
  VikTrack *tr;
  VikLayersPanel *vlp;
  gchar *track_name;
  gint      profile_width;
  gint      profile_height;
  gint      profile_width_old;
  gint      profile_height_old;
  gint      profile_width_offset;
  gint      profile_height_offset;
  GtkWidget *dialog;
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
  GtkWidget *w_cur_dist; /*< Current distance */
  GtkWidget *w_cur_elevation;
  GtkWidget *w_cur_time; /*< Current time */
  GtkWidget *w_cur_speed;
  GtkWidget *w_show_dem;
  GtkWidget *w_show_alt_gps_speed;
  GtkWidget *w_show_gps_speed;
  gdouble   track_length;
  PropSaved elev_graph_saved_img;
  PropSaved speed_graph_saved_img;
  GtkWidget *elev_box;
  GtkWidget *speed_box;
  gdouble   *altitudes;
  gdouble   *speeds;
  VikTrackpoint *marker_tp;
  gboolean  is_marker_drawn;
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
  if (widgets->altitudes)
    g_free(widgets->altitudes);
  if (widgets->speeds)
    g_free(widgets->speeds);
  g_free(widgets);
}

static void minmax_array(const gdouble *array, gdouble *min, gdouble *max, gboolean NO_ALT_TEST, gint PROFILE_WIDTH)
{
  *max = -1000;
  *min = 20000;
  guint i;
  for ( i=0; i < PROFILE_WIDTH; i++ ) {
    if ( NO_ALT_TEST || (array[i] != VIK_DEFAULT_ALTITUDE) ) {
      if ( array[i] > *max )
        *max = array[i];
      if ( array[i] < *min )
        *min = array[i];
    }
  }
}

#define MARGIN 70
#define LINES 5
static VikTrackpoint *set_center_at_graph_position(gdouble event_x,
						   gint img_width,
						   VikTrwLayer *vtl,
						   VikLayersPanel *vlp,
						   VikViewport *vvp,
						   VikTrack *tr,
						   gboolean time_base,
						   gint PROFILE_WIDTH)
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
    if ( vlp ) {
      vik_viewport_set_center_coord ( vik_layers_panel_get_viewport(vlp), &coord );
      vik_layers_panel_emit_update ( vlp );
    }
    else {
      /* since vlp not set, vvp should be valid instead! */
      if ( vvp )
	vik_viewport_set_center_coord ( vvp, &coord );
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    }
  }
  return trackpoint;
}

/**
 * Returns whether the marker was drawn or not
 */
static void save_image_and_draw_graph_mark (GtkWidget *image,
					    gdouble event_x,
					    gint img_width,
					    GdkGC *gc,
					    PropSaved *saved_img,
					    gint PROFILE_WIDTH,
					    gint PROFILE_HEIGHT,
					    gboolean *marker_drawn)
{
  GdkPixmap *pix = NULL;
  /* the pixmap = margin + graph area */
  gdouble x = event_x - img_width/2 + PROFILE_WIDTH/2 + MARGIN/2;

  // fprintf(stderr, "event_x=%f img_width=%d x=%f\n", event_x, img_width, x);

  gtk_image_get_pixmap(GTK_IMAGE(image), &pix, NULL);
  /* Restore previously saved image */
  if (saved_img->saved) {
    gdk_draw_image(GDK_DRAWABLE(pix), gc, saved_img->img, 0, 0,
        saved_img->pos, 0, -1, -1);
    saved_img->saved = FALSE;
    gtk_widget_queue_draw_area(image,
        saved_img->pos + img_width/2 - PROFILE_WIDTH/2 - MARGIN/2, 0,
        saved_img->img->width, saved_img->img->height);
  }
  if ((x >= MARGIN) && (x < (PROFILE_WIDTH + MARGIN))) {
    /* Save part of the image */
    if (saved_img->img)
      gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img,
          x, 0, 0, 0, saved_img->img->width, saved_img->img->height);
    else
      saved_img->img = gdk_drawable_copy_to_image(GDK_DRAWABLE(pix),
          saved_img->img, x, 0, 0, 0, 1, PROFILE_HEIGHT);
    saved_img->pos = x;
    saved_img->saved = TRUE;
    gdk_draw_line (GDK_DRAWABLE(pix), gc, x, 0, x, image->allocation.height);
    /* redraw the area which contains the line, saved_width is just convenient */
    gtk_widget_queue_draw_area(image, event_x, 0, 1, PROFILE_HEIGHT);
    *marker_drawn = TRUE;
  }
  else
    *marker_drawn = FALSE;
}

/**
 * Return the percentage of how far a trackpoint is a long a track via the time method
 */
static gdouble tp_percentage_by_time ( VikTrack *tr, VikTrackpoint *trackpoint )
{
  gdouble pc = NAN;
  if (trackpoint == NULL)
    return pc;
  time_t t_start, t_end, t_total;
  t_start = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  t_end = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;
  t_total = t_end - t_start;
  pc = (gdouble)(trackpoint->timestamp - t_start)/t_total;
  return pc;
}

/**
 * Return the percentage of how far a trackpoint is a long a track via the distance method
 */
static gdouble tp_percentage_by_distance ( VikTrack *tr, VikTrackpoint *trackpoint, gdouble track_length )
{
  gdouble pc = NAN;
  if (trackpoint == NULL)
    return pc;
  gdouble dist = 0.0;
  GList *iter;
  for (iter = tr->trackpoints->next; iter != NULL; iter = iter->next) {
    dist += vik_coord_diff(&(VIK_TRACKPOINT(iter->data)->coord),
			   &(VIK_TRACKPOINT(iter->prev->data)->coord));
    /* Assuming trackpoint is not a copy */
    if (trackpoint == VIK_TRACKPOINT(iter->data))
      break;
  }
  if (iter != NULL)
    pc = dist/track_length;
  return pc;
}

static void track_graph_click( GtkWidget *event_box, GdkEventButton *event, gpointer *pass_along, gboolean is_vt_graph )
{
  VikTrack *tr = pass_along[0];
  VikLayersPanel *vlp = pass_along[1];
  VikViewport *vvp = pass_along[2];
  PropWidgets *widgets = pass_along[3];
  GList *child = gtk_container_get_children(GTK_CONTAINER(event_box));
  GtkWidget *image = GTK_WIDGET(child->data);
  GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(event_box));

  VikTrackpoint *trackpoint = set_center_at_graph_position(event->x, event_box->allocation.width, widgets->vtl, vlp, vvp, tr, is_vt_graph, widgets->profile_width);
  save_image_and_draw_graph_mark(image,
				 event->x,
				 event_box->allocation.width,
				 window->style->black_gc,
				 is_vt_graph ? &widgets->speed_graph_saved_img : &widgets->elev_graph_saved_img,
				 widgets->profile_width,
				 widgets->profile_height,
				 &widgets->is_marker_drawn);
  g_list_free(child);
  widgets->marker_tp = trackpoint;
  gtk_dialog_set_response_sensitive(GTK_DIALOG(widgets->dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, widgets->is_marker_drawn);

  /* draw on the other graph */
  if (trackpoint == NULL || widgets->elev_box == NULL || widgets->speed_box == NULL)
    /* This test assumes we have only 2 graphs */
    return;

  gdouble pc = NAN;
  gdouble x2;
  GList *other_child = gtk_container_get_children(GTK_CONTAINER(
                         is_vt_graph ? widgets->elev_box : widgets->speed_box));
  GtkWidget *other_image = GTK_WIDGET(other_child->data);
  if (is_vt_graph) {
    pc = tp_percentage_by_distance ( tr, trackpoint, widgets->track_length );
  } else {
    pc = tp_percentage_by_time ( tr, trackpoint );
  }
  if (!isnan(pc)) {
    x2 = pc * widgets->profile_width + MARGIN + (event_box->allocation.width/2 - widgets->profile_width/2 - MARGIN/2);
    save_image_and_draw_graph_mark(other_image,
				   x2,
				   event_box->allocation.width,
				   window->style->black_gc,
				   is_vt_graph ? &widgets->elev_graph_saved_img : &widgets->speed_graph_saved_img,
				   widgets->profile_width,
				   widgets->profile_height,
				   &widgets->is_marker_drawn);
  }

  g_list_free(other_child);

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
  PropWidgets *widgets = pass_along[3];
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  gdouble x = mouse_x - image->allocation.width / 2 + widgets->profile_width / 2 - MARGIN / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  gdouble meters_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_dist ( tr, (gdouble) x / widgets->profile_width, &meters_from_start );
  if (trackpoint && widgets->w_cur_dist) {
    static gchar tmp_buf[20];
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", meters_from_start/1000.0);
      break;
    case VIK_UNITS_DISTANCE_MILES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", VIK_METERS_TO_MILES(meters_from_start) );
      break;
    default:
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_dist), tmp_buf);
  }

  // Show track elevation for this position - to the nearest whole number
  if (trackpoint && widgets->w_cur_elevation) {
    static gchar tmp_buf[20];
    if (a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET)
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%d ft", (int)VIK_METERS_TO_FEET(trackpoint->altitude));
    else
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%d m", (int)trackpoint->altitude);
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_elevation), tmp_buf);
  }
}

void track_vt_move( GtkWidget *image, GdkEventMotion *event, gpointer *pass_along )
{
  VikTrack *tr = pass_along[0];
  PropWidgets *widgets = pass_along[3];
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  gdouble x = mouse_x - image->allocation.width / 2 + widgets->profile_width / 2 - MARGIN / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  time_t seconds_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_time ( tr, (gdouble) x / widgets->profile_width, &seconds_from_start );
  if (trackpoint && widgets->w_cur_time) {
    static gchar tmp_buf[20];
    guint h, m, s;
    h = seconds_from_start/3600;
    m = (seconds_from_start - h*3600)/60;
    s = seconds_from_start - (3600*h) - (60*m);
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%02d:%02d:%02d", h, m, s);

    gtk_label_set_text(GTK_LABEL(widgets->w_cur_time), tmp_buf);
  }

  // Show track speed for this position
  if (trackpoint && widgets->w_cur_speed) {
    static gchar tmp_buf[20];
    // Even if GPS speed available (trackpoint->speed), the text will correspond to the speed map shown
    gint ix = (gint)x;
    // Ensure ix is inbounds
    if (ix == widgets->profile_width)
      ix--;

    vik_units_speed_t speed_units = a_vik_get_units_speed ();
    switch (speed_units) {
    case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f kph"), VIK_MPS_TO_KPH(widgets->speeds[ix]));
      break;
    case VIK_UNITS_SPEED_MILES_PER_HOUR:
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f mph"), VIK_MPS_TO_MPH(widgets->speeds[ix]));
      break;
    case VIK_UNITS_SPEED_KNOTS:
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f knots"), VIK_MPS_TO_KNOTS(widgets->speeds[ix]));
      break;
    default:
      // VIK_UNITS_SPEED_METRES_PER_SECOND:
      // No need to convert as already in m/s
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f m/s"), widgets->speeds[ix]);
      break;
    }
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_speed), tmp_buf);
  }
}

/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 *   (which is the elevations graph)
 */
static void draw_dem_alt_speed_dist(VikTrack *tr,
				    GdkDrawable *pix,
				    GdkGC *alt_gc,
				    GdkGC *speed_gc,
				    gdouble alt_offset,
				    gdouble alt_diff,
				    gint width,
				    gint height,
				    gint margin,
				    gboolean do_dem,
				    gboolean do_speed)
{
  GList *iter;
  gdouble max_speed = 0;
  gdouble total_length = vik_track_get_length_including_gaps(tr);

  if (do_speed) {
    for (iter = tr->trackpoints->next; iter; iter = iter->next) {
      if (!isnan(VIK_TRACKPOINT(iter->data)->speed))
	max_speed = MAX(max_speed, VIK_TRACKPOINT(iter->data)->speed);
    }
    max_speed = max_speed * 110 / 100;
  }

  gdouble dist = 0;
  for (iter = tr->trackpoints->next; iter; iter = iter->next) {
    int x;
    dist += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
			     &(VIK_TRACKPOINT(iter->prev->data)->coord) );
    x = (width * dist)/total_length + margin;
    if (do_dem) {
      gint16 elev = a_dems_get_elev_by_coord(&(VIK_TRACKPOINT(iter->data)->coord), VIK_DEM_INTERPOL_BEST);
      elev -= alt_offset;
      if ( elev != VIK_DEM_INVALID_ELEVATION ) {
	int y_alt = height - ((height * elev)/alt_diff);
	gdk_draw_rectangle(GDK_DRAWABLE(pix), alt_gc, TRUE, x-2, y_alt-2, 4, 4);
      }
    }
    if (do_speed) {
      // This is just a speed indicator - no actual values can be inferred by user
      if (!isnan(VIK_TRACKPOINT(iter->data)->speed)) {
	int y_speed = height - (height * VIK_TRACKPOINT(iter->data)->speed)/max_speed;
	gdk_draw_rectangle(GDK_DRAWABLE(pix), speed_gc, TRUE, x-2, y_speed-2, 4, 4);
      }
    }
  }
}

/**
 * Draw just the height profile image
 */
static void draw_elevations (GtkWidget *image, VikTrack *tr, PropWidgets *widgets )
{
  GtkWidget *window;
  GdkPixmap *pix;
  gdouble mina, maxa;
  guint i;

  GdkGC *no_alt_info;
  GdkGC *dem_alt_gc;
  GdkGC *gps_speed_gc;

  GdkColor color;

  // Free previous allocation
  if ( widgets->altitudes )
    g_free ( widgets->altitudes );

  widgets->altitudes = vik_track_make_elevation_map ( tr, widgets->profile_width );

  if ( widgets->altitudes == NULL )
    return;

  minmax_array(widgets->altitudes, &mina, &maxa, TRUE, widgets->profile_width);
  maxa = maxa + ((maxa - mina) * 0.25); // Make visible window a bit bigger than highest point

  window = gtk_widget_get_toplevel (widgets->elev_box);

  pix = gdk_pixmap_new( window->window, widgets->profile_width + MARGIN, widgets->profile_height, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  no_alt_info = gdk_gc_new ( window->window );
  gdk_color_parse ( "yellow", &color );
  gdk_gc_set_rgb_fg_color ( no_alt_info, &color);

  dem_alt_gc = gdk_gc_new ( window->window );
  gdk_color_parse ( "green", &color );
  gdk_gc_set_rgb_fg_color ( dem_alt_gc, &color);

  gps_speed_gc = gdk_gc_new ( window->window );
  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

  /* clear the image */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->bg_gc[0], 
		     TRUE, 0, 0, MARGIN, widgets->profile_height);
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->mid_gc[0], 
		     TRUE, MARGIN, 0, widgets->profile_width, widgets->profile_height);
  
  /* draw grid */
  vik_units_height_t height_units = a_vik_get_units_height ();
  for (i=0; i<=LINES; i++) {
    PangoFontDescription *pfd;
    PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(image), NULL);
    gchar s[32];
    int w, h;

    pango_layout_set_alignment (pl, PANGO_ALIGN_RIGHT);
    pfd = pango_font_description_from_string (PROPWIN_LABEL_FONT);
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);
    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      sprintf(s, "%8dm", (int)(mina + (LINES-i)*(maxa-mina)/LINES));
      break;
    case VIK_UNITS_HEIGHT_FEET:
      sprintf(s, "%8dft", (int)VIK_METERS_TO_FEET(mina + (LINES-i)*(maxa-mina)/LINES));
      break;
    default:
      sprintf(s, "--");
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }
    pango_layout_set_text(pl, s, -1);
    pango_layout_get_pixel_size (pl, &w, &h);
    gdk_draw_layout(GDK_DRAWABLE(pix), window->style->fg_gc[0], MARGIN-w-3, 
		    CLAMP((int)i*widgets->profile_height/LINES - h/2, 0, widgets->profile_height-h), pl);

    gdk_draw_line (GDK_DRAWABLE(pix), window->style->dark_gc[0], 
		   MARGIN, widgets->profile_height/LINES * i, MARGIN + widgets->profile_width, widgets->profile_height/LINES * i);
    g_object_unref ( G_OBJECT ( pl ) );
    pl = NULL;
  }

  /* draw elevations */
  for ( i = 0; i < widgets->profile_width; i++ )
    if ( widgets->altitudes[i] == VIK_DEFAULT_ALTITUDE )
      gdk_draw_line ( GDK_DRAWABLE(pix), no_alt_info, 
		      i + MARGIN, 0, i + MARGIN, widgets->profile_height );
    else 
      gdk_draw_line ( GDK_DRAWABLE(pix), window->style->dark_gc[3], 
		      i + MARGIN, widgets->profile_height, i + MARGIN, widgets->profile_height-widgets->profile_height*(widgets->altitudes[i]-mina)/(maxa-mina) );

  draw_dem_alt_speed_dist(tr,
			  GDK_DRAWABLE(pix),
			  dem_alt_gc,
			  gps_speed_gc,
			  mina,
			  maxa - mina,
			  widgets->profile_width,
			  widgets->profile_height,
			  MARGIN,
			  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widgets->w_show_dem)),
			  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed)));

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->black_gc, FALSE, MARGIN, 0, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );
  g_object_unref ( G_OBJECT(no_alt_info) );
  g_object_unref ( G_OBJECT(dem_alt_gc) );
  g_object_unref ( G_OBJECT(gps_speed_gc) );

}

/**
 * Draw just the speed (velocity)/time image
 */
static void draw_vt ( GtkWidget *image, VikTrack *tr, PropWidgets *widgets)
{
  GtkWidget *window;
  GdkPixmap *pix;
  gdouble mins, maxs;
  guint i;

  // Free previous allocation
  if ( widgets->speeds )
    g_free ( widgets->speeds );

  widgets->speeds = vik_track_make_speed_map ( tr, widgets->profile_width );
  if ( widgets->speeds == NULL )
    return;

  GdkGC *gps_speed_gc;
  GdkColor color;

  window = gtk_widget_get_toplevel (widgets->speed_box);

  pix = gdk_pixmap_new( window->window, widgets->profile_width + MARGIN, widgets->profile_height, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  gps_speed_gc = gdk_gc_new ( window->window );
  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

  minmax_array(widgets->speeds, &mins, &maxs, FALSE, widgets->profile_width);
  if (mins < 0.0)
    mins = 0; /* splines sometimes give negative speeds */
  maxs = maxs + ((maxs - mins) * 0.1);
  if  (maxs-mins < MIN_SPEED_DIFF) {
    maxs = mins + MIN_SPEED_DIFF;
  }
  
  /* clear the image */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->bg_gc[0], 
		     TRUE, 0, 0, MARGIN, widgets->profile_height);
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->mid_gc[0], 
		     TRUE, MARGIN, 0, widgets->profile_width, widgets->profile_height);

  /* draw grid */
  for (i=0; i<=LINES; i++) {
    PangoFontDescription *pfd;
    PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(image), NULL);
    gchar s[32];
    int w, h;

    pango_layout_set_alignment (pl, PANGO_ALIGN_RIGHT);
    pfd = pango_font_description_from_string (PROPWIN_LABEL_FONT);
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);
    vik_units_speed_t speed_units = a_vik_get_units_speed ();
    switch (speed_units) {
    case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
      sprintf(s, "%6.1fkm/h", VIK_MPS_TO_KPH(mins + (LINES-i)*(maxs-mins)/LINES));
      break;
    case VIK_UNITS_SPEED_MILES_PER_HOUR:
      sprintf(s, "%6.1fmph", VIK_MPS_TO_MPH(mins + (LINES-i)*(maxs-mins)/LINES));
      break;
    case VIK_UNITS_SPEED_METRES_PER_SECOND:
      sprintf(s, "%8dm/s", (int)(mins + (LINES-i)*(maxs-mins)/LINES));
      break;
    case VIK_UNITS_SPEED_KNOTS:
      sprintf(s, "%6.1fknots", VIK_MPS_TO_KNOTS(mins + (LINES-i)*(maxs-mins)/LINES));
      break;
    default:
      sprintf(s, "--");
      g_critical("Houston, we've had a problem. speed=%d", speed_units);
    }

    pango_layout_set_text(pl, s, -1);
    pango_layout_get_pixel_size (pl, &w, &h);
    gdk_draw_layout(GDK_DRAWABLE(pix), window->style->fg_gc[0], MARGIN-w-3, 
		    CLAMP((int)i*widgets->profile_height/LINES - h/2, 0, widgets->profile_height-h), pl);

    gdk_draw_line (GDK_DRAWABLE(pix), window->style->dark_gc[0], 
		   MARGIN, widgets->profile_height/LINES * i, MARGIN + widgets->profile_width, widgets->profile_height/LINES * i);
    g_object_unref ( G_OBJECT ( pl ) );
    pl = NULL;
  }
  

  /* draw speeds */
  for ( i = 0; i < widgets->profile_width; i++ )
      gdk_draw_line ( GDK_DRAWABLE(pix), window->style->dark_gc[3], 
		      i + MARGIN, widgets->profile_height, i + MARGIN, widgets->profile_height-widgets->profile_height*(widgets->speeds[i]-mins)/(maxs-mins) );


  time_t beg_time = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  time_t dur =  VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp - beg_time;

  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_gps_speed)) ) {
    GList *iter;
    for (iter = tr->trackpoints; iter; iter = iter->next) {
      gdouble gps_speed = VIK_TRACKPOINT(iter->data)->speed;
      if (isnan(gps_speed))
        continue;
      int x = MARGIN + widgets->profile_width * (VIK_TRACKPOINT(iter->data)->timestamp - beg_time) / dur;
      int y = widgets->profile_height - widgets->profile_height*(gps_speed - mins)/(maxs - mins);
      gdk_draw_rectangle(GDK_DRAWABLE(pix), gps_speed_gc, TRUE, x-2, y-2, 4, 4);
    }
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), window->style->black_gc, FALSE, MARGIN, 0, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );
  g_object_unref ( G_OBJECT(gps_speed_gc) );

}
#undef LINES

/**
 * Draw all graphs
 */
static void draw_all_graphs ( GtkWidget *widget, gpointer *pass_along, gboolean resized )
{
  VikTrack *tr = pass_along[0];
  PropWidgets *widgets = pass_along[3];

  // Draw graphs even if they are not visible

  GList *child = NULL;
  GtkWidget *image = NULL;
  GtkWidget *window = gtk_widget_get_toplevel(widget);
  gdouble pc = NAN;

  // Draw elevations
  if (widgets->elev_box != NULL) {

    // Saved image no longer any good as we've resized, so we remove it here
    if (resized && widgets->elev_graph_saved_img.img) {
      g_object_unref(widgets->elev_graph_saved_img.img);
      widgets->elev_graph_saved_img.img = NULL;
      widgets->elev_graph_saved_img.saved = FALSE;
    }

    child = gtk_container_get_children(GTK_CONTAINER(widgets->elev_box));
    draw_elevations (GTK_WIDGET(child->data), tr, widgets );

    image = GTK_WIDGET(child->data);
    g_list_free(child);

    // Ensure marker is redrawn if necessary
    if (widgets->is_marker_drawn) {

      pc = tp_percentage_by_distance ( tr, widgets->marker_tp, widgets->track_length );
      gdouble marker_x = 0.0;
      if (!isnan(pc)) {
	marker_x = (pc * widgets->profile_width) + MARGIN + (image->allocation.width/2 - widgets->profile_width/2 - MARGIN/2);
	save_image_and_draw_graph_mark(image,
				       marker_x,
				       image->allocation.width,
				       window->style->black_gc,
				       &widgets->elev_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       &widgets->is_marker_drawn);
      }
    }
  }

  // Draw speeds
  if (widgets->speed_box != NULL) {

    // Saved image no longer any good as we've resized
    if (resized && widgets->speed_graph_saved_img.img) {
      g_object_unref(widgets->speed_graph_saved_img.img);
      widgets->speed_graph_saved_img.img = NULL;
      widgets->speed_graph_saved_img.saved = FALSE;
    }

    child = gtk_container_get_children(GTK_CONTAINER(widgets->speed_box));
    draw_vt (GTK_WIDGET(child->data), tr, widgets );

    image = GTK_WIDGET(child->data);
    g_list_free(child);

    // Ensure marker is redrawn if necessary
    if (widgets->is_marker_drawn) {

      pc = tp_percentage_by_time ( tr, widgets->marker_tp );

      gdouble marker_x = 0.0;
      if (!isnan(pc)) {
	marker_x = (pc * widgets->profile_width) + MARGIN + (image->allocation.width/2 - widgets->profile_width/2 - MARGIN/2);
	save_image_and_draw_graph_mark(image,
				       marker_x,
				       image->allocation.width,
				       window->style->black_gc,
				       &widgets->speed_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       &widgets->is_marker_drawn);
      }
    }
  }
}

/**
 * Configure/Resize the profile & speed/time images
 */
static gboolean configure_event ( GtkWidget *widget, GdkEventConfigure *event, gpointer *pass_along )
{
  PropWidgets *widgets = pass_along[3];

  if (widgets->configure_dialog) {
    // Determine size offsets between dialog size and size for images
    // Only on the initialisation of the dialog
    widgets->profile_width_offset = event->width - widgets->profile_width;
    widgets->profile_height_offset = event->height - widgets->profile_height;
    widgets->configure_dialog = FALSE;

    // Without this the settting, the dialog will only grow in vertical size - one can not then make it smaller!
    gtk_widget_set_size_request ( widget, widgets->profile_width+widgets->profile_width_offset, widgets->profile_height );
    // In fact this allows one to compress it a bit more vertically as I don't add on the height offset
  }
  else {
    widgets->profile_width_old = widgets->profile_width;
    widgets->profile_height_old = widgets->profile_height;
  }

  // Now adjust From Dialog size to get image size
  widgets->profile_width = event->width - widgets->profile_width_offset;
  widgets->profile_height = event->height - widgets->profile_height_offset;

  // ATM we receive configure_events when the dialog is moved and so no further action is necessary
  if ( !widgets->configure_dialog &&
       (widgets->profile_width_old == widgets->profile_width) && (widgets->profile_height_old == widgets->profile_height) )
    return FALSE;

  // Draw stuff
  draw_all_graphs ( widget, pass_along, TRUE );

  return FALSE;
}

/**
 * Create height profile widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_profile ( GtkWidget *window, VikTrack *tr, gpointer vlp, VikViewport *vvp, PropWidgets *widgets, gdouble *min_alt, gdouble *max_alt)
{
  GdkPixmap *pix;
  GtkWidget *image;
  GtkWidget *eventbox;
  gpointer *pass_along;

  // First allocation
  widgets->altitudes = vik_track_make_elevation_map ( tr, widgets->profile_width );

  if ( widgets->altitudes == NULL ) {
    *min_alt = *max_alt = VIK_DEFAULT_ALTITUDE;
    return NULL;
  }

  minmax_array(widgets->altitudes, min_alt, max_alt, TRUE, widgets->profile_width);
  
  pix = gdk_pixmap_new( window->window, widgets->profile_width + MARGIN, widgets->profile_height, -1 );
  image = gtk_image_new_from_pixmap ( pix, NULL );

  g_object_unref ( G_OBJECT(pix) );

  pass_along = g_malloc ( sizeof(gpointer) * 4 ); /* FIXME: mem leak -- never be freed */
  pass_along[0] = tr;
  pass_along[1] = vlp;
  pass_along[2] = vvp;
  pass_along[3] = widgets;

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_profile_click), pass_along );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_profile_move), pass_along );
  g_signal_connect_swapped ( G_OBJECT(eventbox), "destroy", G_CALLBACK(g_free), pass_along );
  gtk_container_add ( GTK_CONTAINER(eventbox), image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_STRUCTURE_MASK);

  return eventbox;
}

/**
 * Create speed/time widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_vtdiag ( GtkWidget *window, VikTrack *tr, gpointer vlp, VikViewport *vvp, PropWidgets *widgets)
{
  GdkPixmap *pix;
  GtkWidget *image;
  GtkWidget *eventbox;
  gpointer *pass_along;

  // First allocation
  widgets->speeds = vik_track_make_speed_map ( tr, widgets->profile_width );
  if ( widgets->speeds == NULL )
    return NULL;

  pass_along = g_malloc ( sizeof(gpointer) * 4 ); /* FIXME: mem leak -- never be freed */
  pass_along[0] = tr;
  pass_along[1] = vlp;
  pass_along[2] = vvp;
  pass_along[3] = widgets;

  pix = gdk_pixmap_new( window->window, widgets->profile_width + MARGIN, widgets->profile_height, -1 );
  image = gtk_image_new_from_pixmap ( pix, NULL );

#if 0
  /* XXX this can go out, it's just a helpful dev tool */
  {
    int j;
    GdkGC **colors[8] = { window->style->bg_gc,
			  window->style->fg_gc,
			  window->style->light_gc,
			  window->style->dark_gc,
			  window->style->mid_gc,
			  window->style->text_gc,
			  window->style->base_gc,
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
#endif

  g_object_unref ( G_OBJECT(pix) );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_vt_click), pass_along );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_vt_move), pass_along );
  g_signal_connect_swapped ( G_OBJECT(eventbox), "destroy", G_CALLBACK(g_free), pass_along );
  gtk_container_add ( GTK_CONTAINER(eventbox), image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

  return eventbox;
}
#undef MARGIN

static void propwin_response_cb( GtkDialog *dialog, gint resp, PropWidgets *widgets)
{
  VikTrack *tr = widgets->tr;
  VikTrwLayer *vtl = widgets->vtl;
  gboolean keep_dialog = FALSE;

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
             ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl), "The track \"%s\" exists, do you wish to overwrite it?", new_tr_name ) ) )
          {
            gchar *new_new_tr_name = a_dialog_new_track ( VIK_GTK_WINDOW_FROM_LAYER(vtl), vik_trw_layer_get_tracks(vtl), NULL );
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
    case VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER:
      {
        GList *iter = tr->trackpoints;
        while ((iter = iter->next)) {
          if (widgets->marker_tp == VIK_TRACKPOINT(iter->data))
            break;
        }
        if (iter == NULL) {
          a_dialog_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), GTK_MESSAGE_ERROR,
                  _("Failed spliting track. Track unchanged"), NULL);
          keep_dialog = TRUE;
          break;
        }

        gchar *r_name = g_strdup_printf("%s #R", widgets->track_name);
        if (vik_trw_layer_get_track(vtl, r_name ) && 
             ( ! a_dialog_yes_or_no( VIK_GTK_WINDOW_FROM_LAYER(vtl),
              "The track \"%s\" exists, do you wish to overwrite it?", r_name)))
        {
	  gchar *new_r_name = a_dialog_new_track( VIK_GTK_WINDOW_FROM_LAYER(vtl), vik_trw_layer_get_tracks(vtl), NULL );
            if (new_r_name) {
              g_free( r_name );
              r_name = new_r_name;
            }
            else {
              a_dialog_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), GTK_MESSAGE_WARNING,
                  _("Operation Aborted. Track unchanged"), NULL);
              keep_dialog = TRUE;
              break;
            }
        }
        iter->prev->next = NULL;
        iter->prev = NULL;
        VikTrack *tr_right = vik_track_new();
        if ( tr->comment )
          vik_track_set_comment ( tr_right, tr->comment );
        tr_right->visible = tr->visible;
        tr_right->trackpoints = iter;

        vik_trw_layer_add_track(vtl, r_name, tr_right);
        vik_layer_emit_update ( VIK_LAYER(vtl) );
      }
      break;
    default:
      fprintf(stderr, "DEBUG: unknown response\n");
      return;
  }

  /* Keep same behaviour for now: destroy dialog if click on any button */
  if (!keep_dialog) {
    prop_widgets_free(widgets);
    vik_track_clear_property_dialog(tr);
    gtk_widget_destroy ( GTK_WIDGET(dialog) );
  }
}

/**
 * Force a redraw when checkbutton has been toggled to show/hide that information
 */
static void checkbutton_toggle_cb ( GtkToggleButton *togglebutton, gpointer *pass_along, gpointer dummy)
{
  PropWidgets *widgets = pass_along[3];
  // Even though not resized, we'll pretend it is -
  //  as this invalidates the saved images (since the image may have changed)
  draw_all_graphs ( widgets->dialog, pass_along, TRUE);
}

/**
 *  Create the widgets for the given graph tab
 */
static GtkWidget *create_graph_page ( GtkWidget *graph,
				      const gchar *markup,
				      GtkWidget *value,
				      const gchar *markup2,
				      GtkWidget *value2,
				      GtkWidget *checkbutton1,
				      gboolean checkbutton1_default,
				      GtkWidget *checkbutton2,
				      gboolean checkbutton2_default )
{
  GtkWidget *hbox = gtk_hbox_new ( FALSE, 10 );
  GtkWidget *vbox = gtk_vbox_new ( FALSE, 10 );
  GtkWidget *label = gtk_label_new (NULL);
  GtkWidget *label2 = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(vbox), graph, FALSE, FALSE, 0);
  gtk_label_set_markup ( GTK_LABEL(label), markup );
  gtk_label_set_markup ( GTK_LABEL(label2), markup2 );
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), value, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), label2, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), value2, FALSE, FALSE, 0);
  if (checkbutton2) {
    gtk_box_pack_end (GTK_BOX(hbox), checkbutton2, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton2), checkbutton2_default);
  }
  if (checkbutton1) {
    gtk_box_pack_end (GTK_BOX(hbox), checkbutton1, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton1), checkbutton1_default);
  }
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  return vbox;
}

void vik_trw_layer_propwin_run ( GtkWindow *parent, VikTrwLayer *vtl, VikTrack *tr, gpointer vlp, gchar *track_name, VikViewport *vvp )
{
  /* FIXME: free widgets when destroy signal received */
  PropWidgets *widgets = prop_widgets_new();
  widgets->vtl = vtl;
  widgets->tr = tr;
  widgets->vlp = vlp;
  widgets->profile_width  = PROPWIN_PROFILE_WIDTH;
  widgets->profile_height = PROPWIN_PROFILE_HEIGHT;
  widgets->track_name = track_name;
  gchar *title = g_strdup_printf(_("%s - Track Properties"), track_name);
  GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
                         parent,
                         GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                         _("Split at _Marker"), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER,
                         _("Split _Segments"), VIK_TRW_LAYER_PROPWIN_SPLIT,
                         _("_Reverse"),        VIK_TRW_LAYER_PROPWIN_REVERSE,
                         _("_Delete Dupl."),   VIK_TRW_LAYER_PROPWIN_DEL_DUP,
                         GTK_STOCK_OK,     GTK_RESPONSE_ACCEPT,
                         NULL);
  widgets->dialog = dialog;
  g_free(title);
  g_signal_connect(dialog, "response", G_CALLBACK(propwin_response_cb), widgets);
  GtkTable *table;
  gdouble tr_len;
  guint32 tp_count, seg_count;

  gdouble min_alt, max_alt;
  widgets->elev_box = vik_trw_layer_create_profile(GTK_WIDGET(parent), tr, vlp, vvp, widgets, &min_alt, &max_alt);
  widgets->speed_box = vik_trw_layer_create_vtdiag(GTK_WIDGET(parent), tr, vlp, vvp, widgets);
  GtkWidget *graphs = gtk_notebook_new();

  GtkWidget *content[20];
  int cnt;
  int i;

  static gchar *label_texts[] = { N_("<b>Comment:</b>"), N_("<b>Track Length:</b>"), N_("<b>Trackpoints:</b>"), N_("<b>Segments:</b>"), N_("<b>Duplicate Points:</b>"), N_("<b>Max Speed:</b>"), N_("<b>Avg. Speed:</b>"), N_("<b>Avg. Dist. Between TPs:</b>"), N_("<b>Elevation Range:</b>"), N_("<b>Total Elevation Gain/Loss:</b>"), N_("<b>Start:</b>"), N_("<b>End:</b>"), N_("<b>Duration:</b>") };
  static gchar tmp_buf[50];
  gdouble tmp_speed;

  cnt = 0;
  widgets->w_comment = gtk_entry_new ();
  if ( tr->comment )
    gtk_entry_set_text ( GTK_ENTRY(widgets->w_comment), tr->comment );
  g_signal_connect_swapped ( widgets->w_comment, "activate", G_CALLBACK(a_dialog_response_accept), GTK_DIALOG(dialog) );
  content[cnt++] = widgets->w_comment;

  vik_units_distance_t dist_units = a_vik_get_units_distance ();

  tr_len = widgets->track_length = vik_track_get_length(tr);
  switch (dist_units) {
  case VIK_UNITS_DISTANCE_KILOMETRES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", tr_len/1000.0 );
    break;
  case VIK_UNITS_DISTANCE_MILES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", VIK_METERS_TO_MILES(tr_len) );
    break;
  default:
    g_critical("Houston, we've had a problem. distance=%d", dist_units);
  }
  widgets->w_track_length = content[cnt++] = gtk_label_new ( tmp_buf );

  tp_count = vik_track_get_tp_count(tr);
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%u", tp_count );
  widgets->w_tp_count = content[cnt++] = gtk_label_new ( tmp_buf );

  seg_count = vik_track_get_segment_count(tr) ;
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count );
  widgets->w_segment_count = content[cnt++] = gtk_label_new ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%lu", vik_track_get_dup_point_count(tr) );
  widgets->w_duptp_count = content[cnt++] = gtk_label_new ( tmp_buf );

  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  tmp_speed = vik_track_get_max_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    switch (speed_units) {
    case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km/h", VIK_MPS_TO_KPH(tmp_speed));
      break;
    case VIK_UNITS_SPEED_MILES_PER_HOUR:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f mph", VIK_MPS_TO_MPH(tmp_speed));
      break;
    case VIK_UNITS_SPEED_METRES_PER_SECOND:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", tmp_speed );
      break;
    case VIK_UNITS_SPEED_KNOTS:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f knots", VIK_MPS_TO_KNOTS(tmp_speed));
      break;
    default:
      g_snprintf (tmp_buf, sizeof(tmp_buf), "--" );
      g_critical("Houston, we've had a problem. speed=%d", speed_units);
    }
  }
  widgets->w_max_speed = content[cnt++] = gtk_label_new ( tmp_buf );

  tmp_speed = vik_track_get_average_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    switch (speed_units) {
    case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km/h", VIK_MPS_TO_KPH(tmp_speed));
      break;
    case VIK_UNITS_SPEED_MILES_PER_HOUR:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f mph", VIK_MPS_TO_MPH(tmp_speed));
      break;
    case VIK_UNITS_SPEED_METRES_PER_SECOND:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m/s", tmp_speed );
      break;
    case VIK_UNITS_SPEED_KNOTS:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f knots", VIK_MPS_TO_KNOTS(tmp_speed));
      break;
    default:
      g_snprintf (tmp_buf, sizeof(tmp_buf), "--" );
      g_critical("Houston, we've had a problem. speed=%d", speed_units);
    }
  }
  widgets->w_avg_speed = content[cnt++] = gtk_label_new ( tmp_buf );

  switch (dist_units) {
  case VIK_UNITS_DISTANCE_KILOMETRES:
    // Even though kilometres, the average distance between points is going to be quite small so keep in metres
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / ( tp_count - seg_count ) );
    break;
  case VIK_UNITS_DISTANCE_MILES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.3f miles", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_MILES(tr_len / ( tp_count - seg_count )) );
    break;
  default:
    g_critical("Houston, we've had a problem. distance=%d", dist_units);
  }
  widgets->w_avg_dist = content[cnt++] = gtk_label_new ( tmp_buf );

  vik_units_height_t height_units = a_vik_get_units_height ();
  if ( min_alt == VIK_DEFAULT_ALTITUDE )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt );
      break;
    case VIK_UNITS_HEIGHT_FEET:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet - %.0f feet", VIK_METERS_TO_FEET(min_alt), VIK_METERS_TO_FEET(max_alt) );
      break;
    default:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "--" );
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }
  }
  widgets->w_elev_range = content[cnt++] = gtk_label_new ( tmp_buf );

  vik_track_get_total_elevation_gain(tr, &max_alt, &min_alt );
  if ( min_alt == VIK_DEFAULT_ALTITUDE )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m / %.0f m", max_alt, min_alt );
      break;
    case VIK_UNITS_HEIGHT_FEET:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet / %.0f feet", VIK_METERS_TO_FEET(max_alt), VIK_METERS_TO_FEET(min_alt) );
      break;
    default:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "--" );
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }
  }
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

    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d minutes"), (int)(t2-t1)/60);
    widgets->w_time_dur = content[cnt++] = gtk_label_new(tmp_buf);
  } else {
    widgets->w_time_start = content[cnt++] = gtk_label_new(_("No Data"));
    widgets->w_time_end = content[cnt++] = gtk_label_new(_("No Data"));
    widgets->w_time_dur = content[cnt++] = gtk_label_new(_("No Data"));
  }

  table = GTK_TABLE(gtk_table_new (cnt, 2, FALSE));
  gtk_table_set_col_spacing (table, 0, 10);
  for (i=0; i<cnt; i++) {
    GtkWidget *label;

    // Settings so the text positioning only moves around vertically when the dialog is resized
    // This also gives more room to see the track comment
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment ( GTK_MISC(label), 1, 0.5 ); // Position text centrally in vertical plane
    gtk_label_set_markup ( GTK_LABEL(label), _(label_texts[i]) );
    gtk_table_attach ( table, label, 0, 1, i, i+1, GTK_FILL, GTK_SHRINK, 0, 0 );
    if (GTK_IS_MISC(content[i])) {
      gtk_misc_set_alignment ( GTK_MISC(content[i]), 0, 0.5 );
    }
    gtk_table_attach_defaults ( table, content[i], 1, 2, i, i+1 );
  }

  gtk_notebook_append_page(GTK_NOTEBOOK(graphs), GTK_WIDGET(table), gtk_label_new(_("Statistics")));

  gpointer *pass_along;
  pass_along = g_malloc ( sizeof(gpointer) * 4 ); /* FIXME: mem leak -- never be freed */
  pass_along[0] = tr;
  pass_along[1] = vlp;
  pass_along[2] = vvp;
  pass_along[3] = widgets;

  if ( widgets->elev_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_dist = gtk_label_new(_("No Data"));
    widgets->w_cur_elevation = gtk_label_new(_("No Data"));
    widgets->w_show_dem = gtk_check_button_new_with_mnemonic(_("Show D_EM"));
    widgets->w_show_alt_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
    page = create_graph_page (widgets->elev_box,
			      _("<b>Track Distance:</b>"), widgets->w_cur_dist,
			      _("<b>Track Height:</b>"), widgets->w_cur_elevation,
			      widgets->w_show_dem, TRUE,
			      widgets->w_show_alt_gps_speed, TRUE);
    g_signal_connect (widgets->w_show_dem, "toggled", G_CALLBACK (checkbutton_toggle_cb), pass_along);
    g_signal_connect (widgets->w_show_alt_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), pass_along);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Elevation-distance")));
  }

  if ( widgets->speed_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_time = gtk_label_new(_("No Data"));
    widgets->w_cur_speed = gtk_label_new(_("No Data"));
    widgets->w_show_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
    page = create_graph_page (widgets->speed_box,
			      _("<b>Track Time:</b>"), widgets->w_cur_time,
			      _("<b>Track Speed:</b>"), widgets->w_cur_speed,
			      widgets->w_show_gps_speed, TRUE,
			      NULL, FALSE);
    g_signal_connect (widgets->w_show_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), pass_along);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Speed-time")));
  }

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), graphs, FALSE, FALSE, 0);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, FALSE);
  if (seg_count <= 1)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT, FALSE);
  if (vik_track_get_dup_point_count(tr) <= 0)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_DEL_DUP, FALSE);

  // On dialog realization configure_event casues the graphs to be initially drawn
  widgets->configure_dialog = TRUE;
  g_signal_connect ( G_OBJECT(dialog), "configure-event", G_CALLBACK (configure_event), pass_along);

  vik_track_set_property_dialog(tr, dialog);
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  gtk_widget_show_all ( dialog );
}
