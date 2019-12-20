/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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
#include "viktrwlayer.h"
#include "viktrwlayer_propwin.h"
#include "dems.h"
#include "viking.h"
#include "vikviewport.h" /* ugh */
#include "ui_util.h"
#include <gdk-pixbuf/gdk-pixdata.h>

#define BLOB_SIZE 4

typedef enum {
  PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE,
  PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE,
  PROPWIN_GRAPH_TYPE_SPEED_TIME,
  PROPWIN_GRAPH_TYPE_DISTANCE_TIME,
  PROPWIN_GRAPH_TYPE_ELEVATION_TIME,
  PROPWIN_GRAPH_TYPE_SPEED_DISTANCE,
  PROPWIN_GRAPH_TYPE_END,
} VikPropWinGraphType_t;

/* (Hopefully!) Human friendly altitude grid sizes - note no fixed 'ratio' just numbers that look nice...*/
static const gdouble chunksa[] = {2.0, 5.0, 10.0, 15.0, 20.0,
				  25.0, 40.0, 50.0, 75.0, 100.0,
				  150.0, 200.0, 250.0, 375.0, 500.0,
				  750.0, 1000.0, 2000.0, 5000.0, 10000.0, 100000.0};

/* (Hopefully!) Human friendly gradient grid sizes - note no fixed 'ratio' just numbers that look nice...*/
static const gdouble chunksg[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				  12.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 75.0,
				  100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				  750.0, 1000.0, 10000.0, 100000.0};
// Normally gradients should range up to couple hundred precent at most,
//  however there are possibilities of having points with no altitude after a point with a big altitude
//  (such as places with invalid DEM values in otherwise mountainous regions) - thus giving huge negative gradients.

/* (Hopefully!) Human friendly grid sizes - note no fixed 'ratio' just numbers that look nice...*/
/* As need to cover walking speeds - have many low numbers (but also may go up to airplane speeds!) */
static const gdouble chunkss[] = {1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				  15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
				  100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				  750.0, 1000.0, 10000.0};

/* (Hopefully!) Human friendly distance grid sizes - note no fixed 'ratio' just numbers that look nice...*/
static const gdouble chunksd[] = {0.1, 0.2, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0,
				  15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
				  100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
				  750.0, 1000.0, 10000.0};

// Time chunks in seconds
static const gdouble chunkst[] = {
  60,     // 1 minute
  120,    // 2 minutes
  300,    // 5 minutes
  900,    // 15 minutes
  1800,   // half hour
  3600,   // 1 hour
  10800,  // 3 hours
  21600,  // 6 hours
  43200,  // 12 hours
  86400,  // 1 day
  172800, // 2 days
  604800, // 1 week
  1209600,// 2 weeks
  2419200,// 4 weeks
};

// Local show settings to restore on dialog opening
static gboolean show_dem                = TRUE;
static gboolean show_alt_gps_speed      = TRUE;
static gboolean show_gps_speed          = TRUE;
static gboolean show_gradient_gps_speed = TRUE;
static gboolean show_dist_speed         = FALSE;
static gboolean show_elev_speed         = FALSE;
static gboolean show_elev_dem           = FALSE;
static gboolean show_sd_gps_speed       = TRUE;

static gboolean main_show_dem = TRUE;
static gboolean main_show_alt_gps_speed = FALSE; // ATM delibrately no GUI control
static gboolean main_show_gps_speed = TRUE;
static VikPropWinGraphType_t main_last_graph = PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE;

typedef struct _propsaved {
  gboolean saved;
  GdkImage *img;
} PropSaved;

typedef struct _propwidgets {
  gboolean  configure_dialog;
  VikTrwLayer *vtl;
  VikTrack *tr;
  VikViewport *vvp;
  VikLayersPanel *vlp;
  gint      profile_width;
  gint      profile_height;
  gint      profile_width_old;
  gint      profile_height_old;
  gint      profile_width_offset;
  gint      profile_height_offset;
  GtkWidget *dialog;
  GtkWidget *graphs; // When embedded in main window
  GtkWidget *self; // When embedded in main window
  GtkWidget *w_comment;
  GtkWidget *w_description;
  GtkWidget *w_source;
  GtkWidget *w_type;
  GtkWidget *w_track_length;
  GtkWidget *w_tp_count;
  GtkWidget *w_segment_count;
  GtkWidget *w_duptp_count;
  GtkWidget *w_max_speed;
  GtkWidget *w_avg_speed;
  GtkWidget *w_mvg_speed;
  GtkWidget *w_avg_dist;
  GtkWidget *w_elev_range;
  GtkWidget *w_elev_gain;
  GtkWidget *w_time_start;
  GtkWidget *w_time_end;
  GtkWidget *w_time_dur;
  GtkWidget *w_color;
  GtkWidget *w_namelabel;
  GtkWidget *w_number_distlabels;
  GtkWidget *w_cur_dist; /*< Current distance */
  GtkWidget *w_cur_elevation;
  GtkWidget *w_cur_gradient_dist; /*< Current distance on gradient graph */
  GtkWidget *w_cur_gradient_gradient; /*< Current gradient on gradient graph */
  GtkWidget *w_cur_time; /*< Current track time */
  GtkWidget *w_cur_time_real; /*< Actual time as on a clock */
  GtkWidget *w_cur_speed;
  GtkWidget *w_cur_dist_dist; /*< Current distance on distance graph */
  GtkWidget *w_cur_dist_time; /*< Current track time on distance graph */
  GtkWidget *w_cur_dist_time_real; // Clock time
  GtkWidget *w_cur_elev_elev;
  GtkWidget *w_cur_elev_time; // Track time
  GtkWidget *w_cur_elev_time_real; // Clock time
  GtkWidget *w_cur_speed_dist;
  GtkWidget *w_cur_speed_speed;
  GtkWidget *w_show_dem;
  GtkWidget *w_show_alt_gps_speed;
  GtkWidget *w_show_gps_speed;
  GtkWidget *w_show_gradient_gps_speed;
  GtkWidget *w_show_dist_speed;
  GtkWidget *w_show_elev_speed;
  GtkWidget *w_show_elev_dem;
  GtkWidget *w_show_sd_gps_speed;
  gboolean  show_dem;
  gboolean  show_alt_gps_speed;
  gboolean  show_gps_speed;
  gdouble   track_length;
  gdouble   track_length_inc_gaps;
  PropSaved elev_graph_saved_img;
  PropSaved gradient_graph_saved_img;
  PropSaved speed_graph_saved_img;
  PropSaved dist_graph_saved_img;
  PropSaved elev_time_graph_saved_img;
  PropSaved speed_dist_graph_saved_img;
  GtkWidget *elev_box;
  GtkWidget *elev_image;
  GtkWidget *gradient_box;
  GtkWidget *gradient_image;
  GtkWidget *speed_box;
  GtkWidget *speed_image;
  GtkWidget *dist_box;
  GtkWidget *dist_image;
  GtkWidget *elev_time_box;
  GtkWidget *elev_time_image;
  GtkWidget *speed_dist_box;
  GtkWidget *speed_dist_image;
  gdouble   alt_create_time;
  gdouble   *altitudes;
  gdouble   *ats; // altitudes in time
  gdouble   min_altitude;
  gdouble   max_altitude;
  gdouble   draw_min_altitude;
  gdouble   draw_min_altitude_time;
  gint      cia; // Chunk size Index into Altitudes
  gint      ciat; // Chunk size Index into Altitudes / Time
  // NB cia & ciat are normally same value but sometimes not due to differing methods of altitude array creation
  //    thus also have draw_min_altitude for each altitude graph type
  gdouble   *gradients;
  gdouble   min_gradient;
  gdouble   max_gradient;
  gdouble   draw_min_gradient;
  gint      cig; // Chunk size Index into Gradients
  gdouble   *speeds;
  gdouble   *speeds_dist;
  gdouble   min_speed;
  gdouble   max_speed;
  gdouble   draw_min_speed;
  gdouble   max_speed_dist;
  gint      cis; // Chunk size Index into Speeds
  gint      cisd; // Chunk size Index into Speed/Distance
  gdouble   *distances;
  gint      cid; // Chunk size Index into Distance
  VikTrackpoint *marker_tp;
  gboolean  is_marker_drawn;
  VikTrackpoint *blob_tp;
  gboolean  is_blob_drawn;
  gdouble   duration;
  gchar     *tz; // TimeZone at track's location
} PropWidgets;

// Local functions
static gboolean split_at_marker ( PropWidgets *widgets );
static void draw_all_graphs ( GtkWidget *widget, PropWidgets *widgets, gboolean resized );
static GtkWidget *create_statistics_page ( PropWidgets *widgets, VikTrack *tr );
static GtkWidget *create_splits_tables ( VikTrack *trk );

static PropWidgets *prop_widgets_new()
{
  PropWidgets *widgets = g_malloc0(sizeof(PropWidgets));

  return widgets;
}

static void prop_widgets_free(PropWidgets *widgets)
{
  if (widgets->elev_graph_saved_img.img)
    g_object_unref(widgets->elev_graph_saved_img.img);
  if (widgets->gradient_graph_saved_img.img)
    g_object_unref(widgets->gradient_graph_saved_img.img);
  if (widgets->speed_graph_saved_img.img)
    g_object_unref(widgets->speed_graph_saved_img.img);
  if (widgets->dist_graph_saved_img.img)
    g_object_unref(widgets->dist_graph_saved_img.img);
  if (widgets->elev_time_graph_saved_img.img)
    g_object_unref(widgets->elev_time_graph_saved_img.img);
  if (widgets->speed_dist_graph_saved_img.img)
    g_object_unref(widgets->speed_dist_graph_saved_img.img);
  if (widgets->altitudes)
    g_free(widgets->altitudes);
  if (widgets->gradients)
    g_free(widgets->gradients);
  if (widgets->speeds)
    g_free(widgets->speeds);
  if (widgets->distances)
    g_free(widgets->distances);
  if (widgets->ats)
    g_free(widgets->ats);
  if (widgets->speeds_dist)
    g_free(widgets->speeds_dist);
  g_free(widgets);
}

static void minmax_array(const gdouble *array, gdouble *min, gdouble *max, gboolean NO_ALT_TEST, gint PROFILE_WIDTH)
{
  *max = -1000;
  *min = 20000;
  guint i;
  for ( i=0; i < PROFILE_WIDTH; i++ ) {
    if ( NO_ALT_TEST || (!isnan(array[i])) ) {
      if ( array[i] > *max )
        *max = array[i];
      if ( array[i] < *min )
        *min = array[i];
    }
  }
}

#define MARGIN_X 70
#define MARGIN_Y 20
#define LINES 5
/**
 * get_new_min_and_chunk_index:
 * Returns via pointers:
 *   the new minimum value to be used for the graph
 *   the index in to the chunk sizes array (ci = Chunk Index)
 */
static void get_new_min_and_chunk_index (gdouble mina, gdouble maxa, const gdouble *chunks, size_t chunky, gdouble *new_min, gint *ci)
{
  /* Get unitized chunk */
  /* Find suitable chunk index */
  *ci = 0;
  gdouble diff_chunk = (maxa - mina)/LINES;

  /* Loop through to find best match */
  while (diff_chunk > chunks[*ci]) {
    (*ci)++;
    /* Last Resort Check */
    if ( *ci == chunky ) {
      // Use previous value and exit loop
      (*ci)--;
      break;
    }
  }

  /* Ensure adjusted minimum .. maximum covers mina->maxa */

  // Now work out adjusted minimum point to the nearest lowest chunk divisor value
  // When negative ensure logic uses lowest value
  if ( mina < 0 )
    *new_min = (gdouble) ( (gint)((mina - chunks[*ci]) / chunks[*ci]) * chunks[*ci] );
  else
    *new_min = (gdouble) ( (gint)(mina / chunks[*ci]) * chunks[*ci] );

  // Range not big enough - as new minimum has lowered
  if ((*new_min + (chunks[*ci] * LINES) < maxa)) {
    // Next chunk should cover it
    if ( *ci < chunky-1 ) {
      (*ci)++;
      // Remember to adjust the minimum too...
      if ( mina < 0 )
        *new_min = (gdouble) ( (gint)((mina - chunks[*ci]) / chunks[*ci]) * chunks[*ci] );
      else
        *new_min = (gdouble) ( (gint)(mina / chunks[*ci]) * chunks[*ci] );
    }
  }
}

static guint get_time_chunk_index (gdouble duration)
{
  // Grid split
  gdouble myduration = duration / LINES;

  // Search nearest chunk index
  guint ci = 0;
  guint last_chunk = G_N_ELEMENTS(chunkst);

  // Loop through to find best match
  while (myduration > chunkst[ci]) {
    ci++;
    // Last Resort Check
    if ( ci == last_chunk )
      break;
  }
  // Use previous value
  if ( ci != 0 )
   ci--;

  return ci;
}

/**
 *
 */
static guint get_distance_chunk_index (gdouble length)
{
  // Grid split
  gdouble mylength = length / LINES;

  // Search nearest chunk index
  guint ci = 0;
  guint last_chunk = G_N_ELEMENTS(chunksd);

  // Loop through to find best match
  while (mylength > chunksd[ci]) {
    ci++;
    // Last Resort Check
    if ( ci == last_chunk )
      break;
  }
  // Use previous value
  if ( ci != 0 )
   ci--;

  return ci;
}

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
  gdouble x = event_x - img_width / 2 + PROFILE_WIDTH / 2 - MARGIN_X / 2;
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
      vik_viewport_set_center_coord ( vik_layers_panel_get_viewport(vlp), &coord, TRUE );
      vik_layers_panel_emit_update ( vlp );
    }
    else {
      /* since vlp not set, vvp should be valid instead! */
      if ( vvp )
        vik_viewport_set_center_coord ( vvp, &coord, TRUE );
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    }
  }
  return trackpoint;
}

/**
 * Returns whether the marker was drawn or not and whether the blob was drawn or not
 */
static void save_image_and_draw_graph_marks (GtkWidget *image,
					     gdouble marker_x,
					     GdkGC *gc,
					     gint blob_x,
					     gint blob_y,
					     PropSaved *saved_img,
					     gint PROFILE_WIDTH,
					     gint PROFILE_HEIGHT,
					     guint blob_size,
					     gboolean *marker_drawn,
					     gboolean *blob_drawn)
{
  GdkPixmap *pix = NULL;
  /* the pixmap = margin + graph area */
  gtk_image_get_pixmap(GTK_IMAGE(image), &pix, NULL);

  /* Restore previously saved image */
  if (saved_img->saved) {
    gdk_draw_image(GDK_DRAWABLE(pix), gc, saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
    saved_img->saved = FALSE;
  }

  // ATM always save whole image - as anywhere could have changed
  if (saved_img->img)
    gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
  else
    saved_img->img = gdk_drawable_copy_to_image(GDK_DRAWABLE(pix), saved_img->img, 0, 0, 0, 0, MARGIN_X+PROFILE_WIDTH, MARGIN_Y+PROFILE_HEIGHT);
  saved_img->saved = TRUE;

  if ((marker_x >= MARGIN_X) && (marker_x < (PROFILE_WIDTH + MARGIN_X))) {
    gdk_draw_line (GDK_DRAWABLE(pix), gc, marker_x, MARGIN_Y, marker_x, PROFILE_HEIGHT + MARGIN_Y);
    *marker_drawn = TRUE;
  }
  else
    *marker_drawn = FALSE;

  // Draw a square blob to indicate where we are on track for this graph
  if ( (blob_x >= MARGIN_X) && (blob_x < (PROFILE_WIDTH + MARGIN_X)) && (blob_y < PROFILE_HEIGHT+MARGIN_Y) ) {
    gdk_draw_rectangle (GDK_DRAWABLE(pix), gc, TRUE, blob_x-3, blob_y-3, blob_size, blob_size);
    *blob_drawn = TRUE;
  }
  else
    *blob_drawn = FALSE;
  
  // Anywhere on image could have changed
  if (*marker_drawn || *blob_drawn)
    gtk_widget_queue_draw(image);
}

/**
 * Return the percentage of how far a trackpoint is a long a track via the time method
 */
static gdouble tp_percentage_by_time ( VikTrack *tr, VikTrackpoint *trackpoint )
{
  gdouble pc = NAN;
  if (trackpoint == NULL)
    return pc;
  gdouble t_start, t_end, t_total;
  t_start = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  t_end = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;
  t_total = t_end - t_start;
  pc = (trackpoint->timestamp - t_start)/t_total;
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

/**
 * Menu callbacks for the main display graphs
 */
static gboolean menu_split_at_marker_cb ( PropWidgets *widgets )
{
  (void)split_at_marker ( widgets );
  return FALSE;
}

static gboolean menu_show_dem_cb ( PropWidgets *widgets )
{
  widgets->show_dem = !widgets->show_dem;
  // Force redraw
  draw_all_graphs ( GTK_WIDGET(widgets->graphs), widgets, TRUE );
  return FALSE;
}

static gboolean menu_show_gps_speed_cb ( PropWidgets *widgets )
{
  widgets->show_gps_speed = !widgets->show_gps_speed;
  // Force redraw
  draw_all_graphs ( GTK_WIDGET(widgets->graphs), widgets, TRUE );
  return FALSE;
}

static gboolean menu_properties_cb ( PropWidgets *widgets )
{
  vik_trw_layer_propwin_run ( VIK_GTK_WINDOW_FROM_LAYER(widgets->vtl),
			      widgets->vtl,
			      widgets->tr,
			      widgets->vlp,
			      widgets->vvp,
			      FALSE );
  return FALSE;
}

static gboolean menu_statistics_cb ( PropWidgets *widgets )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons ( widgets->tr->name,
						    GTK_WINDOW(gtk_widget_get_toplevel(widgets->self)),
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);

  GtkWidget *nb = gtk_notebook_new();

  gtk_notebook_append_page ( GTK_NOTEBOOK(nb), create_statistics_page(widgets, widgets->tr), gtk_label_new(_("Statistics")) );

  if ( widgets->speed_box )
    gtk_notebook_append_page ( GTK_NOTEBOOK(nb), create_splits_tables(widgets->tr), gtk_label_new(_("Splits")) );

  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), nb, TRUE, TRUE, 0 );

  gtk_widget_show_all ( dialog );

  (void)gtk_dialog_run ( GTK_DIALOG(dialog) );

  gtk_widget_destroy ( dialog );

  return FALSE;
}

