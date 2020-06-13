/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2020, Rob Norris <rw_norris@hotmail.com>
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
#include "viking.h"
#include "viktrwlayer_propwin.h"
#include "dems.h"
#include "degrees_converters.h"

#define BLOB_SIZE 6

// NB PGT_SPEED_TIME is now (processed) first so that speed information
// can be available for use on the other graphs
typedef enum {
  PGT_ELEVATION_DISTANCE = 0,
  PGT_GRADIENT_DISTANCE,
  PGT_SPEED_TIME,
  PGT_DISTANCE_TIME,
  PGT_ELEVATION_TIME,
  PGT_SPEED_DISTANCE,
  PGT_HEART_RATE, // NB Only doing a time based graph ATM
  PGT_CADENCE,    // NB Only doing a time based graph ATM
  PGT_TEMP,       // NB Only doing a time based graph ATM
  PGT_POWER,      // NB Only doing a time based graph ATM
  PGT_END,
} VikPropWinGraphType_t;

static const gdouble chunks[] = {0.1, 0.2, 0.5, 1.0, 2.0, 4.0, 5.0, 8.0, 10.0,
                                 15.0, 20.0, 25.0, 40.0, 50.0, 75.0,
                                 100.0, 150.0, 200.0, 250.0, 375.0, 500.0,
                                 750.0, 1000.0, 2000.0, 5000.0, 10000.0, 100000.0};

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
static gboolean show_dem[PGT_END] = { TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };
// Each is either Interpolated speed or GPS Speed
static gboolean show_speed[PGT_END] = { TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };
static gboolean main_show_dem = TRUE;
static gboolean main_show_gps_speed = TRUE;
static VikPropWinGraphType_t main_last_graph = PGT_ELEVATION_DISTANCE;
static gint stats_height = 0;

typedef struct _propsaved {
  gboolean saved;
  GdkImage *img;
} PropSaved;

typedef gpointer ui_change_values[UI_CHG_LAST];

typedef gdouble* (*make_map_func) (const VikTrack*, guint16);
typedef void (*convert_values_func) (gdouble* values, guint profile_width);
typedef void (*get_y_text_func) (gchar* ss, guint size, gdouble value);
typedef void (*draw_extra_func) (gpointer widgets, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt);
typedef void (*button_update_func) (VikTrackpoint* trackpoint, gpointer widgets, gdouble from_start, guint ix, VikPropWinGraphType_t pwgt);

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
  gboolean  stats_configured;
  GtkWidget *w_comment;
  GtkWidget *w_description;
  GtkWidget *w_source;
  GtkWidget *w_number;
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
  GtkWidget *w_cur_value1[PGT_END];
  GtkWidget *w_cur_value2[PGT_END];
  GtkWidget *w_cur_value3[PGT_END];
  GtkWidget *w_show_dem[PGT_END];
  GtkWidget *w_show_speed[PGT_END];
  gboolean  show_speed[PGT_END];
  gboolean  show_dem[PGT_END];
  gdouble   track_length;
  gdouble   track_length_inc_gaps;
  PropSaved graph_saved_img[PGT_END];
  GtkWidget* event_box[PGT_END];
  GtkWidget* image[PGT_END];
  gdouble   alt_create_time;
  gdouble   min_value[PGT_END];
  gdouble   max_value[PGT_END];
  gdouble   draw_min[PGT_END];
  guint     ci[PGT_END];
  gboolean  user_set_axis; // Whether to use specific values or otherwise auto calculate best fit
  guint     user_cia; // Chunk size set by the user (only for altitude graph ATM)
  gdouble   user_mina;
  gdouble   **values;
  make_map_func make_map[PGT_END];
  convert_values_func convert_values[PGT_END];
  get_y_text_func get_y_text[PGT_END];
  draw_extra_func draw_extra[PGT_END];
  button_update_func button_update[PGT_END];
  gboolean speeds_evaluated;
  //
  VikTrackpoint *marker_tp;
  gboolean  is_marker_drawn;
  VikTrackpoint *blob_tp;
  gboolean  is_blob_drawn;
  gdouble   duration;
  gchar     *tz; // TimeZone at track's location
  VikCoord  vc;  // Center of track
} PropWidgets;

// Local functions
static gboolean split_at_marker ( PropWidgets *widgets );
static void draw_all_graphs ( GtkWidget *widget, PropWidgets *widgets, gboolean resized );
static GtkWidget *create_statistics_page ( PropWidgets *widgets, VikTrack *tr );
static GtkWidget *create_splits_tables ( VikTrack *trk );

static PropWidgets *prop_widgets_new()
{
  PropWidgets *widgets = g_malloc0(sizeof(PropWidgets));
  widgets->values = (gdouble**)g_malloc0(PGT_END*sizeof(gdouble*));
  return widgets;
}

static void prop_widgets_free(PropWidgets *widgets)
{
  for ( VikPropWinGraphType_t pwgt = 0; pwgt < PGT_END; pwgt++ ) {
    if ( widgets->graph_saved_img[pwgt].img )
      g_object_unref ( widgets->graph_saved_img[pwgt].img );
    if ( widgets->values[pwgt] )
     g_free ( widgets->values[pwgt] );
  }
  g_free ( widgets->values );
  g_free(widgets);
}

#define TPW_PREFS_GROUP_KEY "track.propwin"
#define TPW_PREFS_NS "track.propwin."

static VikLayerParam prefs[] = {
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"tabs_on_side", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Tabs on the side:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_splits", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show splits tab:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_elev_dist", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Elevation-distance graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_elev_time", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Elevation-time graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_grad_dist", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Gradient-distance graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_speed_time", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Speed-time graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_speed_dist", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Speed-distance graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_dist_time", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Distance-time graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_heart_rate", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Heart Rate graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_cadence", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Cadence graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_temp", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Temperature graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, TPW_PREFS_NS"show_power", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Power graph:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
};

void vik_trw_layer_propwin_init ()
{
  a_preferences_register_group ( TPW_PREFS_GROUP_KEY, _("Track Properties Dialog") );
  for ( guint ii = 0; ii < G_N_ELEMENTS(prefs); ii++ )
    a_preferences_register ( &prefs[ii], (VikLayerParamData){0}, TPW_PREFS_GROUP_KEY );
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
static void get_new_min_and_chunk_index (gdouble mina, gdouble maxa, const gdouble *chunks, size_t chunky, gdouble *new_min, guint *ci)
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
  guint last_chunk = G_N_ELEMENTS(chunks);

  // Loop through to find best match
  while (mylength > chunks[ci]) {
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
  if ( isnan(t_start) || isnan(t_end) || isnan(trackpoint->timestamp) )
    return pc;
  t_total = t_end - t_start;
  pc = (trackpoint->timestamp - t_start)/t_total;
  if ( pc < 0.0 || pc > 1.0 )
    pc = NAN;
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
  widgets->show_dem[PGT_ELEVATION_DISTANCE] = !widgets->show_dem[PGT_ELEVATION_DISTANCE];
  // Force redraw
  draw_all_graphs ( GTK_WIDGET(widgets->graphs), widgets, TRUE );
  return FALSE;
}

static gboolean menu_show_gps_speed_cb ( PropWidgets *widgets )
{
  widgets->show_speed[PGT_SPEED_TIME] = !widgets->show_speed[PGT_SPEED_TIME];
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

static gboolean stats_configure_event ( GtkWidget *widget, GdkEventConfigure *event, PropWidgets *widgets )
{
  if ( !widgets->stats_configured ) {
    widgets->stats_configured = TRUE;

    // Allow sizing back down to the minimum
    GdkGeometry geom = { event->width, event->height, 0, 0, 0, 0, 0, 0, 0, 0, GDK_GRAVITY_STATIC };
    gtk_window_set_geometry_hints ( GTK_WINDOW(widget), widget, &geom, GDK_HINT_MIN_SIZE );

    // As a scrollable window don't start on a too small size
    // Restore previous size (if one was set) only in vertical direction
    stats_height = stats_height ? stats_height : 333 * vik_viewport_get_scale(widgets->vvp);
    gtk_window_resize ( GTK_WINDOW(widget), event->width, stats_height );
  }
  stats_height = event->height;
  return FALSE;
}

static void stats_destroy_cb ( GtkDialog *dialog, PropWidgets *widgets )
{
  // Reset configured to enable new stats dialog instance
  widgets->stats_configured = FALSE;
}

static gboolean menu_statistics_cb ( PropWidgets *widgets )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons ( widgets->tr->name,
						    GTK_WINDOW(gtk_widget_get_toplevel(widgets->self)),
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);

  GtkWidget *nb = gtk_notebook_new();

  gtk_notebook_append_page ( GTK_NOTEBOOK(nb), create_statistics_page(widgets, widgets->tr), gtk_label_new(_("Statistics")) );

  if ( widgets->event_box[PGT_SPEED_TIME] )
    gtk_notebook_append_page ( GTK_NOTEBOOK(nb), create_splits_tables(widgets->tr), gtk_label_new(_("Splits")) );

  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), nb, TRUE, TRUE, 0 );

  gtk_widget_show_all ( dialog );

  g_signal_connect ( G_OBJECT(dialog), "configure-event", G_CALLBACK(stats_configure_event), widgets );
  g_signal_connect ( G_OBJECT(dialog), "destroy", G_CALLBACK(stats_destroy_cb), widgets );

  (void)gtk_dialog_run ( GTK_DIALOG(dialog) );

  gtk_widget_destroy ( dialog );

  return FALSE;
}

static gboolean menu_edit_trkpt_cb ( PropWidgets *widgets )
{
  trw_layer_tpwin_init ( widgets->vtl );
  return FALSE;
}

static gboolean menu_center_view_cb ( PropWidgets *widgets )
{
  vik_trw_layer_center_view_track ( widgets->vtl, widgets->tr, widgets->vvp, widgets->vlp );
  return FALSE;
}

/**
 * Allow configuration of the X (height) axis
 * One use of being able to manually select the values,
 *  is that one can compare different tracks more easily
 *  (by ensuring each has the same values)
 */
