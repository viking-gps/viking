/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2011, Rob Norris <rw_norris@hotmail.com>
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

#define WAYPOINT_FONT "Sans 8"

/* WARNING: If you go beyond this point, we are NOT responsible for any ill effects on your sanity */
/* viktrwlayer.c -- 5000+ lines can make a difference in the state of things */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viking.h"
#include "vikmapslayer.h"
#include "viktrwlayer_tpwin.h"
#include "viktrwlayer_propwin.h"
#ifdef VIK_CONFIG_GEOTAG
#include "viktrwlayer_geotag.h"
#include "geotag_exif.h"
#endif
#include "garminsymbols.h"
#include "thumbnails.h"
#include "background.h"
#include "gpx.h"
#include "babel.h"
#include "dem.h"
#include "dems.h"
#include "geonamessearch.h"
#ifdef VIK_CONFIG_OPENSTREETMAP
#include "osm-traces.h"
#endif
#include "acquire.h"
#include "datasources.h"
#include "util.h"

#include "icons/icons.h"

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

/* Relax some dependencies */
#if ! GLIB_CHECK_VERSION(2,12,0)
static gboolean return_true (gpointer a, gpointer b, gpointer c) { return TRUE; }
static g_hash_table_remove_all (GHashTable *ght) { g_hash_table_foreach_remove ( ght, (GHRFunc) return_true, FALSE ); }
#endif

#define GOOGLE_DIRECTIONS_STRING "maps.google.com/maps?q=from:%s,%s+to:%s,%s&output=kml"
#define VIK_TRW_LAYER_TRACK_GC 13
#define VIK_TRW_LAYER_TRACK_GC_RATES 10
#define VIK_TRW_LAYER_TRACK_GC_MIN 0
#define VIK_TRW_LAYER_TRACK_GC_MAX 11
#define VIK_TRW_LAYER_TRACK_GC_BLACK 12

#define DRAWMODE_BY_TRACK 0
#define DRAWMODE_BY_VELOCITY 1
#define DRAWMODE_ALL_BLACK 2

#define POINTS 1
#define LINES 2

/* this is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400
#define DRAW_ELEVATION_FACTOR 30 /* height of elevation plotting, sort of relative to zoom level ("mpp" that isn't mpp necessarily) */
                                 /* this is multiplied by user-inputted value from 1-100. */
enum {
VIK_TRW_LAYER_SUBLAYER_TRACKS,
VIK_TRW_LAYER_SUBLAYER_WAYPOINTS,
VIK_TRW_LAYER_SUBLAYER_TRACK,
VIK_TRW_LAYER_SUBLAYER_WAYPOINT
};

enum { WP_SYMBOL_FILLED_SQUARE, WP_SYMBOL_SQUARE, WP_SYMBOL_CIRCLE, WP_SYMBOL_X, WP_NUM_SYMBOLS };

struct _VikTrwLayer {
  VikLayer vl;
  GHashTable *tracks;
  GHashTable *tracks_iters;
  GHashTable *waypoints_iters;
  GHashTable *waypoints;
  GtkTreeIter waypoints_iter, tracks_iter;
  gboolean tracks_visible, waypoints_visible;
  guint8 drawmode;
  guint8 drawpoints;
  guint8 drawelevation;
  guint8 elevation_factor;
  guint8 drawstops;
  guint32 stop_length;
  guint8 drawlines;
  guint8 line_thickness;
  guint8 bg_line_thickness;

  guint8 wp_symbol;
  guint8 wp_size;
  gboolean wp_draw_symbols;

  gdouble velocity_min, velocity_max;
  GArray *track_gc;
  guint16 track_gc_iter;
  GdkGC *current_track_gc;
  GdkGC *track_bg_gc;
  GdkGC *waypoint_gc;
  GdkGC *waypoint_text_gc;
  GdkGC *waypoint_bg_gc;
  GdkFont *waypoint_font;
  VikTrack *current_track;
  guint16 ct_x1, ct_y1, ct_x2, ct_y2;
  gboolean ct_sync_done;


  VikCoordMode coord_mode;

  /* wp editing tool */
  VikWaypoint *current_wp;
  gchar *current_wp_name;
  gboolean moving_wp;
  gboolean waypoint_rightclick;

  /* track editing tool */
  GList *current_tpl;
  gchar *current_tp_track_name;
  VikTrwLayerTpwin *tpwin;

  /* weird hack for joining tracks */
  GList *last_tpl;
  gchar *last_tp_track_name;

  /* track editing tool -- more specifically, moving tps */
  gboolean moving_tp;

  /* route finder tool */
  gboolean route_finder_started;
  VikCoord route_finder_coord;
  gboolean route_finder_check_added_track;
  gchar *route_finder_added_track_name;
  VikTrack *route_finder_current_track;
  gboolean route_finder_append;

  gboolean drawlabels;
  gboolean drawimages;
  guint8 image_alpha;
  GQueue *image_cache;
  guint8 image_size;
  guint16 image_cache_size;

  /* for waypoint text */
  PangoLayout *wplabellayout;

  gboolean has_verified_thumbnails;

  GtkMenu *wp_right_click_menu;
  GtkMenu *track_right_click_menu;

  /* menu */
  VikStdLayerMenuItem menu_selection;

  gint highest_wp_number;
};

/* A caached waypoint image. */
typedef struct {
  GdkPixbuf *pixbuf;
  gchar *image; /* filename */
} CachedPixbuf;

struct DrawingParams {
  VikViewport *vp;
  VikTrwLayer *vtl;
  gdouble xmpp, ympp;
  guint16 width, height;
  const VikCoord *center;
  gint track_gc_iter;
  gboolean one_zone, lat_lon;
  gdouble ce1, ce2, cn1, cn2;
};

static void trw_layer_delete_item ( gpointer pass_along[6] );
static void trw_layer_copy_item_cb ( gpointer pass_along[6] );
static void trw_layer_cut_item_cb ( gpointer pass_along[6] );

static void trw_layer_find_maxmin_waypoints ( const gchar *name, const VikWaypoint *w, struct LatLon maxmin[2] );
static void trw_layer_find_maxmin_tracks ( const gchar *name, GList **t, struct LatLon maxmin[2] );	
static void trw_layer_find_maxmin (VikTrwLayer *vtl, struct LatLon maxmin[2]);

static void trw_layer_new_track_gcs ( VikTrwLayer *vtl, VikViewport *vp );
static void trw_layer_free_track_gcs ( VikTrwLayer *vtl );

static gint calculate_velocity ( VikTrwLayer *vtl, VikTrackpoint *tp1, VikTrackpoint *tp2 );
static void trw_layer_draw_track_cb ( const gchar *name, VikTrack *track, struct DrawingParams *dp );
static void trw_layer_draw_waypoint ( const gchar *name, VikWaypoint *wp, struct DrawingParams *dp );

static void goto_coord ( gpointer *vlp, gpointer vvp, gpointer vl, const VikCoord *coord );
static void trw_layer_goto_track_startpoint ( gpointer pass_along[6] );
static void trw_layer_goto_track_endpoint ( gpointer pass_along[6] );
static void trw_layer_goto_track_max_speed ( gpointer pass_along[6] );
static void trw_layer_goto_track_max_alt ( gpointer pass_along[6] );
static void trw_layer_goto_track_min_alt ( gpointer pass_along[6] );
static void trw_layer_goto_track_center ( gpointer pass_along[6] );
static void trw_layer_merge_by_timestamp ( gpointer pass_along[6] );
static void trw_layer_merge_with_other ( gpointer pass_along[6] );
static void trw_layer_split_by_timestamp ( gpointer pass_along[6] );
static void trw_layer_split_by_n_points ( gpointer pass_along[6] );
static void trw_layer_reverse ( gpointer pass_along[6] );
static void trw_layer_download_map_along_track_cb ( gpointer pass_along[6] );
static void trw_layer_edit_trackpoint ( gpointer pass_along[6] );
static void trw_layer_show_picture ( gpointer pass_along[6] );

static void trw_layer_centerize ( gpointer layer_and_vlp[2] );
static void trw_layer_auto_view ( gpointer layer_and_vlp[2] );
static void trw_layer_export ( gpointer layer_and_vlp[2], const gchar* title, const gchar* default_name, const gchar* trackname, guint file_type );
static void trw_layer_goto_wp ( gpointer layer_and_vlp[2] );
static void trw_layer_new_wp ( gpointer lav[2] );
static void trw_layer_auto_waypoints_view ( gpointer lav[2] );
static void trw_layer_auto_tracks_view ( gpointer lav[2] );
static void trw_layer_delete_all_tracks ( gpointer lav[2] );
static void trw_layer_delete_tracks_from_selection ( gpointer lav[2] );
static void trw_layer_delete_all_waypoints ( gpointer lav[2] );
static void trw_layer_delete_waypoints_from_selection ( gpointer lav[2] );
static void trw_layer_new_wikipedia_wp_viewport ( gpointer lav[2] );
static void trw_layer_new_wikipedia_wp_layer ( gpointer lav[2] );
#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_geotagging_waypoint_mtime_keep ( gpointer pass_along[6] );
static void trw_layer_geotagging_waypoint_mtime_update ( gpointer pass_along[6] );
static void trw_layer_geotagging_track ( gpointer pass_along[6] );
static void trw_layer_geotagging ( gpointer lav[2] );
#endif
static void trw_layer_acquire_gps_cb ( gpointer lav[2] );
static void trw_layer_acquire_google_cb ( gpointer lav[2] );
#ifdef VIK_CONFIG_OPENSTREETMAP
static void trw_layer_acquire_osm_cb ( gpointer lav[2] );
#endif
#ifdef VIK_CONFIG_GEOCACHES
static void trw_layer_acquire_geocache_cb ( gpointer lav[2] );
#endif
#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_acquire_geotagged_cb ( gpointer lav[2] );
#endif

/* pop-up items */
static void trw_layer_properties_item ( gpointer pass_along[6] );
static void trw_layer_goto_waypoint ( gpointer pass_along[6] );
static void trw_layer_waypoint_gc_webpage ( gpointer pass_along[6] );

static void trw_layer_realize_waypoint ( gchar *name, VikWaypoint *wp, gpointer pass_along[5] );
static void trw_layer_realize_track ( gchar *name, VikTrack *track, gpointer pass_along[5] );
static void init_drawing_params ( struct DrawingParams *dp, VikViewport *vp );

static void trw_layer_insert_tp_after_current_tp ( VikTrwLayer *vtl );
static void trw_layer_cancel_last_tp ( VikTrwLayer *vtl );
static void trw_layer_cancel_current_tp ( VikTrwLayer *vtl, gboolean destroy );
static void trw_layer_tpwin_response ( VikTrwLayer *vtl, gint response );
static void trw_layer_tpwin_init ( VikTrwLayer *vtl );

static gpointer tool_edit_trackpoint_create ( VikWindow *vw, VikViewport *vvp);
static gboolean tool_edit_trackpoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static gboolean tool_edit_trackpoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data );
static gboolean tool_edit_trackpoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static gpointer tool_show_picture_create ( VikWindow *vw, VikViewport *vvp);
static gboolean tool_show_picture_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp ); 
static gpointer tool_edit_waypoint_create ( VikWindow *vw, VikViewport *vvp);
static gboolean tool_edit_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static gboolean tool_edit_waypoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data );
static gboolean tool_edit_waypoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static gpointer tool_begin_track_create ( VikWindow *vw, VikViewport *vvp);
static gboolean tool_begin_track_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp ); 
static gpointer tool_new_track_create ( VikWindow *vw, VikViewport *vvp);
static gboolean tool_new_track_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp ); 
static VikLayerToolFuncStatus tool_new_track_move ( VikTrwLayer *vtl, GdkEventMotion *event, VikViewport *vvp ); 
static gboolean tool_new_track_key_press ( VikTrwLayer *vtl, GdkEventKey *event, VikViewport *vvp ); 
static gpointer tool_new_waypoint_create ( VikWindow *vw, VikViewport *vvp);
static gboolean tool_new_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp );
static gpointer tool_route_finder_create ( VikWindow *vw, VikViewport *vvp);
static gboolean tool_route_finder_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp );


static void cached_pixbuf_free ( CachedPixbuf *cp );
static gint cached_pixbuf_cmp ( CachedPixbuf *cp, const gchar *name );

static VikTrackpoint *closest_tp_in_five_pixel_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y );
static VikWaypoint *closest_wp_in_five_pixel_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y );

static gchar *get_new_unique_sublayer_name (VikTrwLayer *vtl, gint sublayer_type, const gchar *name);
static void waypoint_convert ( const gchar *name, VikWaypoint *wp, VikCoordMode *dest_mode );
static void track_convert ( const gchar *name, VikTrack *tr, VikCoordMode *dest_mode );

static gchar *highest_wp_number_get(VikTrwLayer *vtl);
static void highest_wp_number_reset(VikTrwLayer *vtl);
static void highest_wp_number_add_wp(VikTrwLayer *vtl, const gchar *new_wp_name);
static void highest_wp_number_remove_wp(VikTrwLayer *vtl, const gchar *old_wp_name);