/**
 *
 */
static void graph_click_menu_popup ( PropWidgets *widgets, VikPropWinGraphType_t graph_type )
{
  GtkWidget *menu = gtk_menu_new ();
  GtkWidget *iprop = vu_menu_add_item ( GTK_MENU(menu), NULL, GTK_STOCK_PROPERTIES, G_CALLBACK(menu_properties_cb), widgets );
  gtk_widget_set_sensitive ( GTK_WIDGET(iprop), !widgets->tr->property_dialog );
  GtkWidget *ist = vu_menu_add_item ( GTK_MENU(menu), _("_Statistics"), NULL, G_CALLBACK(menu_statistics_cb), widgets );
  gtk_widget_set_sensitive ( GTK_WIDGET(ist), !widgets->tr->property_dialog );
  GtkWidget *ism = vu_menu_add_item ( GTK_MENU(menu), _("Split at Marker"), GTK_STOCK_CUT, G_CALLBACK(menu_split_at_marker_cb), widgets );
  gtk_widget_set_sensitive ( ism, (gboolean)GPOINTER_TO_INT(widgets->marker_tp) );

  // Only for the embedded graphs
  GtkMenu *show_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *items = vu_menu_add_item ( GTK_MENU(menu), _("_Show"), NULL, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(items), GTK_WIDGET(show_submenu) );

  // Ensure item value is set before adding the callback on these check menu items
  //  otherwise the callback may be invoked when we set the value!
  if ( graph_type == PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE ) {
    GtkWidget *id = gtk_check_menu_item_new_with_mnemonic ( _("_DEM") );
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(id), widgets->show_dem );
    g_signal_connect_swapped ( G_OBJECT(id), "toggled", G_CALLBACK(menu_show_dem_cb), widgets );
    gtk_menu_shell_append ( GTK_MENU_SHELL(show_submenu), id );
    gtk_widget_set_sensitive ( id, a_dems_overlaps_bbox (widgets->tr->bbox) );
  }

  if ( graph_type == PROPWIN_GRAPH_TYPE_SPEED_TIME ) {
    GtkWidget *ig = gtk_check_menu_item_new_with_mnemonic ( _("_GPS Speed") );
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(ig), widgets->show_gps_speed );
    g_signal_connect_swapped ( G_OBJECT(ig), "toggled", G_CALLBACK(menu_show_gps_speed_cb), widgets );
    gtk_menu_shell_append ( GTK_MENU_SHELL(show_submenu), ig );
  }

  gtk_widget_show_all ( menu );
  gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time());
}

static void track_graph_click( GtkWidget *event_box, GdkEventButton *event, PropWidgets *widgets, VikPropWinGraphType_t graph_type )
{
  // Only use primary clicks for marker position
  if ( event->button != 1 ) {
    // 'right' click for menu - only on the embedded graphs
    if ( event->button == 3 && widgets->self ) {
      graph_click_menu_popup ( widgets, graph_type );
    }
    return;
  }

  gboolean is_time_graph =
    ( graph_type == PROPWIN_GRAPH_TYPE_SPEED_TIME ||
      graph_type == PROPWIN_GRAPH_TYPE_DISTANCE_TIME ||
      graph_type == PROPWIN_GRAPH_TYPE_ELEVATION_TIME );

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );

  VikTrackpoint *trackpoint = set_center_at_graph_position(event->x, allocation.width, widgets->vtl, widgets->vlp, widgets->vvp, widgets->tr, is_time_graph, widgets->profile_width);
  // Unable to get the point so give up
  if ( trackpoint == NULL ) {
    if ( widgets->dialog )
      gtk_dialog_set_response_sensitive(GTK_DIALOG(widgets->dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, FALSE);
    return;
  }

  widgets->marker_tp = trackpoint;

  GtkWidget *image;
  GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(event_box));
  GtkWidget *graph_box;
  PropSaved *graph_saved_img;
  gdouble pc = NAN;

  // Attempt to redraw marker on all graph types
  gint graphite;
  for ( graphite = PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE;
	graphite < PROPWIN_GRAPH_TYPE_END;
	graphite++ ) {

    // Switch commonal variables to particular graph type
    switch (graphite) {
    default:
    case PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE:
      image           = widgets->elev_image;
      graph_box       = widgets->elev_box;
      graph_saved_img = &widgets->elev_graph_saved_img;
      is_time_graph   = FALSE;
      break;
    case PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE:
      image           = widgets->gradient_image;
      graph_box       = widgets->gradient_box;
      graph_saved_img = &widgets->gradient_graph_saved_img;
      is_time_graph   = FALSE;
      break;
    case PROPWIN_GRAPH_TYPE_SPEED_TIME:
      image           = widgets->speed_image;
      graph_box       = widgets->speed_box;
      graph_saved_img = &widgets->speed_graph_saved_img;
      is_time_graph   = TRUE;
      break;
    case PROPWIN_GRAPH_TYPE_DISTANCE_TIME:
      image           = widgets->dist_image;
      graph_box       = widgets->dist_box;
      graph_saved_img = &widgets->dist_graph_saved_img;
      is_time_graph   = TRUE;
      break;
    case PROPWIN_GRAPH_TYPE_ELEVATION_TIME:
      image           = widgets->elev_time_image;
      graph_box       = widgets->elev_time_box;
      graph_saved_img = &widgets->elev_time_graph_saved_img;
      is_time_graph   = TRUE;
      break;
    case PROPWIN_GRAPH_TYPE_SPEED_DISTANCE:
      image           = widgets->speed_dist_image;
      graph_box       = widgets->speed_dist_box;
      graph_saved_img = &widgets->speed_dist_graph_saved_img;
      is_time_graph   = FALSE;
      break;
    }

    // Commonal method of redrawing marker
    if ( graph_box ) {

      if (is_time_graph)
	pc = tp_percentage_by_time ( widgets->tr, trackpoint );
      else
	pc = tp_percentage_by_distance ( widgets->tr, trackpoint, widgets->track_length_inc_gaps );

      if (!isnan(pc)) {
        gdouble marker_x = (pc * widgets->profile_width) + MARGIN_X;
	save_image_and_draw_graph_marks(image,
					marker_x,
					gtk_widget_get_style(window)->black_gc,
					-1, // Don't draw blob on clicks
					0,
					graph_saved_img,
					widgets->profile_width,
					widgets->profile_height,
					BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
					&widgets->is_marker_drawn,
					&widgets->is_blob_drawn);
      }
    }
  }

  if ( widgets->dialog )
    gtk_dialog_set_response_sensitive(GTK_DIALOG(widgets->dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, widgets->is_marker_drawn);
}

static gboolean track_profile_click( GtkWidget *event_box, GdkEventButton *event, gpointer ptr )
{
  track_graph_click(event_box, event, ptr, PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE);
  return TRUE;  /* don't call other (further) callbacks */
}

static gboolean track_gradient_click( GtkWidget *event_box, GdkEventButton *event, gpointer ptr )
{
  track_graph_click(event_box, event, ptr, PROPWIN_GRAPH_TYPE_GRADIENT_DISTANCE);
  return TRUE;  /* don't call other (further) callbacks */
}

static gboolean track_vt_click( GtkWidget *event_box, GdkEventButton *event, gpointer ptr )
{
  track_graph_click(event_box, event, ptr, PROPWIN_GRAPH_TYPE_SPEED_TIME);
  return TRUE;  /* don't call other (further) callbacks */
}

static gboolean track_dt_click( GtkWidget *event_box, GdkEventButton *event, gpointer ptr )
{
  track_graph_click(event_box, event, ptr, PROPWIN_GRAPH_TYPE_DISTANCE_TIME);
  return TRUE;  /* don't call other (further) callbacks */
}

static gboolean track_et_click( GtkWidget *event_box, GdkEventButton *event, gpointer ptr )
{
  track_graph_click(event_box, event, ptr, PROPWIN_GRAPH_TYPE_ELEVATION_TIME);
  return TRUE;  /* don't call other (further) callbacks */
}

static gboolean track_sd_click( GtkWidget *event_box, GdkEventButton *event, gpointer ptr )
{
  track_graph_click(event_box, event, ptr, PROPWIN_GRAPH_TYPE_SPEED_DISTANCE);
  return TRUE;  /* don't call other (further) callbacks */
}

/**
 * Calculate y position for blob on elevation graph
 */
static gint blobby_altitude ( gdouble x_blob, PropWidgets *widgets )
{
  gint ix = (gint)x_blob;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;

  gint y_blob = widgets->profile_height-widgets->profile_height*(widgets->altitudes[ix]-widgets->draw_min_altitude)/(chunksa[widgets->cia]*LINES);

  return y_blob;
}

/**
 * Calculate y position for blob on gradient graph
 */
static gint blobby_gradient ( gdouble x_blob, PropWidgets *widgets )
{
  gint ix = (gint)x_blob;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;
  if (ix < 0) ix = 0;

  gint y_blob = widgets->profile_height-widgets->profile_height*(widgets->gradients[ix]-widgets->draw_min_gradient)/(chunksg[widgets->cig]*LINES);

  return y_blob;
}

/**
 * Calculate y position for blob on speed graph
 */
static gint blobby_speed ( gdouble x_blob, PropWidgets *widgets )
{
  gint ix = (gint)x_blob;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;
  if (ix < 0) ix = 0;

  gint y_blob = widgets->profile_height-widgets->profile_height*(widgets->speeds[ix]-widgets->draw_min_speed)/(chunkss[widgets->cis]*LINES);

  return y_blob;
}

/**
 * Calculate y position for blob on distance graph
 */
static gint blobby_distance ( gdouble x_blob, PropWidgets *widgets )
{
  gint ix = (gint)x_blob;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;
  if (ix < 0) ix = 0;

  gint y_blob = widgets->profile_height-widgets->profile_height*(widgets->distances[ix])/(chunksd[widgets->cid]*LINES);
  //NB min distance is always 0, so no need to subtract that from this  ______/

  return y_blob;
}

/**
 * Calculate y position for blob on elevation/time graph
 */
static gint blobby_altitude_time ( gdouble x_blob, PropWidgets *widgets )
{
  gint ix = (gint)x_blob;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;
  if (ix < 0) ix = 0;

  gint y_blob = widgets->profile_height-widgets->profile_height*(widgets->ats[ix]-widgets->draw_min_altitude_time)/(chunksa[widgets->ciat]*LINES);
  return y_blob;
}

/**
 * Calculate y position for blob on speed/dist graph
 */
static gint blobby_speed_dist ( gdouble x_blob, PropWidgets *widgets )
{
  gint ix = (gint)x_blob;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;
  if (ix < 0) ix = 0;

  gint y_blob = widgets->profile_height-widgets->profile_height*(widgets->speeds_dist[ix]-widgets->draw_min_speed)/(chunkss[widgets->cisd]*LINES);

  return y_blob;
}

static void get_distance_text ( gchar* buf, guint size, gdouble meters_from_start )
{
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  switch ( dist_units ) {
  case VIK_UNITS_DISTANCE_KILOMETRES:
    g_snprintf ( buf, size, "%.2f km", meters_from_start/1000.0 );
    break;
  case VIK_UNITS_DISTANCE_MILES:
    g_snprintf ( buf, size, "%.2f miles", VIK_METERS_TO_MILES(meters_from_start) );
    break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    g_snprintf ( buf, size, "%.2f NM", VIK_METERS_TO_NAUTICAL_MILES(meters_from_start) );
    break;
  default:
    g_snprintf ( buf, size, "--" );
    g_critical("Houston, we've had a problem. distance=%d", dist_units);
    break;
  }
}

static void get_altitude_text ( gchar* buf, guint size, VikTrackpoint *trackpoint )
{
  if ( isnan(trackpoint->altitude) ) {
    g_snprintf ( buf, size, "--" );
  } else {
    if ( a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET )
      g_snprintf ( buf, size, "%d ft", (int)round(VIK_METERS_TO_FEET(trackpoint->altitude)) );
    else
      g_snprintf ( buf, size, "%d m", (int)round(trackpoint->altitude) );
  }
}

static void track_profile_move( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
{
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );

  gdouble x = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  gdouble meters_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_dist ( widgets->tr, x / widgets->profile_width, &meters_from_start );

  if ( trackpoint && widgets->w_cur_dist ) {
    static gchar tmp_buf[20];
    get_distance_text ( tmp_buf, sizeof(tmp_buf), meters_from_start );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_dist), tmp_buf );
  }

  // Show track elevation for this position - to the nearest whole number
  if ( trackpoint && widgets->w_cur_elevation ) {
    static gchar tmp_buf[20];
    get_altitude_text ( tmp_buf, sizeof(tmp_buf), trackpoint );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_elevation), tmp_buf );
  }

  widgets->blob_tp = trackpoint;

  if ( widgets->altitudes == NULL )
    return;

  GtkWidget *window = gtk_widget_get_toplevel (event_box);
  gint y_blob = blobby_altitude (x, widgets);

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if (widgets->is_marker_drawn) {
    gdouble pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
    if (!isnan(pc)) {
      marker_x = (pc * widgets->profile_width) + MARGIN_X;
    }
  }

  save_image_and_draw_graph_marks (widgets->elev_image,
				   marker_x,
				   gtk_widget_get_style(window)->black_gc,
				   MARGIN_X+x,
				   MARGIN_Y+y_blob,
				   &widgets->elev_graph_saved_img,
				   widgets->profile_width,
				   widgets->profile_height,
				   BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				   &widgets->is_marker_drawn,
				   &widgets->is_blob_drawn);
}

void track_gradient_move( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
{
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );

  gdouble x = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  gdouble meters_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_dist ( widgets->tr, (gdouble) x / widgets->profile_width, &meters_from_start );
  if (trackpoint && widgets->w_cur_gradient_dist) {
    static gchar tmp_buf[20];
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", meters_from_start/1000.0);
      break;
    case VIK_UNITS_DISTANCE_MILES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", VIK_METERS_TO_MILES(meters_from_start) );
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", VIK_METERS_TO_NAUTICAL_MILES(meters_from_start) );
      break;
    default:
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_gradient_dist), tmp_buf);
  }

  // Show track gradient for this position - to the nearest whole number
  if (trackpoint && widgets->w_cur_gradient_gradient) {
    static gchar tmp_buf[20];
    
    double gradient = widgets->gradients[(int) x];

    g_snprintf(tmp_buf, sizeof(tmp_buf), "%d%%", (int)gradient);
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_gradient_gradient), tmp_buf);
  }

  widgets->blob_tp = trackpoint;

  if ( widgets->gradients == NULL )
    return;

  GtkWidget *window = gtk_widget_get_toplevel (event_box);
  gint y_blob = blobby_gradient (x, widgets);

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if (widgets->is_marker_drawn) {
    gdouble pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
    if (!isnan(pc)) {
      marker_x = (pc * widgets->profile_width) + MARGIN_X;
    }
  }

  save_image_and_draw_graph_marks (widgets->gradient_image,
				   marker_x,
				   gtk_widget_get_style(window)->black_gc,
				   MARGIN_X+x,
				   MARGIN_Y+y_blob,
				   &widgets->gradient_graph_saved_img,
				   widgets->profile_width,
				   widgets->profile_height,
				   BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				   &widgets->is_marker_drawn,
				   &widgets->is_blob_drawn);
}

//
static void time_label_update (GtkWidget *widget, gdouble seconds)
{
  static gchar tmp_buf[20];
  time_t seconds_from_start = round ( seconds );
  guint h = seconds_from_start/3600;
  guint m = (seconds_from_start - h*3600)/60;
  guint s = seconds_from_start - (3600*h) - (60*m);
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%02d:%02d:%02d", h, m, s);
  gtk_label_set_text(GTK_LABEL(widget), tmp_buf);
}

//
static void real_time_label_update ( PropWidgets *widgets, GtkWidget *widget, VikTrackpoint *trackpoint)
{
  static gchar tmp_buf[64];
  if ( !isnan(trackpoint->timestamp) ) {
    time_t ts = round ( trackpoint->timestamp );
    // Alternatively could use %c format but I prefer a slightly more compact form here
    //  The full date can of course be seen on the Statistics tab
    strftime (tmp_buf, sizeof(tmp_buf), "%X %x %Z", localtime(&ts));
  }
  else
    g_snprintf (tmp_buf, sizeof(tmp_buf), _("No Data"));
  gtk_label_set_text(GTK_LABEL(widget), tmp_buf);
}