static gboolean menu_axis_cb ( PropWidgets *widgets )
{
  GtkWidget *ww = gtk_widget_get_toplevel ( widgets->dialog ? widgets->dialog : widgets->self );
  GtkWidget *dialog = gtk_dialog_new_with_buttons ( _("Axis Control"),
						    GTK_WINDOW(ww),
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_APPLY, GTK_RESPONSE_APPLY, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
						    NULL );
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
#endif

  GtkWidget *dlabel = gtk_label_new ( _("Height Divisions:") );
  GtkWidget *combo = vik_combo_box_text_new();
  for ( guint xx = 0; xx < G_N_ELEMENTS(chunks); xx++ ) {
    gchar *str = g_strdup_printf ( "%.1f", chunks[xx] );
    vik_combo_box_text_append (GTK_COMBO_BOX(combo), str );
    g_free ( str );
  }
  gtk_combo_box_set_active ( GTK_COMBO_BOX(combo), widgets->ci[PGT_ELEVATION_DISTANCE] );

  vik_units_height_t height_units = a_vik_get_units_height ();
  gchar tmp_str[64];
  switch (height_units) {
  case VIK_UNITS_HEIGHT_FEET:
    g_snprintf ( tmp_str, sizeof(tmp_str), "%.1f", VIK_METERS_TO_FEET(widgets->draw_min[PGT_ELEVATION_DISTANCE]) );
    break;
  default:
    g_snprintf ( tmp_str, sizeof(tmp_str), "%.1f", widgets->draw_min[PGT_ELEVATION_DISTANCE] );
    break;
  }

  GtkWidget *mlabel = gtk_label_new ( _("Minimum Height:") );
  GtkWidget *mentry = ui_entry_new ( tmp_str, GTK_ENTRY_ICON_SECONDARY );

  GtkTable *box = GTK_TABLE(gtk_table_new(2, 2, FALSE));
  gtk_table_attach_defaults ( box, mlabel, 0, 1, 0, 1);
  gtk_table_attach_defaults ( box, mentry, 1, 2, 0, 1);
  gtk_table_attach_defaults ( box, dlabel, 0, 1, 1, 2);
  gtk_table_attach_defaults ( box, combo, 1, 2, 1, 2);

  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), GTK_WIDGET(box), FALSE, FALSE, 5 );

  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  gtk_widget_show_all ( dialog );

  gint resp = GTK_RESPONSE_APPLY;
  while ( resp == GTK_RESPONSE_APPLY ) {
    resp = gtk_dialog_run (GTK_DIALOG (dialog));
    if ( resp == GTK_RESPONSE_ACCEPT || resp == GTK_RESPONSE_APPLY ) {
      widgets->user_set_axis = TRUE;
      widgets->user_cia = gtk_combo_box_get_active ( GTK_COMBO_BOX(combo) );

      gchar const *mtext = gtk_entry_get_text ( GTK_ENTRY(mentry) );
      if ( mtext && strlen(mtext) ) {
        // Always store in metres
        switch (height_units) {
        case VIK_UNITS_HEIGHT_FEET:
          widgets->user_mina = VIK_FEET_TO_METERS(atof(mtext));
          break;
        default:
          // VIK_UNITS_HEIGHT_METRES:
          widgets->user_mina = atof ( mtext );
        }
      } else
        widgets->user_mina = widgets->draw_min[PGT_ELEVATION_DISTANCE];
    }

    if ( resp == GTK_RESPONSE_APPLY )
      draw_all_graphs ( widgets->dialog ? widgets->dialog : widgets->graphs, widgets, TRUE );
  }

  gtk_widget_destroy ( dialog );

  if ( resp == GTK_RESPONSE_ACCEPT )
    draw_all_graphs ( widgets->dialog ? widgets->dialog : widgets->graphs, widgets, TRUE );

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
  (void)vu_menu_add_item ( GTK_MENU(menu), widgets->tr->is_route ? _("_Center View on Route") : _("_Center View on Track"),
                           GTK_STOCK_ZOOM_FIT, G_CALLBACK(menu_center_view_cb), widgets );
  GtkWidget *itp = vu_menu_add_item ( GTK_MENU(menu), _("_Edit Trackpoint..."), NULL, G_CALLBACK(menu_edit_trkpt_cb), widgets );
  gtk_widget_set_sensitive ( GTK_WIDGET(itp), a_vik_get_auto_trackpoint_select() &&
                                              !trw_layer_tpwin_is_shown(widgets->vtl) );
  GtkWidget *ism = vu_menu_add_item ( GTK_MENU(menu), _("Split at Marker"), GTK_STOCK_CUT, G_CALLBACK(menu_split_at_marker_cb), widgets );
  gtk_widget_set_sensitive ( ism, (gboolean)GPOINTER_TO_INT(widgets->marker_tp) );

  (void)vu_menu_add_item ( GTK_MENU(menu), _("Axis Control..."), NULL, G_CALLBACK(menu_axis_cb), widgets );

  // Only for the embedded graphs
  GtkMenu *show_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *items = vu_menu_add_item ( GTK_MENU(menu), _("_Show"), NULL, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(items), GTK_WIDGET(show_submenu) );

  // Ensure item value is set before adding the callback on these check menu items
  //  otherwise the callback may be invoked when we set the value!
  if ( graph_type == PGT_ELEVATION_DISTANCE ) {
    GtkWidget *id = gtk_check_menu_item_new_with_mnemonic ( _("_DEM") );
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(id), widgets->show_dem[PGT_ELEVATION_DISTANCE] );
    g_signal_connect_swapped ( G_OBJECT(id), "toggled", G_CALLBACK(menu_show_dem_cb), widgets );
    gtk_menu_shell_append ( GTK_MENU_SHELL(show_submenu), id );
    gtk_widget_set_sensitive ( id, a_dems_overlaps_bbox (widgets->tr->bbox) );
  }

  if ( graph_type == PGT_SPEED_TIME ) {
    GtkWidget *ig = gtk_check_menu_item_new_with_mnemonic ( _("_GPS Speed") );
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(ig), widgets->show_speed[PGT_SPEED_TIME] );
    g_signal_connect_swapped ( G_OBJECT(ig), "toggled", G_CALLBACK(menu_show_gps_speed_cb), widgets );
    gtk_menu_shell_append ( GTK_MENU_SHELL(show_submenu), ig );
  }

  gtk_widget_show_all ( menu );
  gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time());
}

/**
 *
 */
static void elev_dialog_click_menu_popup ( PropWidgets *widgets )
{
  GtkWidget *menu = gtk_menu_new ();
  (void)vu_menu_add_item ( GTK_MENU(menu), _("Axis Control..."), NULL, G_CALLBACK(menu_axis_cb), widgets );
  gtk_widget_show_all ( menu );
  gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time());
}

static gboolean is_time_graph ( VikPropWinGraphType_t pwgt )
{
  return ( pwgt == PGT_SPEED_TIME ||
           pwgt == PGT_DISTANCE_TIME ||
           pwgt == PGT_ELEVATION_TIME ||
           pwgt == PGT_HEART_RATE ||
           pwgt == PGT_CADENCE ||
           pwgt == PGT_TEMP ||
           pwgt == PGT_POWER );
}

static void track_graph_click( GtkWidget *event_box, GdkEventButton *event, PropWidgets *widgets, VikPropWinGraphType_t graph_type )
{
  // Only use primary clicks for marker position
  if ( event->button != 1 ) {
    // 'right' click for menu
    if ( event->button == 3 ) {
      if ( widgets->self )
        graph_click_menu_popup ( widgets, graph_type );
      else if ( graph_type == PGT_ELEVATION_DISTANCE )
        elev_dialog_click_menu_popup ( widgets );
    }
    return;
  }

  gboolean time_graph = is_time_graph ( graph_type );

  GtkAllocation allocation;
  gtk_widget_get_allocation ( event_box, &allocation );

  VikTrackpoint *trackpoint = set_center_at_graph_position(event->x, allocation.width, widgets->vtl, widgets->vlp, widgets->vvp, widgets->tr, time_graph, widgets->profile_width);
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
  VikPropWinGraphType_t graphite;
  for ( graphite = 0;
	graphite < PGT_END;
	graphite++ ) {

    graph_saved_img = &widgets->graph_saved_img[graphite];
    image           = widgets->image[graphite];
    graph_box       = widgets->event_box[graphite];

    // Commonal method of redrawing marker
    if ( graph_box ) {

      if ( is_time_graph(graphite) )
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

static VikPropWinGraphType_t event_box_to_graph_type ( GtkWidget *event_box, PropWidgets *widgets )
{
  VikPropWinGraphType_t pwgt = PGT_ELEVATION_DISTANCE;
  if ( event_box == widgets->event_box[PGT_GRADIENT_DISTANCE] )
    pwgt = PGT_GRADIENT_DISTANCE;
  else if ( event_box == widgets->event_box[PGT_SPEED_TIME] )
    pwgt = PGT_SPEED_TIME;
  else if ( event_box == widgets->event_box[PGT_DISTANCE_TIME] )
    pwgt = PGT_DISTANCE_TIME;
  else if ( event_box == widgets->event_box[PGT_ELEVATION_TIME] )
    pwgt = PGT_ELEVATION_TIME;
  else if ( event_box == widgets->event_box[PGT_SPEED_DISTANCE] )
    pwgt = PGT_SPEED_DISTANCE;
  else if ( event_box == widgets->event_box[PGT_HEART_RATE] )
    pwgt = PGT_HEART_RATE;
  else if ( event_box == widgets->event_box[PGT_CADENCE] )
    pwgt = PGT_CADENCE;
  else if ( event_box == widgets->event_box[PGT_TEMP] )
    pwgt = PGT_TEMP;
  else if ( event_box == widgets->event_box[PGT_POWER] )
    pwgt = PGT_POWER;
  return pwgt;
}

static gboolean track_graph_click_cb ( GtkWidget *event_box, GdkEventButton *event, gpointer ptr )
{
  track_graph_click ( event_box, event, ptr, event_box_to_graph_type(event_box, ptr) );
  return TRUE; // don't call other (further) callbacks
}

/**
 * Calculate y position for blob on any graph
 */
static guint blob_y_position ( guint ix, PropWidgets *widgets, VikPropWinGraphType_t pwgt )
{
  // Shouldn't really happen but might if deleted all points whilst graph shown
  if ( !widgets->values[pwgt] )
    return 0;

  gdouble value = widgets->values[pwgt][ix];
  gdouble draw_min = widgets->draw_min[pwgt];
  guint ci = widgets->ci[pwgt];
  // User override?
  if ( widgets->user_set_axis && pwgt == PGT_ELEVATION_DISTANCE ) {
    ci = widgets->user_cia;
    draw_min = widgets->user_mina;
  }
  guint y_blob = widgets->profile_height-widgets->profile_height*(value-draw_min)/(chunks[ci]*LINES);
  return y_blob;
}

static void get_distance_text ( gchar* buf, guint size, gdouble meters_from_start )
{
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  vu_distance_text ( buf, size, dist_units, meters_from_start, TRUE, "%.2f", FALSE );
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

static void update_elevation_distance_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble meters_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    static gchar tmp_buf[20];
    get_distance_text ( tmp_buf, sizeof(tmp_buf), meters_from_start );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt]), tmp_buf );
  }

  // Show track elevation for this position - to the nearest whole number
  if ( trackpoint && widgets->w_cur_value2[pwgt] ) {
    static gchar tmp_buf[20];
    get_altitude_text ( tmp_buf, sizeof(tmp_buf), trackpoint );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value2[pwgt]), tmp_buf );
  }
}

static void update_gradient_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble meters_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    static gchar tmp_buf[20];
    get_distance_text ( tmp_buf, sizeof(tmp_buf), meters_from_start );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt] ), tmp_buf);
  }

  // Show track gradient for this position - to the nearest whole number
  if ( trackpoint && widgets->w_cur_value2[pwgt] ) {
    static gchar tmp_buf[20];
    double gradient = round(widgets->values[PGT_GRADIENT_DISTANCE][ix]);
    g_snprintf(tmp_buf, sizeof(tmp_buf), "%d%%", (int)gradient);
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value2[pwgt]), tmp_buf );
  }
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
static void real_time_label_update ( PropWidgets *widgets, GtkWidget *widget, VikTrackpoint *trackpoint )
{
  if ( !isnan(trackpoint->timestamp) ) {
    time_t ts = round ( trackpoint->timestamp );
    // Alternatively could use %c format but I prefer a slightly more compact form here
    //  The full date can of course be seen on the Statistics tab
    gchar *msg = vu_get_time_string ( &ts, "%X %x %Z", &widgets->vc, widgets->tz );
    gtk_label_set_text ( GTK_LABEL(widget), msg );
    g_free ( msg );
  }
  else {
    static gchar tmp_buf[64];
    g_snprintf (tmp_buf, sizeof(tmp_buf), _("No Data"));
    gtk_label_set_text ( GTK_LABEL(widget), tmp_buf );
  }
}