static VikToolInterface trw_layer_tools[] = {
  { N_("Create Waypoint"), (VikToolConstructorFunc) tool_new_waypoint_create,    NULL, NULL, NULL, 
    (VikToolMouseFunc) tool_new_waypoint_click,    NULL, NULL, (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_addwp_pixbuf },

  { N_("Create Track"),    (VikToolConstructorFunc) tool_new_track_create,       NULL, NULL, NULL, 
    (VikToolMouseFunc) tool_new_track_click, (VikToolMouseMoveFunc) tool_new_track_move, NULL,
    (VikToolKeyFunc) tool_new_track_key_press, GDK_CURSOR_IS_PIXMAP, &cursor_addtr_pixbuf },

  { N_("Begin Track"),    (VikToolConstructorFunc) tool_begin_track_create,       NULL, NULL, NULL, 
    (VikToolMouseFunc) tool_begin_track_click,       NULL, NULL, (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_begintr_pixbuf },

  { N_("Edit Waypoint"),   (VikToolConstructorFunc) tool_edit_waypoint_create,   NULL, NULL, NULL, 
    (VikToolMouseFunc) tool_edit_waypoint_click,   
    (VikToolMouseMoveFunc) tool_edit_waypoint_move,
    (VikToolMouseFunc) tool_edit_waypoint_release, (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_edwp_pixbuf },

  { N_("Edit Trackpoint"), (VikToolConstructorFunc) tool_edit_trackpoint_create, NULL, NULL, NULL, 
    (VikToolMouseFunc) tool_edit_trackpoint_click,
    (VikToolMouseMoveFunc) tool_edit_trackpoint_move,
    (VikToolMouseFunc) tool_edit_trackpoint_release, (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_edtr_pixbuf },

  { N_("Show Picture"),    (VikToolConstructorFunc) tool_show_picture_create,    NULL, NULL, NULL, 
    (VikToolMouseFunc) tool_show_picture_click,    NULL, NULL, (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_showpic_pixbuf },

  { N_("Route Finder"),  (VikToolConstructorFunc) tool_route_finder_create,  NULL, NULL, NULL,
    (VikToolMouseFunc) tool_route_finder_click, NULL, NULL, (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_route_finder_pixbuf },
};
enum { TOOL_CREATE_WAYPOINT=0, TOOL_CREATE_TRACK, TOOL_BEGIN_TRACK, TOOL_EDIT_WAYPOINT, TOOL_EDIT_TRACKPOINT, TOOL_SHOW_PICTURE, NUM_TOOLS };

/****** PARAMETERS ******/

static gchar *params_groups[] = { N_("Waypoints"), N_("Tracks"), N_("Waypoint Images") };
enum { GROUP_WAYPOINTS, GROUP_TRACKS, GROUP_IMAGES };

static gchar *params_drawmodes[] = { N_("Draw by Track"), N_("Draw by Velocity"), N_("All Tracks Black"), 0 };
static gchar *params_wpsymbols[] = { N_("Filled Square"), N_("Square"), N_("Circle"), N_("X"), 0 };


static VikLayerParamScale params_scales[] = {
 /* min  max    step digits */
 {  1,   10,    1,   0 }, /* line_thickness */
 {  0.0, 99.0,  1,   2 }, /* velocity_min */
 {  1.0, 100.0, 1.0, 2 }, /* velocity_max */
                /* 5 * step == how much to turn */
 {  16,   128,  3.2, 0 }, /* image_size */
 {   0,   255,  5,   0 }, /* image alpha */
 {   5,   500,  5,   0 }, /* image cache_size */
 {   0,   8,    1,   0 }, /* image cache_size */
 {   1,  64,    1,   0 }, /* wpsize */
 {   MIN_STOP_LENGTH, MAX_STOP_LENGTH, 1,   0 }, /* stop_length */
 {   1, 100, 1,   0 }, /* stop_length */
};

VikLayerParam trw_layer_params[] = {
  { "tracks_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES },
  { "waypoints_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES },

  { "drawmode", VIK_LAYER_PARAM_UINT, GROUP_TRACKS, N_("Track Drawing Mode:"), VIK_LAYER_WIDGET_RADIOGROUP, NULL },
  { "drawlines", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Track Lines"), VIK_LAYER_WIDGET_CHECKBUTTON },
  { "drawpoints", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Trackpoints"), VIK_LAYER_WIDGET_CHECKBUTTON },
  { "drawelevation", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Elevation"), VIK_LAYER_WIDGET_CHECKBUTTON },
  { "elevation_factor", VIK_LAYER_PARAM_UINT, GROUP_TRACKS, N_("Draw Elevation Height %:"), VIK_LAYER_WIDGET_HSCALE, params_scales + 9 },

  { "drawstops", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Stops"), VIK_LAYER_WIDGET_CHECKBUTTON },
  { "stop_length", VIK_LAYER_PARAM_UINT, GROUP_TRACKS, N_("Min Stop Length (seconds):"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales + 8 },

  { "line_thickness", VIK_LAYER_PARAM_UINT, GROUP_TRACKS, N_("Track Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales + 0 },
  { "bg_line_thickness", VIK_LAYER_PARAM_UINT, GROUP_TRACKS, N_("Track BG Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales + 6 },
  { "trackbgcolor", VIK_LAYER_PARAM_COLOR, GROUP_TRACKS, N_("Track Background Color"), VIK_LAYER_WIDGET_COLOR, 0 },
  { "velocity_min", VIK_LAYER_PARAM_DOUBLE, GROUP_TRACKS, N_("Min Track Velocity:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales + 1 },
  { "velocity_max", VIK_LAYER_PARAM_DOUBLE, GROUP_TRACKS, N_("Max Track Velocity:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales + 2 },

  { "drawlabels", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Draw Labels"), VIK_LAYER_WIDGET_CHECKBUTTON },
  { "wpcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Waypoint Color:"), VIK_LAYER_WIDGET_COLOR, 0 },
  { "wptextcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Waypoint Text:"), VIK_LAYER_WIDGET_COLOR, 0 },
  { "wpbgcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Background:"), VIK_LAYER_WIDGET_COLOR, 0 },
  { "wpbgand", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Fake BG Color Translucency:"), VIK_LAYER_WIDGET_CHECKBUTTON, 0 },
  { "wpsymbol", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint marker:"), VIK_LAYER_WIDGET_RADIOGROUP, NULL },
  { "wpsize", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint size:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales + 7 },
  { "wpsyms", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Draw Waypoint Symbols:"), VIK_LAYER_WIDGET_CHECKBUTTON },

  { "drawimages", VIK_LAYER_PARAM_BOOLEAN, GROUP_IMAGES, N_("Draw Waypoint Images"), VIK_LAYER_WIDGET_CHECKBUTTON },
  { "image_size", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Size (pixels):"), VIK_LAYER_WIDGET_HSCALE, params_scales + 3 },
  { "image_alpha", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales + 4 },
  { "image_cache_size", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Memory Cache Size:"), VIK_LAYER_WIDGET_HSCALE, params_scales + 5 },
};

enum { PARAM_TV, PARAM_WV, PARAM_DM, PARAM_DL, PARAM_DP, PARAM_DE, PARAM_EF, PARAM_DS, PARAM_SL, PARAM_LT, PARAM_BLT, PARAM_TBGC, PARAM_VMIN, PARAM_VMAX, PARAM_DLA, PARAM_WPC, PARAM_WPTC, PARAM_WPBC, PARAM_WPBA, PARAM_WPSYM, PARAM_WPSIZE, PARAM_WPSYMS, PARAM_DI, PARAM_IS, PARAM_IA, PARAM_ICS, NUM_PARAMS };

/*** TO ADD A PARAM:
 *** 1) Add to trw_layer_params and enumeration
 *** 2) Handle in get_param & set_param (presumably adding on to VikTrwLayer struct)
 ***/

/****** END PARAMETERS ******/

static VikTrwLayer* trw_layer_new ( gint drawmode );
/* Layer Interface function definitions */
static VikTrwLayer* trw_layer_create ( VikViewport *vp );
static void trw_layer_realize ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter );
static void trw_layer_free ( VikTrwLayer *trwlayer );
static void trw_layer_draw ( VikTrwLayer *l, gpointer data );
static void trw_layer_change_coord_mode ( VikTrwLayer *vtl, VikCoordMode dest_mode );
static void trw_layer_set_menu_selection ( VikTrwLayer *vtl, guint16 );
static guint16 trw_layer_get_menu_selection ( VikTrwLayer *vtl );
static void trw_layer_add_menu_items ( VikTrwLayer *vtl, GtkMenu *menu, gpointer vlp );
static gboolean trw_layer_sublayer_add_menu_items ( VikTrwLayer *l, GtkMenu *menu, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter, VikViewport *vvp );
static const gchar* trw_layer_sublayer_rename_request ( VikTrwLayer *l, const gchar *newname, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter );
static gboolean trw_layer_sublayer_toggle_visible ( VikTrwLayer *l, gint subtype, gpointer sublayer );
static const gchar* trw_layer_layer_tooltip ( VikTrwLayer *vtl );
static const gchar* trw_layer_sublayer_tooltip ( VikTrwLayer *l, gint subtype, gpointer sublayer );
static gboolean trw_layer_selected ( VikTrwLayer *l, gint subtype, gpointer sublayer, gint type, gpointer vlp );
static void trw_layer_marshall ( VikTrwLayer *vtl, guint8 **data, gint *len );
static VikTrwLayer *trw_layer_unmarshall ( guint8 *data, gint len, VikViewport *vvp );
static gboolean trw_layer_set_param ( VikTrwLayer *vtl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation );
static VikLayerParamData trw_layer_get_param ( VikTrwLayer *vtl, guint16 id, gboolean is_file_operation );
static void trw_layer_del_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer );
static void trw_layer_cut_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer );
static void trw_layer_copy_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer, guint8 **item, guint *len );
static gboolean trw_layer_paste_item ( VikTrwLayer *vtl, gint subtype, guint8 *item, guint len );
static void trw_layer_free_copied_item ( gint subtype, gpointer item );
static void trw_layer_drag_drop_request ( VikTrwLayer *vtl_src, VikTrwLayer *vtl_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path );
static gboolean trw_layer_select_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t *t );
static gboolean trw_layer_select_move ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t *t );
static gboolean trw_layer_select_release ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t *t );
static gboolean trw_layer_show_selected_viewport_menu ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp );
/* End Layer Interface function definitions */

VikLayerInterface vik_trw_layer_interface = {
  "TrackWaypoint",
  &viktrwlayer_pixbuf,

  trw_layer_tools,
  sizeof(trw_layer_tools) / sizeof(VikToolInterface),

  trw_layer_params,
  NUM_PARAMS,
  params_groups, /* params_groups */
  sizeof(params_groups)/sizeof(params_groups[0]),    /* number of groups */

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  trw_layer_create,
  (VikLayerFuncRealize)                 trw_layer_realize,
  (VikLayerFuncPostRead)                trw_layer_verify_thumbnails,
  (VikLayerFuncFree)                    trw_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    trw_layer_draw,
  (VikLayerFuncChangeCoordMode)         trw_layer_change_coord_mode,

  (VikLayerFuncSetMenuItemsSelection)   trw_layer_set_menu_selection,
  (VikLayerFuncGetMenuItemsSelection)   trw_layer_get_menu_selection,

  (VikLayerFuncAddMenuItems)            trw_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    trw_layer_sublayer_add_menu_items,

  (VikLayerFuncSublayerRenameRequest)   trw_layer_sublayer_rename_request,
  (VikLayerFuncSublayerToggleVisible)   trw_layer_sublayer_toggle_visible,
  (VikLayerFuncSublayerTooltip)         trw_layer_sublayer_tooltip,
  (VikLayerFuncLayerTooltip)            trw_layer_layer_tooltip,
  (VikLayerFuncLayerSelected)           trw_layer_selected,

  (VikLayerFuncMarshall)                trw_layer_marshall,
  (VikLayerFuncUnmarshall)              trw_layer_unmarshall,

  (VikLayerFuncSetParam)                trw_layer_set_param,
  (VikLayerFuncGetParam)                trw_layer_get_param,

  (VikLayerFuncReadFileData)            a_gpspoint_read_file,
  (VikLayerFuncWriteFileData)           a_gpspoint_write_file,

  (VikLayerFuncDeleteItem)              trw_layer_del_item,
  (VikLayerFuncCutItem)                 trw_layer_cut_item,
  (VikLayerFuncCopyItem)                trw_layer_copy_item,
  (VikLayerFuncPasteItem)               trw_layer_paste_item,
  (VikLayerFuncFreeCopiedItem)          trw_layer_free_copied_item,
  
  (VikLayerFuncDragDropRequest)         trw_layer_drag_drop_request,

  (VikLayerFuncSelectClick)             trw_layer_select_click,
  (VikLayerFuncSelectMove)              trw_layer_select_move,
  (VikLayerFuncSelectRelease)           trw_layer_select_release,
  (VikLayerFuncSelectedViewportMenu)    trw_layer_show_selected_viewport_menu,
};

/* for copy & paste (I think?) */
typedef struct {
  guint len;
  guint8 data[0];
  //  gchar *name;
  //  VikWaypoint *wp;
} FlatItem;

GType vik_trw_layer_get_type ()
{
  static GType vtl_type = 0;

  if (!vtl_type)
  {
    static const GTypeInfo vtl_info =
    {
      sizeof (VikTrwLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikTrwLayer),
      0,
      NULL /* instance init */
    };
    vtl_type = g_type_register_static ( VIK_LAYER_TYPE, "VikTrwLayer", &vtl_info, 0 );
  }

  return vtl_type;
}

static void trw_layer_del_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer )
{
  static gpointer pass_along[6];
  if (!sublayer) {
    return;
  }
  
  pass_along[0] = vtl;
  pass_along[1] = NULL;
  pass_along[2] = GINT_TO_POINTER (subtype);
  pass_along[3] = sublayer;
  pass_along[4] = GINT_TO_POINTER (1); // Confirm delete request
  pass_along[5] = NULL;

  trw_layer_delete_item ( pass_along );
}

static void trw_layer_cut_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer )
{
  static gpointer pass_along[6];
  if (!sublayer) {
    return;
  }

  pass_along[0] = vtl;
  pass_along[1] = NULL;
  pass_along[2] = GINT_TO_POINTER (subtype);
  pass_along[3] = sublayer;
  pass_along[4] = GINT_TO_POINTER (0); // No delete confirmation needed for auto delete
  pass_along[5] = NULL;

  trw_layer_copy_item_cb(pass_along);
  trw_layer_cut_item_cb(pass_along);
}

static void trw_layer_copy_item_cb ( gpointer pass_along[6])
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(pass_along[0]);
  gint subtype = GPOINTER_TO_INT (pass_along[2]);
  gpointer * sublayer = pass_along[3];
  guint8 *data = NULL;
  guint len;

  trw_layer_copy_item( vtl, subtype, sublayer, &data, &len);

  if (data) {
    a_clipboard_copy( VIK_CLIPBOARD_DATA_SUBLAYER, VIK_LAYER_TRW,
		      subtype, len, (const gchar*) sublayer, data);
  }
}

static void trw_layer_cut_item_cb ( gpointer pass_along[6])
{
  trw_layer_copy_item_cb(pass_along);
  pass_along[4] = GINT_TO_POINTER (0); // Never need to confirm automatic delete
  trw_layer_delete_item(pass_along);
}

static void trw_layer_copy_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer, guint8 **item, guint *len )
{
  FlatItem *fi;
  guint8 *id;
  guint il;

  if (!sublayer) {
    *item = NULL;
    return;
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    vik_waypoint_marshall ( g_hash_table_lookup ( vtl->waypoints, sublayer ), &id, &il );
  } else {
    vik_track_marshall ( g_hash_table_lookup ( vtl->tracks, sublayer ), &id, &il );
  }

  *len = sizeof(FlatItem) + strlen(sublayer) + 1 + il;
  fi = g_malloc ( *len );
  fi->len = strlen(sublayer) + 1;
  memcpy(fi->data, sublayer, fi->len);
  memcpy(fi->data + fi->len, id, il);
  g_free(id);
  *item = (guint8 *)fi;
}

static gboolean trw_layer_paste_item ( VikTrwLayer *vtl, gint subtype, guint8 *item, guint len )
{
  FlatItem *fi = (FlatItem *) item;

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT && fi )
  {
    VikWaypoint *w;
    gchar *name;

    name = get_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, (gchar *)fi->data);
    w = vik_waypoint_unmarshall(fi->data + fi->len, len - sizeof(*fi) - fi->len);
    vik_trw_layer_add_waypoint ( vtl, name, w );
    waypoint_convert(name, w, &vtl->coord_mode);
    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->waypoints_visible && w->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK && fi )
  {
    VikTrack *t;
    gchar *name;
    name = get_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, (gchar *)fi->data);
    t = vik_track_unmarshall(fi->data + fi->len, len - sizeof(*fi) - fi->len);
    vik_trw_layer_add_track ( vtl, name, t );
    track_convert(name, t, &vtl->coord_mode);
    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->tracks_visible && t->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }
  return FALSE;
}

static void trw_layer_free_copied_item ( gint subtype, gpointer item )
{
  if (item) {
    g_free(item);
  }
}

static gboolean trw_layer_set_param ( VikTrwLayer *vtl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
  switch ( id )
  {
    case PARAM_TV: vtl->tracks_visible = data.b; break;
    case PARAM_WV: vtl->waypoints_visible = data.b; break;
    case PARAM_DM: vtl->drawmode = data.u; break;
    case PARAM_DP: vtl->drawpoints = data.b; break;
    case PARAM_DE: vtl->drawelevation = data.b; break;
    case PARAM_DS: vtl->drawstops = data.b; break;
    case PARAM_DL: vtl->drawlines = data.b; break;
    case PARAM_SL: if ( data.u >= MIN_STOP_LENGTH && data.u <= MAX_STOP_LENGTH )
                     vtl->stop_length = data.u;
                   break;
    case PARAM_EF: if ( data.u >= 1 && data.u <= 100 )
                     vtl->elevation_factor = data.u;
                   break;
    case PARAM_LT: if ( data.u > 0 && data.u < 15 && data.u != vtl->line_thickness )
                   {
                     vtl->line_thickness = data.u;
                     trw_layer_new_track_gcs ( vtl, vp );
                   }
                   break;
    case PARAM_BLT: if ( data.u >= 0 && data.u <= 8 && data.u != vtl->bg_line_thickness )
                   {
                     vtl->bg_line_thickness = data.u;
                     trw_layer_new_track_gcs ( vtl, vp );
                   }
                   break;
    case PARAM_VMIN:
    {
      /* Convert to store internally
         NB file operation always in internal units (metres per second) */
      vik_units_speed_t speed_units = a_vik_get_units_speed ();
      if ( is_file_operation || speed_units == VIK_UNITS_SPEED_METRES_PER_SECOND )
	vtl->velocity_min = data.d;
      else if ( speed_units == VIK_UNITS_SPEED_KILOMETRES_PER_HOUR )
	vtl->velocity_min = VIK_KPH_TO_MPS(data.d);
      else if ( speed_units == VIK_UNITS_SPEED_MILES_PER_HOUR )
	vtl->velocity_min = VIK_MPH_TO_MPS(data.d);
      else
	/* Knots */
	vtl->velocity_min = VIK_KNOTS_TO_MPS(data.d);
      break;
    }
    case PARAM_VMAX:
    {
      /* Convert to store internally
         NB file operation always in internal units (metres per second) */
      vik_units_speed_t speed_units = a_vik_get_units_speed ();
      if ( is_file_operation || speed_units == VIK_UNITS_SPEED_METRES_PER_SECOND )
	vtl->velocity_max = data.d;
      else if ( speed_units == VIK_UNITS_SPEED_KILOMETRES_PER_HOUR )
	vtl->velocity_max = VIK_KPH_TO_MPS(data.d);
      else if ( speed_units == VIK_UNITS_SPEED_MILES_PER_HOUR )
	vtl->velocity_max = VIK_MPH_TO_MPS(data.d);
      else
	/* Knots */
	vtl->velocity_max = VIK_KNOTS_TO_MPS(data.d);
      break;
    }
    case PARAM_TBGC: gdk_gc_set_rgb_fg_color(vtl->track_bg_gc, &(data.c)); break;
    case PARAM_DLA: vtl->drawlabels = data.b; break;
    case PARAM_DI: vtl->drawimages = data.b; break;
    case PARAM_IS: if ( data.u != vtl->image_size )
      {
        vtl->image_size = data.u;
        g_list_foreach ( vtl->image_cache->head, (GFunc) cached_pixbuf_free, NULL );
        g_queue_free ( vtl->image_cache );
        vtl->image_cache = g_queue_new ();
      }
      break;
    case PARAM_IA: vtl->image_alpha = data.u; break;
    case PARAM_ICS: vtl->image_cache_size = data.u;
      while ( vtl->image_cache->length > vtl->image_cache_size ) /* if shrinking cache_size, free pixbuf ASAP */
          cached_pixbuf_free ( g_queue_pop_tail ( vtl->image_cache ) );
      break;
    case PARAM_WPC: gdk_gc_set_rgb_fg_color(vtl->waypoint_gc, &(data.c)); break;
    case PARAM_WPTC: gdk_gc_set_rgb_fg_color(vtl->waypoint_text_gc, &(data.c)); break;
    case PARAM_WPBC: gdk_gc_set_rgb_fg_color(vtl->waypoint_bg_gc, &(data.c)); break;
    case PARAM_WPBA: gdk_gc_set_function(vtl->waypoint_bg_gc, data.b ? GDK_AND : GDK_COPY ); break;
    case PARAM_WPSYM: if ( data.u < WP_NUM_SYMBOLS ) vtl->wp_symbol = data.u; break;
    case PARAM_WPSIZE: if ( data.u > 0 && data.u <= 64 ) vtl->wp_size = data.u; break;
    case PARAM_WPSYMS: vtl->wp_draw_symbols = data.b; break;
  }
  return TRUE;
}

static VikLayerParamData trw_layer_get_param ( VikTrwLayer *vtl, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_TV: rv.b = vtl->tracks_visible; break;
    case PARAM_WV: rv.b = vtl->waypoints_visible; break;
    case PARAM_DM: rv.u = vtl->drawmode; break;
    case PARAM_DP: rv.b = vtl->drawpoints; break;
    case PARAM_DE: rv.b = vtl->drawelevation; break;
    case PARAM_EF: rv.u = vtl->elevation_factor; break;
    case PARAM_DS: rv.b = vtl->drawstops; break;
    case PARAM_SL: rv.u = vtl->stop_length; break;
    case PARAM_DL: rv.b = vtl->drawlines; break;
    case PARAM_LT: rv.u = vtl->line_thickness; break;
    case PARAM_BLT: rv.u = vtl->bg_line_thickness; break;
    case PARAM_VMIN:
    {
      /* Convert to store internally
         NB file operation always in internal units (metres per second) */
      vik_units_speed_t speed_units = a_vik_get_units_speed ();
      if ( is_file_operation || speed_units == VIK_UNITS_SPEED_METRES_PER_SECOND )
	rv.d = vtl->velocity_min;
      else if ( speed_units == VIK_UNITS_SPEED_KILOMETRES_PER_HOUR )
	rv.d = VIK_MPS_TO_KPH(vtl->velocity_min);
      else if ( speed_units == VIK_UNITS_SPEED_MILES_PER_HOUR )
	rv.d = VIK_MPS_TO_MPH(vtl->velocity_min);
      else
	/* Knots */
	rv.d = VIK_MPS_TO_KNOTS(vtl->velocity_min);
      break;
    }
    case PARAM_VMAX:
    {
      /* Convert to store internally
         NB file operation always in internal units (metres per second) */
      vik_units_speed_t speed_units = a_vik_get_units_speed ();
      if ( is_file_operation || speed_units == VIK_UNITS_SPEED_METRES_PER_SECOND )
	rv.d = vtl->velocity_max;
      else if ( speed_units == VIK_UNITS_SPEED_KILOMETRES_PER_HOUR )
	rv.d = VIK_MPS_TO_KPH(vtl->velocity_max);
      else if ( speed_units == VIK_UNITS_SPEED_MILES_PER_HOUR )
	rv.d = VIK_MPS_TO_MPH(vtl->velocity_max);
      else
	/* Knots */
	rv.d = VIK_MPS_TO_KNOTS(vtl->velocity_max);
      break;
    }
    case PARAM_DLA: rv.b = vtl->drawlabels; break;
    case PARAM_DI: rv.b = vtl->drawimages; break;
    case PARAM_TBGC: vik_gc_get_fg_color(vtl->track_bg_gc, &(rv.c)); break;
    case PARAM_IS: rv.u = vtl->image_size; break;
    case PARAM_IA: rv.u = vtl->image_alpha; break;
    case PARAM_ICS: rv.u = vtl->image_cache_size; break;
    case PARAM_WPC: vik_gc_get_fg_color(vtl->waypoint_gc, &(rv.c)); break;
    case PARAM_WPTC: vik_gc_get_fg_color(vtl->waypoint_text_gc, &(rv.c)); break;
    case PARAM_WPBC: vik_gc_get_fg_color(vtl->waypoint_bg_gc, &(rv.c)); break;
    case PARAM_WPBA: rv.b = (vik_gc_get_function(vtl->waypoint_bg_gc)==GDK_AND); break;
    case PARAM_WPSYM: rv.u = vtl->wp_symbol; break;
    case PARAM_WPSIZE: rv.u = vtl->wp_size; break;
    case PARAM_WPSYMS: rv.b = vtl->wp_draw_symbols; break;
  }
  return rv;
}

static void trw_layer_marshall( VikTrwLayer *vtl, guint8 **data, gint *len )
{
  guint8 *pd;
  gchar *dd;
  gsize dl;
  gint pl;
  gchar *tmpname;
  FILE *f;

  *data = NULL;

  if ((f = fdopen(g_file_open_tmp (NULL, &tmpname, NULL), "r+"))) {
    a_gpx_write_file(vtl, f);
    vik_layer_marshall_params(VIK_LAYER(vtl), &pd, &pl);
    fclose(f);
    f = NULL;
    g_file_get_contents(tmpname, &dd, &dl, NULL);
    *len = sizeof(pl) + pl + dl;
    *data = g_malloc(*len);
    memcpy(*data, &pl, sizeof(pl));
    memcpy(*data + sizeof(pl), pd, pl);
    memcpy(*data + sizeof(pl) + pl, dd, dl);
    
    g_free(pd);
    g_free(dd);
    g_remove(tmpname);
    g_free(tmpname);
  }
}

static VikTrwLayer *trw_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikTrwLayer *rv = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vvp, NULL, FALSE ));
  gint pl;
  gchar *tmpname;
  FILE *f;


  memcpy(&pl, data, sizeof(pl));
  data += sizeof(pl);
  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, pl, vvp );
  data += pl;

  if (!(f = fdopen(g_file_open_tmp (NULL, &tmpname, NULL), "r+"))) {
    g_critical("couldn't open temp file");
    exit(1);
  }
  fwrite(data, len - pl - sizeof(pl), 1, f);
  rewind(f);
  a_gpx_read_file(rv, f);
  fclose(f);
  f = NULL;
  g_remove(tmpname);
  g_free(tmpname);
  return rv;
}

static GList * str_array_to_glist(gchar* data[])
{
  GList *gl = NULL;
  gpointer * p;
  for (p = (gpointer)data; *p; p++)
    gl = g_list_prepend(gl, *p);
  return(g_list_reverse(gl));
}

static gboolean strcase_equal(gconstpointer s1, gconstpointer s2)
{
  return (strcasecmp(s1, s2) == 0);
}

static guint strcase_hash(gconstpointer v)
{
  /* 31 bit hash function */
  int i;
  const gchar *t = v;
  gchar s[128];   /* malloc is too slow for reading big files */
  gchar *p = s;

  for (i = 0; (i < (sizeof(s)- 1)) && t[i]; i++)
      p[i] = toupper(t[i]);
  p[i] = '\0';

  p = s;
  guint32 h = *p;
  if (h) {
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;
  }

  return h;  
}

static VikTrwLayer* trw_layer_new ( gint drawmode )
{
  if (trw_layer_params[PARAM_DM].widget_data == NULL)
    trw_layer_params[PARAM_DM].widget_data = str_array_to_glist(params_drawmodes);
  if (trw_layer_params[PARAM_WPSYM].widget_data == NULL)
    trw_layer_params[PARAM_WPSYM].widget_data = str_array_to_glist(params_wpsymbols);

  VikTrwLayer *rv = VIK_TRW_LAYER ( g_object_new ( VIK_TRW_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(rv), VIK_LAYER_TRW );

  rv->waypoints = g_hash_table_new_full ( strcase_hash, strcase_equal, g_free, (GDestroyNotify) vik_waypoint_free );
  rv->tracks = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, (GDestroyNotify) vik_track_free );
  rv->tracks_iters = g_hash_table_new_full ( g_str_hash, g_str_equal, NULL, g_free );
  rv->waypoints_iters = g_hash_table_new_full ( strcase_hash, strcase_equal, NULL, g_free );

  /* TODO: constants at top */
  rv->waypoints_visible = rv->tracks_visible = TRUE;
  rv->drawmode = drawmode;
  rv->drawpoints = TRUE;
  rv->drawstops = FALSE;
  rv->drawelevation = FALSE;
  rv->elevation_factor = 30;
  rv->stop_length = 60;
  rv->drawlines = TRUE;
  rv->wplabellayout = NULL;
  rv->wp_right_click_menu = NULL;
  rv->track_right_click_menu = NULL;
  rv->waypoint_gc = NULL;
  rv->waypoint_text_gc = NULL;
  rv->waypoint_bg_gc = NULL;
  rv->track_gc = NULL;
  rv->velocity_max = 5.0;
  rv->velocity_min = 0.0;
  rv->line_thickness = 1;
  rv->bg_line_thickness = 0;
  rv->current_wp = NULL;
  rv->current_wp_name = NULL;
  rv->current_track = NULL;
  rv->current_tpl = NULL;
  rv->current_tp_track_name = NULL;
  rv->moving_tp = FALSE;
  rv->moving_wp = FALSE;

  rv->ct_sync_done = TRUE;

  rv->route_finder_started = FALSE;
  rv->route_finder_check_added_track = FALSE;
  rv->route_finder_added_track_name = NULL;
  rv->route_finder_current_track = NULL;
  rv->route_finder_append = FALSE;

  rv->waypoint_rightclick = FALSE;
  rv->last_tpl = NULL;
  rv->last_tp_track_name = NULL;
  rv->tpwin = NULL;
  rv->image_cache = g_queue_new();
  rv->image_size = 64;
  rv->image_alpha = 255;
  rv->image_cache_size = 300;
  rv->drawimages = TRUE;
  rv->drawlabels = TRUE;
  return rv;
}


static void trw_layer_free ( VikTrwLayer *trwlayer )
{
  g_hash_table_destroy(trwlayer->waypoints);
  g_hash_table_destroy(trwlayer->tracks);

  /* ODC: replace with GArray */
  trw_layer_free_track_gcs ( trwlayer );

  if ( trwlayer->wp_right_click_menu )
    g_object_ref_sink ( G_OBJECT(trwlayer->wp_right_click_menu) );

  if ( trwlayer->track_right_click_menu )
    gtk_object_sink ( GTK_OBJECT(trwlayer->track_right_click_menu) );

  if ( trwlayer->wplabellayout != NULL)
    g_object_unref ( G_OBJECT ( trwlayer->wplabellayout ) );

  if ( trwlayer->waypoint_gc != NULL )
    g_object_unref ( G_OBJECT ( trwlayer->waypoint_gc ) );

  if ( trwlayer->waypoint_text_gc != NULL )
    g_object_unref ( G_OBJECT ( trwlayer->waypoint_text_gc ) );

  if ( trwlayer->waypoint_bg_gc != NULL )
    g_object_unref ( G_OBJECT ( trwlayer->waypoint_bg_gc ) );

  if ( trwlayer->waypoint_font != NULL )
    gdk_font_unref ( trwlayer->waypoint_font );

  if ( trwlayer->tpwin != NULL )
    gtk_widget_destroy ( GTK_WIDGET(trwlayer->tpwin) );

  g_list_foreach ( trwlayer->image_cache->head, (GFunc) cached_pixbuf_free, NULL );
  g_queue_free ( trwlayer->image_cache );
}

static void init_drawing_params ( struct DrawingParams *dp, VikViewport *vp )
{
  dp->vp = vp;
  dp->xmpp = vik_viewport_get_xmpp ( vp );
  dp->ympp = vik_viewport_get_ympp ( vp );
  dp->width = vik_viewport_get_width ( vp );
  dp->height = vik_viewport_get_height ( vp );
  dp->center = vik_viewport_get_center ( vp );
  dp->one_zone = vik_viewport_is_one_zone ( vp ); /* false if some other projection besides UTM */
  dp->lat_lon = vik_viewport_get_coord_mode ( vp ) == VIK_COORD_LATLON;

  if ( dp->one_zone )
  {
    gint w2, h2;
    w2 = dp->xmpp * (dp->width / 2) + 1600 / dp->xmpp; 
    h2 = dp->ympp * (dp->height / 2) + 1600 / dp->ympp;
    /* leniency -- for tracks. Obviously for waypoints this SHOULD be a lot smaller */
 
    dp->ce1 = dp->center->east_west-w2; 
    dp->ce2 = dp->center->east_west+w2;
    dp->cn1 = dp->center->north_south-h2;
    dp->cn2 = dp->center->north_south+h2;
  } else if ( dp->lat_lon ) {
    VikCoord upperleft, bottomright;
    /* quick & dirty calculation; really want to check all corners due to lat/lon smaller at top in northern hemisphere */
    /* this also DOESN'T WORK if you are crossing 180/-180 lon. I don't plan to in the near future...  */
    vik_viewport_screen_to_coord ( vp, -500, -500, &upperleft );
    vik_viewport_screen_to_coord ( vp, dp->width+500, dp->height+500, &bottomright );
    dp->ce1 = upperleft.east_west;
    dp->ce2 = bottomright.east_west;
    dp->cn1 = bottomright.north_south;
    dp->cn2 = upperleft.north_south;
  }

  dp->track_gc_iter = 0;
}

static gint calculate_velocity ( VikTrwLayer *vtl, VikTrackpoint *tp1, VikTrackpoint *tp2 )
{
  static gdouble rv = 0;
  if ( tp1->has_timestamp && tp2->has_timestamp )
  {
    rv = ( vik_coord_diff ( &(tp1->coord), &(tp2->coord) )
           / (tp1->timestamp - tp2->timestamp) ) - vtl->velocity_min;

    if ( rv < 0 )
      return VIK_TRW_LAYER_TRACK_GC_MIN;
    else if ( vtl->velocity_min >= vtl->velocity_max )
      return VIK_TRW_LAYER_TRACK_GC_MAX;

    rv *= (VIK_TRW_LAYER_TRACK_GC_RATES / (vtl->velocity_max - vtl->velocity_min));

    if ( rv >= VIK_TRW_LAYER_TRACK_GC_MAX )
      return VIK_TRW_LAYER_TRACK_GC_MAX;
    return (gint) rv;
 }
 else
   return VIK_TRW_LAYER_TRACK_GC_BLACK;
}

void draw_utm_skip_insignia ( VikViewport *vvp, GdkGC *gc, gint x, gint y )
{
  vik_viewport_draw_line ( vvp, gc, x+5, y, x-5, y );
  vik_viewport_draw_line ( vvp, gc, x, y+5, x, y-5 );
  vik_viewport_draw_line ( vvp, gc, x+5, y+5, x-5, y-5 );
  vik_viewport_draw_line ( vvp, gc, x+5, y-5, x-5, y+5 );
}

static void trw_layer_draw_track ( const gchar *name, VikTrack *track, struct DrawingParams *dp, gboolean drawing_white_background )
{
  /* TODO: this function is a mess, get rid of any redundancy */
  GList *list = track->trackpoints;
  GdkGC *main_gc;
  gboolean useoldvals = TRUE;

  gboolean drawpoints;
  gboolean drawstops;
  gboolean drawelevation;
  gdouble min_alt, max_alt, alt_diff = 0;

  const guint8 tp_size_reg = 2;
  const guint8 tp_size_cur = 4;
  guint8 tp_size;

  if ( dp->vtl->drawelevation )
  {
    /* assume if it has elevation at the beginning, it has it throughout. not ness a true good assumption */
    if ( ( drawelevation = vik_track_get_minmax_alt ( track, &min_alt, &max_alt ) ) )
      alt_diff = max_alt - min_alt;
  }

  if ( ! track->visible )
    return;

  /* admittedly this is not an efficient way to do it because we go through the whole GC thing all over... */
  if ( dp->vtl->bg_line_thickness && !drawing_white_background )
    trw_layer_draw_track ( name, track, dp, TRUE );

  if ( drawing_white_background )
    drawpoints = drawstops = FALSE;
  else {
    drawpoints = dp->vtl->drawpoints;
    drawstops = dp->vtl->drawstops;
  }

  /* Current track - used for creation */
  if ( track == dp->vtl->current_track )
    main_gc = dp->vtl->current_track_gc;
  else {
    if ( vik_viewport_get_draw_highlight ( dp->vp ) ) {
      /* Draw all tracks of the layer in special colour */
      /* if track is member of selected layer or is the current selected track
	 then draw in the highlight colour.
	 NB this supercedes the drawmode */
      if ( dp->vtl && ( ( dp->vtl == vik_window_get_selected_trw_layer ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ) ||
			( dp->vtl->tracks == vik_window_get_selected_tracks ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ) ||
			track == vik_window_get_selected_track ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ) ) {
	main_gc = vik_viewport_get_gc_highlight (dp->vp);
      }
      else {
	if ( dp->vtl->drawmode == DRAWMODE_ALL_BLACK )
	  dp->track_gc_iter = VIK_TRW_LAYER_TRACK_GC_BLACK;

	main_gc = g_array_index(dp->vtl->track_gc, GdkGC *, dp->track_gc_iter);
      }
    }
    else {
      if ( dp->vtl->drawmode == DRAWMODE_ALL_BLACK )
	dp->track_gc_iter = VIK_TRW_LAYER_TRACK_GC_BLACK;
	  
      main_gc = g_array_index(dp->vtl->track_gc, GdkGC *, dp->track_gc_iter);
    }
  }

  if (list) {
    int x, y, oldx, oldy;
    VikTrackpoint *tp = VIK_TRACKPOINT(list->data);
  
    tp_size = (list == dp->vtl->current_tpl) ? tp_size_cur : tp_size_reg;

    vik_viewport_coord_to_screen ( dp->vp, &(tp->coord), &x, &y );

    if ( (drawpoints) && dp->track_gc_iter < VIK_TRW_LAYER_TRACK_GC )
    {
      GdkPoint trian[3] = { { x, y-(3*tp_size) }, { x-(2*tp_size), y+(2*tp_size) }, {x+(2*tp_size), y+(2*tp_size)} };
      vik_viewport_draw_polygon ( dp->vp, main_gc, TRUE, trian, 3 );
    }

    oldx = x;
    oldy = y;

    while ((list = g_list_next(list)))
    {
      tp = VIK_TRACKPOINT(list->data);
      tp_size = (list == dp->vtl->current_tpl) ? tp_size_cur : tp_size_reg;

      /* check some stuff -- but only if we're in UTM and there's only ONE ZONE; or lat lon */
      if ( (!dp->one_zone && !dp->lat_lon) ||     /* UTM & zones; do everything */
             ( ((!dp->one_zone) || tp->coord.utm_zone == dp->center->utm_zone) &&   /* only check zones if UTM & one_zone */
             tp->coord.east_west < dp->ce2 && tp->coord.east_west > dp->ce1 &&  /* both UTM and lat lon */
             tp->coord.north_south > dp->cn1 && tp->coord.north_south < dp->cn2 ) )
      {
        vik_viewport_coord_to_screen ( dp->vp, &(tp->coord), &x, &y );

	/*
	 * If points are the same in display coordinates, don't draw.
	 */
	if ( useoldvals && x == oldx && y == oldy )
	{
	  // Still need to process points to ensure 'stops' are drawn if required
	  if ( drawstops && drawpoints && ! drawing_white_background && list->next &&
	       (VIK_TRACKPOINT(list->next->data)->timestamp - VIK_TRACKPOINT(list->data)->timestamp > dp->vtl->stop_length) )
	    vik_viewport_draw_arc ( dp->vp, g_array_index(dp->vtl->track_gc, GdkGC *, 11), TRUE, x-(3*tp_size), y-(3*tp_size), 6*tp_size, 6*tp_size, 0, 360*64 );

	  goto skip;
	}

        if ( drawpoints && ! drawing_white_background )
        {
          if ( list->next ) {
	    /*
	     * The concept of drawing stops is that a trackpoint
	     * that is if the next trackpoint has a timestamp far into
	     * the future, we draw a circle of 6x trackpoint size,
	     * instead of a rectangle of 2x trackpoint size.
	     * This is drawn first so the trackpoint will be drawn on top
	     */
            /* stops */
            if ( drawstops && VIK_TRACKPOINT(list->next->data)->timestamp - VIK_TRACKPOINT(list->data)->timestamp > dp->vtl->stop_length )
	      /* Stop point.  Draw 6x circle. */
              vik_viewport_draw_arc ( dp->vp, g_array_index(dp->vtl->track_gc, GdkGC *, 11), TRUE, x-(3*tp_size), y-(3*tp_size), 6*tp_size, 6*tp_size, 0, 360*64 );

	    /* Regular point - draw 2x square. */
	    vik_viewport_draw_rectangle ( dp->vp, main_gc, TRUE, x-tp_size, y-tp_size, 2*tp_size, 2*tp_size );
          }
          else
	    /* Final point - draw 4x circle. */
            vik_viewport_draw_arc ( dp->vp, main_gc, TRUE, x-(2*tp_size), y-(2*tp_size), 4*tp_size, 4*tp_size, 0, 360*64 );
        }

        if ((!tp->newsegment) && (dp->vtl->drawlines))
        {
          VikTrackpoint *tp2 = VIK_TRACKPOINT(list->prev->data);

          /* UTM only: zone check */
          if ( drawpoints && dp->vtl->coord_mode == VIK_COORD_UTM && tp->coord.utm_zone != dp->center->utm_zone )
            draw_utm_skip_insignia (  dp->vp, main_gc, x, y);

          if ( dp->vtl->drawmode == DRAWMODE_BY_VELOCITY ) {
            dp->track_gc_iter = calculate_velocity ( dp->vtl, tp, tp2 );
            main_gc = g_array_index(dp->vtl->track_gc, GdkGC *, dp->track_gc_iter);
          }

          if (!useoldvals)
            vik_viewport_coord_to_screen ( dp->vp, &(tp2->coord), &oldx, &oldy );

          if ( drawing_white_background ) {
            vik_viewport_draw_line ( dp->vp, dp->vtl->track_bg_gc, oldx, oldy, x, y);
          }
          else {

            vik_viewport_draw_line ( dp->vp, main_gc, oldx, oldy, x, y);
            if ( dp->vtl->drawelevation && list->next && VIK_TRACKPOINT(list->next->data)->altitude != VIK_DEFAULT_ALTITUDE ) {
              GdkPoint tmp[4];
              #define FIXALTITUDE(what) ((VIK_TRACKPOINT((what))->altitude-min_alt)/alt_diff*DRAW_ELEVATION_FACTOR*dp->vtl->elevation_factor/dp->xmpp)

	      tmp[0].x = oldx;
	      tmp[0].y = oldy;
	      tmp[1].x = oldx;
	      tmp[1].y = oldy-FIXALTITUDE(list->data);
	      tmp[2].x = x;
	      tmp[2].y = y-FIXALTITUDE(list->next->data);
	      tmp[3].x = x;
	      tmp[3].y = y;

	      GdkGC *tmp_gc;
	      if ( ((oldx - x) > 0 && (oldy - y) > 0) || ((oldx - x) < 0 && (oldy - y) < 0))
		tmp_gc = GTK_WIDGET(dp->vp)->style->light_gc[3];
	      else
		tmp_gc = GTK_WIDGET(dp->vp)->style->dark_gc[0];
	      vik_viewport_draw_polygon ( dp->vp, tmp_gc, TRUE, tmp, 4);

              vik_viewport_draw_line ( dp->vp, main_gc, oldx, oldy-FIXALTITUDE(list->data), x, y-FIXALTITUDE(list->next->data));
            }
          }
        }

      skip:
        oldx = x;
        oldy = y;
        useoldvals = TRUE;
      }
      else {
        if (useoldvals && dp->vtl->drawlines && (!tp->newsegment))
        {
          VikTrackpoint *tp2 = VIK_TRACKPOINT(list->prev->data);
          if ( dp->vtl->coord_mode != VIK_COORD_UTM || tp->coord.utm_zone == dp->center->utm_zone )
          {
            vik_viewport_coord_to_screen ( dp->vp, &(tp->coord), &x, &y );
            if ( dp->vtl->drawmode == DRAWMODE_BY_VELOCITY ) {
              dp->track_gc_iter = calculate_velocity ( dp->vtl, tp, tp2 );
              main_gc = g_array_index(dp->vtl->track_gc, GdkGC *, dp->track_gc_iter);
            }

	    /*
	     * If points are the same in display coordinates, don't draw.
	     */
	    if ( x != oldx || y != oldy )
	      {
		if ( drawing_white_background )
		  vik_viewport_draw_line ( dp->vp, dp->vtl->track_bg_gc, oldx, oldy, x, y);
		else
		  vik_viewport_draw_line ( dp->vp, main_gc, oldx, oldy, x, y);
	      }
          }
          else 
          {
	    /*
	     * If points are the same in display coordinates, don't draw.
	     */
	    if ( x != oldx && y != oldy )
	      {
		vik_viewport_coord_to_screen ( dp->vp, &(tp2->coord), &x, &y );
		draw_utm_skip_insignia ( dp->vp, main_gc, x, y );
	      }
          }
        }
        useoldvals = FALSE;
      }
    }
  }
  if ( dp->vtl->drawmode == DRAWMODE_BY_TRACK )
    if ( ++(dp->track_gc_iter) >= VIK_TRW_LAYER_TRACK_GC )
      dp->track_gc_iter = 0;
}

/* the only reason this exists is so that trw_layer_draw_track can first call itself to draw the white track background */
static void trw_layer_draw_track_cb ( const gchar *name, VikTrack *track, struct DrawingParams *dp )
{
  trw_layer_draw_track ( name, track, dp, FALSE );
}

static void cached_pixbuf_free ( CachedPixbuf *cp )
{
  g_object_unref ( G_OBJECT(cp->pixbuf) );
  g_free ( cp->image );
}

static gint cached_pixbuf_cmp ( CachedPixbuf *cp, const gchar *name )
{
  return strcmp ( cp->image, name );
}

static void trw_layer_draw_waypoint ( const gchar *name, VikWaypoint *wp, struct DrawingParams *dp )
{
  if ( wp->visible )
  if ( (!dp->one_zone && !dp->lat_lon) || ( ( dp->lat_lon || wp->coord.utm_zone == dp->center->utm_zone ) && 
             wp->coord.east_west < dp->ce2 && wp->coord.east_west > dp->ce1 && 
             wp->coord.north_south > dp->cn1 && wp->coord.north_south < dp->cn2 ) )
  {
    gint x, y;
    GdkPixbuf *sym = NULL;
    vik_viewport_coord_to_screen ( dp->vp, &(wp->coord), &x, &y );

    /* if in shrunken_cache, get that. If not, get and add to shrunken_cache */

    if ( wp->image && dp->vtl->drawimages )
    {
      GdkPixbuf *pixbuf = NULL;
      GList *l;

      if ( dp->vtl->image_alpha == 0)
        return;

      l = g_list_find_custom ( dp->vtl->image_cache->head, wp->image, (GCompareFunc) cached_pixbuf_cmp );
      if ( l )
        pixbuf = ((CachedPixbuf *) l->data)->pixbuf;
      else
      {
        gchar *image = wp->image;
        GdkPixbuf *regularthumb = a_thumbnails_get ( wp->image );
        if ( ! regularthumb )
        {
          regularthumb = a_thumbnails_get_default (); /* cache one 'not yet loaded' for all thumbs not loaded */
          image = "\x12\x00"; /* this shouldn't occur naturally. */
        }
        if ( regularthumb )
        {
          CachedPixbuf *cp = NULL;
          cp = g_malloc ( sizeof ( CachedPixbuf ) );
          if ( dp->vtl->image_size == 128 )
            cp->pixbuf = regularthumb;
          else
          {
            cp->pixbuf = a_thumbnails_scale_pixbuf(regularthumb, dp->vtl->image_size, dp->vtl->image_size);
            g_assert ( cp->pixbuf );
            g_object_unref ( G_OBJECT(regularthumb) );
          }
          cp->image = g_strdup ( image );

          /* needed so 'click picture' tool knows how big the pic is; we don't
           * store it in cp because they may have been freed already. */
          wp->image_width = gdk_pixbuf_get_width ( cp->pixbuf );
          wp->image_height = gdk_pixbuf_get_height ( cp->pixbuf );

          g_queue_push_head ( dp->vtl->image_cache, cp );
          if ( dp->vtl->image_cache->length > dp->vtl->image_cache_size )
            cached_pixbuf_free ( g_queue_pop_tail ( dp->vtl->image_cache ) );

          pixbuf = cp->pixbuf;
        }
        else
        {
          pixbuf = a_thumbnails_get_default (); /* thumbnail not yet loaded */
        }
      }
      if ( pixbuf )
      {
        gint w, h;
        w = gdk_pixbuf_get_width ( pixbuf );
        h = gdk_pixbuf_get_height ( pixbuf );

        if ( x+(w/2) > 0 && y+(h/2) > 0 && x-(w/2) < dp->width && y-(h/2) < dp->height ) /* always draw within boundaries */
        {
	  if ( vik_viewport_get_draw_highlight ( dp->vp ) ) {
	    if ( dp->vtl == vik_window_get_selected_trw_layer ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ||
		 dp->vtl->waypoints == vik_window_get_selected_waypoints ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ||
		 wp == vik_window_get_selected_waypoint ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ) {
	      // Highlighted - so draw a little border around the chosen one
	      // single line seems a little weak so draw 2 of them
	      vik_viewport_draw_rectangle (dp->vp, vik_viewport_get_gc_highlight (dp->vp), FALSE,
					   x - (w/2) - 1, y - (h/2) - 1, w + 2, h + 2 );
	      vik_viewport_draw_rectangle (dp->vp, vik_viewport_get_gc_highlight (dp->vp), FALSE,
					   x - (w/2) - 2, y - (h/2) - 2, w + 4, h + 4 );
	    }
	  }
          if ( dp->vtl->image_alpha == 255 )
            vik_viewport_draw_pixbuf ( dp->vp, pixbuf, 0, 0, x - (w/2), y - (h/2), w, h );
          else
            vik_viewport_draw_pixbuf_with_alpha ( dp->vp, pixbuf, dp->vtl->image_alpha, 0, 0, x - (w/2), y - (h/2), w, h );
        }
        return; /* if failed to draw picture, default to drawing regular waypoint (below) */
      }
    }

    /* DRAW ACTUAL DOT */
    if ( dp->vtl->wp_draw_symbols && wp->symbol && (sym = a_get_wp_sym(wp->symbol)) ) {
      vik_viewport_draw_pixbuf ( dp->vp, sym, 0, 0, x - gdk_pixbuf_get_width(sym)/2, y - gdk_pixbuf_get_height(sym)/2, -1, -1 );
    } 
    else if ( wp == dp->vtl->current_wp ) {
      switch ( dp->vtl->wp_symbol ) {
        case WP_SYMBOL_FILLED_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, TRUE, x - (dp->vtl->wp_size), y - (dp->vtl->wp_size), dp->vtl->wp_size*2, dp->vtl->wp_size*2 ); break;
        case WP_SYMBOL_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, FALSE, x - (dp->vtl->wp_size), y - (dp->vtl->wp_size), dp->vtl->wp_size*2, dp->vtl->wp_size*2 ); break;
        case WP_SYMBOL_CIRCLE: vik_viewport_draw_arc ( dp->vp, dp->vtl->waypoint_gc, TRUE, x - dp->vtl->wp_size, y - dp->vtl->wp_size, dp->vtl->wp_size, dp->vtl->wp_size, 0, 360*64 ); break;
        case WP_SYMBOL_X: vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x - dp->vtl->wp_size*2, y - dp->vtl->wp_size*2, x + dp->vtl->wp_size*2, y + dp->vtl->wp_size*2 );
                          vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x - dp->vtl->wp_size*2, y + dp->vtl->wp_size*2, x + dp->vtl->wp_size*2, y - dp->vtl->wp_size*2 );
      }
    }
    else {
      switch ( dp->vtl->wp_symbol ) {
        case WP_SYMBOL_FILLED_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, TRUE, x - dp->vtl->wp_size/2, y - dp->vtl->wp_size/2, dp->vtl->wp_size, dp->vtl->wp_size ); break;
        case WP_SYMBOL_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, FALSE, x - dp->vtl->wp_size/2, y - dp->vtl->wp_size/2, dp->vtl->wp_size, dp->vtl->wp_size ); break;
        case WP_SYMBOL_CIRCLE: vik_viewport_draw_arc ( dp->vp, dp->vtl->waypoint_gc, TRUE, x-dp->vtl->wp_size/2, y-dp->vtl->wp_size/2, dp->vtl->wp_size, dp->vtl->wp_size, 0, 360*64 ); break;
        case WP_SYMBOL_X: vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x-dp->vtl->wp_size, y-dp->vtl->wp_size, x+dp->vtl->wp_size, y+dp->vtl->wp_size );
                          vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x-dp->vtl->wp_size, y+dp->vtl->wp_size, x+dp->vtl->wp_size, y-dp->vtl->wp_size ); break;
      }
    }

    if ( dp->vtl->drawlabels )
    {
      /* thanks to the GPSDrive people (Fritz Ganter et al.) for hints on this part ... yah, I'm too lazy to study documentation */
      gint label_x, label_y;
      gint width, height;
      pango_layout_set_text ( dp->vtl->wplabellayout, name, -1 );
      pango_layout_get_pixel_size ( dp->vtl->wplabellayout, &width, &height );
      label_x = x - width/2;
      if (sym)
        label_y = y - height - 2 - gdk_pixbuf_get_height(sym)/2;
      else
        label_y = y - dp->vtl->wp_size - height - 2;

      /* if highlight mode on, then draw background text in highlight colour */
      if ( vik_viewport_get_draw_highlight ( dp->vp ) ) {
	if ( dp->vtl == vik_window_get_selected_trw_layer ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ||
	     dp->vtl->waypoints == vik_window_get_selected_waypoints ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) ||
	     wp == vik_window_get_selected_waypoint ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl) ) )
	  vik_viewport_draw_rectangle ( dp->vp, vik_viewport_get_gc_highlight (dp->vp), TRUE, label_x - 1, label_y-1,width+2,height+2);
	else
	  vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_bg_gc, TRUE, label_x - 1, label_y-1,width+2,height+2);
      }
      else {
	vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_bg_gc, TRUE, label_x - 1, label_y-1,width+2,height+2);
      }
      vik_viewport_draw_layout ( dp->vp, dp->vtl->waypoint_text_gc, label_x, label_y, dp->vtl->wplabellayout );
    }
  }
}