void track_vt_move( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
{
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );
  gdouble x = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  gdouble seconds_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_time ( widgets->tr, (gdouble) x / widgets->profile_width, &seconds_from_start );
  if (trackpoint && widgets->w_cur_time) {
    time_label_update ( widgets->w_cur_time, seconds_from_start );
  }

  if (trackpoint && widgets->w_cur_time_real) {
    real_time_label_update ( widgets, widgets->w_cur_time_real, trackpoint );
  }

  gint ix = (gint)x;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;

  // Show track speed for this position
  if (trackpoint && widgets->w_cur_speed) {
    static gchar tmp_buf[20];
    // Even if GPS speed available (trackpoint->speed), the text will correspond to the speed map shown
    // No conversions needed as already in appropriate units
    vik_units_speed_t speed_units = a_vik_get_units_speed ();
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, widgets->speeds[ix], FALSE, "%.1f" );
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_speed), tmp_buf);
  }

  widgets->blob_tp = trackpoint;

  if ( widgets->speeds == NULL )
    return;

  GtkWidget *window = gtk_widget_get_toplevel (event_box);
  gint y_blob = blobby_speed (x, widgets);

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if (widgets->is_marker_drawn) {
    gdouble pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );
    if (!isnan(pc)) {
      marker_x = (pc * widgets->profile_width) + MARGIN_X;
    }
  }

  save_image_and_draw_graph_marks (widgets->speed_image,
				   marker_x,
				   gtk_widget_get_style(window)->black_gc,
				   MARGIN_X+x,
				   MARGIN_Y+y_blob,
				   &widgets->speed_graph_saved_img,
				   widgets->profile_width,
				   widgets->profile_height,
				   BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				   &widgets->is_marker_drawn,
				   &widgets->is_blob_drawn);
}

/**
 * Update labels and blob marker on mouse moves in the distance/time graph
 */
void track_dt_move( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
{
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );

  gdouble x = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  gdouble seconds_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_time ( widgets->tr, (gdouble) x / widgets->profile_width, &seconds_from_start );
  if (trackpoint && widgets->w_cur_dist_time) {
    time_label_update ( widgets->w_cur_dist_time, seconds_from_start );
  }

  if (trackpoint && widgets->w_cur_dist_time_real) {
    real_time_label_update ( widgets, widgets->w_cur_dist_time_real, trackpoint );
  }

  gint ix = (gint)x;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;

  if (trackpoint && widgets->w_cur_dist_dist) {
    static gchar tmp_buf[20];
    switch ( a_vik_get_units_distance () ) {
    case VIK_UNITS_DISTANCE_MILES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", widgets->distances[ix]);
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", widgets->distances[ix]);
      break;
    default:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", widgets->distances[ix]);
      break;
    }
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_dist_dist), tmp_buf);
  }

  widgets->blob_tp = trackpoint;

  if ( widgets->distances == NULL )
    return;

  GtkWidget *window = gtk_widget_get_toplevel (event_box);
  gint y_blob = blobby_distance (x, widgets);

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if (widgets->is_marker_drawn) {
    gdouble pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );
    if (!isnan(pc)) {
      marker_x = (pc * widgets->profile_width) + MARGIN_X;
    }
  }

  save_image_and_draw_graph_marks (widgets->dist_image,
				   marker_x,
				   gtk_widget_get_style(window)->black_gc,
				   MARGIN_X+x,
				   MARGIN_Y+y_blob,
				   &widgets->dist_graph_saved_img,
				   widgets->profile_width,
				   widgets->profile_height,
				   BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				   &widgets->is_marker_drawn,
				   &widgets->is_blob_drawn);
}

/**
 * Update labels and blob marker on mouse moves in the elevation/time graph
 */
void track_et_move( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
{
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );

  gdouble x = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  gdouble seconds_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_time ( widgets->tr, (gdouble) x / widgets->profile_width, &seconds_from_start );
  if (trackpoint && widgets->w_cur_elev_time) {
    time_label_update ( widgets->w_cur_elev_time, seconds_from_start );
  }

  if (trackpoint && widgets->w_cur_elev_time_real) {
    real_time_label_update ( widgets, widgets->w_cur_elev_time_real, trackpoint );
  }

  gint ix = (gint)x;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;

  if (trackpoint && widgets->w_cur_elev_elev) {
    static gchar tmp_buf[20];
    if (a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET)
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%d ft", (int)VIK_METERS_TO_FEET(trackpoint->altitude));
    else
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%d m", (int)trackpoint->altitude);
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_elev_elev), tmp_buf);
  }

  widgets->blob_tp = trackpoint;

  if ( widgets->ats == NULL )
    return;

  GtkWidget *window = gtk_widget_get_toplevel (event_box);
  gint y_blob = blobby_altitude_time (x, widgets);

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if (widgets->is_marker_drawn) {
    gdouble pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );
    if (!isnan(pc)) {
      marker_x = (pc * widgets->profile_width) + MARGIN_X;
    }
  }

  save_image_and_draw_graph_marks (widgets->elev_time_image,
				   marker_x,
				   gtk_widget_get_style(window)->black_gc,
				   MARGIN_X+x,
				   MARGIN_Y+y_blob,
				   &widgets->elev_time_graph_saved_img,
				   widgets->profile_width,
				   widgets->profile_height,
				   BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				   &widgets->is_marker_drawn,
				   &widgets->is_blob_drawn);
}

void track_sd_move( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
{
  int mouse_x, mouse_y;
  GdkModifierType state;

  if (event->is_hint)
    gdk_window_get_pointer (event->window, &mouse_x, &mouse_y, &state);
  else
    mouse_x = event->x;

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );

  gdouble x = mouse_x - allocation.width / 2 + widgets->profile_width / 2 - MARGIN_X / 2;
  if (x < 0)
    x = 0;
  if (x > widgets->profile_width)
    x = widgets->profile_width;

  gdouble meters_from_start;
  VikTrackpoint *trackpoint = vik_track_get_closest_tp_by_percentage_dist ( widgets->tr, (gdouble) x / widgets->profile_width, &meters_from_start );
  if (trackpoint && widgets->w_cur_speed_dist) {
    static gchar tmp_buf[20];
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", meters_from_start/1000.0);
      break;
    case VIK_UNITS_DISTANCE_MILES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", VIK_METERS_TO_MILES(meters_from_start) );
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", VIK_METERS_TO_NAUTICAL_MILES(meters_from_start) );
      break;
    default:
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_speed_dist), tmp_buf);
  }

  gint ix = (gint)x;
  // Ensure ix is inbounds
  if (ix == widgets->profile_width)
    ix--;

  if ( widgets->speeds_dist == NULL )
    return;

  // Show track speed for this position
  if (widgets->w_cur_speed_speed) {
    static gchar tmp_buf[20];
    // Even if GPS speed available (trackpoint->speed), the text will correspond to the speed map shown
    // No conversions needed as already in appropriate units
    vik_units_speed_t speed_units = a_vik_get_units_speed ();
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, widgets->speeds[ix], FALSE, "%.1f" );
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_speed_speed), tmp_buf);
  }

  widgets->blob_tp = trackpoint;

  GtkWidget *window = gtk_widget_get_toplevel (event_box);
  gint y_blob = blobby_speed_dist (x, widgets);

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if (widgets->is_marker_drawn) {
    gdouble pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
    if (!isnan(pc)) {
      marker_x = (pc * widgets->profile_width) + MARGIN_X;
    }
  }

  save_image_and_draw_graph_marks (widgets->speed_dist_image,
				   marker_x,
				   gtk_widget_get_style(window)->black_gc,
				   MARGIN_X+x,
				   MARGIN_Y+y_blob,
				   &widgets->speed_dist_graph_saved_img,
				   widgets->profile_width,
				   widgets->profile_height,
				   BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				   &widgets->is_marker_drawn,
				   &widgets->is_blob_drawn);
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
				    gdouble max_speed_in,
				    gint cia,
				    gint width,
				    gint height,
				    gint margin,
				    gboolean do_dem,
				    gboolean do_speed)
{
  GList *iter;
  gdouble max_speed = 0;
  gdouble total_length = vik_track_get_length_including_gaps(tr);

  // Calculate the max speed factor
  if (do_speed)
    max_speed = max_speed_in * 110 / 100;

  gdouble dist = 0;
  gint h2 = height + MARGIN_Y; // Adjust height for x axis labelling offset
  gint achunk = chunksa[cia]*LINES;

  for (iter = tr->trackpoints->next; iter; iter = iter->next) {
    int x;
    dist += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
			     &(VIK_TRACKPOINT(iter->prev->data)->coord) );
    x = (width * dist)/total_length + margin;
    if (do_dem) {
      gint16 elev = a_dems_get_elev_by_coord(&(VIK_TRACKPOINT(iter->data)->coord), VIK_DEM_INTERPOL_BEST);
      if ( elev != VIK_DEM_INVALID_ELEVATION ) {
	// Convert into height units
	if (a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET)
	  elev =  VIK_METERS_TO_FEET(elev);
	// No conversion needed if already in metres

        // offset is in current height units
        elev -= alt_offset;

        // consider chunk size
        int y_alt = h2 - ((height * elev)/achunk );
        gdk_draw_rectangle(GDK_DRAWABLE(pix), alt_gc, TRUE, x-2, y_alt-2, 4, 4);
      }
    }
    if (do_speed) {
      // This is just a speed indicator - no actual values can be inferred by user
      if (!isnan(VIK_TRACKPOINT(iter->data)->speed)) {
        int y_speed = h2 - (height * VIK_TRACKPOINT(iter->data)->speed)/max_speed;
        gdk_draw_rectangle(GDK_DRAWABLE(pix), speed_gc, TRUE, x-2, y_speed-2, 4, 4);
      }
    }
  }
}

/**
 * draw_grid_y:
 *
 * A common way to draw the grid with y axis labels
 *
 */
static void draw_grid_y ( GtkWidget *window, GtkWidget *image, PropWidgets *widgets, GdkPixmap *pix, gchar *ss, gint i )
{
  PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(image), NULL);

  pango_layout_set_alignment (pl, PANGO_ALIGN_RIGHT);
  pango_layout_set_font_description (pl, gtk_widget_get_style(window)->font_desc);

  gchar *label_markup = g_strdup_printf ( "<span size=\"small\">%s</span>", ss );
  pango_layout_set_markup ( pl, label_markup, -1 );
  g_free ( label_markup );

  int w, h;
  pango_layout_get_pixel_size ( pl, &w, &h );

  gdk_draw_layout ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->fg_gc[0],
                    MARGIN_X-w-3,
                    CLAMP((int)i*widgets->profile_height/LINES - h/2 + MARGIN_Y, 0, widgets->profile_height-h+MARGIN_Y),
                    pl );
  g_object_unref ( G_OBJECT ( pl ) );

  gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[0],
                  MARGIN_X, MARGIN_Y + widgets->profile_height/LINES * i,
                  MARGIN_X + widgets->profile_width, MARGIN_Y + widgets->profile_height/LINES * i );
}

/**
 * draw_grid_x_time:
 *
 * A common way to draw the grid with x axis labels for time graphs
 *
 */
static void draw_grid_x_time ( GtkWidget *window, GtkWidget *image, PropWidgets *widgets, GdkPixmap *pix, guint ii, guint tt, guint xx )
{
  gchar *label_markup = NULL;
  switch (ii) {
    case 0:
    case 1:
    case 2:
    case 3:
      // Minutes
      label_markup = g_strdup_printf ( "<span size=\"small\">%d %s</span>", tt/60, _("mins") );
      break;
    case 4:
    case 5:
    case 6:
    case 7:
      // Hours
      label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", (gdouble)tt/(60*60), _("h") );
      break;
    case 8:
    case 9:
    case 10:
      // Days
      label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", (gdouble)tt/(60*60*24), _("d") );
      break;
    case 11:
    case 12:
      // Weeks
      label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", (gdouble)tt/(60*60*24*7), _("w") );
      break;
    case 13:
      // 'Months'
      label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", (gdouble)tt/(60*60*24*28), _("M") );
      break;
    default:
      break;
  }
  if ( label_markup ) {

    PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(image), NULL);
    pango_layout_set_font_description (pl, gtk_widget_get_style(window)->font_desc);

    pango_layout_set_markup ( pl, label_markup, -1 );
    g_free ( label_markup );
    int ww, hh;
    pango_layout_get_pixel_size ( pl, &ww, &hh );

    gdk_draw_layout ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->fg_gc[0],
                      MARGIN_X+xx-ww/2, MARGIN_Y/2-hh/2, pl );
    g_object_unref ( G_OBJECT ( pl ) );
  }

  gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[0],
                  MARGIN_X+xx, MARGIN_Y, MARGIN_X+xx, MARGIN_Y+widgets->profile_height );
}

/**
 * draw_grid_x_distance:
 *
 * A common way to draw the grid with x axis labels for distance graphs
 *
 */
static void draw_grid_x_distance ( GtkWidget *window, GtkWidget *image, PropWidgets *widgets, GdkPixmap *pix, guint ii, gdouble dd, guint xx, vik_units_distance_t dist_units )
{
  gchar *label_markup = NULL;
  switch ( dist_units ) {
  case VIK_UNITS_DISTANCE_MILES:
    if ( ii > 4 )
      label_markup = g_strdup_printf ( "<span size=\"small\">%d %s</span>", (guint)dd, _("miles") );
    else
      label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", dd, _("miles") );
    break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    if ( ii > 4 )
      label_markup = g_strdup_printf ( "<span size=\"small\">%d %s</span>", (guint)dd, _("NM") );
    else
      label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", dd, _("NM") );
    break;
  default:
    // VIK_UNITS_DISTANCE_KILOMETRES:
    if ( ii > 4 )
      label_markup = g_strdup_printf ( "<span size=\"small\">%d %s</span>", (guint)dd, _("km") );
    else
      label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", dd, _("km") );
    break;
  }

  if ( label_markup ) {
    PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(image), NULL);
    pango_layout_set_font_description (pl, gtk_widget_get_style(window)->font_desc);

    pango_layout_set_markup ( pl, label_markup, -1 );
    g_free ( label_markup );
    int ww, hh;
    pango_layout_get_pixel_size ( pl, &ww, &hh );

    gdk_draw_layout ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->fg_gc[0],
                      MARGIN_X+xx-ww/2, MARGIN_Y/2-hh/2, pl );
    g_object_unref ( G_OBJECT ( pl ) );
  }

  gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[0],
                  MARGIN_X+xx, MARGIN_Y, MARGIN_X+xx, MARGIN_Y+widgets->profile_height );
}

/**
 * clear the images (scale texts & actual graph)
 */
static void clear_images (GdkPixmap *pix, GtkWidget *window, PropWidgets *widgets)
{
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->bg_gc[0],
                     TRUE, 0, 0, widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y);
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->mid_gc[0],
                     TRUE, 0, 0, widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y);
}

/**
 *
 */
static void draw_distance_divisions ( GtkWidget *window, GtkWidget *image, GdkPixmap *pix, PropWidgets *widgets, vik_units_distance_t dist_units )
{
  // Set to display units from length in metres.
  gdouble length = widgets->track_length_inc_gaps;
  switch (dist_units) {
    case VIK_UNITS_DISTANCE_MILES:
      length = VIK_METERS_TO_MILES(length);
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      length = VIK_METERS_TO_NAUTICAL_MILES(length);
      break;
    default:
      // KM
      length = length/1000.0;
      break;
  }
  guint index = get_distance_chunk_index ( length );
  gdouble dist_per_pixel = length/widgets->profile_width;

  for (guint i=1; chunksd[index]*i <= length; i++) {
    draw_grid_x_distance ( window, image, widgets, pix, index, chunksd[index]*i, (guint)(chunksd[index]*i/dist_per_pixel), dist_units );
  }
}

/**
 * Draw just the height profile image
 */
static void draw_elevations (GtkWidget *image, VikTrack *tr, PropWidgets *widgets )
{
  guint i;

  GdkGC *no_alt_info;
  GdkColor color;

  // Free previous allocation
  if ( widgets->altitudes )
    g_free ( widgets->altitudes );

  widgets->altitudes = vik_track_make_elevation_map ( tr, widgets->profile_width );

  if ( widgets->altitudes == NULL )
    return;

  // Convert into appropriate units
  vik_units_height_t height_units = a_vik_get_units_height ();
  if ( height_units == VIK_UNITS_HEIGHT_FEET ) {
    // Convert altitudes into feet units
    for ( i = 0; i < widgets->profile_width; i++ ) {
      widgets->altitudes[i] = VIK_METERS_TO_FEET(widgets->altitudes[i]);
    }
  }
  // Otherwise leave in metres

  minmax_array(widgets->altitudes, &widgets->min_altitude, &widgets->max_altitude, TRUE, widgets->profile_width);

  get_new_min_and_chunk_index (widgets->min_altitude, widgets->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &widgets->draw_min_altitude, &widgets->cia);

  // Assign locally
  gdouble mina = widgets->draw_min_altitude;

  GtkWidget *window = gtk_widget_get_toplevel (widgets->elev_box);
  GdkPixmap *pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  no_alt_info = gdk_gc_new ( gtk_widget_get_window(window) );
  gdk_color_parse ( "yellow", &color );
  gdk_gc_set_rgb_fg_color ( no_alt_info, &color);

  // Reset before redrawing
  clear_images (pix, window, widgets);

  /* draw grid */
  for (i=0; i<=LINES; i++) {
    gchar s[32];

    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      sprintf(s, "%8dm", (int)(mina + (LINES-i)*chunksa[widgets->cia]));
      break;
    case VIK_UNITS_HEIGHT_FEET:
      // NB values already converted into feet
      sprintf(s, "%8dft", (int)(mina + (LINES-i)*chunksa[widgets->cia]));
      break;
    default:
      sprintf(s, "--");
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }

    draw_grid_y ( window, image, widgets, pix, s, i );
  }

  draw_distance_divisions ( window, image, pix, widgets, a_vik_get_units_distance() );

  /* draw elevations */
  guint height = MARGIN_Y+widgets->profile_height;
  for ( i = 0; i < widgets->profile_width; i++ )
    if ( isnan(widgets->altitudes[i]) )
      gdk_draw_line ( GDK_DRAWABLE(pix), no_alt_info, 
                      i + MARGIN_X, MARGIN_Y, i + MARGIN_X, height );
    else 
      gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
                      i + MARGIN_X, height, i + MARGIN_X, height-widgets->profile_height*(widgets->altitudes[i]-mina)/(chunksa[widgets->cia]*LINES) );

  if ( widgets->show_dem || widgets->show_alt_gps_speed ) {

    GdkGC *dem_alt_gc = gdk_gc_new ( gtk_widget_get_window(window) );
    GdkGC *gps_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );

    gdk_color_parse ( "green", &color );
    gdk_gc_set_rgb_fg_color ( dem_alt_gc, &color);

    gdk_color_parse ( "red", &color );
    gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

    // Ensure somekind of max speed when not set
    if ( widgets->max_speed < 0.01 )
      widgets->max_speed = vik_track_get_max_speed(tr);

    draw_dem_alt_speed_dist(tr,
			    GDK_DRAWABLE(pix),
			    dem_alt_gc,
			    gps_speed_gc,
			    mina,
			    widgets->max_altitude - mina,
			    widgets->max_speed,
			    widgets->cia,
			    widgets->profile_width,
			    widgets->profile_height,
			    MARGIN_X,
                            widgets->show_dem,
                            widgets->show_alt_gps_speed);
    
    g_object_unref ( G_OBJECT(dem_alt_gc) );
    g_object_unref ( G_OBJECT(gps_speed_gc) );
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, FALSE, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );
  g_object_unref ( G_OBJECT(no_alt_info) );
}