static void update_speed_time_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble seconds_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] )
    time_label_update ( widgets->w_cur_value1[pwgt], seconds_from_start );

  // Show track speed for this position
  if ( trackpoint && widgets->w_cur_value2[pwgt] ) {
    static gchar tmp_buf[20];
    // Even if GPS speed available (trackpoint->speed), the text will correspond to the speed map shown
    // No conversions needed as already in appropriate units
    vik_units_speed_t speed_units = a_vik_get_units_speed ();
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, widgets->values[pwgt][ix], FALSE, "%.1f", FALSE );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value2[pwgt]), tmp_buf );
  }

  if ( trackpoint && widgets->w_cur_value3[pwgt] )
    real_time_label_update ( widgets, widgets->w_cur_value3[pwgt], trackpoint );
}

static void update_distance_time_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble seconds_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    static gchar tmp_buf[20];
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    // Value already in the correct units
    vu_distance_text ( tmp_buf, sizeof(tmp_buf), dist_units, widgets->values[pwgt][ix], FALSE, "%.2f", FALSE );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt]), tmp_buf );
  }

  if ( trackpoint && widgets->w_cur_value2[pwgt] )
    time_label_update ( widgets->w_cur_value2[pwgt], seconds_from_start );

  if ( trackpoint && widgets->w_cur_value3[pwgt] )
    real_time_label_update ( widgets, widgets->w_cur_value3[pwgt], trackpoint );
}

static void update_elevation_time_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble seconds_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] )
    time_label_update ( widgets->w_cur_value1[pwgt], seconds_from_start );

  if ( trackpoint && widgets->w_cur_value2[pwgt] ) {
    static gchar tmp_buf[20];
    if (a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET)
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%d ft", (int)VIK_METERS_TO_FEET(trackpoint->altitude));
    else
      g_snprintf(tmp_buf, sizeof(tmp_buf), "%d m", (int)trackpoint->altitude);
    gtk_label_set_text(GTK_LABEL(widgets->w_cur_value2[pwgt]), tmp_buf);
  }

  if ( trackpoint && widgets->w_cur_value3[pwgt] )
    real_time_label_update ( widgets, widgets->w_cur_value3[pwgt], trackpoint );
}

static void update_speed_distance_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble meters_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    static gchar tmp_buf[20];
    get_distance_text ( tmp_buf, sizeof(tmp_buf), meters_from_start );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt]), tmp_buf );
  }

  // Show track speed for this position
  if ( widgets->w_cur_value2[pwgt] ) {
    static gchar tmp_buf[20];
    // Even if GPS speed available (trackpoint->speed), the text will correspond to the speed map shown
    // No conversions needed as already in appropriate units
    vik_units_speed_t speed_units = a_vik_get_units_speed ();
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, widgets->values[pwgt][ix], FALSE, "%.1f", FALSE );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value2[pwgt]), tmp_buf );
  }
}

static void update_hr_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble seconds_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    gchar tmp_buf[32];
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d bpm"), trackpoint->heart_rate);
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt]), tmp_buf );
  }

  if ( trackpoint && widgets->w_cur_value2[pwgt] )
    time_label_update ( widgets->w_cur_value2[pwgt], seconds_from_start );
}

static void update_cad_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble seconds_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    gchar tmp_buf[32];
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d RPM"), trackpoint->cadence);
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt]), tmp_buf );
  }

  if ( trackpoint && widgets->w_cur_value2[pwgt] )
    time_label_update ( widgets->w_cur_value2[pwgt], seconds_from_start );
}

static void update_temp_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble seconds_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    gchar tmp_buf[32];
    if ( a_vik_get_units_temp() == VIK_UNITS_TEMP_CELSIUS )
      g_snprintf ( tmp_buf, sizeof(tmp_buf), "%.1f%sC", trackpoint->temp, DEGREE_SYMBOL );
    else
      g_snprintf ( tmp_buf, sizeof(tmp_buf), "%.1f%sF", VIK_CELSIUS_TO_FAHRENHEIT(trackpoint->temp), DEGREE_SYMBOL );
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt]), tmp_buf );
  }

  if ( trackpoint && widgets->w_cur_value2[pwgt] )
    time_label_update ( widgets->w_cur_value2[pwgt], seconds_from_start );
}

static void update_power_buttons ( VikTrackpoint *trackpoint, gpointer ptr, gdouble seconds_from_start, guint ix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( trackpoint && widgets->w_cur_value1[pwgt] ) {
    gchar tmp_buf[32];
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d Watts"), trackpoint->power);
    gtk_label_set_text ( GTK_LABEL(widgets->w_cur_value1[pwgt]), tmp_buf );
  }

  if ( trackpoint && widgets->w_cur_value2[pwgt] )
    time_label_update ( widgets->w_cur_value2[pwgt], seconds_from_start );
}

static void track_graph_move ( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
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

  VikPropWinGraphType_t pwgt = event_box_to_graph_type ( event_box, widgets );
  gdouble from_start;
  VikTrackpoint *trackpoint = NULL;
  if ( is_time_graph(pwgt) )
    trackpoint = vik_track_get_closest_tp_by_percentage_time ( widgets->tr, (gdouble) x / widgets->profile_width, &from_start );
  else
    trackpoint = vik_track_get_closest_tp_by_percentage_dist ( widgets->tr, (gdouble) x / widgets->profile_width, &from_start );

  widgets->blob_tp = trackpoint;

  guint ix = (guint)x;
  // Ensure ix is inbounds
  if ( ix == widgets->profile_width )
    ix--;

  widgets->button_update[pwgt] ( trackpoint, widgets, from_start, ix, pwgt );

  guint y_blob = blob_y_position ( ix, widgets, pwgt );

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if ( widgets->is_marker_drawn ) {
    gdouble pc = NAN;
    if ( is_time_graph(pwgt) )
      pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );
    else
      pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
    if ( !isnan(pc) ) {
      marker_x = (pc * widgets->profile_width) + MARGIN_X;
    }
  }

  GtkWidget *window = gtk_widget_get_toplevel ( event_box );
  save_image_and_draw_graph_marks ( widgets->image[pwgt],
                                    marker_x,
                                    gtk_widget_get_style(window)->black_gc,
                                    MARGIN_X+x,
                                    MARGIN_Y+y_blob,
                                    &widgets->graph_saved_img[pwgt],
                                    widgets->profile_width,
                                    widgets->profile_height,
                                    BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
                                    &widgets->is_marker_drawn,
                                    &widgets->is_blob_drawn );

  if ( widgets->graphs && widgets->blob_tp )
    vik_trw_layer_trackpoint_draw ( widgets->vtl, widgets->vvp, widgets->tr, widgets->blob_tp);
}

static void track_graph_leave ( GtkWidget *event_box, GdkEventMotion *event, PropWidgets *widgets )
{
  vik_trw_layer_trackpoint_draw ( widgets->vtl, widgets->vvp, NULL, NULL );
}

/**
 * Draws DEM points and a respresentative speed on the supplied pixmap
 *  Pixmap x axis should be distance based
 */
static void draw_dem_alt_speed_dist ( VikTrack *tr,
                                      GdkDrawable *pix,
                                      GdkGC *alt_gc,
                                      GdkGC *speed_gc,
                                      gdouble alt_offset,
                                      gdouble draw_min_speed,
                                      guint cia,
                                      guint cis,
                                      guint width,
                                      guint height,
                                      guint margin,
                                      gboolean do_dem,
                                      gboolean do_speed )
{
  GList *iter;
  gdouble total_length = vik_track_get_length_including_gaps(tr);

  gdouble dist = 0;
  gint h2 = height + MARGIN_Y; // Adjust height for x axis labelling offset
  gdouble achunk = chunks[cia]*LINES;
  gdouble schunk = chunks[cis]*LINES;
  vik_units_speed_t speed_units = a_vik_get_units_speed ();

  for (iter = tr->trackpoints->next; iter; iter = iter->next) {
    dist += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
			     &(VIK_TRACKPOINT(iter->prev->data)->coord) );
    int x = (width * dist)/total_length + margin;
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
	gdouble spd = vu_speed_convert ( speed_units, VIK_TRACKPOINT(iter->data)->speed ) ;
        int y_speed = h2 - (height * (spd-draw_min_speed))/schunk;
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
  gchar *units = vu_distance_units_text ( dist_units );
  // When smaller values covered by the graph, have a higher precision readout
  if ( ii > 4 )
    label_markup = g_strdup_printf ( "<span size=\"small\">%d %s</span>", (guint)dd, units );
  else
    label_markup = g_strdup_printf ( "<span size=\"small\">%.1f %s</span>", dd, units );
  g_free ( units );

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
  gdouble length = vu_distance_convert ( dist_units, widgets->track_length_inc_gaps);
  guint index = get_distance_chunk_index ( length );
  gdouble dist_per_pixel = length/widgets->profile_width;

  for (guint i=1; chunks[index]*i <= length; i++) {
    draw_grid_x_distance ( window, image, widgets, pix, index, chunks[index]*i, (guint)(chunks[index]*i/dist_per_pixel), dist_units );
  }
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

static void elev_convert ( gdouble *values, guint profile_width )
{
  guint i;
  vik_units_height_t height_units = a_vik_get_units_height ();
  if ( height_units == VIK_UNITS_HEIGHT_FEET ) {
    // Convert altitudes into feet units
    for ( i = 0; i < profile_width; i++ ) {
      values[i] = VIK_METERS_TO_FEET(values[i]);
    }
    // Otherwise leave in metres
  }
}

static void speed_convert ( gdouble *values, guint profile_width )
{
  guint i;
  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  for ( i = 0; i < profile_width; i++ ) {
    values[i] = vu_speed_convert ( speed_units, values[i] );
  }
}

static void dist_convert ( gdouble *values, guint profile_width )
{
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  for ( guint i = 0; i < profile_width; i++ )
    values[i] = vu_distance_convert ( dist_units, values[i] );
}

static void temp_convert ( gdouble *values, guint profile_width )
{
  if ( a_vik_get_units_temp() == VIK_UNITS_TEMP_FAHRENHEIT )
    for ( guint i = 0; i < profile_width; i++ )
      values[i] = VIK_CELSIUS_TO_FAHRENHEIT(values[i]);
}

static void elev_y_text ( gchar *ss, guint size, gdouble value )
{
  vik_units_height_t height_units = a_vik_get_units_height ();
  switch ( height_units ) {
  case VIK_UNITS_HEIGHT_FEET:
    // NB values already converted into feet
    sprintf ( ss, "%8dft", (int)value );
    break;
  default: // VIK_UNITS_HEIGHT_METRES:
    sprintf ( ss, "%8dm", (int)value );
    break;
  }
}

static void dist_y_text ( gchar *ss, guint size, gdouble value )
{
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  vu_distance_text ( ss, size, dist_units, value, FALSE, "%.1f", FALSE );
}

static void pct_y_text ( gchar *ss, guint size, gdouble value )
{
  snprintf ( ss, size, "%8d%%", (int)round(value));
}

static void speed_y_text ( gchar *ss, guint size, gdouble value )
{
  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  vu_speed_text ( ss, size, speed_units, value, FALSE, "%.1f", TRUE );
}

static void hr_y_text ( gchar *ss, guint size, gdouble value )
{
  snprintf ( ss, size, _("%d bpm"), (int)round(value) );
}

static void cad_y_text ( gchar *ss, guint size, gdouble value )
{
  snprintf ( ss, size, _("%d RPM"), (int)round(value) );
}

static void temp_y_text ( gchar *ss, guint size, gdouble value )
{
  snprintf ( ss, size, "%d%s%c", (int)round(value), DEGREE_SYMBOL, a_vik_get_units_temp() == VIK_UNITS_TEMP_CELSIUS ? 'C' : 'F' );
}

static void power_y_text ( gchar *ss, guint size, gdouble value )
{
  snprintf ( ss, size, _("%d Watts"), (int)round(value) );
}

static void draw_dem_gps_speed ( PropWidgets *widgets, GtkWidget *window, GdkPixmap *pix )
{
  GdkGC *dem_alt_gc = gdk_gc_new ( gtk_widget_get_window(window) );
  GdkGC *gps_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );

  GdkColor color;
  gdk_color_parse ( "green", &color );
  gdk_gc_set_rgb_fg_color ( dem_alt_gc, &color);

  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

  gdouble min = widgets->draw_min[PGT_ELEVATION_DISTANCE];
  guint ci = widgets->ci[PGT_ELEVATION_DISTANCE];

  // User override?
  if ( widgets->user_set_axis ) {
    ci = widgets->user_cia;
    min = widgets->user_mina;
  }

  draw_dem_alt_speed_dist ( widgets->tr,
                            GDK_DRAWABLE(pix),
                            dem_alt_gc,
                            gps_speed_gc,
                            min,
                            0.0,
                            ci,
                            widgets->ci[PGT_SPEED_TIME],
                            widgets->profile_width,
                            widgets->profile_height,
                            MARGIN_X,
                            widgets->show_dem[PGT_ELEVATION_DISTANCE],
                            widgets->show_speed[PGT_ELEVATION_DISTANCE] );
    
  g_object_unref ( G_OBJECT(dem_alt_gc) );
  g_object_unref ( G_OBJECT(gps_speed_gc) );
}