static void trw_layer_draw ( VikTrwLayer *l, gpointer data )
{
  static struct DrawingParams dp;
  g_assert ( l != NULL );

  init_drawing_params ( &dp, VIK_VIEWPORT(data) );
  dp.vtl = l;

  if ( l->tracks_visible )
    g_hash_table_foreach ( l->tracks, (GHFunc) trw_layer_draw_track_cb, &dp );

  if (l->waypoints_visible)
    g_hash_table_foreach ( l->waypoints, (GHFunc) trw_layer_draw_waypoint, &dp );
}

static void trw_layer_free_track_gcs ( VikTrwLayer *vtl )
{
  int i;
  if ( vtl->track_bg_gc ) 
  {
    g_object_unref ( vtl->track_bg_gc );
    vtl->track_bg_gc = NULL;
  }
  if ( vtl->current_track_gc ) 
  {
    g_object_unref ( vtl->current_track_gc );
    vtl->current_track_gc = NULL;
  }

  if ( ! vtl->track_gc )
    return;
  for ( i = vtl->track_gc->len - 1; i >= 0; i-- )
    g_object_unref ( g_array_index ( vtl->track_gc, GObject *, i ) );
  g_array_free ( vtl->track_gc, TRUE );
  vtl->track_gc = NULL;
}

static void trw_layer_new_track_gcs ( VikTrwLayer *vtl, VikViewport *vp )
{
  GdkGC *gc[ VIK_TRW_LAYER_TRACK_GC ];
  gint width = vtl->line_thickness;

  if ( vtl->track_gc )
    trw_layer_free_track_gcs ( vtl );

  if ( vtl->track_bg_gc )
    g_object_unref ( vtl->track_bg_gc );
  vtl->track_bg_gc = vik_viewport_new_gc ( vp, "#FFFFFF", width + vtl->bg_line_thickness );

  if ( vtl->current_track_gc )
    g_object_unref ( vtl->current_track_gc );
  vtl->current_track_gc = vik_viewport_new_gc ( vp, "#FF0000", 2 );
  gdk_gc_set_line_attributes ( vtl->current_track_gc, 2, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND );

  vtl->track_gc = g_array_sized_new ( FALSE, FALSE, sizeof ( GdkGC * ), VIK_TRW_LAYER_TRACK_GC );

  gc[0] = vik_viewport_new_gc ( vp, "#2d870a", width ); /* below range */

  gc[1] = vik_viewport_new_gc ( vp, "#0a8742", width );
  gc[2] = vik_viewport_new_gc ( vp, "#0a8783", width );
  gc[3] = vik_viewport_new_gc ( vp, "#0a4d87", width );
  gc[4] = vik_viewport_new_gc ( vp, "#05469f", width );
  gc[5] = vik_viewport_new_gc ( vp, "#1b059f", width );
  gc[6] = vik_viewport_new_gc ( vp, "#2d059f", width );
  gc[7] = vik_viewport_new_gc ( vp, "#4a059f", width );
  gc[8] = vik_viewport_new_gc ( vp, "#84059f", width );
  gc[9] = vik_viewport_new_gc ( vp, "#96059f", width );
  gc[10] = vik_viewport_new_gc ( vp, "#f22ef2", width );

  gc[11] = vik_viewport_new_gc ( vp, "#874200", width ); /* above range */

  gc[12] = vik_viewport_new_gc ( vp, "#000000", width ); /* black / no speed data */

  g_array_append_vals ( vtl->track_gc, gc, VIK_TRW_LAYER_TRACK_GC );
}

static VikTrwLayer* trw_layer_create ( VikViewport *vp )
{
  VikTrwLayer *rv = trw_layer_new ( DRAWMODE_BY_TRACK );
  vik_layer_rename ( VIK_LAYER(rv), vik_trw_layer_interface.name );

  if ( vp == NULL || GTK_WIDGET(vp)->window == NULL ) {
    /* early exit, as the rest is GUI related */
    return rv;
  }

  PangoFontDescription *pfd;
  rv->wplabellayout = gtk_widget_create_pango_layout (GTK_WIDGET(vp), NULL);
  pfd = pango_font_description_from_string (WAYPOINT_FONT);
  pango_layout_set_font_description (rv->wplabellayout, pfd);
  /* freeing PangoFontDescription, cause it has been copied by prev. call */
  pango_font_description_free (pfd);

  trw_layer_new_track_gcs ( rv, vp );

  rv->waypoint_gc = vik_viewport_new_gc ( vp, "#000000", 2 );
  rv->waypoint_text_gc = vik_viewport_new_gc ( vp, "#FFFFFF", 1 );
  rv->waypoint_bg_gc = vik_viewport_new_gc ( vp, "#8383C4", 1 );
  gdk_gc_set_function ( rv->waypoint_bg_gc, GDK_AND );

  rv->waypoint_font = gdk_font_load ( "-*-helvetica-bold-r-normal-*-*-100-*-*-p-*-iso8859-1" );

  rv->has_verified_thumbnails = FALSE;
  rv->wp_symbol = WP_SYMBOL_FILLED_SQUARE;
  rv->wp_size = 4;
  rv->wp_draw_symbols = TRUE;

  rv->coord_mode = vik_viewport_get_coord_mode ( vp );

  rv->menu_selection = vik_layer_get_interface(VIK_LAYER(rv)->type)->menu_items_selection;

  return rv;
}

static void trw_layer_realize_track ( gchar *name, VikTrack *track, gpointer pass_along[5] )
{
  GtkTreeIter *new_iter = g_malloc(sizeof(GtkTreeIter));

#ifdef VIK_CONFIG_ALPHABETIZED_TRW
  vik_treeview_add_sublayer_alphabetized ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], name, pass_along[2], name, GPOINTER_TO_INT (pass_along[4]), NULL, TRUE, TRUE );
#else
  vik_treeview_add_sublayer ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], name, pass_along[2], name, GPOINTER_TO_INT (pass_along[4]), NULL, TRUE, TRUE );
#endif

  *new_iter = *((GtkTreeIter *) pass_along[1]);
  g_hash_table_insert ( VIK_TRW_LAYER(pass_along[2])->tracks_iters, name, new_iter );

  if ( ! track->visible )
    vik_treeview_item_set_visible ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[1], FALSE );
}

static void trw_layer_realize_waypoint ( gchar *name, VikWaypoint *wp, gpointer pass_along[5] )
{
  GtkTreeIter *new_iter = g_malloc(sizeof(GtkTreeIter));
#ifdef VIK_CONFIG_ALPHABETIZED_TRW
  vik_treeview_add_sublayer_alphabetized ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], name, pass_along[2], name, GPOINTER_TO_INT (pass_along[4]), NULL, TRUE, TRUE );
#else
  vik_treeview_add_sublayer ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], name, pass_along[2], name, GPOINTER_TO_INT (pass_along[4]), NULL, TRUE, TRUE );
#endif

  *new_iter = *((GtkTreeIter *) pass_along[1]);
  g_hash_table_insert ( VIK_TRW_LAYER(pass_along[2])->waypoints_iters, name, new_iter );

  if ( ! wp->visible )
    vik_treeview_item_set_visible ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[1], FALSE );
}


static void trw_layer_realize ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  GtkTreeIter iter2;
  gpointer pass_along[5] = { &(vtl->tracks_iter), &iter2, vtl, vt, (gpointer) VIK_TRW_LAYER_SUBLAYER_TRACK };

#ifdef VIK_CONFIG_ALPHABETIZED_TRW
  vik_treeview_add_sublayer_alphabetized ( (VikTreeview *) vt, layer_iter, &(vtl->tracks_iter), _("Tracks"), vtl, NULL, VIK_TRW_LAYER_SUBLAYER_TRACKS, NULL, TRUE, FALSE );
#else
  vik_treeview_add_sublayer ( (VikTreeview *) vt, layer_iter, &(vtl->tracks_iter), _("Tracks"), vtl, NULL, VIK_TRW_LAYER_SUBLAYER_TRACKS, NULL, TRUE, FALSE );
#endif
  if ( ! vtl->tracks_visible )
    vik_treeview_item_set_visible ( (VikTreeview *) vt, &(vtl->tracks_iter), FALSE ); 

  g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_realize_track, pass_along );

#ifdef VIK_CONFIG_ALPHABETIZED_TRW
  vik_treeview_add_sublayer_alphabetized ( (VikTreeview *) vt, layer_iter, &(vtl->waypoints_iter), _("Waypoints"), vtl, NULL, VIK_TRW_LAYER_SUBLAYER_WAYPOINTS, NULL, TRUE, FALSE );
#else
  vik_treeview_add_sublayer ( (VikTreeview *) vt, layer_iter, &(vtl->waypoints_iter), _("Waypoints"), vtl, NULL, VIK_TRW_LAYER_SUBLAYER_WAYPOINTS, NULL, TRUE, FALSE );
#endif

  if ( ! vtl->waypoints_visible )
    vik_treeview_item_set_visible ( (VikTreeview *) vt, &(vtl->waypoints_iter), FALSE ); 

  pass_along[0] = &(vtl->waypoints_iter);
  pass_along[4] = (gpointer) VIK_TRW_LAYER_SUBLAYER_WAYPOINT;

  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_realize_waypoint, pass_along );

}

static gboolean trw_layer_sublayer_toggle_visible ( VikTrwLayer *l, gint subtype, gpointer sublayer )
{
  switch ( subtype )
  {
    case VIK_TRW_LAYER_SUBLAYER_TRACKS: return (l->tracks_visible ^= 1);
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS: return (l->waypoints_visible ^= 1);
    case VIK_TRW_LAYER_SUBLAYER_TRACK:
    {
      VikTrack *t = g_hash_table_lookup ( l->tracks, sublayer );
      if (t)
        return (t->visible ^= 1);
      else
        return TRUE;
    }
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
    {
      VikWaypoint *t = g_hash_table_lookup ( l->waypoints, sublayer );
      if (t)
        return (t->visible ^= 1);
      else
        return TRUE;
    }
  }
  return TRUE;
}

/*
 * Return a property about tracks for this layer
 */
gint vik_trw_layer_get_property_tracks_line_thickness ( VikTrwLayer *vtl )
{
  return vtl->line_thickness;
}

// Structure to hold multiple track information for a layer
typedef struct {
  gdouble length;
  time_t  start_time;
  time_t  end_time;
  gint    duration;
} tooltip_tracks;

/*
 * Build up layer multiple track information via updating the tooltip_tracks structure
 */
static void trw_layer_tracks_tooltip ( const gchar *name, VikTrack *tr, tooltip_tracks *tt )
{
  tt->length = tt->length + vik_track_get_length (tr);

  // Ensure times are available
  if ( tr->trackpoints &&
       VIK_TRACKPOINT(tr->trackpoints->data)->has_timestamp &&
       VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->has_timestamp ) {

    time_t t1, t2;
    t1 = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
    t2 = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;

    // Assume never actually have a track with a time of 0 (1st Jan 1970)
    // Hence initialize to the first 'proper' value
    if ( tt->start_time == 0 )
	tt->start_time = t1;
    if ( tt->end_time == 0 )
	tt->end_time = t2;

    // Update find the earliest / last times
    if ( t1 < tt->start_time )
	tt->start_time = t1;
    if ( t2 > tt->end_time )
	tt->end_time = t2;

    // Keep track of total time
    //  there maybe gaps within a track (eg segments)
    //  but this should be generally good enough for a simple indicator
    tt->duration = tt->duration + (int)(t2-t1);
  }
}

/*
 * Generate tooltip text for the layer.
 * This is relatively complicated as it considers information for
 *   no tracks, a single track or multiple tracks
 *     (which may or may not have timing information)
 */
static const gchar* trw_layer_layer_tooltip ( VikTrwLayer *vtl )
{
  gchar tbuf1[32];
  gchar tbuf2[64];
  gchar tbuf3[64];
  gchar tbuf4[10];
  tbuf1[0] = '\0';
  tbuf2[0] = '\0';
  tbuf3[0] = '\0';
  tbuf4[0] = '\0';

  static gchar tmp_buf[128];
  tmp_buf[0] = '\0';

  // For compact date format I'm using '%x'     [The preferred date representation for the current locale without the time.]

  // Safety check - I think these should always be valid
  if ( vtl->tracks && vtl->waypoints ) {
    tooltip_tracks tt = { 0.0, 0, 0 };
    g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_tracks_tooltip, &tt );

    GDate* gdate_start = g_date_new ();
    g_date_set_time_t (gdate_start, tt.start_time);

    GDate* gdate_end = g_date_new ();
    g_date_set_time_t (gdate_end, tt.end_time);

    if ( g_date_compare (gdate_start, gdate_end) ) {
      // Dates differ so print range on separate line
      g_date_strftime (tbuf1, sizeof(tbuf1), "%x", gdate_start);
      g_date_strftime (tbuf2, sizeof(tbuf2), "%x", gdate_end);
      g_snprintf (tbuf3, sizeof(tbuf3), "%s to %s\n", tbuf1, tbuf2);
    }
    else {
      // Same date so just show it and keep rest of text on the same line - provided it's a valid time!
      if ( tt.start_time != 0 )
	g_date_strftime (tbuf3, sizeof(tbuf3), "%x: ", gdate_start);
    }

    tbuf2[0] = '\0';
    if ( tt.length > 0.0 ) {
      gdouble len_in_units;

      // Setup info dependent on distance units
      if ( a_vik_get_units_distance() == VIK_UNITS_DISTANCE_MILES ) {
	g_snprintf (tbuf4, sizeof(tbuf4), "miles");
	len_in_units = VIK_METERS_TO_MILES(tt.length);
      }
      else {
	g_snprintf (tbuf4, sizeof(tbuf4), "kms");
	len_in_units = tt.length/1000.0;
      }

      // Timing information if available
      tbuf1[0] = '\0';
      if ( tt.duration > 0 ) {
	g_snprintf (tbuf1, sizeof(tbuf1),
		    _(" in %d:%02d hrs:mins"),
		    (int)round(tt.duration/3600), (int)round((tt.duration/60)%60));
      }
      g_snprintf (tbuf2, sizeof(tbuf2),
		  _("\n%sTotal Length %.1f %s%s"),
		  tbuf3, len_in_units, tbuf4, tbuf1);
    }

    // Put together all the elements to form compact tooltip text
    g_snprintf (tmp_buf, sizeof(tmp_buf),
		_("Tracks: %d - Waypoints: %d%s"),
		g_hash_table_size (vtl->tracks), g_hash_table_size (vtl->waypoints), tbuf2);

    g_date_free (gdate_start);
    g_date_free (gdate_end);

  }

  return tmp_buf;
}