/**
 * Draws representative speed on the supplied pixmap
 *   (which is the gradients graph)
 */
static void draw_speed_dist(VikTrack *tr,
				    GdkDrawable *pix,
				    GdkGC *speed_gc,
				    gdouble max_speed_in,
				    gint width,
				    gint height,
				    gint margin,
				    gboolean do_speed)
{
  GList *iter;
  gdouble max_speed = 0;
  gdouble total_length = vik_track_get_length_including_gaps(tr);

  // Calculate the max speed factor
  if (do_speed)
    max_speed = max_speed_in * 110 / 100;

  gdouble dist = 0;
  for (iter = tr->trackpoints->next; iter; iter = iter->next) {
    int x;
    dist += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
			     &(VIK_TRACKPOINT(iter->prev->data)->coord) );
    x = (width * dist)/total_length + MARGIN_X;
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
 * Draw just the gradient image
 */
static void draw_gradients (GtkWidget *image, VikTrack *tr, PropWidgets *widgets )
{
  guint i;

  // Free previous allocation
  if ( widgets->gradients )
    g_free ( widgets->gradients );

  widgets->gradients = vik_track_make_gradient_map ( tr, widgets->profile_width );

  if ( widgets->gradients == NULL )
    return;

  minmax_array(widgets->gradients, &widgets->min_gradient, &widgets->max_gradient, TRUE, widgets->profile_width);

  get_new_min_and_chunk_index (widgets->min_gradient, widgets->max_gradient, chunksg, G_N_ELEMENTS(chunksg), &widgets->draw_min_gradient, &widgets->cig);

  // Assign locally
  gdouble mina = widgets->draw_min_gradient;

  GtkWidget *window = gtk_widget_get_toplevel (widgets->gradient_box);
  GdkPixmap *pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  // Reset before redrawing
  clear_images (pix, window, widgets);

  /* draw grid */
  for (i=0; i<=LINES; i++) {
    gchar s[32];

    sprintf(s, "%8d%%", (int)(mina + (LINES-i)*chunksg[widgets->cig]));

    draw_grid_y ( window, image, widgets, pix, s, i );
  }

  draw_distance_divisions ( window, image, pix, widgets, a_vik_get_units_distance() );

  /* draw gradients */
  guint height = widgets->profile_height + MARGIN_Y;
  for ( i = 0; i < widgets->profile_width; i++ )
    gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
                    i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->gradients[i]-mina)/(chunksg[widgets->cig]*LINES) );

  if ( gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widgets->w_show_gradient_gps_speed)) ) {
    GdkGC *gps_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );

    GdkColor color;
    gdk_color_parse ( "red", &color );
    gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

    // Ensure somekind of max speed when not set
    if ( widgets->max_speed < 0.01 )
      widgets->max_speed = vik_track_get_max_speed(tr);

    draw_speed_dist(tr,
			    GDK_DRAWABLE(pix),
			    gps_speed_gc,
			    widgets->max_speed,
			    widgets->profile_width,
			    widgets->profile_height,
			    MARGIN_X,
			    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed)));
    
    g_object_unref ( G_OBJECT(gps_speed_gc) );
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, FALSE, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );
}

static void draw_time_lines ( GtkWidget *window, GtkWidget *image, GdkPixmap *pix, PropWidgets *widgets )
{
  guint index = get_time_chunk_index ( widgets->duration );
  gdouble time_per_pixel = (gdouble)(widgets->duration)/widgets->profile_width;

  // If stupidly long track in time - don't bother trying to draw grid lines
  if ( widgets->duration > chunkst[G_N_ELEMENTS(chunkst)-1]*LINES*LINES )
    return;

  for (guint i=1; chunkst[index]*i <= widgets->duration; i++) {
    draw_grid_x_time ( window, image, widgets, pix, index, chunkst[index]*i, (guint)(chunkst[index]*i/time_per_pixel) );
  }
}

/**
 * Draw just the speed (velocity)/time image
 */
static void draw_vt ( GtkWidget *image, VikTrack *tr, PropWidgets *widgets )
{
  guint i;

  // Free previous allocation
  if ( widgets->speeds )
    g_free ( widgets->speeds );

  widgets->speeds = vik_track_make_speed_map ( tr, widgets->profile_width );
  if ( widgets->speeds == NULL )
    return;

  widgets->duration = vik_track_get_duration ( tr, TRUE );
  // Negative time or other problem
  if ( widgets->duration <= 0 )
    return;

  // Convert into appropriate units
  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  for ( i = 0; i < widgets->profile_width; i++ ) {
    widgets->speeds[i] = vu_speed_convert ( speed_units, widgets->speeds[i] );
  }

  GtkWidget *window = gtk_widget_get_toplevel (widgets->speed_box);
  GdkPixmap *pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  minmax_array(widgets->speeds, &widgets->min_speed, &widgets->max_speed, FALSE, widgets->profile_width);
  if (widgets->min_speed < 0.0)
    widgets->min_speed = 0; /* splines sometimes give negative speeds */

  /* Find suitable chunk index */
  get_new_min_and_chunk_index (widgets->min_speed, widgets->max_speed, chunkss, G_N_ELEMENTS(chunkss), &widgets->draw_min_speed, &widgets->cis);

  // Assign locally
  gdouble mins = widgets->draw_min_speed;

  // Reset before redrawing
  clear_images (pix, window, widgets);

  /* draw grid */
  for (i=0; i<=LINES; i++) {
    gchar s[32];

    // NB: No need to convert here anymore as numbers are in the appropriate units
    switch (speed_units) {
    case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
      sprintf(s, "%8dkm/h", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    case VIK_UNITS_SPEED_MILES_PER_HOUR:
      sprintf(s, "%8dmph", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    case VIK_UNITS_SPEED_METRES_PER_SECOND:
      sprintf(s, "%8dm/s", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    case VIK_UNITS_SPEED_KNOTS:
      sprintf(s, "%8dknots", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    case VIK_UNITS_SPEED_SECONDS_PER_KM:
      sprintf(s, "%8ds/km", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    case VIK_UNITS_SPEED_MINUTES_PER_KM:
      sprintf(s, "%8dmin/km", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    case VIK_UNITS_SPEED_SECONDS_PER_MILE:
      sprintf(s, "%8dsec/mi", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    case VIK_UNITS_SPEED_MINUTES_PER_MILE:
      sprintf(s, "%8dmin/mi", (int)(mins + (LINES-i)*chunkss[widgets->cis]));
      break;
    default:
      sprintf(s, "--");
      g_critical("Houston, we've had a problem. speed=%d", speed_units);
    }

    draw_grid_y ( window, image, widgets, pix, s, i );
  }

  draw_time_lines ( window, image, pix, widgets );

  /* draw speeds */
  guint height = widgets->profile_height + MARGIN_Y;
  for ( i = 0; i < widgets->profile_width; i++ )
    gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
                    i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->speeds[i]-mins)/(chunkss[widgets->cis]*LINES) );

  if ( widgets->show_gps_speed ) {

    GdkGC *gps_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );
    GdkColor color;
    gdk_color_parse ( "red", &color );
    gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

    gdouble beg_time = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
    gdouble dur = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp - beg_time;

    GList *iter;
    for (iter = tr->trackpoints; iter; iter = iter->next) {
      gdouble gps_speed = VIK_TRACKPOINT(iter->data)->speed;
      if (isnan(gps_speed))
        continue;

      gps_speed = vu_speed_convert ( speed_units, gps_speed );

      int x = MARGIN_X + widgets->profile_width * (VIK_TRACKPOINT(iter->data)->timestamp - beg_time) / dur;
      int y = height - widgets->profile_height*(gps_speed - mins)/(chunkss[widgets->cis]*LINES);
      gdk_draw_rectangle(GDK_DRAWABLE(pix), gps_speed_gc, TRUE, x-2, y-2, 4, 4);
    }
    g_object_unref ( G_OBJECT(gps_speed_gc) );
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, FALSE, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );
}

/**
 * Draw just the distance/time image
 */
static void draw_dt ( GtkWidget *image, VikTrack *tr, PropWidgets *widgets )
{
  guint i;

  // Free previous allocation
  if ( widgets->distances )
    g_free ( widgets->distances );

  widgets->distances = vik_track_make_distance_map ( tr, widgets->profile_width );
  if ( widgets->distances == NULL )
    return;

  // Convert into appropriate units
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  switch ( dist_units ) {
    case VIK_UNITS_DISTANCE_MILES:
      for ( i = 0; i < widgets->profile_width; i++ ) {
        widgets->distances[i] = VIK_METERS_TO_MILES(widgets->distances[i]);
      }
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      for ( i = 0; i < widgets->profile_width; i++ ) {
        widgets->distances[i] = VIK_METERS_TO_NAUTICAL_MILES(widgets->distances[i]);
      }
      break;
    default:
      // Metres - but want in kms
      for ( i = 0; i < widgets->profile_width; i++ ) {
        widgets->distances[i] = widgets->distances[i]/1000.0;
      }
      break;
  }

  widgets->duration = vik_track_get_duration ( widgets->tr, TRUE );
  // Negative time or other problem
  if ( widgets->duration <= 0 )
    return;

  GtkWidget *window = gtk_widget_get_toplevel (widgets->dist_box);
  GdkPixmap *pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  // easy to work out min / max of distance!
  // Assign locally
  // mind = 0.0; - Thus not used
  gdouble maxd;
  switch ( dist_units ) {
  case VIK_UNITS_DISTANCE_MILES:
    maxd = VIK_METERS_TO_MILES(vik_track_get_length_including_gaps (tr));
    break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    maxd = VIK_METERS_TO_NAUTICAL_MILES(vik_track_get_length_including_gaps (tr));
    break;
  default:
    maxd = vik_track_get_length_including_gaps (tr) / 1000.0;
    break;
  }

  /* Find suitable chunk index */
  gdouble dummy = 0.0; // expect this to remain the same! (not that it's used)
  get_new_min_and_chunk_index (0, maxd, chunksd, G_N_ELEMENTS(chunksd), &dummy, &widgets->cid);

  // Reset before redrawing
  clear_images (pix, window, widgets);

  /* draw grid */
  for (i=0; i<=LINES; i++) {
    gchar s[32];

    switch ( dist_units ) {
    case VIK_UNITS_DISTANCE_MILES:
      sprintf(s, _("%.1f miles"), ((LINES-i)*chunksd[widgets->cid]));
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      sprintf(s, _("%.1f NM"), ((LINES-i)*chunksd[widgets->cid]));
      break;
    default:
      sprintf(s, _("%.1f km"), ((LINES-i)*chunksd[widgets->cid]));
      break;
    }

    draw_grid_y ( window, image, widgets, pix, s, i );
  }
  
  draw_time_lines ( window, image, pix, widgets );

  /* draw distance */
  guint height = widgets->profile_height + MARGIN_Y;
  for ( i = 0; i < widgets->profile_width; i++ )
    gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
                    i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->distances[i])/(chunksd[widgets->cid]*LINES) );

  // Show speed indicator
  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_dist_speed)) ) {
    GdkGC *dist_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );
    GdkColor color;
    gdk_color_parse ( "red", &color );
    gdk_gc_set_rgb_fg_color ( dist_speed_gc, &color);

    gdouble max_speed = 0;
    max_speed = widgets->max_speed * 110 / 100;

    // This is just an indicator - no actual values can be inferred by user
    for ( i = 0; i < widgets->profile_width; i++ ) {
      int y_speed = widgets->profile_height - (widgets->profile_height * widgets->speeds[i])/max_speed;
      gdk_draw_rectangle(GDK_DRAWABLE(pix), dist_speed_gc, TRUE, i+MARGIN_X-2, y_speed-2, 4, 4);
    }
    g_object_unref ( G_OBJECT(dist_speed_gc) );
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, FALSE, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );

}

/**
 * Draw just the elevation/time image
 */
static void draw_et ( GtkWidget *image, VikTrack *tr, PropWidgets *widgets )
{
  guint i;

  // Free previous allocation
  if ( widgets->ats )
    g_free ( widgets->ats );

  widgets->ats = vik_track_make_elevation_time_map ( tr, widgets->profile_width );

  if ( widgets->ats == NULL )
    return;

  // Convert into appropriate units
  vik_units_height_t height_units = a_vik_get_units_height ();
  if ( height_units == VIK_UNITS_HEIGHT_FEET ) {
    // Convert altitudes into feet units
    for ( i = 0; i < widgets->profile_width; i++ ) {
      widgets->ats[i] = VIK_METERS_TO_FEET(widgets->ats[i]);
    }
  }
  // Otherwise leave in metres

  minmax_array(widgets->ats, &widgets->min_altitude, &widgets->max_altitude, TRUE, widgets->profile_width);

  get_new_min_and_chunk_index (widgets->min_altitude, widgets->max_altitude, chunksa, G_N_ELEMENTS(chunksa), &widgets->draw_min_altitude_time, &widgets->ciat);

  // Assign locally
  gdouble mina = widgets->draw_min_altitude_time;

  widgets->duration = vik_track_get_duration ( widgets->tr, TRUE );
  // Negative time or other problem
  if ( widgets->duration <= 0 )
    return;

  GtkWidget *window = gtk_widget_get_toplevel (widgets->elev_time_box);
  GdkPixmap *pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  // Reset before redrawing
  clear_images (pix, window, widgets);

  /* draw grid */
  for (i=0; i<=LINES; i++) {
    gchar s[32];

    switch (height_units) {
    case VIK_UNITS_HEIGHT_METRES:
      sprintf(s, "%8dm", (int)(mina + (LINES-i)*chunksa[widgets->ciat]));
      break;
    case VIK_UNITS_HEIGHT_FEET:
      // NB values already converted into feet
      sprintf(s, "%8dft", (int)(mina + (LINES-i)*chunksa[widgets->ciat]));
      break;
    default:
      sprintf(s, "--");
      g_critical("Houston, we've had a problem. height=%d", height_units);
    }

    draw_grid_y ( window, image, widgets, pix, s, i );
  }

  draw_time_lines ( window, image, pix, widgets );

  /* draw elevations */
  guint height = widgets->profile_height + MARGIN_Y;
  for ( i = 0; i < widgets->profile_width; i++ )
    gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
                    i + MARGIN_X, height, i + MARGIN_X, height-widgets->profile_height*(widgets->ats[i]-mina)/(chunksa[widgets->ciat]*LINES) );

  // Show DEMS
  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_elev_dem)) )  {
    GdkColor color;
    GdkGC *dem_alt_gc = gdk_gc_new ( gtk_widget_get_window(window) );
    gdk_color_parse ( "green", &color );
    gdk_gc_set_rgb_fg_color ( dem_alt_gc, &color);

    gint h2 = widgets->profile_height + MARGIN_Y; // Adjust height for x axis labelling offset
    gint achunk = chunksa[widgets->ciat]*LINES;

    for ( i = 0; i < widgets->profile_width; i++ ) {
      // This could be slow doing this each time...
      VikTrackpoint *tp = vik_track_get_closest_tp_by_percentage_time ( widgets->tr, ((gdouble)i/(gdouble)widgets->profile_width), NULL );
      if ( tp ) {
        gint16 elev = a_dems_get_elev_by_coord(&(tp->coord), VIK_DEM_INTERPOL_SIMPLE);
        if ( elev != VIK_DEM_INVALID_ELEVATION ) {
          // Convert into height units
          if ( a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET )
            elev = VIK_METERS_TO_FEET(elev);
          // No conversion needed if already in metres

          // offset is in current height units
          elev -= mina;

          // consider chunk size
          int y_alt = h2 - ((widgets->profile_height * elev)/achunk );
          gdk_draw_rectangle(GDK_DRAWABLE(pix), dem_alt_gc, TRUE, i+MARGIN_X-2, y_alt-2, 4, 4);
        }
      }
    }
    g_object_unref ( G_OBJECT(dem_alt_gc) );
  }

  // Show speeds
  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_elev_speed)) ) {
    GdkColor color;
    // This is just an indicator - no actual values can be inferred by user
    GdkGC *elev_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );
    gdk_color_parse ( "red", &color );
    gdk_gc_set_rgb_fg_color ( elev_speed_gc, &color);

    gdouble max_speed = widgets->max_speed * 110 / 100;

    for ( i = 0; i < widgets->profile_width; i++ ) {
      int y_speed = widgets->profile_height - (widgets->profile_height * widgets->speeds[i])/max_speed;
      gdk_draw_rectangle(GDK_DRAWABLE(pix), elev_speed_gc, TRUE, i+MARGIN_X-2, y_speed-2, 4, 4);
    }

    g_object_unref ( G_OBJECT(elev_speed_gc) );
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, FALSE, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );
}