/**
 * Need to evaluate speeds (as they have not been yet been done)
 *  typically if the speed-time graph has been deselected from showing
 */
static void evaluate_speeds ( PropWidgets *widgets )
{
  const VikPropWinGraphType_t pwgt = PGT_SPEED_TIME;
  if ( widgets->values[pwgt] )
    g_free ( widgets->values[pwgt] );
  widgets->values[pwgt] = vik_track_make_speed_map ( widgets->tr, widgets->profile_width );
  if ( widgets->values[pwgt] == NULL )
    return;
  speed_convert ( widgets->values[pwgt], widgets->profile_width );
  minmax_array(widgets->values[pwgt], &widgets->min_value[pwgt], &widgets->max_value[pwgt], FALSE, widgets->profile_width);
  // Speeds can't be negative (but make_speed_map might give negatives)
  if ( widgets->min_value[pwgt] < 0.0 )
    widgets->min_value[pwgt] = 0;
  get_new_min_and_chunk_index (widgets->min_value[pwgt], widgets->max_value[pwgt], chunks, G_N_ELEMENTS(chunks), &widgets->draw_min[pwgt], &widgets->ci[pwgt]);
  widgets->speeds_evaluated = TRUE;
}

static void draw_ed_extra ( gpointer ptr, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( widgets->show_dem[pwgt] || widgets->show_speed[pwgt] ) {
    if ( !widgets->speeds_evaluated )
      evaluate_speeds ( widgets );
    draw_dem_gps_speed ( widgets, window, pix );
  }
}

static void draw_gps_speed_by_dist ( PropWidgets *widgets, GtkWidget *window, GdkPixmap *pix )
{
  GdkGC *gc = gdk_gc_new ( gtk_widget_get_window(window) );

  GdkColor color;
  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( gc, &color);

  draw_dem_alt_speed_dist ( widgets->tr,
                            GDK_DRAWABLE(pix),
                            NULL,
                            gc,
                            0.0,
                            widgets->draw_min[PGT_SPEED_TIME],
                            0,
                            widgets->ci[PGT_SPEED_TIME],
                            widgets->profile_width,
                            widgets->profile_height,
                            MARGIN_X,
                            FALSE,
                            TRUE );
  g_object_unref ( G_OBJECT(gc) );
}

static void draw_gps_speed_extra ( gpointer ptr, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( widgets->show_speed[pwgt] ) {
    if ( !widgets->speeds_evaluated )
      evaluate_speeds ( widgets );
    draw_gps_speed_by_dist ( widgets, window, pix );
  }
}

static void draw_speed_indicator ( PropWidgets *widgets, GtkWidget *window, GdkPixmap *pix )
{
  GdkGC *speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );
  GdkColor color;
  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( speed_gc, &color);

  gdouble max_speed = widgets->max_value[PGT_SPEED_TIME] * 110 / 100;

  // This is just an indicator - no actual values can be inferred by user
  for (int i = 0; i < widgets->profile_width; i++ ) {
    int y_speed = widgets->profile_height - (widgets->profile_height * widgets->values[PGT_SPEED_TIME][i])/max_speed;
    gdk_draw_rectangle ( GDK_DRAWABLE(pix), speed_gc, TRUE, i+MARGIN_X-2, y_speed-2, 4, 4 );
  }
  g_object_unref ( G_OBJECT(speed_gc) );
}

static void draw_gps_speed_by_time ( PropWidgets *widgets, GtkWidget *window, GdkPixmap *pix )
{
  // Use speed time graph for determining pixel positions,
  //  although the drawing is put on whatever the pixmap is passed in
  VikPropWinGraphType_t pwgt = PGT_SPEED_TIME;
  GdkGC *gps_speed_gc = gdk_gc_new ( gtk_widget_get_window(window) );
  GdkColor color;
  gdk_color_parse ( "red", &color );
  gdk_gc_set_rgb_fg_color ( gps_speed_gc, &color);

  gdouble beg_time = VIK_TRACKPOINT(widgets->tr->trackpoints->data)->timestamp;
  gdouble dur = VIK_TRACKPOINT(g_list_last(widgets->tr->trackpoints)->data)->timestamp - beg_time;

  gdouble chunk = chunks[widgets->ci[pwgt]];
  vik_units_speed_t speed_units = a_vik_get_units_speed ();
  guint height = widgets->profile_height + MARGIN_Y;
  gdouble mins = widgets->draw_min[pwgt];

  for (GList *iter = widgets->tr->trackpoints; iter; iter = iter->next) {
    gdouble gps_speed = VIK_TRACKPOINT(iter->data)->speed;
    if (isnan(gps_speed))
      continue;

    gps_speed = vu_speed_convert ( speed_units, gps_speed );

    int x = MARGIN_X + widgets->profile_width * (VIK_TRACKPOINT(iter->data)->timestamp - beg_time) / dur;
    int y = height - widgets->profile_height*(gps_speed - mins)/(chunk*LINES);
    gdk_draw_rectangle(GDK_DRAWABLE(pix), gps_speed_gc, TRUE, x-2, y-2, 4, 4);
  }
  g_object_unref ( G_OBJECT(gps_speed_gc) );
}

static void draw_vt_gps_speed_extra ( gpointer ptr, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( widgets->show_speed[pwgt] ) {
    if ( !widgets->speeds_evaluated )
      evaluate_speeds ( widgets );
    draw_gps_speed_by_time ( widgets, window, pix );
  }
}

/**
 * Draw an image
 */