static const gchar* trw_layer_sublayer_tooltip ( VikTrwLayer *l, gint subtype, gpointer sublayer )
{
  switch ( subtype )
  {
    case VIK_TRW_LAYER_SUBLAYER_TRACKS: return NULL;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS: return NULL;
    case VIK_TRW_LAYER_SUBLAYER_TRACK:
    {
      VikTrack *tr = g_hash_table_lookup ( l->tracks, sublayer );
      if ( tr ) {
	// Could be a better way of handling strings - but this works...
	gchar time_buf1[20];
	gchar time_buf2[20];
	time_buf1[0] = '\0';
	time_buf2[0] = '\0';
	static gchar tmp_buf[100];
	// Compact info: Short date eg (11/20/99), duration and length
	// Hopefully these are the things that are most useful and so promoted into the tooltip
	if ( tr->trackpoints && VIK_TRACKPOINT(tr->trackpoints->data)->has_timestamp ) {
	  // %x     The preferred date representation for the current locale without the time.
	  strftime (time_buf1, sizeof(time_buf1), "%x: ", gmtime(&(VIK_TRACKPOINT(tr->trackpoints->data)->timestamp)));
	  if ( VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->has_timestamp ) {
	    gint dur = ( (VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp) - (VIK_TRACKPOINT(tr->trackpoints->data)->timestamp) );
	    if ( dur > 0 )
	      g_snprintf ( time_buf2, sizeof(time_buf2), _("- %d:%02d hrs:mins"), (int)round(dur/3600), (int)round((dur/60)%60) );
	  }
	}
	// Get length and consider the appropriate distance units
	gdouble tr_len = vik_track_get_length(tr);
	vik_units_distance_t dist_units = a_vik_get_units_distance ();
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_KILOMETRES:
	  g_snprintf (tmp_buf, sizeof(tmp_buf), _("%s%.1f km %s"), time_buf1, tr_len/1000.0, time_buf2);
	  break;
	case VIK_UNITS_DISTANCE_MILES:
	  g_snprintf (tmp_buf, sizeof(tmp_buf), _("%s%.1f miles %s"), time_buf1, VIK_METERS_TO_MILES(tr_len), time_buf2);
	  break;
	default:
	  break;
	}
	return tmp_buf;
      }
    }
    break;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
    {
      VikWaypoint *w = g_hash_table_lookup ( l->waypoints, sublayer );
      // NB It's OK to return NULL
      return w->comment;
    }
    break;
    default: break;
  }
  return NULL;
}

/*
 * Function to show basic track point information on the statusbar
 */
static void set_statusbar_msg_info_trkpt ( VikTrwLayer *vtl, VikTrackpoint *trkpt )
{
  gchar tmp_buf1[64];
  switch (a_vik_get_units_height ()) {
  case VIK_UNITS_HEIGHT_FEET:
    g_snprintf(tmp_buf1, sizeof(tmp_buf1), _("Trkpt: Alt %dft"), (int)round(VIK_METERS_TO_FEET(trkpt->altitude)));
    break;
  default:
    //VIK_UNITS_HEIGHT_METRES:
    g_snprintf(tmp_buf1, sizeof(tmp_buf1), _("Trkpt: Alt %dm"), (int)round(trkpt->altitude));
  }
  
  gchar tmp_buf2[64];
  tmp_buf2[0] = '\0';
  if ( trkpt->has_timestamp ) {
    // Compact date time format
    strftime (tmp_buf2, sizeof(tmp_buf2), _(" | Time %x %X"), localtime(&(trkpt->timestamp)));
  }

  // Position part
  // Position is put later on, as this bit may not be seen if the display is not big enough,
  //   one can easily use the current pointer position to see this if needed
  gchar *lat = NULL, *lon = NULL;
  static struct LatLon ll;
  vik_coord_to_latlon (&(trkpt->coord), &ll);
  a_coords_latlon_to_string ( &ll, &lat, &lon );

  // Track name
  // Again is put later on, as this bit may not be seen if the display is not big enough
  //  trackname can be seen from the treeview (when enabled)
  // Also name could be very long to not leave room for anything else
  gchar tmp_buf3[64];
  tmp_buf3[0] = '\0';
  if ( vtl->current_tp_track_name ) {
    g_snprintf(tmp_buf3, sizeof(tmp_buf3),  _(" | Track: %s"), vtl->current_tp_track_name );
  }

  // Combine parts to make overall message
  gchar *msg = g_strdup_printf (_("%s%s | %s %s %s"), tmp_buf1, tmp_buf2, lat, lon, tmp_buf3);
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, msg );
  g_free ( lat );
  g_free ( lon );
  g_free ( msg );
}

/*
 * Function to show basic waypoint information on the statusbar
 */
static void set_statusbar_msg_info_wpt ( VikTrwLayer *vtl, VikWaypoint *wpt )
{
  gchar tmp_buf1[64];
  switch (a_vik_get_units_height ()) {
  case VIK_UNITS_HEIGHT_FEET:
    g_snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dft"), (int)round(VIK_METERS_TO_FEET(wpt->altitude)));
    break;
  default:
    //VIK_UNITS_HEIGHT_METRES:
    g_snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dm"), (int)round(wpt->altitude));
  }
  
  // Position part
  // Position is put last, as this bit is most likely not to be seen if the display is not big enough,
  //   one can easily use the current pointer position to see this if needed
  gchar *lat = NULL, *lon = NULL;
  static struct LatLon ll;
  vik_coord_to_latlon (&(wpt->coord), &ll);
  a_coords_latlon_to_string ( &ll, &lat, &lon );

  // Combine parts to make overall message
  gchar *msg;
  if ( wpt->comment )
    // Add comment if available
    msg = g_strdup_printf ( _("%s | %s %s | Comment: %s"), tmp_buf1, lat, lon, wpt->comment );
  else
    msg = g_strdup_printf ( _("%s | %s %s"), tmp_buf1, lat, lon );
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, msg );
  g_free ( lat );
  g_free ( lon );
  g_free ( msg );
}

/**
 * General layer selection function, find out which bit is selected and take appropriate action
 */
static gboolean trw_layer_selected ( VikTrwLayer *l, gint subtype, gpointer sublayer, gint type, gpointer vlp )
{
  // Reset
  l->current_wp      = NULL;
  l->current_wp_name = NULL;
  trw_layer_cancel_current_tp ( l, FALSE );

  // Clear statusbar
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l))), VIK_STATUSBAR_INFO, "" );

  switch ( type )
    {
    case VIK_TREEVIEW_TYPE_LAYER:
      {
	vik_window_set_selected_trw_layer ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), l );
	/* Mark for redraw */
	return TRUE;
      }
      break;

    case VIK_TREEVIEW_TYPE_SUBLAYER:
      {
	switch ( subtype )
	  {
	  case VIK_TRW_LAYER_SUBLAYER_TRACKS:
	    {
	      vik_window_set_selected_tracks ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), l->tracks, l );
	      /* Mark for redraw */
	      return TRUE;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_TRACK:
	    {
	      VikTrack *track = g_hash_table_lookup ( l->tracks, sublayer );
	      vik_window_set_selected_track ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), (gpointer)track, l, sublayer );
	      /* Mark for redraw */
	      return TRUE;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS:
	    {
	      vik_window_set_selected_waypoints ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), l->waypoints, l );
	      /* Mark for redraw */
	      return TRUE;
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
	    {
	      VikWaypoint *wpt = g_hash_table_lookup ( l->waypoints, sublayer );
	      vik_window_set_selected_waypoint ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l), (gpointer)wpt, l, sublayer );
	      // Show some waypoint info
	      set_statusbar_msg_info_wpt ( l, wpt );
	      /* Mark for redraw */
	      return TRUE;
	    }
	    break;
	  default:
	    {
	      return vik_window_clear_highlight ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l) );
	    }
	    break;
	  }
	return FALSE;
      }
      break;

    default:
      return vik_window_clear_highlight ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l) );
      break;
    }
}

GHashTable *vik_trw_layer_get_tracks ( VikTrwLayer *l )
{
  return l->tracks;
}

GHashTable *vik_trw_layer_get_waypoints ( VikTrwLayer *l )
{
  return l->waypoints;
}

static void trw_layer_find_maxmin_waypoints ( const gchar *name, const VikWaypoint *w, struct LatLon maxmin[2] )
{
  static VikCoord fixme;
  vik_coord_copy_convert ( &(w->coord), VIK_COORD_LATLON, &fixme );
  if ( VIK_LATLON(&fixme)->lat > maxmin[0].lat || maxmin[0].lat == 0.0 )
    maxmin[0].lat = VIK_LATLON(&fixme)->lat;
  if ( VIK_LATLON(&fixme)->lat < maxmin[1].lat || maxmin[1].lat == 0.0 )
    maxmin[1].lat = VIK_LATLON(&fixme)->lat;
  if ( VIK_LATLON(&fixme)->lon > maxmin[0].lon || maxmin[0].lon == 0.0 )
    maxmin[0].lon = VIK_LATLON(&fixme)->lon;
  if ( VIK_LATLON(&fixme)->lon < maxmin[1].lon || maxmin[1].lon == 0.0 )
    maxmin[1].lon = VIK_LATLON(&fixme)->lon;
}

static void trw_layer_find_maxmin_tracks ( const gchar *name, GList **t, struct LatLon maxmin[2] )
{
  GList *tr = *t;
  static VikCoord fixme;

  while ( tr )
  {
    vik_coord_copy_convert ( &(VIK_TRACKPOINT(tr->data)->coord), VIK_COORD_LATLON, &fixme );
    if ( VIK_LATLON(&fixme)->lat > maxmin[0].lat || maxmin[0].lat == 0.0 )
      maxmin[0].lat = VIK_LATLON(&fixme)->lat;
    if ( VIK_LATLON(&fixme)->lat < maxmin[1].lat || maxmin[1].lat == 0.0 )
      maxmin[1].lat = VIK_LATLON(&fixme)->lat;
    if ( VIK_LATLON(&fixme)->lon > maxmin[0].lon || maxmin[0].lon == 0.0 )
      maxmin[0].lon = VIK_LATLON(&fixme)->lon;
    if ( VIK_LATLON(&fixme)->lon < maxmin[1].lon || maxmin[1].lon == 0.0 )
      maxmin[1].lon = VIK_LATLON(&fixme)->lon;
    tr = tr->next;
  }
}

static void trw_layer_find_maxmin (VikTrwLayer *vtl, struct LatLon maxmin[2])
{
  struct LatLon wpt_maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
  struct LatLon trk_maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
  
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_find_maxmin_waypoints, wpt_maxmin );
  g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_find_maxmin_tracks, trk_maxmin );
  if ((wpt_maxmin[0].lat != 0.0 && wpt_maxmin[0].lat > trk_maxmin[0].lat) || trk_maxmin[0].lat == 0.0) {
    maxmin[0].lat = wpt_maxmin[0].lat;
  }
  else {
    maxmin[0].lat = trk_maxmin[0].lat;
  }
  if ((wpt_maxmin[0].lon != 0.0 && wpt_maxmin[0].lon > trk_maxmin[0].lon) || trk_maxmin[0].lon == 0.0) {
    maxmin[0].lon = wpt_maxmin[0].lon;
  }
  else {
    maxmin[0].lon = trk_maxmin[0].lon;
  }
  if ((wpt_maxmin[1].lat != 0.0 && wpt_maxmin[1].lat < trk_maxmin[1].lat) || trk_maxmin[1].lat == 0.0) {
    maxmin[1].lat = wpt_maxmin[1].lat;
  }
  else {
    maxmin[1].lat = trk_maxmin[1].lat;
  }
  if ((wpt_maxmin[1].lon != 0.0 && wpt_maxmin[1].lon < trk_maxmin[1].lon) || trk_maxmin[1].lon == 0.0) {
    maxmin[1].lon = wpt_maxmin[1].lon;
  }
  else {
    maxmin[1].lon = trk_maxmin[1].lon;
  }
}

gboolean vik_trw_layer_find_center ( VikTrwLayer *vtl, VikCoord *dest )
{
  /* TODO: what if there's only one waypoint @ 0,0, it will think nothing found. like I don't have more important things to worry about... */
  struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
  trw_layer_find_maxmin (vtl, maxmin);
  if (maxmin[0].lat == 0.0 && maxmin[0].lon == 0.0 && maxmin[1].lat == 0.0 && maxmin[1].lon == 0.0)
    return FALSE;
  else
  {
    struct LatLon average = { (maxmin[0].lat+maxmin[1].lat)/2, (maxmin[0].lon+maxmin[1].lon)/2 };
    vik_coord_load_from_latlon ( dest, vtl->coord_mode, &average );
    return TRUE;
  }
}

static void trw_layer_centerize ( gpointer layer_and_vlp[2] )
{
  VikCoord coord;
  if ( vik_trw_layer_find_center ( VIK_TRW_LAYER(layer_and_vlp[0]), &coord ) )
    goto_coord ( layer_and_vlp[1], NULL, NULL, &coord );
  else
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(layer_and_vlp[0]), _("This layer has no waypoints or trackpoints.") );
}

static void trw_layer_zoom_to_show_latlons ( VikTrwLayer *vtl, VikViewport *vvp, struct LatLon maxmin[2] )
{
  /* First set the center [in case previously viewing from elsewhere] */
  /* Then loop through zoom levels until provided positions are in view */
  /* This method is not particularly fast - but should work well enough */
  struct LatLon average = { (maxmin[0].lat+maxmin[1].lat)/2, (maxmin[0].lon+maxmin[1].lon)/2 };
  VikCoord coord;
  vik_coord_load_from_latlon ( &coord, vtl->coord_mode, &average );
  vik_viewport_set_center_coord ( vvp, &coord );

  /* Convert into definite 'smallest' and 'largest' positions */
  struct LatLon minmin;
  if ( maxmin[0].lat < maxmin[1].lat )
    minmin.lat = maxmin[0].lat;
  else
    minmin.lat = maxmin[1].lat;

  struct LatLon maxmax;
  if ( maxmin[0].lon > maxmin[1].lon )
    maxmax.lon = maxmin[0].lon;
  else
    maxmax.lon = maxmin[1].lon;

  /* Never zoom in too far - generally not that useful, as too close ! */
  /* Always recalculate the 'best' zoom level */
  gdouble zoom = 1.0;
  vik_viewport_set_zoom ( vvp, zoom );

  gdouble min_lat, max_lat, min_lon, max_lon;
  /* Should only be a maximum of about 18 iterations from min to max zoom levels */
  while ( zoom <= VIK_VIEWPORT_MAX_ZOOM ) {
    vik_viewport_get_min_max_lat_lon ( vvp, &min_lat, &max_lat, &min_lon, &max_lon );
    /* NB I think the logic used in this test to determine if the bounds is within view
       fails if track goes across 180 degrees longitude.
       Hopefully that situation is not too common...
       Mind you viking doesn't really do edge locations to well anyway */
    if ( min_lat < minmin.lat &&
	 max_lat > minmin.lat &&
	 min_lon < maxmax.lon &&
	 max_lon > maxmax.lon )
      /* Found within zoom level */
      break;

    /* Try next */
    zoom = zoom * 2;
    vik_viewport_set_zoom ( vvp, zoom );
  }
}

gboolean vik_trw_layer_auto_set_view ( VikTrwLayer *vtl, VikViewport *vvp )
{
  /* TODO: what if there's only one waypoint @ 0,0, it will think nothing found. */
  struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
  trw_layer_find_maxmin (vtl, maxmin);
  if (maxmin[0].lat == 0.0 && maxmin[0].lon == 0.0 && maxmin[1].lat == 0.0 && maxmin[1].lon == 0.0)
    return FALSE;
  else {
    trw_layer_zoom_to_show_latlons ( vtl, vvp, maxmin );
    return TRUE;
  }
}

static void trw_layer_auto_view ( gpointer layer_and_vlp[2] )
{
  if ( vik_trw_layer_auto_set_view ( VIK_TRW_LAYER(layer_and_vlp[0]), vik_layers_panel_get_viewport (VIK_LAYERS_PANEL(layer_and_vlp[1])) ) ) {
    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(layer_and_vlp[1]) );
  }
  else
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(layer_and_vlp[0]), _("This layer has no waypoints or trackpoints.") );
}

static void trw_layer_export ( gpointer layer_and_vlp[2], const gchar *title, const gchar* default_name, const gchar* trackname, guint file_type )
{
  GtkWidget *file_selector;
  const gchar *fn;
  gboolean failed = FALSE;
  file_selector = gtk_file_chooser_dialog_new (title,
					       NULL,
					       GTK_FILE_CHOOSER_ACTION_SAVE,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					       NULL);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(file_selector), default_name);

  while ( gtk_dialog_run ( GTK_DIALOG(file_selector) ) == GTK_RESPONSE_ACCEPT )
  {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(file_selector) );
    if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) == FALSE )
    {
      gtk_widget_hide ( file_selector );
      failed = ! a_file_export ( VIK_TRW_LAYER(layer_and_vlp[0]), fn, file_type, trackname);
      break;
    }
    else
    {
      if ( a_dialog_yes_or_no ( GTK_WINDOW(file_selector), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
      {
        gtk_widget_hide ( file_selector );
	failed = ! a_file_export ( VIK_TRW_LAYER(layer_and_vlp[0]), fn, file_type, trackname);
        break;
      }
    }
  }
  gtk_widget_destroy ( file_selector );
  if ( failed )
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(layer_and_vlp[0]), _("The filename you requested could not be opened for writing.") );
}

static void trw_layer_export_gpspoint ( gpointer layer_and_vlp[2] )
{
  trw_layer_export ( layer_and_vlp, _("Export Layer"), vik_layer_get_name(VIK_LAYER(layer_and_vlp[0])), NULL, FILE_TYPE_GPSPOINT );
}

static void trw_layer_export_gpsmapper ( gpointer layer_and_vlp[2] )
{
  trw_layer_export ( layer_and_vlp, _("Export Layer"), vik_layer_get_name(VIK_LAYER(layer_and_vlp[0])), NULL, FILE_TYPE_GPSMAPPER );
}

static void trw_layer_export_gpx ( gpointer layer_and_vlp[2] )
{
  /* Auto append '.gpx' to track name (providing it's not already there) for the default filename */
  gchar *auto_save_name = g_strdup ( vik_layer_get_name(VIK_LAYER(layer_and_vlp[0])) );
  if ( ! check_file_ext ( auto_save_name, ".gpx" ) )
    auto_save_name = g_strconcat ( auto_save_name, ".gpx", NULL );

  trw_layer_export ( layer_and_vlp, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPX );

  g_free ( auto_save_name );
}

static void trw_layer_export_kml ( gpointer layer_and_vlp[2] )
{
  /* Auto append '.kml' to the name (providing it's not already there) for the default filename */
  gchar *auto_save_name = g_strdup ( vik_layer_get_name(VIK_LAYER(layer_and_vlp[0])) );
  if ( ! check_file_ext ( auto_save_name, ".kml" ) )
    auto_save_name = g_strconcat ( auto_save_name, ".kml", NULL );

  trw_layer_export ( layer_and_vlp, _("Export Layer"), auto_save_name, NULL, FILE_TYPE_KML );

  g_free ( auto_save_name );
}

/**
 * Convert the given TRW layer into a temporary GPX file and open it with the specified program
 *
 */
static void trw_layer_export_external_gpx ( gpointer layer_and_vlp[2], const gchar* external_program )
{
  gchar *name_used = NULL;
  int fd;

  if ((fd = g_file_open_tmp("tmp-viking.XXXXXX.gpx", &name_used, NULL)) >= 0) {
    gboolean failed = ! a_file_export ( VIK_TRW_LAYER(layer_and_vlp[0]), name_used, FILE_TYPE_GPX, NULL);
    if (failed) {
      a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(layer_and_vlp[0]), _("Could not create temporary file for export.") );
    }
    else {
      GError *err = NULL;
      gchar *quoted_file = g_shell_quote ( name_used );
      gchar *cmd = g_strdup_printf ( "%s %s", external_program, quoted_file );
      g_free ( quoted_file );
      if ( ! g_spawn_command_line_async ( cmd, &err ) )
	{
	  a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER( layer_and_vlp[0]), _("Could not launch %s."), external_program );
	  g_error_free ( err );
	}
      g_free ( cmd );
    }
    // Note ATM the 'temporary' file is not deleted, as loading via another program is not instantaneous
    //g_remove ( name_used );
    // Perhaps should be deleted when the program ends?
    // For now leave it to the user to delete it / use system temp cleanup methods.
    g_free ( name_used );
  }
}

static void trw_layer_export_external_gpx_1 ( gpointer layer_and_vlp[2] )
{
  trw_layer_export_external_gpx ( layer_and_vlp, a_vik_get_external_gpx_program_1() );
}

static void trw_layer_export_external_gpx_2 ( gpointer layer_and_vlp[2] )
{
  trw_layer_export_external_gpx ( layer_and_vlp, a_vik_get_external_gpx_program_2() );
}

static void trw_layer_export_gpx_track ( gpointer pass_along[6] )
{
  gpointer layer_and_vlp[2];
  layer_and_vlp[0] = pass_along[0];
  layer_and_vlp[1] = pass_along[1];

  /* Auto append '.gpx' to track name (providing it's not already there) for the default filename */
  gchar *auto_save_name = g_strdup ( pass_along[3] );
  if ( ! check_file_ext ( auto_save_name, ".gpx" ) )
    auto_save_name = g_strconcat ( auto_save_name, ".gpx", NULL );

  trw_layer_export ( layer_and_vlp, _("Export Track as GPX"), auto_save_name, pass_along[3], FILE_TYPE_GPX );

  g_free ( auto_save_name );
}

static void trw_layer_goto_wp ( gpointer layer_and_vlp[2] )
{
  GHashTable *wps = vik_trw_layer_get_waypoints ( VIK_TRW_LAYER(layer_and_vlp[0]) );
  GtkWidget *dia = gtk_dialog_new_with_buttons (_("Find"),
                                                 VIK_GTK_WINDOW_FROM_LAYER(layer_and_vlp[0]),
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_STOCK_CANCEL,
                                                 GTK_RESPONSE_REJECT,
                                                 GTK_STOCK_OK,
                                                 GTK_RESPONSE_ACCEPT,
                                                 NULL);

  GtkWidget *label, *entry;
  label = gtk_label_new(_("Waypoint Name:"));
  entry = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox), entry, FALSE, FALSE, 0);
  gtk_widget_show_all ( label );
  gtk_widget_show_all ( entry );

  gtk_dialog_set_default_response ( GTK_DIALOG(dia), GTK_RESPONSE_ACCEPT );

  while ( gtk_dialog_run ( GTK_DIALOG(dia) ) == GTK_RESPONSE_ACCEPT )
  {
    VikWaypoint *wp;
    gchar *upname = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    int i;

    for ( i = strlen(upname)-1; i >= 0; i-- )
      upname[i] = toupper(upname[i]);

    wp = g_hash_table_lookup ( wps, upname );

    if (!wp)
      a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(layer_and_vlp[0]), _("Waypoint not found in this layer.") );
    else
    {
      vik_viewport_set_center_coord ( vik_layers_panel_get_viewport(VIK_LAYERS_PANEL(layer_and_vlp[1])), &(wp->coord) );
      vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(layer_and_vlp[1]) );
      vik_treeview_select_iter ( VIK_LAYER(layer_and_vlp[0])->vt, g_hash_table_lookup ( VIK_TRW_LAYER(layer_and_vlp[0])->waypoints_iters, upname ), TRUE );
      break;
    }

    g_free ( upname );

  }
  gtk_widget_destroy ( dia );
}

gboolean vik_trw_layer_new_waypoint ( VikTrwLayer *vtl, GtkWindow *w, const VikCoord *def_coord )
{
  gchar *default_name = highest_wp_number_get(vtl);
  VikWaypoint *wp = vik_waypoint_new();
  gchar *returned_name;
  gboolean updated;
  wp->coord = *def_coord;
  
  // Attempt to auto set height if DEM data is available
  gint16 elev = a_dems_get_elev_by_coord ( &(wp->coord), VIK_DEM_INTERPOL_BEST );
  if ( elev != VIK_DEM_INVALID_ELEVATION )
    wp->altitude = (gdouble)elev;

  if ( ( returned_name = a_dialog_waypoint ( w, default_name, wp, vik_trw_layer_get_waypoints ( vtl ), vtl->coord_mode, TRUE, &updated ) ) )
  {
    wp->visible = TRUE;
    vik_trw_layer_add_waypoint ( vtl, returned_name, wp );
    g_free (default_name);
    return TRUE;
  }
  g_free (default_name);
  vik_waypoint_free(wp);
  return FALSE;
}

static void trw_layer_new_wikipedia_wp_viewport ( gpointer lav[2] )
{
  VikCoord one, two;
  struct LatLon one_ll, two_ll;
  struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);
  vik_viewport_screen_to_coord ( vvp, 0, 0, &one);
  vik_viewport_screen_to_coord ( vvp, vik_viewport_get_width(vvp), vik_viewport_get_height(vvp), &two);
  vik_coord_to_latlon(&one, &one_ll);
  vik_coord_to_latlon(&two, &two_ll);
  if (one_ll.lat > two_ll.lat) {
    maxmin[0].lat = one_ll.lat;
    maxmin[1].lat = two_ll.lat;
  }
  else {
    maxmin[0].lat = two_ll.lat;
    maxmin[1].lat = one_ll.lat;
  }
  if (one_ll.lon > two_ll.lon) {
    maxmin[0].lon = one_ll.lon;
    maxmin[1].lon = two_ll.lon;
  }
  else {
    maxmin[0].lon = two_ll.lon;
    maxmin[1].lon = one_ll.lon;
  }
  a_geonames_wikipedia_box((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vtl, vlp, maxmin);
}

static void trw_layer_new_wikipedia_wp_layer ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
  
  trw_layer_find_maxmin (vtl, maxmin);
  a_geonames_wikipedia_box((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vtl, vlp, maxmin);
}

#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_geotagging_waypoint_mtime_keep ( gpointer pass_along[6] )
{
  VikWaypoint *wp = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->waypoints, pass_along[3] );
  if ( wp )
    // Update directly - not changing the mtime
    a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, TRUE );
}

static void trw_layer_geotagging_waypoint_mtime_update ( gpointer pass_along[6] )
{
  VikWaypoint *wp = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->waypoints, pass_along[3] );
  if ( wp )
    // Update directly
    a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, FALSE );
}

/*
 * Use code in separate file for this feature as reasonably complex
 */
static void trw_layer_geotagging_track ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(pass_along[0]);
  VikTrack *track = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );
  // Unset so can be reverified later if necessary
  vtl->has_verified_thumbnails = FALSE;

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    vtl,
			    track,
			    pass_along[3] );
}

static void trw_layer_geotagging ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  // Unset so can be reverified later if necessary
  vtl->has_verified_thumbnails = FALSE;

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    vtl,
			    NULL,
			    NULL);
}
#endif

// 'Acquires' - Same as in File Menu -> Acquire - applies into the selected TRW Layer //

/*
 * Acquire into this TRW Layer straight from GPS Device
 */
static void trw_layer_acquire_gps_cb ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  vik_datasource_gps_interface.mode = VIK_DATASOURCE_ADDTOLAYER;
  a_acquire ( vw, vlp, vvp, &vik_datasource_gps_interface );
}

/*
 * Acquire into this TRW Layer from Google Directions
 */
static void trw_layer_acquire_google_cb ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  a_acquire ( vw, vlp, vvp, &vik_datasource_google_interface );
}

#ifdef VIK_CONFIG_OPENSTREETMAP
/*
 * Acquire into this TRW Layer from OSM
 */
static void trw_layer_acquire_osm_cb ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  a_acquire ( vw, vlp, vvp, &vik_datasource_osm_interface );
}
#endif

#ifdef VIK_CONFIG_GEOCACHES
/*
 * Acquire into this TRW Layer from Geocaching.com
 */
static void trw_layer_acquire_geocache_cb ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  a_acquire ( vw, vlp, vvp, &vik_datasource_gc_interface );
}
#endif

#ifdef VIK_CONFIG_GEOTAG
/*
 * Acquire into this TRW Layer from images
 */
static void trw_layer_acquire_geotagged_cb ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  vik_datasource_geotag_interface.mode = VIK_DATASOURCE_ADDTOLAYER;
  a_acquire ( vw, vlp, vvp, &vik_datasource_geotag_interface );

  // Reverify thumbnails as they may have changed
  vtl->has_verified_thumbnails = FALSE;
  trw_layer_verify_thumbnails ( vtl, NULL );
}
#endif

static void trw_layer_new_wp ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);
  /* TODO longone: okay, if layer above (aggregate) is invisible but vtl->visible is true, this redraws for no reason.
     instead return true if you want to update. */
  if ( vik_trw_layer_new_waypoint ( vtl, VIK_GTK_WINDOW_FROM_LAYER(vtl), vik_viewport_get_center(vik_layers_panel_get_viewport(vlp))) && VIK_LAYER(vtl)->visible )
    vik_layers_panel_emit_update ( vlp );
}