/**
 * Draw just the speed/distance image
 */
static void draw_sd ( GtkWidget *image, VikTrack *tr, PropWidgets *widgets)
{
  gdouble mins;
  guint i;

  // Free previous allocation
  if ( widgets->speeds_dist )
    g_free ( widgets->speeds_dist );

  widgets->speeds_dist = vik_track_make_speed_dist_map ( tr, widgets->profile_width );
  if ( widgets->speeds_dist == NULL )
    return;

  // Convert into appropriate units
  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  for ( i = 0; i < widgets->profile_width; i++ ) {
    widgets->speeds_dist[i] = vu_speed_convert ( speed_units, widgets->speeds_dist[i] );
  }

  GtkWidget *window = gtk_widget_get_toplevel (widgets->speed_dist_box);
  GdkPixmap *pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );

  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );

  // OK to resuse min_speed here
  minmax_array(widgets->speeds_dist, &widgets->min_speed, &widgets->max_speed_dist, FALSE, widgets->profile_width);
  if (widgets->min_speed < 0.0)
    widgets->min_speed = 0; /* splines sometimes give negative speeds */

  /* Find suitable chunk index */
  get_new_min_and_chunk_index (widgets->min_speed, widgets->max_speed_dist, chunkss, G_N_ELEMENTS(chunkss), &widgets->draw_min_speed, &widgets->cisd);

  // Assign locally
  mins = widgets->draw_min_speed;
  
  // Reset before redrawing
  clear_images (pix, window, widgets);

  /* draw grid */
  for (i=0; i<=LINES; i++) {
    gchar s[32];

    // NB: No need to convert here anymore as numbers are in the appropriate units
    switch (speed_units) {
    case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
      sprintf(s, "%8dkm/h", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    case VIK_UNITS_SPEED_MILES_PER_HOUR:
      sprintf(s, "%8dmph", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    case VIK_UNITS_SPEED_METRES_PER_SECOND:
      sprintf(s, "%8dm/s", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    case VIK_UNITS_SPEED_KNOTS:
      sprintf(s, "%8dknots", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    case VIK_UNITS_SPEED_SECONDS_PER_KM:
      sprintf(s, "%8ds/km", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    case VIK_UNITS_SPEED_MINUTES_PER_KM:
      sprintf(s, "%8dmin/km", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    case VIK_UNITS_SPEED_SECONDS_PER_MILE:
      sprintf(s, "%8dsec/mi", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    case VIK_UNITS_SPEED_MINUTES_PER_MILE:
      sprintf(s, "%8dmin/mi", (int)(mins + (LINES-i)*chunkss[widgets->cisd]));
      break;
    default:
      sprintf(s, "--");
      g_critical("Houston, we've had a problem. speed=%d", speed_units);
    }

    draw_grid_y ( window, image, widgets, pix, s, i );
  }

  draw_distance_divisions ( window, image, pix, widgets, a_vik_get_units_distance() );

  /* draw speeds */
  guint height = widgets->profile_height + MARGIN_Y;
  for ( i = 0; i < widgets->profile_width; i++ )
    gdk_draw_line ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->dark_gc[3],
                    i + MARGIN_X, height, i + MARGIN_X, height - widgets->profile_height*(widgets->speeds_dist[i]-mins)/(chunkss[widgets->cisd]*LINES) );


  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets->w_show_sd_gps_speed)) ) {

    GdkGC *gps_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );
    GdkColor color;
    gdk_color_parse ( "red", &color );
    gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

    gdouble dist = vik_track_get_length_including_gaps(tr);
    gdouble dist_tp = 0.0;

    GList *iter = tr->trackpoints;
    for (iter = iter->next; iter; iter = iter->next) {
      gdouble gps_speed = VIK_TRACKPOINT(iter->data)->speed;
      if (isnan(gps_speed))
        continue;

      gps_speed = vu_speed_convert ( speed_units, gps_speed );

      dist_tp += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord), &(VIK_TRACKPOINT(iter->prev->data)->coord) );
      int x = MARGIN_X + (widgets->profile_width * dist_tp / dist);
      int y = height - widgets->profile_height*(gps_speed - mins)/(chunkss[widgets->cisd]*LINES);
      gdk_draw_rectangle(GDK_DRAWABLE(pix), gps_speed_gc, TRUE, x-2, y-2, 4, 4);
    }
    g_object_unref ( G_OBJECT(gps_speed_gc) );
  }

  /* draw border */
  gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, FALSE, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1);

  g_object_unref ( G_OBJECT(pix) );
}
#undef LINES

/**
 * Draw all graphs
 */
static void draw_all_graphs ( GtkWidget *widget, PropWidgets *widgets, gboolean resized )
{
  // Draw graphs even if they are not visible
  GtkWidget *window = gtk_widget_get_toplevel(widget);
  gdouble pc = NAN;
  gdouble pc_blob = NAN;

  // Draw elevations
  if (widgets->elev_box != NULL) {

    // Saved image no longer any good as we've resized, so we remove it here
    if (resized && widgets->elev_graph_saved_img.img) {
      g_object_unref(widgets->elev_graph_saved_img.img);
      widgets->elev_graph_saved_img.img = NULL;
      widgets->elev_graph_saved_img.saved = FALSE;
    }

    draw_elevations ( widgets->elev_image, widgets->tr, widgets );

    // Ensure marker or blob are redrawn if necessary
    if (widgets->is_marker_drawn || widgets->is_blob_drawn) {

      pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
      gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
      gint y_blob = 0;
      if (widgets->is_blob_drawn) {
	pc_blob = tp_percentage_by_distance ( widgets->tr, widgets->blob_tp, widgets->track_length_inc_gaps );
	if (!isnan(pc_blob)) {
	  x_blob = (pc_blob * widgets->profile_width);
	}
	y_blob = blobby_altitude (x_blob, widgets);
      }

      gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
      if (!isnan(pc)) {
        marker_x = (pc * widgets->profile_width) + MARGIN_X;
      }

      save_image_and_draw_graph_marks (widgets->elev_image,
				       marker_x,
				       gtk_widget_get_style(window)->black_gc,
				       x_blob+MARGIN_X,
				       y_blob+MARGIN_Y,
				       &widgets->elev_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				       &widgets->is_marker_drawn,
				       &widgets->is_blob_drawn);
    }
  }

  // Draw gradients
  if (widgets->gradient_box != NULL) {

    // Saved image no longer any good as we've resized, so we remove it here
    if (resized && widgets->gradient_graph_saved_img.img) {
      g_object_unref(widgets->gradient_graph_saved_img.img);
      widgets->gradient_graph_saved_img.img = NULL;
      widgets->gradient_graph_saved_img.saved = FALSE;
    }

    draw_gradients ( widgets->gradient_image, widgets->tr, widgets );

    // Ensure marker or blob are redrawn if necessary
    if (widgets->is_marker_drawn || widgets->is_blob_drawn) {

      pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
      gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
      gint y_blob = 0;
      if (widgets->is_blob_drawn) {
	pc_blob = tp_percentage_by_distance ( widgets->tr, widgets->blob_tp, widgets->track_length_inc_gaps );
	if (!isnan(pc_blob)) {
	  x_blob = (pc_blob * widgets->profile_width);
	}
	y_blob = blobby_gradient (x_blob, widgets);
      }

      gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
      if (!isnan(pc)) {
        marker_x = (pc * widgets->profile_width) + MARGIN_X;
      }

      save_image_and_draw_graph_marks (widgets->gradient_image,
				       marker_x,
				       gtk_widget_get_style(window)->black_gc,
				       x_blob+MARGIN_X,
				       y_blob+MARGIN_Y,
				       &widgets->gradient_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				       &widgets->is_marker_drawn,
				       &widgets->is_blob_drawn);
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

    draw_vt ( widgets->speed_image, widgets->tr, widgets );

    // Ensure marker or blob are redrawn if necessary
    if (widgets->is_marker_drawn || widgets->is_blob_drawn) {

      pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );

      gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
      gint    y_blob = 0;
      if (widgets->is_blob_drawn) {
	pc_blob = tp_percentage_by_time ( widgets->tr, widgets->blob_tp );
	if (!isnan(pc_blob)) {
	  x_blob = (pc_blob * widgets->profile_width);
	}
	 
	y_blob = blobby_speed (x_blob, widgets);
      }

      gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
      if (!isnan(pc)) {
        marker_x = (pc * widgets->profile_width) + MARGIN_X;
      }

      save_image_and_draw_graph_marks (widgets->speed_image,
				       marker_x,
				       gtk_widget_get_style(window)->black_gc,
				       x_blob+MARGIN_X,
				       y_blob+MARGIN_Y,
				       &widgets->speed_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				       &widgets->is_marker_drawn,
				       &widgets->is_blob_drawn);
    }
  }

  // Draw Distances
  if (widgets->dist_box != NULL) {

    // Saved image no longer any good as we've resized
    if (resized && widgets->dist_graph_saved_img.img) {
      g_object_unref(widgets->dist_graph_saved_img.img);
      widgets->dist_graph_saved_img.img = NULL;
      widgets->dist_graph_saved_img.saved = FALSE;
    }

    draw_dt ( widgets->dist_image, widgets->tr, widgets );

    // Ensure marker or blob are redrawn if necessary
    if (widgets->is_marker_drawn || widgets->is_blob_drawn) {

      pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );

      gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
      gint    y_blob = 0;
      if (widgets->is_blob_drawn) {
	pc_blob = tp_percentage_by_time ( widgets->tr, widgets->blob_tp );
	if (!isnan(pc_blob)) {
	  x_blob = (pc_blob * widgets->profile_width);
	}
	 
	y_blob = blobby_distance (x_blob, widgets);
      }

      gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
      if (!isnan(pc)) {
        marker_x = (pc * widgets->profile_width) + MARGIN_X;
      }

      save_image_and_draw_graph_marks (widgets->dist_image,
				       marker_x,
				       gtk_widget_get_style(window)->black_gc,
				       x_blob+MARGIN_X,
				       y_blob+MARGIN_Y,
				       &widgets->dist_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				       &widgets->is_marker_drawn,
				       &widgets->is_blob_drawn);
    }
  }

  // Draw Elevations in timely manner
  if (widgets->elev_time_box != NULL) {

    // Saved image no longer any good as we've resized
    if (resized && widgets->elev_time_graph_saved_img.img) {
      g_object_unref(widgets->elev_time_graph_saved_img.img);
      widgets->elev_time_graph_saved_img.img = NULL;
      widgets->elev_time_graph_saved_img.saved = FALSE;
    }

    draw_et ( widgets->elev_time_image, widgets->tr, widgets );

    // Ensure marker or blob are redrawn if necessary
    if (widgets->is_marker_drawn || widgets->is_blob_drawn) {

      pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );

      gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
      gint    y_blob = 0;
      if (widgets->is_blob_drawn) {
	pc_blob = tp_percentage_by_time ( widgets->tr, widgets->blob_tp );
	if (!isnan(pc_blob)) {
	  x_blob = (pc_blob * widgets->profile_width);
	}
	y_blob = blobby_altitude_time (x_blob, widgets);
      }

      gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
      if (!isnan(pc)) {
        marker_x = (pc * widgets->profile_width) + MARGIN_X;
      }

      save_image_and_draw_graph_marks (widgets->elev_time_image,
				       marker_x,
				       gtk_widget_get_style(window)->black_gc,
				       x_blob+MARGIN_X,
				       y_blob+MARGIN_Y,
				       &widgets->elev_time_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				       &widgets->is_marker_drawn,
				       &widgets->is_blob_drawn);
    }
  }

  // Draw speed distances
  if (widgets->speed_dist_box != NULL) {

    // Saved image no longer any good as we've resized, so we remove it here
    if (resized && widgets->speed_dist_graph_saved_img.img) {
      g_object_unref(widgets->speed_dist_graph_saved_img.img);
      widgets->speed_dist_graph_saved_img.img = NULL;
      widgets->speed_dist_graph_saved_img.saved = FALSE;
    }

    draw_sd ( widgets->speed_dist_image, widgets->tr, widgets );

    // Ensure marker or blob are redrawn if necessary
    if (widgets->is_marker_drawn || widgets->is_blob_drawn) {

      pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
      gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
      gint y_blob = 0;
      if (widgets->is_blob_drawn) {
	pc_blob = tp_percentage_by_distance ( widgets->tr, widgets->blob_tp, widgets->track_length_inc_gaps );
	if (!isnan(pc_blob)) {
	  x_blob = (pc_blob * widgets->profile_width);
	}
	y_blob = blobby_speed_dist (x_blob, widgets);
      }

      gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
      if (!isnan(pc)) {
        marker_x = (pc * widgets->profile_width) + MARGIN_X;
      }

      save_image_and_draw_graph_marks (widgets->speed_dist_image,
				       marker_x,
				       gtk_widget_get_style(window)->black_gc,
				       x_blob+MARGIN_X,
				       y_blob+MARGIN_Y,
				       &widgets->speed_dist_graph_saved_img,
				       widgets->profile_width,
				       widgets->profile_height,
				       BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
				       &widgets->is_marker_drawn,
				       &widgets->is_blob_drawn);
    }
  }

}

/**
 * Configure/Resize the profile & speed/time images
 */
static gboolean configure_event ( GtkWidget *widget, GdkEventConfigure *event, PropWidgets *widgets )
{
  if (widgets->configure_dialog) {
    // Determine size offsets between dialog size and size for images
    // Only on the initialisation of the dialog
    widgets->profile_width_offset = event->width - widgets->profile_width;
    widgets->profile_height_offset = event->height - widgets->profile_height;
    widgets->configure_dialog = FALSE;

    // Without this the settting, the dialog will only grow in vertical size - one can not then make it smaller!
    gtk_widget_set_size_request ( widget, widgets->profile_width+widgets->profile_width_offset, widgets->profile_height+widgets->profile_height_offset );

    // Allow resizing back down to a minimal size (especially useful if the initial size has been made bigger after restoring from the saved settings)
    GdkGeometry geom = { 600+widgets->profile_width_offset, 300+widgets->profile_height_offset, 0, 0, 0, 0, 0, 0, 0, 0, GDK_GRAVITY_STATIC };
    gdk_window_set_geometry_hints ( gtk_widget_get_window(widget), &geom, GDK_HINT_MIN_SIZE );
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

  // These widgets are only on the dialog
  if ( widgets->w_show_dem )
    widgets->show_dem = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dem) );
  if ( widgets->w_show_alt_gps_speed )
    widgets->show_alt_gps_speed = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed) );
  if ( widgets->w_show_gps_speed )
    widgets->show_gps_speed = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_gps_speed) );

  // Draw stuff
  draw_all_graphs ( widget, widgets, TRUE );

  return FALSE;
}

/**
 * Create height profile widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_profile ( GtkWidget *window, PropWidgets *widgets )
{
  GdkPixmap *pix;
  GtkWidget *eventbox;

  // First allocation & monitor how quick (or not it is)
  clock_t begin = clock();
  widgets->altitudes = vik_track_make_elevation_map ( widgets->tr, widgets->profile_width );
  clock_t end = clock();
  widgets->alt_create_time = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f", __FUNCTION__, widgets->alt_create_time );
  if ( widgets->altitudes == NULL )
    return NULL;

  pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  widgets->elev_image = gtk_image_new_from_pixmap ( pix, NULL );
  g_object_unref ( G_OBJECT(pix) );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_profile_click), widgets );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_profile_move), widgets );
  gtk_container_add ( GTK_CONTAINER(eventbox), widgets->elev_image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_STRUCTURE_MASK);

  return eventbox;
}

/**
 * Create height profile widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_gradient ( GtkWidget *window, PropWidgets *widgets)
{
  GdkPixmap *pix;
  GtkWidget *eventbox;

  // First allocation
  widgets->gradients = vik_track_make_gradient_map ( widgets->tr, widgets->profile_width );

  if ( widgets->gradients == NULL ) {
    return NULL;
  }

  pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  widgets->gradient_image = gtk_image_new_from_pixmap ( pix, NULL );
  g_object_unref ( G_OBJECT(pix) );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_gradient_click), widgets );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_gradient_move), widgets );
  gtk_container_add ( GTK_CONTAINER(eventbox), widgets->gradient_image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_STRUCTURE_MASK);

  return eventbox;
}

/**
 * Create speed/time widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_vtdiag ( GtkWidget *window, PropWidgets *widgets)
{
  GdkPixmap *pix;
  GtkWidget *eventbox;

  // First allocation
  widgets->speeds = vik_track_make_speed_map ( widgets->tr, widgets->profile_width );
  if ( widgets->speeds == NULL )
    return NULL;

  pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  widgets->speed_image = gtk_image_new_from_pixmap ( pix, NULL );

#if 0
  /* XXX this can go out, it's just a helpful dev tool */
  {
    int j;
    GdkGC **colors[8] = { gtk_widget_get_style(window)->bg_gc,
			  gtk_widget_get_style(window)->fg_gc,
			  gtk_widget_get_style(window)->light_gc,
			  gtk_widget_get_style(window)->dark_gc,
			  gtk_widget_get_style(window)->mid_gc,
			  gtk_widget_get_style(window)->text_gc,
			  gtk_widget_get_style(window)->base_gc,
			  gtk_widget_get_style(window)->text_aa_gc };
    for (i=0; i<5; i++) {
      for (j=0; j<8; j++) {
	gdk_draw_rectangle(GDK_DRAWABLE(pix), colors[j][i],
			   TRUE, i*20, j*20, 20, 20);
	gdk_draw_rectangle(GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc,
			   FALSE, i*20, j*20, 20, 20);
      }
    }
  }