static void draw_it ( GtkWidget *image, VikTrack *trk, PropWidgets *widgets, GtkWidget *window, VikPropWinGraphType_t pwgt )
{
  guint i;

  if ( widgets->values[pwgt] )
    g_free ( widgets->values[pwgt] );

  if ( widgets->make_map[pwgt] )
    widgets->values[pwgt] = widgets->make_map[pwgt] ( trk, widgets->profile_width );
  else {
    VikTrackValueType vtvt;
    switch ( pwgt ) {
    case PGT_ELEVATION_TIME: vtvt = TRACK_VALUE_ELEVATION; break;
    case PGT_HEART_RATE:     vtvt = TRACK_VALUE_HEART_RATE; break;
    case PGT_CADENCE:        vtvt = TRACK_VALUE_CADENCE; break;
    case PGT_TEMP:           vtvt = TRACK_VALUE_TEMP; break;
    case PGT_POWER:          vtvt = TRACK_VALUE_POWER; break;
    default: return; break;
    }
    widgets->values[pwgt] = vik_track_make_time_map_for ( trk, widgets->profile_width, vtvt );
  }

  if ( widgets->values[pwgt] == NULL )
    return;

  if ( is_time_graph(pwgt) ) {
    widgets->duration = vik_track_get_duration ( trk, TRUE );
    // Negative time or other problem
    if ( widgets->duration <= 0 )
      return;
  }

  // Convert into appropriate units
  if ( widgets->convert_values[pwgt] )
    widgets->convert_values[pwgt] ( widgets->values[pwgt], widgets->profile_width );

  minmax_array ( widgets->values[pwgt], &widgets->min_value[pwgt], &widgets->max_value[pwgt],
                 (pwgt == PGT_ELEVATION_DISTANCE), widgets->profile_width );

  if ( pwgt == PGT_SPEED_TIME || pwgt == PGT_SPEED_DISTANCE )
    if ( widgets->min_value[pwgt] < 0.0 )
      widgets->min_value[pwgt] = 0; // splines sometimes give negative speeds!

  // Find suitable chunk index
  get_new_min_and_chunk_index ( widgets->min_value[pwgt], widgets->max_value[pwgt], chunks, G_N_ELEMENTS(chunks), &widgets->draw_min[pwgt], &widgets->ci[pwgt] );

  // Assign locally
  gdouble min = widgets->draw_min[pwgt];
  guint ci = widgets->ci[pwgt];

  // User override?
  if ( pwgt == PGT_ELEVATION_DISTANCE && widgets->user_set_axis ) {
    ci = widgets->user_cia;
    min = widgets->user_mina;
  }

  GdkPixmap *pix = gdk_pixmap_new ( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  gtk_image_set_from_pixmap ( GTK_IMAGE(image), pix, NULL );
  // Reset before redrawing
  clear_images ( pix, window, widgets );

  const gdouble chunk = chunks[ci];

  // draw grid
  for ( i=0; i<=LINES; i++ ) {
    gchar ss[32];
    widgets->get_y_text[pwgt] ( ss, sizeof(ss), min + (LINES-i)*chunk );
    draw_grid_y ( window, image, widgets, pix, ss, i );
  }

  if ( is_time_graph(pwgt) )
    draw_time_lines ( window, image, pix, widgets );
  else    
    draw_distance_divisions ( window, image, pix, widgets, a_vik_get_units_distance() );

  const guint height = MARGIN_Y+widgets->profile_height;

  GdkGC *no_info_gc = gdk_gc_new ( gtk_widget_get_window(window) );
  GdkColor color;
  gdk_color_parse ( "yellow", &color );
  gdk_gc_set_rgb_fg_color ( no_info_gc, &color );

  GdkGC *gc = gtk_widget_get_style(window)->dark_gc[3];
  const gdouble chunk_lines = chunk * LINES;

  for ( i = 0; i < widgets->profile_width; i++ ) {
    if ( isnan(widgets->values[pwgt][i]) ) {
      gdk_draw_line ( GDK_DRAWABLE(pix), no_info_gc, i + MARGIN_X, MARGIN_Y, i + MARGIN_X, height );
    }
    else
      gdk_draw_line ( GDK_DRAWABLE(pix), gc,
                      i + MARGIN_X, height, i + MARGIN_X, height-widgets->profile_height*(widgets->values[pwgt][i]-min)/chunk_lines );
  }

  if ( widgets->draw_extra[pwgt] )
    widgets->draw_extra[pwgt] ( widgets, window, pix, pwgt );

  // Draw border
  gdk_draw_rectangle ( GDK_DRAWABLE(pix), gtk_widget_get_style(window)->black_gc, FALSE, MARGIN_X, MARGIN_Y, widgets->profile_width-1, widgets->profile_height-1 );
 
  g_object_unref ( G_OBJECT(no_info_gc) );
  g_object_unref ( G_OBJECT(pix) );

  if ( pwgt == PGT_SPEED_TIME )
    widgets->speeds_evaluated = TRUE;
}

static void draw_dt_extra ( gpointer ptr, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( widgets->show_speed[pwgt] ) {
    if ( !widgets->speeds_evaluated )
      evaluate_speeds ( widgets );
    draw_speed_indicator ( widgets, window, pix );
  }
}

// ATM Only works for PGT_ELEVATION_TIME
static void draw_dem_by_time ( PropWidgets *widgets, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt )
{
  GdkColor color;
  GdkGC *dem_alt_gc = gdk_gc_new ( gtk_widget_get_window(window) );
  gdk_color_parse ( "green", &color );
  gdk_gc_set_rgb_fg_color ( dem_alt_gc, &color);

  const gint h2 = widgets->profile_height + MARGIN_Y; // Adjust height for x axis labelling offset
  const gdouble chunk = chunks[widgets->ci[pwgt]];
  const guint achunk = chunk*LINES;
  const gdouble mina = widgets->draw_min[pwgt];
  if ( achunk == 0 )
    return;

  for ( guint i = 0; i < widgets->profile_width; i++ ) {
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

static void draw_extra_dem_and_speed ( gpointer ptr, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( widgets->show_dem[pwgt] )
    draw_dem_by_time ( widgets, window, pix, PGT_ELEVATION_TIME );

  if ( widgets->show_speed[pwgt] ) {
    if ( !widgets->speeds_evaluated )
      evaluate_speeds ( widgets );
    draw_speed_indicator ( widgets, window, pix );
  }
}

static void draw_extra_dem_and_gps_speed ( gpointer ptr, GtkWidget *window, GdkPixmap *pix, VikPropWinGraphType_t pwgt )
{
  PropWidgets *widgets = (PropWidgets*)ptr;
  if ( widgets->show_dem[pwgt] )
    draw_dem_by_time ( widgets, window, pix, PGT_ELEVATION_TIME );

  if ( widgets->show_speed[pwgt] ) {
    if ( !widgets->speeds_evaluated )
      evaluate_speeds ( widgets );
    draw_gps_speed_by_time ( widgets, window, pix );
  }
}

#undef LINES

/**
 *
 */
static void clear_saved_img ( gboolean resized, PropWidgets *widgets, VikPropWinGraphType_t pwgt )
{
  if ( resized && widgets->graph_saved_img[pwgt].img ) {
    g_object_unref ( widgets->graph_saved_img[pwgt].img );
    widgets->graph_saved_img[pwgt].img = NULL;
    widgets->graph_saved_img[pwgt].saved = FALSE;
  }
}

/**
 * Draw all graphs
 */
static void draw_all_graphs ( GtkWidget *widget, PropWidgets *widgets, gboolean resized )
{
  // Draw graphs even if they are not visible
  GtkWidget *window = gtk_widget_get_toplevel(widget);
  gdouble pc = NAN;
  gdouble pc_blob = NAN;

  VikPropWinGraphType_t pwgt;
  for ( pwgt = 0; pwgt < PGT_END; pwgt++ ) {
    if ( widgets->event_box[pwgt] ) {
      // Saved image no longer any good as we've resized, so we remove it here
      clear_saved_img ( resized, widgets, pwgt );

      // Main drawing
      draw_it ( widgets->image[pwgt], widgets->tr, widgets, window, pwgt );

      // Ensure marker or blob are redrawn if necessary
      if ( widgets->is_marker_drawn || widgets->is_blob_drawn ) {

        if ( is_time_graph(pwgt) )
          pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );
        else
          pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );

        gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
        guint   y_blob = 0;
        if ( widgets->is_blob_drawn ) {
          if ( is_time_graph(pwgt) )
            pc_blob = tp_percentage_by_time ( widgets->tr, widgets->blob_tp );
          else
            pc_blob = tp_percentage_by_distance ( widgets->tr, widgets->blob_tp, widgets->track_length_inc_gaps );

          if ( !isnan(pc_blob) ) {
            x_blob = (pc_blob * widgets->profile_width);
          }
          y_blob = blob_y_position ( x_blob > 0 ? (guint)x_blob : 0, widgets, pwgt );
        }

        gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
        if ( !isnan(pc) ) {
          marker_x = (pc * widgets->profile_width) + MARGIN_X;
        }

        save_image_and_draw_graph_marks ( widgets->image[pwgt],
                                          marker_x,
                                          gtk_widget_get_style(window)->black_gc,
                                          x_blob+MARGIN_X,
                                          y_blob+MARGIN_Y,
                                          &widgets->graph_saved_img[pwgt],
                                          widgets->profile_width,
                                          widgets->profile_height,
                                          BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
                                          &widgets->is_marker_drawn,
                                          &widgets->is_blob_drawn );
      }
    }
  }
  widgets->speeds_evaluated = FALSE;
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
  for ( VikPropWinGraphType_t pwgt = 0; pwgt < PGT_END; pwgt++ ) {
    if ( widgets->w_show_dem[pwgt] )
      widgets->show_dem[pwgt] = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dem[pwgt]) );
    if ( widgets->w_show_speed[pwgt] )
      widgets->show_speed[pwgt] = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_speed[pwgt]) );
  }

  // Draw stuff
  draw_all_graphs ( widget, widgets, TRUE );

  return FALSE;
}

/**
 * Commonal creation of widgets including the image and callbacks
 */
GtkWidget *create_event_box ( GtkWidget *window, PropWidgets *widgets, VikPropWinGraphType_t pwgt )
{
  GdkPixmap *pix = gdk_pixmap_new ( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
  widgets->image[pwgt] = gtk_image_new_from_pixmap ( pix, NULL );
  g_object_unref ( G_OBJECT(pix) );

  GtkWidget *eventbox = gtk_event_box_new ();
  g_signal_connect ( G_OBJECT(eventbox), "button_press_event", G_CALLBACK(track_graph_click_cb), widgets );
  g_signal_connect ( G_OBJECT(eventbox), "motion_notify_event", G_CALLBACK(track_graph_move), widgets );
  gtk_container_add ( GTK_CONTAINER(eventbox), widgets->image[pwgt] );
  gtk_widget_set_events ( eventbox, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_STRUCTURE_MASK );
  return eventbox;
}

/**
 * Create height profile widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_profile ( GtkWidget *window, PropWidgets *widgets )
{
  // First allocation & monitor how quick (or not it is)
  const VikPropWinGraphType_t pwgt = PGT_ELEVATION_DISTANCE;
  clock_t begin = clock();
  widgets->values[pwgt] = vik_track_make_elevation_map ( widgets->tr, widgets->profile_width );
  clock_t end = clock();
  widgets->alt_create_time = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f", __FUNCTION__, widgets->alt_create_time );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->make_map[pwgt] = vik_track_make_elevation_map;
  widgets->convert_values[pwgt] = elev_convert;
  widgets->get_y_text[pwgt] = elev_y_text;
  widgets->draw_extra[pwgt] = draw_ed_extra;
  widgets->button_update[pwgt] = update_elevation_distance_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create height profile widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_gradient ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_GRADIENT_DISTANCE;
  widgets->values[pwgt] = vik_track_make_gradient_map ( widgets->tr, widgets->profile_width );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->make_map[pwgt] = vik_track_make_gradient_map;
  widgets->convert_values[pwgt] = NULL;
  widgets->get_y_text[pwgt] = pct_y_text;
  widgets->draw_extra[pwgt] = draw_gps_speed_extra;
  widgets->button_update[pwgt] = update_gradient_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create speed/time widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_vtdiag ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_SPEED_TIME;
  widgets->values[pwgt] = vik_track_make_speed_map ( widgets->tr, widgets->profile_width );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->make_map[pwgt] = vik_track_make_speed_map;
  widgets->convert_values[pwgt] = speed_convert;
  widgets->get_y_text[pwgt] = speed_y_text;
  widgets->draw_extra[pwgt] = draw_vt_gps_speed_extra;
  widgets->button_update[pwgt] = update_speed_time_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create distance / time widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_dtdiag ( GtkWidget *window, PropWidgets *widgets)
{
  // First allocation
  const VikPropWinGraphType_t pwgt = PGT_DISTANCE_TIME;
  widgets->values[pwgt] = vik_track_make_distance_map ( widgets->tr, widgets->profile_width );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->make_map[pwgt] = vik_track_make_distance_map;
  widgets->convert_values[pwgt] = dist_convert;
  widgets->get_y_text[pwgt] = dist_y_text;
  widgets->draw_extra[pwgt] = draw_dt_extra;
  widgets->button_update[pwgt] = update_distance_time_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create elevation / time widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_etdiag ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_ELEVATION_TIME;
  widgets->values[pwgt] = vik_track_make_time_map_for ( widgets->tr, widgets->profile_width, TRACK_VALUE_ELEVATION );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->convert_values[pwgt] = elev_convert;
  widgets->get_y_text[pwgt] = elev_y_text;
  widgets->draw_extra[pwgt] = draw_extra_dem_and_speed;
  widgets->button_update[pwgt] = update_elevation_time_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create speed/distance widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_sddiag ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_SPEED_DISTANCE;
  widgets->values[pwgt] = vik_track_make_speed_dist_map ( widgets->tr, widgets->profile_width );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->make_map[pwgt] = vik_track_make_speed_dist_map;
  widgets->convert_values[pwgt] = speed_convert;
  widgets->get_y_text[pwgt] = speed_y_text;
  widgets->draw_extra[pwgt] = draw_gps_speed_extra;
  widgets->button_update[pwgt] = update_speed_distance_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create heart rate widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_hrdiag ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_HEART_RATE;
  widgets->values[pwgt] = vik_track_make_time_map_for ( widgets->tr, widgets->profile_width, TRACK_VALUE_HEART_RATE );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->convert_values[pwgt] = NULL;
  widgets->get_y_text[pwgt] = hr_y_text;
  widgets->draw_extra[pwgt] = draw_extra_dem_and_gps_speed;
  widgets->button_update[pwgt] = update_hr_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create cadence widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_caddiag ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_CADENCE;
  widgets->values[pwgt] = vik_track_make_time_map_for ( widgets->tr, widgets->profile_width, TRACK_VALUE_CADENCE );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->convert_values[pwgt] = NULL;
  widgets->get_y_text[pwgt] = cad_y_text;
  widgets->draw_extra[pwgt] = draw_extra_dem_and_gps_speed;
  widgets->button_update[pwgt] = update_cad_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create temperature widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_tempdiag ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_TEMP;
  widgets->values[pwgt] = vik_track_make_time_map_for ( widgets->tr, widgets->profile_width, TRACK_VALUE_TEMP );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->convert_values[pwgt] = temp_convert;
  widgets->get_y_text[pwgt] = temp_y_text;
  widgets->draw_extra[pwgt] = draw_extra_dem_and_gps_speed;
  widgets->button_update[pwgt] = update_temp_buttons;
  return create_event_box ( window, widgets, pwgt );
}

/**
 * Create power widgets including the image and callbacks
 */
GtkWidget *vik_trw_layer_create_powdiag ( GtkWidget *window, PropWidgets *widgets)
{
  const VikPropWinGraphType_t pwgt = PGT_POWER;
  widgets->values[pwgt] = vik_track_make_time_map_for ( widgets->tr, widgets->profile_width, TRACK_VALUE_POWER );
  if ( widgets->values[pwgt] == NULL )
    return NULL;
  widgets->convert_values[pwgt] = NULL;
  widgets->get_y_text[pwgt] = power_y_text;
  widgets->draw_extra[pwgt] = draw_extra_dem_and_gps_speed;
  widgets->button_update[pwgt] = update_power_buttons;
  return create_event_box ( window, widgets, pwgt );
}

#define VIK_SETTINGS_TRACK_PROFILE_WIDTH "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"

static void save_values ( PropWidgets *widgets )
{
  // Session settings
  a_settings_set_integer ( VIK_SETTINGS_TRACK_PROFILE_WIDTH, widgets->profile_width );
  a_settings_set_integer ( VIK_SETTINGS_TRACK_PROFILE_HEIGHT, widgets->profile_height );

  // Just for this session ATM
  for ( VikPropWinGraphType_t pwgt = 0; pwgt < PGT_END; pwgt++ ) {
    if ( widgets->w_show_dem[pwgt] )
      show_dem[pwgt] = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dem[pwgt]) );
    if ( widgets->w_show_speed[pwgt] )
      show_speed[pwgt] = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_speed[pwgt]) );
  }
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
        tr_right->draw_name_mode = tr->draw_name_mode;
        tr_right->max_number_dist_labels = tr->max_number_dist_labels;
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
  guint old_number = tr->number;

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
      tr->number = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(widgets->w_number) );
      trw_layer_update_treeview ( widgets->vtl, widgets->tr, (old_number != tr->number) );
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
  for ( VikPropWinGraphType_t pwgt = 0; pwgt < PGT_END; pwgt++ ) {
    if ( widgets->w_show_dem[pwgt] )
      widgets->show_dem[pwgt] = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dem[pwgt]) );
    if ( widgets->w_show_speed[pwgt] )
      widgets->show_speed[pwgt] = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(widgets->w_show_speed[pwgt]) );
  }
  draw_all_graphs ( widgets->dialog, widgets, TRUE );
}