static void trw_layer_auto_tracks_view ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);

  if ( g_hash_table_size (vtl->tracks) > 0 ) {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_find_maxmin_tracks, maxmin );
    trw_layer_zoom_to_show_latlons ( vtl, vik_layers_panel_get_viewport (vlp), maxmin );
    vik_layers_panel_emit_update ( vlp );
  }
}

static void trw_layer_single_waypoint_jump ( const gchar *name, const VikWaypoint *wp, gpointer vvp )
{
  /* NB do not care if wp is visible or not */
  vik_viewport_set_center_coord ( VIK_VIEWPORT(vvp), &(wp->coord) );
}

static void trw_layer_auto_waypoints_view ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(lav[1]);

  /* Only 1 waypoint - jump straight to it */
  if ( g_hash_table_size (vtl->waypoints) == 1 ) {
    VikViewport *vvp = vik_layers_panel_get_viewport (vlp);
    g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_single_waypoint_jump, (gpointer) vvp );
  }
  /* If at least 2 waypoints - find center and then zoom to fit */
  else if ( g_hash_table_size (vtl->waypoints) > 1 )
  {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_find_maxmin_waypoints, maxmin );
    trw_layer_zoom_to_show_latlons ( vtl, vik_layers_panel_get_viewport (vlp), maxmin );
  }

  vik_layers_panel_emit_update ( vlp );
}

static void trw_layer_add_menu_items ( VikTrwLayer *vtl, GtkMenu *menu, gpointer vlp )
{
  static gpointer pass_along[2];
  GtkWidget *item;
  GtkWidget *export_submenu;
  pass_along[0] = vtl;
  pass_along[1] = vlp;

  item = gtk_menu_item_new();
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );

  /* Now with icons */
  item = gtk_image_menu_item_new_with_mnemonic ( _("_View Layer") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_view), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("View All Trac_ks") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_tracks_view), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("V_iew All Waypoints") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_waypoints_view), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto Center of Layer") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_centerize), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Goto _Waypoint...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_wp), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  export_submenu = gtk_menu_new ();
  item = gtk_image_menu_item_new_with_mnemonic ( _("_Export Layer") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), export_submenu );
  
  item = gtk_menu_item_new_with_mnemonic ( _("Export as GPS_Point...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpspoint), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Export as GPS_Mapper...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpsmapper), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Export as _GPX...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpx), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("Export as _KML...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_kml), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  gchar* external1 = g_strconcat ( _("Open with External Program_1: "), a_vik_get_external_gpx_program_1(), NULL );
  item = gtk_menu_item_new_with_mnemonic ( external1 );
  g_free ( external1 );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_external_gpx_1), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  gchar* external2 = g_strconcat ( _("Open with External Program_2: "), a_vik_get_external_gpx_program_2(), NULL );
  item = gtk_menu_item_new_with_mnemonic ( external2 );
  g_free ( external2 );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_external_gpx_2), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_New Waypoint...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wp), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

#ifdef VIK_CONFIG_GEONAMES
  GtkWidget *wikipedia_submenu = gtk_menu_new();
  item = gtk_image_menu_item_new_with_mnemonic ( _("_Add Wikipedia Waypoints") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
  gtk_widget_show(item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), wikipedia_submenu);

  item = gtk_image_menu_item_new_with_mnemonic ( _("Within _Layer Bounds") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wikipedia_wp_layer), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (wikipedia_submenu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Within _Current View") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_100, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wikipedia_wp_viewport), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (wikipedia_submenu), item);
  gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_GEOTAG
  item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
#endif

  GtkWidget *acquire_submenu = gtk_menu_new ();
  item = gtk_image_menu_item_new_with_mnemonic ( _("Ac_quire") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), acquire_submenu );
  
  item = gtk_menu_item_new_with_mnemonic ( _("From _GPS...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_gps_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_mnemonic ( _("From G_oogle Directions...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_google_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );

#ifdef VIK_CONFIG_OPENSTREETMAP
  item = gtk_menu_item_new_with_mnemonic ( _("From _OSM Traces...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_osm_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_GEOCACHES
  item = gtk_menu_item_new_with_mnemonic ( _("From Geo_caching...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_geocache_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_GEOTAG
  item = gtk_menu_item_new_with_mnemonic ( _("From Geotagged _Images...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_geotagged_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
  gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_OPENSTREETMAP 
  item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _OSM...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(osm_traces_upload_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
#endif

  GtkWidget *delete_submenu = gtk_menu_new ();
  item = gtk_image_menu_item_new_with_mnemonic ( _("De_lete") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), delete_submenu );
  
  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete All _Tracks") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_tracks), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );
  
  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Tracks _From Selection...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_tracks_from_selection), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );
  
  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete All _Waypoints") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_waypoints), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );
  
  item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Waypoints From _Selection...") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_waypoints_from_selection), pass_along );
  gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
  gtk_widget_show ( item );
  
  item = a_acquire_trwlayer_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vlp,
				   vik_layers_panel_get_viewport(VIK_LAYERS_PANEL(vlp)), vtl );
  if ( item ) {
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }  

  item = a_acquire_trwlayer_track_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vlp,
					 vik_layers_panel_get_viewport(VIK_LAYERS_PANEL(vlp)), vtl );
  if ( item ) {
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }  
}

void vik_trw_layer_add_waypoint ( VikTrwLayer *vtl, gchar *name, VikWaypoint *wp )
{
  if ( VIK_LAYER(vtl)->realized )
  {
    VikWaypoint *oldwp = VIK_WAYPOINT ( g_hash_table_lookup ( vtl->waypoints, name ) );
    if ( oldwp )
      wp->visible = oldwp->visible; /* same visibility so we don't have to update viktreeview */
    else
    {
      GtkTreeIter *iter = g_malloc(sizeof(GtkTreeIter));
      // Visibility column always needed for waypoints
#ifdef VIK_CONFIG_ALPHABETIZED_TRW
      vik_treeview_add_sublayer_alphabetized ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), iter, name, vtl, name, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, NULL, TRUE, TRUE );
#else
      vik_treeview_add_sublayer ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), iter, name, vtl, name, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, NULL, TRUE, TRUE );
#endif
      // Actual setting of visibility dependent on the waypoint
      vik_treeview_item_set_visible ( VIK_LAYER(vtl)->vt, iter, wp->visible );
      g_hash_table_insert ( vtl->waypoints_iters, name, iter );
    }
  }

  highest_wp_number_add_wp(vtl, name);
  g_hash_table_insert ( vtl->waypoints, name, wp );
 
}

void vik_trw_layer_add_track ( VikTrwLayer *vtl, gchar *name, VikTrack *t )
{
  if ( VIK_LAYER(vtl)->realized )
  {
    VikTrack *oldt = VIK_TRACK ( g_hash_table_lookup ( vtl->tracks, name ) );
    if ( oldt )
      t->visible = oldt->visible; /* same visibility so we don't have to update viktreeview */
    else
    {
      GtkTreeIter *iter = g_malloc(sizeof(GtkTreeIter));
      // Visibility column always needed for tracks
#ifdef VIK_CONFIG_ALPHABETIZED_TRW
      vik_treeview_add_sublayer_alphabetized ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter), iter, name, vtl, name, VIK_TRW_LAYER_SUBLAYER_TRACK, NULL, TRUE, TRUE );
#else
      vik_treeview_add_sublayer ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter), iter, name, vtl, name, VIK_TRW_LAYER_SUBLAYER_TRACK, NULL, TRUE, TRUE );
#endif
      // Actual setting of visibility dependent on the track
      vik_treeview_item_set_visible ( VIK_LAYER(vtl)->vt, iter, t->visible );
      g_hash_table_insert ( vtl->tracks_iters, name, iter );
    }
  }

  g_hash_table_insert ( vtl->tracks, name, t );
 
}

/* to be called whenever a track has been deleted or may have been changed. */
void trw_layer_cancel_tps_of_track ( VikTrwLayer *vtl, const gchar *trk_name )
{
  if (vtl->current_tp_track_name && g_strcasecmp(trk_name, vtl->current_tp_track_name) == 0)
    trw_layer_cancel_current_tp ( vtl, FALSE );
  else if (vtl->last_tp_track_name && g_strcasecmp(trk_name, vtl->last_tp_track_name) == 0)
    trw_layer_cancel_last_tp ( vtl );
}
	
static gchar *get_new_unique_sublayer_name (VikTrwLayer *vtl, gint sublayer_type, const gchar *name)
{
 gint i = 2;
 gchar *newname = g_strdup(name);
 while ((sublayer_type == VIK_TRW_LAYER_SUBLAYER_TRACK) ?
         (void *)vik_trw_layer_get_track(vtl, newname) : (void *)vik_trw_layer_get_waypoint(vtl, newname)) {
    gchar *new_newname = g_strdup_printf("%s#%d", name, i);
    g_free(newname);
    newname = new_newname;
    i++;
  }
  return newname;
}

void vik_trw_layer_filein_add_waypoint ( VikTrwLayer *vtl, gchar *name, VikWaypoint *wp )
{
  vik_trw_layer_add_waypoint ( vtl,
                        get_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, name),
                        wp );
}
void vik_trw_layer_filein_add_track ( VikTrwLayer *vtl, gchar *name, VikTrack *tr )
{
  if ( vtl->route_finder_append && vtl->route_finder_current_track ) {
    vik_track_remove_dup_points ( tr ); /* make "double point" track work to undo */
    vik_track_steal_and_append_trackpoints ( vtl->route_finder_current_track, tr );
    vik_track_free ( tr );
    vtl->route_finder_append = FALSE; /* this means we have added it */
  } else {
    gchar *new_name = get_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, name);
    vik_trw_layer_add_track ( vtl, new_name, tr );

    if ( vtl->route_finder_check_added_track ) {
      vik_track_remove_dup_points ( tr ); /* make "double point" track work to undo */
      if ( vtl->route_finder_added_track_name ) /* for google routes */
        g_free ( vtl->route_finder_added_track_name );
      vtl->route_finder_added_track_name = g_strdup(new_name);
    }
  }
}

static void trw_layer_enum_item ( const gchar *name, GList **tr, GList **l )
{
  *l = g_list_append(*l, (gpointer)name);
}

static void trw_layer_move_item ( VikTrwLayer *vtl_src, VikTrwLayer *vtl_dest, gchar *name, gint type )
{
  gchar *newname = get_new_unique_sublayer_name(vtl_dest, type, name);
  if (type == VIK_TRW_LAYER_SUBLAYER_TRACK) {
    VikTrack *t;
    t = vik_track_copy(vik_trw_layer_get_track(vtl_src, name));
    vik_trw_layer_delete_track(vtl_src, name);
    vik_trw_layer_add_track(vtl_dest, newname, t);
  }
  if (type==VIK_TRW_LAYER_SUBLAYER_WAYPOINT) {
    VikWaypoint *w;
    w = vik_waypoint_copy(vik_trw_layer_get_waypoint(vtl_src, name));
    vik_trw_layer_delete_waypoint(vtl_src, name);
    vik_trw_layer_add_waypoint(vtl_dest, newname, w);
  }
}

static void trw_layer_drag_drop_request ( VikTrwLayer *vtl_src, VikTrwLayer *vtl_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path )
{
  VikTreeview *vt = VIK_LAYER(vtl_src)->vt;
  gint type = vik_treeview_item_get_data(vt, src_item_iter);

  if (!vik_treeview_item_get_pointer(vt, src_item_iter)) {
    GList *items = NULL;
    GList *iter;

    if (type==VIK_TRW_LAYER_SUBLAYER_TRACKS) {
      g_hash_table_foreach ( vtl_src->tracks, (GHFunc)trw_layer_enum_item, &items);
    } 
    if (type==VIK_TRW_LAYER_SUBLAYER_WAYPOINTS) {
      g_hash_table_foreach ( vtl_src->waypoints, (GHFunc)trw_layer_enum_item, &items);
    }    
      
    iter = items;
    while (iter) {
      if (type==VIK_TRW_LAYER_SUBLAYER_TRACKS) {
	trw_layer_move_item ( vtl_src, vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_TRACK);
      } else {
	trw_layer_move_item ( vtl_src, vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_WAYPOINT);
      }
      iter = iter->next;
    }
    if (items) 
      g_list_free(items);
  } else {
    gchar *name = vik_treeview_item_get_pointer(vt, src_item_iter);
    trw_layer_move_item(vtl_src, vtl_dest, name, type);
  }
}

gboolean vik_trw_layer_delete_track ( VikTrwLayer *vtl, const gchar *trk_name )
{
  VikTrack *t = g_hash_table_lookup ( vtl->tracks, trk_name );
  gboolean was_visible = FALSE;
  if ( t )
  {
    GtkTreeIter *it;
    was_visible = t->visible;
    if ( t == vtl->current_track ) {
      vtl->current_track = NULL;
    }
    if ( t == vtl->route_finder_current_track )
      vtl->route_finder_current_track = NULL;

    /* could be current_tp, so we have to check */
    trw_layer_cancel_tps_of_track ( vtl, trk_name );

    g_assert ( ( it = g_hash_table_lookup ( vtl->tracks_iters, trk_name ) ) );
    vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, it );
    g_hash_table_remove ( vtl->tracks_iters, trk_name );

    /* do this last because trk_name may be pointing to actual orig key */
    g_hash_table_remove ( vtl->tracks, trk_name );
  }
  return was_visible;
}

gboolean vik_trw_layer_delete_waypoint ( VikTrwLayer *vtl, const gchar *wp_name )
{
  gboolean was_visible = FALSE;
  VikWaypoint *wp;

  wp = g_hash_table_lookup ( vtl->waypoints, wp_name );
  if ( wp ) {
    GtkTreeIter *it;

    if ( wp == vtl->current_wp ) {
      vtl->current_wp = NULL;
      vtl->current_wp_name = NULL;
      vtl->moving_wp = FALSE;
    }

    was_visible = wp->visible;
    g_assert ( ( it = g_hash_table_lookup ( vtl->waypoints_iters, (gchar *) wp_name ) ) );
    vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, it );
    g_hash_table_remove ( vtl->waypoints_iters, (gchar *) wp_name );

    highest_wp_number_remove_wp(vtl, wp_name);
    g_hash_table_remove ( vtl->waypoints, wp_name ); /* last because this frees name */
  }

  return was_visible;
}

static void remove_item_from_treeview(const gchar *name, GtkTreeIter *it, VikTreeview * vt)
{
    vik_treeview_item_delete (vt, it );
}

void vik_trw_layer_delete_all_tracks ( VikTrwLayer *vtl )
{

  vtl->current_track = NULL;
  vtl->route_finder_current_track = NULL;
  if (vtl->current_tp_track_name)
    trw_layer_cancel_current_tp(vtl, FALSE);
  if (vtl->last_tp_track_name)
    trw_layer_cancel_last_tp ( vtl );

  g_hash_table_foreach(vtl->tracks_iters, (GHFunc) remove_item_from_treeview, VIK_LAYER(vtl)->vt);
  g_hash_table_remove_all(vtl->tracks_iters);
  g_hash_table_remove_all(vtl->tracks);

  vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
}

void vik_trw_layer_delete_all_waypoints ( VikTrwLayer *vtl )
{
  vtl->current_wp = NULL;
  vtl->current_wp_name = NULL;
  vtl->moving_wp = FALSE;

  highest_wp_number_reset(vtl);

  g_hash_table_foreach(vtl->waypoints_iters, (GHFunc) remove_item_from_treeview, VIK_LAYER(vtl)->vt);
  g_hash_table_remove_all(vtl->waypoints_iters);
  g_hash_table_remove_all(vtl->waypoints);

  vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
}

static void trw_layer_delete_all_tracks ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    _("Are you sure you want to delete all tracks in %s?"),
			    vik_layer_get_name ( VIK_LAYER(vtl) ) ) )
    vik_trw_layer_delete_all_tracks (vtl);
}

static void trw_layer_delete_all_waypoints ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    _("Are you sure you want to delete all waypoints in %s?"),
			    vik_layer_get_name ( VIK_LAYER(vtl) ) ) )
    vik_trw_layer_delete_all_waypoints (vtl);
}

static void trw_layer_delete_item ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(pass_along[0]);
  gboolean was_visible = FALSE;
  if ( GPOINTER_TO_INT (pass_along[2]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    if ( GPOINTER_TO_INT ( pass_along[4]) )
      // Get confirmation from the user
      // Maybe this Waypoint Delete should be optional as is it could get annoying...
      if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
				  _("Are you sure you want to delete the waypoint \"%s\""),
				  pass_along[3] ) )
	return;
    was_visible = vik_trw_layer_delete_waypoint ( vtl, (gchar *) pass_along[3] );
  }
  else
  {
    if ( GPOINTER_TO_INT ( pass_along[4]) )
      // Get confirmation from the user
      if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
				  _("Are you sure you want to delete the track \"%s\""),
				  pass_along[3] ) )
	return;
    was_visible = vik_trw_layer_delete_track ( vtl, (gchar *) pass_along[3] );
  }
  if ( was_visible )
    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
}


static void trw_layer_properties_item ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(pass_along[0]);
  if ( GPOINTER_TO_INT (pass_along[2]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    VikWaypoint *wp = g_hash_table_lookup ( vtl->waypoints, pass_along[3] );
    if ( wp )
    {
      gboolean updated = FALSE;
      a_dialog_waypoint ( VIK_GTK_WINDOW_FROM_LAYER(vtl), pass_along[3], wp, NULL, vtl->coord_mode, FALSE, &updated );

      if ( updated && VIK_LAYER(vtl)->visible )
	vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    }
  }
  else
  {
    VikTrack *tr = g_hash_table_lookup ( vtl->tracks, pass_along[3] );
    if ( tr )
    {
      vik_trw_layer_propwin_run ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
				  vtl, tr,
				  pass_along[1], /* vlp */
				  pass_along[3],  /* track name */
				  pass_along[5] );  /* vvp */
    }
  }
}

/*
   Parameter 1 -> VikLayersPanel
   Parameter 2 -> VikLayer
   Parameter 3 -> VikViewport
*/
static void goto_coord ( gpointer *vlp, gpointer vl, gpointer vvp, const VikCoord *coord )
{
  if ( vlp ) {
    vik_viewport_set_center_coord ( vik_layers_panel_get_viewport (VIK_LAYERS_PANEL(vlp)), coord );
    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp) );
  }
  else {
    /* since vlp not set, vl & vvp should be valid instead! */
    if ( vl && vvp ) {
      vik_viewport_set_center_coord ( VIK_VIEWPORT(vvp), coord );
      vik_layer_emit_update ( VIK_LAYER(vl), FALSE );
    }
  }
}

static void trw_layer_goto_track_startpoint ( gpointer pass_along[6] )
{
  GList *trps = ((VikTrack *) g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] ))->trackpoints;
  if ( trps && trps->data )
    goto_coord ( pass_along[1], pass_along[0], pass_along[5], &(((VikTrackpoint *) trps->data)->coord));
}

static void trw_layer_goto_track_center ( gpointer pass_along[6] )
{
  /* FIXME: get this into viktrack.c, and should be ->trackpoints right? */
  GList **trps = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );
  if ( trps && *trps )
  {
    struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
    VikCoord coord;
    trw_layer_find_maxmin_tracks ( NULL, trps, maxmin );
    average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
    average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
    vik_coord_load_from_latlon ( &coord, VIK_TRW_LAYER(pass_along[0])->coord_mode, &average );
    goto_coord ( pass_along[1], pass_along[0], pass_along[5], &coord);
  }
}

static void trw_layer_extend_track_end ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(pass_along[0]);
  VikTrack *track = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );

  vtl->current_track = track;
  vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_CREATE_TRACK);

  if ( track->trackpoints )
    goto_coord ( pass_along[1], pass_along[0], pass_along[5], &(((VikTrackpoint *)g_list_last(track->trackpoints)->data)->coord) );
}

/**
 * extend a track using route finder
 */
static void trw_layer_extend_track_end_route_finder ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(pass_along[0]);
  VikTrack *track = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );
  VikCoord last_coord = (((VikTrackpoint *)g_list_last(track->trackpoints)->data)->coord);

  vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, NUM_TOOLS );
  vtl->route_finder_coord =  last_coord;
  vtl->route_finder_current_track = track;
  vtl->route_finder_started = TRUE;

  if ( track->trackpoints )
    goto_coord ( pass_along[1], pass_along[0], pass_along[5], &last_coord) ;

}

static void trw_layer_apply_dem_data ( gpointer pass_along[6] )
{
  /* TODO: check & warn if no DEM data, or no applicable DEM data. */
  /* Also warn if overwrite old elevation data */
  VikTrack *track = (VikTrack *) g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );

  vik_track_apply_dem_data ( track );
}

static void trw_layer_goto_track_endpoint ( gpointer pass_along[6] )
{
  GList *trps = ((VikTrack *) g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] ))->trackpoints;
  if ( !trps )
    return;
  trps = g_list_last(trps);
  goto_coord ( pass_along[1], pass_along[0], pass_along[5], &(((VikTrackpoint *) trps->data)->coord));
}

static void trw_layer_goto_track_max_speed ( gpointer pass_along[6] )
{
  VikTrackpoint* vtp = vik_track_get_tp_by_max_speed ( g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] ) );
  if ( !vtp )
    return;
  goto_coord ( pass_along[1], pass_along[0], pass_along[5], &(vtp->coord));
}

static void trw_layer_goto_track_max_alt ( gpointer pass_along[6] )
{
  VikTrackpoint* vtp = vik_track_get_tp_by_max_alt ( g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] ) );
  if ( !vtp )
    return;
  goto_coord ( pass_along[1], pass_along[0], pass_along[5], &(vtp->coord));
}

static void trw_layer_goto_track_min_alt ( gpointer pass_along[6] )
{
  VikTrackpoint* vtp = vik_track_get_tp_by_min_alt ( g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] ) );
  if ( !vtp )
    return;
  goto_coord ( pass_along[1], pass_along[0], pass_along[5], &(vtp->coord));
}

/*
 * Automatically change the viewport to center on the track and zoom to see the extent of the track
 */
static void trw_layer_auto_track_view ( gpointer pass_along[6] )
{
  GList **trps = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );
  if ( trps && *trps )
  {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    trw_layer_find_maxmin_tracks ( NULL, trps, maxmin );
    trw_layer_zoom_to_show_latlons ( VIK_TRW_LAYER(pass_along[0]), pass_along[5], maxmin );
    if ( pass_along[1] )
      vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(pass_along[1]) );
    else
      vik_layer_emit_update ( VIK_LAYER(pass_along[0]), FALSE );
  }
}

static void trw_layer_edit_trackpoint ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(pass_along[0]);
  trw_layer_tpwin_init ( vtl );
}

/*************************************
 * merge/split by time routines 
 *************************************/

/* called for each key in track hash table.
 * If the current track has the same time stamp type, add it to the result,
 * except the one pointed by "exclude".
 * set exclude to NULL if there is no exclude to check.
 * Note that the result is in reverse (for performance reasons).
 */
typedef struct {
  GList **result;
  GList  *exclude;
  gboolean with_timestamps;
} twt_udata;
static void find_tracks_with_timestamp_type(gpointer key, gpointer value, gpointer udata)
{
  twt_udata *user_data = udata;
  VikTrackpoint *p1, *p2;

  if (VIK_TRACK(value)->trackpoints == user_data->exclude) {
    return;
  }

  if (VIK_TRACK(value)->trackpoints) {
    p1 = VIK_TRACKPOINT(VIK_TRACK(value)->trackpoints->data);
    p2 = VIK_TRACKPOINT(g_list_last(VIK_TRACK(value)->trackpoints)->data);

    if ( user_data->with_timestamps ) {
      if (!p1->has_timestamp || !p2->has_timestamp) {
	return;
      }
    }
    else {
      // Don't add tracks with timestamps when getting non timestamp tracks
      if (p1->has_timestamp || p2->has_timestamp) {
	return;
      }
    }
  }

  *(user_data->result) = g_list_prepend(*(user_data->result), key);
}

/* called for each key in track hash table. if original track user_data[1] is close enough
 * to the passed one, add it to list in user_data[0] 
 */
static void find_nearby_track(gpointer key, gpointer value, gpointer user_data)
{
  time_t t1, t2;
  VikTrackpoint *p1, *p2;

  GList **nearby_tracks = ((gpointer *)user_data)[0];
  GList *orig_track = ((gpointer *)user_data)[1];
  guint thr = GPOINTER_TO_UINT (((gpointer *)user_data)[2]);

  /* outline: 
   * detect reasons for not merging, and return
   * if no reason is found not to merge, then do it.
   */

  if (VIK_TRACK(value)->trackpoints == orig_track) {
    return;
  }

  t1 = VIK_TRACKPOINT(orig_track->data)->timestamp;
  t2 = VIK_TRACKPOINT(g_list_last(orig_track)->data)->timestamp;

  if (VIK_TRACK(value)->trackpoints) {
    p1 = VIK_TRACKPOINT(VIK_TRACK(value)->trackpoints->data);
    p2 = VIK_TRACKPOINT(g_list_last(VIK_TRACK(value)->trackpoints)->data);

    if (!p1->has_timestamp || !p2->has_timestamp) {
      g_print("no timestamp\n");
      return;
    }

    /*  g_print("Got track named %s, times %d, %d\n", (gchar *)key, p1->timestamp, p2->timestamp); */
    if (! (abs(t1 - p2->timestamp) < thr*60 ||
	/*  p1 p2      t1 t2 */
	   abs(p1->timestamp - t2) < thr*60)
	/*  t1 t2      p1 p2 */
	) {
      return;
    }
  }

  *nearby_tracks = g_list_prepend(*nearby_tracks, key);
}

/* comparison function used to sort tracks; a and b are hash table keys */
/* Not actively used - can be restored if needed
static gint track_compare(gconstpointer a, gconstpointer b, gpointer user_data)
{
  GHashTable *tracks = user_data;
  time_t t1, t2;

  t1 = VIK_TRACKPOINT(VIK_TRACK(g_hash_table_lookup(tracks, a))->trackpoints->data)->timestamp;
  t2 = VIK_TRACKPOINT(VIK_TRACK(g_hash_table_lookup(tracks, b))->trackpoints->data)->timestamp;
  
  if (t1 < t2) return -1;
  if (t1 > t2) return 1;
  return 0;
}
*/

/* comparison function used to sort trackpoints */
static gint trackpoint_compare(gconstpointer a, gconstpointer b)
{
  time_t t1 = VIK_TRACKPOINT(a)->timestamp, t2 = VIK_TRACKPOINT(b)->timestamp;
  
  if (t1 < t2) return -1;
  if (t1 > t2) return 1;
  return 0;
}

/**
 * comparison function which can be used to sort tracks or waypoints by name
 */
static gint sort_alphabetically (gconstpointer a, gconstpointer b, gpointer user_data)
{
  const gchar* namea = (const gchar*) a;
  const gchar* nameb = (const gchar*) b;
  if ( namea == NULL || nameb == NULL)
    return 0;
  else
    // Same sort method as used in the vik_treeview_*_alphabetize functions
    return strcmp ( namea, nameb );
}

/**
 * Attempt to merge selected track with other tracks specified by the user
 * Tracks to merge with must be of the same 'type' as the selected track -
 *  either all with timestamps, or all without timestamps
 */