#endif

  g_object_unref ( G_OBJECT(pix) );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_vt_click), widgets );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_vt_move), widgets );
  gtk_container_add ( GTK_CONTAINER(eventbox), widgets->speed_image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

  return eventbox;
}

/**
 * Create distance / time widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_dtdiag ( GtkWidget *window, PropWidgets *widgets)
{
  GdkPixmap *pix;
  GtkWidget *eventbox;

  // First allocation
  widgets->distances = vik_track_make_distance_map ( widgets->tr, widgets->profile_width );
  if ( widgets->distances == NULL )
    return NULL;

  pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  widgets->dist_image = gtk_image_new_from_pixmap ( pix, NULL );
  g_object_unref ( G_OBJECT(pix) );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_dt_click), widgets );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_dt_move), widgets );
  gtk_container_add ( GTK_CONTAINER(eventbox), widgets->dist_image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

  return eventbox;
}

/**
 * Create elevation / time widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_etdiag ( GtkWidget *window, PropWidgets *widgets)
{
  GdkPixmap *pix;
  GtkWidget *eventbox;

  // First allocation
  widgets->ats = vik_track_make_elevation_time_map ( widgets->tr, widgets->profile_width );
  if ( widgets->ats == NULL )
    return NULL;

  pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  widgets->elev_time_image = gtk_image_new_from_pixmap ( pix, NULL );
  g_object_unref ( G_OBJECT(pix) );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_et_click), widgets );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_et_move), widgets );
  gtk_container_add ( GTK_CONTAINER(eventbox), widgets->elev_time_image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

  return eventbox;
}

/**
 * Create speed/distance widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_sddiag ( GtkWidget *window, PropWidgets *widgets)
{
  GdkPixmap *pix;
  GtkWidget *eventbox;

  // First allocation
  widgets->speeds_dist = vik_track_make_speed_dist_map ( widgets->tr, widgets->profile_width );
  if ( widgets->speeds_dist == NULL )
    return NULL;

  pix = gdk_pixmap_new( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  widgets->speed_dist_image = gtk_image_new_from_pixmap ( pix, NULL );
  g_object_unref ( G_OBJECT(pix) );

  eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_sd_click), widgets );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_sd_move), widgets );
  gtk_container_add ( GTK_CONTAINER(eventbox), widgets->speed_dist_image );
  gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

  return eventbox;
}

#define VIK_SETTINGS_TRACK_PROFILE_WIDTH "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"

static void save_values ( PropWidgets *widgets )
{
  // Session settings
  a_settings_set_integer ( VIK_SETTINGS_TRACK_PROFILE_WIDTH, widgets->profile_width );
  a_settings_set_integer ( VIK_SETTINGS_TRACK_PROFILE_HEIGHT, widgets->profile_height );

  // Just for this session ATM
  if ( widgets->w_show_dem )
    show_dem                = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dem) );
  if ( widgets->w_show_alt_gps_speed )
    show_alt_gps_speed      = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed) );
  if ( widgets->w_show_gps_speed )
    show_gps_speed          = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_gps_speed) );
  if ( widgets->w_show_gradient_gps_speed )
    show_gradient_gps_speed = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_gradient_gps_speed) );
  if ( widgets->w_show_dist_speed )
    show_dist_speed         = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dist_speed) );
  if ( widgets->w_show_elev_dem )
    show_elev_dem           = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_elev_dem) );
  if ( widgets->w_show_elev_speed )
    show_elev_speed         = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_elev_speed) );
  if ( widgets->w_show_sd_gps_speed )
    show_sd_gps_speed       = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_sd_gps_speed) );
}

static void destroy_cb ( GtkDialog *dialog, PropWidgets *widgets )
{
  save_values(widgets);
  prop_widgets_free(widgets);
}

/**
 *
 */
static gboolean split_at_marker ( PropWidgets *widgets )
{
  VikTrack *tr = widgets->tr;
  VikTrwLayer *vtl = widgets->vtl;
      {
        GList *iter = tr->trackpoints;
        while ((iter = iter->next)) {
          if (widgets->marker_tp == VIK_TRACKPOINT(iter->data))
            break;
        }
        if (iter == NULL) {
          a_dialog_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), GTK_MESSAGE_ERROR,
                  _("Failed spliting track. Track unchanged"), NULL);
          return TRUE;
        }

        gchar *r_name = trw_layer_new_unique_sublayer_name(vtl,
                                                           widgets->tr->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK,
                                                           widgets->tr->name);
        iter->prev->next = NULL;
        iter->prev = NULL;
        VikTrack *tr_right = vik_track_new();
        if ( tr->comment )
          vik_track_set_comment ( tr_right, tr->comment );
        tr_right->visible = tr->visible;
        tr_right->is_route = tr->is_route;
        tr_right->trackpoints = iter;

        if ( widgets->tr->is_route )
          vik_trw_layer_add_route(vtl, r_name, tr_right);
        else
          vik_trw_layer_add_track(vtl, r_name, tr_right);
        vik_track_calculate_bounds ( tr );
        vik_track_calculate_bounds ( tr_right );

        g_free ( r_name );

        vik_layer_emit_update ( VIK_LAYER(vtl) );
      }

      return FALSE;
}

static void propwin_response_cb( GtkDialog *dialog, gint resp, PropWidgets *widgets )
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
      vik_track_set_description(tr, gtk_entry_get_text(GTK_ENTRY(widgets->w_description)));
      vik_track_set_source(tr, gtk_entry_get_text(GTK_ENTRY(widgets->w_source)));
      vik_track_set_type(tr, gtk_entry_get_text(GTK_ENTRY(widgets->w_type)));
      gtk_color_button_get_color ( GTK_COLOR_BUTTON(widgets->w_color), &(tr->color) );
      tr->draw_name_mode = gtk_combo_box_get_active ( GTK_COMBO_BOX(widgets->w_namelabel) );
      tr->max_number_dist_labels = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(widgets->w_number_distlabels) );
      trw_layer_update_treeview ( widgets->vtl, widgets->tr );
      vik_layer_emit_update ( VIK_LAYER(vtl) );
      break;
    case VIK_TRW_LAYER_PROPWIN_REVERSE:
      vik_track_reverse(tr);
      vik_layer_emit_update ( VIK_LAYER(vtl) );
      break;
    case VIK_TRW_LAYER_PROPWIN_DEL_DUP:
      (void)vik_track_remove_dup_points(tr); // NB ignore the returned answer
      // As we could have seen the number of duplicates that would be deleted in the properties statistics tab,
      //   choose not to inform the user unnecessarily

      /* above operation could have deleted current_tp or last_tp */
      trw_layer_cancel_tps_of_track ( vtl, tr );
      vik_layer_emit_update ( VIK_LAYER(vtl) );
      break;
    case VIK_TRW_LAYER_PROPWIN_SPLIT:
      {
        /* get new tracks, add them and then the delete old one. old can still exist on clipboard. */
        guint ntracks;
	
        VikTrack **tracks = vik_track_split_into_segments(tr, &ntracks);
        gchar *new_tr_name;
        guint i;
        for ( i = 0; i < ntracks; i++ )
        {
          if ( tracks[i] ) {
	    new_tr_name = trw_layer_new_unique_sublayer_name ( vtl,
                                                               widgets->tr->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK,
                                                               widgets->tr->name);
            if ( widgets->tr->is_route )
              vik_trw_layer_add_route ( vtl, new_tr_name, tracks[i] );
            else
              vik_trw_layer_add_track ( vtl, new_tr_name, tracks[i] );
            vik_track_calculate_bounds ( tracks[i] );

            g_free ( new_tr_name );
	  }
        }
        if ( tracks )
        {
          g_free ( tracks );
          /* Don't let track destroy this dialog */
          vik_track_clear_property_dialog(tr);
          if ( widgets->tr->is_route )
            vik_trw_layer_delete_route ( vtl, tr );
          else
            vik_trw_layer_delete_track ( vtl, tr );
          vik_layer_emit_update ( VIK_LAYER(vtl) ); /* chase thru the hoops */
        }
      }
      break;
    case VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER:
      keep_dialog = split_at_marker ( widgets );
      break;
    default:
      fprintf(stderr, "DEBUG: unknown response\n");
      return;
  }

  /* Keep same behaviour for now: destroy dialog if click on any button */
  if (!keep_dialog) {
    vik_track_clear_property_dialog(tr);
    gtk_widget_destroy ( GTK_WIDGET(dialog) );
  }
}

/**
 * Force a redraw when checkbutton has been toggled to show/hide that information
 */
static void checkbutton_toggle_cb ( GtkToggleButton *togglebutton, PropWidgets *widgets, gpointer dummy )
{
  // Even though not resized, we'll pretend it is -
  //  as this invalidates the saved images (since the image may have changed)
  if ( widgets->w_show_dem )
    widgets->show_dem = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dem) );
  if ( widgets->w_show_alt_gps_speed )
    widgets->show_alt_gps_speed = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_alt_gps_speed) );
  if ( widgets->w_show_gps_speed )
    widgets->show_gps_speed = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_gps_speed) );

  draw_all_graphs ( widgets->dialog, widgets, TRUE );
}

/**
 *  Create the widgets for the given graph tab
 */
static GtkWidget *create_graph_page ( GtkWidget *graph,
				      const gchar *markup,
				      GtkWidget *value,
				      const gchar *markup2,
				      GtkWidget *value2,
				      const gchar *markup3,
				      GtkWidget *value3,
				      GtkWidget *checkbutton1,
				      gboolean checkbutton1_default,
				      GtkWidget *checkbutton2,
				      gboolean checkbutton2_default )
{
  GtkWidget *hbox = gtk_hbox_new ( FALSE, 10 );
  GtkWidget *vbox = gtk_vbox_new ( FALSE, 10 );
  GtkWidget *label = gtk_label_new (NULL);
  GtkWidget *label2 = gtk_label_new (NULL);
  GtkWidget *label3 = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(vbox), graph, FALSE, FALSE, 0);
  gtk_label_set_markup ( GTK_LABEL(label), markup );
  gtk_label_set_markup ( GTK_LABEL(label2), markup2 );
  gtk_label_set_markup ( GTK_LABEL(label3), markup3 );
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), value, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), label2, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), value2, FALSE, FALSE, 0);
  if ( value3 ) {
    gtk_box_pack_start (GTK_BOX(hbox), label3, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(hbox), value3, FALSE, FALSE, 0);
  }
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

static GtkWidget *create_table (int cnt, char *labels[], GtkWidget *contents[])
{
  GtkTable *table = GTK_TABLE(gtk_table_new (cnt, 2, FALSE));
  gtk_table_set_col_spacing (table, 0, 10);
  for (guint i=0; i<cnt; i++) {
    // Settings so the text positioning only moves around vertically when the dialog is resized
    // This also gives more room to see the track comment
    GtkWidget *label = gtk_label_new(NULL);
    gtk_misc_set_alignment ( GTK_MISC(label), 1, 0.5 ); // Position text centrally in vertical plane
    gtk_label_set_markup ( GTK_LABEL(label), _(labels[i]) );
    gtk_table_attach ( table, label, 0, 1, i, i+1, GTK_FILL, GTK_SHRINK, 0, 0 );
    if (GTK_IS_MISC(contents[i])) {
      gtk_misc_set_alignment ( GTK_MISC(contents[i]), 0, 0.5 );
    }
    if ( GTK_IS_COLOR_BUTTON(contents[i]) || GTK_IS_COMBO_BOX(contents[i]) )
      // Buttons compressed - otherwise look weird (to me) if vertically massive
      gtk_table_attach ( table, contents[i], 1, 2, i, i+1, GTK_FILL, GTK_SHRINK, 0, 5 );
    else
      // Expand for comments + descriptions / labels
      gtk_table_attach_defaults ( table, contents[i], 1, 2, i, i+1 );
  }

  return GTK_WIDGET (table);
}

#define SPLIT_COLS 5

static void splits_copy_all ( GtkWidget *tree_view )
{
  GString *str = g_string_new ( NULL );
  gchar sep = '\t';

  // Get info from the GTK store
  //  using this way gets the items in the ordered by the user
  GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(tree_view) );
  GtkTreeIter iter;
  if ( !gtk_tree_model_get_iter_first(model, &iter) )
    return;

  vik_units_speed_t speed_units = a_vik_get_units_speed ();

  gboolean cont = TRUE;
  while ( cont ) {
    gint ivalue;
    gdouble dvalue;

    gtk_tree_model_get ( model, &iter, 0, &dvalue, -1 );
    g_string_append_printf ( str, "%.1f%c", dvalue, sep );

    gtk_tree_model_get ( model, &iter, 1, &ivalue, -1 );
    gint minutes, seconds;
    minutes = ivalue / 60;
    seconds = ivalue % 60;
    g_string_append_printf ( str, "%d:%02d%c", minutes, seconds, sep );

    // Speed value a little more involved:
    gtk_tree_model_get ( model, &iter, 2, &dvalue, -1 );
    gchar buf[16];
    vu_speed_text_value ( buf, sizeof(buf), speed_units, dvalue, "%.1f" );
    g_string_append_printf ( str, "%s%c", buf, sep );

    gtk_tree_model_get ( model, &iter, 3, &ivalue, -1 );
    g_string_append_printf ( str, "%d%c", ivalue, sep );

    gtk_tree_model_get ( model, &iter, 4, &ivalue, -1 );
    g_string_append_printf ( str, "%d%c", ivalue, sep );

    g_string_append_printf ( str, "\n" );
    cont = gtk_tree_model_iter_next ( model, &iter );
  }

  a_clipboard_copy ( VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, str->str, NULL );
  g_string_free ( str, TRUE );
}

static gboolean splits_menu_popup ( GtkWidget *tree_view,
                                    GdkEventButton *event,
                                    gpointer data )
{
  GtkWidget *menu = gtk_menu_new();
  (void)vu_menu_add_item ( GTK_MENU(menu), _("_Copy Data"), GTK_STOCK_COPY, G_CALLBACK(splits_copy_all), tree_view );
  gtk_widget_show_all ( menu );
  gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
  return TRUE;
}

static gboolean splits_button_pressed ( GtkWidget *tree_view,
                                        GdkEventButton *event,
                                        gpointer data )
{
  // Only on right clicks...
  if ( ! (event->type == GDK_BUTTON_PRESS && event->button == 3) )
    return FALSE;
  return splits_menu_popup ( tree_view, event, data );
}

static void format_time_cell_data_func ( GtkTreeViewColumn *col,
                                         GtkCellRenderer   *renderer,
                                         GtkTreeModel      *model,
                                         GtkTreeIter       *iter,
                                         gpointer           user_data )
{
  guint value;
  gchar buf[32];
  gint column = GPOINTER_TO_INT (user_data);
  gtk_tree_model_get ( model, iter, column, &value, -1 );

  gint minutes, seconds;
  minutes = value / 60;
  seconds = value % 60;
  g_snprintf ( buf, sizeof(buf), "%d:%02d", minutes, seconds );

  g_object_set ( renderer, "text", buf, NULL );
}

/**
 * create_a_split_table:
 *
 * Create a table of the split values in a scrollable treeview,
 *  which then allows sorting by each of the columns and a way to copy all the data
 */