/**
 *  Create the widgets for the given graph tab
 */
static GtkWidget *create_graph_page ( PropWidgets *widgets,
                                      VikPropWinGraphType_t pwgt,
				      const gchar *markup,
				      const gchar *markup2,
				      const gchar *markup3,
				      gboolean checkbutton1,
                                      gboolean DEM_available,
                                      const gchar *show_speed_mnemonic )
{
  GtkWidget *graph = widgets->event_box[pwgt];
  // Every graph has two value labels
  widgets->w_cur_value1[pwgt] = ui_label_new_selectable ( _("No Data") );
  widgets->w_cur_value2[pwgt] = ui_label_new_selectable ( _("No Data") );
  gtk_widget_set_can_focus ( widgets->w_cur_value1[pwgt], FALSE ); // Don't let notebook autofocus on it
  gtk_widget_set_can_focus ( widgets->w_cur_value2[pwgt], FALSE ); // Don't let notebook autofocus on it
  GtkWidget *hbox = gtk_hbox_new ( FALSE, 10 );
  GtkWidget *vbox = gtk_vbox_new ( FALSE, 10 );
  GtkWidget *label = gtk_label_new (NULL);
  GtkWidget *label2 = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(vbox), graph, FALSE, FALSE, 0);
  gtk_label_set_markup ( GTK_LABEL(label), markup );
  gtk_label_set_markup ( GTK_LABEL(label2), markup2 );
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), widgets->w_cur_value1[pwgt], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), label2, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), widgets->w_cur_value2[pwgt], FALSE, FALSE, 0);
  if ( markup3 ) {
    GtkWidget *label3 = gtk_label_new (NULL);
    gtk_label_set_markup ( GTK_LABEL(label3), markup3 );
    widgets->w_cur_value3[pwgt] = ui_label_new_selectable(_("No Data"));
    gtk_widget_set_can_focus ( widgets->w_cur_value3[pwgt], FALSE ); // Don't let notebook autofocus on it
    gtk_box_pack_start (GTK_BOX(hbox), label3, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(hbox), widgets->w_cur_value3[pwgt], FALSE, FALSE, 0);
  }
  if ( show_speed_mnemonic ) {
    widgets->w_show_speed[pwgt] = gtk_check_button_new_with_mnemonic ( show_speed_mnemonic );
    gtk_box_pack_end (GTK_BOX(hbox), widgets->w_show_speed[pwgt], FALSE, FALSE, 0);
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(widgets->w_show_speed[pwgt]), show_speed[pwgt] );
  }
  if (checkbutton1) {
    widgets->w_show_dem[pwgt] = gtk_check_button_new_with_mnemonic ( _("Show D_EM") );
    gtk_widget_set_sensitive ( widgets->w_show_dem[pwgt], DEM_available );
    gtk_box_pack_end (GTK_BOX(hbox), widgets->w_show_dem[pwgt], FALSE, FALSE, 0);
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(widgets->w_show_dem[pwgt]), show_dem[pwgt] );
  }
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  gtk_widget_set_can_focus ( GTK_WIDGET(vbox), FALSE ); // Don't let notebook autofocus on it
  return vbox;
}

static void attach_to_table ( GtkTable *table, int i, char *mylabel, GtkWidget *content )
{
  // Settings so the text positioning only moves around vertically when the dialog is resized
  // This also gives more room to see the track comment
  GtkWidget *label = gtk_label_new(NULL);
  gtk_misc_set_alignment ( GTK_MISC(label), 1, 0.5 ); // Position text centrally in vertical plane
  gtk_label_set_markup ( GTK_LABEL(label), _(mylabel) );
  gtk_table_attach ( table, label, 0, 1, i, i+1, GTK_FILL, GTK_SHRINK, 0, 0 );
  if ( GTK_IS_MISC(content) ) {
    gtk_misc_set_alignment ( GTK_MISC(content), 0, 0.5 );
  }
  if ( GTK_IS_COLOR_BUTTON(content) || GTK_IS_COMBO_BOX(content) )
    // Buttons compressed - otherwise look weird (to me) if vertically massive
    gtk_table_attach ( table, content, 1, 2, i, i+1, GTK_FILL, GTK_SHRINK, 0, 5 );
  else {
     // Expand for comments + descriptions / labels
     gtk_table_attach_defaults ( table, content, 1, 2, i, i+1 );
     if ( GTK_IS_LABEL(content) )
       gtk_widget_set_can_focus ( content, FALSE ); // Prevent notebook auto selecting it
  }
}

static GtkWidget *create_table (int cnt, char *labels[], GtkWidget *contents[])
{
  GtkTable *table = GTK_TABLE(gtk_table_new (cnt, 2, FALSE));
  gtk_table_set_col_spacing (table, 0, 10);
  for (guint i=0; i<cnt; i++)
    attach_to_table ( table, i, labels[i], contents[i] );

  return GTK_WIDGET (table);
}

static void attach_to_table_extra ( GtkWidget *table, gchar *text, int ii, char *mylabel )
{
  GtkWidget *wgt = ui_label_new_selectable ( text );
  attach_to_table ( GTK_TABLE(table), ii, mylabel, wgt );
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
  gdouble split_length = vu_distance_deconvert ( dist_units, split_factor );

  vik_units_height_t height_units = a_vik_get_units_height ();
  vik_units_speed_t speed_units = a_vik_get_units_speed ();

  GtkWidget *view = gtk_tree_view_new();
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *column;
  gint column_runner = 0;

  GString *str0 = g_string_new ( _("Distance") );
  gchar *dist_str = vu_distance_units_text ( dist_units );
  g_string_append_printf ( str0, "\n(%s)", dist_str );
  g_free ( dist_str );

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

    gdouble length = vu_distance_convert ( dist_units, vtss.length );
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

  // Don't use minmax_array(widgets->values[PGT_ELEVATION_DISTANCE]), as that is a simplified representative of the points
  //  thus can miss the highest & lowest values by a few metres
  gdouble min_alt, max_alt;
  if ( !vik_track_get_minmax_alt (widgets->tr, &min_alt, &max_alt) )
    min_alt = max_alt = NAN;

  vik_units_distance_t dist_units = a_vik_get_units_distance ();

  // NB This value not shown yet - but is used by internal calculations
  widgets->track_length_inc_gaps = vik_track_get_length_including_gaps(tr);

  tr_len = widgets->track_length = vik_track_get_length(tr);
  vu_distance_text ( tmp_buf, sizeof(tmp_buf), dist_units, tr_len, TRUE, "%.2f", FALSE );
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
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, tmp_speed, TRUE, "%.2f", FALSE );
  }
  widgets->w_max_speed = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  tmp_speed = vik_track_get_average_speed(tr);
  if ( tmp_speed == 0 )
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
  else {
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, tmp_speed, TRUE, "%.2f", FALSE );
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
    vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, tmp_speed, TRUE, "%.2f", FALSE );
  }
  widgets->w_mvg_speed = content[cnt++] = ui_label_new_selectable ( tmp_buf );

  // The average distance between points is going to be quite small use the smaller units
  gdouble adbp = (tp_count - seg_count) == 0 ? 0 : tr_len / ( tp_count - seg_count );
  vu_distance_text_precision ( tmp_buf, sizeof(tmp_buf), dist_units, adbp, "%.2f" );
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

    // Notional center of a track is simply an average of the bounding box extremities
    struct LatLon center = { (tr->bbox.north+tr->bbox.south)/2, (tr->bbox.east+tr->bbox.west)/2 };
    vik_coord_load_from_latlon ( &widgets->vc, vik_trw_layer_get_coord_mode(widgets->vtl), &center );

    widgets->tz = vu_get_tz_at_location ( &widgets->vc );

    time_t ts1 = round ( t1 );
    time_t ts2 = round ( t2 );
    gchar *msg;
    msg = vu_get_time_string ( &ts1, "%c", &widgets->vc, widgets->tz );
    widgets->w_time_start = content[cnt++] = ui_label_new_selectable(msg);
    g_free ( msg );

    msg = vu_get_time_string ( &ts2, "%c", &widgets->vc, widgets->tz );
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

  // ATM just appending extra information at the bottom
  // TODO only if it's there (i.e. no 'No Data') to keep the dialog from getting too big
  //  and less need for scrollling...
  // However if made optional need way to align value to text label...
  table = create_table (cnt, stats_texts, content);

  guint max_cad = vik_track_get_max_cadence ( tr );
  if ( max_cad != VIK_TRKPT_CADENCE_NONE ) {
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d RPM"), max_cad);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Max Cadence:</b>") );
  }

  gdouble avg_cad = vik_track_get_avg_cadence ( tr );
  if ( !isnan(avg_cad) ) {
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f RPM"), avg_cad);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Avg. Cadence:</b>") );
  }

  guint max_hr = vik_track_get_max_heart_rate ( tr );
  if ( max_hr ) {
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d bpm"), max_hr);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Max Heart Rate:</b>") );
  }

  gdouble avg_hr = vik_track_get_avg_heart_rate ( tr );
  if ( !isnan(avg_hr) ) {
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f bpm"), avg_hr);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Avg. Heart Rate:</b>") );
  }

  gdouble min_temp, max_temp;
  if ( vik_track_get_minmax_temp(tr, &min_temp, &max_temp) ) {
    if ( a_vik_get_units_temp() == VIK_UNITS_TEMP_CELSIUS )
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f%sC / %.1f%sC"), min_temp, DEGREE_SYMBOL, max_temp, DEGREE_SYMBOL);
    else
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f%sF / %.1f%sF"), VIK_CELSIUS_TO_FAHRENHEIT(min_temp), DEGREE_SYMBOL, VIK_CELSIUS_TO_FAHRENHEIT(max_temp), DEGREE_SYMBOL);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Min/Max Temperature:</b>") );
  }

  gdouble avg_temp = vik_track_get_avg_temp ( tr );
  if ( !isnan(avg_temp) ) {
    if ( a_vik_get_units_temp() == VIK_UNITS_TEMP_CELSIUS )
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f%sC"), avg_temp, DEGREE_SYMBOL);
    else
      g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f%sF"), VIK_CELSIUS_TO_FAHRENHEIT(avg_temp), DEGREE_SYMBOL);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Avg. Temperature:</b>") );
  }

  guint max_pow = vik_track_get_max_power ( tr );
  if ( max_pow != VIK_TRKPT_POWER_NONE ) {
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%d Watts"), max_pow);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Max Power:</b>") );
  }

  gdouble avg_pow = vik_track_get_avg_power ( tr );
  if ( !isnan(avg_pow) ) {
    g_snprintf(tmp_buf, sizeof(tmp_buf), _("%.1f Watts"), avg_pow);
    attach_to_table_extra ( table, tmp_buf, ++cnt, _("<b>Avg. Power:</b>") );
  }

  GtkWidget *sw = gtk_scrolled_window_new ( NULL, NULL );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
  gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW(sw), table );
  return sw;
  //return table;
}