static void trw_layer_merge_with_other ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = (VikTrwLayer *)pass_along[0];
  gchar *orig_track_name = pass_along[3];
  GList *other_tracks = NULL;
  VikTrack *track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, orig_track_name );

  if ( !track->trackpoints )
    return;

  twt_udata udata;
  udata.result = &other_tracks;
  udata.exclude = track->trackpoints;
  // Allow merging with 'similar' time type time tracks
  // i.e. either those times, or those without
  udata.with_timestamps = (VIK_TRACKPOINT(track->trackpoints->data)->has_timestamp);

  g_hash_table_foreach(vtl->tracks, find_tracks_with_timestamp_type, (gpointer)&udata);
  other_tracks = g_list_reverse(other_tracks);

  if ( !other_tracks ) {
    if ( udata.with_timestamps )
      a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other tracks with timestamps in this layer found"));
    else
      a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other tracks without timestamps in this layer found"));
    return;
  }

  // Sort alphabetically for user presentation
  other_tracks = g_list_sort_with_data (other_tracks, sort_alphabetically, NULL);

  GList *merge_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
      other_tracks, TRUE,
      _("Merge with..."), _("Select track to merge with"));
  g_list_free(other_tracks);

  if (merge_list)
  {
    GList *l;
    for (l = merge_list; l != NULL; l = g_list_next(l)) {
      VikTrack *merge_track = (VikTrack *) g_hash_table_lookup (vtl->tracks, l->data );
      if (merge_track) {
        track->trackpoints = g_list_concat(track->trackpoints, merge_track->trackpoints);
        merge_track->trackpoints = NULL;
        vik_trw_layer_delete_track(vtl, l->data);
        track->trackpoints = g_list_sort(track->trackpoints, trackpoint_compare);
      }
    }
    /* TODO: free data before free merge_list */
    for (l = merge_list; l != NULL; l = g_list_next(l))
      g_free(l->data);
    g_list_free(merge_list);
    vik_layer_emit_update( VIK_LAYER(vtl), FALSE );
  }
}

/* merge by time routine */
static void trw_layer_merge_by_timestamp ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = (VikTrwLayer *)pass_along[0];
  gchar *orig_track_name = strdup(pass_along[3]);

  //time_t t1, t2;
  GList *nearby_tracks;
  VikTrack *track;
  GList *trps;
  static  guint thr = 1;
  guint track_count = 0;

  GList *tracks_with_timestamp = NULL;
  track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, orig_track_name );
  if (track->trackpoints &&
      !VIK_TRACKPOINT(track->trackpoints->data)->has_timestamp) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. This track does not have timestamp"));
    free(orig_track_name);
    return;
  }

  twt_udata udata;
  udata.result = &tracks_with_timestamp;
  udata.exclude = track->trackpoints;
  udata.with_timestamps = TRUE;
  g_hash_table_foreach(vtl->tracks, find_tracks_with_timestamp_type, (gpointer)&udata);
  tracks_with_timestamp = g_list_reverse(tracks_with_timestamp);

  if (!tracks_with_timestamp) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other track in this layer has timestamp"));
    free(orig_track_name);
    return;
  }
  g_list_free(tracks_with_timestamp);

  if (!a_dialog_time_threshold(VIK_GTK_WINDOW_FROM_LAYER(vtl), 
			       _("Merge Threshold..."), 
			       _("Merge when time between tracks less than:"), 
			       &thr)) {
    free(orig_track_name);
    return;
  }

  /* merge tracks until we can't */
  nearby_tracks = NULL;
  do {
    gpointer params[3];

    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, orig_track_name );
    trps = track->trackpoints;
    if ( !trps )
      return;


    if (nearby_tracks) {
      g_list_free(nearby_tracks);
      nearby_tracks = NULL;
    }

    //t1 = ((VikTrackpoint *)trps->data)->timestamp;
    //t2 = ((VikTrackpoint *)g_list_last(trps)->data)->timestamp;
    
    /*    g_print("Original track times: %d and %d\n", t1, t2);  */
    params[0] = &nearby_tracks;
    params[1] = trps;
    params[2] = GUINT_TO_POINTER (thr);

    /* get a list of adjacent-in-time tracks */
    g_hash_table_foreach(vtl->tracks, find_nearby_track, (gpointer)params);

    /* add original track */
    nearby_tracks = g_list_prepend(nearby_tracks, orig_track_name);

    /* merge them */
    { 
#define get_track(x) VIK_TRACK(g_hash_table_lookup(vtl->tracks, (gchar *)((x)->data)))
#define get_first_trackpoint(x) VIK_TRACKPOINT(get_track(x)->trackpoints->data)
#define get_last_trackpoint(x) VIK_TRACKPOINT(g_list_last(get_track(x)->trackpoints)->data)
      GList *l = nearby_tracks;
      VikTrack *tr = vik_track_new();
      tr->visible = track->visible;
      track_count = 0;
      while (l) {
	/*
	time_t t1, t2;
	t1 = get_first_trackpoint(l)->timestamp;
	t2 = get_last_trackpoint(l)->timestamp;
	g_print("     %20s: track %d - %d\n", (char *)l->data, (int)t1, (int)t2);
	*/


	/* remove trackpoints from merged track, delete track */
	tr->trackpoints = g_list_concat(tr->trackpoints, get_track(l)->trackpoints);
	get_track(l)->trackpoints = NULL;
	vik_trw_layer_delete_track(vtl, l->data);

	track_count ++;
	l = g_list_next(l);
      }
      tr->trackpoints = g_list_sort(tr->trackpoints, trackpoint_compare);
      vik_trw_layer_add_track(vtl, strdup(orig_track_name), tr);

#undef get_first_trackpoint
#undef get_last_trackpoint
#undef get_track
    }
  } while (track_count > 1);
  g_list_free(nearby_tracks);
  free(orig_track_name);
  vik_layer_emit_update( VIK_LAYER(vtl), FALSE );
}

/* split by time routine */
static void trw_layer_split_by_timestamp ( gpointer pass_along[6] )
{
  VikTrack *track = (VikTrack *) g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );
  GList *trps = track->trackpoints;
  GList *iter;
  GList *newlists = NULL;
  GList *newtps = NULL;
  guint i;
  static guint thr = 1;

  time_t ts, prev_ts;

  if ( !trps )
    return;

  if (!a_dialog_time_threshold(VIK_GTK_WINDOW_FROM_LAYER(pass_along[0]), 
			       _("Split Threshold..."), 
			       _("Split when time between trackpoints exceeds:"), 
			       &thr)) {
    return;
  }

  /* iterate through trackpoints, and copy them into new lists without touching original list */
  prev_ts = VIK_TRACKPOINT(trps->data)->timestamp;
  iter = trps;

  while (iter) {
    ts = VIK_TRACKPOINT(iter->data)->timestamp;
    if (ts < prev_ts) {
      g_print("panic: ts < prev_ts: this should never happen!\n");
      return;
    }
    if (ts - prev_ts > thr*60) {
      /* flush accumulated trackpoints into new list */
      newlists = g_list_append(newlists, g_list_reverse(newtps));
      newtps = NULL;
    }

    /* accumulate trackpoint copies in newtps, in reverse order */
    newtps = g_list_prepend(newtps, vik_trackpoint_copy(VIK_TRACKPOINT(iter->data)));
    prev_ts = ts;
    iter = g_list_next(iter);
  }
  if (newtps) {
      newlists = g_list_append(newlists, g_list_reverse(newtps));
  }

  /* put lists of trackpoints into tracks */
  iter = newlists;
  i = 1;
  // Only bother updating if the split results in new tracks
  if (g_list_length (newlists) > 1) {
    while (iter) {
      gchar *new_tr_name;
      VikTrack *tr;

      tr = vik_track_new();
      tr->visible = track->visible;
      tr->trackpoints = (GList *)(iter->data);

      new_tr_name = g_strdup_printf("%s #%d", (gchar *) pass_along[3], i++);
      vik_trw_layer_add_track(VIK_TRW_LAYER(pass_along[0]), new_tr_name, tr);
      /*    g_print("adding track %s, times %d - %d\n", new_tr_name, VIK_TRACKPOINT(tr->trackpoints->data)->timestamp,
	  VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp);*/

      iter = g_list_next(iter);
    }
    vik_trw_layer_delete_track(VIK_TRW_LAYER(pass_along[0]), (gchar *)pass_along[3]);
    vik_layer_emit_update(VIK_LAYER(pass_along[0]), FALSE);
  }
  g_list_free(newlists);
}

/**
 * Split a track by the number of points as specified by the user
 */
static void trw_layer_split_by_n_points ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = (VikTrwLayer *)pass_along[0];
  VikTrack *track = (VikTrack *)g_hash_table_lookup ( vtl->tracks, pass_along[3] );

  // Check valid track
  GList *trps = track->trackpoints;
  if ( !trps )
    return;

  gint points = a_dialog_get_positive_number(VIK_GTK_WINDOW_FROM_LAYER(pass_along[0]),
					     _("Split Every Nth Point"),
					     _("Split on every Nth point:"),
					     250,   // Default value as per typical limited track capacity of various GPS devices
					     2,     // Min
					     65536, // Max
					     5);    // Step
  // Was a valid number returned?
  if (!points)
    return;

  // Now split...
  GList *iter;
  GList *newlists = NULL;
  GList *newtps = NULL;
  gint count = 0;
  iter = trps;

  while (iter) {
    /* accumulate trackpoint copies in newtps, in reverse order */
    newtps = g_list_prepend(newtps, vik_trackpoint_copy(VIK_TRACKPOINT(iter->data)));
    count++;
    if (count >= points) {
      /* flush accumulated trackpoints into new list */
      newlists = g_list_append(newlists, g_list_reverse(newtps));
      newtps = NULL;
      count = 0;
    }
    iter = g_list_next(iter);
  }

  // If there is a remaining chunk put that into the new split list
  // This may well be the whole track if no split points were encountered
  if (newtps) {
      newlists = g_list_append(newlists, g_list_reverse(newtps));
  }

  /* put lists of trackpoints into tracks */
  iter = newlists;
  guint i = 1;
  // Only bother updating if the split results in new tracks
  if (g_list_length (newlists) > 1) {
    while (iter) {
      gchar *new_tr_name;
      VikTrack *tr;

      tr = vik_track_new();
      tr->visible = track->visible;
      tr->trackpoints = (GList *)(iter->data);

      new_tr_name = g_strdup_printf("%s #%d", (gchar *) pass_along[3], i++);
      vik_trw_layer_add_track(VIK_TRW_LAYER(pass_along[0]), new_tr_name, tr);

      iter = g_list_next(iter);
    }
    // Remove original track and then update the display
    vik_trw_layer_delete_track(VIK_TRW_LAYER(pass_along[0]), (gchar *)pass_along[3]);
    vik_layer_emit_update(VIK_LAYER(pass_along[0]), FALSE);
  }
  g_list_free(newlists);
}

/* end of split/merge routines */

/**
 * Reverse a track
 */
static void trw_layer_reverse ( gpointer pass_along[6] )
{
  VikTrwLayer *vtl = (VikTrwLayer *)pass_along[0];
  VikTrack *track = (VikTrack *)g_hash_table_lookup ( vtl->tracks, pass_along[3] );

  // Check valid track
  GList *trps = track->trackpoints;
  if ( !trps )
    return;

  vik_track_reverse ( track );
 
  vik_layer_emit_update ( VIK_LAYER(pass_along[0]), FALSE );
}

/**
 * Similar to trw_layer_enum_item, but this uses a sorted method
 */
static void trw_layer_sorted_name_list(gpointer key, gpointer value, gpointer udata)
{
  GList **list = (GList**)udata;
  //*list = g_list_prepend(*all, key); //unsorted method
  // Sort named list alphabetically
  *list = g_list_insert_sorted_with_data (*list, key, sort_alphabetically, NULL);
}

/**
 *
 */
static void trw_layer_delete_tracks_from_selection ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  GList *all = NULL;
  // Sort list alphabetically for better presentation
  g_hash_table_foreach(vtl->tracks, trw_layer_sorted_name_list, &all);

  if ( ! all ) {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl),	_("No tracks found"));
    return;
  }

  // Get list of items to delete from the user
  GList *delete_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
						 all,
						 TRUE,
						 _("Delete Selection"),
						 _("Select tracks to delete"));
  g_list_free(all);

  // Delete requested tracks
  // since specificly requested, IMHO no need for extra confirmation
  if ( delete_list ) {
    GList *l;
    for (l = delete_list; l != NULL; l = g_list_next(l)) {
      vik_trw_layer_delete_track(vtl, l->data);
    }
    g_list_free(delete_list);
    vik_layer_emit_update( VIK_LAYER(vtl), FALSE );
  }
}

/**
 *
 */
static void trw_layer_delete_waypoints_from_selection ( gpointer lav[2] )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(lav[0]);
  GList *all = NULL;

  // Sort list alphabetically for better presentation
  g_hash_table_foreach ( vtl->waypoints, trw_layer_sorted_name_list, &all);
  if ( ! all ) {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl),	_("No waypoints found"));
    return;
  }

  all = g_list_sort_with_data(all, sort_alphabetically, NULL);

  // Get list of items to delete from the user
  GList *delete_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
						 all,
						 TRUE,
						 _("Delete Selection"),
						 _("Select waypoints to delete"));
  g_list_free(all);

  // Delete requested waypoints
  // since specificly requested, IMHO no need for extra confirmation
  if ( delete_list ) {
    GList *l;
    for (l = delete_list; l != NULL; l = g_list_next(l)) {
      vik_trw_layer_delete_waypoint(vtl, l->data);
    }
    g_list_free(delete_list);
    vik_layer_emit_update( VIK_LAYER(vtl), FALSE );
  }

}

static void trw_layer_goto_waypoint ( gpointer pass_along[6] )
{
  VikWaypoint *wp = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->waypoints, pass_along[3] );
  if ( wp )
    goto_coord ( pass_along[1], pass_along[0], pass_along[5], &(wp->coord) );
}

static void trw_layer_waypoint_gc_webpage ( gpointer pass_along[6] )
{
  gchar *webpage = g_strdup_printf("http://www.geocaching.com/seek/cache_details.aspx?wp=%s", (gchar *) pass_along[3] );
  open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(pass_along[0])), webpage);
  g_free ( webpage );
}

static const gchar* trw_layer_sublayer_rename_request ( VikTrwLayer *l, const gchar *newname, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter )
{
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    gchar *rv;
    VikWaypoint *wp;

    if (strcmp(newname, sublayer) == 0 )
      return NULL;

    if (strcasecmp(newname, sublayer)) { /* Not just changing case */
      if (g_hash_table_lookup( l->waypoints, newname))
      {
        a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(l), _("Waypoint Already Exists") );
        return NULL;
      }
    }

    iter = g_hash_table_lookup ( l->waypoints_iters, sublayer );
    g_hash_table_steal ( l->waypoints_iters, sublayer );

    wp = vik_waypoint_copy ( VIK_WAYPOINT(g_hash_table_lookup ( l->waypoints, sublayer )) );
    highest_wp_number_remove_wp(l, sublayer);
    g_hash_table_remove ( l->waypoints, sublayer );

    rv = g_strdup(newname);

    vik_treeview_item_set_pointer ( VIK_LAYER(l)->vt, iter, rv );

    highest_wp_number_add_wp(l, rv);
    g_hash_table_insert ( l->waypoints, rv, wp );
    g_hash_table_insert ( l->waypoints_iters, rv, iter );

    /* it hasn't been updated yet so we pass new name */
#ifdef VIK_CONFIG_ALPHABETIZED_TRW
    vik_treeview_sublayer_realphabetize ( VIK_LAYER(l)->vt, iter, rv );
#endif

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp) );
    return rv;
  }
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    gchar *rv;
    VikTrack *tr;
    GtkTreeIter *iter;
    gchar *orig_key;

    if (strcmp(newname, sublayer) == 0)
      return NULL;

    if (strcasecmp(newname, sublayer)) { /* Not just changing case */
      if (g_hash_table_lookup( l->tracks, newname))
      {
        a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(l), _("Track Already Exists") );
        return NULL;
      }
    }

    g_hash_table_lookup_extended ( l->tracks, sublayer, (void *)&orig_key, (void *)&tr );
    g_hash_table_steal ( l->tracks, sublayer );

    iter = g_hash_table_lookup ( l->tracks_iters, sublayer );
    g_hash_table_steal ( l->tracks_iters, sublayer );

    rv = g_strdup(newname);

    vik_treeview_item_set_pointer ( VIK_LAYER(l)->vt, iter, rv );

    g_hash_table_insert ( l->tracks, rv, tr );
    g_hash_table_insert ( l->tracks_iters, rv, iter );

    /* don't forget about current_tp_track_name, update that too */
    if ( l->current_tp_track_name && g_strcasecmp(orig_key,l->current_tp_track_name) == 0 )
    {
      l->current_tp_track_name = rv;
      if ( l->tpwin )
        vik_trw_layer_tpwin_set_track_name ( l->tpwin, rv );
    }
    else if ( l->last_tp_track_name && g_strcasecmp(orig_key,l->last_tp_track_name) == 0 )
      l->last_tp_track_name = rv;

    g_free ( orig_key );

#ifdef VIK_CONFIG_ALPHABETIZED_TRW
    vik_treeview_sublayer_realphabetize ( VIK_LAYER(l)->vt, iter, rv );
#endif

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp) );
    return rv;
  }
  return NULL;
}

static gboolean is_valid_geocache_name ( gchar *str )
{
  gint len = strlen ( str );
  return len >= 3 && len <= 7 && str[0] == 'G' && str[1] == 'C' && isalnum(str[2]) && (len < 4 || isalnum(str[3])) && (len < 5 || isalnum(str[4])) && (len < 6 || isalnum(str[5])) && (len < 7 || isalnum(str[6]));
}

static void trw_layer_track_use_with_filter ( gpointer pass_along[6] )
{
  gchar *track_name = (gchar *) pass_along[3];
  VikTrack *tr = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, track_name );
  a_acquire_set_filter_track ( tr, track_name );
}

static gboolean is_valid_google_route ( VikTrwLayer *vtl, const gchar *track_name )
{
  VikTrack *tr = g_hash_table_lookup ( vtl->tracks, track_name );
  return ( tr && tr->comment && strlen(tr->comment) > 7 && !strncmp(tr->comment, "from:", 5) );
}

static void trw_layer_track_google_route_webpage ( gpointer pass_along[6] )
{
  gchar *track_name = (gchar *) pass_along[3];
  VikTrack *tr = g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, track_name );
  if ( tr ) {
    gchar *escaped = uri_escape ( tr->comment );
    gchar *webpage = g_strdup_printf("http://maps.google.com/maps?f=q&hl=en&q=%s", escaped );
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(pass_along[0])), webpage);
    g_free ( escaped );
    g_free ( webpage );
  }
}

/* vlp can be NULL if necessary - i.e. right-click from a tool */
/* viewpoint is now available instead */
static gboolean trw_layer_sublayer_add_menu_items ( VikTrwLayer *l, GtkMenu *menu, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter, VikViewport *vvp )
{
  static gpointer pass_along[6];
  GtkWidget *item;
  gboolean rv = FALSE;

  pass_along[0] = l;
  pass_along[1] = vlp;
  pass_along[2] = GINT_TO_POINTER (subtype);
  pass_along[3] = sublayer;
  pass_along[4] = GINT_TO_POINTER (1); // Confirm delete request
  pass_along[5] = vvp;

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT || subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    rv = TRUE;

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_PROPERTIES, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_properties_item), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) {
      VikTrwLayer *vtl = l;
      VikTrack *tr = g_hash_table_lookup ( vtl->tracks, sublayer );
      if (tr && tr->property_dialog)
        gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE );
    }

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_CUT, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_cut_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_COPY, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_DELETE, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_item), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
    {
      gboolean separator_created = FALSE;

      /* could be a right-click using the tool */
      if ( vlp != NULL ) {
	item = gtk_menu_item_new ();
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );

	separator_created = TRUE;

        item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_waypoint), pass_along );
        gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
        gtk_widget_show ( item );
      }

      if ( is_valid_geocache_name ( (gchar *) sublayer ) )
      {
	if ( !separator_created ) {
	  item = gtk_menu_item_new ();
	  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	  gtk_widget_show ( item );
	  separator_created = TRUE;
	}

        item = gtk_menu_item_new_with_mnemonic ( _("_Visit Geocache Webpage") );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_gc_webpage), pass_along );
        gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
        gtk_widget_show ( item );
      }

      VikWaypoint *wp = g_hash_table_lookup ( VIK_TRW_LAYER(l)->waypoints, sublayer );

      if ( wp && wp->image )
      {
	if ( !separator_created ) {
	  item = gtk_menu_item_new ();
	  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	  gtk_widget_show ( item );
	  separator_created = TRUE;
	}

	// Set up image paramater
	pass_along[5] = wp->image;

        item = gtk_image_menu_item_new_with_mnemonic ( _("_Show Picture...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("Show Picture", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_show_picture), pass_along );
        gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
        gtk_widget_show ( item );

#ifdef VIK_CONFIG_GEOTAG
	GtkWidget *geotag_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic ( _("Update Geotag on _Image") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), geotag_submenu );
  
	item = gtk_menu_item_new_with_mnemonic ( _("_Update") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_update), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (geotag_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("Update and _Keep File Timestamp") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_keep), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (geotag_submenu), item);
	gtk_widget_show ( item );
#endif
      }

    }
  }

  if ( vlp && (subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) )
  {
    rv = TRUE;
    item = gtk_image_menu_item_new_with_mnemonic ( _("_New Waypoint...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wp), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS )
  {
    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_waypoints_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Goto _Waypoint...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_wp), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_waypoints), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Waypoints From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_waypoints_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS )
  {
    rv = TRUE;

    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_tracks_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_tracks), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Tracks From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_tracks_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    GtkWidget *goto_submenu;
    item = gtk_menu_item_new ();
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    goto_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), goto_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Startpoint") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_FIRST, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_startpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("\"_Center\"") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_center), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Endpoint") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_endpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Highest Altitude") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_TOP, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_max_alt), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Lowest Altitude") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_BOTTOM, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_min_alt), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Maximum Speed") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_MEDIA_FORWARD, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_max_speed), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_View Track") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_track_view), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Merge By Time...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_by_timestamp), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("Merge _With Other Tracks...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_with_other), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Split By Time...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_by_timestamp), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("Split By _Number of Points...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_by_n_points), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Reverse Track") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_reverse), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    /* ATM This function is only available via the layers panel, due to the method in finding out the maps in use */
    if ( vlp ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("Down_load Maps Along Track...") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("Maps Download", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_download_map_along_track_cb), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Apply DEM Data") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("DEM Download/Import", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Export Trac_k as GPX...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpx_track), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("E_xtend Track End") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_extend_track_end), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Extend _Using Route Finder") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("Route Finder", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_extend_track_end_route_finder), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

#ifdef VIK_CONFIG_OPENSTREETMAP
    item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _OSM...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(osm_traces_upload_track_cb), pass_along );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
#endif

    if ( is_valid_google_route ( l, (gchar *) sublayer ) )
    {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_View Google Directions") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_google_route_webpage), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    item = gtk_image_menu_item_new_with_mnemonic ( _("Use with _Filter") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_use_with_filter), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    /* ATM This function is only available via the layers panel, due to needing a vlp */
    if ( vlp ) {
      item = a_acquire_track_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), vlp,
				    vik_layers_panel_get_viewport(VIK_LAYERS_PANEL(vlp)),
				    g_hash_table_lookup ( l->tracks, (gchar *) sublayer ) );
      if ( item ) {
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
      }
    }

#ifdef VIK_CONFIG_GEOTAG
  item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_track), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
#endif

    // Only show on viewport popmenu when a trackpoint is selected
    if ( ! vlp && l->current_tpl ) {
      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Edit Trackpoint") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_edit_trackpoint), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

  }

  return rv;
}

static void trw_layer_insert_tp_after_current_tp ( VikTrwLayer *vtl )
{
  /* sanity checks */
  if (!vtl->current_tpl)
    return;
  if (!vtl->current_tpl->next)
    return;

  VikTrackpoint *tp_current = VIK_TRACKPOINT(vtl->current_tpl->data);
  VikTrackpoint *tp_next = VIK_TRACKPOINT(vtl->current_tpl->next->data);

  /* Use current and next trackpoints to form a new track point which is inserted into the tracklist */
  if ( tp_next ) {

    VikTrackpoint *tp_new = vik_trackpoint_new();
    struct LatLon ll_current, ll_next;
    vik_coord_to_latlon ( &tp_current->coord, &ll_current );
    vik_coord_to_latlon ( &tp_next->coord, &ll_next );

    /* main positional interpolation */
    struct LatLon ll_new = { (ll_current.lat+ll_next.lat)/2, (ll_current.lon+ll_next.lon)/2 };
    vik_coord_load_from_latlon ( &(tp_new->coord), vtl->coord_mode, &ll_new );

    /* Now other properties that can be interpolated */
    tp_new->altitude = (tp_current->altitude + tp_next->altitude) / 2;

    if (tp_current->has_timestamp && tp_next->has_timestamp) {
      /* Note here the division is applied to each part, then added
	 This is to avoid potential overflow issues with a 32 time_t for dates after midpoint of this Unix time on 2004/01/04 */
      tp_new->timestamp = (tp_current->timestamp/2) + (tp_next->timestamp/2);
      tp_new->has_timestamp = TRUE;
    }

    if (tp_current->speed != NAN && tp_next->speed != NAN)
      tp_new->speed = (tp_current->speed + tp_next->speed)/2;

    /* TODO - improve interpolation of course, as it may not be correct.
       if courses in degrees are 350 + 020, the mid course more likely to be 005 (not 185)
       [similar applies if value is in radians] */
    if (tp_current->course != NAN && tp_next->course != NAN)
      tp_new->speed = (tp_current->course + tp_next->course)/2;

    /* DOP / sat values remain at defaults as not they do not seem applicable to a dreamt up point */

    /* Insert new point into the trackpoints list after the current TP */
    VikTrack *tr = g_hash_table_lookup ( vtl->tracks, vtl->current_tp_track_name );
    gint index =  g_list_index ( tr->trackpoints, tp_current );
    if ( index > -1 ) {
      tr->trackpoints = g_list_insert (tr->trackpoints, tp_new, index+1 );
    }
  }
}

/* to be called when last_tpl no long exists. */
static void trw_layer_cancel_last_tp ( VikTrwLayer *vtl )
{
  if ( vtl->tpwin ) /* can't join with a non-existant TP. */
    vik_trw_layer_tpwin_disable_join ( vtl->tpwin );
  vtl->last_tpl = NULL;
  vtl->last_tp_track_name = NULL;
}

static void trw_layer_cancel_current_tp ( VikTrwLayer *vtl, gboolean destroy )
{
  if ( vtl->tpwin )
  {
    if ( destroy)
    {
      gtk_widget_destroy ( GTK_WIDGET(vtl->tpwin) );
      vtl->tpwin = NULL;
    }
    else
      vik_trw_layer_tpwin_set_empty ( vtl->tpwin );
  }
  if ( vtl->current_tpl )
  {
    vtl->current_tpl = NULL;
    vtl->current_tp_track_name = NULL;
    vik_layer_emit_update(VIK_LAYER(vtl), FALSE);
  }
}