static GtkWidget *create_a_split_table ( VikTrack *trk, guint split_factor )
{
  GtkWidget *scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

  GtkTreeStore *store = gtk_tree_store_new ( SPLIT_COLS,
                                             G_TYPE_DOUBLE,  // 0: Distance
                                             G_TYPE_UINT,    // 1: Time
                                             G_TYPE_DOUBLE,  // 2: Speed
                                             G_TYPE_INT,     // 3: Gain
                                             G_TYPE_INT ) ;  // 4: Loss

  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  gdouble split_length;
  switch ( dist_units ) {
  case VIK_UNITS_DISTANCE_MILES:
    split_length = VIK_MILES_TO_METERS(split_factor); break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    split_length = VIK_NAUTICAL_MILES_TO_METERS(split_factor); break;
  default: // VIK_UNITS_DISTANCE_KILOMETRES
    split_length = split_factor * 1000.0; break;
  }

  vik_units_height_t height_units = a_vik_get_units_height ();
  vik_units_speed_t speed_units = a_vik_get_units_speed ();

  GtkWidget *view = gtk_tree_view_new();
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *column;
  gint column_runner = 0;

  GString *str0 = g_string_new ( _("Distance") );
  gchar *dist_str = NULL;
  switch ( dist_units ) {
  case VIK_UNITS_DISTANCE_MILES:          dist_str = _("miles"); break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES: dist_str = _("NM"); break;
    // VIK_UNITS_DISTANCE_KILOMETRES
  default:                                dist_str = _("km"); break;
  }
  g_string_append_printf ( str0, "\n(%s)", dist_str );

  column = ui_new_column_text ( str0->str, renderer, view, column_runner++ );
  gtk_tree_view_column_set_cell_data_func ( column, renderer, ui_format_1f_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL );
  g_string_free ( str0, TRUE );

  column = ui_new_column_text ( _("Time\n(m:ss)"), renderer, view, column_runner++ );
  gtk_tree_view_column_set_cell_data_func ( column, renderer, format_time_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL );

  gchar *speed_units_str = vu_speed_units_text ( speed_units );

  GString *str1 = g_string_new ( _("Speed") );
  g_string_append_printf ( str1, "\n(%s)", speed_units_str );
  column = ui_new_column_text ( str1->str, renderer, view, column_runner++ );
  gtk_tree_view_column_set_cell_data_func ( column, renderer, vu_format_speed_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL );
  g_string_free ( str1, TRUE );
  g_free ( speed_units_str );

  gchar *hght_str = NULL;
  if ( height_units == VIK_UNITS_HEIGHT_METRES )
    hght_str = _("m");
  else
    hght_str = _("ft");

  GString *str2 = g_string_new ( _("Gain") );
  g_string_append_printf ( str2, "\n(%s)", hght_str );
  (void)ui_new_column_text ( str2->str, renderer, view, column_runner++ );
  g_string_free ( str2, TRUE );

  GString *str3 = g_string_new ( _("Loss") );
  g_string_append_printf ( str3, "\n(%s)", hght_str );
  (void)ui_new_column_text ( str3->str, renderer, view, column_runner++ );
  g_string_free ( str3, TRUE );

  gtk_tree_view_set_model ( GTK_TREE_VIEW(view), GTK_TREE_MODEL(store) );
  gtk_tree_selection_set_mode ( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_NONE );
  gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW(view), TRUE );

  g_signal_connect ( view, "button-press-event", G_CALLBACK(splits_button_pressed), NULL );

  gtk_container_add ( GTK_CONTAINER(scrolledwindow), view );

  // Update the datastore

  GArray *ga = vik_track_speed_splits ( trk, split_length );
  GtkTreeIter t_iter;

  for ( guint gg = 0; gg < ga->len; gg++ ) {
    VikTrackSpeedSplits_t vtss = g_array_index ( ga, VikTrackSpeedSplits_t, gg );

    gdouble length;
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_MILES:
      length = VIK_METERS_TO_MILES(vtss.length); break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      length = VIK_METERS_TO_NAUTICAL_MILES(vtss.length); break;
    default: // VIK_UNITS_DISTANCE_KILOMETRES:
      length = vtss.length/1000.0; break;
    }

    gdouble my_speed = vu_speed_convert ( speed_units, vtss.speed );

    gdouble up, down;
    switch (height_units) {
    case VIK_UNITS_HEIGHT_FEET:
      up = VIK_METERS_TO_FEET(vtss.elev_up);
      down = VIK_METERS_TO_FEET(vtss.elev_down);
      break;
    default: // VIK_UNITS_HEIGHT_METRES:
      // No conversion needed
      up = vtss.elev_up;
      down = vtss.elev_down;
      break;
    }

    gtk_tree_store_append ( store, &t_iter, NULL );
    gtk_tree_store_set ( store, &t_iter,
                         0, length,
                         1, vtss.time,
                         2, my_speed,
                         3, (gint)round(up),
                         4, (gint)round(down),
                         -1 );
  }

  (void)g_array_free ( ga, TRUE );

  return scrolledwindow;
}

static GtkWidget *create_splits_tables ( VikTrack *trk )
{
  // Standard distance splits (in whatever the preferred distance units are)
  int index[3] = {1, 5, 10};
  // When very long tracks use bigger split distances
  gdouble len = vik_track_get_length(trk);
  // > 1000 KM ?
  if ( len > 1000000 ) {
    index[0] = 25;
    index[1] = 50;
    index[2] = 50;
  } // > 100 KM ?
  else if ( len > 100000 ) {
    index[0] = 5;
    index[1] = 10;
    index[2] = 25;
  }

  // Create the tables and stick in tabs
  GtkWidget *tabs = gtk_notebook_new();
  for ( int i = 0; i < G_N_ELEMENTS(index); i++ ) {
    GtkWidget *table = create_a_split_table ( trk, index[i] );
    gchar *str = g_strdup_printf (_("Split %d"), index[i]);
    gtk_notebook_append_page ( GTK_NOTEBOOK(tabs), GTK_WIDGET(table), gtk_label_new(str) );
    g_free ( str );
  }

  return tabs;
}

/**
 *
 */
static GtkWidget *create_statistics_page ( PropWidgets *widgets, VikTrack *tr )
{
  GtkWidget *content[20];
  int cnt = 0;
  GtkWidget *table;
  gdouble tr_len;
  gulong tp_count;
  gdouble tmp_speed;

  static gchar tmp_buf[50];

  static gchar *stats_texts[] = {
    N_("<b>Track Length:</b>"),
    N_("<b>Trackpoints:</b>"),
    N_("<b>Segments:</b>"),
    N_("<b>Duplicate Points:</b>"),
    N_("<b>Max Speed:</b>"),
    N_("<b>Avg. Speed:</b>"),
    N_("<b>Moving Avg. Speed:</b>"),
    N_("<b>Avg. Dist. Between TPs:</b>"),
    N_("<b>Elevation Range:</b>"),
    N_("<b>Total Elevation Gain/Loss:</b>"),
    N_("<b>Start:</b>"),
    N_("<b>End:</b>"),
    N_("<b>Duration:</b>"),
  };

  guint seg_count = vik_track_get_segment_count ( widgets->tr );

  // Don't use minmax_array(widgets->altitudes), as that is a simplified representative of the points
  //  thus can miss the highest & lowest values by a few metres
  gdouble min_alt, max_alt;
  if ( !vik_track_get_minmax_alt (widgets->tr, &min_alt, &max_alt) )
    min_alt = max_alt = NAN;

  vik_units_distance_t dist_units = a_vik_get_units_distance ();

  // NB This value not shown yet - but is used by internal calculations
  widgets->track_length_inc_gaps = vik_track_get_length_including_gaps(tr);

  tr_len = widgets->track_length = vik_track_get_length(tr);
  switch (dist_units) {
  case VIK_UNITS_DISTANCE_KILOMETRES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f km", tr_len/1000.0 );
    break;
  case VIK_UNITS_DISTANCE_MILES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f miles", VIK_METERS_TO_MILES(tr_len) );
    break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f NM", VIK_METERS_TO_NAUTICAL_MILES(tr_len) );
    break;
  default:
    g_critical("Houston, we've had a problem. distance=%d", dist_units);
  }
  widgets->w_track_length = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  tp_count = vik_track_get_tp_count(tr);
  g_snprintf(tmp_buf, sizeof(tmp_buf), "%lu", tp_count );
  widgets->w_tp_count = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count );
  widgets->w_segment_count = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  g_snprintf(tmp_buf, sizeof(tmp_buf), "%lu", vik_track_get_dup_point_count(tr) );
  widgets->w_duptp_count = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  tmp_speed = vik_track_get_max_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, tmp_speed, TRUE, "%.2f" );
  }
  widgets->w_max_speed = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  tmp_speed = vik_track_get_average_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, tmp_speed, TRUE, "%.2f" );
  }
  widgets->w_avg_speed = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  // Use 60sec as the default period to be considered stopped
  //  this is the TrackWaypoint draw stops default value 'vtl->stop_length'
  //  however this variable is not directly accessible - and I don't expect it's often changed from the default
  //  so ATM just put in the number
  tmp_speed = vik_track_get_average_speed_moving(tr, 60);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, tmp_speed, TRUE, "%.2f" );
  }
  widgets->w_mvg_speed = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  switch (dist_units) {
  case VIK_UNITS_DISTANCE_KILOMETRES:
    // Even though kilometres, the average distance between points is going to be quite small so keep in metres
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / ( tp_count - seg_count ) );
    break;
  case VIK_UNITS_DISTANCE_MILES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.3f miles", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_MILES(tr_len / ( tp_count - seg_count )) );
    break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%.3f NM", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_NAUTICAL_MILES(tr_len / ( tp_count - seg_count )) );
    break;
  default:
    g_critical("Houston, we've had a problem. distance=%d", dist_units);
  }
  widgets->w_avg_dist = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  vik_units_height_t height_units = a_vik_get_units_height ();
  if ( isnan(min_alt) && isnan(max_alt) )
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
  widgets->w_elev_range = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  vik_track_get_total_elevation_gain(tr, &max_alt, &min_alt );
  if ( isnan(min_alt) && isnan(max_alt) )
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
  widgets->w_elev_gain = content[cnt++] = ui_label_new_selectable ( tmp_buf );

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

  if ( tr->trackpoints && !isnan(VIK_TRACKPOINT(tr->trackpoints->data)->timestamp) )
  {
    gdouble t1 = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
    gdouble t2 = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;

    VikCoord vc;
    // Notional center of a track is simply an average of the bounding box extremities
    struct LatLon center = { (tr->bbox.north+tr->bbox.south)/2, (tr->bbox.east+tr->bbox.west)/2 };
    vik_coord_load_from_latlon ( &vc, vik_trw_layer_get_coord_mode(widgets->vtl), &center );

    widgets->tz = vu_get_tz_at_location ( &vc );

    time_t ts1 = round ( t1 );
    time_t ts2 = round ( t2 );
    gchar *msg;
    msg = vu_get_time_string ( &ts1, "%c", &vc, widgets->tz );
    widgets->w_time_start = content[cnt++] = ui_label_new_selectable(msg);
    g_free ( msg );

    msg = vu_get_time_string ( &ts2, "%c", &vc, widgets->tz );
    widgets->w_time_end = content[cnt++] = ui_label_new_selectable(msg);
    g_free ( msg );

    gint total_duration_s = (gint)(t2-t1);
    gint segments_duration_s = (gint)vik_track_get_duration(tr,FALSE);
    gint total_duration_m = total_duration_s/60;
    gint segments_duration_m = segments_duration_s/60;
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d minutes - %d minutes moving"), total_duration_m, segments_duration_m);
    widgets->w_time_dur = content[cnt++] = ui_label_new_selectable(tmp_buf);

    // A tooltip to show in more readable hours:minutes:seconds
    gchar tip_buf_total[20];
    guint h_tot, m_tot, s_tot;
    util_time_decompose ( total_duration_s, &h_tot, &m_tot, &s_tot );
    g_snprintf(tip_buf_total, sizeof(tip_buf_total), "%d:%02d:%02d", h_tot, m_tot, s_tot);

    gchar tip_buf_segments[20];
    guint h_seg, m_seg, s_seg;
    util_time_decompose ( segments_duration_s, &h_seg, &m_seg, &s_seg );
    g_snprintf(tip_buf_segments, sizeof(tip_buf_segments), "%d:%02d:%02d", h_seg, m_seg, s_tot);

    gchar *tip = g_strdup_printf (_("%s total - %s in segments"), tip_buf_total, tip_buf_segments);
    gtk_widget_set_tooltip_text ( GTK_WIDGET(widgets->w_time_dur), tip );
    g_free (tip);
  } else {
    widgets->w_time_start = content[cnt++] = gtk_label_new(_("No Data"));
    widgets->w_time_end = content[cnt++] = gtk_label_new(_("No Data"));
    widgets->w_time_dur = content[cnt++] = gtk_label_new(_("No Data"));
  }

  table = create_table (cnt, stats_texts, content);

  return table;
}

/**
 *
 */