gboolean bool_pref_get ( const gchar *pref )
{
  VikLayerParamData *vlpd = a_preferences_get ( pref );
  return vlpd->b;
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

  gboolean DEM_available = a_dems_overlaps_bbox (tr->bbox);

  if ( bool_pref_get(TPW_PREFS_NS"show_elev_dist") )
    widgets->event_box[PGT_ELEVATION_DISTANCE] = vik_trw_layer_create_profile(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_grad_dist") )
    widgets->event_box[PGT_GRADIENT_DISTANCE] = vik_trw_layer_create_gradient(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_speed_time") )
    widgets->event_box[PGT_SPEED_TIME] = vik_trw_layer_create_vtdiag(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_dist_time") )
    widgets->event_box[PGT_DISTANCE_TIME] = vik_trw_layer_create_dtdiag(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_elev_time") )
    widgets->event_box[PGT_ELEVATION_TIME] = vik_trw_layer_create_etdiag(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_speed_dist") )
    widgets->event_box[PGT_SPEED_DISTANCE] = vik_trw_layer_create_sddiag(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_heart_rate") )
    widgets->event_box[PGT_HEART_RATE] = vik_trw_layer_create_hrdiag(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_cadence") )
    widgets->event_box[PGT_CADENCE] = vik_trw_layer_create_caddiag(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_temp") )
    widgets->event_box[PGT_TEMP] = vik_trw_layer_create_tempdiag(GTK_WIDGET(parent), widgets);
  if ( bool_pref_get(TPW_PREFS_NS"show_power") )
    widgets->event_box[PGT_POWER] = vik_trw_layer_create_powdiag(GTK_WIDGET(parent), widgets);
  GtkWidget *graphs = gtk_notebook_new();

  if ( bool_pref_get(TPW_PREFS_NS"tabs_on_side") )
    gtk_notebook_set_tab_pos ( GTK_NOTEBOOK(graphs), GTK_POS_LEFT );

  GtkWidget *content_prop[5];
  int cnt_prop = 0;

  static gchar *label_texts[] = {
    N_("<b>Comment:</b>"),
    N_("<b>Description:</b>"),
    N_("<b>Source:</b>"),
    N_("<b>Type:</b>"),
    N_("<b>Number:</b>"),
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

  widgets->w_number = ui_spin_button_new ( (GtkAdjustment*)gtk_adjustment_new(tr->number,0,9999,1,10,0), 10.0, 0 );
  content_prop[cnt_prop++] = widgets->w_number;

  GtkWidget *content_draw[3];
  guint cnt_draw = 0;

  static gchar *draw_texts[] = {
    N_("<b>Color:</b>"),
    N_("<b>Draw Name:</b>"),
    N_("<b>Distance Labels:</b>"),
  };

  widgets->w_color = content_draw[cnt_draw++] = gtk_color_button_new_with_color ( &(tr->color) );

  static gchar *draw_name_labels[] = {
    N_("No"),
    N_("Centre"),
    N_("Start only"),
    N_("End only"),
    N_("Start and End"),
    N_("Centre, Start and End"),
    NULL
  };

  widgets->w_namelabel = content_draw[cnt_draw++] = vik_combo_box_text_new ();
  gchar **pstr = draw_name_labels;
  while ( *pstr )
    vik_combo_box_text_append ( widgets->w_namelabel, *(pstr++) );
  gtk_combo_box_set_active ( GTK_COMBO_BOX(widgets->w_namelabel), tr->draw_name_mode );

  widgets->w_number_distlabels = content_draw[cnt_draw++] =
   gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new(tr->max_number_dist_labels, 0, 100, 1, 1, 0)), 1, 0 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(widgets->w_number_distlabels), _("Maximum number of distance labels to be shown") );

  GtkWidget *table = create_table ( cnt_prop, label_texts, content_prop );
  GtkWidget *table_draw = create_table ( cnt_draw, draw_texts, content_draw );
  GtkWidget *props = gtk_notebook_new();
  gtk_notebook_append_page ( GTK_NOTEBOOK(props), table, gtk_label_new(_("General")));
  gtk_notebook_append_page ( GTK_NOTEBOOK(props), table_draw, gtk_label_new(_("Drawing")));

  if ( widgets->tr->extensions ) {
    GtkWidget *sw = gtk_scrolled_window_new ( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    GtkWidget *ext = ui_label_new_selectable ( widgets->tr->extensions );
    gtk_widget_set_can_focus ( GTK_WIDGET(ext), FALSE ); // Don't let notebook autofocus on it
    gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW(sw), ext );
    gtk_notebook_append_page ( GTK_NOTEBOOK(props), sw, gtk_label_new(_("GPX Extensions")) );
  }

  gtk_notebook_append_page ( GTK_NOTEBOOK(graphs), GTK_WIDGET(props), gtk_label_new(_("Properties")) );
  gtk_notebook_append_page(GTK_NOTEBOOK(graphs), create_statistics_page(widgets, widgets->tr), gtk_label_new(_("Statistics")));

  // TODO: One day might be nice to have bar chart equivalent of the simple table values.
  // Only bother showing timing splits if track has some kind of timespan
  if ( bool_pref_get(TPW_PREFS_NS"show_splits") )
    if ( vik_track_get_duration(tr,FALSE) > 1 )
      gtk_notebook_append_page(GTK_NOTEBOOK(graphs), create_splits_tables(tr), gtk_label_new(_("Splits")));

  if ( widgets->event_box[PGT_ELEVATION_DISTANCE] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_ELEVATION_DISTANCE,
                                          _("<b>Track Distance:</b>"),
                                          _("<b>Track Height:</b>"),
                                          NULL,
                                          TRUE, DEM_available,
                                          _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Elevation-distance")));
  }

  if ( widgets->event_box[PGT_GRADIENT_DISTANCE] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_GRADIENT_DISTANCE,
                                          _("<b>Track Distance:</b>"),
                                          _("<b>Track Gradient:</b>"),
                                          NULL,
                                          FALSE, FALSE,
                                          _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Gradient-distance")));
  }

  if ( widgets->event_box[PGT_SPEED_TIME] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_SPEED_TIME,
                                          _("<b>Track Time:</b>"),
                                          _("<b>Track Speed:</b>"),
                                          _("<b>Time/Date:</b>"),
                                          FALSE, FALSE,
                                          _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Speed-time")));
  }

  if ( widgets->event_box[PGT_DISTANCE_TIME] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_DISTANCE_TIME,
                                          _("<b>Track Distance:</b>"),
                                          _("<b>Track Time:</b>"),
                                          _("<b>Time/Date:</b>"),
                                         FALSE, FALSE,
                                         _("Show S_peed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Distance-time")));
  }

  if ( widgets->event_box[PGT_ELEVATION_TIME] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_ELEVATION_TIME,
                                          _("<b>Track Time:</b>"),
                                          _("<b>Track Height:</b>"),
                                          _("<b>Time/Date:</b>"),
                                          TRUE, DEM_available,
                                          _("Show S_peed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Elevation-time")));
  }

  if ( widgets->event_box[PGT_SPEED_DISTANCE] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_SPEED_DISTANCE,
                                          _("<b>Track Distance:</b>"),
                                          _("<b>Track Speed:</b>"),
                                          NULL,
                                          FALSE, FALSE,
                                         _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Speed-distance")));
  }

  if ( widgets->event_box[PGT_HEART_RATE] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_HEART_RATE,
                                          _("<b>Heart Rate:</b>"),
                                          _("<b>Track Time:</b>"),
                                          NULL,
                                          TRUE, DEM_available,
                                          _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Heart Rate")));
  }

  if ( widgets->event_box[PGT_CADENCE] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_CADENCE,
                                          _("<b>Cadence:</b>"),
                                          _("<b>Track Time:</b>"),
                                          NULL,
                                          TRUE, DEM_available,
                                          _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Cadence")));
  }

  if ( widgets->event_box[PGT_TEMP] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_TEMP,
                                          _("<b>Temperature:</b>"),
                                          _("<b>Track Time:</b>"),
                                          NULL,
                                          TRUE, DEM_available,
                                          _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Temperature")));
  }

  if ( widgets->event_box[PGT_POWER] ) {
    GtkWidget *page = create_graph_page ( widgets, PGT_POWER,
                                          _("<b>Power:</b>"),
                                          _("<b>Track Time:</b>"),
                                          NULL,
                                          TRUE, DEM_available,
                                          _("Show _GPS Speed") );
    gtk_notebook_append_page(GTK_NOTEBOOK(graphs), page, gtk_label_new(_("Power")));
  }

  // All checkboxes goto the same callback
  for ( VikPropWinGraphType_t pwgt = 0; pwgt < PGT_END; pwgt++ ) {
    if ( widgets->w_show_dem[pwgt] )
      g_signal_connect ( widgets->w_show_dem[pwgt], "toggled", G_CALLBACK (checkbutton_toggle_cb), widgets );
    if ( widgets->w_show_speed[pwgt] )
      g_signal_connect ( widgets->w_show_speed[pwgt], "toggled", G_CALLBACK(checkbutton_toggle_cb), widgets );
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
 * Draw blob
 */
void vik_trw_layer_propwin_main_draw_blob ( gpointer self, VikTrackpoint *trkpt )
{
  PropWidgets *widgets = (PropWidgets*)self;
  gdouble pc = NAN;
  gdouble pc_blob = NAN;

  if ( !widgets->graphs )
    return;

  widgets->blob_tp = trkpt;

  GtkWidget *page = gtk_notebook_get_nth_page ( GTK_NOTEBOOK(widgets->graphs),
                                                gtk_notebook_get_current_page(GTK_NOTEBOOK(widgets->graphs)) );
  if ( !page )
    return;

  GtkWidget *window = gtk_widget_get_toplevel ( page );
  GtkWidget *image;
  PropSaved saved_img;
  gdouble x_blob = -MARGIN_X - 1.0; // i.e. Don't draw unless we get a valid value
  guint   y_blob = 0;

  if ( page == widgets->event_box[PGT_ELEVATION_DISTANCE] ) {
    pc = tp_percentage_by_distance ( widgets->tr, widgets->marker_tp, widgets->track_length_inc_gaps );
    pc_blob = tp_percentage_by_distance ( widgets->tr, widgets->blob_tp, widgets->track_length_inc_gaps );
    image = widgets->image[PGT_ELEVATION_DISTANCE];
    saved_img = widgets->graph_saved_img[PGT_ELEVATION_DISTANCE];
  }

  if ( page == widgets->event_box[PGT_SPEED_TIME] ) {
    pc = tp_percentage_by_time ( widgets->tr, widgets->marker_tp );
    pc_blob = tp_percentage_by_time ( widgets->tr, widgets->blob_tp );
    image = widgets->image[PGT_SPEED_TIME];
    saved_img = widgets->graph_saved_img[PGT_SPEED_TIME];
  }

  if ( !isnan(pc_blob) ) {
    x_blob = pc_blob * widgets->profile_width;

    if ( page == widgets->event_box[PGT_ELEVATION_DISTANCE] )
      y_blob = blob_y_position ( (guint)x_blob, widgets, PGT_ELEVATION_DISTANCE );
    if ( page == widgets->event_box[PGT_SPEED_TIME] )
      y_blob = blob_y_position ( (guint)x_blob, widgets, PGT_SPEED_TIME );
  }

  gdouble marker_x = -1.0; // i.e. Don't draw unless we get a valid value
  if ( !isnan(pc) )
    marker_x = (pc * widgets->profile_width) + MARGIN_X;

  save_image_and_draw_graph_marks ( image,
                                    marker_x,
                                    gtk_widget_get_style(window)->black_gc,
                                    x_blob+MARGIN_X,
                                    y_blob+MARGIN_Y,
                                    &saved_img,
                                    widgets->profile_width,
                                    widgets->profile_height,
                                    BLOB_SIZE * vik_viewport_get_scale(widgets->vvp),
                                    &widgets->is_marker_drawn,
                                    &widgets->is_blob_drawn );
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

  if ( widget == widgets->event_box[PGT_ELEVATION_DISTANCE] ) {
    trackpoint = vik_track_get_closest_tp_by_percentage_dist ( widgets->tr, xx/widgets->profile_width, &meters_from_start );
    if ( trackpoint ) {
      add_tip_text_dist_elev ( gtip, trackpoint, meters_from_start );

      // NB ATM skip working out the speed if not available directly,
      //  otherwise extra overhead to work it out and not that important to show
      if ( !isnan(trackpoint->speed) ) {
        static gchar tmp_buf1[64];
        vik_units_speed_t speed_units = a_vik_get_units_speed ();
        vu_speed_text ( tmp_buf1, sizeof(tmp_buf1), speed_units, trackpoint->speed, FALSE, "%.1f", FALSE );
        g_string_append_printf ( gtip, "%s\n", tmp_buf1 );
      }

      VikTrackpoint *tp1 = vik_track_get_tp_first ( widgets->tr );
      if ( !isnan(trackpoint->timestamp) && tp1 && !isnan(tp1->timestamp) ) {
        seconds_from_start = trackpoint->timestamp - tp1->timestamp;
      }
    }
  }

  if ( widget == widgets->event_box[PGT_SPEED_TIME] ) {
    trackpoint = vik_track_get_closest_tp_by_percentage_time ( widgets->tr, xx/widgets->profile_width, &seconds_from_start );
    if ( trackpoint ) {

      // Since vik_track_get_length_to_trackpoint() might reprocess the whole track (yet again)
      //  it could be potentially make this extra slow and so not best practice to perform in the tooltip every time
      //  otherwise the tooltip might not get shown at all
      if ( widgets->alt_create_time < 0.01 ) {
        meters_from_start = vik_track_get_length_to_trackpoint ( widgets->tr, trackpoint );
        add_tip_text_dist_elev ( gtip, trackpoint, meters_from_start );
      }

      guint ix = (guint)xx;
      // Ensure ix is inbounds
      if (ix == widgets->profile_width)
        ix--;
      static gchar tmp_buf1[64];
      vik_units_speed_t speed_units = a_vik_get_units_speed ();
      vu_speed_text ( tmp_buf1, sizeof(tmp_buf1), speed_units, widgets->values[PGT_SPEED_TIME][ix], FALSE, "%.1f", FALSE );
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
      gchar *msg = vu_get_time_string ( &ts, "%X %x %Z", &trackpoint->coord, widgets->tz );
      g_strlcpy ( time_buf, msg, sizeof(time_buf) );
      g_free ( msg );
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

  // Ensure closed if not visible
  if ( !widgets->tr->visible ) {
    vik_window_close_graphs ( vw );
    return FALSE;
  }

  // Should be on the right track...
  widgets->track_length_inc_gaps = vik_track_get_length_including_gaps ( widgets->tr );

  if ( widgets->values[PGT_ELEVATION_DISTANCE] )
    g_free ( widgets->values[PGT_ELEVATION_DISTANCE] );
  widgets->values[PGT_ELEVATION_DISTANCE] = vik_track_make_elevation_map ( widgets->tr, widgets->profile_width );

  evaluate_speeds ( widgets );

  // If no current values then clear display of any previous stuff
  if ( !widgets->values[PGT_ELEVATION_DISTANCE] && widgets->event_box[PGT_ELEVATION_DISTANCE] ) {
    GtkWidget *window = gtk_widget_get_toplevel ( widgets->event_box[PGT_ELEVATION_DISTANCE] );
    GdkPixmap *pix = gdk_pixmap_new ( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
    gtk_image_set_from_pixmap ( GTK_IMAGE(widgets->image[PGT_ELEVATION_DISTANCE]), pix, NULL );
    clear_images ( pix, window, widgets );

    // Extra protection in case all trackpoints of an existing timed track get deleted
    if ( !widgets->values[PGT_SPEED_TIME] && widgets->event_box[PGT_SPEED_TIME] ) {
      GdkPixmap *pix = gdk_pixmap_new ( gtk_widget_get_window(window), widgets->profile_width+MARGIN_X, widgets->profile_height+MARGIN_Y, -1 );
      gtk_image_set_from_pixmap ( GTK_IMAGE(widgets->image[PGT_SPEED_TIME]), pix, NULL );
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
  if ( widgets->event_box[PGT_ELEVATION_DISTANCE] ) {
    gtk_widget_get_allocation ( widgets->event_box[PGT_ELEVATION_DISTANCE], &allocation );
  }
  else if ( widgets->event_box[PGT_SPEED_TIME] )
    gtk_widget_get_allocation ( widgets->event_box[PGT_SPEED_TIME], &allocation );
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
  widgets->show_dem[PGT_ELEVATION_DISTANCE] = main_show_dem;
  widgets->show_speed[PGT_SPEED_TIME] = main_show_gps_speed;

  if ( tr && tr->trackpoints && a_vik_get_time_ref_frame() == VIK_TIME_REF_WORLD ) {
    widgets->tz = vu_get_tz_at_location ( &VIK_TRACKPOINT(tr->trackpoints->data)->coord );
  }

  GtkWidget *graphs = gtk_notebook_new ( );
  // By storing the graphs here & then deleting on close, means any associated signals also get removed
  //  otherwise signals would keep being called with stale values (or need to manually delete the signals)
  widgets->graphs = graphs;
  gtk_notebook_set_tab_pos ( GTK_NOTEBOOK(graphs), GTK_POS_RIGHT ); // Maybe allow config of Left/Right?

  widgets->event_box[PGT_ELEVATION_DISTANCE] = vik_trw_layer_create_profile ( GTK_WIDGET(parent), widgets );
  widgets->event_box[PGT_SPEED_TIME] = vik_trw_layer_create_vtdiag ( GTK_WIDGET(parent), widgets );

  if ( widgets->event_box[PGT_ELEVATION_DISTANCE] ) {
    g_signal_connect ( G_OBJECT(widgets->event_box[PGT_ELEVATION_DISTANCE]), "leave_notify_event", G_CALLBACK(track_graph_leave), widgets );
    gtk_notebook_append_page ( GTK_NOTEBOOK(graphs), widgets->event_box[PGT_ELEVATION_DISTANCE], gtk_label_new(_("Elevation-distance")) );
    g_object_set ( widgets->event_box[PGT_ELEVATION_DISTANCE], "has-tooltip", TRUE, NULL );
    g_signal_connect ( widgets->event_box[PGT_ELEVATION_DISTANCE], "query-tooltip", G_CALLBACK(graph_tooltip_cb), widgets );
  }

  if ( widgets->event_box[PGT_SPEED_TIME] ) {
    g_signal_connect ( G_OBJECT(widgets->event_box[PGT_SPEED_TIME]), "leave_notify_event", G_CALLBACK(track_graph_leave), widgets );
    gtk_notebook_append_page ( GTK_NOTEBOOK(graphs), widgets->event_box[PGT_SPEED_TIME], gtk_label_new(_("Speed-time")) );
    g_object_set ( widgets->event_box[PGT_SPEED_TIME], "has-tooltip", TRUE, NULL );
    g_signal_connect ( widgets->event_box[PGT_SPEED_TIME], "query-tooltip", G_CALLBACK(graph_tooltip_cb), widgets );
  }

  // If no elevation or time info then don't show anything
  if ( !widgets->event_box[PGT_ELEVATION_DISTANCE] && !widgets->event_box[PGT_SPEED_TIME] ) {
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

  if ( main_last_graph == PGT_SPEED_TIME ) {
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
  main_show_dem = widgets->show_dem[PGT_ELEVATION_DISTANCE];
  main_show_gps_speed = widgets->show_speed[PGT_SPEED_TIME];

  gint page = gtk_notebook_get_current_page ( GTK_NOTEBOOK(widgets->graphs) );
  if ( page == 1 )
    main_last_graph = PGT_SPEED_TIME;
  else
    main_last_graph = PGT_ELEVATION_DISTANCE;

  // The manually destroy widgets
  // NB otherwise not currently destroyed when shown in the main window
  //  since in the dialog version these should get destroyed automatically by the overall dialog being destroyed
  if ( widgets->event_box[PGT_ELEVATION_DISTANCE] )
    gtk_widget_destroy ( widgets->event_box[PGT_ELEVATION_DISTANCE] );
  if ( widgets->event_box[PGT_SPEED_TIME] )
    gtk_widget_destroy ( widgets->event_box[PGT_SPEED_TIME] );
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