static void trw_layer_tpwin_response ( VikTrwLayer *vtl, gint response )
{
  g_assert ( vtl->tpwin != NULL );
  if ( response == VIK_TRW_LAYER_TPWIN_CLOSE )
    trw_layer_cancel_current_tp ( vtl, TRUE );

  if ( vtl->current_tpl == NULL )
    return;

  if ( response == VIK_TRW_LAYER_TPWIN_SPLIT && vtl->current_tpl->next && vtl->current_tpl->prev )
  {
    gchar *name = get_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, vtl->current_tp_track_name);
    if ( ( name = a_dialog_new_track ( GTK_WINDOW(vtl->tpwin), vtl->tracks, name ) ) )
    {
      VikTrack *tr = vik_track_new ();
      GList *newglist = g_list_alloc ();
      newglist->prev = NULL;
      newglist->next = vtl->current_tpl->next;
      newglist->data = vik_trackpoint_copy(VIK_TRACKPOINT(vtl->current_tpl->data));
      tr->trackpoints = newglist;

      vtl->current_tpl->next->prev = newglist; /* end old track here */
      vtl->current_tpl->next = NULL;

      vtl->current_tpl = newglist; /* change tp to first of new track. */
      vtl->current_tp_track_name = name;

      vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl, vtl->current_tp_track_name );

      tr->visible = TRUE;

      vik_trw_layer_add_track ( vtl, name, tr );
      vik_layer_emit_update(VIK_LAYER(vtl), FALSE);
    }
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_DELETE )
  {
    VikTrack *tr = g_hash_table_lookup ( vtl->tracks, vtl->current_tp_track_name );
    GList *new_tpl;
    g_assert(tr != NULL);

    /* can't join with a non-existent trackpoint */
    vtl->last_tpl = NULL;
    vtl->last_tp_track_name = NULL;

    if ( (new_tpl = vtl->current_tpl->next) || (new_tpl = vtl->current_tpl->prev) )
    {
      if ( VIK_TRACKPOINT(vtl->current_tpl->data)->newsegment && vtl->current_tpl->next )
        VIK_TRACKPOINT(vtl->current_tpl->next->data)->newsegment = TRUE; /* don't concat segments on del */

      tr->trackpoints = g_list_remove_link ( tr->trackpoints, vtl->current_tpl ); /* this nulls current_tpl->prev and next */

      /* at this point the old trackpoint exists, but the list links are correct (new), so it is safe to do this. */
      vik_trw_layer_tpwin_set_tp ( vtl->tpwin, new_tpl, vtl->current_tp_track_name );

      trw_layer_cancel_last_tp ( vtl );

      g_free ( vtl->current_tpl->data ); /* TODO: vik_trackpoint_free() */
      g_list_free_1 ( vtl->current_tpl );
      vtl->current_tpl = new_tpl;
      vik_layer_emit_update(VIK_LAYER(vtl), FALSE);
    }
    else
    {
      tr->trackpoints = g_list_remove_link ( tr->trackpoints, vtl->current_tpl );
      g_free ( vtl->current_tpl->data ); /* TODO longone: vik_trackpoint_new() and vik_trackpoint_free() */
      g_list_free_1 ( vtl->current_tpl );
      trw_layer_cancel_current_tp ( vtl, FALSE );
    }
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_FORWARD && vtl->current_tpl->next )
  {
    vtl->last_tpl = vtl->current_tpl;
    vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl = vtl->current_tpl->next, vtl->current_tp_track_name );
    vik_layer_emit_update(VIK_LAYER(vtl), FALSE); /* TODO longone: either move or only update if tp is inside drawing window */
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_BACK && vtl->current_tpl->prev )
  {
    vtl->last_tpl = vtl->current_tpl;
    vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl = vtl->current_tpl->prev, vtl->current_tp_track_name );
    vik_layer_emit_update(VIK_LAYER(vtl), FALSE);
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_JOIN )
  {
    // Check tracks exist and are different before joining
    if ( ! vtl->last_tp_track_name || ! vtl->current_tp_track_name || vtl->last_tp_track_name == vtl->current_tp_track_name )
      return;

    VikTrack *tr1 = g_hash_table_lookup ( vtl->tracks, vtl->last_tp_track_name );
    VikTrack *tr2 = g_hash_table_lookup ( vtl->tracks, vtl->current_tp_track_name );

    VikTrack *tr_first = tr1, *tr_last = tr2;

    gchar *tmp;

    if ( (!vtl->last_tpl->next) && (!vtl->current_tpl->next) ) /* both endpoints */
      vik_track_reverse ( tr2 ); /* reverse the second, that way second track clicked will be later. */
    else if ( (!vtl->last_tpl->prev) && (!vtl->current_tpl->prev) )
      vik_track_reverse ( tr1 );
    else if ( (!vtl->last_tpl->prev) && (!vtl->current_tpl->next) ) /* clicked startpoint, then endpoint -- concat end to start */
    {
      tr_first = tr2;
      tr_last = tr1;
    }
    /* default -- clicked endpoint then startpoint -- connect endpoint to startpoint */

    if ( tr_last->trackpoints ) /* deleting this part here joins them into 1 segmented track. useful but there's no place in the UI for this feature. segments should be deprecated anyway. */
      VIK_TRACKPOINT(tr_last->trackpoints->data)->newsegment = FALSE;
    tr1->trackpoints = g_list_concat ( tr_first->trackpoints, tr_last->trackpoints );
    tr2->trackpoints = NULL;

    tmp = vtl->current_tp_track_name;

    vtl->current_tp_track_name = vtl->last_tp_track_name; /* current_tp stays the same (believe it or not!) */
    vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl, vtl->current_tp_track_name );

    /* if we did this before, trw_layer_delete_track would have canceled the current tp because
     * it was the current track. canceling the current tp would have set vtl->current_tpl to NULL */
    vik_trw_layer_delete_track ( vtl, tmp );

    trw_layer_cancel_last_tp ( vtl ); /* same TP, can't join. */
    vik_layer_emit_update(VIK_LAYER(vtl), FALSE);
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_INSERT && vtl->current_tpl->next )
  {
    trw_layer_insert_tp_after_current_tp ( vtl );
    vik_layer_emit_update(VIK_LAYER(vtl), FALSE);
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_DATA_CHANGED )
    vik_layer_emit_update(VIK_LAYER(vtl), FALSE);
}

static void trw_layer_tpwin_init ( VikTrwLayer *vtl )
{
  if ( ! vtl->tpwin )
  {
    vtl->tpwin = vik_trw_layer_tpwin_new ( VIK_GTK_WINDOW_FROM_LAYER(vtl) );
    g_signal_connect_swapped ( GTK_DIALOG(vtl->tpwin), "response", G_CALLBACK(trw_layer_tpwin_response), vtl );
    /* connect signals -- DELETE SIGNAL VERY IMPORTANT TO SET TO NULL */
    g_signal_connect_swapped ( vtl->tpwin, "delete-event", G_CALLBACK(trw_layer_cancel_current_tp), vtl );
    gtk_widget_show_all ( GTK_WIDGET(vtl->tpwin) );
  }
  if ( vtl->current_tpl )
    vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl, vtl->current_tp_track_name );
  /* set layer name and TP data */
}

/***************************************************************************
 ** Tool code
 ***************************************************************************/

/*** Utility data structures and functions ****/

typedef struct {
  gint x, y;
  gint closest_x, closest_y;
  gchar *closest_wp_name;
  VikWaypoint *closest_wp;
  VikViewport *vvp;
} WPSearchParams;

typedef struct {
  gint x, y;
  gint closest_x, closest_y;
  gchar *closest_track_name;
  VikTrackpoint *closest_tp;
  VikViewport *vvp;
  GList *closest_tpl;
} TPSearchParams;

static void waypoint_search_closest_tp ( gchar *name, VikWaypoint *wp, WPSearchParams *params )
{
  gint x, y;
  if ( !wp->visible )
    return;

  vik_viewport_coord_to_screen ( params->vvp, &(wp->coord), &x, &y );

  // If waypoint has an image then use the image size to select
  if ( wp->image ) {
    gint slackx, slacky;
    slackx = wp->image_width / 2;
    slacky = wp->image_height / 2;

    if (    x <= params->x + slackx && x >= params->x - slackx
         && y <= params->y + slacky && y >= params->y - slacky ) {
      params->closest_wp_name = name;
      params->closest_wp = wp;
      params->closest_x = x;
      params->closest_y = y;
    }
  }
  else if ( abs (x - params->x) <= WAYPOINT_SIZE_APPROX && abs (y - params->y) <= WAYPOINT_SIZE_APPROX &&
	    ((!params->closest_wp) ||        /* was the old waypoint we already found closer than this one? */
	     abs(x - params->x)+abs(y - params->y) < abs(x - params->closest_x)+abs(y - params->closest_y)))
    {
      params->closest_wp_name = name;
      params->closest_wp = wp;
      params->closest_x = x;
      params->closest_y = y;
    }
}

static void track_search_closest_tp ( gchar *name, VikTrack *t, TPSearchParams *params )
{
  GList *tpl = t->trackpoints;
  VikTrackpoint *tp;

  if ( !t->visible )
    return;

  while (tpl)
  {
    gint x, y;
    tp = VIK_TRACKPOINT(tpl->data);

    vik_viewport_coord_to_screen ( params->vvp, &(tp->coord), &x, &y );
 
    if ( abs (x - params->x) <= TRACKPOINT_SIZE_APPROX && abs (y - params->y) <= TRACKPOINT_SIZE_APPROX &&
        ((!params->closest_tp) ||        /* was the old trackpoint we already found closer than this one? */
          abs(x - params->x)+abs(y - params->y) < abs(x - params->closest_x)+abs(y - params->closest_y)))
    {
      params->closest_track_name = name;
      params->closest_tp = tp;
      params->closest_tpl = tpl;
      params->closest_x = x;
      params->closest_y = y;
    }
    tpl = tpl->next;
  }
}

static VikTrackpoint *closest_tp_in_five_pixel_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y )
{
  TPSearchParams params;
  params.x = x;
  params.y = y;
  params.vvp = vvp;
  params.closest_track_name = NULL;
  params.closest_tp = NULL;
  g_hash_table_foreach ( vtl->tracks, (GHFunc) track_search_closest_tp, &params);
  return params.closest_tp;
}

static VikWaypoint *closest_wp_in_five_pixel_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y )
{
  WPSearchParams params;
  params.x = x;
  params.y = y;
  params.vvp = vvp;
  params.closest_wp = NULL;
  params.closest_wp_name = NULL;
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) waypoint_search_closest_tp, &params);
  return params.closest_wp;
}


// Some forward declarations
static void marker_begin_move ( tool_ed_t *t, gint x, gint y );
static void marker_moveto ( tool_ed_t *t, gint x, gint y );
static void marker_end_move ( tool_ed_t *t );
//

static gboolean trw_layer_select_move ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t* t )
{
  if ( t->holding ) {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    // Here always allow snapping back to the original location
    //  this is useful when one decides not to move the thing afterall
    // If one wants to move the item only a little bit then don't hold down the 'snap' key!
 
    // snap to TP
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    // snap to WP
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( wp )
        new_coord = wp->coord;
    }
    
    gint x, y;
    vik_viewport_coord_to_screen ( vvp, &new_coord, &x, &y );

    marker_moveto ( t, x, y );

    return TRUE;
  }
  return FALSE;
}

static gboolean trw_layer_select_release ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t* t )
{
  if ( t->holding && event->button == 1 )
  {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    // snap to TP
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    // snap to WP
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( wp )
        new_coord = wp->coord;
    }

    marker_end_move ( t );

    // Determine if working on a waypoint or a trackpoint
    if ( t->is_waypoint )
      vtl->current_wp->coord = new_coord;
    else {
      if ( vtl->current_tpl ) {
	VIK_TRACKPOINT(vtl->current_tpl->data)->coord = new_coord;
      
	if ( vtl->tpwin )
	  vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl, vtl->current_tp_track_name );

	// Don't really know what this is for but seems like it might be handy...
	/* can't join with itself! */
	trw_layer_cancel_last_tp ( vtl );
      }
    }

    // Reset
    vtl->current_wp      = NULL;
    vtl->current_wp_name = NULL;
    trw_layer_cancel_current_tp ( vtl, FALSE );

    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }
  return FALSE;
}

/*
  Returns true if a waypoint or track is found near the requested event position for this particular layer
  The item found is automatically selected
  This is a tool like feature but routed via the layer interface, since it's instigated by a 'global' layer tool in vikwindow.c
 */
static gboolean trw_layer_select_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t* tet )
{
  if ( event->button != 1 )
    return FALSE;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( !vtl->tracks_visible && !vtl->waypoints_visible )
    return FALSE;

  // Go for waypoints first as these often will be near a track, but it's likely the wp is wanted rather then the track

  if (vtl->waypoints_visible) {
    WPSearchParams wp_params;
    wp_params.vvp = vvp;
    wp_params.x = event->x;
    wp_params.y = event->y;
    wp_params.closest_wp_name = NULL;
    wp_params.closest_wp = NULL;

    g_hash_table_foreach ( vtl->waypoints, (GHFunc) waypoint_search_closest_tp, &wp_params);

    if ( wp_params.closest_wp )  {

      // Select
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->waypoints_iters, wp_params.closest_wp_name ), TRUE );

      // Too easy to move it so must be holding shift to start immediately moving it
      //   or otherwise be previously selected but not have an image (otherwise clicking within image bounds (again) moves it)
      if ( event->state & GDK_SHIFT_MASK ||
	   ( vtl->current_wp == wp_params.closest_wp && !vtl->current_wp->image ) ) {
	// Put into 'move buffer'
	// NB vvp & vw already set in tet
	tet->vtl = (gpointer)vtl;
	tet->is_waypoint = TRUE;
      
	marker_begin_move (tet, event->x, event->y);
      }

      vtl->current_wp =      wp_params.closest_wp;
      vtl->current_wp_name = wp_params.closest_wp_name;

      vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );

      return TRUE;
    }
  }

  if (vtl->tracks_visible) {
    TPSearchParams tp_params;
    tp_params.vvp = vvp;
    tp_params.x = event->x;
    tp_params.y = event->y;
    tp_params.closest_track_name = NULL;
    tp_params.closest_tp = NULL;

    g_hash_table_foreach ( vtl->tracks, (GHFunc) track_search_closest_tp, &tp_params);

    if ( tp_params.closest_tp )  {

      // Always select + highlight the track
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->tracks_iters, tp_params.closest_track_name ), TRUE );

      tet->is_waypoint = FALSE;

      // Select the Trackpoint
      // Can move it immediately when control held or it's the previously selected tp
      if ( event->state & GDK_CONTROL_MASK ||
	   vtl->current_tpl == tp_params.closest_tpl ) {
	// Put into 'move buffer'
	// NB vvp & vw already set in tet
	tet->vtl = (gpointer)vtl;
	marker_begin_move (tet, event->x, event->y);
      }

      vtl->current_tpl = tp_params.closest_tpl;
      vtl->current_tp_track_name = tp_params.closest_track_name;

      set_statusbar_msg_info_trkpt ( vtl, tp_params.closest_tp );

      if ( vtl->tpwin )
	vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl, vtl->current_tp_track_name );

      vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
      return TRUE;
    }
  }

  /* these aren't the droids you're looking for */
  vtl->current_wp      = NULL;
  vtl->current_wp_name = NULL;
  trw_layer_cancel_current_tp ( vtl, FALSE );

  // Blank info
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, "" );

  return FALSE;
}

static gboolean trw_layer_show_selected_viewport_menu ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  if ( event->button != 3 )
    return FALSE;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( !vtl->tracks_visible && !vtl->waypoints_visible )
    return FALSE;

  /* Post menu for the currently selected item */

  /* See if a track is selected */
  VikTrack *track = (VikTrack*)vik_window_get_selected_track ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) );
  if ( track && track->visible ) {

    if ( vik_window_get_selected_name ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) ) ) {

      if ( vtl->track_right_click_menu )
	gtk_object_sink ( GTK_OBJECT(vtl->track_right_click_menu) );

      vtl->track_right_click_menu = GTK_MENU ( gtk_menu_new () );
      
      trw_layer_sublayer_add_menu_items ( vtl,
					  vtl->track_right_click_menu,
					  NULL,
					  VIK_TRW_LAYER_SUBLAYER_TRACK,
					  vik_window_get_selected_name ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) ),
					  g_hash_table_lookup ( vtl->tracks_iters, vik_window_get_selected_name ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) ) ),
					  vvp);

      gtk_menu_popup ( vtl->track_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
	
      return TRUE;
    }
  }

  /* See if a waypoint is selected */
  VikWaypoint *waypoint = (VikWaypoint*)vik_window_get_selected_waypoint ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) );
  if ( waypoint && waypoint->visible ) {
    if ( vik_window_get_selected_name ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) ) ) {

      if ( vtl->wp_right_click_menu )
	gtk_object_sink ( GTK_OBJECT(vtl->wp_right_click_menu) );

      vtl->wp_right_click_menu = GTK_MENU ( gtk_menu_new () );
      trw_layer_sublayer_add_menu_items ( vtl,
					  vtl->wp_right_click_menu,
					  NULL,
					  VIK_TRW_LAYER_SUBLAYER_WAYPOINT,
					  vik_window_get_selected_name ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) ),
					  g_hash_table_lookup ( vtl->waypoints_iters, vik_window_get_selected_name ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) ) ),
					  vvp);
      gtk_menu_popup ( vtl->wp_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );

      return TRUE;
    }
  }

  return FALSE;
}

/* background drawing hook, to be passed the viewport */
static gboolean tool_sync_done = TRUE;

static gboolean tool_sync(gpointer data)
{
  VikViewport *vvp = data;
  gdk_threads_enter();
  vik_viewport_sync(vvp);
  tool_sync_done = TRUE;
  gdk_threads_leave();
  return FALSE;
}

static void marker_begin_move ( tool_ed_t *t, gint x, gint y )
{
  t->holding = TRUE;
  t->gc = vik_viewport_new_gc (t->vvp, "black", 2);
  gdk_gc_set_function ( t->gc, GDK_INVERT );
  vik_viewport_draw_rectangle ( t->vvp, t->gc, FALSE, x-3, y-3, 6, 6 );
  vik_viewport_sync(t->vvp);
  t->oldx = x;
  t->oldy = y;
}

static void marker_moveto ( tool_ed_t *t, gint x, gint y )
{
  VikViewport *vvp =  t->vvp;
  vik_viewport_draw_rectangle ( vvp, t->gc, FALSE, t->oldx-3, t->oldy-3, 6, 6 );
  vik_viewport_draw_rectangle ( vvp, t->gc, FALSE, x-3, y-3, 6, 6 );
  t->oldx = x;
  t->oldy = y;

  if (tool_sync_done) {
    g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, tool_sync, vvp, NULL);
    tool_sync_done = FALSE;
  }
}

static void marker_end_move ( tool_ed_t *t )
{
  vik_viewport_draw_rectangle ( t->vvp, t->gc, FALSE, t->oldx-3, t->oldy-3, 6, 6 );
  g_object_unref ( t->gc );
  t->holding = FALSE;
}

/*** Edit waypoint ****/

static gpointer tool_edit_waypoint_create ( VikWindow *vw, VikViewport *vvp)
{
  tool_ed_t *t = g_new(tool_ed_t, 1);
  t->vvp = vvp;
  t->holding = FALSE;
  return t;
}

static gboolean tool_edit_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  WPSearchParams params;
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( t->holding ) {
    return TRUE;
  }

  if ( !vtl->vl.visible || !vtl->waypoints_visible )
    return FALSE;

  if ( vtl->current_wp && vtl->current_wp->visible )
  {
    /* first check if current WP is within area (other may be 'closer', but we want to move the current) */
    gint x, y;
    vik_viewport_coord_to_screen ( vvp, &(vtl->current_wp->coord), &x, &y );

    if ( abs(x - event->x) <= WAYPOINT_SIZE_APPROX &&
         abs(y - event->y) <= WAYPOINT_SIZE_APPROX )
    {
      if ( event->button == 3 )
        vtl->waypoint_rightclick = TRUE; /* remember that we're clicking; other layers will ignore release signal */
      else {
	marker_begin_move(t, event->x, event->y);
      }
      return TRUE;
    }
  }

  params.vvp = vvp;
  params.x = event->x;
  params.y = event->y;
  params.closest_wp_name = NULL;
  /* TODO: should get track listitem so we can break it up, make a new track, mess it up, all that. */
  params.closest_wp = NULL;
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) waypoint_search_closest_tp, &params);
  if ( vtl->current_wp == params.closest_wp && vtl->current_wp != NULL )
  {
    /* how do we get here? I'm putting in the abort until we can figure it out. -alex */
    marker_begin_move(t, event->x, event->y);
    g_critical("shouldn't be here");
    exit(1);
  }
  else if ( params.closest_wp )
  {
    if ( event->button == 3 )
      vtl->waypoint_rightclick = TRUE; /* remember that we're clicking; other layers will ignore release signal */
    else
      vtl->waypoint_rightclick = FALSE;

    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->waypoints_iters, params.closest_wp_name ), TRUE );

    vtl->current_wp = params.closest_wp;
    vtl->current_wp_name = params.closest_wp_name;

    /* could make it so don't update if old WP is off screen and new is null but oh well */
    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }

  vtl->current_wp = NULL;
  vtl->current_wp_name = NULL;
  vtl->waypoint_rightclick = FALSE;
  vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
  return FALSE;
}

static gboolean tool_edit_waypoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( t->holding ) {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    /* snap to WP */
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( wp && wp != vtl->current_wp )
        new_coord = wp->coord;
    }
    
    { 
      gint x, y;
      vik_viewport_coord_to_screen ( vvp, &new_coord, &x, &y );

      marker_moveto ( t, x, y );
    } 
    return TRUE;
  }
  return FALSE;
}

static gboolean tool_edit_waypoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;
  
  if ( t->holding && event->button == 1 )
  {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    /* snap to WP */
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( wp && wp != vtl->current_wp )
        new_coord = wp->coord;
    }

    marker_end_move ( t );

    vtl->current_wp->coord = new_coord;
    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }
  /* PUT IN RIGHT PLACE!!! */
  if ( event->button == 3 && vtl->waypoint_rightclick )
  {
    if ( vtl->wp_right_click_menu )
      g_object_ref_sink ( G_OBJECT(vtl->wp_right_click_menu) );
    if ( vtl->current_wp ) {
      vtl->wp_right_click_menu = GTK_MENU ( gtk_menu_new () );
      trw_layer_sublayer_add_menu_items ( vtl, vtl->wp_right_click_menu, NULL, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, vtl->current_wp_name, g_hash_table_lookup ( vtl->waypoints_iters, vtl->current_wp_name ), vvp );
      gtk_menu_popup ( vtl->wp_right_click_menu, NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
    }
    vtl->waypoint_rightclick = FALSE;
  }
  return FALSE;
}

/**** Begin track ***/
static gpointer tool_begin_track_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean tool_begin_track_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  vtl->current_track = NULL;
  return tool_new_track_click ( vtl, event, vvp );
}

/*** New track ****/

static gpointer tool_new_track_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

typedef struct {
  VikTrwLayer *vtl;
  VikViewport *vvp;
  gint x1,y1,x2,y2,x3,y3;
  const gchar* str;
} new_track_move_passalong_t;

/* sync and undraw, but only when we have time */
static gboolean ct_sync ( gpointer passalong )
{
  new_track_move_passalong_t *p = (new_track_move_passalong_t *) passalong;

  vik_viewport_sync ( p->vvp );
  gdk_gc_set_function ( p->vtl->current_track_gc, GDK_INVERT );
  vik_viewport_draw_line ( p->vvp, p->vtl->current_track_gc, p->x1, p->y1, p->x2, p->y2 );
  vik_viewport_draw_string (p->vvp, gdk_font_from_description (pango_font_description_from_string ("Sans 8")), p->vtl->current_track_gc, p->x3, p->y3, p->str);
  gdk_gc_set_function ( p->vtl->current_track_gc, GDK_COPY );

  g_free ( (gpointer) p->str ) ;
  p->vtl->ct_sync_done = TRUE;
  g_free ( p );
  return FALSE;
}

static const gchar* distance_string (gdouble distance)
{
  gchar str[128];

  /* draw label with distance */
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  switch (dist_units) {
  case VIK_UNITS_DISTANCE_MILES:
    if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
      g_sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
    } else if (distance < 1609.4) {
      g_sprintf(str, "%d yards", (int)(distance*1.0936133));
    } else {
      g_sprintf(str, "%d miles", (int)VIK_METERS_TO_MILES(distance));
    }
    break;
  default:
    // VIK_UNITS_DISTANCE_KILOMETRES
    if (distance >= 1000 && distance < 100000) {
      g_sprintf(str, "%3.2f km", distance/1000.0);
    } else if (distance < 1000) {
      g_sprintf(str, "%d m", (int)distance);
    } else {
      g_sprintf(str, "%d km", (int)distance/1000);
    }
    break;
  }
  return g_strdup (str);
}

/*
 * Actually set the message in statusbar
 */
static void statusbar_write (const gchar *distance_string, gdouble elev_gain, gdouble elev_loss, VikTrwLayer *vtl )
{
  // Only show elevation data when track has some elevation properties
  gchar str_gain_loss[64];
  str_gain_loss[0] = '\0';
  
  if ( (elev_gain > 0.1) || (elev_loss > 0.1) ) {
    if ( a_vik_get_units_height () == VIK_UNITS_HEIGHT_METRES )
      g_sprintf(str_gain_loss, _(" - Gain %dm:Loss %dm"), (int)elev_gain, (int)elev_loss);
    else
      g_sprintf(str_gain_loss, _(" - Gain %dft:Loss %dft"), (int)VIK_METERS_TO_FEET(elev_gain), (int)VIK_METERS_TO_FEET(elev_loss));
  }

  // Write with full gain/loss information
  gchar *msg = g_strdup_printf ( "%s%s", distance_string, str_gain_loss);
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, msg );
  g_free ( msg );
}

/*
 * Figure out what information should be set in the statusbar and then write it
 */
static void update_statusbar ( VikTrwLayer *vtl )
{
  // Get elevation data
  gdouble elev_gain, elev_loss;
  vik_track_get_total_elevation_gain ( vtl->current_track, &elev_gain, &elev_loss);

  /* Find out actual distance of current track */
  gdouble distance = vik_track_get_length (vtl->current_track);
  const gchar *str = distance_string (distance);

  statusbar_write (str, elev_gain, elev_loss, vtl);

  g_free ((gpointer)str);
}


static VikLayerToolFuncStatus tool_new_track_move ( VikTrwLayer *vtl, GdkEventMotion *event, VikViewport *vvp )
{
  /* if we haven't sync'ed yet, we don't have time to do more. */
  if ( vtl->ct_sync_done && vtl->current_track && vtl->current_track->trackpoints ) {
    GList *iter = vtl->current_track->trackpoints;
    new_track_move_passalong_t *passalong;
    gint x1, y1;

    while ( iter->next )
      iter = iter->next;
    gdk_gc_set_function ( vtl->current_track_gc, GDK_INVERT );
    vik_viewport_coord_to_screen ( vvp, &(VIK_TRACKPOINT(iter->data)->coord), &x1, &y1 );
    vik_viewport_draw_line ( vvp, vtl->current_track_gc, x1, y1, event->x, event->y );

    /* Find out actual distance of current track */
    gdouble distance = vik_track_get_length (vtl->current_track);

    // Now add distance to where the pointer is //
    VikCoord coord;
    struct LatLon ll;
    vik_viewport_screen_to_coord ( vvp, (gint) event->x, (gint) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );
    distance = distance + vik_coord_diff( &coord, &(VIK_TRACKPOINT(iter->data)->coord));

    // Get elevation data
    gdouble elev_gain, elev_loss;
    vik_track_get_total_elevation_gain ( vtl->current_track, &elev_gain, &elev_loss);

    // Adjust elevation data (if available) for the current pointer position
    gdouble elev_new;
    elev_new = (gdouble) a_dems_get_elev_by_coord ( &coord, VIK_DEM_INTERPOL_BEST );
    if ( elev_new != VIK_DEM_INVALID_ELEVATION ) {
      if ( VIK_TRACKPOINT(iter->data)->altitude != VIK_DEFAULT_ALTITUDE ) {
	// Adjust elevation of last track point
	if ( elev_new > VIK_TRACKPOINT(iter->data)->altitude )
	  // Going up
	  elev_gain += elev_new - VIK_TRACKPOINT(iter->data)->altitude;
	else
	  // Going down
	  elev_loss += VIK_TRACKPOINT(iter->data)->altitude - elev_new;
      }
    }
      
    const gchar *str = distance_string (distance);
    gint xd,yd;
    /* offset from cursor a bit */
    xd = event->x + 10;
    yd = event->y - 10;
    /* note attempted setting text using pango layouts - but the 'undraw' technique didn't work */
    vik_viewport_draw_string (vvp, gdk_font_from_description (pango_font_description_from_string ("Sans 8")), vtl->current_track_gc, xd, yd, str);

    gdk_gc_set_function ( vtl->current_track_gc, GDK_COPY );

    passalong = g_new(new_track_move_passalong_t,1); /* freed by sync */
    passalong->vtl = vtl;
    passalong->vvp = vvp;
    passalong->x1 = x1;
    passalong->y1 = y1;
    passalong->x2 = event->x;
    passalong->y2 = event->y;
    passalong->x3 = xd;
    passalong->y3 = yd;
    passalong->str = str;

    // Update statusbar with full gain/loss information
    statusbar_write (str, elev_gain, elev_loss, vtl);

    /* this will sync and undraw when we have time to */
    g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, ct_sync, passalong, NULL);
    vtl->ct_sync_done = FALSE;
    return VIK_LAYER_TOOL_ACK_GRAB_FOCUS;
  }
  return VIK_LAYER_TOOL_ACK;
}