void vik_trw_layer_propwin_run ( GtkWindow *parent,
                                 VikTrwLayer *vtl,
                                 VikTrack *tr,
                                 gpointer vlp,
                                 VikViewport *vvp,
                                 gboolean start_on_stats )
{
  PropWidgets *widgets = prop_widgets_new();
  widgets->vtl = vtl;
  widgets->vvp = vvp;
  widgets->vlp = vlp;
  widgets->tr = tr;

  gint profile_size_value;
  // Ensure minimum values
  widgets->profile_width = 600 * vik_viewport_get_scale(vvp);
  if ( a_settings_get_integer ( VIK_SETTINGS_TRACK_PROFILE_WIDTH, &profile_size_value ) )
    if ( profile_size_value > widgets->profile_width )
      widgets->profile_width = profile_size_value;

  widgets->profile_height = 300 * vik_viewport_get_scale(vvp);
  if ( a_settings_get_integer ( VIK_SETTINGS_TRACK_PROFILE_HEIGHT, &profile_size_value ) )
    if ( profile_size_value > widgets->profile_height )
      widgets->profile_height = profile_size_value;

  gchar *title = g_strdup_printf(_("%s - Track Properties"), tr->name);
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
  g_signal_connect( G_OBJECT(dialog), "response", G_CALLBACK(propwin_response_cb), widgets);

  g_free(title);
  GtkWidget *table;

  gboolean DEM_available = a_dems_overlaps_bbox (tr->bbox);

  widgets->elev_box = vik_trw_layer_create_profile(GTK_WIDGET(parent), widgets);
  widgets->gradient_box = vik_trw_layer_create_gradient(GTK_WIDGET(parent), widgets);
  widgets->speed_box = vik_trw_layer_create_vtdiag(GTK_WIDGET(parent), widgets);
  widgets->dist_box = vik_trw_layer_create_dtdiag(GTK_WIDGET(parent), widgets);
  widgets->elev_time_box = vik_trw_layer_create_etdiag(GTK_WIDGET(parent), widgets);
  widgets->speed_dist_box = vik_trw_layer_create_sddiag(GTK_WIDGET(parent), widgets);
  GtkWidget *graphs = gtk_notebook_new();

  GtkWidget *content_prop[20];
  int cnt_prop = 0;

  static gchar *label_texts[] = {
    N_("<b>Comment:</b>"),
    N_("<b>Description:</b>"),
    N_("<b>Source:</b>"),
    N_("<b>Type:</b>"),
    N_("<b>Color:</b>"),
    N_("<b>Draw Name:</b>"),
    N_("<b>Distance Labels:</b>"),
  };

  // Properties
  widgets->w_comment = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
  if ( tr->comment )
    gtk_entry_set_text ( GTK_ENTRY(widgets->w_comment), tr->comment );
  content_prop[cnt_prop++] = widgets->w_comment;

  widgets->w_description = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
  if ( tr->description )
    gtk_entry_set_text ( GTK_ENTRY(widgets->w_description), tr->description );
  content_prop[cnt_prop++] = widgets->w_description;

  widgets->w_source = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
  if ( tr->source )
    gtk_entry_set_text ( GTK_ENTRY(widgets->w_source), tr->source );
  content_prop[cnt_prop++] = widgets->w_source;

  widgets->w_type = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
  if ( tr->type )
    gtk_entry_set_text ( GTK_ENTRY(widgets->w_type), tr->type );
  content_prop[cnt_prop++] = widgets->w_type;

  widgets->w_color = content_prop[cnt_prop++] = gtk_color_button_new_with_color ( &(tr->color) );

  static gchar *draw_name_labels[] = {
    N_("No"),
    N_("Centre"),
    N_("Start only"),
    N_("End only"),
    N_("Start and End"),
    N_("Centre, Start and End"),
    NULL
  };

  widgets->w_namelabel = content_prop[cnt_prop++] = vik_combo_box_text_new ();
  gchar **pstr = draw_name_labels;
  while ( *pstr )
    vik_combo_box_text_append ( widgets->w_namelabel, *(pstr++) );
  gtk_combo_box_set_active ( GTK_COMBO_BOX(widgets->w_namelabel), tr->draw_name_mode );

  widgets->w_number_distlabels = content_prop[cnt_prop++] =
   gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new(tr->max_number_dist_labels, 0, 100, 1, 1, 0)), 1, 0 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(widgets->w_number_distlabels), _("Maximum number of distance labels to be shown") );

  table = create_table (cnt_prop, label_texts, content_prop);

  gtk_notebook_append_page(GTK_NOTEBOOK(graphs), GTK_WIDGET(table), gtk_label_new(_("Properties")));

  gtk_notebook_append_page(GTK_NOTEBOOK(graphs), create_statistics_page(widgets, widgets->tr), gtk_label_new(_("Statistics")));

  // TODO: One day might be nice to have bar chart equivalent of the simple table values.
  // Only bother showing timing splits if times are available
  if ( widgets->speed_box )
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), create_splits_tables(tr), gtk_label_new(_("Splits")));

  if ( widgets->elev_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_dist = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_elevation = ui_label_new_selectable(_("No Data"));
    widgets->w_show_dem = gtk_check_button_new_with_mnemonic(_("Show D_EM"));
    gtk_widget_set_sensitive ( widgets->w_show_dem, DEM_available );
    widgets->w_show_alt_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
    page = create_graph_page (widgets->elev_box,
                              _("<b>Track Distance:</b>"), widgets->w_cur_dist,
                              _("<b>Track Height:</b>"), widgets->w_cur_elevation,
                              NULL, NULL,
                              widgets->w_show_dem, show_dem,
                              widgets->w_show_alt_gps_speed, show_alt_gps_speed);
    g_signal_connect (widgets->w_show_dem, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    g_signal_connect (widgets->w_show_alt_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Elevation-distance")));
  }

  if ( widgets->gradient_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_gradient_dist = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_gradient_gradient = ui_label_new_selectable(_("No Data"));
    widgets->w_show_gradient_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
    page = create_graph_page (widgets->gradient_box,
                              _("<b>Track Distance:</b>"), widgets->w_cur_gradient_dist,
                              _("<b>Track Gradient:</b>"), widgets->w_cur_gradient_gradient,
                              NULL, NULL,
                              widgets->w_show_gradient_gps_speed, show_gradient_gps_speed,
                              NULL, FALSE);
    g_signal_connect (widgets->w_show_gradient_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Gradient-distance")));
  }

  if ( widgets->speed_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_time = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_speed = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_time_real = ui_label_new_selectable(_("No Data"));
    widgets->w_show_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
    page = create_graph_page (widgets->speed_box,
                              _("<b>Track Time:</b>"), widgets->w_cur_time,
                              _("<b>Track Speed:</b>"), widgets->w_cur_speed,
                              _("<b>Time/Date:</b>"), widgets->w_cur_time_real,
                              widgets->w_show_gps_speed, show_gps_speed,
                              NULL, FALSE);
    g_signal_connect (widgets->w_show_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Speed-time")));
  }

  if ( widgets->dist_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_dist_time = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_dist_dist = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_dist_time_real = ui_label_new_selectable(_("No Data"));
    widgets->w_show_dist_speed = gtk_check_button_new_with_mnemonic(_("Show S_peed"));
    page = create_graph_page (widgets->dist_box,
                              _("<b>Track Distance:</b>"), widgets->w_cur_dist_dist,
                              _("<b>Track Time:</b>"), widgets->w_cur_dist_time,
                              _("<b>Time/Date:</b>"), widgets->w_cur_dist_time_real,
                              widgets->w_show_dist_speed, show_dist_speed,
                              NULL, FALSE);
    g_signal_connect (widgets->w_show_dist_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Distance-time")));
  }

  if ( widgets->elev_time_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_elev_time = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_elev_elev = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_elev_time_real = ui_label_new_selectable(_("No Data"));
    widgets->w_show_elev_speed = gtk_check_button_new_with_mnemonic(_("Show S_peed"));
    widgets->w_show_elev_dem = gtk_check_button_new_with_mnemonic(_("Show D_EM"));
    gtk_widget_set_sensitive ( widgets->w_show_elev_dem, DEM_available );
    page = create_graph_page (widgets->elev_time_box,
                              _("<b>Track Time:</b>"), widgets->w_cur_elev_time,
                              _("<b>Track Height:</b>"), widgets->w_cur_elev_elev,
                              _("<b>Time/Date:</b>"), widgets->w_cur_elev_time_real,
                              widgets->w_show_elev_dem, show_elev_dem,
                              widgets->w_show_elev_speed, show_elev_speed);
    g_signal_connect (widgets->w_show_elev_dem, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    g_signal_connect (widgets->w_show_elev_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Elevation-time")));
  }

  if ( widgets->speed_dist_box ) {
    GtkWidget *page = NULL;
    widgets->w_cur_speed_dist = ui_label_new_selectable(_("No Data"));
    widgets->w_cur_speed_speed = ui_label_new_selectable(_("No Data"));
    widgets->w_show_sd_gps_speed = gtk_check_button_new_with_mnemonic(_("Show _GPS Speed"));
    page = create_graph_page (widgets->speed_dist_box,
                              _("<b>Track Distance:</b>"), widgets->w_cur_speed_dist,
                              _("<b>Track Speed:</b>"), widgets->w_cur_speed_speed,
                              NULL, NULL,
                              widgets->w_show_sd_gps_speed, show_sd_gps_speed,
                              NULL, FALSE);
    g_signal_connect (widgets->w_show_sd_gps_speed, "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets);
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Speed-distance")));
  }

  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), graphs, FALSE, FALSE, 0);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER, FALSE);
  if (vik_track_get_segment_count(tr) <= 1)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_SPLIT, FALSE);
  if (vik_track_get_dup_point_count(tr) <= 0)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), VIK_TRW_LAYER_PROPWIN_DEL_DUP, FALSE);

  // On dialog realization configure_event causes the graphs to be initially drawn
  widgets->configure_dialog = TRUE;
  g_signal_connect ( G_OBJECT(dialog), "configure-event", G_CALLBACK (configure_event), widgets );

  g_signal_connect ( G_OBJECT(dialog), "destroy", G_CALLBACK (destroy_cb), widgets );

  vik_track_set_property_dialog(tr, dialog);
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  gtk_widget_show_all ( dialog );

  // Gtk note: due to historical reasons, this must be done after widgets are shown
  if ( start_on_stats )
    gtk_notebook_set_current_page ( GTK_NOTEBOOK(graphs), 1 );
}


/**
 * Update this property dialog
 * e.g. if the track has been renamed
 */
void vik_trw_layer_propwin_update ( VikTrack *trk )
{
  // If not displayed do nothing
  if ( !trk->property_dialog )
    return;

  // Update title with current name
  if ( trk->name ) {
    gchar *title = g_strdup_printf ( _("%s - Track Properties"), trk->name );
    gtk_window_set_title ( GTK_WINDOW(trk->property_dialog), title );
    g_free(title);
  }

}

static void add_tip_text_dist_elev ( GString *gtip, VikTrackpoint *trackpoint, gdouble meters_from_start )
{
  static gchar tmp_buf[20];
  get_distance_text ( tmp_buf, sizeof(tmp_buf), meters_from_start );
  g_string_append_printf ( gtip, "%s\n", tmp_buf );
  get_altitude_text ( tmp_buf, sizeof(tmp_buf), trackpoint );
  g_string_append_printf ( gtip, "%s\n", tmp_buf );
}

/**
 * A tooltip featuring elevation, distance & time
 */
static gboolean graph_tooltip_cb ( GtkWidget  *widget,
                                   gint        x,
                                   gint        y,
                                   gboolean    keyboard_tip,
                                   GtkTooltip *tooltip,
                                   gpointer    data )
{
  PropWidgets *widgets = (PropWidgets*)data;
  // x & y won't be valid
  if ( keyboard_tip )
    return FALSE;

  gdouble xx = x - MARGIN_X;
  if ( xx < 0 ) xx = 0.0;
  if ( xx > widgets->profile_width ) xx = widgets->profile_width;

  gboolean ans = FALSE;
  GString *gtip = g_string_new ( NULL );
  VikTrackpoint *trackpoint = NULL;
  gdouble seconds_from_start = NAN;
  gdouble meters_from_start;

  if ( widget == widgets->elev_box ) {
    trackpoint = vik_track_get_closest_tp_by_percentage_dist ( widgets->tr, xx/widgets->profile_width, &meters_from_start );
    if ( trackpoint ) {
      add_tip_text_dist_elev ( gtip, trackpoint, meters_from_start );

      // NB ATM skip working out the speed if not available directly,
      //  otherwise extra overhead to work it out and not that important to show
      if ( !isnan(trackpoint->speed) ) {
        static gchar tmp_buf1[64];
        vik_units_speed_t speed_units = a_vik_get_units_speed ();
        vu_speed_text ( tmp_buf1, sizeof(tmp_buf1), speed_units, trackpoint->speed, FALSE, "%.1f" );
        g_string_append_printf ( gtip, "%s\n", tmp_buf1 );
      }

      VikTrackpoint *tp1 = vik_track_get_tp_first ( widgets->tr );
      if ( !isnan(trackpoint->timestamp) && tp1 && !isnan(tp1->timestamp) ) {
        seconds_from_start = trackpoint->timestamp - tp1->timestamp;
      }
    }
  }

  if ( widget == widgets->speed_box ) {
    trackpoint = vik_track_get_closest_tp_by_percentage_time ( widgets->tr, xx/widgets->profile_width, &seconds_from_start );
    if ( trackpoint ) {

      // Since vik_track_get_length_to_trackpoint() might reprocess the whole track (yet again)
      //  it could be potentially make this extra slow and so not best practice to perform in the tooltip every time
      //  otherwise the tooltip might not get shown at all
      if ( widgets->alt_create_time < 0.01 ) {
        meters_from_start = vik_track_get_length_to_trackpoint ( widgets->tr, trackpoint );
        add_tip_text_dist_elev ( gtip, trackpoint, meters_from_start );
      }

      gint ix = (gint)xx;
      // Ensure ix is inbounds
      if (ix == widgets->profile_width)
        ix--;
      static gchar tmp_buf1[64];
      vik_units_speed_t speed_units = a_vik_get_units_speed ();
      vu_speed_text ( tmp_buf1, sizeof(tmp_buf1), speed_units, widgets->speeds[ix], FALSE, "%.1f" );
      g_string_append_printf ( gtip, "%s\n", tmp_buf1 );
    }
  }

  if ( !isnan(seconds_from_start) ) {
    seconds_from_start = round ( seconds_from_start );
    guint h = seconds_from_start/3600;
    guint m = (seconds_from_start - h*3600)/60;
    guint s = seconds_from_start - (3600*h) - (60*m);

    gchar time_buf[64];
    if ( !isnan(trackpoint->timestamp) ) {
      time_t ts = round ( trackpoint->timestamp );
      // Alternatively could use %c format but I prefer a slightly more compact form here
      strftime ( time_buf, sizeof(time_buf), "%X %x %Z", localtime(&ts) );
    }
    // NB No newline as this is always the last bit ATM
    g_string_append_printf ( gtip, "%02d:%02d:%02d - %s", h, m, s, time_buf );
  }

  if ( gtip->str ) {
    gtk_tooltip_set_text ( tooltip, gtip->str );
    ans = TRUE;
  }

  g_string_free ( gtip, TRUE );

  return ans;
}

/**
 * vik_trw_layer_propwin_main_refresh:
 *
 * Since the track may have changed, recalculate & redraw
 */
gboolean vik_trw_layer_propwin_main_refresh ( VikLayer *vl )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  PropWidgets *widgets = vik_window_get_graphs_widgets ( vw );
  if ( !widgets )
    return FALSE;

  // Should be on the right track...
  widgets->track_length_inc_gaps = vik_track_get_length_including_gaps ( widgets->tr );

  if ( widgets->altitudes )
    g_free ( widgets->altitudes );
  widgets->altitudes = vik_track_make_elevation_map ( widgets->tr, widgets->profile_width );

  if ( widgets->speeds )
    g_free ( widgets->speeds );
  widgets->speeds = vik_track_make_speed_map ( widgets->tr, widgets->profile_width );

  // If no current values then clear display of any previous stuff
  if ( !widgets->altitudes && widgets->elev_box ) {
    GtkWidget *window = gtk_widget_get_toplevel ( widgets->elev_box );
    GdkPixmap *pix = gdk_pixmap_new ( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
    gtk_image_set_from_pixmap ( GTK_IMAGE(widgets->elev_image), pix, NULL );
    clear_images ( pix, window, widgets );

    // Extra protection in case all trackpoints of an existing timed track get deleted
    if ( !widgets->speeds && widgets->speed_box ) {
      GdkPixmap *pix = gdk_pixmap_new ( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
      gtk_image_set_from_pixmap ( GTK_IMAGE(widgets->speed_image), pix, NULL );
      clear_images ( pix, window, widgets );
    }
  }
  else
    draw_all_graphs ( GTK_WIDGET(widgets->graphs), widgets, TRUE );

  return FALSE;
}

/**
 * Draw the graphs when in the main display
 */
static gboolean redraw_signal_event ( GtkWidget *widget, GdkEvent *event, PropWidgets *widgets )
{
  GtkAllocation allocation;
  if ( widgets->elev_box ) {
    gtk_widget_get_allocation ( widgets->elev_box, &allocation );
  }
  else if ( widgets->speed_box )
    gtk_widget_get_allocation ( widgets->speed_box, &allocation );
  else {
    g_critical ( "%s shouldn't happen - trying to draw but no graphs!!", __FUNCTION__ );
    return TRUE;
  }

  // Work out actual size used for the graphs considering the margin
  widgets->profile_width = allocation.width - MARGIN_X;
  widgets->profile_height = allocation.height - MARGIN_Y;

  // Draw it
  if ( widgets->profile_width > 0 && widgets->profile_height > 0 ) {
    draw_all_graphs ( widget, widgets, TRUE );
  }
  return FALSE;
}

/**
 * vik_trw_layer_propwin_main:
 *
 *  Show the properties widgets in the main display
 *  Here it is a cut down version with just of couple of graphs,
 *   but with a tooltip and a right click menu
 *  (instead of the buttons on the dialog version)
 */
gpointer vik_trw_layer_propwin_main ( GtkWindow *parent,
                                      VikTrwLayer *vtl,
                                      VikTrack *tr,
                                      VikViewport *vvp,
                                      GtkWidget *self,
                                      gboolean show )
{
  PropWidgets *widgets = prop_widgets_new();
  widgets->vvp = vvp;
  widgets->vtl = vtl;
  widgets->tr = tr;    // NB These should be the 'selected' vikwindow ones.
  widgets->self = self;
  // The first width value doesn't really make any difference,
  //  as the array values will get recalculated in the redraw_signal_event() with the latest widget size allocation
  //  however here ww use an indicative size, which is useful to determine how quick the processing is
  widgets->profile_width = 600;
  widgets->profile_height = 100;

  widgets->track_length_inc_gaps = vik_track_get_length_including_gaps ( tr );
  widgets->show_dem = main_show_dem;
  widgets->show_alt_gps_speed = main_show_alt_gps_speed;
  widgets->show_gps_speed = main_show_gps_speed;

  GtkWidget *graphs = gtk_notebook_new ( );
  // By storing the graphs here & then deleting on close, means any associated signals also get removed
  //  otherwise signals would keep being called with stale values (or need to manually delete the signals)
  widgets->graphs = graphs;
  gtk_notebook_set_tab_pos ( GTK_NOTEBOOK(graphs), GTK_POS_RIGHT ); // Maybe allow config of Left/Right?

  widgets->elev_box = vik_trw_layer_create_profile ( GTK_WIDGET(parent), widgets );
  widgets->speed_box = vik_trw_layer_create_vtdiag ( GTK_WIDGET(parent), widgets );

  if ( widgets->elev_box ) {
    gtk_notebook_append_page ( GTK_NOTEBOOK(graphs), widgets->elev_box, gtk_label_new(_("Elevation-distance")) );
    g_object_set ( widgets->elev_box, "has-tooltip", TRUE, NULL );
    g_signal_connect ( widgets->elev_box, "query-tooltip", G_CALLBACK(graph_tooltip_cb), widgets );
  }

  if ( widgets->speed_box ) {
    gtk_notebook_append_page ( GTK_NOTEBOOK(graphs), widgets->speed_box, gtk_label_new(_("Speed-time")) );
    g_object_set ( widgets->speed_box, "has-tooltip", TRUE, NULL );
    g_signal_connect ( widgets->speed_box, "query-tooltip", G_CALLBACK(graph_tooltip_cb), widgets );
  }

  // If no elevation or time info then don't show anything
  if ( !widgets->elev_box && !widgets->speed_box ) {
    prop_widgets_free ( widgets );
    return NULL;
  }

  gtk_container_add ( GTK_CONTAINER(self), graphs );

  // Ensure can reize down to a small size
  gtk_widget_set_size_request ( self, 0, 0 );

  // Only display the widgets if the graphs are to be shown
  if ( show ) {
    gtk_widget_show_all ( self );
  }

  if ( main_last_graph == PROPWIN_GRAPH_TYPE_SPEED_TIME ) {
    gtk_notebook_set_current_page ( GTK_NOTEBOOK(widgets->graphs), 1 );
  } else {
    gtk_notebook_set_current_page ( GTK_NOTEBOOK(widgets->graphs), 0 );
  }

  g_signal_connect ( G_OBJECT(graphs), "expose-event", G_CALLBACK(redraw_signal_event), widgets );
  // NB We get an initial expose-event so don't need to force a first draw

  return (gpointer)widgets;
}

/**
 * Save any state values and then
 *  free any allocations when the propwin was shown in the main display
 */
void vik_trw_layer_propwin_main_close ( gpointer self )
{
  PropWidgets *widgets = (PropWidgets*)self;

  // Specific save values for embedded graphs (different from dialog method)
  main_show_dem = widgets->show_dem;
  main_show_alt_gps_speed = widgets->show_alt_gps_speed;
  main_show_gps_speed = widgets->show_gps_speed;

  gint page = gtk_notebook_get_current_page ( GTK_NOTEBOOK(widgets->graphs) );
  if ( page == 1 )
    main_last_graph = PROPWIN_GRAPH_TYPE_SPEED_TIME;
  else
    main_last_graph = PROPWIN_GRAPH_TYPE_ELEVATION_DISTANCE;

  // The manually destroy widgets
  // NB otherwise not currently destroyed when shown in the main window
  //  since in the dialog version these should get destroyed automatically by the overall dialog being destroyed
  if ( widgets->elev_box )
    gtk_widget_destroy ( widgets->elev_box );
  if ( widgets->speed_box )
    gtk_widget_destroy ( widgets->speed_box );
  if ( widgets->graphs )
    gtk_widget_destroy ( widgets->graphs );

  prop_widgets_free ( widgets );
}

/**
 * Get the current track being displayed
 */
vik_trw_and_track_t vik_trw_layer_propwin_main_get_track ( gpointer self )
{
  PropWidgets *widgets = (PropWidgets*)self;
  vik_trw_and_track_t vt;
  vt.trk = widgets->tr;
  vt.vtl = widgets->vtl;
  return vt;
}