static gboolean tool_new_track_key_press ( VikTrwLayer *vtl, GdkEventKey *event, VikViewport *vvp )
{
  if ( vtl->current_track && event->keyval == GDK_Escape ) {
    vtl->current_track = NULL;
    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  } else if ( vtl->current_track && event->keyval == GDK_BackSpace ) {
    /* undo */
    if ( vtl->current_track->trackpoints )
    {
      GList *last = g_list_last(vtl->current_track->trackpoints);
      g_free ( last->data );
      vtl->current_track->trackpoints = g_list_remove_link ( vtl->current_track->trackpoints, last );
    }
    
    update_statusbar ( vtl );

    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }
  return FALSE;
}

static gboolean tool_new_track_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  VikTrackpoint *tp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( event->button == 3 && vtl->current_track )
  {
    /* undo */
    if ( vtl->current_track->trackpoints )
    {
      GList *last = g_list_last(vtl->current_track->trackpoints);
      g_free ( last->data );
      vtl->current_track->trackpoints = g_list_remove_link ( vtl->current_track->trackpoints, last );
    }
    update_statusbar ( vtl );

    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }

  if ( event->type == GDK_2BUTTON_PRESS )
  {
    /* subtract last (duplicate from double click) tp then end */
    if ( vtl->current_track && vtl->current_track->trackpoints && vtl->ct_x1 == vtl->ct_x2 && vtl->ct_y1 == vtl->ct_y2 )
    {
      GList *last = g_list_last(vtl->current_track->trackpoints);
      g_free ( last->data );
      vtl->current_track->trackpoints = g_list_remove_link ( vtl->current_track->trackpoints, last );
      /* undo last, then end */
      vtl->current_track = NULL;
    }
    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }

  if ( ! vtl->current_track )
  {
    gchar *name = get_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, _("Track"));
    if ( ( name = a_dialog_new_track ( VIK_GTK_WINDOW_FROM_LAYER(vtl), vtl->tracks, name ) ) )
    {
      vtl->current_track = vik_track_new();
      vtl->current_track->visible = TRUE;
      vik_trw_layer_add_track ( vtl, name, vtl->current_track );

      /* incase it was created by begin track */
      vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_CREATE_TRACK );
    }
    else
      return TRUE;
  }
  tp = vik_trackpoint_new();
  vik_viewport_screen_to_coord ( vvp, event->x, event->y, &(tp->coord) );

  /* snap to other TP */
  if ( event->state & GDK_CONTROL_MASK )
  {
    VikTrackpoint *other_tp = closest_tp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
    if ( other_tp )
      tp->coord = other_tp->coord;
  }

  tp->newsegment = FALSE;
  tp->has_timestamp = FALSE;
  tp->timestamp = 0;
  vtl->current_track->trackpoints = g_list_append ( vtl->current_track->trackpoints, tp );
  /* Auto attempt to get elevation from DEM data (if it's available) */
  vik_track_apply_dem_data_last_trackpoint ( vtl->current_track );

  vtl->ct_x1 = vtl->ct_x2;
  vtl->ct_y1 = vtl->ct_y2;
  vtl->ct_x2 = event->x;
  vtl->ct_y2 = event->y;

  vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
  return TRUE;
}

/*** New waypoint ****/

static gpointer tool_new_waypoint_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean tool_new_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  VikCoord coord;
  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;
  vik_viewport_screen_to_coord ( vvp, event->x, event->y, &coord );
  if (vik_trw_layer_new_waypoint ( vtl, VIK_GTK_WINDOW_FROM_LAYER(vtl), &coord ) && VIK_LAYER(vtl)->visible)
    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
  return TRUE;
}


/*** Edit trackpoint ****/

static gpointer tool_edit_trackpoint_create ( VikWindow *vw, VikViewport *vvp)
{
  tool_ed_t *t = g_new(tool_ed_t, 1);
  t->vvp = vvp;
  t->holding = FALSE;
  return t;
}

static gboolean tool_edit_trackpoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;
  TPSearchParams params;
  /* OUTDATED DOCUMENTATION:
   find 5 pixel range on each side. then put these UTM, and a pointer
   to the winning track name (and maybe the winning track itself), and a
   pointer to the winning trackpoint, inside an array or struct. pass 
   this along, do a foreach on the tracks which will do a foreach on the 
   trackpoints. */
  params.vvp = vvp;
  params.x = event->x;
  params.y = event->y;
  params.closest_track_name = NULL;
  /* TODO: should get track listitem so we can break it up, make a new track, mess it up, all that. */
  params.closest_tp = NULL;

  if ( event->button != 1 ) 
    return FALSE;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( !vtl->vl.visible || !vtl->tracks_visible )
    return FALSE;

  if ( vtl->current_tpl )
  {
    /* first check if it is within range of prev. tp. and if current_tp track is shown. (if it is, we are moving that trackpoint.) */
    VikTrackpoint *tp = VIK_TRACKPOINT(vtl->current_tpl->data);
    VikTrack *current_tr = VIK_TRACK(g_hash_table_lookup(vtl->tracks, vtl->current_tp_track_name));
    gint x, y;
    g_assert ( current_tr );

    vik_viewport_coord_to_screen ( vvp, &(tp->coord), &x, &y );

    if ( current_tr->visible && 
         abs(x - event->x) < TRACKPOINT_SIZE_APPROX &&
         abs(y - event->y) < TRACKPOINT_SIZE_APPROX ) {
      marker_begin_move ( t, event->x, event->y );
      return TRUE;
    }

    vtl->last_tpl = vtl->current_tpl;
    vtl->last_tp_track_name = vtl->current_tp_track_name;
  }

  g_hash_table_foreach ( vtl->tracks, (GHFunc) track_search_closest_tp, &params);

  if ( params.closest_tp )
  {
    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->tracks_iters, params.closest_track_name ), TRUE );
    vtl->current_tpl = params.closest_tpl;
    vtl->current_tp_track_name = params.closest_track_name;
    trw_layer_tpwin_init ( vtl );
    set_statusbar_msg_info_trkpt ( vtl, params.closest_tp );
    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }

  /* these aren't the droids you're looking for */
  return FALSE;
}

static gboolean tool_edit_trackpoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( t->holding )
  {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( tp && tp != vtl->current_tpl->data )
        new_coord = tp->coord;
    }
    //    VIK_TRACKPOINT(vtl->current_tpl->data)->coord = new_coord;
    { 
      gint x, y;
      vik_viewport_coord_to_screen ( vvp, &new_coord, &x, &y );
      marker_moveto ( t, x, y );
    } 

    return TRUE;
  }
  return FALSE;
}

static gboolean tool_edit_trackpoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;
  if ( event->button != 1) 
    return FALSE;

  if ( t->holding ) {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_five_pixel_interval ( vtl, vvp, event->x, event->y );
      if ( tp && tp != vtl->current_tpl->data )
        new_coord = tp->coord;
    }

    VIK_TRACKPOINT(vtl->current_tpl->data)->coord = new_coord;

    marker_end_move ( t );

    /* diff dist is diff from orig */
    if ( vtl->tpwin )
      vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl, vtl->current_tp_track_name );
    /* can't join with itself! */
    trw_layer_cancel_last_tp ( vtl );

    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
    return TRUE;
  }
  return FALSE;
}


/*** Route Finder ***/
static gpointer tool_route_finder_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean tool_route_finder_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  VikCoord tmp;
  if ( !vtl ) return FALSE;
  vik_viewport_screen_to_coord ( vvp, event->x, event->y, &tmp );
  if ( event->button == 3 && vtl->route_finder_current_track ) {
    VikCoord *new_end;
    new_end = vik_track_cut_back_to_double_point ( vtl->route_finder_current_track );
    if ( new_end ) {
      vtl->route_finder_coord = *new_end;
      g_free ( new_end );
      vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
      /* remove last ' to:...' */
      if ( vtl->route_finder_current_track->comment ) {
        gchar *last_to = strrchr ( vtl->route_finder_current_track->comment, 't' );
        if ( last_to && (last_to - vtl->route_finder_current_track->comment > 1) ) {
          gchar *new_comment = g_strndup ( vtl->route_finder_current_track->comment,
                                           last_to - vtl->route_finder_current_track->comment - 1);
          vik_track_set_comment_no_copy ( vtl->route_finder_current_track, new_comment );
        }
      }
    }
  }
  else if ( vtl->route_finder_started || (event->state & GDK_CONTROL_MASK && vtl->route_finder_current_track) ) {
    struct LatLon start, end;
    gchar startlat[G_ASCII_DTOSTR_BUF_SIZE], startlon[G_ASCII_DTOSTR_BUF_SIZE];
    gchar endlat[G_ASCII_DTOSTR_BUF_SIZE], endlon[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *url;

    vik_coord_to_latlon ( &(vtl->route_finder_coord), &start );
    vik_coord_to_latlon ( &(tmp), &end );
    vtl->route_finder_coord = tmp; /* for continuations */

    /* these are checked when adding a track from a file (vik_trw_layer_filein_add_track) */
    if ( event->state & GDK_CONTROL_MASK && vtl->route_finder_current_track ) {
      vtl->route_finder_append = TRUE;  // merge tracks. keep started true.
    } else {
      vtl->route_finder_check_added_track = TRUE;
      vtl->route_finder_started = FALSE;
    }

    url = g_strdup_printf(GOOGLE_DIRECTIONS_STRING,
                          g_ascii_dtostr (startlat, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) start.lat),
                          g_ascii_dtostr (startlon, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) start.lon),
                          g_ascii_dtostr (endlat, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) end.lat),
                          g_ascii_dtostr (endlon, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) end.lon));
    a_babel_convert_from_url ( vtl, url, "kml", NULL, NULL );
    g_free ( url );

    /* see if anything was done -- a track was added or appended to */
    if ( vtl->route_finder_check_added_track && vtl->route_finder_added_track_name ) {
      VikTrack *tr;

      tr = g_hash_table_lookup ( vtl->tracks, vtl->route_finder_added_track_name );

      if ( tr )
        vik_track_set_comment_no_copy ( tr, g_strdup_printf("from: %f,%f to: %f,%f", start.lat, start.lon, end.lat, end.lon ) );
 
      vtl->route_finder_current_track = tr;

      g_free ( vtl->route_finder_added_track_name );
      vtl->route_finder_added_track_name = NULL;
    } else if ( vtl->route_finder_append == FALSE && vtl->route_finder_current_track ) {
      /* route_finder_append was originally TRUE but set to FALSE by filein_add_track */
      gchar *new_comment = g_strdup_printf("%s to: %f,%f", vtl->route_finder_current_track->comment, end.lat, end.lon );
      vik_track_set_comment_no_copy ( vtl->route_finder_current_track, new_comment );
    }
    vtl->route_finder_check_added_track = FALSE;
    vtl->route_finder_append = FALSE;

    vik_layer_emit_update ( VIK_LAYER(vtl), FALSE );
  } else {
    vtl->route_finder_started = TRUE;
    vtl->route_finder_coord = tmp;
    vtl->route_finder_current_track = NULL;
  }
  return TRUE;
}

/*** Show picture ****/

static gpointer tool_show_picture_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

/* Params are: vvp, event, last match found or NULL */
static void tool_show_picture_wp ( char *name, VikWaypoint *wp, gpointer params[3] )
{
  if ( wp->image && wp->visible )
  {
    gint x, y, slackx, slacky;
    GdkEventButton *event = (GdkEventButton *) params[1];

    vik_viewport_coord_to_screen ( VIK_VIEWPORT(params[0]), &(wp->coord), &x, &y );
    slackx = wp->image_width / 2;
    slacky = wp->image_height / 2;
    if (    x <= event->x + slackx && x >= event->x - slackx
         && y <= event->y + slacky && y >= event->y - slacky )
    {
      params[2] = wp->image; /* we've found a match. however continue searching
                              * since we want to find the last match -- that
                              * is, the match that was drawn last. */
    }
  }
}

static void trw_layer_show_picture ( gpointer pass_along[6] )
{
  /* thanks to the Gaim people for showing me ShellExecute and g_spawn_command_line_async */
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) pass_along[2], NULL, ".\\", 0);
#else /* WINDOWS */
  GError *err = NULL;
  gchar *quoted_file = g_shell_quote ( (gchar *) pass_along[5] );
  gchar *cmd = g_strdup_printf ( "%s %s", a_vik_get_image_viewer(), quoted_file );
  g_free ( quoted_file );
  if ( ! g_spawn_command_line_async ( cmd, &err ) )
    {
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER( pass_along[0]), _("Could not launch %s to open file."), a_vik_get_image_viewer() );
      g_error_free ( err );
    }
  g_free ( cmd );
#endif /* WINDOWS */
}

static gboolean tool_show_picture_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  gpointer params[3] = { vvp, event, NULL };
  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) tool_show_picture_wp, params );
  if ( params[2] )
  {
    static gpointer pass_along[6];
    pass_along[0] = vtl;
    pass_along[5] = params[2];
    trw_layer_show_picture ( pass_along );
    return TRUE; /* found a match */
  }
  else
    return FALSE; /* go through other layers, searching for a match */
}

/***************************************************************************
 ** End tool code 
 ***************************************************************************/





static void image_wp_make_list ( char *name, VikWaypoint *wp, GSList **pics )
{
  if ( wp->image && ( ! a_thumbnails_exists ( wp->image ) ) )
    *pics = g_slist_append ( *pics, (gpointer) g_strdup ( wp->image ) );
}

/* Structure for thumbnail creating data used in the background thread */
typedef struct {
  VikTrwLayer *vtl; // Layer needed for redrawing
  GSList *pics;     // Image list
} thumbnail_create_thread_data;

static int create_thumbnails_thread ( thumbnail_create_thread_data *tctd, gpointer threaddata )
{
  guint total = g_slist_length(tctd->pics), done = 0;
  while ( tctd->pics )
  {
    a_thumbnails_create ( (gchar *) tctd->pics->data );
    int result = a_background_thread_progress ( threaddata, ((gdouble) ++done) / total );
    if ( result != 0 )
      return -1; /* Abort thread */

    tctd->pics = tctd->pics->next;
  }

  // Redraw to show the thumbnails as they are now created
  if ( IS_VIK_LAYER(tctd->vtl) )
    vik_layer_emit_update ( VIK_LAYER(tctd->vtl), TRUE ); // Yes update from background thread

  return 0;
}

static void thumbnail_create_thread_free ( thumbnail_create_thread_data *tctd )
{
  while ( tctd->pics )
  {
    g_free ( tctd->pics->data );
    tctd->pics = g_slist_delete_link ( tctd->pics, tctd->pics );
  }
  g_free ( tctd );
}

void trw_layer_verify_thumbnails ( VikTrwLayer *vtl, GtkWidget *vp )
{
  if ( ! vtl->has_verified_thumbnails )
  {
    GSList *pics = NULL;
    g_hash_table_foreach ( vtl->waypoints, (GHFunc) image_wp_make_list, &pics );
    if ( pics )
    {
      gint len = g_slist_length ( pics );
      gchar *tmp = g_strdup_printf ( _("Creating %d Image Thumbnails..."), len );
      thumbnail_create_thread_data *tctd = g_malloc ( sizeof(thumbnail_create_thread_data) );
      tctd->vtl = vtl;
      tctd->pics = pics;
      a_background_thread ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    tmp,
			    (vik_thr_func) create_thumbnails_thread,
			    tctd,
			    (vik_thr_free_func) thumbnail_create_thread_free,
			    NULL,
			    len );
      g_free ( tmp );
    }
  }
}

VikCoordMode vik_trw_layer_get_coord_mode ( VikTrwLayer *vtl )
{
  return vtl->coord_mode;
}



static void waypoint_convert ( const gchar *name, VikWaypoint *wp, VikCoordMode *dest_mode )
{
  vik_coord_convert ( &(wp->coord), *dest_mode );
}

static void track_convert ( const gchar *name, VikTrack *tr, VikCoordMode *dest_mode )
{
  vik_track_convert ( tr, *dest_mode );
}

static void trw_layer_change_coord_mode ( VikTrwLayer *vtl, VikCoordMode dest_mode )
{
  if ( vtl->coord_mode != dest_mode )
  {
    vtl->coord_mode = dest_mode;
    g_hash_table_foreach ( vtl->waypoints, (GHFunc) waypoint_convert, &dest_mode );
    g_hash_table_foreach ( vtl->tracks, (GHFunc) track_convert, &dest_mode );
  }
}

VikWaypoint *vik_trw_layer_get_waypoint ( VikTrwLayer *vtl, gchar *name )
{
  return g_hash_table_lookup ( vtl->waypoints, name );
}

VikTrack *vik_trw_layer_get_track ( VikTrwLayer *vtl, const gchar *name )
{
  return g_hash_table_lookup ( vtl->tracks, name );
}

static void trw_layer_set_menu_selection ( VikTrwLayer *vtl, guint16 selection )
{
  vtl->menu_selection = selection;
}

static guint16 trw_layer_get_menu_selection ( VikTrwLayer *vtl )
{
  return (vtl->menu_selection);
}

/* ----------- Downloading maps along tracks --------------- */

static int get_download_area_width(VikViewport *vvp, gdouble zoom_level, struct LatLon *wh)
{
  /* TODO: calculating based on current size of viewport */
  const gdouble w_at_zoom_0_125 = 0.0013;
  const gdouble h_at_zoom_0_125 = 0.0011;
  gdouble zoom_factor = zoom_level/0.125;

  wh->lat = h_at_zoom_0_125 * zoom_factor;
  wh->lon = w_at_zoom_0_125 * zoom_factor;

  return 0;   /* all OK */
}

static VikCoord *get_next_coord(VikCoord *from, VikCoord *to, struct LatLon *dist, gdouble gradient)
{
  if ((dist->lon >= ABS(to->east_west - from->east_west)) &&
      (dist->lat >= ABS(to->north_south - from->north_south)))
    return NULL;

  VikCoord *coord = g_malloc(sizeof(VikCoord));
  coord->mode = VIK_COORD_LATLON;

  if (ABS(gradient) < 1) {
    if (from->east_west > to->east_west)
      coord->east_west = from->east_west - dist->lon;
    else
      coord->east_west = from->east_west + dist->lon;
    coord->north_south = gradient * (coord->east_west - from->east_west) + from->north_south;
  } else {
    if (from->north_south > to->north_south)
      coord->north_south = from->north_south - dist->lat;
    else
      coord->north_south = from->north_south + dist->lat;
    coord->east_west = (1/gradient) * (coord->north_south - from->north_south) + from->north_south;
  }

  return coord;
}

static GList *add_fillins(GList *list, VikCoord *from, VikCoord *to, struct LatLon *dist)
{
  /* TODO: handle virtical track (to->east_west - from->east_west == 0) */
  gdouble gradient = (to->north_south - from->north_south)/(to->east_west - from->east_west);

  VikCoord *next = from;
  while (TRUE) {
    if ((next = get_next_coord(next, to, dist, gradient)) == NULL)
        break;
    list = g_list_prepend(list, next);
  }

  return list;
}

void vik_track_download_map(VikTrack *tr, VikMapsLayer *vml, VikViewport *vvp, gdouble zoom_level)
{
  typedef struct _Rect {
    VikCoord tl;
    VikCoord br;
    VikCoord center;
  } Rect;
#define GLRECT(iter) ((Rect *)((iter)->data))

  struct LatLon wh;
  GList *rects_to_download = NULL;
  GList *rect_iter;

  if (get_download_area_width(vvp, zoom_level, &wh))
    return;

  GList *iter = tr->trackpoints;
  if (!iter)
    return;

  gboolean new_map = TRUE;
  VikCoord *cur_coord, tl, br;
  Rect *rect;
  while (iter) {
    cur_coord = &(VIK_TRACKPOINT(iter->data))->coord;
    if (new_map) {
      vik_coord_set_area(cur_coord, &wh, &tl, &br);
      rect = g_malloc(sizeof(Rect));
      rect->tl = tl;
      rect->br = br;
      rect->center = *cur_coord;
      rects_to_download = g_list_prepend(rects_to_download, rect);
      new_map = FALSE;
      iter = iter->next;
      continue;
    }
    gboolean found = FALSE;
    for (rect_iter = rects_to_download; rect_iter; rect_iter = rect_iter->next) {
      if (vik_coord_inside(cur_coord, &GLRECT(rect_iter)->tl, &GLRECT(rect_iter)->br)) {
        found = TRUE;
        break;
      }
    }
    if (found)
        iter = iter->next;
    else
      new_map = TRUE;
  }

  GList *fillins = NULL;
  /* 'fillin' doesn't work in UTM mode - potentially ending up in massive loop continually allocating memory - hence don't do it */
  /* seems that ATM the function get_next_coord works only for LATLON */
  if ( cur_coord->mode == VIK_COORD_LATLON ) {
    /* fill-ins for far apart points */
    GList *cur_rect, *next_rect;
    for (cur_rect = rects_to_download;
	 (next_rect = cur_rect->next) != NULL;
	 cur_rect = cur_rect->next) {
      if ((wh.lon < ABS(GLRECT(cur_rect)->center.east_west - GLRECT(next_rect)->center.east_west)) ||
	  (wh.lat < ABS(GLRECT(cur_rect)->center.north_south - GLRECT(next_rect)->center.north_south))) {
	fillins = add_fillins(fillins, &GLRECT(cur_rect)->center, &GLRECT(next_rect)->center, &wh);
      }
    }
  } else
    g_message("%s: this feature works only in Mercator mode", __FUNCTION__);

  if (fillins) {
    GList *iter = fillins;
    while (iter) {
      cur_coord = (VikCoord *)(iter->data);
      vik_coord_set_area(cur_coord, &wh, &tl, &br);
      rect = g_malloc(sizeof(Rect));
      rect->tl = tl;
      rect->br = br;
      rect->center = *cur_coord;
      rects_to_download = g_list_prepend(rects_to_download, rect);
      iter = iter->next;
    }
  }

  for (rect_iter = rects_to_download; rect_iter; rect_iter = rect_iter->next) {
    maps_layer_download_section_without_redraw(vml, vvp, &(((Rect *)(rect_iter->data))->tl), &(((Rect *)(rect_iter->data))->br), zoom_level);
  }

  if (fillins) {
    for (iter = fillins; iter; iter = iter->next)
      g_free(iter->data);
    g_list_free(fillins);
  }
  if (rects_to_download) {
    for (rect_iter = rects_to_download; rect_iter; rect_iter = rect_iter->next)
      g_free(rect_iter->data);
    g_list_free(rects_to_download);
  }
}

static void trw_layer_download_map_along_track_cb ( gpointer pass_along[6] )
{
  VikMapsLayer *vml;
  gint selected_map, default_map;
  gchar *zoomlist[] = {"0.125", "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", NULL };
  gdouble zoom_vals[] = {0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
  gint selected_zoom, default_zoom;
  int i,j;


  VikTrwLayer *vtl = pass_along[0];
  VikLayersPanel *vlp = pass_along[1];
  VikTrack *tr = (VikTrack *) g_hash_table_lookup ( VIK_TRW_LAYER(pass_along[0])->tracks, pass_along[3] );
  VikViewport *vvp = vik_window_viewport((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)));

  GList *vmls = vik_layers_panel_get_all_layers_of_type(vlp, VIK_LAYER_MAPS, TRUE); // Includes hidden map layer types
  int num_maps = g_list_length(vmls);

  if (!num_maps) {
    a_dialog_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), GTK_MESSAGE_ERROR, _("No map layer in use. Create one first"), NULL);
    return;
  }

  gchar **map_names = g_malloc(1 + num_maps * sizeof(gpointer));
  VikMapsLayer **map_layers = g_malloc(1 + num_maps * sizeof(gpointer));

  gchar **np = map_names;
  VikMapsLayer **lp = map_layers;
  for (i = 0; i < num_maps; i++) {
    gboolean dup = FALSE;
    vml = (VikMapsLayer *)(vmls->data);
    for (j = 0; j < i; j++) { /* no duplicate allowed */
      if (vik_maps_layer_get_map_type(vml) == vik_maps_layer_get_map_type(map_layers[j])) {
        dup = TRUE;
        break;
      }
    }
    if (!dup) {
      *lp++ = vml;
      *np++ = vik_maps_layer_get_map_label(vml);
    }
    vmls = vmls->next;
  }
  *lp = NULL;
  *np = NULL;
  num_maps = lp - map_layers;

  for (default_map = 0; default_map < num_maps; default_map++) {
    /* TODO: check for parent layer's visibility */
    if (VIK_LAYER(map_layers[default_map])->visible)
      break;
  }
  default_map = (default_map == num_maps) ? 0 : default_map;

  gdouble cur_zoom = vik_viewport_get_zoom(vvp);
  for (default_zoom = 0; default_zoom < sizeof(zoom_vals)/sizeof(gdouble); default_zoom++) {
    if (cur_zoom == zoom_vals[default_zoom])
      break;
  }
  default_zoom = (default_zoom == sizeof(zoom_vals)/sizeof(gdouble)) ? sizeof(zoom_vals)/sizeof(gdouble) - 1 : default_zoom;

  if (!a_dialog_map_n_zoom(VIK_GTK_WINDOW_FROM_LAYER(vtl), map_names, default_map, zoomlist, default_zoom, &selected_map, &selected_zoom))
    goto done;

  vik_track_download_map(tr, map_layers[selected_map], vvp, zoom_vals[selected_zoom]);

done:
  for (i = 0; i < num_maps; i++)
    g_free(map_names[i]);
  g_free(map_names);
  g_free(map_layers);

  g_list_free(vmls);

}

/**** lowest waypoint number calculation ***/
static gint highest_wp_number_name_to_number(const gchar *name) {
  if ( strlen(name) == 3 ) {
    int n = atoi(name);
    if ( n < 100 && name[0] != '0' )
      return -1;
    if ( n < 10 && name[0] != '0' )
      return -1;
    return n;
  }
  return -1;
}


static void highest_wp_number_reset(VikTrwLayer *vtl)
{
  vtl->highest_wp_number = -1;
}

static void highest_wp_number_add_wp(VikTrwLayer *vtl, const gchar *new_wp_name)
{
  /* if is bigger that top, add it */
  gint new_wp_num = highest_wp_number_name_to_number(new_wp_name);
  if ( new_wp_num > vtl->highest_wp_number )
    vtl->highest_wp_number = new_wp_num;
}

static void highest_wp_number_remove_wp(VikTrwLayer *vtl, const gchar *old_wp_name)
{
  /* if wasn't top, do nothing. if was top, count backwards until we find one used */
  gint old_wp_num = highest_wp_number_name_to_number(old_wp_name);
  if ( vtl->highest_wp_number == old_wp_num ) {
    gchar buf[4];
    vtl->highest_wp_number --;

    g_snprintf(buf,4,"%03d", vtl->highest_wp_number );
    /* search down until we find something that *does* exist */

    while ( vtl->highest_wp_number > 0 && ! g_hash_table_lookup ( vtl->waypoints, buf ) ) {
      vtl->highest_wp_number --;
      g_snprintf(buf,4,"%03d", vtl->highest_wp_number );
    }
  }
}

/* get lowest unused number */
static gchar *highest_wp_number_get(VikTrwLayer *vtl)
{
  gchar buf[4];
  if ( vtl->highest_wp_number < 0 || vtl->highest_wp_number >= 999 )
    return NULL;
  g_snprintf(buf,4,"%03d", vtl->highest_wp_number+1 );
  return g_strdup(buf);
}
