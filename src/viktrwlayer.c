/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
/* WARNING: If you go beyond this point, we are NOT responsible for any ill effects on your sanity */
/* viktrwlayer.c -- 8000+ lines can make a difference in the state of things */
#include "viking.h"
#include "vikmapslayer.h"
#include "vikgpslayer.h"
#include "viktrwlayer_export.h"
#include "viktrwlayer_tpwin.h"
#include "viktrwlayer_wpwin.h"
#include "viktrwlayer_propwin.h"
#include "viktrwlayer_analysis.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_waypointlist.h"
#ifdef VIK_CONFIG_GEOTAG
#include "viktrwlayer_geotag.h"
#include "geotag_exif.h"
#endif
#include "garminsymbols.h"
#include "thumbnails.h"
#include "background.h"
#include "gpx.h"
#include "geojson.h"
#include "babel.h"
#include "dem.h"
#include "dems.h"
#include "geonamessearch.h"
#ifdef VIK_CONFIG_OPENSTREETMAP
#include "osm-traces.h"
#endif
#include "acquire.h"
#include "datasources.h"
#include "datasource_gps.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "vikrouting.h"

#include <ctype.h>
#include <gdk/gdkkeysyms.h>

#define TRACKWAYPOINT_FIXED_NAME "TrackWaypoint"

#define VIK_TRW_LAYER_TRACK_GC 6
#define VIK_TRW_LAYER_TRACK_GCS 10
#define VIK_TRW_LAYER_TRACK_GC_BLACK 0
#define VIK_TRW_LAYER_TRACK_GC_SLOW 1
#define VIK_TRW_LAYER_TRACK_GC_AVER 2
#define VIK_TRW_LAYER_TRACK_GC_FAST 3
#define VIK_TRW_LAYER_TRACK_GC_STOP 4
#define VIK_TRW_LAYER_TRACK_GC_SINGLE 5

#define COLOR_STOP "#874200"
#define COLOR_SLOW "#E6202E" // red-ish
#define COLOR_AVER "#D2CD26" // yellow-ish
#define COLOR_FAST "#2B8700" // green-ish

#define DRAWMODE_BY_TRACK 0
#define DRAWMODE_BY_SPEED 1
#define DRAWMODE_ALL_SAME_COLOR 2
// Note using DRAWMODE_BY_SPEED may be slow especially for vast numbers of trackpoints
//  as we are (re)calculating the colour for every point

#define POINTS 1
#define LINES 2

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400
#define DRAW_ELEVATION_FACTOR 30 /* height of elevation plotting, sort of relative to zoom level ("mpp" that isn't mpp necessarily) */
                                 /* this is multiplied by user-inputted value from 1-100. */

enum { WP_SYMBOL_FILLED_SQUARE, WP_SYMBOL_SQUARE, WP_SYMBOL_CIRCLE, WP_SYMBOL_X, WP_NUM_SYMBOLS };

// See http://developer.gnome.org/pango/stable/PangoMarkupFormat.html
typedef enum {
  FS_XX_SMALL = 0, // 'xx-small'
  FS_X_SMALL,
  FS_SMALL,
  FS_MEDIUM, // DEFAULT
  FS_LARGE,
  FS_X_LARGE,
  FS_XX_LARGE,
  FS_NUM_SIZES
} font_size_t;

typedef enum {
  VIK_TRW_LAYER_INTERNAL = 0,
  VIK_TRW_LAYER_EXTERNAL,
  VIK_TRW_LAYER_EXTERNAL_NO_WRITE,
  VIK_EXTERNAL_TYPE_LAST
} trw_external_type_t;

struct _VikTrwLayer {
  VikLayer vl;
  GHashTable *tracks;
  GHashTable *tracks_iters;
  GHashTable *routes;
  GHashTable *routes_iters;
  GHashTable *waypoints_iters;
  GHashTable *waypoints;
  GtkTreeIter tracks_iter, routes_iter, waypoints_iter;
  gboolean tracks_visible, routes_visible, waypoints_visible;
  LatLonBBox waypoints_bbox;

  gboolean track_draw_labels;
  guint8 drawmode;
  guint8 drawpoints;
  guint8 drawpoints_size;
  guint8 drawelevation;
  guint8 elevation_factor;
  guint8 drawstops;
  guint32 stop_length;
  guint8 drawlines;
  guint8 drawdirections;
  guint8 drawdirections_size;
  guint8 line_thickness;
  guint8 bg_line_thickness;
  vik_layer_sort_order_t track_sort_order;
  gboolean auto_dem;
  gboolean auto_dedupl;
  gboolean prefer_gps_speed;

  // Metadata
  VikTRWMetadata *metadata;
  gpx_version_t gpx_version;
  gchar *gpx_header;
  gchar *gpx_extensions;

  PangoLayout *tracklabellayout;
  font_size_t track_font_size;
  gchar *track_fsize_str;

  guint8 wp_symbol;
  guint8 wp_size;
  gboolean wp_draw_symbols;
  font_size_t wp_font_size;
  gchar *wp_fsize_str;
  vik_layer_sort_order_t wp_sort_order;

  gdouble track_draw_speed_factor;

  GdkColor track_color;
  GdkColor track_bg_color;
  GdkColor waypoint_color;
  GdkColor waypoint_text_color;
  GdkColor waypoint_bg_color;
  GdkColor light_color; // -
  GdkColor dark_color;  // -- Mostly for track elevation
  GdkColor black_color;
  GdkColor slow_color;
  GdkColor aver_color;
  GdkColor fast_color;
  GdkColor stop_color;

  GArray *track_gc;
  GdkGC *track_1color_gc;
  GdkGC *current_track_gc;
  // Separate GC for a track's potential new point as drawn via separate method
  //  (compared to the actual track points drawn in the main trw_layer_draw_track function)
  GdkGC *current_track_newpoint_gc;
  GdkGC *track_bg_gc;
  GdkGC *waypoint_gc;
  GdkGC *waypoint_text_gc;
  GdkGC *waypoint_bg_gc;
  GdkGC *track_graph_point_gc;

  gboolean wpbgand;
  VikTrack *current_track; // ATM shared between new tracks and new routes
  guint16 ct_x1, ct_y1, ct_x2, ct_y2;
  gboolean draw_sync_done;
  gboolean draw_sync_do;
  GdkCursor *crosshair_cursor;

  VikCoordMode coord_mode;

  VikTrwLayerWpwin *wpwin;
  VikWaypoint *wpwin_wpt; // Similar to current_wp

  /* wp editing tool */
  VikWaypoint *current_wp;
  gpointer current_wp_id;
  gboolean moving_wp;
  gboolean waypoint_rightclick;

  /* track editing tool */
  GList *current_tpl;
  VikTrack *current_tp_track;
  VikTrwLayerTpwin *tpwin;

  /* track editing tool -- more specifically, moving tps */
  gboolean moving_tp;

  /* route finder tool */
  gboolean route_finder_check_added_track;
  VikTrack *route_finder_added_track;
  gboolean route_finder_append;
  VikCoord route_finder_request_coord;
  guint route_finder_timer_id;
  gboolean route_finder_end;

  gboolean drawlabels;
  gboolean drawimages;
  guint8 image_alpha;
  GHashTable *image_cache;
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

  // One per layer
  GtkWidget *tracks_analysis_dialog;

  trw_external_type_t external_layer;
  gchar *external_file;
  gboolean external_loaded;
  gchar *external_dirpath;
};

struct DrawingParams {
  VikViewport *vp;
  VikTrwLayer *vtl;
  VikWindow *vw;
  gdouble xmpp, ympp;
  guint16 width, height;
  gdouble cc; // Cosine factor in track directions
  gdouble ss; // Sine factor in track directions
  const VikCoord *center;
  gboolean one_zone, lat_lon;
  gdouble ce1, ce2, cn1, cn2;
  LatLonBBox bbox;
  gboolean highlight;
};

static gboolean trw_layer_delete_waypoint ( VikTrwLayer *vtl, VikWaypoint *wp );

typedef enum {
  MA_VTL = 0,
  MA_VLP,
  MA_SUBTYPE, // OR END for Layer only
  MA_SUBLAYER_ID,
  MA_CONFIRM,
  MA_VVP,
  MA_TV_ITER,
  MA_MISC,
  MA_LAST,
} menu_array_index;

typedef gpointer menu_array_layer[2];
typedef gpointer menu_array_sublayer[MA_LAST];

static void trw_layer_delete_item ( menu_array_sublayer values );
static void trw_layer_copy_item_cb ( menu_array_sublayer values );
static void trw_layer_cut_item_cb ( menu_array_sublayer values );

static void trw_layer_find_maxmin_tracks ( const gpointer id, const VikTrack *trk, struct LatLon maxmin[2] );
static void trw_layer_find_maxmin (VikTrwLayer *vtl, struct LatLon maxmin[2]);

static void trw_layer_edit_track_gcs ( VikTrwLayer *vtl, VikViewport *vp );
static void trw_layer_free_track_gcs ( VikTrwLayer *vtl );

static void trw_layer_draw_track_cb ( const gpointer id, VikTrack *track, struct DrawingParams *dp );
static void trw_layer_draw_waypoint ( const gpointer id, VikWaypoint *wp, struct DrawingParams *dp );

static void trw_layer_select_trackpoint ( VikTrwLayer *vtl, VikTrack *trk, VikTrackpoint *tpt, gboolean draw_graph_blob );
static void goto_coord ( gpointer *vlp, gpointer vvp, gpointer vl, const VikCoord *coord );
static void trw_layer_goto_track_startpoint ( menu_array_sublayer values );
static void trw_layer_goto_track_endpoint ( menu_array_sublayer values );
static void trw_layer_goto_track_max_speed ( menu_array_sublayer values );
static void trw_layer_goto_track_max_alt ( menu_array_sublayer values );
static void trw_layer_goto_track_min_alt ( menu_array_sublayer values );
static void trw_layer_goto_track_center ( menu_array_sublayer values );
static void trw_layer_goto_track_date ( menu_array_sublayer values );
static void trw_layer_goto_track_prev_point ( menu_array_sublayer values );
static void trw_layer_goto_track_next_point ( menu_array_sublayer values );
static void trw_layer_merge_by_segment ( menu_array_sublayer values );
static void trw_layer_merge_by_timestamp ( menu_array_sublayer values );
static void trw_layer_merge_with_other ( menu_array_sublayer values );
static void trw_layer_append_track ( menu_array_sublayer values );
static void trw_layer_split_by_timestamp ( menu_array_sublayer values );
static void trw_layer_split_by_n_points ( menu_array_sublayer values );
static void trw_layer_split_at_trackpoint ( menu_array_sublayer values );
static void trw_layer_split_segments ( menu_array_sublayer values );
static void trw_layer_delete_point_selected ( menu_array_sublayer values );
static void trw_layer_delete_points_same_position ( menu_array_sublayer values );
static void trw_layer_delete_points_same_time ( menu_array_sublayer values );
static void trw_layer_reverse ( menu_array_sublayer values );
static void trw_layer_download_map_along_track_cb ( menu_array_sublayer values );
static void trw_layer_edit_trackpoint ( menu_array_sublayer values );
static void trw_layer_show_picture ( menu_array_sublayer values );
static void trw_layer_gps_upload_any ( menu_array_sublayer values );

static void trw_layer_centerize ( menu_array_layer values );
static void trw_layer_auto_view ( menu_array_layer values );
static void trw_layer_goto_wp ( menu_array_layer values );
static void trw_layer_new_wp ( menu_array_layer values );
static void trw_layer_edit_track ( menu_array_layer values );
static void trw_layer_edit_route ( menu_array_layer values );
static void trw_layer_finish_track ( menu_array_layer values );
static void trw_layer_auto_waypoints_view ( menu_array_layer values );
static void trw_layer_auto_tracks_view ( menu_array_layer values );
static void trw_layer_delete_all_tracks ( menu_array_layer values );
static void trw_layer_delete_tracks_from_selection ( menu_array_layer values );
static void trw_layer_delete_all_waypoints ( menu_array_layer values );
static void trw_layer_delete_waypoints_from_selection ( menu_array_layer values );
static void trw_layer_delete_duplicate_waypoints ( menu_array_layer values );
static void trw_layer_new_wikipedia_wp_viewport ( menu_array_layer values );
static void trw_layer_new_wikipedia_wp_layer ( menu_array_layer values );
#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_geotagging_waypoint_mtime_keep ( menu_array_sublayer values );
static void trw_layer_geotagging_waypoint_mtime_update ( menu_array_sublayer values );
static void trw_layer_geotagging_track ( menu_array_sublayer values );
static void trw_layer_geotagging ( menu_array_layer values );
#endif
static void trw_layer_acquire_gps_cb ( menu_array_layer values );
static void trw_layer_acquire_routing_cb ( menu_array_layer values );
static void trw_layer_acquire_url_cb ( menu_array_layer values );
#ifdef VIK_CONFIG_OPENSTREETMAP
static void trw_layer_acquire_osm_cb ( menu_array_layer values );
static void trw_layer_acquire_osm_my_traces_cb ( menu_array_layer values );
#endif
#ifdef VIK_CONFIG_GEOCACHES
static void trw_layer_acquire_geocache_cb ( menu_array_layer values );
#endif
#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_acquire_geotagged_cb ( menu_array_layer values );
#endif
static void trw_layer_acquire_file_cb ( menu_array_layer values );
static void trw_layer_gps_upload ( menu_array_layer values );

static void trw_layer_track_list_dialog_single ( menu_array_sublayer values );
static void trw_layer_track_list_dialog ( menu_array_layer values );
static void trw_layer_waypoint_list_dialog ( menu_array_layer values );

// Specific route versions:
//  Most track handling functions can handle operating on the route list
//  However these ones are easier in separate functions
static void trw_layer_auto_routes_view ( menu_array_layer values );
static void trw_layer_delete_all_routes ( menu_array_layer values );
static void trw_layer_delete_routes_from_selection ( menu_array_layer values );

/* pop-up items */
static void trw_layer_properties_item ( gpointer pass_along[7] ); //TODO??
static void trw_layer_goto_waypoint ( menu_array_sublayer values );
static void trw_layer_waypoint_gc_webpage ( menu_array_sublayer values );
static void trw_layer_waypoint_webpage ( menu_array_sublayer values );

static void trw_layer_realize_waypoint ( gpointer id, VikWaypoint *wp, gpointer pass_along[5] );
static void trw_layer_realize_track ( gpointer id, VikTrack *track, gpointer pass_along[5] );

static void trw_layer_insert_tp_beside_current_tp ( VikTrwLayer *vtl, gboolean before, gboolean is_route );
static void trw_layer_cancel_current_tp ( VikTrwLayer *vtl, gboolean destroy );
static void trw_layer_tpwin_response ( VikTrwLayer *vtl, gint response );

static void trw_layer_sort_order_specified ( VikTrwLayer *vtl, guint sublayer_type, vik_layer_sort_order_t order );
static void trw_layer_sort_all ( VikTrwLayer *vtl );

static VikLayerToolFuncStatus tool_edit_trackpoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_trackpoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_trackpoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static gpointer tool_show_picture_create ( VikWindow *vw, VikViewport *vvp);
static VikLayerToolFuncStatus tool_show_picture_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp ); 
static VikLayerToolFuncStatus tool_edit_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_waypoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_waypoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_route_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_track_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_track_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data );
static VikLayerToolFuncStatus tool_edit_track_release ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp );
static gboolean tool_edit_track_key_press ( VikTrwLayer *vtl, GdkEventKey *event, gpointer data );
static gboolean tool_edit_track_key_release ( VikTrwLayer *vtl, GdkEventKey *event, gpointer data );
static gpointer tool_new_waypoint_create ( VikWindow *vw, VikViewport *vvp);
static VikLayerToolFuncStatus tool_new_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp );
static void tool_edit_track_deactivate ( VikTrwLayer *vtl, tool_ed_t *te );
static void tool_edit_route_finder_activate ( VikTrwLayer *vtl, tool_ed_t *te );
static VikLayerToolFuncStatus tool_extended_route_finder_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data );
static gboolean tool_extended_route_finder_key_press ( VikTrwLayer *vtl, GdkEventKey *event, gpointer data );
static gpointer tool_splitter_create ( VikWindow *vw, VikViewport *vvp);
static VikLayerToolFuncStatus tool_splitter_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp );

static VikTrackpoint *closest_tp_in_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y );
static VikWaypoint *closest_wp_in_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y );

static void waypoint_convert ( const gpointer id, VikWaypoint *wp, VikCoordMode *dest_mode );
static void track_convert ( const gpointer id, VikTrack *tr, VikCoordMode *dest_mode );

static gchar *highest_wp_number_get(VikTrwLayer *vtl);
static void highest_wp_number_reset(VikTrwLayer *vtl);
static void highest_wp_number_add_wp(VikTrwLayer *vtl, const gchar *new_wp_name);
static void highest_wp_number_remove_wp(VikTrwLayer *vtl, const gchar *old_wp_name);

// Note for the following tool GtkRadioActionEntry texts:
//  the very first text value is an internal name not displayed anywhere
//  the first N_ text value is the name used for menu entries - hence has an underscore for the keyboard accelerator
//    * remember not to clash with the values used for VikWindow level tools (Pan, Zoom, Ruler + Select)
//  the second N_ text value is used for the button tooltip (i.e. generally don't want an underscore here)
//  the value is always set to 0 and the tool loader in VikWindow will set the actual appropriate value used
static VikToolInterface trw_layer_tools[] = {
  { "addwp_18",
    { "CreateWaypoint", "addwp_18", N_("Create _Waypoint"), "<control><shift>W", N_("Create Waypoint"), 0 },
    (VikToolConstructorFunc) tool_new_waypoint_create,    NULL, NULL, NULL,
    (VikToolMouseFunc) tool_new_waypoint_click,    NULL, NULL,
    (VikToolKeyFunc) NULL,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, "cursor_addwp", NULL },

  // CreateTrack instead of EditTrack for backwards compatibility
  { "addtr_18",
    { "CreateTrack", "addtr_18", N_("Edit _Track"), "<control><shift>T", N_("Edit Track"), 0 },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    NULL,
    (VikToolActivationFunc) tool_edit_track_deactivate,
    (VikToolMouseFunc) tool_edit_track_click,
    (VikToolMouseMoveFunc) tool_edit_track_move,
    (VikToolMouseFunc) tool_edit_track_release,
    (VikToolKeyFunc) tool_edit_track_key_press,
    (VikToolKeyFunc) tool_edit_track_key_release,
    TRUE, // Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing
    GDK_CURSOR_IS_PIXMAP, "cursor_addtr", NULL },

  // CreateRoute instead of EditRoute for backwards compatibility
  { "vik_new_route_18",
    { "CreateRoute", "vik_new_route_18", N_("Edit _Route"), "<control><shift>B", N_("Edit Route"), 0 },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    NULL,
    (VikToolActivationFunc) tool_edit_track_deactivate,
    (VikToolMouseFunc) tool_edit_route_click,
    (VikToolMouseMoveFunc) tool_edit_track_move, // -\#
    (VikToolMouseFunc) tool_edit_track_release,  //   -> Reuse these track methods on a route
    (VikToolKeyFunc) tool_edit_track_key_press,  // -/#
    (VikToolKeyFunc) tool_edit_track_key_release,
    TRUE, // Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing
    GDK_CURSOR_IS_PIXMAP, "cursor_new_route", NULL },

  { "route_finder_18",
    { "ExtendedRouteFinder", "route_finder_18", N_("Route _Finder"), "<control><shift>F", N_("Route Finder"), 0 },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    (VikToolActivationFunc) tool_edit_route_finder_activate,
    (VikToolActivationFunc) tool_edit_track_deactivate,
    (VikToolMouseFunc) tool_extended_route_finder_click,
    (VikToolMouseMoveFunc) tool_edit_track_move, // -\#
    (VikToolMouseFunc) tool_edit_track_release,  //   -> Reuse these track methods on a route
    (VikToolKeyFunc) tool_extended_route_finder_key_press,
    (VikToolKeyFunc) tool_edit_track_key_release,
    TRUE, // Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing
    GDK_CURSOR_IS_PIXMAP, "cursor_route_finder", NULL },

  { "splitter_18",
    { "Splitter", "splitter_18", N_("Splitter"), "<control><shift>L", N_("Splitter"), 0 },
    (VikToolConstructorFunc) tool_splitter_create,  NULL, NULL, NULL,
    (VikToolMouseFunc) tool_splitter_click,
    (VikToolMouseMoveFunc) NULL,
    (VikToolMouseFunc) NULL,
    (VikToolKeyFunc) NULL,
    (VikToolKeyFunc) NULL,
    TRUE, // Still need to handle clicks when in PAN mode to disable the potential trackpoint drawing
    GDK_CURSOR_IS_PIXMAP, "cursor_splitter", NULL },

  { "edwp_18",
    { "EditWaypoint", "edwp_18", N_("_Edit Waypoint"), "<control><shift>E", N_("Edit Waypoint"), 0 },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    NULL, NULL,
    (VikToolMouseFunc) tool_edit_waypoint_click,   
    (VikToolMouseMoveFunc) tool_edit_waypoint_move,
    (VikToolMouseFunc) tool_edit_waypoint_release,
    (VikToolKeyFunc) NULL,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, "cursor_edwp", NULL },

  { "edtr_18",
    { "EditTrackpoint", "edtr_18", N_("Edit Trac_kpoint"), "<control><shift>K", N_("Edit Trackpoint"), 0 },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    NULL, NULL,
    (VikToolMouseFunc) tool_edit_trackpoint_click,
    (VikToolMouseMoveFunc) tool_edit_trackpoint_move,
    (VikToolMouseFunc) tool_edit_trackpoint_release,
    (VikToolKeyFunc) NULL,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, "cursor_edtr", NULL },

  { NULL, // a pixbuf for this one is already made globally available
    { "ShowPicture", VIK_ICON_SHOW_PICTURE, N_("Show P_icture"), "<control><shift>I", N_("Show Picture"), 0 },
    (VikToolConstructorFunc) tool_show_picture_create,    NULL, NULL, NULL,
    (VikToolMouseFunc) tool_show_picture_click,    NULL, NULL,
    (VikToolKeyFunc) NULL,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, "cursor_showpic", NULL },

};

enum {
  TOOL_CREATE_WAYPOINT=0,
  TOOL_CREATE_TRACK,
  TOOL_CREATE_ROUTE,
  TOOL_ROUTE_FINDER,
  TOOL_EDIT_WAYPOINT,
  TOOL_EDIT_TRACKPOINT,
  TOOL_SHOW_PICTURE,
  NUM_TOOLS
};

/****** PARAMETERS ******/

static gchar *params_groups[] = { N_("Waypoints"), N_("Tracks"), N_("Waypoint Images"), N_("Tracks Advanced"), N_("Metadata"), N_("Filesystem") };
enum { GROUP_WAYPOINTS, GROUP_TRACKS, GROUP_IMAGES, GROUP_TRACKS_ADV, GROUP_METADATA, GROUP_FILESYSTEM };

static gchar *params_drawmodes[] = { N_("Draw by Track"), N_("Draw by Speed"), N_("All Tracks Same Color"), NULL };
static gchar *params_wpsymbols[] = { N_("Filled Square"), N_("Square"), N_("Circle"), N_("X"), 0 };

#define MIN_POINT_SIZE 2
#define MAX_POINT_SIZE 10

#define MIN_ARROW_SIZE 3
#define MAX_ARROW_SIZE 20

static VikLayerParamScale params_scales[] = {
 /* min  max    step digits */
 {  1,   10,    1,   0 }, /* line_thickness */
 {  0,   100,   1,   0 }, /* track draw speed factor */
 {  1.0, 100.0, 1.0, 2 }, /* UNUSED */
                /* 5 * step == how much to turn */
 {  16,   128,  4,   0 }, // 3: image_size - NB step size ignored when an HSCALE used
 {   0,   255,  5,   0 }, // 4: image alpha -    "     "      "            "
 {   5,   500,  5,   0 }, // 5: image cache_size -     "      "
 {   0,   8,    1,   0 }, // 6: Background line thickness
 {   1,  64,    1,   0 }, /* wpsize */
 {   MIN_STOP_LENGTH, MAX_STOP_LENGTH, 1,   0 }, /* stop_length */
 {   1, 100, 1,   0 }, // 9: elevation factor
 {   MIN_POINT_SIZE,  MAX_POINT_SIZE,  1,   0 }, // 10: track point size
 {   MIN_ARROW_SIZE,  MAX_ARROW_SIZE,  1,   0 }, // 11: direction arrow size
};

static gchar* params_font_sizes[] = {
  N_("Extra Extra Small"),
  N_("Extra Small"),
  N_("Small"),
  N_("Medium"),
  N_("Large"),
  N_("Extra Large"),
  N_("Extra Extra Large"),
  NULL };

// Needs to align with vik_layer_sort_order_t
static gchar* params_sort_order[] = {
  N_("None"),
  N_("Name Ascending"),
  N_("Name Descending"),
  N_("Date Ascending"),
  N_("Date Descending"),
  N_("Number Ascending"),
  N_("Number Descending"),
  NULL
};

// NB Waypoints don't have 'Track' numbers
static gchar* params_sort_order_wp[] = {
  N_("None"),
  N_("Name Ascending"),
  N_("Name Descending"),
  N_("Date Ascending"),
  N_("Date Descending"),
  NULL
};

// Needs to align with trw_external_type_t
static gchar* params_external_type[] = {
  N_("No"),
  N_("Yes"),
  N_("No write"),
  NULL
};

static gchar* params_gpx_version[] = {
  N_("1.0"),
  N_("1.1"),
  NULL
};

static VikLayerParamData black_color_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#000000", &data.c ); return data; // Black
}
static VikLayerParamData drawmode_default ( void ) { return VIK_LPD_UINT ( DRAWMODE_BY_TRACK ); }
static VikLayerParamData line_thickness_default ( void ) { return VIK_LPD_UINT ( 1  * vik_viewport_get_scale(NULL) ); }
static VikLayerParamData trkpointsize_default ( void ) { return VIK_LPD_UINT ( MIN_POINT_SIZE * vik_viewport_get_scale (NULL) ); }
static VikLayerParamData trkdirectionsize_default ( void ) { return VIK_LPD_UINT ( 5 * vik_viewport_get_scale(NULL) ); }
static VikLayerParamData bg_line_thickness_default ( void ) { return VIK_LPD_UINT ( 0 ); }
static VikLayerParamData trackbgcolor_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#FFFFFF", &data.c ); return data; // White
}
static VikLayerParamData elevation_factor_default ( void ) { return VIK_LPD_UINT ( 30 ); }
static VikLayerParamData stop_length_default ( void ) { return VIK_LPD_UINT ( 60 ); }
static VikLayerParamData speed_factor_default ( void ) { return VIK_LPD_DOUBLE ( 30.0 ); }

static VikLayerParamData tnfontsize_default ( void )
{
  if ( vik_viewport_get_scale(NULL) < 2 )
    return VIK_LPD_UINT ( FS_MEDIUM );
  else
    return VIK_LPD_UINT ( FS_LARGE );
}

static VikLayerParamData wpfontsize_default ( void )
{
  if ( vik_viewport_get_scale(NULL) < 2 )
    return VIK_LPD_UINT ( FS_MEDIUM );
  else
    return VIK_LPD_UINT ( FS_LARGE );
}

static VikLayerParamData wptextcolor_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#FFFFFF", &data.c ); return data; // White
}
static VikLayerParamData wpbgcolor_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#8383C4", &data.c ); return data; // Kind of Blue
}
static VikLayerParamData wpsize_default ( void ) { return VIK_LPD_UINT ( 4 * vik_viewport_get_scale(NULL) );
}
static VikLayerParamData wpsymbol_default ( void ) { return VIK_LPD_UINT ( WP_SYMBOL_FILLED_SQUARE ); }

static VikLayerParamData image_size_default ( void ) { return VIK_LPD_UINT ( 64 * vik_viewport_get_scale(NULL) ); }
static VikLayerParamData image_alpha_default ( void ) { return VIK_LPD_UINT ( 255 * vik_viewport_get_scale(NULL) ); }
static VikLayerParamData image_cache_size_default ( void ) { return VIK_LPD_UINT ( 300 ); }

static VikLayerParamData sort_order_default ( void ) { return VIK_LPD_UINT ( 0 ); }

static VikLayerParamData string_default ( void )
{
  VikLayerParamData data;
  data.s = "";
  return data;
}

static VikLayerParamData external_layer_default ( void ) { return VIK_LPD_UINT ( VIK_TRW_LAYER_INTERNAL ); }

static VikLayerParamData gpx_version_default ( void ) { return VIK_LPD_UINT ( GPX_V1_1 ); }

static void reset_cb ( GtkWidget *widget, gpointer ptr )
{
  a_layer_defaults_reset_show ( TRACKWAYPOINT_FIXED_NAME, ptr, GROUP_TRACKS );
  a_layer_defaults_reset_show ( TRACKWAYPOINT_FIXED_NAME, ptr, GROUP_TRACKS_ADV );
  a_layer_defaults_reset_show ( TRACKWAYPOINT_FIXED_NAME, ptr, GROUP_WAYPOINTS );
  a_layer_defaults_reset_show ( TRACKWAYPOINT_FIXED_NAME, ptr, GROUP_IMAGES );
  a_layer_defaults_reset_show ( TRACKWAYPOINT_FIXED_NAME, ptr, GROUP_METADATA );
  a_layer_defaults_reset_show ( TRACKWAYPOINT_FIXED_NAME, ptr, GROUP_FILESYSTEM );
}

static VikLayerParamData reset_default ( void ) { return VIK_LPD_PTR(reset_cb); }

VikLayerParam trw_layer_params[] = {
  { VIK_LAYER_TRW, "tracks_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "waypoints_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "routes_visible", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },

  { VIK_LAYER_TRW, "trackdrawlabels", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Labels"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Note: the individual track controls what labels may be displayed"), vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackfontsize", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track Labels Font Size:"), VIK_LAYER_WIDGET_COMBOBOX, params_font_sizes, NULL, NULL, tnfontsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawmode", VIK_LAYER_PARAM_UINT, GROUP_TRACKS, N_("Track Drawing Mode:"), VIK_LAYER_WIDGET_COMBOBOX, params_drawmodes, NULL, NULL, drawmode_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackcolor", VIK_LAYER_PARAM_COLOR, GROUP_TRACKS, N_("All Tracks Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL,
    N_("The color used when 'All Tracks Same Color' drawing mode is selected"), black_color_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawlines", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Track Lines"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "line_thickness", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[0], NULL, NULL, line_thickness_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawdirections", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Track Direction"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "trkdirectionsize", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Direction Size:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[11], NULL, NULL, trkdirectionsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawpoints", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Trackpoints"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "trkpointsize", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Trackpoint Size:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[10], NULL, NULL, trkpointsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawelevation", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Elevation"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "elevation_factor", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Draw Elevation Height %:"), VIK_LAYER_WIDGET_HSCALE, &params_scales[9], NULL, NULL, elevation_factor_default, NULL, NULL },
  { VIK_LAYER_TRW, "drawstops", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS, N_("Draw Stops"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Whether to draw a marker when trackpoints are at the same position but over the minimum stop length apart in time"), vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "stop_length", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Min Stop Length (seconds):"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[8], NULL, NULL, stop_length_default, NULL, NULL },

  { VIK_LAYER_TRW, "bg_line_thickness", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track BG Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[6], NULL, NULL, bg_line_thickness_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackbgcolor", VIK_LAYER_PARAM_COLOR, GROUP_TRACKS_ADV, N_("Track Background Color"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, trackbgcolor_default, NULL, NULL },
  { VIK_LAYER_TRW, "speed_factor", VIK_LAYER_PARAM_DOUBLE, GROUP_TRACKS_ADV, N_("Draw by Speed Factor (%):"), VIK_LAYER_WIDGET_HSCALE, &params_scales[1], NULL,
    N_("The percentage factor away from the average speed determining the color used"), speed_factor_default, NULL, NULL },
  { VIK_LAYER_TRW, "tracksortorder", VIK_LAYER_PARAM_UINT, GROUP_TRACKS_ADV, N_("Track Sort Order:"), VIK_LAYER_WIDGET_COMBOBOX, params_sort_order, NULL, NULL, sort_order_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackautodem", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS_ADV, N_("Apply DEM Automatically"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Automatically apply DEM to trackpoints on file load"), vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "trackautodedupl", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS_ADV, N_("Remove Duplicate Trackpoints"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Automatically delete duplicate trackpoints on file load"), vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "preferGPSspeed", VIK_LAYER_PARAM_BOOLEAN, GROUP_TRACKS_ADV, N_("Use GPS Speed"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Use reported GPS speed values - particularly for maximum speed"), vik_lpd_true_default, NULL, NULL },

  { VIK_LAYER_TRW, "drawlabels", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Draw Labels"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpfontsize", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint Font Size:"), VIK_LAYER_WIDGET_COMBOBOX, params_font_sizes, NULL, NULL, wpfontsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Waypoint Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, black_color_default, NULL, NULL },
  { VIK_LAYER_TRW, "wptextcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Waypoint Text:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, wptextcolor_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpbgcolor", VIK_LAYER_PARAM_COLOR, GROUP_WAYPOINTS, N_("Background:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, wpbgcolor_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpbgand", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Fake BG Color Translucency:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsymbol", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint marker:"), VIK_LAYER_WIDGET_COMBOBOX, params_wpsymbols, NULL, NULL, wpsymbol_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsize", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint size:"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[7], NULL, NULL, wpsize_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsyms", VIK_LAYER_PARAM_BOOLEAN, GROUP_WAYPOINTS, N_("Draw Waypoint Symbols:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "wpsortorder", VIK_LAYER_PARAM_UINT, GROUP_WAYPOINTS, N_("Waypoint Sort Order:"), VIK_LAYER_WIDGET_COMBOBOX, params_sort_order_wp, NULL, NULL, sort_order_default, NULL, NULL },

  { VIK_LAYER_TRW, "drawimages", VIK_LAYER_PARAM_BOOLEAN, GROUP_IMAGES, N_("Draw Waypoint Images"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_TRW, "image_size", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Size (pixels):"), VIK_LAYER_WIDGET_HSCALE, &params_scales[3], NULL, NULL, image_size_default, NULL, NULL },
  { VIK_LAYER_TRW, "image_alpha", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Alpha:"), VIK_LAYER_WIDGET_HSCALE, &params_scales[4], NULL, NULL, image_alpha_default, NULL, NULL },
  { VIK_LAYER_TRW, "image_cache_size", VIK_LAYER_PARAM_UINT, GROUP_IMAGES, N_("Image Memory Cache Size:"), VIK_LAYER_WIDGET_HSCALE, &params_scales[5], NULL, NULL, image_cache_size_default, NULL, NULL },

  { VIK_LAYER_TRW, "metadatadesc", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Description"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
  { VIK_LAYER_TRW, "metadataauthor", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Author"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
  { VIK_LAYER_TRW, "metadatatime", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Creation Time"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
  { VIK_LAYER_TRW, "metadatakeywords", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("Keywords"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, string_default, NULL, NULL },
  { VIK_LAYER_TRW, "metadataurl", VIK_LAYER_PARAM_STRING, GROUP_METADATA, N_("URL"), VIK_LAYER_WIDGET_ENTRY_URL, NULL, NULL, NULL, string_default, NULL, NULL },

  { VIK_LAYER_TRW, "gpx_version_enum", VIK_LAYER_PARAM_UINT, GROUP_FILESYSTEM, N_("GPX Version"), VIK_LAYER_WIDGET_COMBOBOX, params_gpx_version, NULL, NULL, gpx_version_default, NULL, NULL },
  { VIK_LAYER_TRW, "external_layer", VIK_LAYER_PARAM_UINT, GROUP_FILESYSTEM, N_("External layer:"), VIK_LAYER_WIDGET_COMBOBOX, params_external_type, NULL, N_("Layer data stored in the Viking file, in an external file, or in an external file but changes are not written to the file (file only loaded at startup)"), external_layer_default, NULL, NULL },
  { VIK_LAYER_TRW, "external_file", VIK_LAYER_PARAM_STRING, GROUP_FILESYSTEM, N_("Save layer as:"), VIK_LAYER_WIDGET_FILESAVE, GINT_TO_POINTER(VF_FILTER_GPX), NULL, N_("Specify where layer should be saved.  Overwrites file if it exists."), string_default, NULL, NULL },
  { VIK_LAYER_TRW, "reset", VIK_LAYER_PARAM_PTR_DEFAULT, VIK_LAYER_GROUP_NONE, NULL,
    VIK_LAYER_WIDGET_BUTTON, N_("Reset to Defaults"), NULL, NULL, reset_default, NULL, NULL },
};

// ENUMERATION MUST BE IN THE SAME ORDER AS THE NAMED PARAMS ABOVE
enum {
  // Sublayer visibilities
  PARAM_TV,
  PARAM_WV,
  PARAM_RV,
  // Tracks
  PARAM_TDL,
  PARAM_TLFONTSIZE,
  PARAM_DM,
  PARAM_TC,
  PARAM_DL,
  PARAM_LT,
  PARAM_DD,
  PARAM_DDS,
  PARAM_DP,
  PARAM_DPS,
  PARAM_DE,
  PARAM_EF,
  PARAM_DS,
  PARAM_SL,
  PARAM_BLT,
  PARAM_TBGC,
  PARAM_TDSF,
  PARAM_TSO,
  PARAM_TADEM,
  PARAM_TRDUP,
  PARAM_PGS,
  // Waypoints
  PARAM_DLA,
  PARAM_WPFONTSIZE,
  PARAM_WPC,
  PARAM_WPTC,
  PARAM_WPBC,
  PARAM_WPBA,
  PARAM_WPSYM,
  PARAM_WPSIZE,
  PARAM_WPSYMS,
  PARAM_WPSO,
  // WP images
  PARAM_DI,
  PARAM_IS,
  PARAM_IA,
  PARAM_ICS,
  // Metadata
  PARAM_MDDESC,
  PARAM_MDAUTH,
  PARAM_MDTIME,
  PARAM_MDKEYS,
  PARAM_MDURL,
  // Filesystem
  PARAM_GPXV,
  PARAM_EXTL,
  PARAM_EXTF,
  PARAM_RESET,
  NUM_PARAMS
};

/*** TO ADD A PARAM:
 *** 1) Add to trw_layer_params and enumeration
 *** 2) Handle in get_param & set_param (presumably adding on to VikTrwLayer struct)
 ***/

/****** END PARAMETERS ******/

/* Layer Interface function definitions */
static VikTrwLayer* trw_layer_create ( VikViewport *vp );
static void trw_layer_realize ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter );
static void trw_layer_post_read ( VikTrwLayer *vtl, VikViewport *vvp, gboolean from_file );
static void trw_layer_free ( VikTrwLayer *trwlayer );
static void trw_layer_draw ( VikTrwLayer *l, VikViewport *vvp );
static void trw_layer_configure ( VikTrwLayer *l, VikViewport *vvp );
static void trw_layer_change_coord_mode ( VikTrwLayer *vtl, VikCoordMode dest_mode );
static gdouble trw_layer_get_timestamp ( VikTrwLayer *vtl );
static void trw_layer_set_menu_selection ( VikTrwLayer *vtl, guint16 );
static guint16 trw_layer_get_menu_selection ( VikTrwLayer *vtl );
static void trw_layer_add_menu_items ( VikTrwLayer *vtl, GtkMenu *menu, gpointer vlp );
static gboolean trw_layer_sublayer_add_menu_items ( VikTrwLayer *l, GtkMenu *menu, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter, VikViewport *vvp );
static const gchar* trw_layer_sublayer_rename_request ( VikTrwLayer *l, const gchar *newname, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter );
static gboolean trw_layer_sublayer_toggle_visible ( VikTrwLayer *l, gint subtype, gpointer sublayer );
static const gchar* trw_layer_layer_tooltip ( VikTrwLayer *vtl );
static const gchar* trw_layer_sublayer_tooltip ( VikTrwLayer *l, gint subtype, gpointer sublayer );
static gboolean trw_layer_selected ( VikTrwLayer *l, gint subtype, gpointer sublayer, gint type, gpointer vlp );
static void trw_layer_layer_toggle_visible ( VikTrwLayer *vtl );
static void trw_layer_marshall ( VikTrwLayer *vtl, guint8 **data, guint *len );
static VikTrwLayer *trw_layer_unmarshall ( const guint8 *data_in, guint len, VikViewport *vvp );
static gboolean trw_layer_set_param ( VikTrwLayer *vtl, VikLayerSetParam *vlsp );
static VikLayerParamData trw_layer_get_param ( VikTrwLayer *vtl, guint16 id, gboolean is_file_operation );
static void trw_layer_change_param ( GtkWidget *widget, ui_change_values values );
static void trw_layer_del_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer );
static void trw_layer_cut_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer );
static void trw_layer_copy_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer, guint8 **item, guint *len );
static gboolean trw_layer_paste_item ( VikTrwLayer *vtl, gint subtype, guint8 *item, guint len );
static void trw_layer_free_copied_item ( gint subtype, gpointer item );
static void trw_layer_drag_drop_request ( VikTrwLayer *vtl_src, VikTrwLayer *vtl_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path );
static gboolean trw_layer_select_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t *t );
static gboolean trw_layer_select_move ( VikTrwLayer *vtl, GdkEventMotion *event, VikViewport *vvp, tool_ed_t *t );
static gboolean trw_layer_select_release ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, tool_ed_t *t );
static gboolean trw_layer_show_selected_viewport_menu ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp );
static void trw_write_file ( VikTrwLayer *trw, FILE *f, const gchar *dirpath );
static gboolean trw_read_file ( VikTrwLayer *trw, FILE *f, const gchar *dirpath );
static void trw_write_file_external ( VikTrwLayer *trw, FILE *f, const gchar *dirpath );
static gboolean trw_read_file_external ( VikTrwLayer *trw, FILE *f, const gchar *dirpath );
static gboolean trw_load_external_layer ( VikTrwLayer *trw );
static void trw_update_layer_icon ( VikTrwLayer *trw );

/* End Layer Interface function definitions */

VikLayerInterface vik_trw_layer_interface = {
  TRACKWAYPOINT_FIXED_NAME,
  N_("TrackWaypoint"),
  "<control><shift>Y",
  "viktrwlayer", // Icon name

  trw_layer_tools,
  G_N_ELEMENTS(trw_layer_tools),

  trw_layer_params,
  NUM_PARAMS,
  params_groups, /* params_groups */
  G_N_ELEMENTS(params_groups),    // number of groups

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  trw_layer_create,
  (VikLayerFuncRealize)                 trw_layer_realize,
  (VikLayerFuncPostRead)                trw_layer_post_read,
  (VikLayerFuncFree)                    trw_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    trw_layer_draw,
  (VikLayerFuncConfigure)               trw_layer_configure,
  (VikLayerFuncChangeCoordMode)         trw_layer_change_coord_mode,
  (VikLayerFuncGetTimestamp)            trw_layer_get_timestamp,

  (VikLayerFuncSetMenuItemsSelection)   trw_layer_set_menu_selection,
  (VikLayerFuncGetMenuItemsSelection)   trw_layer_get_menu_selection,

  (VikLayerFuncAddMenuItems)            trw_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    trw_layer_sublayer_add_menu_items,

  (VikLayerFuncSublayerRenameRequest)   trw_layer_sublayer_rename_request,
  (VikLayerFuncSublayerToggleVisible)   trw_layer_sublayer_toggle_visible,
  (VikLayerFuncSublayerTooltip)         trw_layer_sublayer_tooltip,
  (VikLayerFuncLayerTooltip)            trw_layer_layer_tooltip,
  (VikLayerFuncLayerSelected)           trw_layer_selected,
  (VikLayerFuncLayerToggleVisible)      trw_layer_layer_toggle_visible,

  (VikLayerFuncMarshall)                trw_layer_marshall,
  (VikLayerFuncUnmarshall)              trw_layer_unmarshall,

  (VikLayerFuncSetParam)                trw_layer_set_param,
  (VikLayerFuncGetParam)                trw_layer_get_param,
  (VikLayerFuncChangeParam)             trw_layer_change_param,

  (VikLayerFuncReadFileData)            trw_read_file,
  (VikLayerFuncWriteFileData)           trw_write_file,

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

  (VikLayerFuncRefresh)                 vik_trw_layer_propwin_main_refresh,
};

static gboolean have_diary_program = FALSE;
static gchar *diary_program = NULL;
#define VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM "external_diary_program"

static gboolean have_geojson_export = FALSE;

static gboolean have_astro_program = FALSE;
static gchar *astro_program = NULL;
#define VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM "external_astro_program"

static gboolean have_text_program = FALSE;
static gchar *text_program = NULL;
#define VIK_SETTINGS_EXTERNAL_TEXT_PROGRAM "external_text_program"

// NB Only performed once per program run
static void vik_trwlayer_class_init ( VikTrwLayerClass *klass )
{
  if ( ! a_settings_get_string ( VIK_SETTINGS_EXTERNAL_DIARY_PROGRAM, &diary_program ) ) {
#ifdef WINDOWS
    //diary_program = g_strdup ( "C:\\Program Files\\Rednotebook\\rednotebook.exe" );
    diary_program = g_strdup ( "C:/Progra~1/Rednotebook/rednotebook.exe" );
#else
    diary_program = g_strdup ( "rednotebook" );
#endif
  }
  else {
    // User specified so assume it works
    have_diary_program = TRUE;
  }

  gchar *dp = g_find_program_in_path ( diary_program );
  if ( dp ) {
    g_free ( dp );
    gchar *mystdout = NULL;
    gchar *mystderr = NULL;
    // Needs RedNotebook 1.7.3+ for support of opening on a specified date
    gchar *cmd = g_strconcat ( diary_program, " --version", NULL ); // "rednotebook --version"
    if ( g_spawn_command_line_sync ( cmd, &mystdout, &mystderr, NULL, NULL ) ) {
      // Annoyingly 1.7.1|2|3 versions of RedNotebook prints the version to stderr!!
      if ( mystdout )
        g_debug ("Diary: %s", mystdout ); // Should be something like 'RedNotebook 1.4'
      if ( mystderr )
        g_warning ("Diary: stderr: %s", mystderr );

      gchar **tokens = NULL;
      if ( mystdout && g_strcmp0(mystdout, "") )
        tokens = g_strsplit(mystdout, " ", 0);
      else if ( mystderr )
        tokens = g_strsplit(mystderr, " ", 0);

      if ( tokens ) {
        gint num = 0;
        gchar *token = tokens[num];
        while ( token && num < 2 ) {
          if (num == 1) {
            if ( viking_version_to_number(token) >= viking_version_to_number("1.7.3") )
              have_diary_program = TRUE;
          }
          num++;
          token = tokens[num];
        }
      }
      g_strfreev ( tokens );
    }
    g_free ( mystdout );
    g_free ( mystderr );
    g_free ( cmd );
  }

  gchar* geojson_prog = g_find_program_in_path ( a_geojson_program_export() );
  if ( geojson_prog ) {
    have_geojson_export = TRUE;
    g_free ( geojson_prog );
  }

  // Astronomy Domain
  if ( ! a_settings_get_string ( VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM, &astro_program ) ) {
#ifdef WINDOWS
    //astro_program = g_strdup ( "C:\\Program Files\\Stellarium\\stellarium.exe" );
    astro_program = g_strdup ( "C:/Progra~1/Stellarium/stellarium.exe" );
#else
    astro_program = g_strdup ( "stellarium" );
#endif
  }
  else {
    // User specified so assume it works
    have_astro_program = TRUE;
  }
  gchar *ap = g_find_program_in_path ( astro_program );
  if ( ap ) {
    g_free ( ap );
    have_astro_program = TRUE;
  }

  // NB don't use xdg-open by default,
  //  otherwise can end up opening back in a new instance of Viking!
  if ( ! a_settings_get_string ( VIK_SETTINGS_EXTERNAL_TEXT_PROGRAM, &text_program ) ) {
#ifdef WINDOWS
    text_program = g_strdup ( "notepad" );
#else
    text_program = g_strdup ( "gedit" );
#endif
    gchar *tp = g_find_program_in_path ( text_program );
    if ( tp ) {
      g_free ( tp );
      have_text_program = TRUE;
    }
  }
  else {
    // User specified so assume it works
    have_text_program = TRUE;
  }
}

/**
 * Can't use GClassFinalizeFunc, since VikTrwLayer is a static type
 *  Thus have to manually perform cleanup for anything done in vik_trwlayer_class_init
 */
void vik_trwlayer_uninit ()
{
  g_free ( diary_program );
  g_free ( astro_program );
  g_free ( text_program );
  // Might as well do this, as only used by this layer
  a_garmin_icons_uninit();
}

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
      (GClassInitFunc) vik_trwlayer_class_init, /* class init */
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

VikTRWMetadata *vik_trw_metadata_new()
{
  return (VikTRWMetadata*)g_malloc0(sizeof(VikTRWMetadata));
}

void vik_trw_metadata_free ( VikTRWMetadata *metadata)
{
  g_free (metadata->description);
  g_free (metadata->author);
  g_free (metadata->timestamp);
  g_free (metadata->keywords);
  g_free (metadata->url);
  g_free (metadata);
}

VikTRWMetadata *vik_trw_layer_get_metadata ( VikTrwLayer *vtl )
{
  return vtl->metadata;
}

void vik_trw_layer_set_metadata ( VikTrwLayer *vtl, VikTRWMetadata *metadata )
{
  if ( vtl->metadata )
    vik_trw_metadata_free ( vtl->metadata );
  vtl->metadata = metadata;
}

gpx_version_t vik_trw_layer_get_gpx_version ( VikTrwLayer *vtl )
{
  return vtl->gpx_version;
}

void vik_trw_layer_set_gpx_version ( VikTrwLayer *vtl, gpx_version_t value )
{
  vtl->gpx_version = value;
}

gchar *vik_trw_layer_get_gpx_header ( VikTrwLayer *vtl )
{
  return vtl->gpx_header;
}

void vik_trw_layer_set_gpx_header ( VikTrwLayer *vtl, gchar* value )
{
  vtl->gpx_header = value;
}

gchar *vik_trw_layer_get_gpx_extensions ( VikTrwLayer *vtl )
{
  return vtl->gpx_extensions;
}

void vik_trw_layer_set_gpx_extensions ( VikTrwLayer *vtl, gchar *value)
{
  if ( vtl->gpx_extensions )
    g_free ( vtl->gpx_extensions );
  vtl->gpx_extensions = g_strdup ( value );
}


typedef struct {
  gboolean found;
  const gchar *date_str;
  const VikTrack *trk;
  const VikWaypoint *wpt;
  gpointer trk_id;
  gpointer wpt_id;
} date_finder_type;

static gboolean trw_layer_find_date_track ( const gpointer id, const VikTrack *trk, date_finder_type *df )
{
  gchar date_buf[20];
  date_buf[0] = '\0';
  // Might be an easier way to compare dates rather than converting the strings all the time...
  if ( trk->trackpoints && !isnan(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp) ) {
    time_t time = round(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp);
    strftime (date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&time));

    if ( ! g_strcmp0 ( df->date_str, date_buf ) ) {
      df->found = TRUE;
      df->trk = trk;
      df->trk_id = id;
    }
  }
  return df->found;
}

static gboolean trw_layer_find_date_waypoint ( const gpointer id, const VikWaypoint *wpt, date_finder_type *df )
{
  gchar date_buf[20];
  date_buf[0] = '\0';
  // Might be an easier way to compare dates rather than converting the strings all the time...
  if ( !isnan(wpt->timestamp) ) {
    time_t time = round ( wpt->timestamp );
    strftime (date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&time));

    if ( ! g_strcmp0 ( df->date_str, date_buf ) ) {
      df->found = TRUE;
      df->wpt = wpt;
      df->wpt_id = id;
    }
  }
  return df->found;
}

/**
 * Find an item by date
 */
gboolean vik_trw_layer_find_date ( VikTrwLayer *vtl, const gchar *date_str, VikCoord *position, VikViewport *vvp, gboolean do_tracks, gboolean select )
{
  date_finder_type df;
  df.found = FALSE;
  df.date_str = date_str;
  df.trk = NULL;
  df.wpt = NULL;
  // Only tracks ATM
  if ( do_tracks )
    g_hash_table_find ( vtl->tracks, (GHRFunc) trw_layer_find_date_track, &df );
  else
    g_hash_table_find ( vtl->waypoints, (GHRFunc) trw_layer_find_date_waypoint, &df );

  if ( select && df.found ) {
    if ( do_tracks && df.trk ) {
      struct LatLon maxmin[2] = { {0,0}, {0,0} };
      trw_layer_find_maxmin_tracks ( NULL, df.trk, maxmin );
      trw_layer_zoom_to_show_latlons ( vtl, vvp, maxmin );
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup (vtl->tracks_iters, df.trk_id), TRUE );
    }
    else if ( df.wpt ) {
      vik_viewport_set_center_coord ( vvp, &(df.wpt->coord), TRUE );
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup (vtl->waypoints_iters, df.wpt_id), TRUE );
    }
    vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
  return df.found;
}

static void trw_layer_del_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer )
{
  static menu_array_sublayer values;
  if (!sublayer) {
    return;
  }

  gint ii;
  for ( ii = MA_VTL; ii < MA_LAST; ii++ )
    values[ii] = NULL;

  values[MA_VTL]         = vtl;
  values[MA_SUBTYPE]     = GINT_TO_POINTER (subtype);
  values[MA_SUBLAYER_ID] = sublayer;
  values[MA_CONFIRM]     = GINT_TO_POINTER (1); // Confirm delete request

  trw_layer_delete_item ( values );
}

static void trw_layer_cut_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer )
{
  static menu_array_sublayer values;
  if (!sublayer) {
    return;
  }

  gint ii;
  for ( ii = MA_VTL; ii < MA_LAST; ii++ )
    values[ii] = NULL;

  values[MA_VTL]         = vtl;
  values[MA_SUBTYPE]     = GINT_TO_POINTER (subtype);
  values[MA_SUBLAYER_ID] = sublayer;
  values[MA_CONFIRM]     = GINT_TO_POINTER (1); // Confirm delete request

  trw_layer_copy_item_cb(values);
  trw_layer_cut_item_cb(values);
}

static void trw_layer_copy_item_cb ( menu_array_sublayer values)
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gint subtype = GPOINTER_TO_INT (values[MA_SUBTYPE]);
  gpointer sublayer = values[MA_SUBLAYER_ID];
  guint8 *data = NULL;
  guint len;

  trw_layer_copy_item( vtl, subtype, sublayer, &data, &len);

  if (data) {
    const gchar* name;
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
      VikWaypoint *wp = g_hash_table_lookup ( vtl->waypoints, sublayer);
      if ( wp && wp->name )
        name = wp->name;
      else
        name = NULL; // Broken :(
    }
    else if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      VikTrack *trk = g_hash_table_lookup ( vtl->tracks, sublayer);
      if ( trk && trk->name )
        name = trk->name;
      else
        name = NULL; // Broken :(
    }
    else {
      VikTrack *trk = g_hash_table_lookup ( vtl->routes, sublayer);
      if ( trk && trk->name )
        name = trk->name;
      else
        name = NULL; // Broken :(
    }

    a_clipboard_copy( VIK_CLIPBOARD_DATA_SUBLAYER, VIK_LAYER_TRW,
		      subtype, len, name, data);
  }
}

static void trw_layer_cut_item_cb ( menu_array_sublayer values)
{
  trw_layer_copy_item_cb(values);
  values[MA_CONFIRM] = GINT_TO_POINTER (0); // Never need to confirm automatic delete
  trw_layer_delete_item(values);
}

static void trw_layer_paste_item_cb ( menu_array_sublayer values)
{
  // Slightly cheating method, routing via the panels capability
  a_clipboard_paste (VIK_LAYERS_PANEL(values[MA_VLP]));
}

static void trw_layer_copy_item ( VikTrwLayer *vtl, gint subtype, gpointer sublayer, guint8 **item, guint *len )
{
  guint8 *id;
  guint il;

  if (!sublayer) {
    *item = NULL;
    return;
  }

  GByteArray *ba = g_byte_array_new ();

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    vik_waypoint_marshall ( g_hash_table_lookup ( vtl->waypoints, sublayer ), &id, &il );
  } else if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
    vik_track_marshall ( g_hash_table_lookup ( vtl->tracks, sublayer ), &id, &il );
  } else {
    vik_track_marshall ( g_hash_table_lookup ( vtl->routes, sublayer ), &id, &il );
  }

  g_byte_array_append ( ba, id, il );

  g_free(id);

  *len = ba->len;
  *item = ba->data;
}

static gboolean trw_layer_paste_item ( VikTrwLayer *vtl, gint subtype, guint8 *item, guint len )
{
  if ( !item )
    return FALSE;

  gchar *name;

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    VikWaypoint *w;

    w = vik_waypoint_unmarshall ( item, len );
    // When copying - we'll create a new name based on the original
    name = trw_layer_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, w->name);
    vik_trw_layer_add_waypoint ( vtl, name, w );
    waypoint_convert (NULL, w, &vtl->coord_mode);
    g_free ( name );

    trw_layer_calculate_bounds_waypoints ( vtl );

    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->waypoints_visible && w->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    return TRUE;
  }
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    VikTrack *t;

    t = vik_track_unmarshall ( item, len );
    // When copying - we'll create a new name based on the original
    name = trw_layer_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, t->name);
    vik_trw_layer_add_track ( vtl, name, t );
    vik_track_convert (t, vtl->coord_mode);
    g_free ( name );

    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->tracks_visible && t->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    return TRUE;
  }
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    VikTrack *t;

    t = vik_track_unmarshall ( item, len );
    // When copying - we'll create a new name based on the original
    name = trw_layer_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_ROUTE, t->name);
    vik_trw_layer_add_route ( vtl, name, t );
    vik_track_convert (t, vtl->coord_mode);
    g_free ( name );

    // Consider if redraw necessary for the new item
    if ( vtl->vl.visible && vtl->routes_visible && t->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
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

static gboolean trw_layer_set_param ( VikTrwLayer *vtl, VikLayerSetParam *vlsp )
{
  switch ( vlsp->id )
  {
    case PARAM_TV: vtl->tracks_visible = vlsp->data.b; break;
    case PARAM_WV: vtl->waypoints_visible = vlsp->data.b; break;
    case PARAM_RV: vtl->routes_visible = vlsp->data.b; break;
    case PARAM_TDL: vtl->track_draw_labels = vlsp->data.b; break;
    case PARAM_TLFONTSIZE:
      if ( vlsp->data.u < FS_NUM_SIZES ) {
        vtl->track_font_size = vlsp->data.u;
        g_free ( vtl->track_fsize_str );
        switch ( vtl->track_font_size ) {
          case FS_XX_SMALL: vtl->track_fsize_str = g_strdup ( "xx-small" ); break;
          case FS_X_SMALL: vtl->track_fsize_str = g_strdup ( "x-small" ); break;
          case FS_SMALL: vtl->track_fsize_str = g_strdup ( "small" ); break;
          case FS_LARGE: vtl->track_fsize_str = g_strdup ( "large" ); break;
          case FS_X_LARGE: vtl->track_fsize_str = g_strdup ( "x-large" ); break;
          case FS_XX_LARGE: vtl->track_fsize_str = g_strdup ( "xx-large" ); break;
          default: vtl->track_fsize_str = g_strdup ( "medium" ); break;
        }
      }
      break;
    case PARAM_DM: vtl->drawmode = vlsp->data.u; break;
    case PARAM_TC:
      vtl->track_color = vlsp->data.c;
      if ( vlsp->vp ) trw_layer_edit_track_gcs ( vtl, vlsp->vp );
      break;
    case PARAM_DP: vtl->drawpoints = vlsp->data.b; break;
    case PARAM_DPS:
      if ( vlsp->data.u >= MIN_POINT_SIZE && vlsp->data.u <= MAX_POINT_SIZE )
        vtl->drawpoints_size = vlsp->data.u;
      break;
    case PARAM_DE: vtl->drawelevation = vlsp->data.b; break;
    case PARAM_DS: vtl->drawstops = vlsp->data.b; break;
    case PARAM_DL: vtl->drawlines = vlsp->data.b; break;
    case PARAM_DD: vtl->drawdirections = vlsp->data.b; break;
    case PARAM_DDS:
      if ( vlsp->data.u >= MIN_ARROW_SIZE && vlsp->data.u <= MAX_ARROW_SIZE )
        vtl->drawdirections_size = vlsp->data.u;
      break;
    case PARAM_SL:
      if ( vlsp->data.u >= MIN_STOP_LENGTH && vlsp->data.u <= MAX_STOP_LENGTH )
        vtl->stop_length = vlsp->data.u;
      break;
    case PARAM_EF:
      if ( vlsp->data.u >= 1 && vlsp->data.u <= 100 )
        vtl->elevation_factor = vlsp->data.u;
      break;
    case PARAM_LT:
      if ( vlsp->data.u > 0 && vlsp->data.u < 15 && vlsp->data.u != vtl->line_thickness ) {
        vtl->line_thickness = vlsp->data.u;
        if ( vlsp->vp ) trw_layer_edit_track_gcs ( vtl, vlsp->vp );
      }
      break;
    case PARAM_BLT:
      if ( vlsp->data.u <= 8 && vlsp->data.u != vtl->bg_line_thickness ) {
        vtl->bg_line_thickness = vlsp->data.u;
        if ( vlsp->vp ) trw_layer_edit_track_gcs ( vtl, vlsp->vp );
      }
      break;
    case PARAM_TBGC:
      vtl->track_bg_color = vlsp->data.c;
#if !GTK_CHECK_VERSION (3,0,0)
      if ( vtl->track_bg_gc )
        gdk_gc_set_rgb_fg_color(vtl->track_bg_gc, &(vtl->track_bg_color));
#endif
      break;
    case PARAM_TDSF: vtl->track_draw_speed_factor = vlsp->data.d; break;
    case PARAM_TSO:
      if ( vlsp->data.u < VL_SO_LAST ) {
        vik_layer_sort_order_t old = vtl->track_sort_order;
        vtl->track_sort_order = vlsp->data.u;
        if ( vtl->track_sort_order != old )
          if ( !vlsp->is_file_operation ) {
            if ( g_hash_table_size(vtl->tracks) )
              trw_layer_sort_order_specified ( vtl, VIK_TRW_LAYER_SUBLAYER_TRACKS, vtl->track_sort_order );
            if ( g_hash_table_size(vtl->routes) )
              trw_layer_sort_order_specified ( vtl, VIK_TRW_LAYER_SUBLAYER_ROUTES, vtl->track_sort_order );
          }
      }
      break;
    case PARAM_TADEM: vtl->auto_dem = vlsp->data.b; break;
    case PARAM_TRDUP: vtl->auto_dedupl = vlsp->data.b; break;
    case PARAM_PGS: vtl->prefer_gps_speed = vlsp->data.b; break;
    case PARAM_DLA: vtl->drawlabels = vlsp->data.b; break;
    case PARAM_DI: vtl->drawimages = vlsp->data.b; break;
    case PARAM_IS:
      if ( vlsp->data.u != vtl->image_size ) {
        vtl->image_size = vlsp->data.u;
        g_hash_table_remove_all ( vtl->image_cache );
      }
      break;
    case PARAM_IA:
      if ( vlsp->data.u != vtl->image_alpha ) {
        vtl->image_alpha = vlsp->data.u;
        g_hash_table_remove_all ( vtl->image_cache );
      }
      break;
    case PARAM_ICS:
      // If cache size is made smaller, then reset cache
      if ( vlsp->data.u < vtl->image_cache_size )
        g_hash_table_remove_all ( vtl->image_cache );
      vtl->image_cache_size = vlsp->data.u;
      break;
    case PARAM_WPC:
      vtl->waypoint_color = vlsp->data.c;
#if !GTK_CHECK_VERSION (3,0,0)
      if ( vtl->waypoint_gc )
        gdk_gc_set_rgb_fg_color(vtl->waypoint_gc, &(vtl->waypoint_color));
#endif
      break;
    case PARAM_WPTC:
      vtl->waypoint_text_color = vlsp->data.c;
#if !GTK_CHECK_VERSION (3,0,0)
      if ( vtl->waypoint_text_gc )
        gdk_gc_set_rgb_fg_color(vtl->waypoint_text_gc, &(vtl->waypoint_text_color));
      break;
#endif
    case PARAM_WPBC:
      vtl->waypoint_bg_color = vlsp->data.c;
#if !GTK_CHECK_VERSION (3,0,0)
      if ( vtl->waypoint_bg_gc )
        gdk_gc_set_rgb_fg_color(vtl->waypoint_bg_gc, &(vtl->waypoint_bg_color));
      break;
#endif
    case PARAM_WPBA:
      vtl->wpbgand = vlsp->data.b;
#if !GTK_CHECK_VERSION (3,0,0)
      if ( vtl->waypoint_bg_gc )
        gdk_gc_set_function(vtl->waypoint_bg_gc, vlsp->data.b ? GDK_AND : GDK_COPY );
      break;
#endif
    case PARAM_WPSYM: if ( vlsp->data.u < WP_NUM_SYMBOLS ) vtl->wp_symbol = vlsp->data.u; break;
    case PARAM_WPSIZE: if ( vlsp->data.u > 0 && vlsp->data.u <= 64 ) vtl->wp_size = vlsp->data.u; break;
    case PARAM_WPSYMS: vtl->wp_draw_symbols = vlsp->data.b; break;
    case PARAM_WPFONTSIZE:
      if ( vlsp->data.u < FS_NUM_SIZES ) {
        vtl->wp_font_size = vlsp->data.u;
        g_free ( vtl->wp_fsize_str );
        switch ( vtl->wp_font_size ) {
          case FS_XX_SMALL: vtl->wp_fsize_str = g_strdup ( "xx-small" ); break;
          case FS_X_SMALL: vtl->wp_fsize_str = g_strdup ( "x-small" ); break;
          case FS_SMALL: vtl->wp_fsize_str = g_strdup ( "small" ); break;
          case FS_LARGE: vtl->wp_fsize_str = g_strdup ( "large" ); break;
          case FS_X_LARGE: vtl->wp_fsize_str = g_strdup ( "x-large" ); break;
          case FS_XX_LARGE: vtl->wp_fsize_str = g_strdup ( "xx-large" ); break;
          default: vtl->wp_fsize_str = g_strdup ( "medium" ); break;
        }
      }
      break;
    case PARAM_WPSO:
      if ( vlsp->data.u < VL_SO_LAST ) {
        vik_layer_sort_order_t old = vtl->wp_sort_order;
        vtl->wp_sort_order = vlsp->data.u;
        if ( vtl->wp_sort_order != old )
          if ( !vlsp->is_file_operation )
            if ( g_hash_table_size(vtl->waypoints) )
              trw_layer_sort_order_specified ( vtl, VIK_TRW_LAYER_SUBLAYER_WAYPOINTS, vtl->wp_sort_order );
      }
      break;
    // Metadata
    case PARAM_MDDESC:
      if ( vlsp->data.s && vtl->metadata ) {
        g_free (vtl->metadata->description);
        vtl->metadata->description = g_strdup (vlsp->data.s);
      }
      break;
    case PARAM_MDAUTH:
      if ( vlsp->data.s && vtl->metadata ) {
        g_free (vtl->metadata->author);
        vtl->metadata->author = g_strdup (vlsp->data.s);
      }
      break;
    case PARAM_MDTIME:
      if ( vlsp->data.s && vtl->metadata ) {
        g_free (vtl->metadata->timestamp);
        vtl->metadata->timestamp = g_strdup (vlsp->data.s);
      }
      break;
    case PARAM_MDKEYS:
      if ( vlsp->data.s && vtl->metadata ) {
        g_free (vtl->metadata->keywords);
        vtl->metadata->keywords = g_strdup (vlsp->data.s);
      }
      break;
    case PARAM_MDURL:
      if ( vlsp->data.s && vtl->metadata ) {
        g_free (vtl->metadata->url);
        vtl->metadata->url = g_strdup (vlsp->data.s);
      }
      break;
    // Filesystem
    case PARAM_GPXV:
      if ( vlsp->data.u <= GPX_V1_1 )
        vtl->gpx_version = vlsp->data.u;
      break;
    case PARAM_EXTL:
      if ( vlsp->data.u < VIK_EXTERNAL_TYPE_LAST ) {
          vtl->external_layer = vlsp->data.u;
          trw_update_layer_icon ( vtl );
      }
      break;
    case PARAM_EXTF:
      if ( vlsp->data.s ) {
        g_free (vtl->external_file);
        vtl->external_file = g_strdup (vlsp->data.s);
      }
      break;
    default: break;
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
    case PARAM_RV: rv.b = vtl->routes_visible; break;
    case PARAM_TDL: rv.b = vtl->track_draw_labels; break;
    case PARAM_TLFONTSIZE: rv.u = vtl->track_font_size; break;
    case PARAM_DM: rv.u = vtl->drawmode; break;
    case PARAM_TC: rv.c = vtl->track_color; break;
    case PARAM_DP: rv.b = vtl->drawpoints; break;
    case PARAM_DPS: rv.u = vtl->drawpoints_size; break;
    case PARAM_DE: rv.b = vtl->drawelevation; break;
    case PARAM_EF: rv.u = vtl->elevation_factor; break;
    case PARAM_DS: rv.b = vtl->drawstops; break;
    case PARAM_SL: rv.u = vtl->stop_length; break;
    case PARAM_DL: rv.b = vtl->drawlines; break;
    case PARAM_DD: rv.b = vtl->drawdirections; break;
    case PARAM_DDS: rv.u = vtl->drawdirections_size; break;
    case PARAM_LT: rv.u = vtl->line_thickness; break;
    case PARAM_BLT: rv.u = vtl->bg_line_thickness; break;
    case PARAM_DLA: rv.b = vtl->drawlabels; break;
    case PARAM_DI: rv.b = vtl->drawimages; break;
    case PARAM_TBGC: rv.c = vtl->track_bg_color; break;
    case PARAM_TDSF: rv.d = vtl->track_draw_speed_factor; break;
    case PARAM_TSO: rv.u = vtl->track_sort_order; break;
    case PARAM_TADEM: rv.b = vtl->auto_dem; break;
    case PARAM_TRDUP: rv.b = vtl->auto_dedupl; break;
    case PARAM_PGS: rv.b = vtl->prefer_gps_speed; break;
    case PARAM_IS: rv.u = vtl->image_size; break;
    case PARAM_IA: rv.u = vtl->image_alpha; break;
    case PARAM_ICS: rv.u = vtl->image_cache_size; break;
    case PARAM_WPC: rv.c = vtl->waypoint_color; break;
    case PARAM_WPTC: rv.c = vtl->waypoint_text_color; break;
    case PARAM_WPBC: rv.c = vtl->waypoint_bg_color; break;
    case PARAM_WPBA: rv.b = vtl->wpbgand; break;
    case PARAM_WPSYM: rv.u = vtl->wp_symbol; break;
    case PARAM_WPSIZE: rv.u = vtl->wp_size; break;
    case PARAM_WPSYMS: rv.b = vtl->wp_draw_symbols; break;
    case PARAM_WPFONTSIZE: rv.u = vtl->wp_font_size; break;
    case PARAM_WPSO: rv.u = vtl->wp_sort_order; break;
    // Metadata
    case PARAM_MDDESC: if (vtl->metadata) { rv.s = vtl->metadata->description; } break;
    case PARAM_MDAUTH: if (vtl->metadata) { rv.s = vtl->metadata->author; } break;
    case PARAM_MDTIME: if (vtl->metadata) { rv.s = vtl->metadata->timestamp; } break;
    case PARAM_MDKEYS: if (vtl->metadata) { rv.s = vtl->metadata->keywords; } break;
    case PARAM_MDURL:  if (vtl->metadata) { rv.s = vtl->metadata->url; } break;
    // Filesystem
    case PARAM_GPXV: rv.u = vtl->gpx_version; break;
    case PARAM_EXTL: rv.u = vtl->external_layer; break;
    case PARAM_EXTF: rv.s = vtl->external_file; break;
    // Reset
    case PARAM_RESET: rv.ptr = reset_cb; break;
    default: break;
  }
  return rv;
}

static void trw_layer_change_param ( GtkWidget *widget, ui_change_values values )
{
  // This '-3' is to account for the first few parameters not in the properties
  const gint OFFSET = -3;

  switch ( GPOINTER_TO_INT(values[UI_CHG_PARAM_ID]) ) {
    // Alter sensitivity of waypoint draw image related widgets according to the draw image setting.
    case PARAM_DI: {
      // Get new value
      VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, values[UI_CHG_PARAM] );
      GtkWidget **ww1 = values[UI_CHG_WIDGETS];
      GtkWidget **ww2 = values[UI_CHG_LABELS];
      GtkWidget *w1 = ww1[OFFSET + PARAM_IS];
      GtkWidget *w2 = ww2[OFFSET + PARAM_IS];
      GtkWidget *w3 = ww1[OFFSET + PARAM_IA];
      GtkWidget *w4 = ww2[OFFSET + PARAM_IA];
      GtkWidget *w5 = ww1[OFFSET + PARAM_ICS];
      GtkWidget *w6 = ww2[OFFSET + PARAM_ICS];
      if ( w1 ) gtk_widget_set_sensitive ( w1, vlpd.b );
      if ( w2 ) gtk_widget_set_sensitive ( w2, vlpd.b );
      if ( w3 ) gtk_widget_set_sensitive ( w3, vlpd.b );
      if ( w4 ) gtk_widget_set_sensitive ( w4, vlpd.b );
      if ( w5 ) gtk_widget_set_sensitive ( w5, vlpd.b );
      if ( w6 ) gtk_widget_set_sensitive ( w6, vlpd.b );
      break;
    }
    // Alter sensitivity of waypoint label related widgets according to the draw label setting.
    case PARAM_DLA: {
      // Get new value
      VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, values[UI_CHG_PARAM] );
      GtkWidget **ww1 = values[UI_CHG_WIDGETS];
      GtkWidget **ww2 = values[UI_CHG_LABELS];
      GtkWidget *w1 = ww1[OFFSET + PARAM_WPTC];
      GtkWidget *w2 = ww2[OFFSET + PARAM_WPTC];
      GtkWidget *w3 = ww1[OFFSET + PARAM_WPBC];
      GtkWidget *w4 = ww2[OFFSET + PARAM_WPBC];
      GtkWidget *w5 = ww1[OFFSET + PARAM_WPBA];
      GtkWidget *w6 = ww2[OFFSET + PARAM_WPBA];
      GtkWidget *w7 = ww1[OFFSET + PARAM_WPFONTSIZE];
      GtkWidget *w8 = ww2[OFFSET + PARAM_WPFONTSIZE];
      if ( w1 ) gtk_widget_set_sensitive ( w1, vlpd.b );
      if ( w2 ) gtk_widget_set_sensitive ( w2, vlpd.b );
      if ( w3 ) gtk_widget_set_sensitive ( w3, vlpd.b );
      if ( w4 ) gtk_widget_set_sensitive ( w4, vlpd.b );
      if ( w5 ) gtk_widget_set_sensitive ( w5, vlpd.b );
      if ( w6 ) gtk_widget_set_sensitive ( w6, vlpd.b );
      if ( w7 ) gtk_widget_set_sensitive ( w7, vlpd.b );
      if ( w8 ) gtk_widget_set_sensitive ( w8, vlpd.b );
      break;
    }
    // Alter sensitivity of all track colours according to the draw track mode.
    case PARAM_DM: {
      // Get new value
      VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, values[UI_CHG_PARAM] );
      gboolean sensitive = ( vlpd.u == DRAWMODE_ALL_SAME_COLOR );
      GtkWidget **ww1 = values[UI_CHG_WIDGETS];
      GtkWidget **ww2 = values[UI_CHG_LABELS];
      GtkWidget *w1 = ww1[OFFSET + PARAM_TC];
      GtkWidget *w2 = ww2[OFFSET + PARAM_TC];
      if ( w1 ) gtk_widget_set_sensitive ( w1, sensitive );
      if ( w2 ) gtk_widget_set_sensitive ( w2, sensitive );
      break;
    }
    case PARAM_MDTIME: {
      // Force metadata->timestamp to be always read-only for now.
      GtkWidget **ww = values[UI_CHG_WIDGETS];
      GtkWidget *w1 = ww[OFFSET + PARAM_MDTIME];
      if ( w1 ) gtk_widget_set_sensitive ( w1, FALSE );
    }
    // NB Since other track settings have been split across tabs,
    // I don't think it's useful to set sensitivities on widgets you can't immediately see
    case PARAM_EXTL: {
      VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, values[UI_CHG_PARAM] );
      gboolean sensitive = ( vlpd.u == VIK_TRW_LAYER_EXTERNAL || vlpd.u == VIK_TRW_LAYER_EXTERNAL_NO_WRITE );
      GtkWidget **ww1 = values[UI_CHG_WIDGETS];
      GtkWidget **ww2 = values[UI_CHG_LABELS];
      GtkWidget *w1 = ww1[OFFSET + PARAM_EXTF];
      GtkWidget *w2 = ww2[OFFSET + PARAM_EXTF];
      GtkWidget *w3 = ww1[OFFSET + PARAM_EXTL];
      VikFileEntry *vfe = VIK_FILE_ENTRY(w1);
      const gchar *file_name = vik_file_entry_get_filename ( vfe );
      if ( w1 ) {
        gtk_widget_set_sensitive ( w1, sensitive );
        if ( sensitive && strlen( file_name ) == 0)
            choose_file(VIK_FILE_ENTRY(w1));
      }
      if ( w2 ) gtk_widget_set_sensitive ( w2, sensitive );
      if ( w3 && strlen ( file_name ) == 0 && vlpd.u != VIK_TRW_LAYER_EXTERNAL_NO_WRITE )
        gtk_combo_box_text_remove ( GTK_COMBO_BOX_TEXT(w3), VIK_TRW_LAYER_EXTERNAL_NO_WRITE );
    }
    default: break;
  }
}

static void trw_layer_marshall( VikTrwLayer *vtl, guint8 **data, guint *len )
{
  guint8 *pd;
  guint pl;

  *data = NULL;

  // Use byte arrays to store sublayer data
  // much like done elsewhere e.g. vik_layer_marshall_params()
  GByteArray *ba = g_byte_array_new ( );

  guint8 *sl_data;
  guint sl_len;

  guint object_length;
  guint subtype;
  // store:
  // the length of the item
  // the sublayer type of item
  // the the actual item
#define tlm_append(object_pointer, size, type)	\
  subtype = (type); \
  object_length = (size); \
  g_byte_array_append ( ba, (guint8 *)&object_length, sizeof(object_length) ); \
  g_byte_array_append ( ba, (guint8 *)&subtype, sizeof(subtype) ); \
  g_byte_array_append ( ba, (object_pointer), object_length );

  // Layer parameters first
  vik_layer_marshall_params(VIK_LAYER(vtl), &pd, &pl);
  g_byte_array_append ( ba, (guint8 *)&pl, sizeof(pl) ); \
  g_byte_array_append ( ba, pd, pl );
  g_free ( pd );

  // Now sublayer data
  GHashTableIter iter;
  gpointer key, value;

  // Waypoints
  g_hash_table_iter_init ( &iter, vtl->waypoints );
  while ( g_hash_table_iter_next (&iter, &key, &value) ) {
    vik_waypoint_marshall ( VIK_WAYPOINT(value), &sl_data, &sl_len );
    tlm_append ( sl_data, sl_len, VIK_TRW_LAYER_SUBLAYER_WAYPOINT );
    g_free ( sl_data );
  }

  // Tracks
  g_hash_table_iter_init ( &iter, vtl->tracks );
  while ( g_hash_table_iter_next (&iter, &key, &value) ) {
    vik_track_marshall ( VIK_TRACK(value), &sl_data, &sl_len );
    tlm_append ( sl_data, sl_len, VIK_TRW_LAYER_SUBLAYER_TRACK );
    g_free ( sl_data );
  }

  // Routes
  g_hash_table_iter_init ( &iter, vtl->routes );
  while ( g_hash_table_iter_next (&iter, &key, &value) ) {
    vik_track_marshall ( VIK_TRACK(value), &sl_data, &sl_len );
    tlm_append ( sl_data, sl_len, VIK_TRW_LAYER_SUBLAYER_ROUTE );
    g_free ( sl_data );
  }

#undef tlm_append

  *data = ba->data;
  *len = ba->len;
}

static VikTrwLayer *trw_layer_unmarshall ( const guint8 *data_in, guint len, VikViewport *vvp )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vvp, FALSE ));
  guint pl;
  guint consumed_length;
  guint8 *data = (guint8*)data_in;

  // First the overall layer parameters
  memcpy(&pl, data, sizeof(pl));
  data += sizeof(pl);
  vik_layer_unmarshall_params ( VIK_LAYER(vtl), data, pl, vvp );
  data += pl;

  consumed_length = pl;
  const guint sizeof_len_and_subtype = sizeof(guint) + sizeof(guint);

#define tlm_size (*(guint *)data)
  // See marshalling above for order of how this is written

  // Now the individual sublayers:
  while ( data && (consumed_length < len) ) {
    // Normally four extra bytes at the end of the datastream
    //  (since it's a GByteArray and that's where it's length is stored)
    //  So only attempt read when there's an actual block of sublayer data
    if ( consumed_length + tlm_size < len ) {

      // Reuse pl to read the subtype from the data stream
      memcpy(&pl, data+sizeof(guint), sizeof(pl));

      // Also remember to (attempt to) convert each coordinate in case this is pasted into a different drawmode
      if ( pl == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
        VikTrack *trk = vik_track_unmarshall ( data + sizeof_len_and_subtype, 0 );
        vik_trw_layer_add_track ( vtl, NULL, trk );
        vik_track_convert (trk, vtl->coord_mode);
      }
      if ( pl == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
        VikWaypoint *wp = vik_waypoint_unmarshall ( data + sizeof_len_and_subtype, 0 );
        vik_trw_layer_add_waypoint ( vtl, NULL, wp );
        waypoint_convert (NULL, wp, &vtl->coord_mode);
      }
      if ( pl == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
        VikTrack *trk = vik_track_unmarshall ( data + sizeof_len_and_subtype, 0 );
        vik_trw_layer_add_route ( vtl, NULL, trk );
        vik_track_convert (trk, vtl->coord_mode);
      }
    }
    // Don't shift data pointer to beyond our buffer of data - as otherwise it could point to anything
    if ( consumed_length + tlm_size < len ) {
      consumed_length += tlm_size + sizeof_len_and_subtype;
      //g_debug ("data %d, consumed_length %d vs len %d", tlm_size, consumed_length, len);
      data += sizeof_len_and_subtype + tlm_size;
    }
    else
      // Done
      data = NULL;
  }

  // Not stored anywhere else so need to regenerate
  trw_layer_calculate_bounds_waypoints ( vtl );

  return vtl;
}

// Keep interesting hash function at least visible
/*
static guint strcase_hash(gconstpointer v)
{
  // 31 bit hash function
  int i;
  const gchar *t = v;
  gchar s[128];   // malloc is too slow for reading big files
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
*/

static void pixbuf_free ( GdkPixbuf *pixbuf )
{
  g_object_unref ( G_OBJECT(pixbuf) );
}

// Stick a 1 at the end of the function name to make it more unique
//  thus more easily searchable in a simple text editor
static VikTrwLayer* trw_layer_new1 ( VikViewport *vvp )
{
  VikTrwLayer *rv = VIK_TRW_LAYER ( g_object_new ( VIK_TRW_LAYER_TYPE, NULL ) );
  vik_layer_set_type ( VIK_LAYER(rv), VIK_LAYER_TRW );

  // It's not entirely clear the benefits of hash tables usage here - possibly the simplicity of first implementation for unique names
  // Now with the name of the item stored as part of the item - these tables are effectively straightforward lists

  // For this reworking I've choosen to keep the use of hash tables since for the expected data sizes
  // - even many hundreds of waypoints and tracks is quite small in the grand scheme of things,
  //  and with normal PC processing capabilities - it has negligibile performance impact
  // This also minimized the amount of rework - as the management of the hash tables already exists.

  // The hash tables are indexed by simple integers acting as a UUID hash, which again shouldn't affect performance much
  //   we have to maintain a uniqueness (as before when multiple names where not allowed),
  //   this is to ensure it refers to the same item in the data structures used on the viewport and on the layers panel

  rv->waypoints = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) vik_waypoint_free );
  rv->waypoints_iters = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, g_free );
  rv->tracks = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) vik_track_free );
  rv->tracks_iters = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, g_free );
  rv->routes = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) vik_track_free );
  rv->routes_iters = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, g_free );

  rv->image_cache = g_hash_table_new_full ( g_str_hash, g_str_equal, NULL, (GDestroyNotify) pixbuf_free ); // Must be performed before set_params via set_defaults

  vik_layer_set_defaults ( VIK_LAYER(rv), vvp );

  // Param settings that are not available via the GUI
  // Force to on after processing params (which defaults them to off with a zero value)
  rv->waypoints_visible = rv->tracks_visible = rv->routes_visible = TRUE;

  rv->metadata = vik_trw_metadata_new ();
  rv->draw_sync_done = TRUE;
  rv->draw_sync_do = TRUE;
  rv->coord_mode = VIK_COORD_LATLON;

  // Everything else is 0, FALSE or NULL

  return rv;
}

static void trw_layer_free ( VikTrwLayer *trwlayer )
{
  g_hash_table_destroy(trwlayer->waypoints);
  g_hash_table_destroy(trwlayer->waypoints_iters);
  g_hash_table_destroy(trwlayer->tracks);
  g_hash_table_destroy(trwlayer->tracks_iters);
  g_hash_table_destroy(trwlayer->routes);
  g_hash_table_destroy(trwlayer->routes_iters);

  trw_layer_free_track_gcs ( trwlayer );

  if ( trwlayer->wp_right_click_menu )
    g_object_ref_sink ( G_OBJECT(trwlayer->wp_right_click_menu) );

  if ( trwlayer->track_right_click_menu )
    g_object_ref_sink ( G_OBJECT(trwlayer->track_right_click_menu) );

  if ( trwlayer->tracklabellayout != NULL)
    g_object_unref ( G_OBJECT ( trwlayer->tracklabellayout ) );

  if ( trwlayer->wplabellayout != NULL)
    g_object_unref ( G_OBJECT ( trwlayer->wplabellayout ) );

  if ( trwlayer->waypoint_gc != NULL )
    ui_gc_unref ( trwlayer->waypoint_gc );

  if ( trwlayer->waypoint_text_gc != NULL )
    ui_gc_unref ( trwlayer->waypoint_text_gc );

  if ( trwlayer->waypoint_bg_gc != NULL )
    ui_gc_unref ( trwlayer->waypoint_bg_gc );

  if ( trwlayer->track_graph_point_gc )
    ui_gc_unref ( trwlayer->track_graph_point_gc );

  g_free ( trwlayer->wp_fsize_str );
  g_free ( trwlayer->track_fsize_str );

  if ( trwlayer->tpwin != NULL )
    gtk_widget_destroy ( GTK_WIDGET(trwlayer->tpwin) );

  if ( trwlayer->wpwin != NULL )
    vik_trw_layer_wpwin_destroy ( trwlayer->wpwin );

  if ( trwlayer->tracks_analysis_dialog != NULL )
    gtk_widget_destroy ( GTK_WIDGET(trwlayer->tracks_analysis_dialog) );

  g_hash_table_destroy ( trwlayer->image_cache );

  g_free ( trwlayer->external_file );
  g_free ( trwlayer->external_dirpath );

  if ( trwlayer->crosshair_cursor )
  {
    gdk_cursor_unref ( trwlayer->crosshair_cursor );
    trwlayer->crosshair_cursor = NULL;
  }

  vik_trw_metadata_free ( trwlayer->metadata );
  g_free ( trwlayer->gpx_header );
  g_free ( trwlayer->gpx_extensions );
}

static void init_drawing_params ( struct DrawingParams *dp, VikTrwLayer *vtl, VikViewport *vp, gboolean highlight )
{
  dp->vtl = vtl;
  dp->vp = vp;
  dp->highlight = highlight;
  dp->vw = (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(dp->vtl);
  dp->xmpp = vik_viewport_get_xmpp ( vp );
  dp->ympp = vik_viewport_get_ympp ( vp );
  dp->width = vik_viewport_get_width ( vp );
  dp->height = vik_viewport_get_height ( vp );
  dp->cc = vtl->drawdirections_size*cos(DEG2RAD(45)); // Calculate once per vtl update - even if not used
  dp->ss = vtl->drawdirections_size*sin(DEG2RAD(45)); // Calculate once per vtl update - even if not used

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

  dp->bbox = vik_viewport_get_bbox ( vp );
}

/*
 * Determine the colour of the trackpoint (and/or trackline) relative to the average speed
 * Here a simple traffic like light colour system is used:
 *  . slow points are red
 *  . average is yellow
 *  . fast points are green
 */
static gint track_section_colour_by_speed ( VikTrwLayer *vtl, VikTrackpoint *tp1, VikTrackpoint *tp2, gdouble average_speed, gdouble low_speed, gdouble high_speed )
{
  gdouble rv = 0;
  if ( !isnan(tp1->timestamp) && !isnan(tp2->timestamp) ) {
    if ( average_speed > 0 ) {
      rv = ( vik_coord_diff ( &(tp1->coord), &(tp2->coord) ) / (tp1->timestamp - tp2->timestamp) );
      if ( rv < low_speed )
        return VIK_TRW_LAYER_TRACK_GC_SLOW;
      else if ( rv > high_speed )
        return VIK_TRW_LAYER_TRACK_GC_FAST;
      else
        return VIK_TRW_LAYER_TRACK_GC_AVER;
    }
  }
  return VIK_TRW_LAYER_TRACK_GC_BLACK;
}


#if GTK_CHECK_VERSION (3,0,0)
static GdkColor track_section_gdkcolour_by_speed ( VikTrwLayer *vtl, VikTrackpoint *tp1, VikTrackpoint *tp2, gdouble average_speed, gdouble low_speed, gdouble high_speed )
{
  if ( !isnan(tp1->timestamp) && !isnan(tp2->timestamp) ) {
    if ( average_speed > 0 ) {
      gdouble spd = ( vik_coord_diff ( &(tp1->coord), &(tp2->coord) ) / (tp1->timestamp - tp2->timestamp) );
      if ( spd < low_speed )
        return vtl->slow_color;
      else if ( spd > high_speed )
        return vtl->fast_color;
      else
        return vtl->aver_color;
    }
  }
  return vtl->black_color;
}
#endif

static void draw_utm_skip_insignia ( VikViewport *vvp, GdkGC *gc, gint x, gint y, GdkColor *clr, guint lt )
{
  vik_viewport_draw_line ( vvp, gc, x+5, y, x-5, y, clr, lt );
  vik_viewport_draw_line ( vvp, gc, x, y+5, x, y-5, clr, lt );
  vik_viewport_draw_line ( vvp, gc, x+5, y+5, x-5, y-5, clr, lt );
  vik_viewport_draw_line ( vvp, gc, x+5, y-5, x-5, y+5, clr, lt );
}

static void trw_layer_draw_track_label ( gchar *name, gchar *fgcolour, gchar *bgcolour, struct DrawingParams *dp, VikCoord *coord )
{
  gchar *label_markup = g_strdup_printf ( "<span foreground=\"%s\" background=\"%s\" size=\"%s\">%s</span>", fgcolour, bgcolour, dp->vtl->track_fsize_str, name );

  if ( pango_parse_markup ( label_markup, -1, 0, NULL, NULL, NULL, NULL ) )
    pango_layout_set_markup ( dp->vtl->tracklabellayout, label_markup, -1 );
  else
    // Fallback if parse failure
    pango_layout_set_text ( dp->vtl->tracklabellayout, name, -1 );

  g_free ( label_markup );

  gint label_x, label_y;
  gint width, height;
  pango_layout_get_pixel_size ( dp->vtl->tracklabellayout, &width, &height );

  vik_viewport_coord_to_screen ( dp->vp, coord, &label_x, &label_y );
  vik_viewport_draw_layout ( dp->vp, dp->vtl->track_bg_gc, label_x-width/2, label_y-height/2, dp->vtl->tracklabellayout, &dp->vtl->track_bg_color );
}

/**
 * distance_in_preferred_units:
 * @dist: The source distance in standard SI Units (i.e. metres)
 *
 * TODO: This is a generic function that could be moved into globals.c or utils.c
 *
 * Probably best used if you have a only few conversions to perform.
 * However if doing many points (such as on all points along a track) then this may be a bit slow,
 *  since it will be doing the preference check on each call
 *
 * Returns: The distance in the units as specified by the preferences
 */
static gdouble distance_in_preferred_units ( gdouble dist )
{
  gdouble mydist;
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  switch (dist_units) {
  case VIK_UNITS_DISTANCE_MILES:
    mydist = VIK_METERS_TO_MILES(dist);
    break;
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    mydist = VIK_METERS_TO_NAUTICAL_MILES(dist);
    break;
  // VIK_UNITS_DISTANCE_KILOMETRES:
  default:
    mydist = dist/1000.0;
    break;
  }
  return mydist;
}

/**
 * trw_layer_draw_dist_labels:
 *
 * Draw a few labels along a track at nicely seperated distances
 * This might slow things down if there's many tracks being displayed with this on.
 */
static void trw_layer_draw_dist_labels ( struct DrawingParams *dp, VikTrack *trk, gboolean drawing_highlight )
{
  static const gdouble chunksd[] = {0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 20.0,
                                    25.0, 40.0, 50.0, 75.0, 100.0,
                                    150.0, 200.0, 250.0, 500.0, 1000.0};

  gdouble dist = vik_track_get_length_including_gaps ( trk ) / (trk->max_number_dist_labels+1);

  // Convert to specified unit to find the friendly breakdown value
  dist = distance_in_preferred_units ( dist );

  gint index = 0;
  gint i=0;
  for ( i = 0; i < G_N_ELEMENTS(chunksd); i++ ) {
    if ( chunksd[i] > dist ) {
      index = i;
      dist = chunksd[index];
      break;
    }
  }

  vik_units_distance_t dist_units = a_vik_get_units_distance ();

  for ( i = 1; i < trk->max_number_dist_labels+1; i++ ) {
    gdouble dist_i = dist * i;

    // Convert distance back into metres for use in finding a trackpoint
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_MILES:
      dist_i = VIK_MILES_TO_METERS(dist_i);
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      dist_i = VIK_NAUTICAL_MILES_TO_METERS(dist_i);
      break;
      // VIK_UNITS_DISTANCE_KILOMETRES:
    default:
      dist_i = dist_i*1000.0;
      break;
    }

    gdouble dist_current = 0.0;
    VikTrackpoint *tp_current = vik_track_get_tp_by_dist ( trk, dist_i, FALSE, &dist_current );
    gdouble dist_next = 0.0;
    VikTrackpoint *tp_next = vik_track_get_tp_by_dist ( trk, dist_i, TRUE, &dist_next );

    gdouble dist_between_tps = fabs (dist_next - dist_current);
    gdouble ratio = 0.0;
    // Prevent division by 0 errors
    if ( dist_between_tps > 0.0 )
      ratio = fabs(dist_i-dist_current)/dist_between_tps;

    if ( tp_current && tp_next ) {
      // Construct the name based on the distance value
      gchar *name;
      gchar *units;
      switch (dist_units) {
      case VIK_UNITS_DISTANCE_MILES:
        units = g_strdup ( _("miles") );
        break;
      case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
        units = g_strdup ( _("NM") );
        break;
        // VIK_UNITS_DISTANCE_KILOMETRES:
      default:
        units = g_strdup ( _("km") );
        break;
      }

      // Convert for display
      dist_i = distance_in_preferred_units ( dist_i );

      // Make the precision of the output related to the unit size.
      if ( index == 0 )
        name = g_strdup_printf ( "%.2f %s", dist_i, units);
      else if ( index == 1 )
        name = g_strdup_printf ( "%.1f %s", dist_i, units);
      else
        name = g_strdup_printf ( "%d %s", (gint)round(dist_i), units); // TODO single vs plurals
      g_free ( units );

      struct LatLon ll_current, ll_next;
      vik_coord_to_latlon ( &tp_current->coord, &ll_current );
      vik_coord_to_latlon ( &tp_next->coord, &ll_next );

      // positional interpolation
      // Using a simple ratio - may not be perfectly correct due to lat/long projections
      //  but should be good enough over the small scale that I anticipate usage on
      struct LatLon ll_new = { ll_current.lat + (ll_next.lat-ll_current.lat)*ratio,
			       ll_current.lon + (ll_next.lon-ll_current.lon)*ratio };
      VikCoord coord;
      vik_coord_load_from_latlon ( &coord, dp->vtl->coord_mode, &ll_new );

      gchar *fgcolour;
      if ( dp->vtl->drawmode == DRAWMODE_BY_TRACK )
        fgcolour = gdk_color_to_string ( &(trk->color) );
      else
        fgcolour = gdk_color_to_string ( &(dp->vtl->track_color) );

      // if highlight mode on, then colour the background in the highlight colour
      gchar *bgcolour;
      if ( drawing_highlight )
	bgcolour = g_strdup ( vik_viewport_get_highlight_color ( dp->vp ) );
      else
        bgcolour = gdk_color_to_string ( &(dp->vtl->track_bg_color) );

      trw_layer_draw_track_label ( name, fgcolour, bgcolour, dp, &coord );

      g_free ( fgcolour );
      g_free ( bgcolour );
      g_free ( name );
    }
  }
}

/**
 * trw_layer_draw_track_name_labels:
 *
 * Draw a label (or labels) for the track name somewhere depending on the track's properties
 */
static void trw_layer_draw_track_name_labels ( struct DrawingParams *dp, VikTrack *trk, gboolean drawing_highlight )
{
  gchar *fgcolour;
  if ( dp->vtl->drawmode == DRAWMODE_BY_TRACK )
    fgcolour = gdk_color_to_string ( &(trk->color) );
  else
    fgcolour = gdk_color_to_string ( &(dp->vtl->track_color) );

  // if highlight mode on, then colour the background in the highlight colour
  gchar *bgcolour;
  if ( drawing_highlight )
    bgcolour = g_strdup ( vik_viewport_get_highlight_color ( dp->vp ) );
  else
    bgcolour = gdk_color_to_string ( &(dp->vtl->track_bg_color) );

  gchar *ename = g_markup_escape_text ( trk->name, -1 );

  if ( trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE ||
       trk->draw_name_mode == TRACK_DRAWNAME_CENTRE ) {
    struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
    trw_layer_find_maxmin_tracks ( NULL, trk, maxmin );
    average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
    average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
    VikCoord coord;
    vik_coord_load_from_latlon ( &coord, dp->vtl->coord_mode, &average );

    trw_layer_draw_track_label ( ename, fgcolour, bgcolour, dp, &coord );
  }

  if ( trk->draw_name_mode == TRACK_DRAWNAME_CENTRE )
    // No other labels to draw
    return;

  VikTrackpoint *tp_end = vik_track_get_tp_last ( trk );
  if ( !tp_end )
    return;
  VikTrackpoint *tp_begin = vik_track_get_tp_first ( trk );
  if ( !tp_begin )
    return;
  VikCoord begin_coord = tp_begin->coord;
  VikCoord end_coord = tp_end->coord;

  gboolean done_start_end = FALSE;

  if ( trk->draw_name_mode == TRACK_DRAWNAME_START_END ||
       trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE ) {

    // This number can be configured via the settings if you really want to change it
    gdouble distance_diff;
    if ( ! a_settings_get_double ( "trackwaypoint_start_end_distance_diff", &distance_diff ) )
      distance_diff = 100.0; // Metres

    if ( vik_coord_diff ( &begin_coord, &end_coord ) < distance_diff ) {
      // Start and end 'close' together so only draw one label at an average location
      gint x1, x2, y1, y2;
      vik_viewport_coord_to_screen ( dp->vp, &begin_coord, &x1, &y1);
      vik_viewport_coord_to_screen ( dp->vp, &end_coord, &x2, &y2);
      VikCoord av_coord;
      vik_viewport_screen_to_coord ( dp->vp, (x1 + x2) / 2, (y1 + y2) / 2, &av_coord );

      gchar *name = g_strdup_printf ( "%s: %s", ename, _("start/end") );
      trw_layer_draw_track_label ( name, fgcolour, bgcolour, dp, &av_coord );
      g_free ( name );

      done_start_end = TRUE;
    }
  }

  if ( ! done_start_end ) {
    if ( trk->draw_name_mode == TRACK_DRAWNAME_START ||
         trk->draw_name_mode == TRACK_DRAWNAME_START_END ||
         trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE ) {
      gchar *name_start = g_strdup_printf ( "%s: %s", ename, _("start") );
      trw_layer_draw_track_label ( name_start, fgcolour, bgcolour, dp, &begin_coord );
      g_free ( name_start );
    }
    // Don't draw end label if this is the one being created
    if ( trk != dp->vtl->current_track ) {
      if ( trk->draw_name_mode == TRACK_DRAWNAME_END ||
           trk->draw_name_mode == TRACK_DRAWNAME_START_END ||
           trk->draw_name_mode == TRACK_DRAWNAME_START_END_CENTRE ) {
        gchar *name_end = g_strdup_printf ( "%s: %s", ename, _("end") );
        trw_layer_draw_track_label ( name_end, fgcolour, bgcolour, dp, &end_coord );
        g_free ( name_end );
      }
    }
  }

  g_free ( fgcolour );
  g_free ( bgcolour );
  g_free ( ename );
}


/**
 * trw_layer_draw_point_names:
 *
 * Draw a point labels along a track
 * This might slow things down if there's many tracks being displayed with this on.
 */
static void trw_layer_draw_point_names ( struct DrawingParams *dp, VikTrack *trk, gboolean drawing_highlight )
{
  GList *list = trk->trackpoints;
  if (!list) return;
  VikTrackpoint *tp = VIK_TRACKPOINT(list->data);
  gchar *fgcolour;
  if ( dp->vtl->drawmode == DRAWMODE_BY_TRACK )
    fgcolour = gdk_color_to_string ( &(trk->color) );
  else
    fgcolour = gdk_color_to_string ( &(dp->vtl->track_color) );
  gchar *bgcolour;
  if ( drawing_highlight )
    bgcolour = g_strdup ( vik_viewport_get_highlight_color ( dp->vp ) );
  else
    bgcolour = gdk_color_to_string ( &(dp->vtl->track_bg_color) );
  if ( tp->name )
    trw_layer_draw_track_label ( tp->name, fgcolour, bgcolour, dp, &tp->coord );
  while ((list = g_list_next(list)))
  {
    tp = VIK_TRACKPOINT(list->data);
    if ( tp->name )
      trw_layer_draw_track_label ( tp->name, fgcolour, bgcolour, dp, &tp->coord );
  };
  g_free ( fgcolour );
  g_free ( bgcolour );
}

static void trw_layer_draw_track ( const gpointer id, VikTrack *track, struct DrawingParams *dp, gboolean draw_track_outline )
{
  if ( ! track->visible )
    return;

  /* TODO: this function is a mess, get rid of any redundancy */
  GList *list = track->trackpoints;
  gboolean useoldvals = TRUE;

  gboolean drawpoints;
  gboolean drawstops;
  gboolean drawelevation;
  gdouble min_alt, max_alt, alt_diff = 0;

  const guint8 tp_size_reg = dp->vtl->drawpoints_size;
  const guint8 tp_size_cur = dp->vtl->drawpoints_size*2;
  guint8 tp_size;

  if ( dp->vtl->drawelevation ) {
    if ( ( drawelevation = vik_track_get_minmax_alt ( track, &min_alt, &max_alt ) ) )
      alt_diff = max_alt - min_alt;
  }

  /* admittedly this is not an efficient way to do it because we go through the whole GC thing all over... */
  if ( dp->vtl->bg_line_thickness && !draw_track_outline )
    trw_layer_draw_track ( id, track, dp, TRUE );

  if ( draw_track_outline )
    drawpoints = drawstops = FALSE;
  else {
    drawpoints = dp->vtl->drawpoints;
    drawstops = dp->vtl->drawstops;
  }

  gboolean drawing_highlight = FALSE;

  GdkGC *main_gc = NULL;
  GdkColor main_gcolor;
  guint lt = dp->vtl->line_thickness;
  /* Current track - used for creation */
  if ( track == dp->vtl->current_track ) {
    main_gc = dp->vtl->current_track_gc;
    gdk_color_parse ( "#FF0000", &main_gcolor );
    lt = (dp->vtl->line_thickness < 2) ? 2 : dp->vtl->line_thickness;
  }
  else {
    if ( dp->highlight ) {
      /* Draw all tracks of the layer in special colour
         NB this supercedes the drawmode */
      main_gc = vik_viewport_get_gc_highlight (dp->vp);
      drawing_highlight = TRUE;
      main_gcolor = vik_viewport_get_highlight_gdkcolor ( dp->vp );
    }
    if ( !drawing_highlight ) {
      // Still need to figure out the gc according to the drawing mode:
      switch ( dp->vtl->drawmode ) {
      case DRAWMODE_BY_TRACK:
        if ( dp->vtl->track_1color_gc )
          ui_gc_unref ( dp->vtl->track_1color_gc );
        dp->vtl->track_1color_gc = vik_viewport_new_gc_from_color ( dp->vp, &track->color, dp->vtl->line_thickness );
        main_gc = dp->vtl->track_1color_gc;
        main_gcolor = track->color;
	break;
      default:
        // Mostly for DRAWMODE_ALL_SAME_COLOR
        // but includes DRAWMODE_BY_SPEED, main_gc is set later on as necessary
        main_gc = g_array_index(dp->vtl->track_gc, GdkGC *, VIK_TRW_LAYER_TRACK_GC_SINGLE);
        main_gcolor = dp->vtl->track_color;
        break;
      }
    }
  }

  if (list) {
    int x, y, oldx, oldy;
    VikTrackpoint *tp = VIK_TRACKPOINT(list->data);
  
    tp_size = (list == dp->vtl->current_tpl) ? tp_size_cur : tp_size_reg;

    vik_viewport_coord_to_screen ( dp->vp, &(tp->coord), &x, &y );

    // Draw the first point as something a bit different from the normal points
    // ATM it's slightly bigger and a triangle
    if ( drawpoints ) {
      GdkPoint trian[3] = { { x, y-(3*tp_size) }, { x-(2*tp_size), y+(2*tp_size) }, {x+(2*tp_size), y+(2*tp_size)} };
      vik_viewport_draw_polygon ( dp->vp, main_gc, TRUE, trian, 3, &main_gcolor );
    }

    oldx = x;
    oldy = y;

    gdouble average_speed = 0.0;
    gdouble low_speed = 0.0;
    gdouble high_speed = 0.0;
    // If necessary calculate these values - which is done only once per track redraw
    if ( dp->vtl->drawmode == DRAWMODE_BY_SPEED ) {
      // the percentage factor away from the average speed determines transistions between the levels
      average_speed = vik_track_get_average_speed_moving(track, dp->vtl->stop_length);
      low_speed = average_speed - (average_speed*(dp->vtl->track_draw_speed_factor/100.0));
      high_speed = average_speed + (average_speed*(dp->vtl->track_draw_speed_factor/100.0));
    }

    while ((list = g_list_next(list)))
    {
      tp = VIK_TRACKPOINT(list->data);
      tp_size = (list == dp->vtl->current_tpl) ? tp_size_cur : tp_size_reg;

      VikTrackpoint *tp2 = VIK_TRACKPOINT(list->prev->data);
      // See if in a different lat/lon 'quadrant' so don't draw massively long lines (presumably wrong way around the Earth)
      //  Mainly to prevent wrong lines drawn when a track crosses the 180 degrees East-West longitude boundary
      //  (since vik_viewport_draw_line() only copes with pixel value and has no concept of the globe)
      if ( dp->lat_lon &&
           (( tp2->coord.east_west < -90.0 && tp->coord.east_west > 90.0 ) ||
            ( tp2->coord.east_west > 90.0 && tp->coord.east_west < -90.0 )) ) {
        useoldvals = FALSE;
        continue;
      }
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
	  if ( drawstops && drawpoints && ! draw_track_outline && list->next &&
	       (VIK_TRACKPOINT(list->next->data)->timestamp - VIK_TRACKPOINT(list->data)->timestamp > dp->vtl->stop_length) )
	    vik_viewport_draw_arc ( dp->vp, g_array_index(dp->vtl->track_gc, GdkGC *, VIK_TRW_LAYER_TRACK_GC_STOP), TRUE, x-(3*tp_size), y-(3*tp_size), 6*tp_size, 6*tp_size, 0, 360*64, NULL );

	  goto skip;
	}

        if ( drawpoints || dp->vtl->drawlines ) {
          // setup main_gc for both point and line drawing
          if ( !drawing_highlight && (dp->vtl->drawmode == DRAWMODE_BY_SPEED) ) {
#if GTK_CHECK_VERSION (3,0,0)
            main_gcolor = track_section_gdkcolour_by_speed ( dp->vtl, tp, tp2, average_speed, low_speed, high_speed );
#else
            main_gc = g_array_index(dp->vtl->track_gc, GdkGC *, track_section_colour_by_speed ( dp->vtl, tp, tp2, average_speed, low_speed, high_speed ) );
#endif
          }
        }

        if ( drawpoints && ! draw_track_outline )
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
	      /* Stop point.  Draw 6x circle. Always in redish colour */
              vik_viewport_draw_arc ( dp->vp, g_array_index(dp->vtl->track_gc, GdkGC *, VIK_TRW_LAYER_TRACK_GC_STOP), TRUE, x-(3*tp_size), y-(3*tp_size), 6*tp_size, 6*tp_size, 0, 360*64, NULL );

	    /* Regular point - draw 2x square. */
	    vik_viewport_draw_rectangle ( dp->vp, main_gc, TRUE, x-tp_size, y-tp_size, 2*tp_size, 2*tp_size, &main_gcolor );
          }
          else
	    /* Final point - draw 4x circle. */
            vik_viewport_draw_arc ( dp->vp, main_gc, TRUE, x-(2*tp_size), y-(2*tp_size), 4*tp_size, 4*tp_size, 0, 360*64, &main_gcolor );
        }

        if ((!tp->newsegment) && (dp->vtl->drawlines))
        {

          /* UTM only: zone check */
          if ( drawpoints && dp->vtl->coord_mode == VIK_COORD_UTM && tp->coord.utm_zone != dp->center->utm_zone )
            draw_utm_skip_insignia ( dp->vp, main_gc, x, y, &main_gcolor, lt );

          if (!useoldvals)
            vik_viewport_coord_to_screen ( dp->vp, &(tp2->coord), &oldx, &oldy );

          if ( draw_track_outline ) {
            vik_viewport_draw_line ( dp->vp, dp->vtl->track_bg_gc, oldx, oldy, x, y, &dp->vtl->track_bg_color, dp->vtl->line_thickness + dp->vtl->bg_line_thickness );
          }
          else {

            vik_viewport_draw_line ( dp->vp, main_gc, oldx, oldy, x, y, &main_gcolor, lt );

            if ( dp->vtl->drawelevation && list->next && !isnan(VIK_TRACKPOINT(list->next->data)->altitude) ) {
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

	      GdkGC *tmp_gc = main_gc;
              GdkColor gcl = { 0,0,0,0 };
              tmp_gc = main_gc;

	      if ( ((oldx - x) > 0 && (oldy - y) > 0) || ((oldx - x) < 0 && (oldy - y) < 0))
#if GTK_CHECK_VERSION (3,0,0)
                gcl = dp->vtl->light_color;
#else
		tmp_gc = gtk_widget_get_style(GTK_WIDGET(dp->vp))->light_gc[3];
#endif
	      else
#if GTK_CHECK_VERSION (3,0,0)
                gcl = dp->vtl->dark_color;
#else
		tmp_gc = gtk_widget_get_style(GTK_WIDGET(dp->vp))->dark_gc[0];
#endif
	      vik_viewport_draw_polygon ( dp->vp, tmp_gc, TRUE, tmp, 4, &gcl );

              vik_viewport_draw_line ( dp->vp, main_gc, oldx, oldy-FIXALTITUDE(list->data), x, y-FIXALTITUDE(list->next->data), &gcl, lt );
            }
          }
        }

        if ( (!tp->newsegment) && dp->vtl->drawdirections ) {
          // Draw an arrow at the mid point to show the direction of the track
          // Code is a rework from vikwindow::draw_ruler()
          gint midx = (oldx + x) / 2;
          gint midy = (oldy + y) / 2;

          gdouble len = sqrt ( ((midx-oldx) * (midx-oldx)) + ((midy-oldy) * (midy-oldy)) );
          // Avoid divide by zero and ensure at least 1 pixel big
          if ( len > 1 ) {
            gdouble dx = (oldx - midx) / len;
            gdouble dy = (oldy - midy) / len;
            vik_viewport_draw_line ( dp->vp, main_gc, midx, midy, midx + (dx * dp->cc + dy * dp->ss), midy + (dy * dp->cc - dx * dp->ss), &main_gcolor, lt );
            vik_viewport_draw_line ( dp->vp, main_gc, midx, midy, midx + (dx * dp->cc - dy * dp->ss), midy + (dy * dp->cc + dx * dp->ss), &main_gcolor, lt );
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
          if ( dp->vtl->coord_mode != VIK_COORD_UTM || tp->coord.utm_zone == dp->center->utm_zone )
          {
            vik_viewport_coord_to_screen ( dp->vp, &(tp->coord), &x, &y );

            if ( !drawing_highlight && (dp->vtl->drawmode == DRAWMODE_BY_SPEED) ) {
              main_gc = g_array_index(dp->vtl->track_gc, GdkGC *, track_section_colour_by_speed ( dp->vtl, tp, tp2, average_speed, low_speed, high_speed ));
	    }

	    /*
	     * If points are the same in display coordinates, don't draw.
	     */
	    if ( x != oldx || y != oldy )
	      {
		if ( draw_track_outline )
		  vik_viewport_draw_line ( dp->vp, dp->vtl->track_bg_gc, oldx, oldy, x, y, &dp->vtl->track_bg_color, dp->vtl->line_thickness + dp->vtl->bg_line_thickness );
		else
		  vik_viewport_draw_line ( dp->vp, main_gc, oldx, oldy, x, y, &main_gcolor, lt );
	      }
          }
          else 
          {
	    /*
	     * If points are the same in display coordinates, don't draw.
	     */
	    if ( x != oldx || y != oldy )
	      {
		vik_viewport_coord_to_screen ( dp->vp, &(tp2->coord), &x, &y );
		draw_utm_skip_insignia ( dp->vp, main_gc, x, y, &main_gcolor, lt );
	      }
          }
        }
        useoldvals = FALSE;
      }
    }

    // Labels drawn after the trackpoints, so the labels are on top
    if ( dp->vtl->track_draw_labels ) {
      if ( track->max_number_dist_labels > 0 ) {
        trw_layer_draw_dist_labels ( dp, track, drawing_highlight );
      }
      trw_layer_draw_point_names (dp, track, drawing_highlight );

      if ( track->draw_name_mode != TRACK_DRAWNAME_NO ) {
        trw_layer_draw_track_name_labels ( dp, track, drawing_highlight );
      }
    }
  }

#if GTK_CHECK_VERSION (3,0,0)
  //  if ( main_gc )
  //    cairo_stroke ( main_gc );
#endif
}

static void trw_layer_draw_track_cb ( const gpointer id, VikTrack *track, struct DrawingParams *dp )
{
  if ( BBOX_INTERSECT ( track->bbox, dp->bbox ) ) {
    trw_layer_draw_track ( id, track, dp, FALSE );
  }
}

static void trw_layer_draw_waypoint ( const gpointer id, VikWaypoint *wp, struct DrawingParams *dp )
{
  if ( wp->visible )
  if ( (!dp->one_zone && !dp->lat_lon) || ( ( dp->lat_lon || wp->coord.utm_zone == dp->center->utm_zone ) && 
             wp->coord.east_west < dp->ce2 && wp->coord.east_west > dp->ce1 && 
             wp->coord.north_south > dp->cn1 && wp->coord.north_south < dp->cn2 ) )
  {
    gint x, y;
    vik_viewport_coord_to_screen ( dp->vp, &(wp->coord), &x, &y );

    /* if in shrunken_cache, get that. If not, get and add to shrunken_cache */

    if ( wp->image && dp->vtl->drawimages )
    {
      if ( dp->vtl->image_alpha == 0)
        return;

      GdkPixbuf *pixbuf = g_hash_table_lookup ( dp->vtl->image_cache, wp->image );
      if ( !pixbuf )
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
          if ( dp->vtl->image_size == 128 )
            pixbuf = regularthumb;
          else
          {
            pixbuf = a_thumbnails_scale_pixbuf(regularthumb, dp->vtl->image_size, dp->vtl->image_size);
            g_object_unref ( G_OBJECT(regularthumb) );
          }

          // Apply alpha setting to the image before the pixbuf gets stored in the cache
          if ( dp->vtl->image_alpha != 255 )
            pixbuf = ui_pixbuf_set_alpha ( pixbuf, dp->vtl->image_alpha );

          /* needed so 'click picture' tool knows how big the pic is; we don't
           * store it in the cache because they may have been freed already. */
          wp->image_width = gdk_pixbuf_get_width ( pixbuf );
          wp->image_height = gdk_pixbuf_get_height ( pixbuf );

          if ( g_hash_table_size(dp->vtl->image_cache) < dp->vtl->image_cache_size )
            g_hash_table_insert ( dp->vtl->image_cache, image, pixbuf );
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
          if ( dp->highlight ) {
            // Highlighted - so draw a little border around the chosen one
            // single line seems a little weak so draw 2 of them
            GdkColor hcolor = vik_viewport_get_highlight_gdkcolor ( dp->vp );
            vik_viewport_draw_rectangle (dp->vp, vik_viewport_get_gc_highlight (dp->vp), FALSE,
                                         x - (w/2) - 1, y - (h/2) - 1, w + 2, h + 2, &hcolor);
            vik_viewport_draw_rectangle (dp->vp, vik_viewport_get_gc_highlight (dp->vp), FALSE,
                                         x - (w/2) - 2, y - (h/2) - 2, w + 4, h + 4, &hcolor);
          }

          vik_viewport_draw_pixbuf ( dp->vp, pixbuf, 0, 0, x - (w/2), y - (h/2), w, h );
        }
        return; /* if failed to draw picture, default to drawing regular waypoint (below) */
      }
    }

    // Draw appropriate symbol - either symbol image or simple types
    if ( dp->vtl->wp_draw_symbols && wp->symbol && wp->symbol_pixbuf ) {
      vik_viewport_draw_pixbuf ( dp->vp, wp->symbol_pixbuf, 0, 0, x - gdk_pixbuf_get_width(wp->symbol_pixbuf)/2, y - gdk_pixbuf_get_height(wp->symbol_pixbuf)/2, -1, -1 );
    } 
    else if ( wp == dp->vtl->current_wp ) {
      switch ( dp->vtl->wp_symbol ) {
      case WP_SYMBOL_FILLED_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, TRUE, x - (dp->vtl->wp_size), y - (dp->vtl->wp_size), dp->vtl->wp_size*2, dp->vtl->wp_size*2, &dp->vtl->waypoint_color ); break;
      case WP_SYMBOL_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, FALSE, x - (dp->vtl->wp_size), y - (dp->vtl->wp_size), dp->vtl->wp_size*2, dp->vtl->wp_size*2, &dp->vtl->waypoint_color ); break;
        case WP_SYMBOL_CIRCLE: vik_viewport_draw_arc ( dp->vp, dp->vtl->waypoint_gc, TRUE, x - dp->vtl->wp_size, y - dp->vtl->wp_size, dp->vtl->wp_size*2, dp->vtl->wp_size*2, 0, 360*64, &dp->vtl->waypoint_color ); break;
      case WP_SYMBOL_X:
        vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x - dp->vtl->wp_size, y - dp->vtl->wp_size, x + dp->vtl->wp_size, y + dp->vtl->wp_size, &dp->vtl->waypoint_color, 2 );
        vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x - dp->vtl->wp_size, y + dp->vtl->wp_size, x + dp->vtl->wp_size, y - dp->vtl->wp_size, &dp->vtl->waypoint_color, 2 );
        break;
      default: break;
      }
    }
    else {
      switch ( dp->vtl->wp_symbol ) {
      case WP_SYMBOL_FILLED_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, TRUE, x - dp->vtl->wp_size/2, y - dp->vtl->wp_size/2, dp->vtl->wp_size, dp->vtl->wp_size, &dp->vtl->waypoint_color ); break;
      case WP_SYMBOL_SQUARE: vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_gc, FALSE, x - dp->vtl->wp_size/2, y - dp->vtl->wp_size/2, dp->vtl->wp_size, dp->vtl->wp_size, &dp->vtl->waypoint_color ); break;
      case WP_SYMBOL_CIRCLE: vik_viewport_draw_arc ( dp->vp, dp->vtl->waypoint_gc, TRUE, x-dp->vtl->wp_size/2, y-dp->vtl->wp_size/2, dp->vtl->wp_size, dp->vtl->wp_size, 0, 360*64, &dp->vtl->waypoint_color ); break;
      case WP_SYMBOL_X:
        vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x-dp->vtl->wp_size/2, y-dp->vtl->wp_size/2, x+dp->vtl->wp_size/2, y+dp->vtl->wp_size/2, &dp->vtl->waypoint_color, 2 );
        vik_viewport_draw_line ( dp->vp, dp->vtl->waypoint_gc, x-dp->vtl->wp_size/2, y+dp->vtl->wp_size/2, x+dp->vtl->wp_size/2, y-dp->vtl->wp_size/2, &dp->vtl->waypoint_color, 2 );
        break;
      default: break;
      }
    }

    if ( dp->vtl->drawlabels && !wp->hide_name )
    {
      /* thanks to the GPSDrive people (Fritz Ganter et al.) for hints on this part ... yah, I'm too lazy to study documentation */
      gint label_x, label_y;
      gint width, height;
      // Hopefully name won't break the markup (may need to sanitize - g_markup_escape_text())

      // Could this stored in the waypoint rather than recreating each pass?
      gchar *wp_label_markup = g_strdup_printf ( "<span size=\"%s\">%s</span>", dp->vtl->wp_fsize_str, wp->name );

      if ( pango_parse_markup ( wp_label_markup, -1, 0, NULL, NULL, NULL, NULL ) )
        pango_layout_set_markup ( dp->vtl->wplabellayout, wp_label_markup, -1 );
      else
        // Fallback if parse failure
        pango_layout_set_text ( dp->vtl->wplabellayout, wp->name, -1 );

      g_free ( wp_label_markup );

      pango_layout_get_pixel_size ( dp->vtl->wplabellayout, &width, &height );
      label_x = x - width/2;
      if ( wp->symbol_pixbuf )
        label_y = y - height - 2 - gdk_pixbuf_get_height(wp->symbol_pixbuf)/2;
      else
        label_y = y - dp->vtl->wp_size - height - 2;

      /* if highlight mode on, then draw background text in highlight colour */
      if ( dp->highlight ) {
        GdkColor hcolor = vik_viewport_get_highlight_gdkcolor(dp->vp);
#if GTK_CHECK_VERSION (3,0,0)
        if ( dp->vtl->wpbgand ) {
          GdkRGBA bg = { hcolor.red / 65535.0, hcolor.blue / 65535.0, hcolor.green / 65535.0, 0.5 };
          gdk_cairo_set_source_rgba ( dp->vtl->waypoint_bg_gc, &bg );
          vik_viewport_draw_rectangle ( dp->vp, vik_viewport_get_gc_highlight (dp->vp), TRUE, label_x - 1, label_y-1,width+2,height+2, NULL );
        }
        else
#endif
        vik_viewport_draw_rectangle ( dp->vp, vik_viewport_get_gc_highlight (dp->vp), TRUE, label_x - 1, label_y-1,width+2,height+2, &hcolor );
      }
      else {
#if GTK_CHECK_VERSION (3,0,0)
        if ( dp->vtl->wpbgand ) {
          GdkRGBA bg = { dp->vtl->waypoint_bg_color.red / 65535.0,
                         dp->vtl->waypoint_bg_color.blue / 65535.0,
                         dp->vtl->waypoint_bg_color.green / 65535.0,
                         0.5 };
          gdk_cairo_set_source_rgba ( dp->vtl->waypoint_bg_gc, &bg );
          vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_bg_gc, TRUE, label_x - 1, label_y-1,width+2,height+2, NULL );
        }
        else
#endif
        vik_viewport_draw_rectangle ( dp->vp, dp->vtl->waypoint_bg_gc, TRUE, label_x - 1, label_y-1,width+2,height+2, &dp->vtl->waypoint_bg_color );
      }
      vik_viewport_draw_layout ( dp->vp, dp->vtl->waypoint_text_gc, label_x, label_y, dp->vtl->wplabellayout, &dp->vtl->waypoint_text_color );
    }
  }
}

static void trw_layer_draw_waypoint_cb ( gpointer id, VikWaypoint *wp, struct DrawingParams *dp )
{
  if ( BBOX_INTERSECT ( dp->vtl->waypoints_bbox, dp->bbox ) ) {
    trw_layer_draw_waypoint ( id, wp, dp );
  }
}

static void trw_layer_draw_with_highlight ( VikTrwLayer *l, VikViewport *vvp, gboolean highlight )
{
  static struct DrawingParams dp;
  g_assert ( l != NULL );

  init_drawing_params ( &dp, l, vvp, highlight );

  if ( l->tracks_visible )
    g_hash_table_foreach ( l->tracks, (GHFunc) trw_layer_draw_track_cb, &dp );

  if ( l->routes_visible )
    g_hash_table_foreach ( l->routes, (GHFunc) trw_layer_draw_track_cb, &dp );

  if (l->waypoints_visible)
    g_hash_table_foreach ( l->waypoints, (GHFunc) trw_layer_draw_waypoint_cb, &dp );
}

static void trw_layer_draw ( VikTrwLayer *l, VikViewport *vvp )
{
  trw_ensure_layer_loaded ( l );
  // If this layer is to be highlighted - then don't draw now - as it will be drawn later on in the specific highlight draw stage
  // This may seem slightly inefficient to test each time for every layer
  //  but for a layer with *lots* of tracks & waypoints this can save some effort by not drawing the items twice
  if ( vik_viewport_get_draw_highlight ( vvp ) &&
       vik_window_get_selected_trw_layer ((VikWindow*)VIK_GTK_WINDOW_FROM_LAYER((VikLayer*)l)) == l )
    return;
  trw_layer_draw_with_highlight ( l, vvp, FALSE );
}

void vik_trw_layer_draw_highlight ( VikTrwLayer *vtl, VikViewport *vvp )
{
  // Check the layer for visibility (including all the parents visibilities)
  if ( !vik_treeview_item_get_visible_tree (VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter)) )
    return;
  trw_layer_draw_with_highlight ( vtl, vvp, TRUE );
}

/**
 * vik_trw_layer_draw_highlight_item:
 *
 * Only handles a single track or waypoint ATM
 * It assumes the track or waypoint belongs to the TRW Layer (it doesn't check this is the case)
 */
void vik_trw_layer_draw_highlight_item ( VikTrwLayer *vtl, VikTrack *trk, VikWaypoint *wpt, VikViewport *vvp )
{
  // Check the layer for visibility (including all the parents visibilities)
  if ( !vik_treeview_item_get_visible_tree (VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter)) )
    return;

  static struct DrawingParams dp;
  init_drawing_params ( &dp, vtl, vvp, TRUE );

  if ( trk ) {
    gboolean draw = ( trk->is_route && vtl->routes_visible ) || ( !trk->is_route && vtl->tracks_visible );
    if ( draw )
      trw_layer_draw_track_cb ( NULL, trk, &dp );
  }
  if ( vtl->waypoints_visible && wpt ) {
    trw_layer_draw_waypoint_cb ( NULL, wpt, &dp );
  }
}

#if GTK_CHECK_VERSION (3,0,0)
static gboolean is_light ( GdkRGBA *rgba )
{
  // No idea is this really makes any sense
  return ((rgba->red + rgba->green + rgba->blue) > 1.5);
}
#endif

static void trw_layer_create_other_gcs ( VikTrwLayer *vtl, VikViewport *vp )
{
  vtl->waypoint_gc = vik_viewport_new_gc_from_color ( vp, &(vtl->waypoint_color), 2 );
  vtl->waypoint_text_gc = vik_viewport_new_gc_from_color ( vp, &(vtl->waypoint_text_color), 1 );
  vtl->waypoint_bg_gc = vik_viewport_new_gc_from_color ( vp, &(vtl->waypoint_bg_color), 1 );
#if GTK_CHECK_VERSION (3,0,0)
  vtl->track_graph_point_gc = vik_viewport_new_gc ( vp, "black", 1 ); // NB Color and thickness unused(?)
#else
  vtl->track_graph_point_gc = gdk_gc_new ( gtk_widget_get_window(GTK_WIDGET(vp)) );
  gdk_gc_set_function ( vtl->waypoint_bg_gc, vtl->wpbgand );
#endif

#if GTK_CHECK_VERSION (3,0,0)
  // TODO a more sophisticated GTK3 light/dark color
  GdkRGBA *rgbaBC;
  GtkStyleContext *gsc = gtk_widget_get_style_context ( GTK_WIDGET(vp) );
  gtk_style_context_get ( gsc, gtk_style_context_get_state(gsc), GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &rgbaBC, NULL );
  if ( is_light(rgbaBC) ) {
    (void)gdk_color_parse ( "#4a90d9", &vtl->light_color );
    (void)gdk_color_parse ( "grey", &vtl->dark_color );
  } else {
    (void)gdk_color_parse ( "#215d9c", &vtl->light_color );
    (void)gdk_color_parse ( "brown", &vtl->dark_color );
  }
  gdk_rgba_free ( rgbaBC );
#endif
}

static void trw_layer_configure ( VikTrwLayer *vtl, VikViewport *vvp )
{
  trw_layer_edit_track_gcs ( vtl, vvp );

  if ( vtl->waypoint_gc )
    ui_gc_unref ( vtl->waypoint_gc );
  if ( vtl->waypoint_text_gc )
    ui_gc_unref ( vtl->waypoint_text_gc );
  if ( vtl->waypoint_bg_gc )
    ui_gc_unref ( vtl->waypoint_bg_gc );
  if ( vtl->track_graph_point_gc )
    ui_gc_unref ( vtl->track_graph_point_gc );

  trw_layer_create_other_gcs ( vtl, vvp );
}

/**
 * vik_trw_layer_draw_highlight_items:
 *
 * Generally for drawing all tracks or routes or waypoints
 * trks may be actually routes
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void vik_trw_layer_draw_highlight_items ( VikTrwLayer *vtl, GHashTable *trks, GHashTable *wpts, VikViewport *vvp )
{
  // Check the layer for visibility (including all the parents visibilities)
  if ( !vik_treeview_item_get_visible_tree (VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter)) )
    return;

  static struct DrawingParams dp;
  init_drawing_params ( &dp, vtl, vvp, TRUE );

  if ( trks ) {
    gboolean is_routes = (trks == vtl->routes);
    gboolean draw = ( is_routes && vtl->routes_visible ) || ( !is_routes && vtl->tracks_visible );
    if ( draw )
      g_hash_table_foreach ( trks, (GHFunc) trw_layer_draw_track_cb, &dp );
  }

  if ( vtl->waypoints_visible && wpts )
    g_hash_table_foreach ( wpts, (GHFunc) trw_layer_draw_waypoint_cb, &dp );
}


static void trw_layer_free_track_gcs ( VikTrwLayer *vtl )
{
  int i;
  if ( vtl->track_bg_gc ) 
  {
    ui_gc_unref ( vtl->track_bg_gc );
    vtl->track_bg_gc = NULL;
  }
  if ( vtl->track_1color_gc )
  {
    ui_gc_unref ( vtl->track_1color_gc );
    vtl->track_1color_gc = NULL;
  }
  if ( vtl->current_track_gc ) 
  {
    ui_gc_unref ( vtl->current_track_gc );
    vtl->current_track_gc = NULL;
  }
  if ( vtl->current_track_newpoint_gc )
  {
    ui_gc_unref ( vtl->current_track_newpoint_gc );
    vtl->current_track_newpoint_gc = NULL;
  }

  if ( ! vtl->track_gc )
    return;
  for ( i = vtl->track_gc->len - 1; i >= 0; i-- )
    ui_gc_unref ( g_array_index ( vtl->track_gc, GdkGC*, i ) );
  g_array_free ( vtl->track_gc, TRUE );
  vtl->track_gc = NULL;
}

static void trw_layer_edit_track_gcs ( VikTrwLayer *vtl, VikViewport *vp )
{
  GdkGC *gc[ VIK_TRW_LAYER_TRACK_GC ];
  gint width = vtl->line_thickness;

  if ( vtl->track_gc )
    trw_layer_free_track_gcs ( vtl );

  if ( vtl->track_bg_gc )
    ui_gc_unref ( vtl->track_bg_gc );
  vtl->track_bg_gc = vik_viewport_new_gc_from_color ( vp, &(vtl->track_bg_color), width + vtl->bg_line_thickness );

  // Ensure new track drawing heeds line thickness setting
  //  however always have a minium of 2, as 1 pixel is really narrow
  gint new_track_width = (vtl->line_thickness < 2) ? 2 : vtl->line_thickness;
  
  if ( vtl->current_track_gc )
    ui_gc_unref ( vtl->current_track_gc );
  vtl->current_track_gc = vik_viewport_new_gc ( vp, "#FF0000", new_track_width );
#if !GTK_CHECK_VERSION (3,0,0)
  gdk_gc_set_line_attributes ( vtl->current_track_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND );
#endif
  // 'newpoint' gc is exactly the same as the current track gc
  if ( vtl->current_track_newpoint_gc )
    ui_gc_unref ( vtl->current_track_newpoint_gc );
  vtl->current_track_newpoint_gc = vik_viewport_new_gc ( vp, "#FF0000", new_track_width );
#if !GTK_CHECK_VERSION (3,0,0)
  gdk_gc_set_line_attributes ( vtl->current_track_newpoint_gc, new_track_width, GDK_LINE_ON_OFF_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND );
#endif
  vtl->track_gc = g_array_sized_new ( FALSE, FALSE, sizeof ( GdkGC * ), VIK_TRW_LAYER_TRACK_GC );

  gc[VIK_TRW_LAYER_TRACK_GC_STOP] = vik_viewport_new_gc ( vp, COLOR_STOP, width );
  gc[VIK_TRW_LAYER_TRACK_GC_BLACK] = vik_viewport_new_gc ( vp, "#000000", width ); // black

  gc[VIK_TRW_LAYER_TRACK_GC_SLOW] = vik_viewport_new_gc ( vp, COLOR_SLOW, width ); // red-ish
  gc[VIK_TRW_LAYER_TRACK_GC_AVER] = vik_viewport_new_gc ( vp, COLOR_AVER, width ); // yellow-ish
  gc[VIK_TRW_LAYER_TRACK_GC_FAST] = vik_viewport_new_gc ( vp, COLOR_FAST, width ); // green-ish

  gc[VIK_TRW_LAYER_TRACK_GC_SINGLE] = vik_viewport_new_gc_from_color ( vp, &(vtl->track_color), width );

  g_array_append_vals ( vtl->track_gc, gc, VIK_TRW_LAYER_TRACK_GC );
}

static VikTrwLayer* trw_layer_create ( VikViewport *vp )
{
  VikTrwLayer *rv = trw_layer_new1 ( vp );
  vik_layer_rename ( VIK_LAYER(rv), vik_trw_layer_interface.name );

  if ( vp == NULL || gtk_widget_get_window(GTK_WIDGET(vp)) == NULL ) {
    /* early exit, as the rest is GUI related */
    return rv;
  }

  rv->crosshair_cursor = gdk_cursor_new_for_display ( gtk_widget_get_display(GTK_WIDGET(vp)), GDK_CROSSHAIR );

  rv->wplabellayout = gtk_widget_create_pango_layout (GTK_WIDGET(vp), NULL);
  pango_layout_set_font_description (rv->wplabellayout, gtk_widget_get_style(GTK_WIDGET(vp))->font_desc);

  rv->tracklabellayout = gtk_widget_create_pango_layout (GTK_WIDGET(vp), NULL);
  pango_layout_set_font_description (rv->tracklabellayout, gtk_widget_get_style(GTK_WIDGET(vp))->font_desc);

  trw_layer_edit_track_gcs ( rv, vp );
  trw_layer_create_other_gcs ( rv, vp );

  (void)gdk_color_parse ( "#000000", &rv->black_color );
  (void)gdk_color_parse ( COLOR_STOP, &rv->stop_color );
  (void)gdk_color_parse ( COLOR_SLOW, &rv->slow_color );
  (void)gdk_color_parse ( COLOR_AVER, &rv->aver_color );
  (void)gdk_color_parse ( COLOR_FAST, &rv->fast_color );

  rv->coord_mode = vik_viewport_get_coord_mode ( vp );

  rv->menu_selection = vik_layer_get_interface(VIK_LAYER(rv)->type)->menu_items_selection;

  // only set to false if load really needed
  rv->external_loaded = TRUE;

  return rv;
}

#define SMALL_ICON_SIZE 18
/*
 * Can accept a null symbol, and may return null value
 */
GdkPixbuf* get_wp_sym_small ( gchar *symbol )
{
  GdkPixbuf* wp_icon = a_get_wp_sym (symbol);
  // ATM a_get_wp_sym returns a cached icon, with the size dependent on the preferences.
  //  So needing a small icon for the treeview may need some resizing:
  if ( wp_icon && gdk_pixbuf_get_width ( wp_icon ) != SMALL_ICON_SIZE )
    wp_icon = gdk_pixbuf_scale_simple ( wp_icon, SMALL_ICON_SIZE, SMALL_ICON_SIZE, GDK_INTERP_BILINEAR );
  return wp_icon;
}

static void trw_layer_realize_track ( gpointer id, VikTrack *track, gpointer pass_along[5] )
{
  GtkTreeIter *new_iter = g_malloc(sizeof(GtkTreeIter));

  GdkPixbuf *pixbuf = NULL;

  if ( track->has_color )
    pixbuf = ui_pixbuf_new ( &track->color, SMALL_ICON_SIZE, SMALL_ICON_SIZE );

  gdouble timestamp = 0;
  VikTrackpoint *tpt = vik_track_get_tp_first(track);
  if ( tpt && !isnan(tpt->timestamp) )
    timestamp = tpt->timestamp;

  vik_treeview_add_sublayer ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], track->name, pass_along[2], id, GPOINTER_TO_INT (pass_along[4]), pixbuf, TRUE, timestamp, track->number );

  if ( pixbuf )
    g_object_unref (pixbuf);

  *new_iter = *((GtkTreeIter *) pass_along[1]);
  if ( track->is_route )
    g_hash_table_insert ( VIK_TRW_LAYER(pass_along[2])->routes_iters, id, new_iter );
  else
    g_hash_table_insert ( VIK_TRW_LAYER(pass_along[2])->tracks_iters, id, new_iter );

  if ( ! track->visible )
    vik_treeview_item_set_visible ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[1], FALSE );
}

static void trw_layer_realize_waypoint ( gpointer id, VikWaypoint *wp, gpointer pass_along[5] )
{
  GtkTreeIter *new_iter = g_malloc(sizeof(GtkTreeIter));

  gdouble timestamp = 0;
  if ( !isnan(wp->timestamp) )
    timestamp = wp->timestamp;

  vik_treeview_add_sublayer ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[0], (GtkTreeIter *) pass_along[1], wp->name, pass_along[2], id, GPOINTER_TO_UINT (pass_along[4]), get_wp_sym_small (wp->symbol), TRUE, timestamp, 0 );

  *new_iter = *((GtkTreeIter *) pass_along[1]);
  g_hash_table_insert ( VIK_TRW_LAYER(pass_along[2])->waypoints_iters, id, new_iter );

  if ( ! wp->visible )
    vik_treeview_item_set_visible ( (VikTreeview *) pass_along[3], (GtkTreeIter *) pass_along[1], FALSE );
}

static void trw_layer_add_sublayer_tracks ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  vik_treeview_add_sublayer ( (VikTreeview *) vt, layer_iter, &(vtl->tracks_iter), _("Tracks"), vtl, NULL, VIK_TRW_LAYER_SUBLAYER_TRACKS, NULL, FALSE, 0, 0 );
}

static void trw_layer_add_sublayer_waypoints ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  vik_treeview_add_sublayer ( (VikTreeview *) vt, layer_iter, &(vtl->waypoints_iter), _("Waypoints"), vtl, NULL, VIK_TRW_LAYER_SUBLAYER_WAYPOINTS, NULL, FALSE, 0, 0 );
}

static void trw_layer_add_sublayer_routes ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  vik_treeview_add_sublayer ( (VikTreeview *) vt, layer_iter, &(vtl->routes_iter), _("Routes"), vtl, NULL, VIK_TRW_LAYER_SUBLAYER_ROUTES, NULL, FALSE, 0, 0 );
}

static void trw_layer_realize ( VikTrwLayer *vtl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  GtkTreeIter iter2;
  gpointer pass_along[5] = { &(vtl->tracks_iter), &iter2, vtl, vt, GINT_TO_POINTER(VIK_TRW_LAYER_SUBLAYER_TRACK) };

  if ( g_hash_table_size (vtl->tracks) > 0 ) {
    trw_layer_add_sublayer_tracks ( vtl, vt , layer_iter );

    g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_realize_track, pass_along );

    vik_treeview_item_set_visible ( vt, &(vtl->tracks_iter), vtl->tracks_visible );
  }

  if ( g_hash_table_size (vtl->routes) > 0 ) {
    trw_layer_add_sublayer_routes ( vtl, vt, layer_iter );

    pass_along[0] = &(vtl->routes_iter);
    pass_along[4] = GINT_TO_POINTER(VIK_TRW_LAYER_SUBLAYER_ROUTE);

    g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_realize_track, pass_along );

    vik_treeview_item_set_visible ( (VikTreeview *) vt, &(vtl->routes_iter), vtl->routes_visible );
  }

  if ( g_hash_table_size (vtl->waypoints) > 0 ) {
    trw_layer_add_sublayer_waypoints ( vtl, vt, layer_iter );

    pass_along[0] = &(vtl->waypoints_iter);
    pass_along[4] = GINT_TO_POINTER(VIK_TRW_LAYER_SUBLAYER_WAYPOINT);

    g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_realize_waypoint, pass_along );

    vik_treeview_item_set_visible ( (VikTreeview *) vt, &(vtl->waypoints_iter), vtl->waypoints_visible );
  }

  trw_layer_verify_thumbnails ( vtl );

  trw_layer_sort_all ( vtl );

  trw_update_layer_icon ( vtl );
}

// Overload subtype with -1 to close for a layer (no matter which specific track shown)
static void close_graphs_of_specific_track_or_type ( VikTrwLayer *vtl, VikTrack *trk, gint subtype )
{
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  gpointer gw = vik_window_get_graphs_widgets ( vw );
  if ( gw ) {
    vik_trw_and_track_t vt = vik_trw_layer_propwin_main_get_track ( gw );
    if ( vt.trk && (vt.trk == trk) ) {
      vik_window_close_graphs ( vw );
    }
    if ( vt.trk && ((vt.trk->is_route && subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES) ||
		    (!vt.trk->is_route && subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS)) )
      vik_window_close_graphs ( vw );
    if ( subtype < 0 && vt.vtl == vtl )
      vik_window_close_graphs ( vw );
  }
}

static gboolean trw_layer_sublayer_toggle_visible ( VikTrwLayer *l, gint subtype, gpointer sublayer )
{
  gboolean answer = TRUE;
  VikTrack *t = NULL;
  switch ( subtype )
  {
    case VIK_TRW_LAYER_SUBLAYER_TRACKS: answer = (l->tracks_visible ^= 1); break;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS: answer = (l->waypoints_visible ^= 1); break;
    case VIK_TRW_LAYER_SUBLAYER_ROUTES: answer = (l->routes_visible ^= 1); break;
    case VIK_TRW_LAYER_SUBLAYER_TRACK:
    {
      t = g_hash_table_lookup ( l->tracks, sublayer );
      if (t)
        answer = (t->visible ^= 1);
      break;
    }
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
    {
      VikWaypoint *t = g_hash_table_lookup ( l->waypoints, sublayer );
      if (t)
        answer = (t->visible ^= 1);
      break;
    }
    case VIK_TRW_LAYER_SUBLAYER_ROUTE:
    {
      t = g_hash_table_lookup ( l->routes, sublayer );
      if (t)
        answer = (t->visible ^= 1);
      break;
    }
    default: break;
  }
  if ( !answer ) {
    // Close associated graphs on main display if the specific route/track is toggled off
    //   or all routes/tracks toggled off and the same type was on display
    // ATM no method to restore graphs if it was previously shown
    close_graphs_of_specific_track_or_type ( l, t, subtype );
  }
  return answer;
}

static void trw_layer_layer_toggle_visible ( VikTrwLayer *vtl )
{
  // Is it no longer visible?
  if ( !VIK_LAYER(vtl)->visible )
    close_graphs_of_specific_track_or_type ( vtl, NULL, -1 );
}

/*
 * Return a property about tracks for this layer
 */
gint vik_trw_layer_get_property_tracks_line_thickness ( VikTrwLayer *vtl )
{
  return vtl->line_thickness;
}

/*
 * Build up multiple routes information
 */
static void trw_layer_routes_tooltip ( const gpointer id, VikTrack *tr, gdouble *length )
{
  *length = *length + vik_track_get_length (tr);
}

// Structure to hold multiple track information for a layer
typedef struct {
  gdouble length;
  time_t  start_time;
  time_t  end_time;
  gint    duration;
  // Single position for the layer used for the potential timezone
  VikCoord *coord;
} tooltip_tracks;

/*
 * Build up layer multiple track information via updating the tooltip_tracks structure
 */
static void trw_layer_tracks_tooltip ( const gpointer id, VikTrack *tr, tooltip_tracks *tt )
{
  tt->length = tt->length + vik_track_get_length (tr);

  // Ensure times are available
  if ( tr->trackpoints && !isnan(vik_track_get_tp_first(tr)->timestamp) ) {
    // Position of first trackpoint is good enough for timezone;
    //  rather than working out the center of the layer, since the tooltip can be called often
    if ( !tt->coord ) {
      tt->coord = &vik_track_get_tp_first(tr)->coord;
    }
    // Get trkpt only once - as using vik_track_get_tp_last() iterates whole track each time
    VikTrackpoint *trkpt_last = vik_track_get_tp_last(tr);
    if ( !isnan(trkpt_last->timestamp) ) {
      // Seconds precision is good enough for the tooltip
      time_t t1 = (time_t)vik_track_get_tp_first(tr)->timestamp;
      time_t t2 = (time_t)trkpt_last->timestamp;

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
}

/*
 * Generate tooltip text for the layer.
 * This is relatively complicated as it considers information for
 *   no tracks, a single track or multiple tracks
 *     (which may or may not have timing information)
 */
static const gchar* trw_layer_layer_tooltip ( VikTrwLayer *vtl )
{
  gchar tbuf1[64];
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
    tooltip_tracks tt = { 0.0, 0, 0, 0, NULL };
    g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_tracks_tooltip, &tt );

    GDate* gdate_start = NULL;
    GDate* gdate_end = NULL;

    // Put time information into tbuf3
    // When differing dates - put on separate line
    switch ( a_vik_get_time_ref_frame() ) {
    case VIK_TIME_REF_UTC:
      {
        strftime ( tbuf1, sizeof(tbuf1), "%x", gmtime(&tt.start_time) );
        strftime ( tbuf2, sizeof(tbuf2), "%x", gmtime(&tt.end_time) );
        if ( g_strcmp0 ( tbuf1, tbuf2 ) != 0 )
          g_snprintf ( tbuf3, sizeof(tbuf3), _("%s to %s\n"), tbuf1, tbuf2 );
        else
          g_snprintf ( tbuf3, sizeof(tbuf3), "%s: ", tbuf1 );
      }
      break;
    case VIK_TIME_REF_LOCALE:
      {
        gdate_start = g_date_new ();
        g_date_set_time_t (gdate_start, tt.start_time);

        gdate_end = g_date_new ();
        g_date_set_time_t (gdate_end, tt.end_time);

        if ( g_date_compare (gdate_start, gdate_end) ) {
          // Dates differ so print range on separate line
          g_date_strftime (tbuf1, sizeof(tbuf1), "%x", gdate_start);
          g_date_strftime (tbuf2, sizeof(tbuf2), "%x", gdate_end);
          g_snprintf ( tbuf3, sizeof(tbuf3), _("%s to %s\n"), tbuf1, tbuf2 );
        }
        else {
          // Same date so just show it and keep rest of text on the same line - provided it's a valid time!
          if ( tt.start_time != 0 )// {
            g_date_strftime ( tbuf3, sizeof(tbuf3), "%x: ", gdate_start );
        }
      }
      break;
      // case VIK_TIME_REF_WORLD:
    default:
      {
        gchar *time1 = vu_get_time_string ( &tt.start_time, "%x", tt.coord, NULL );
        gchar *time2 = vu_get_time_string ( &tt.end_time, "%x", tt.coord, NULL );
        if ( g_strcmp0 ( time1, time2 ) != 0 )
          g_snprintf ( tbuf3, sizeof(tbuf3), _("%s to %s\n"), time1, time2 );
        else
          g_snprintf ( tbuf3, sizeof(tbuf3), "%s: ", time1 );
        g_free ( time1 );
        g_free ( time2 );
      }
      break;
    }

    tbuf2[0] = '\0';
    if ( tt.length > 0.0 ) {
      gdouble len_in_units;

      // Setup info dependent on distance units
      switch ( a_vik_get_units_distance() ) {
      case VIK_UNITS_DISTANCE_MILES:
        g_snprintf (tbuf4, sizeof(tbuf4), "miles");
        len_in_units = VIK_METERS_TO_MILES(tt.length);
        break;
      case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
        g_snprintf (tbuf4, sizeof(tbuf4), "NM");
        len_in_units = VIK_METERS_TO_NAUTICAL_MILES(tt.length);
        break;
      default:
        g_snprintf (tbuf4, sizeof(tbuf4), "kms");
        len_in_units = tt.length/1000.0;
        break;
      }

      // Timing information if available
      tbuf1[0] = '\0';
      if ( tt.duration > 0 ) {
        guint hours, minutes, seconds;
        util_time_decompose ( tt.duration, &hours, &minutes, &seconds );
        // Less than *nearly* an hour, show minutes + seconds
        if ( tt.duration < 3570 ) {
          g_snprintf ( tbuf1, sizeof(tbuf1), _(" in %d:%02d mins:secs"), minutes, seconds );
        } else {
           // Round to nearest minute, updating the hour value if necessary
          if ( seconds > 30 ) minutes++;
          if ( minutes == 60 ) {
            minutes = 0;
            hours++;
          }
          g_snprintf ( tbuf1, sizeof(tbuf1), _(" in %d:%02d hrs:mins"), hours, minutes );
        }
      }
      g_snprintf (tbuf2, sizeof(tbuf2),
		  _("\n%sTotal Length %.1f %s%s"),
		  tbuf3, len_in_units, tbuf4, tbuf1);
    }

    tbuf1[0] = '\0';
    gdouble rlength = 0.0;
    g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_routes_tooltip, &rlength );
    if ( rlength > 0.0 ) {
      gdouble len_in_units;
      // Setup info dependent on distance units
      switch ( a_vik_get_units_distance() ) {
      case VIK_UNITS_DISTANCE_MILES:
        g_snprintf (tbuf4, sizeof(tbuf4), "miles");
        len_in_units = VIK_METERS_TO_MILES(rlength);
        break;
      case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
        g_snprintf (tbuf4, sizeof(tbuf4), "NM");
        len_in_units = VIK_METERS_TO_NAUTICAL_MILES(rlength);
        break;
      default:
        g_snprintf (tbuf4, sizeof(tbuf4), "kms");
        len_in_units = rlength/1000.0;
        break;
      }
      g_snprintf (tbuf1, sizeof(tbuf1), _("\nTotal route length %.1f %s"), len_in_units, tbuf4);
    }

    // Put together all the elements to form compact tooltip text
    g_snprintf (tmp_buf, sizeof(tmp_buf),
                _("Tracks: %d - Waypoints: %d - Routes: %d%s%s"),
                g_hash_table_size (vtl->tracks), g_hash_table_size (vtl->waypoints), g_hash_table_size (vtl->routes), tbuf2, tbuf1);

    if ( gdate_start )
      g_date_free ( gdate_start );
    if ( gdate_end )
      g_date_free ( gdate_end );
  }

  return tmp_buf;
}

static const gchar* trw_layer_sublayer_tooltip ( VikTrwLayer *l, gint subtype, gpointer sublayer )
{
  switch ( subtype )
  {
    case VIK_TRW_LAYER_SUBLAYER_TRACKS:
    {
      // Very simple tooltip - may expand detail in the future...
      static gchar tmp_buf[32];
      g_snprintf (tmp_buf, sizeof(tmp_buf),
                  _("Tracks: %d"),
                  g_hash_table_size (l->tracks));
      return tmp_buf;
    }
    break;
    case VIK_TRW_LAYER_SUBLAYER_ROUTES:
    {
      // Very simple tooltip - may expand detail in the future...
      static gchar tmp_buf[32];
      g_snprintf (tmp_buf, sizeof(tmp_buf),
                  _("Routes: %d"),
                  g_hash_table_size (l->routes));
      return tmp_buf;
    }
    break;

    case VIK_TRW_LAYER_SUBLAYER_ROUTE:
      // Same tooltip for a route
    case VIK_TRW_LAYER_SUBLAYER_TRACK:
    {
      VikTrack *tr;
      if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
        tr = g_hash_table_lookup ( l->tracks, sublayer );
      else
        tr = g_hash_table_lookup ( l->routes, sublayer );

      if ( tr ) {
	// Could be a better way of handling strings - but this works...
	gchar time_buf1[20];
	gchar time_buf2[20];
	time_buf1[0] = '\0';
	time_buf2[0] = '\0';
	static gchar tmp_buf[100];
	// Compact info: Short date eg (11/20/99), duration and length
	// Hopefully these are the things that are most useful and so promoted into the tooltip
	if ( tr->trackpoints && !isnan(vik_track_get_tp_first(tr)->timestamp) ) {
          VikTrackpoint *tp1 = vik_track_get_tp_first ( tr );
          time_t first = tp1->timestamp;
          // %x     The preferred date representation for the current locale without the time.
          gchar *time_str = vu_get_time_string ( &first, "%x: ", &tp1->coord, NULL );
          g_strlcpy ( time_buf1, time_str, sizeof(time_buf1) );
          g_free ( time_str );
          gdouble dur = vik_track_get_duration ( tr, TRUE );
          if ( dur > 0 ) {
            guint hours, minutes, seconds;
            util_time_decompose ( dur, &hours, &minutes, &seconds );
            // Less than *nearly* an hour, show minutes + seconds
            if ( dur < 3570 ) {
              g_snprintf ( time_buf2, sizeof(time_buf2), _("- %d:%02d mins:secs"), minutes, seconds );
            } else {
              // Round to nearest minute, updating the hour value if necessary
              if ( seconds > 30 ) minutes++;
              if ( minutes == 60 ) {
                minutes = 0;
                hours++;
              }
              g_snprintf ( time_buf2, sizeof(time_buf2), _("- %d:%02d hrs:mins"), hours, minutes );
            }
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
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
	  g_snprintf (tmp_buf, sizeof(tmp_buf), _("%s%.1f NM %s"), time_buf1, VIK_METERS_TO_NAUTICAL_MILES(tr_len), time_buf2);
	  break;
	default:
	  break;
	}
	return tmp_buf;
      }
    }
    break;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS:
    {
      // Very simple tooltip - may expand detail in the future...
      static gchar tmp_buf[32];
      g_snprintf (tmp_buf, sizeof(tmp_buf),
                  _("Waypoints: %d"),
                  g_hash_table_size (l->waypoints));
      return tmp_buf;
    }
    break;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
    {
      VikWaypoint *w = g_hash_table_lookup ( l->waypoints, sublayer );
      // NB It's OK to return NULL
      if ( w ) {
        if ( w->comment )
          return w->comment;
        else
          return w->description;
      }
    }
    break;
    default: break;
  }
  return NULL;
}

#define VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT "trkpt_selected_statusbar_format"

/**
 * set_statusbar_msg_info_trkpt:
 *
 * Function to show track point information on the statusbar
 *  Items displayed is controlled by the settings format code
 */
static void set_statusbar_msg_info_trkpt ( VikTrwLayer *vtl, VikTrackpoint *trkpt )
{
  gchar *statusbar_format_code = NULL;
  VikTrackpoint *trkpt_prev = NULL;
  if ( !a_settings_get_string ( VIK_SETTINGS_TRKPT_SELECTED_STATUSBAR_FORMAT, &statusbar_format_code ) ) {
    // Otherwise use default
    statusbar_format_code = g_strdup ( "KEATDN" );
  }
  else {
    // Format code may want to show speed - so may need previous trkpt to work it out
    trkpt_prev = vik_track_get_tp_prev ( vtl->current_tp_track, trkpt );
  }

  gchar *msg = vu_trackpoint_formatted_message ( statusbar_format_code, trkpt, trkpt_prev, vtl->current_tp_track, NAN );
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, msg );
  g_free ( msg );
  g_free ( statusbar_format_code );
}

/*
 * Function to show basic waypoint information on the statusbar
 */
static void set_statusbar_msg_info_wpt ( VikTrwLayer *vtl, VikWaypoint *wpt )
{
  gchar tmp_buf1[64];
  if ( isnan(wpt->altitude) ) {
    g_snprintf ( tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt --") );
  } else {
    switch (a_vik_get_units_height ()) {
    case VIK_UNITS_HEIGHT_FEET:
      g_snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dft"), (int)round(VIK_METERS_TO_FEET(wpt->altitude)));
      break;
    default:
      //VIK_UNITS_HEIGHT_METRES:
      g_snprintf(tmp_buf1, sizeof(tmp_buf1), _("Wpt: Alt %dm"), (int)round(wpt->altitude));
    }
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
 *
 */
static void close_vw_graphs_if_layer_different ( gpointer props, VikWindow *vw, VikTrwLayer *vtl )
{
  if ( props ) {
    vik_trw_and_track_t vt = vik_trw_layer_propwin_main_get_track ( props );
    if ( vt.vtl != vtl ) {
      vik_window_close_graphs ( vw );
    }
  }
}

/**
 * Returns TRUE if graphs to be (or continued to be) shown
 */
static gboolean show_graphs_for_track ( gpointer gw, VikWindow *vw, VikTrwLayer *vtl, VikTrack *track )
{
  // NB If the same track selected do nothing
  // Check the layer for visibility (including all the parents visibilities)
  if ( track->visible && vik_treeview_item_get_visible_tree(VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter)) ) {
    if ( gw ) {
      vik_trw_and_track_t vt = vik_trw_layer_propwin_main_get_track ( gw );
      if ( vt.trk == track ) {
        return TRUE;
      } else {
        vik_window_close_graphs ( vw );
      }
    }
    GtkWidget *graphs = vik_window_get_graphs_widget ( vw );
    VikViewport *vvp = vik_window_viewport ( vw );
    gboolean show = vik_window_get_graphs_widgets_shown ( vw );
    vik_window_set_graphs_widgets ( vw, vik_trw_layer_propwin_main(GTK_WINDOW(vw), vtl, track, vvp, graphs, show) );
    return TRUE;
  }
  // Not visible so close
  vik_window_close_graphs ( vw );
  return FALSE;
}

static void maybe_show_graph ( VikWindow *vw, VikTrwLayer *vtl, gpointer gw )
{
  VikTrack *trk = NULL;

  if ( a_vik_get_show_graph_for_trwlayer() ) {
    trk = vik_trw_layer_get_only_track ( vtl );
  }

  if ( trk )
    show_graphs_for_track ( gw, vw, vtl, trk );
  else
    close_vw_graphs_if_layer_different ( gw, vw, vtl );
}

/**
 * General layer selection function, find out which bit is selected and take appropriate action
 * Also open or close the associated graphs on the main view, according to what is selected
 */
static gboolean trw_layer_selected ( VikTrwLayer *l, gint subtype, gpointer sublayer, gint type, gpointer vlp )
{
  trw_ensure_layer_loaded ( l );
  
  // Reset
  l->current_wp    = NULL;
  l->current_wp_id = NULL;
  trw_layer_cancel_current_tp ( l, FALSE );

  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l));
  // Clear statusbar
  vik_statusbar_set_message ( vik_window_get_statusbar(vw), VIK_STATUSBAR_INFO, "" );

  gpointer gw = vik_window_get_graphs_widgets ( vw );

  switch ( type )
    {
    case VIK_TREEVIEW_TYPE_LAYER:
      {
        vik_window_set_selected_trw_layer ( vw, l );
        maybe_show_graph ( vw, l, gw );
        return TRUE; // Mark for redraw
      }
      break;

    case VIK_TREEVIEW_TYPE_SUBLAYER:
      {
	switch ( subtype )
	  {
	  case VIK_TRW_LAYER_SUBLAYER_TRACKS:
	    {
              vik_window_set_selected_tracks ( vw, l->tracks, l );
              close_vw_graphs_if_layer_different ( gw, vw, l );
              return TRUE; // Mark for redraw
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_TRACK:
	    {
	      VikTrack *track = g_hash_table_lookup ( l->tracks, sublayer );
              vik_window_set_selected_track ( vw, (gpointer)track, l );
              if ( show_graphs_for_track(gw, vw, l, track) )
                return TRUE; // Mark for redraw
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_ROUTES:
	    {
	      vik_window_set_selected_tracks ( vw, l->routes, l );
              close_vw_graphs_if_layer_different ( gw, vw, l );
              return TRUE; // Mark for redraw
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_ROUTE:
	    {
	      VikTrack *track = g_hash_table_lookup ( l->routes, sublayer );
	      vik_window_set_selected_track ( vw, (gpointer)track, l );
              if ( show_graphs_for_track(gw, vw, l, track) )
                return TRUE; // Mark for redraw
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_WAYPOINTS:
	    {
              vik_window_set_selected_waypoints ( vw, l->waypoints, l );
              close_vw_graphs_if_layer_different ( gw, vw, l );
              return TRUE; // Mark for redraw
	    }
	    break;
	  case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
	    {
              VikWaypoint *wpt = g_hash_table_lookup ( l->waypoints, sublayer );
              if ( wpt ) {
                vik_window_set_selected_waypoint ( vw, (gpointer)wpt, l );
                // Show some waypoint info
                set_statusbar_msg_info_wpt ( l, wpt );
                close_vw_graphs_if_layer_different ( gw, vw, l );
                return TRUE; // Mark for redraw
              }
	    }
	    break;
	  default:
	    {
              return vik_window_clear_selected ( vw );
	    }
	    break;
	  }
	return FALSE;
      }
      break;

    default:
      return vik_window_clear_selected ( vw );
      break;
    }
}

GHashTable *vik_trw_layer_get_tracks ( VikTrwLayer *l )
{
  return l->tracks;
}

GHashTable *vik_trw_layer_get_routes ( VikTrwLayer *l )
{
  return l->routes;
}

GHashTable *vik_trw_layer_get_waypoints ( VikTrwLayer *l )
{
  return l->waypoints;
}

GHashTable *vik_trw_layer_get_tracks_iters ( VikTrwLayer *vtl )
{
  return vtl->tracks_iters;
}

GHashTable *vik_trw_layer_get_routes_iters ( VikTrwLayer *vtl )
{
  return vtl->routes_iters;
}

GHashTable *vik_trw_layer_get_waypoints_iters ( VikTrwLayer *vtl )
{
  return vtl->waypoints_iters;
}

gboolean vik_trw_layer_is_empty ( VikTrwLayer *vtl )
{
  return ! ( g_hash_table_size ( vtl->tracks ) ||
             g_hash_table_size ( vtl->routes ) ||
             g_hash_table_size ( vtl->waypoints ) );
}

/**
 * Returns: a #VikTrack if there is only one track or only one route in the layer
 * (irrespective of the number of waypoints), otherwise returns NULL
 */
VikTrack *vik_trw_layer_get_only_track ( VikTrwLayer *vtl )
{
   VikTrack *trk = NULL;

   GHashTableIter iter;
   gpointer key, value;

   // Get the track or route if there is only one of these in the layer
   guint szr = g_hash_table_size ( vtl->routes );
   guint szt = g_hash_table_size ( vtl->tracks );
   if ( szr == 1 && szt == 0 ) {
     g_hash_table_iter_init ( &iter, vtl->routes );
     g_hash_table_iter_next ( &iter, &key, &value );
     trk = VIK_TRACK(value);
   } else if ( szr == 0 && szt == 1 ) {
     g_hash_table_iter_init ( &iter, vtl->tracks );
     g_hash_table_iter_next ( &iter, &key, &value );
     trk = VIK_TRACK(value);
   }

   return trk;
}

gboolean vik_trw_layer_get_tracks_visibility ( VikTrwLayer *vtl )
{
  return vtl->tracks_visible;
}

gboolean vik_trw_layer_get_routes_visibility ( VikTrwLayer *vtl )
{
  return vtl->routes_visible;
}

gboolean vik_trw_layer_get_waypoints_visibility ( VikTrwLayer *vtl )
{
  return vtl->waypoints_visible;
}

gboolean vik_trw_layer_get_prefer_gps_speed ( VikTrwLayer *vtl )
{
  return vtl->prefer_gps_speed;
}

/*
 * ATM use a case sensitive find
 * Finds the first one
 */
static gboolean trw_layer_waypoint_find ( const gpointer id, const VikWaypoint *wp, const gchar *name )
{
  if ( wp && wp->name )
    if ( ! strcmp ( wp->name, name ) )
      return TRUE;
  return FALSE;
}

/*
 * Get waypoint by name - not guaranteed to be unique
 * Finds the first one
 */
VikWaypoint *vik_trw_layer_get_waypoint ( VikTrwLayer *vtl, const gchar *name )
{
  return g_hash_table_find ( vtl->waypoints, (GHRFunc) trw_layer_waypoint_find, (gpointer) name );
}

/*
 * ATM use a case sensitive find
 * Finds the first one
 */
static gboolean trw_layer_track_find ( const gpointer id, const VikTrack *trk, const gchar *name )
{
  if ( trk && trk->name )
    if ( ! strcmp ( trk->name, name ) )
      return TRUE;
  return FALSE;
}

/*
 * Get track by name - not guaranteed to be unique
 * Finds the first one
 */
VikTrack *vik_trw_layer_get_track ( VikTrwLayer *vtl, const gchar *name )
{
  return g_hash_table_find ( vtl->tracks, (GHRFunc) trw_layer_track_find, (gpointer) name );
}

/*
 * Get route by name - not guaranteed to be unique
 * Finds the first one
 */
VikTrack *vik_trw_layer_get_route ( VikTrwLayer *vtl, const gchar *name )
{
  return g_hash_table_find ( vtl->routes, (GHRFunc) trw_layer_track_find, (gpointer) name );
}

static void trw_layer_find_maxmin_tracks ( const gpointer id, const VikTrack *trk, struct LatLon maxmin[2] )
{
  if ( trk->bbox.north > maxmin[0].lat || maxmin[0].lat == 0.0 )
    maxmin[0].lat = trk->bbox.north;
  if ( trk->bbox.south < maxmin[1].lat || maxmin[1].lat == 0.0 )
    maxmin[1].lat = trk->bbox.south;
  if ( trk->bbox.east > maxmin[0].lon || maxmin[0].lon == 0.0 )
    maxmin[0].lon = trk->bbox.east;
  if ( trk->bbox.west < maxmin[1].lon || maxmin[1].lon == 0.0 )
    maxmin[1].lon = trk->bbox.west;
}

static void trw_layer_find_maxmin (VikTrwLayer *vtl, struct LatLon maxmin[2])
{
  // Continually reuse maxmin to find the latest maximum and minimum values
  // First set to waypoints bounds
  maxmin[0].lat = vtl->waypoints_bbox.north;
  maxmin[1].lat = vtl->waypoints_bbox.south;
  maxmin[0].lon = vtl->waypoints_bbox.east;
  maxmin[1].lon = vtl->waypoints_bbox.west;
  g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_find_maxmin_tracks, maxmin );
  g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_find_maxmin_tracks, maxmin );
}

LatLonBBox vik_trw_layer_get_bbox ( VikTrwLayer *vtl )
{
  struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };
  trw_layer_find_maxmin (vtl, maxmin);
  LatLonBBox bbox;
  bbox.south = maxmin[1].lat;
  bbox.north = maxmin[0].lat;
  bbox.east  = maxmin[0].lon;
  bbox.west  = maxmin[1].lon;
  return bbox;
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

static void trw_layer_centerize ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikCoord coord;
  if ( vik_trw_layer_find_center ( vtl, &coord ) )
    goto_coord ( values[MA_VLP], NULL, NULL, &coord );
  else
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This layer has no waypoints or trackpoints.") );
}

void trw_layer_zoom_to_show_latlons ( VikTrwLayer *vtl, VikViewport *vvp, struct LatLon maxmin[2] )
{
  vu_zoom_to_show_latlons ( vtl->coord_mode, vvp, maxmin );
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

static void trw_layer_auto_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  if ( vik_trw_layer_auto_set_view ( vtl, vik_layers_panel_get_viewport (vlp) ) ) {
    vik_layers_panel_emit_update ( vlp );
  }
  else
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This layer has no waypoints or trackpoints.") );
}

static void trw_layer_export_gpspoint ( menu_array_layer values )
{
  gchar *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GPSPOINT );

  vik_trw_layer_export ( VIK_LAYER(values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSPOINT );

  g_free ( auto_save_name );
}

static void trw_layer_export_gpsmapper ( menu_array_layer values )
{
  gchar *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GPSMAPPER );

  vik_trw_layer_export ( VIK_LAYER(values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPSMAPPER );

  g_free ( auto_save_name );
}

static void trw_layer_export_gpx ( menu_array_layer values )
{
  gchar *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GPX );

  vik_trw_layer_export ( VIK_LAYER(values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GPX );

  g_free ( auto_save_name );
}

static void trw_layer_export_kml ( menu_array_layer values )
{
  gchar *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_KML );

  vik_trw_layer_export ( VIK_LAYER(values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_KML );

  g_free ( auto_save_name );
}

static void trw_layer_export_geojson ( menu_array_layer values )
{
  gchar *auto_save_name = append_file_ext ( vik_layer_get_name(VIK_LAYER(values[MA_VTL])), FILE_TYPE_GEOJSON );

  vik_trw_layer_export ( VIK_LAYER(values[MA_VTL]), _("Export Layer"), auto_save_name, NULL, FILE_TYPE_GEOJSON );

  g_free ( auto_save_name );
}

static void trw_layer_export_babel ( gpointer layer_and_vlp[2] )
{
  const gchar *auto_save_name = vik_layer_get_name(VIK_LAYER(layer_and_vlp[0]));
  vik_trw_layer_export_gpsbabel ( VIK_TRW_LAYER (layer_and_vlp[0]), _("Export Layer"), auto_save_name );
}

static void trw_layer_export_external_gpx_1 ( menu_array_layer values )
{
  vik_trw_layer_export_external_gpx ( VIK_TRW_LAYER (values[MA_VTL]), a_vik_get_external_gpx_program_1() );
}

static void trw_layer_export_external_gpx_2 ( menu_array_layer values )
{
  vik_trw_layer_export_external_gpx ( VIK_TRW_LAYER (values[MA_VTL]), a_vik_get_external_gpx_program_2() );
}

static void trw_layer_export_gpx_track ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !trk || !trk->name )
    return;

  gchar *auto_save_name = append_file_ext ( trk->name, FILE_TYPE_GPX );

  gchar *label = NULL;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    label = _("Export Route as GPX");
  else
    label = _("Export Track as GPX");
  vik_trw_layer_export ( VIK_LAYER(values[MA_VTL]), label, auto_save_name, trk, FILE_TYPE_GPX );

  g_free ( auto_save_name );
}

static void trw_layer_export_external_text ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gchar *extfile_full = util_make_absolute_filename ( vtl->external_file, vtl->external_dirpath );
  gchar *extfile = extfile_full ? extfile_full : vtl->external_file;
  GError *err = NULL;
  gchar *quoted_file = g_shell_quote ( extfile );
  gchar *cmd = g_strdup_printf ( "%s %s", text_program, quoted_file );
  g_free ( quoted_file );
  g_free ( extfile_full );

  if ( ! g_spawn_command_line_async ( cmd, &err ) ) {
    a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER( vtl), _("Could not launch %s."), text_program );
    g_error_free ( err );
  }
  g_free ( cmd );
}

gboolean trw_layer_waypoint_find_uuid ( const gpointer id, const VikWaypoint *wp, gpointer udata )
{
  wpu_udata *user_data = udata;
  if ( wp == user_data->wp ) {
    user_data->uuid = id;
    return TRUE;
  }
  return FALSE;
}

static void trw_layer_goto_wp ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  GtkWidget *dia = gtk_dialog_new_with_buttons (_("Find"),
                                                 VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_STOCK_CANCEL,
                                                 GTK_RESPONSE_REJECT,
                                                 GTK_STOCK_OK,
                                                 GTK_RESPONSE_ACCEPT,
                                                 NULL);

  GtkWidget *label, *entry;
  label = gtk_label_new(_("Waypoint Name:"));
  entry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );

  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), entry, FALSE, FALSE, 0);
  gtk_widget_show_all ( dia );
  // 'ok' when press return in the entry
  g_signal_connect_swapped ( entry, "activate", G_CALLBACK(a_dialog_response_accept), dia );
  gtk_dialog_set_default_response ( GTK_DIALOG(dia), GTK_RESPONSE_ACCEPT );

  while ( gtk_dialog_run ( GTK_DIALOG(dia) ) == GTK_RESPONSE_ACCEPT )
  {
    gchar *name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    // Find *first* wp with the given name
    VikWaypoint *wp = vik_trw_layer_get_waypoint ( vtl, name );

    if ( !wp )
      a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Waypoint not found in this layer.") );
    else
    {
      vik_viewport_set_center_coord ( vik_layers_panel_get_viewport(vlp), &(wp->coord), TRUE );
      vik_layers_panel_emit_update ( vlp );

      // Find and select on the side panel
      wpu_udata udata;
      udata.wp   = wp;
      udata.uuid = NULL;

      // Hmmm, want key of it
      gpointer wpf = g_hash_table_find ( vtl->waypoints, (GHRFunc) trw_layer_waypoint_find_uuid, (gpointer) &udata );

      if ( wpf && udata.uuid ) {
        GtkTreeIter *it = g_hash_table_lookup ( vtl->waypoints_iters, udata.uuid );
        vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, it, TRUE );
      }

      break;
    }

    g_free ( name );

  }
  gtk_widget_destroy ( dia );
}

gboolean vik_trw_layer_new_waypoint ( VikTrwLayer *vtl, GtkWindow *w, const VikCoord *def_coord )
{
  gchar *default_name = highest_wp_number_get(vtl);
  VikWaypoint *wp = vik_waypoint_new();
  wp->coord = *def_coord;
  
  // Attempt to auto set height if DEM data is available
  vik_waypoint_apply_dem_data ( wp, TRUE );

  gboolean is_created = GPOINTER_TO_UINT(vik_trw_layer_wpwin_show ( w, NULL, default_name, vtl, wp, vtl->coord_mode, TRUE ));

  if ( is_created ) {
    vik_trw_layer_add_waypoint ( vtl, NULL, wp );
    g_free (default_name);
    return TRUE;
  }
  g_free (default_name);
  vik_waypoint_free(wp);
  return FALSE;
}

static void trw_layer_new_wikipedia_wp_viewport ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp = vik_window_viewport(vw);
  LatLonBBox bbox = vik_viewport_get_bbox ( vvp );
  a_geonames_wikipedia_box ( vw, vtl, bbox );
  trw_layer_calculate_bounds_waypoints ( vtl );
  vik_layers_panel_emit_update ( vlp );
}

static void trw_layer_new_wikipedia_wp_layer ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  a_geonames_wikipedia_box ( (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vtl, vik_trw_layer_get_bbox(vtl) );
  trw_layer_calculate_bounds_waypoints ( vtl );
  vik_layers_panel_emit_update ( vlp );
}

#ifdef VIK_CONFIG_GEOTAG
static void trw_layer_geotagging_waypoint_mtime_keep ( menu_array_sublayer values )
{
  VikWaypoint *wp = g_hash_table_lookup ( VIK_TRW_LAYER(values[MA_VTL])->waypoints, values[MA_SUBLAYER_ID] );
  if ( wp )
    // Update directly - not changing the mtime
    a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, wp->image_direction, wp->image_direction_ref, TRUE );
}

static void trw_layer_geotagging_waypoint_mtime_update ( menu_array_sublayer values )
{
  VikWaypoint *wp = g_hash_table_lookup ( VIK_TRW_LAYER(values[MA_VTL])->waypoints, values[MA_SUBLAYER_ID] );
  if ( wp )
    // Update directly
    a_geotag_write_exif_gps ( wp->image, wp->coord, wp->altitude, wp->image_direction, wp->image_direction_ref, FALSE );
}

/*
 * Use code in separate file for this feature as reasonably complex
 */
static void trw_layer_geotagging_track ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikTrack *track = g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  // Unset so can be reverified later if necessary
  vtl->has_verified_thumbnails = FALSE;

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            vtl,
                            NULL,
                            track );
}

static void trw_layer_geotagging_waypoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikWaypoint *wpt = g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            vtl,
                            wpt,
                            NULL );
}

static void trw_layer_geotagging ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Unset so can be reverified later if necessary
  vtl->has_verified_thumbnails = FALSE;

  trw_layer_geotag_dialog ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            vtl,
                            NULL,
                            NULL );
}
#endif

// 'Acquires' - Same as in File Menu -> Acquire - applies into the selected TRW Layer //

static void trw_layer_acquire ( menu_array_layer values, VikDataSourceInterface *datasource )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikViewport *vvp =  vik_window_viewport(vw);

  vik_datasource_mode_t mode = datasource->mode;
  if ( mode == VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT )
    mode = VIK_DATASOURCE_ADDTOLAYER;
  a_acquire ( vw, vlp, vvp, mode, datasource, NULL, NULL );
}

/*
 * Acquire into this TRW Layer straight from GPS Device
 */
static void trw_layer_acquire_gps_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_gps_interface );
}

/*
 * Acquire into this TRW Layer from Directions
 */
static void trw_layer_acquire_routing_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_routing_interface );
}

/*
 * Acquire into this TRW Layer from an entered URL
 */
static void trw_layer_acquire_url_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_url_interface );
}

#ifdef VIK_CONFIG_OPENSTREETMAP
/*
 * Acquire into this TRW Layer from OSM
 */
static void trw_layer_acquire_osm_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_osm_interface );
}

/**
 * Acquire into this TRW Layer from OSM for 'My' Traces
 */
static void trw_layer_acquire_osm_my_traces_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_osm_my_traces_interface );
}
#endif

#ifdef VIK_CONFIG_GEOCACHES
/*
 * Acquire into this TRW Layer from Geocaching.com
 */
static void trw_layer_acquire_geocache_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_gc_interface );
}
#endif

#ifdef VIK_CONFIG_GEOTAG
/*
 * Acquire into this TRW Layer from images
 */
static void trw_layer_acquire_geotagged_cb ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  trw_layer_acquire ( values, &vik_datasource_geotag_interface );

  // Reverify thumbnails as they may have changed
  vtl->has_verified_thumbnails = FALSE;
  trw_layer_verify_thumbnails ( vtl );
}
#endif

/*
 * Acquire into this TRW Layer from any GPS Babel supported file
 */
static void trw_layer_acquire_file_cb ( menu_array_layer values )
{
  trw_layer_acquire ( values, &vik_datasource_file_interface );
}

static void trw_layer_gps_upload ( menu_array_layer values )
{
  menu_array_sublayer data;
  gint ii;
  for ( ii = MA_VTL; ii < MA_LAST; ii++ )
    data[ii] = NULL;
  data[MA_VTL] = values[MA_VTL];
  data[MA_VLP] = values[MA_VLP];

  trw_layer_gps_upload_any ( data );
}

/**
 * If pass_along[3] is defined that this will upload just that track
 */
static void trw_layer_gps_upload_any ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);

  // May not actually get a track here as values[2&3] can be null
  VikTrack *track = NULL;
  vik_gps_xfer_type xfer_type = TRK; // VIK_TRW_LAYER_SUBLAYER_TRACKS = 0 so hard to test different from NULL!
  gboolean xfer_all = FALSE;

  if ( values[MA_SUBTYPE] ) {
    xfer_all = FALSE;
    if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
      xfer_type = RTE;
    }
    else if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
      xfer_type = TRK;
    }
    else if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS ) {
      xfer_type = WPT;
    }
    else if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
      xfer_type = RTE;
    }
  }
  else if ( !values[MA_CONFIRM] )
    xfer_all = TRUE; // i.e. whole layer

  if (track && !track->visible) {
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Can not upload invisible track.") );
    return;
  }

  GtkWidget *dialog = gtk_dialog_new_with_buttons ( _("GPS Upload"),
                                                    VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_STOCK_OK,
                                                    GTK_RESPONSE_ACCEPT,
                                                    GTK_STOCK_CANCEL,
                                                    GTK_RESPONSE_REJECT,
                                                    NULL );

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  gpointer dgs = datasource_gps_setup ( dialog, xfer_type, xfer_all );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
    datasource_gps_clean_up ( dgs );
    gtk_widget_destroy ( dialog );
    return;
  }

  // Get info from reused datasource dialog widgets
  gchar* protocol = datasource_gps_get_protocol ( dgs );
  gchar* port = datasource_gps_get_descriptor ( dgs );
  // NB don't free the above strings as they're references to values held elsewhere
  gboolean do_tracks = datasource_gps_get_do_tracks ( dgs );
  gboolean do_routes = datasource_gps_get_do_routes ( dgs );
  gboolean do_waypoints = datasource_gps_get_do_waypoints ( dgs );
  gboolean turn_off = datasource_gps_get_off ( dgs );

  gtk_widget_destroy ( dialog );

  // When called from the viewport - work the corresponding layerspanel:
  if ( !vlp ) {
    vlp = vik_window_layers_panel ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
  }

  // Apply settings to transfer to the GPS device
  vik_gps_comm ( vtl,
                 track,
                 GPS_UP,
                 protocol,
                 port,
                 FALSE,
                 vik_layers_panel_get_viewport (vlp),
                 vlp,
                 do_tracks,
                 do_routes,
                 do_waypoints,
                 turn_off );
}

static void trw_layer_new_wp ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  /* TODO longone: okay, if layer above (aggregate) is invisible but vtl->visible is true, this redraws for no reason.
     instead return true if you want to update. */
  if ( vik_trw_layer_new_waypoint ( vtl, VIK_GTK_WINDOW_FROM_LAYER(vtl), vik_viewport_get_center(vik_layers_panel_get_viewport(vlp))) ) {
    trw_layer_calculate_bounds_waypoints ( vtl );
    if ( VIK_LAYER(vtl)->visible )
      vik_layers_panel_emit_update ( vlp );
  }
}

static void edit_track_create_common ( VikTrwLayer *vtl, gchar *name )
{
  vtl->current_track = vik_track_new();
  if ( vtl->drawmode == DRAWMODE_ALL_SAME_COLOR )
    // Create track with the preferred colour from the layer properties
    vtl->current_track->color = vtl->track_color;
  else
    gdk_color_parse ( "#000000", &(vtl->current_track->color) );
  vtl->current_track->has_color = TRUE;
  vik_trw_layer_add_track ( vtl, name, vtl->current_track );
}

static void trw_layer_edit_track ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  if ( ! vtl->current_track ) {
    gchar *name = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, _("Track")) ;
    edit_track_create_common ( vtl, name );
    g_free ( name );

    vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_CREATE_TRACK );
  }
}

static void edit_route_create_common ( VikTrwLayer *vtl, gchar *name )
{
  vtl->current_track = vik_track_new();
  vtl->current_track->is_route = TRUE;
  // By default make all routes red
  vtl->current_track->has_color = TRUE;
  gdk_color_parse ( "red", &vtl->current_track->color );
  vik_trw_layer_add_route ( vtl, name, vtl->current_track );
}

static void trw_layer_edit_route ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  if ( ! vtl->current_track ) {
    gchar *name = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_ROUTE, _("Route")) ;
    edit_route_create_common ( vtl, name );
    g_free ( name );
    vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_CREATE_ROUTE );
  }
}

static void trw_layer_auto_routes_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);

  if ( g_hash_table_size (vtl->routes) > 0 ) {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_find_maxmin_tracks, maxmin );
    trw_layer_zoom_to_show_latlons ( vtl, vik_layers_panel_get_viewport (vlp), maxmin );
    vik_layers_panel_emit_update ( vlp );
  }
}

static void clear_tool_draw ( VikTrwLayer *vtl, tool_ed_t *te )
{
  if ( te  ) {
#if GTK_CHECK_VERSION (3,0,0)
    if ( te->gc )
      ui_cr_clear ( te->gc );
#endif
  }
}

// NB vtl->current_track must be valid
static void remove_current_track_if_not_enough_points ( VikTrwLayer *vtl )
{
  if ( vik_track_get_tp_count(vtl->current_track) <= 1 ) {
    if ( vtl->current_track->is_route )
      vik_trw_layer_delete_route ( vtl, vtl->current_track );
    else
      vik_trw_layer_delete_track ( vtl, vtl->current_track );
  }
}

static void trw_layer_finish_track ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  if ( vtl->current_track )
    remove_current_track_if_not_enough_points ( vtl );
  vtl->current_track = NULL;
  VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikToolInterface *vti = vik_window_get_active_tool_interface ( vw );
  if ( vti )
    if ( vti->create == (VikToolConstructorFunc)tool_edit_create )
      clear_tool_draw ( vtl, (tool_ed_t*)vik_window_get_active_tool_data(vw) );
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_auto_tracks_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);

  if ( g_hash_table_size (vtl->tracks) > 0 ) {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_find_maxmin_tracks, maxmin );
    trw_layer_zoom_to_show_latlons ( vtl, vik_layers_panel_get_viewport (vlp), maxmin );
    vik_layers_panel_emit_update ( vlp );
  }
}

static void trw_layer_single_waypoint_jump ( const gpointer id, const VikWaypoint *wp, gpointer vvp )
{
  /* NB do not care if wp is visible or not */
  vik_viewport_set_center_coord ( VIK_VIEWPORT(vvp), &(wp->coord), TRUE );
}

static void trw_layer_auto_waypoints_view ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);

  /* Only 1 waypoint - jump straight to it */
  if ( g_hash_table_size (vtl->waypoints) == 1 ) {
    VikViewport *vvp = vik_layers_panel_get_viewport (vlp);
    g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_single_waypoint_jump, (gpointer) vvp );
  }
  /* If at least 2 waypoints - find center and then zoom to fit */
  else if ( g_hash_table_size (vtl->waypoints) > 1 )
  {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    maxmin[0].lat = vtl->waypoints_bbox.north;
    maxmin[1].lat = vtl->waypoints_bbox.south;
    maxmin[0].lon = vtl->waypoints_bbox.east;
    maxmin[1].lon = vtl->waypoints_bbox.west;
    trw_layer_zoom_to_show_latlons ( vtl, vik_layers_panel_get_viewport (vlp), maxmin );
  }

  vik_layers_panel_emit_update ( vlp );
}

static void trw_layer_view_extensions ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), vtl->gpx_extensions );
}

static void trw_layer_visibility_tree ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(values[MA_VLP]);
  vik_treeview_item_set_visible_tree ( VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter) );
  vik_layers_panel_emit_update ( vlp );
}

#ifdef VIK_CONFIG_OPENSTREETMAP
static void trw_layer_osm_traces_upload_cb ( menu_array_layer values )
{
  osm_traces_upload_viktrwlayer(VIK_TRW_LAYER(values[MA_VTL]), NULL);
}

static void trw_layer_osm_traces_upload_track_cb ( menu_array_sublayer values )
{
  if ( values[MA_MISC] ) {
    VikTrack *trk = VIK_TRACK(values[MA_MISC]);
    osm_traces_upload_viktrwlayer(VIK_TRW_LAYER(values[MA_VTL]), trk);
  }
}
#endif

static GtkMenu* create_external_submenu ( GtkMenu *menu )
{
  GtkMenu *external_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *item = vu_menu_add_item ( menu, _("Externa_l"), GTK_STOCK_EXECUTE, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(item), GTK_WIDGET(external_submenu) );
  return external_submenu;
}

static void trw_layer_add_menu_items ( VikTrwLayer *vtl, GtkMenu *menu, gpointer vlp )
{
  static menu_array_layer data;
  data[MA_VTL] = vtl;
  data[MA_VLP] = vlp;

  (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator

  if ( vtl->current_track ) {
    (void)vu_menu_add_item ( menu, vtl->current_track->is_route ? _("_Finish Route") : _("_Finish Track"),
                             NULL, G_CALLBACK(trw_layer_finish_track), data );
    (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator
  }

  /* Now with icons */
  (void)vu_menu_add_item ( menu, _("_View Layer"), GTK_STOCK_ZOOM_FIT, G_CALLBACK(trw_layer_auto_view), data );

  GtkMenu *view_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemv = vu_menu_add_item ( menu, _("V_iew"), GTK_STOCK_FIND, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemv), GTK_WIDGET(view_submenu) );

  (void)vu_menu_add_item ( view_submenu, _("View All _Tracks"), NULL, G_CALLBACK(trw_layer_auto_tracks_view), data );
  (void)vu_menu_add_item ( view_submenu, _("View All _Routes"), NULL, G_CALLBACK(trw_layer_auto_routes_view), data );
  (void)vu_menu_add_item ( view_submenu, _("View All _Waypoints"), NULL, G_CALLBACK(trw_layer_auto_waypoints_view), data );
  (void)vu_menu_add_item ( view_submenu, _("_Ensure Visibility On"), NULL, G_CALLBACK(trw_layer_visibility_tree), data );
  if ( vtl->gpx_extensions )
    (void)vu_menu_add_item ( view_submenu, _("View _GPX Extensions"), NULL, G_CALLBACK(trw_layer_view_extensions), data );

  GtkMenu *goto_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemgoto = vu_menu_add_item ( menu, _("_Goto"), GTK_STOCK_JUMP_TO, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemgoto), GTK_WIDGET(goto_submenu) );

  (void)vu_menu_add_item ( goto_submenu, _("_Goto Center of Layer"), GTK_STOCK_JUMP_TO, G_CALLBACK(trw_layer_centerize), data );
  (void)vu_menu_add_item ( goto_submenu, _("Goto _Waypoint..."), NULL, G_CALLBACK(trw_layer_goto_wp), data );

  GtkMenu *export_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *iteme = vu_menu_add_item ( menu, _("_Export Layer"), GTK_STOCK_HARDDISK, NULL, NULL );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (iteme), GTK_WIDGET(export_submenu) );

  (void)vu_menu_add_item ( export_submenu, _("Export as GPS_Point..."), NULL, G_CALLBACK(trw_layer_export_gpspoint), data );
  (void)vu_menu_add_item ( export_submenu, _("Export as GPS_Mapper..."), NULL, G_CALLBACK(trw_layer_export_gpsmapper), data );
  (void)vu_menu_add_item ( export_submenu, _("Export as _GPX..."), NULL, G_CALLBACK(trw_layer_export_gpx), data );

  if ( a_babel_available () )
    (void)vu_menu_add_item ( export_submenu, _("Export as _KML..."), NULL, G_CALLBACK(trw_layer_export_kml), data );

  if ( have_geojson_export )
    (void)vu_menu_add_item ( export_submenu, _("Export as GEO_JSON..."), NULL, G_CALLBACK(trw_layer_export_geojson), data );

  if ( a_babel_available () )
    (void)vu_menu_add_item ( export_submenu, _("Export via GPSbabel..."), NULL, G_CALLBACK(trw_layer_export_babel), data );

  gchar* external1 = g_strdup_printf ( _("Open with External Program_1: %s"), a_vik_get_external_gpx_program_1() );
  (void)vu_menu_add_item ( export_submenu, external1, NULL, G_CALLBACK(trw_layer_export_external_gpx_1), data );
  g_free ( external1 );

  gchar* external2 = g_strdup_printf ( _("Open with External Program_2: %s"), a_vik_get_external_gpx_program_2() );
  (void)vu_menu_add_item ( export_submenu, external2, NULL, G_CALLBACK(trw_layer_export_external_gpx_2), data );
  g_free ( external2 );

  if ( vtl->external_layer != VIK_TRW_LAYER_INTERNAL && have_text_program ) {
    (void)vu_menu_add_item ( export_submenu, _("Open with Text Editor"), GTK_STOCK_FILE, G_CALLBACK(trw_layer_export_external_text), data );
  }

  GtkMenu *new_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemn = vu_menu_add_item ( menu, _("_New"), GTK_STOCK_NEW, NULL, data );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemn), GTK_WIDGET(new_submenu) );

  (void)vu_menu_add_item ( new_submenu, _("New _Waypoint..."), GTK_STOCK_NEW, G_CALLBACK(trw_layer_new_wp), data );
  GtkWidget *itemnt = vu_menu_add_item ( new_submenu, _("New _Track"), GTK_STOCK_NEW, G_CALLBACK(trw_layer_edit_track), data );
  // Make it available only when a new track *not* already in progress
  gtk_widget_set_sensitive ( itemnt, ! (gboolean)GPOINTER_TO_INT(vtl->current_track) );

  GtkWidget *itemnr = vu_menu_add_item ( new_submenu, _("New _Route"), GTK_STOCK_NEW, G_CALLBACK(trw_layer_edit_route), data );
  // Make it available only when a new track *not* already in progress
  gtk_widget_set_sensitive ( itemnr, ! (gboolean)GPOINTER_TO_INT(vtl->current_track) );

#ifdef VIK_CONFIG_GEOTAG
  (void)vu_menu_add_item ( menu, _("Geotag _Images..."), VIK_ICON_GLOBE, G_CALLBACK(trw_layer_geotagging), data );
#endif

  GtkMenu *acquire_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itema = vu_menu_add_item ( menu, _("_Acquire"), GTK_STOCK_GO_DOWN, NULL, data );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itema), GTK_WIDGET(acquire_submenu) );

  (void)vu_menu_add_item ( acquire_submenu, _("From _GPS..."), NULL, G_CALLBACK(trw_layer_acquire_gps_cb), data );
  /* FIXME: only add menu when at least a routing engine has support for Directions */
  (void)vu_menu_add_item ( acquire_submenu, _("From _Directions..."), NULL, G_CALLBACK(trw_layer_acquire_routing_cb), data );

#ifdef VIK_CONFIG_OPENSTREETMAP
  (void)vu_menu_add_item ( acquire_submenu, _("From _OSM Traces..."), NULL, G_CALLBACK(trw_layer_acquire_osm_cb), data );
  (void)vu_menu_add_item ( acquire_submenu, _("From _My OSM Traces..."), NULL, G_CALLBACK(trw_layer_acquire_osm_my_traces_cb), data );
#endif

  (void)vu_menu_add_item ( acquire_submenu, _("From _URL..."), NULL, G_CALLBACK(trw_layer_acquire_url_cb), data );

#ifdef VIK_CONFIG_GEONAMES
  GtkMenu *wikipedia_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemww = vu_menu_add_item ( acquire_submenu, _("From _Wikipedia Waypoints"), GTK_STOCK_ADD, NULL, data );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemww), GTK_WIDGET(wikipedia_submenu) );

  (void)vu_menu_add_item ( wikipedia_submenu, _("Within _Layer Bounds"), GTK_STOCK_ZOOM_FIT, G_CALLBACK(trw_layer_new_wikipedia_wp_layer), data );
  (void)vu_menu_add_item ( wikipedia_submenu, _("Within _Current Bounds"), GTK_STOCK_ZOOM_100, G_CALLBACK(trw_layer_new_wikipedia_wp_viewport), data );
#endif

#ifdef VIK_CONFIG_GEOCACHES
  (void)vu_menu_add_item ( acquire_submenu, _("From Geo_caching..."), NULL, G_CALLBACK(trw_layer_acquire_geocache_cb), data );
#endif

#ifdef VIK_CONFIG_GEOTAG
  (void)vu_menu_add_item ( acquire_submenu, _("From Geotagged _Images..."), NULL, G_CALLBACK(trw_layer_acquire_geotagged_cb), data );
#endif

  if ( a_babel_available () )
    (void)vu_menu_add_item ( acquire_submenu, _("From _File..."), NULL, G_CALLBACK(trw_layer_acquire_file_cb), data );

  vik_ext_tool_datasources_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), GTK_MENU (acquire_submenu) );

  GtkMenu *upload_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemup = vu_menu_add_item ( menu, _("_Upload"), GTK_STOCK_GO_UP, NULL, data );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemup), GTK_WIDGET(upload_submenu) );

  (void)vu_menu_add_item ( upload_submenu, _("Upload to _GPS..."), GTK_STOCK_GO_FORWARD, G_CALLBACK(trw_layer_gps_upload), data );

#ifdef VIK_CONFIG_OPENSTREETMAP 
  (void)vu_menu_add_item ( upload_submenu, _("Upload to _OSM..."), GTK_STOCK_GO_UP, G_CALLBACK(trw_layer_osm_traces_upload_cb), data );
#endif

  GtkMenu *delete_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemd = vu_menu_add_item ( menu, _("De_lete"), GTK_STOCK_REMOVE, NULL, data );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemd), GTK_WIDGET(delete_submenu) );
  
  GtkWidget *itemdat = vu_menu_add_item ( delete_submenu, _("Delete All _Tracks"), GTK_STOCK_REMOVE, G_CALLBACK(trw_layer_delete_all_tracks), data );
  gtk_widget_set_sensitive ( itemdat, (gboolean)(g_hash_table_size (vtl->tracks)) );
  GtkWidget *itemdts = vu_menu_add_item ( delete_submenu, _("Delete Tracks _From Selection..."), GTK_STOCK_INDEX,
                                          G_CALLBACK(trw_layer_delete_tracks_from_selection), data );
  gtk_widget_set_sensitive ( itemdts, (gboolean)(g_hash_table_size (vtl->tracks)) );
  GtkWidget *itemdar = vu_menu_add_item ( delete_submenu, _("Delete _All Routes"), GTK_STOCK_REMOVE,
                                          G_CALLBACK(trw_layer_delete_all_routes), data );
  gtk_widget_set_sensitive ( itemdar, (gboolean)(g_hash_table_size (vtl->routes)) );
  GtkWidget *itemdrs = vu_menu_add_item ( delete_submenu, _("_Delete Routes From Selection..."), GTK_STOCK_INDEX,
                                          G_CALLBACK(trw_layer_delete_routes_from_selection), data );
  gtk_widget_set_sensitive ( itemdrs, (gboolean)(g_hash_table_size (vtl->routes)) );
  GtkWidget *itemdaw = vu_menu_add_item ( delete_submenu, _("Delete All _Waypoints"), GTK_STOCK_REMOVE,
                                          G_CALLBACK(trw_layer_delete_all_waypoints), data );
  gtk_widget_set_sensitive ( itemdaw, (gboolean)(g_hash_table_size (vtl->waypoints)) );
  GtkWidget *itemdws = vu_menu_add_item ( delete_submenu, _("Delete Waypoints From _Selection..."), GTK_STOCK_INDEX,
                                          G_CALLBACK(trw_layer_delete_waypoints_from_selection), data );
  gtk_widget_set_sensitive ( itemdws, (gboolean)(g_hash_table_size (vtl->waypoints)) );
  GtkWidget *itemddw = vu_menu_add_item ( delete_submenu, _("Delete Duplicate Waypoints"), GTK_STOCK_DELETE,
                                          G_CALLBACK(trw_layer_delete_duplicate_waypoints), data );
  gtk_widget_set_sensitive ( itemddw, (gboolean)(g_hash_table_size (vtl->waypoints)) );
  
  GtkWidget *item = a_acquire_trwlayer_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), vlp,
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

  GtkWidget *itemtl = vu_menu_add_item ( menu, _("Track _List..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_track_list_dialog), data );
  gtk_widget_set_sensitive ( itemtl, (gboolean)(g_hash_table_size (vtl->tracks)+g_hash_table_size (vtl->routes)) );
  GtkWidget *itemwl = vu_menu_add_item ( menu, _("Waypoint _List..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_waypoint_list_dialog), data ); 
  gtk_widget_set_sensitive ( itemwl, (gboolean)(g_hash_table_size (vtl->waypoints)) );

  GtkMenu *external_submenu = create_external_submenu ( menu );
  // TODO: Should use selected layer's centre - rather than implicitly using the current viewport
  vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), external_submenu, NULL );
}

// Fake Waypoint UUIDs vi simple increasing integer
static guint wp_uuid = 0;

/**
 * vik_trw_layer_add_waypoint:
 * @name: New name for the waypoint, maybe NULL.
 *        If NULL then the wp must already have a name
 */
void vik_trw_layer_add_waypoint ( VikTrwLayer *vtl, gchar *name, VikWaypoint *wp )
{
  wp_uuid++;

  if ( name )
    vik_waypoint_set_name (wp, name);

  if ( VIK_LAYER(vtl)->realized )
  {
    // Do we need to create the sublayer:
    if ( g_hash_table_size (vtl->waypoints) == 0 ) {
      trw_layer_add_sublayer_waypoints ( vtl, VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter) );
    }

    GtkTreeIter *iter = g_malloc(sizeof(GtkTreeIter));

    gdouble timestamp = 0;
    if ( !isnan(wp->timestamp) )
      timestamp = wp->timestamp;

    // Visibility column always needed for waypoints
    vik_treeview_add_sublayer ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), iter, wp->name, vtl, GUINT_TO_POINTER(wp_uuid), VIK_TRW_LAYER_SUBLAYER_WAYPOINT, get_wp_sym_small (wp->symbol), TRUE, timestamp, 0 );

    // Actual setting of visibility dependent on the waypoint
    vik_treeview_item_set_visible ( VIK_LAYER(vtl)->vt, iter, wp->visible );

    g_hash_table_insert ( vtl->waypoints_iters, GUINT_TO_POINTER(wp_uuid), iter );

    // Sort now as post_read is not called on a realized waypoint
    vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), vtl->wp_sort_order );
  }

  highest_wp_number_add_wp(vtl, wp->name);
  g_hash_table_insert ( vtl->waypoints, GUINT_TO_POINTER(wp_uuid), wp );
 
}

// Fake Track UUIDs vi simple increasing integer
static guint tr_uuid = 0;

void vik_trw_layer_add_track ( VikTrwLayer *vtl, gchar *name, VikTrack *t )
{
  tr_uuid++;

  if ( name )
    vik_track_set_name ( t, name );

  if ( VIK_LAYER(vtl)->realized )
  {
    // Do we need to create the sublayer:
    if ( g_hash_table_size (vtl->tracks) == 0 ) {
      trw_layer_add_sublayer_tracks ( vtl, VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter) );
    }

    GtkTreeIter *iter = g_malloc(sizeof(GtkTreeIter));

    gdouble timestamp = 0;
    VikTrackpoint *tpt = vik_track_get_tp_first(t);
    if ( tpt && !isnan(tpt->timestamp) )
      timestamp = tpt->timestamp;

    // Visibility column always needed for tracks
    vik_treeview_add_sublayer ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter), iter, t->name, vtl, GUINT_TO_POINTER(tr_uuid), VIK_TRW_LAYER_SUBLAYER_TRACK, NULL, TRUE, timestamp, t->number );

    // Actual setting of visibility dependent on the track
    vik_treeview_item_set_visible ( VIK_LAYER(vtl)->vt, iter, t->visible );

    g_hash_table_insert ( vtl->tracks_iters, GUINT_TO_POINTER(tr_uuid), iter );

    // Sort now as post_read is not called on a realized track
    vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter), vtl->track_sort_order );
  }

  g_hash_table_insert ( vtl->tracks, GUINT_TO_POINTER(tr_uuid), t );

  trw_layer_update_treeview ( vtl, t, FALSE );
}

// Fake Route UUIDs vi simple increasing integer
static guint rt_uuid = 0;

void vik_trw_layer_add_route ( VikTrwLayer *vtl, gchar *name, VikTrack *t )
{
  rt_uuid++;

  if ( name )
    vik_track_set_name ( t, name );

  if ( VIK_LAYER(vtl)->realized )
  {
    // Do we need to create the sublayer:
    if ( g_hash_table_size (vtl->routes) == 0 ) {
      trw_layer_add_sublayer_routes ( vtl, VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter) );
    }

    GtkTreeIter *iter = g_malloc(sizeof(GtkTreeIter));
    // Visibility column always needed for routes
    vik_treeview_add_sublayer ( VIK_LAYER(vtl)->vt, &(vtl->routes_iter), iter, t->name, vtl, GUINT_TO_POINTER(rt_uuid), VIK_TRW_LAYER_SUBLAYER_ROUTE, NULL, TRUE, 0, t->number ); // Routes don't have times
    // Actual setting of visibility dependent on the route
    vik_treeview_item_set_visible ( VIK_LAYER(vtl)->vt, iter, t->visible );

    g_hash_table_insert ( vtl->routes_iters, GUINT_TO_POINTER(rt_uuid), iter );

    // Sort now as post_read is not called on a realized route
    vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->routes_iter), vtl->track_sort_order );
  }

  g_hash_table_insert ( vtl->routes, GUINT_TO_POINTER(rt_uuid), t );

  trw_layer_update_treeview ( vtl, t, FALSE );
}

/* to be called whenever a track has been deleted or may have been changed. */
void trw_layer_cancel_tps_of_track ( VikTrwLayer *vtl, VikTrack *trk )
{
  if (vtl->current_tp_track == trk )
    trw_layer_cancel_current_tp ( vtl, FALSE );
}

/**
 * Normally this is done to due the waypoint size preference having changed
 */
void vik_trw_layer_reset_waypoints ( VikTrwLayer *vtl )
{
  GHashTableIter iter;
  gpointer key, value;

  // Foreach waypoint
  g_hash_table_iter_init ( &iter, vtl->waypoints );
  while ( g_hash_table_iter_next ( &iter, &key, &value ) ) {
    VikWaypoint *wp = VIK_WAYPOINT(value);
    if ( wp->symbol ) {
      // Reapply symbol setting to update the pixbuf
      gchar *tmp_symbol = g_strdup ( wp->symbol );
      vik_waypoint_set_symbol ( wp, tmp_symbol );
      g_free ( tmp_symbol );
    }
  }
}

/**
 * trw_layer_new_unique_sublayer_name:
 *
 * Allocates a unique new name
 */
gchar *trw_layer_new_unique_sublayer_name (VikTrwLayer *vtl, gint sublayer_type, const gchar *name)
{
  gint i = 2;
  gchar *newname = g_strdup(name);

  gpointer id = NULL;
  do {
    id = NULL;
    switch ( sublayer_type ) {
    case VIK_TRW_LAYER_SUBLAYER_TRACK:
      id = (gpointer) vik_trw_layer_get_track ( vtl, newname );
      break;
    case VIK_TRW_LAYER_SUBLAYER_WAYPOINT:
      id = (gpointer) vik_trw_layer_get_waypoint ( vtl, newname );
      break;
    default:
      id = (gpointer) vik_trw_layer_get_route ( vtl, newname );
      break;
    }
    // If found a name already in use try adding 1 to it and we try again
    if ( id ) {
      const gchar *corename = newname;
      gint newi = i;
      // If name is already of the form text#N
      //  set name to text and i to N+1
      gchar **tokens = g_regex_split_simple ( "#(\\d+)", newname, G_REGEX_CASELESS, 0 );
      if ( tokens ) {
        corename = tokens[0];
        if ( tokens[1] ) {
          newi = atoi ( tokens[1] ) + 1;
        }
      }
      gchar *new_newname = g_strdup_printf("%s#%d", corename, newi);
      g_strfreev ( tokens );
      g_free(newname);
      newname = new_newname;
      i++;
    }
  } while ( id != NULL);

  return newname;
}

void vik_trw_layer_filein_add_waypoint ( VikTrwLayer *vtl, gchar *name, VikWaypoint *wp )
{
  // No more uniqueness of name forced when loading from a file
  // This now makes this function a little redunant as we just flow the parameters through
  vik_trw_layer_add_waypoint ( vtl, name, wp );
}

void vik_trw_layer_filein_add_track ( VikTrwLayer *vtl, gchar *name, VikTrack *tr )
{
  if ( vtl->route_finder_append && vtl->current_track ) {
    vik_track_remove_dup_points ( tr ); /* make "double point" track work to undo */

    // enforce end of current track equal to start of tr
    VikTrackpoint *cur_end = vik_track_get_tp_last ( vtl->current_track );
    VikTrackpoint *new_start = vik_track_get_tp_first ( tr );
    if ( cur_end && new_start ) {
      if ( ! vik_coord_equals ( &cur_end->coord, &new_start->coord ) ) {
          vik_track_add_trackpoint ( vtl->current_track,
                                     vik_trackpoint_copy ( cur_end ),
                                     FALSE );
      }
    }

    vik_track_steal_and_append_trackpoints ( vtl->current_track, tr );
    vik_track_free ( tr );
    vtl->route_finder_append = FALSE; /* this means we have added it */
  } else {

    // No more uniqueness of name forced when loading from a file
    if ( tr->is_route )
      vik_trw_layer_add_route ( vtl, name, tr );
    else
      vik_trw_layer_add_track ( vtl, name, tr );

    if ( vtl->route_finder_check_added_track ) {
      vik_track_remove_dup_points ( tr ); /* make "double point" track work to undo */
      vtl->route_finder_added_track = tr;
    }
  }
}

/**
 * ATM Only for removing bad first points
 */
void vik_trw_layer_tidy_tracks ( VikTrwLayer *vtl, guint speed, gboolean recalc_bounds )
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init ( &iter, vtl->tracks );
  while ( g_hash_table_iter_next (&iter, &key, &value) ) {
    VikTrack *trk = VIK_TRACK(value);
    if ( vik_track_remove_dodgy_first_point ( trk, speed, FALSE ) )
      g_message ( "%s: Removed dodgy first point from track: %s", __FUNCTION__, trk->name );
  }
}

static void trw_layer_enum_item ( gpointer id, GList **tr, GList **l )
{
  *l = g_list_append(*l, id);
}

/*
 * Move an item from one TRW layer to another TRW layer
 */
static void trw_layer_move_item ( VikTrwLayer *vtl_src, VikTrwLayer *vtl_dest, gpointer id, gint type )
{
  // When an item is moved the name is checked to see if it clashes with an existing name
  //  in the destination layer and if so then it is allocated a new name

  // TODO reconsider strategy when moving within layer (if anything...)
  if ( vtl_src == vtl_dest )
    return;

  if (type == VIK_TRW_LAYER_SUBLAYER_TRACK) {
    VikTrack *trk = g_hash_table_lookup ( vtl_src->tracks, id );

    gchar *newname = trw_layer_new_unique_sublayer_name ( vtl_dest, type, trk->name );

    VikTrack *trk2 = vik_track_copy ( trk, TRUE );
    vik_trw_layer_add_track ( vtl_dest, newname, trk2 );
    g_free ( newname );
    vik_trw_layer_delete_track ( vtl_src, trk );
    // Reset layer timestamps in case they have now changed
    vik_treeview_item_set_timestamp ( vtl_dest->vl.vt, &vtl_dest->vl.iter, trw_layer_get_timestamp(vtl_dest) );
    vik_treeview_item_set_timestamp ( vtl_src->vl.vt, &vtl_src->vl.iter, trw_layer_get_timestamp(vtl_src) );
  }

  if (type == VIK_TRW_LAYER_SUBLAYER_ROUTE) {
    VikTrack *trk = g_hash_table_lookup ( vtl_src->routes, id );

    gchar *newname = trw_layer_new_unique_sublayer_name ( vtl_dest, type, trk->name );

    VikTrack *trk2 = vik_track_copy ( trk, TRUE );
    vik_trw_layer_add_route ( vtl_dest, newname, trk2 );
    g_free ( newname );
    vik_trw_layer_delete_route ( vtl_src, trk );
  }

  if (type == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) {
    VikWaypoint *wp = g_hash_table_lookup ( vtl_src->waypoints, id );

    gchar *newname = trw_layer_new_unique_sublayer_name ( vtl_dest, type, wp->name );

    VikWaypoint *wp2 = vik_waypoint_copy ( wp );
    vik_trw_layer_add_waypoint ( vtl_dest, newname, wp2 );
    g_free ( newname );
    (void)trw_layer_delete_waypoint ( vtl_src, wp );

    // Recalculate bounds even if not renamed as maybe dragged between layers
    trw_layer_calculate_bounds_waypoints ( vtl_dest );
    trw_layer_calculate_bounds_waypoints ( vtl_src );
    // Reset layer timestamps in case they have now changed
    vik_treeview_item_set_timestamp ( vtl_dest->vl.vt, &vtl_dest->vl.iter, trw_layer_get_timestamp(vtl_dest) );
    vik_treeview_item_set_timestamp ( vtl_src->vl.vt, &vtl_src->vl.iter, trw_layer_get_timestamp(vtl_src) );
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
    if (type==VIK_TRW_LAYER_SUBLAYER_ROUTES) {
      g_hash_table_foreach ( vtl_src->routes, (GHFunc)trw_layer_enum_item, &items);
    }

    iter = items;
    while (iter) {
      if (type==VIK_TRW_LAYER_SUBLAYER_TRACKS) {
        trw_layer_move_item ( vtl_src, vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_TRACK);
      } else if (type==VIK_TRW_LAYER_SUBLAYER_ROUTES) {
        trw_layer_move_item ( vtl_src, vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_ROUTE);
      } else {
        trw_layer_move_item ( vtl_src, vtl_dest, iter->data, VIK_TRW_LAYER_SUBLAYER_WAYPOINT);
      }
      iter = iter->next;
    }
    if (items) 
      g_list_free(items);
  } else {
    gpointer ptr = vik_treeview_item_get_pointer ( vt, src_item_iter );
    trw_layer_move_item ( vtl_src, vtl_dest, ptr, type );
  }
}

gboolean trw_layer_track_find_uuid ( const gpointer id, const VikTrack *trk, gpointer udata )
{
  trku_udata *user_data = udata;
  if ( trk == user_data->trk ) {
    user_data->uuid = id;
    return TRUE;
  }
  return FALSE;
}

gboolean vik_trw_layer_delete_track ( VikTrwLayer *vtl, VikTrack *trk )
{
  gboolean was_visible = FALSE;
  if ( trk && trk->name ) {

    if ( trk == vtl->current_track ) {
      vtl->current_track = NULL;
      vtl->current_tp_track = NULL;
      vtl->moving_tp = FALSE;
    }

    was_visible = trk->visible;

    if ( trk == vtl->route_finder_added_track )
      vtl->route_finder_added_track = NULL;

    trku_udata udata;
    udata.trk  = trk;
    udata.uuid = NULL;

    // Hmmm, want key of it
    gpointer trkf = g_hash_table_find ( vtl->tracks, (GHRFunc) trw_layer_track_find_uuid, &udata );

    if ( trkf && udata.uuid ) {
      /* could be current_tp, so we have to check */
      trw_layer_cancel_tps_of_track ( vtl, trk );

      GtkTreeIter *it = g_hash_table_lookup ( vtl->tracks_iters, udata.uuid );

      if ( it ) {
        vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, it );
        g_hash_table_remove ( vtl->tracks_iters, udata.uuid );
        g_hash_table_remove ( vtl->tracks, udata.uuid );

	// If last sublayer, then remove sublayer container
	if ( g_hash_table_size (vtl->tracks) == 0 ) {
          vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter) );
	}
      }
      // Incase it was selected (no item delete signal ATM)
      (void)vik_window_clear_selected ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
    }
  }
  return was_visible;
}

gboolean vik_trw_layer_delete_route ( VikTrwLayer *vtl, VikTrack *trk )
{
  gboolean was_visible = FALSE;

  if ( trk && trk->name ) {

    if ( trk == vtl->current_track ) {
      vtl->current_track = NULL;
      vtl->current_tp_track = NULL;
      vtl->moving_tp = FALSE;
    }

    was_visible = trk->visible;

    if ( trk == vtl->route_finder_added_track )
      vtl->route_finder_added_track = NULL;

    trku_udata udata;
    udata.trk  = trk;
    udata.uuid = NULL;

    // Hmmm, want key of it
    gpointer trkf = g_hash_table_find ( vtl->routes, (GHRFunc) trw_layer_track_find_uuid, &udata );

    if ( trkf && udata.uuid ) {
      /* could be current_tp, so we have to check */
      trw_layer_cancel_tps_of_track ( vtl, trk );

      GtkTreeIter *it = g_hash_table_lookup ( vtl->routes_iters, udata.uuid );

      if ( it ) {
        vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, it );
        g_hash_table_remove ( vtl->routes_iters, udata.uuid );
        g_hash_table_remove ( vtl->routes, udata.uuid );

        // If last sublayer, then remove sublayer container
        if ( g_hash_table_size (vtl->routes) == 0 ) {
          vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, &(vtl->routes_iter) );
        }
      }
      // Incase it was selected (no item delete signal ATM)
      (void)vik_window_clear_selected ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
    }
  }
  return was_visible;
}

static void delete_waypoint_low_level ( VikTrwLayer *vtl, VikWaypoint *wp, gpointer uuid, GtkTreeIter *it )
{
  if ( vtl->wpwin && wp == vtl->wpwin_wpt ) {
    vik_trw_layer_wpwin_destroy ( vtl->wpwin );
    vtl->wpwin = NULL;
  }

  vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, it );
  g_hash_table_remove ( vtl->waypoints_iters, uuid );

  highest_wp_number_remove_wp ( vtl, wp->name );
  g_hash_table_remove ( vtl->waypoints, uuid ); // last because this frees the name
}

static gboolean trw_layer_delete_waypoint ( VikTrwLayer *vtl, VikWaypoint *wp )
{
  gboolean was_visible = FALSE;

  if ( wp && wp->name ) {

    was_visible = wp->visible;
    
    wpu_udata udata;
    udata.wp   = wp;
    udata.uuid = NULL;

    // Hmmm, want key of it
    gpointer wpf = g_hash_table_find ( vtl->waypoints, (GHRFunc) trw_layer_waypoint_find_uuid, (gpointer) &udata );

    if ( wpf && udata.uuid ) {
      GtkTreeIter *it = g_hash_table_lookup ( vtl->waypoints_iters, udata.uuid );
    
      if ( it ) {
        delete_waypoint_low_level ( vtl, wp, udata.uuid, it );

        if ( wp == vtl->current_wp ) {
          vtl->current_wp = NULL;
          vtl->current_wp_id = NULL;
          vtl->moving_wp = FALSE;
        }

	// If last sublayer, then remove sublayer container
	if ( g_hash_table_size (vtl->waypoints) == 0 ) {
          vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter) );
	}
      }
      // Incase it was selected (no item delete signal ATM)
      (void)vik_window_clear_selected ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
    }

  }

  return was_visible;
}

// Only for temporary use by trw_layer_delete_waypoint_by_name
static gboolean trw_layer_waypoint_find_uuid_by_name ( const gpointer id, const VikWaypoint *wp, gpointer udata )
{
  wpu_udata *user_data = udata;
  if ( ! strcmp ( wp->name, user_data->wp->name ) ) {
    user_data->uuid = id;
    return TRUE;
  }
  return FALSE;
}

/*
 * Delete a waypoint by the given name
 * NOTE: ATM this will delete the first encountered Waypoint with the specified name
 *   as there be multiple waypoints with the same name
 */
static gboolean trw_layer_delete_waypoint_by_name ( VikTrwLayer *vtl, const gchar *name )
{
  wpu_udata udata;
  // Fake a waypoint with the given name
  udata.wp   = vik_waypoint_new ();
  vik_waypoint_set_name (udata.wp, name);
  // Currently only the name is used in this waypoint find function
  udata.uuid = NULL;

  // Hmmm, want key of it
  gpointer wpf = g_hash_table_find ( vtl->waypoints, (GHRFunc) trw_layer_waypoint_find_uuid_by_name, (gpointer) &udata );

  vik_waypoint_free (udata.wp);

  if ( wpf && udata.uuid )
    return trw_layer_delete_waypoint (vtl, g_hash_table_lookup ( vtl->waypoints, udata.uuid ));
  else
    return FALSE;
}

typedef struct {
  VikTrack *trk; // input
  gpointer uuid; // output
} tpu_udata;

// Only for temporary use by trw_layer_delete_track_by_name
static gboolean trw_layer_track_find_uuid_by_name ( const gpointer id, const VikTrack *trk, gpointer udata )
{
  tpu_udata *user_data = udata;
  if ( ! strcmp ( trk->name, user_data->trk->name ) ) {
    user_data->uuid = id;
    return TRUE;
  }
  return FALSE;
}

/*
 * Delete a track by the given name
 * NOTE: ATM this will delete the first encountered Track with the specified name
 *   as there may be multiple tracks with the same name within the specified hash table
 */
static gboolean trw_layer_delete_track_by_name ( VikTrwLayer *vtl, const gchar *name, GHashTable *ht_tracks )
{
  tpu_udata udata;
  // Fake a track with the given name
  udata.trk   = vik_track_new ();
  vik_track_set_name (udata.trk, name);
  // Currently only the name is used in this track find function
  udata.uuid = NULL;

  // Hmmm, want key of it
  gpointer trkf = g_hash_table_find ( ht_tracks, (GHRFunc) trw_layer_track_find_uuid_by_name, &udata );

  vik_track_free (udata.trk);

  if ( trkf && udata.uuid ) {
    // This could be a little better written...
    if ( vtl->tracks == ht_tracks )
      return vik_trw_layer_delete_track (vtl, g_hash_table_lookup ( ht_tracks, udata.uuid ));
    if ( vtl->routes == ht_tracks )
      return vik_trw_layer_delete_route (vtl, g_hash_table_lookup ( ht_tracks, udata.uuid ));
    return FALSE;
  }
  else
    return FALSE;
}

static void remove_item_from_treeview ( const gpointer id, GtkTreeIter *it, VikTreeview * vt )
{
    vik_treeview_item_delete (vt, it );
}

void vik_trw_layer_delete_all_routes ( VikTrwLayer *vtl )
{
  vtl->current_track = NULL;
  vtl->route_finder_added_track = NULL;
  if (vtl->current_tp_track)
    trw_layer_cancel_current_tp(vtl, FALSE);

  g_hash_table_foreach(vtl->routes_iters, (GHFunc) remove_item_from_treeview, VIK_LAYER(vtl)->vt);
  g_hash_table_remove_all(vtl->routes_iters);

  if ( g_hash_table_size (vtl->routes) > 0 )
    vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, &(vtl->routes_iter) );
  g_hash_table_remove_all(vtl->routes);

  close_graphs_of_specific_track_or_type ( vtl, NULL, VIK_TRW_LAYER_SUBLAYER_ROUTES );
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

void vik_trw_layer_delete_all_tracks ( VikTrwLayer *vtl )
{
  vtl->current_track = NULL;
  vtl->route_finder_added_track = NULL;
  if (vtl->current_tp_track)
    trw_layer_cancel_current_tp(vtl, FALSE);

  g_hash_table_foreach(vtl->tracks_iters, (GHFunc) remove_item_from_treeview, VIK_LAYER(vtl)->vt);
  g_hash_table_remove_all(vtl->tracks_iters);

  if ( g_hash_table_size (vtl->tracks) > 0 )
    vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter) );
  g_hash_table_remove_all(vtl->tracks);

  close_graphs_of_specific_track_or_type ( vtl, NULL, VIK_TRW_LAYER_SUBLAYER_TRACKS );
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

void vik_trw_layer_delete_all_waypoints ( VikTrwLayer *vtl )
{
  if ( vtl->wpwin ) {
    vik_trw_layer_wpwin_destroy ( vtl->wpwin );
    vtl->wpwin = NULL;
  }
  vtl->wpwin_wpt = NULL;

  vtl->current_wp = NULL;
  vtl->current_wp_id = NULL;
  vtl->moving_wp = FALSE;

  highest_wp_number_reset(vtl);

  g_hash_table_foreach(vtl->waypoints_iters, (GHFunc) remove_item_from_treeview, VIK_LAYER(vtl)->vt);
  g_hash_table_remove_all(vtl->waypoints_iters);

  if ( g_hash_table_size (vtl->waypoints) > 0 )
    vik_treeview_item_delete ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter) );
  g_hash_table_remove_all(vtl->waypoints);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_delete_all_tracks ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    _("Are you sure you want to delete all tracks in %s?"),
			    vik_layer_get_name ( VIK_LAYER(vtl) ) ) ) {
    vik_trw_layer_delete_all_tracks (vtl);
    if ( values[MA_VLP] )
      vik_layers_panel_calendar_update ( VIK_LAYERS_PANEL(values[MA_VLP]) );
  }
}

static void trw_layer_delete_all_routes ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                            _("Are you sure you want to delete all routes in %s?"),
                            vik_layer_get_name ( VIK_LAYER(vtl) ) ) )
    vik_trw_layer_delete_all_routes (vtl);
}

static void trw_layer_delete_all_waypoints ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // Get confirmation from the user
  if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			    _("Are you sure you want to delete all waypoints in %s?"),
			    vik_layer_get_name ( VIK_LAYER(vtl) ) ) )
    vik_trw_layer_delete_all_waypoints (vtl);
}

static void trw_layer_delete_item ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gboolean was_visible = FALSE;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    VikWaypoint *wp = g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
    if ( wp && wp->name ) {
      if ( GPOINTER_TO_INT (values[MA_CONFIRM]) )
        // Get confirmation from the user
        // Maybe this Waypoint Delete should be optional as is it could get annoying...
        if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
            _("Are you sure you want to delete the waypoint \"%s\"?"),
            wp->name ) )
          return;
      was_visible = trw_layer_delete_waypoint ( vtl, wp );
      trw_layer_calculate_bounds_waypoints ( vtl );
      // Reset layer timestamp in case it has now changed
      vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );
    }
  }
  else if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    VikTrack *trk = g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
    if ( trk && trk->name ) {
      if ( GPOINTER_TO_INT (values[MA_CONFIRM]) )
        // Get confirmation from the user
        if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
				  _("Are you sure you want to delete the track \"%s\"?"),
				  trk->name ) )
          return;
      was_visible = vik_trw_layer_delete_track ( vtl, trk );
      // Reset layer timestamp in case it has now changed
      vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );
      if ( values[MA_VLP] )
	vik_layers_panel_calendar_update ( VIK_LAYERS_PANEL(values[MA_VLP]) );
    }
  }
  else
  {
    VikTrack *trk = g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
    if ( trk && trk->name ) {
      if ( GPOINTER_TO_INT (values[MA_CONFIRM]) )
        // Get confirmation from the user
        if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                    _("Are you sure you want to delete the route \"%s\"?"),
                                    trk->name ) )
          return;
      was_visible = vik_trw_layer_delete_route ( vtl, trk );
    }
  }
  if ( was_visible )
    vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * On changes of time, maintain position of waypoint in the treeview
 */
void trw_layer_treeview_waypoint_align_time ( VikTrwLayer *vtl, VikWaypoint *wp )
{
  wpu_udata udataU;
  udataU.wp   = wp;
  udataU.uuid = NULL;

  // Need key of it for treeview update
  gpointer wpf = g_hash_table_find ( vtl->waypoints, (GHRFunc)trw_layer_waypoint_find_uuid, &udataU );
  if ( wpf && udataU.uuid ) {
    GtkTreeIter *it = g_hash_table_lookup ( vtl->waypoints_iters, udataU.uuid );
    if ( it ) {
      vik_treeview_item_set_timestamp ( VIK_LAYER(vtl)->vt, it, wp->timestamp );
      vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), vtl->wp_sort_order );
    }
  }
}

/**
 *  Rename waypoint and maintain corresponding name of waypoint in the treeview
 */
void trw_layer_waypoint_rename ( VikTrwLayer *vtl, VikWaypoint *wp, const gchar *new_name )
{
  vik_waypoint_set_name ( wp, new_name );

  // Now update the treeview as well
  wpu_udata udataU;
  udataU.wp   = wp;
  udataU.uuid = NULL;

  // Need key of it for treeview update
  gpointer wpf = g_hash_table_find ( vtl->waypoints, (GHRFunc) trw_layer_waypoint_find_uuid, &udataU );

  if ( wpf && udataU.uuid ) {
    GtkTreeIter *it = g_hash_table_lookup ( vtl->waypoints_iters, udataU.uuid );

    if ( it ) {
      vik_treeview_item_set_name ( VIK_LAYER(vtl)->vt, it, new_name );
      vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), vtl->wp_sort_order );
    }
  }
}

void trw_layer_waypoint_properties_changed ( VikTrwLayer *vtl, VikWaypoint *wp )
{
  // Find in treeview
  wpu_udata udataU;
  udataU.wp   = wp;
  udataU.uuid = NULL;
  gpointer wpf = g_hash_table_find ( vtl->waypoints, (GHRFunc)trw_layer_waypoint_find_uuid, &udataU );

  if ( wpf && udataU.uuid ) {
    GtkTreeIter *iter = g_hash_table_lookup ( vtl->waypoints_iters, udataU.uuid );
    if ( iter ) {
      // Update treeview data
      vik_treeview_item_set_name ( VIK_LAYER(vtl)->vt, iter, wp->name );
      vik_treeview_item_set_icon ( VIK_LAYER(vtl)->vt, iter, get_wp_sym_small (wp->symbol) );
      vik_treeview_item_set_timestamp ( VIK_LAYER(vtl)->vt, iter, wp->timestamp );
      vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), vtl->wp_sort_order );
    }
  }
  // Position may have changed
  trw_layer_calculate_bounds_waypoints ( vtl );

  if ( wp->visible && vik_treeview_item_get_visible_tree(VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter)) )
    vik_layer_emit_update ( VIK_LAYER(vtl) );
}

void trw_layer_wpwin_set ( VikTrwLayer *vtl, VikWaypoint *wp, gpointer wpwin )
{
  vtl->wpwin = wpwin;
  vtl->wpwin_wpt = wp;
}

static void trw_layer_properties_item ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    VikWaypoint *wp = g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
    if ( wp && wp->name ) {
      if ( vtl->wpwin )
	vik_trw_layer_wpwin_destroy ( vtl->wpwin );
      vtl->wpwin_wpt = wp;
      vtl->wpwin = vik_trw_layer_wpwin_show ( VIK_GTK_WINDOW_FROM_LAYER(vtl), NULL, wp->name, vtl, wp, vtl->coord_mode, FALSE );
    }
  }
  else
  {
    VikTrack *tr;
    if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK )
      tr = g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
    else
      tr = g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );

    if ( tr && tr->name )
    {
      vik_trw_layer_propwin_run ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                  vtl,
                                  tr,
                                  values[MA_VLP],
                                  values[MA_VVP],
                                  FALSE );
    }
  }
}

/**
 * trw_layer_track_statistics:
 *
 * Show track statistics.
 * ATM jump to the stats page in the properties
 * TODO: consider separating the stats into an individual dialog?
 */
static void trw_layer_track_statistics ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK )
    trk = g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  else
    trk = g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );

  if ( trk && trk->name ) {
    vik_trw_layer_propwin_run ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                vtl,
                                trk,
                                values[MA_VLP],
                                values[MA_VVP],
                                TRUE );
  }
}

/*
 * Update the treeview of the track id - primarily to update the icon
 */
void trw_layer_update_treeview ( VikTrwLayer *vtl, VikTrack *trk, gboolean do_sort )
{
  trku_udata udata;
  udata.trk  = trk;
  udata.uuid = NULL;

  gpointer trkf = NULL;
  if ( trk->is_route )
    trkf = g_hash_table_find ( vtl->routes, (GHRFunc) trw_layer_track_find_uuid, &udata );
  else
    trkf = g_hash_table_find ( vtl->tracks, (GHRFunc) trw_layer_track_find_uuid, &udata );

  if ( trkf && udata.uuid ) {

    GtkTreeIter *iter = NULL;
    if ( trk->is_route )
      iter = g_hash_table_lookup ( vtl->routes_iters, udata.uuid );
    else
      iter = g_hash_table_lookup ( vtl->tracks_iters, udata.uuid );

    if ( iter ) {
      GdkPixbuf *pixbuf = ui_pixbuf_new ( &trk->color, SMALL_ICON_SIZE, SMALL_ICON_SIZE );
      vik_treeview_item_set_icon ( VIK_LAYER(vtl)->vt, iter, pixbuf );
      g_object_unref (pixbuf);

      if ( do_sort ) {
	vik_treeview_item_set_number ( VIK_LAYER(vtl)->vt, iter, trk->number );
        trw_layer_sort_order_specified ( vtl, trk->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTES : VIK_TRW_LAYER_SUBLAYER_TRACKS, vtl->track_sort_order );
      }
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
    vik_viewport_set_center_coord ( vik_layers_panel_get_viewport (VIK_LAYERS_PANEL(vlp)), coord, TRUE );
    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp) );
  }
  else {
    /* since vlp not set, vl & vvp should be valid instead! */
    if ( vl && vvp ) {
      vik_viewport_set_center_coord ( VIK_VIEWPORT(vvp), coord, TRUE );
      vik_layer_emit_update ( VIK_LAYER(vl) );
    }
  }
}

static void trw_layer_goto_track_startpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( track && track->trackpoints ) {
    trw_layer_select_trackpoint ( vtl, track, vik_track_get_tp_first(track), TRUE );
    goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vik_track_get_tp_first(track)->coord) );
  }
}

static void trw_layer_goto_track_center ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( track && track->trackpoints )
  {
    struct LatLon average, maxmin[2] = { {0,0}, {0,0} };
    VikCoord coord;
    trw_layer_find_maxmin_tracks ( NULL, track, maxmin );
    average.lat = (maxmin[0].lat+maxmin[1].lat)/2;
    average.lon = (maxmin[0].lon+maxmin[1].lon)/2;
    vik_coord_load_from_latlon ( &coord, vtl->coord_mode, &average );
    goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &coord);
  }
}

static void trw_layer_goto_track_date ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK )
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  else
    // No dates on routes
    return;

  if ( track && track->trackpoints ) {
    VikTrackpoint *tp = vik_track_get_tp_first( track );
    if ( !isnan(tp->timestamp) ) {
      // Not worried about subsecond resolution here!
      vik_layers_panel_calendar_date ( VIK_LAYERS_PANEL(values[MA_VLP]), (time_t)tp->timestamp );
    }
  }
}

static void my_tpwin_set_tp ( VikTrwLayer *vtl );

static void trw_layer_graph_draw_tp ( VikTrwLayer *vtl )
{
  if ( vtl->current_tpl ) {
    VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));
    gpointer gw = vik_window_get_graphs_widgets ( vw );
    if ( gw )
      vik_trw_layer_propwin_main_draw_blob ( gw, VIK_TRACKPOINT(vtl->current_tpl->data) );
  }
}

static void trw_layer_select_trackpoint ( VikTrwLayer *vtl, VikTrack *trk, VikTrackpoint *tpt, gboolean draw_graph_blob )
{
  GList *tpl = g_list_find ( trk->trackpoints, tpt );
  if ( tpl ) {
    vtl->current_tpl = tpl;
    vtl->current_tp_track = trk;
  }
  if ( draw_graph_blob ) {
    trw_layer_graph_draw_tp ( vtl );
    set_statusbar_msg_info_trkpt ( vtl, tpt );
  }
}

void vik_trw_layer_goto_track_prev_point ( VikTrwLayer *vtl )
{
  if ( !vtl->current_tpl )
    return;
  if ( !vtl->current_tpl->prev )
    return;

  if ( vtl->current_tp_track ) {
    vtl->current_tpl = vtl->current_tpl->prev;
    if ( vtl->tpwin )
      my_tpwin_set_tp ( vtl );
    set_statusbar_msg_info_trkpt ( vtl, vtl->current_tpl->data );
  }
  vik_layer_emit_update(VIK_LAYER(vtl));
  trw_layer_graph_draw_tp ( vtl );
}

static void trw_layer_goto_track_prev_point ( menu_array_sublayer values )
{
  vik_trw_layer_goto_track_prev_point ( (VikTrwLayer*)values[MA_VTL] );
}

void vik_trw_layer_goto_track_next_point ( VikTrwLayer *vtl )
{
  if ( !vtl->current_tpl )
    return;
  if ( !vtl->current_tpl->next )
    return;

  if ( vtl->current_tp_track ) {
    vtl->current_tpl = vtl->current_tpl->next;
    if ( vtl->tpwin )
      my_tpwin_set_tp ( vtl );
    set_statusbar_msg_info_trkpt ( vtl, vtl->current_tpl->data );
  }
  vik_layer_emit_update(VIK_LAYER(vtl));
  trw_layer_graph_draw_tp ( vtl );
}

static void trw_layer_goto_track_next_point ( menu_array_sublayer values )
{
  vik_trw_layer_goto_track_next_point ( (VikTrwLayer*)values[MA_VTL] );
}

/**
 * Convert visible waypoints in the selected layer into a track
 *  Waypoints are ordered according to the waypoint sort order property
 * Note that any waypoint symbols and extra data (URL,images, etc...) are lost in this conversion
 */
static void trw_layer_convert_to_track ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk = vik_track_new();
  if ( vtl->drawmode == DRAWMODE_ALL_SAME_COLOR )
    // Create track with the preferred colour from the layer properties
    trk->color = vtl->track_color;
  else
    gdk_color_parse ( "#000000", &(trk->color) );
  trk->has_color = TRUE;

  GList* gl = vu_sorted_list_from_hash_table ( vtl->waypoints, vtl->wp_sort_order, VIKING_WAYPOINT );

  gchar *name = NULL;
  guint count = 1;
  for ( GList *it = g_list_first(gl); it != NULL; it = g_list_next(it) ) {
    VikWaypoint *wpt = VIK_WAYPOINT(((SortTRWHashT*)it->data)->data);
    if ( wpt->visible ) {
      VikTrackpoint *tp = vik_trackpoint_new();
      if ( count == 1 ) {
        tp->newsegment = TRUE;
	// Remove ending digits
	gchar *wpnm = wpt->name;
	guint len = strlen(wpnm);
	guint pos = 0;
	while ( wpnm ) {
	  if ( g_ascii_isdigit(*wpnm) ) {
	    break;
	  }
	  pos++;
	  wpnm++;
	}
	// Very first is a digit so use whole string
	if ( pos == 0 ) pos = len;
	name = g_strndup ( wpt->name, pos );
      }
      // Do we want to keep/give names?
      //if ( tp->name )
      //  wp->name      = g_strdup ( tp->name );
      tp->coord     = wpt->coord;
      tp->timestamp = wpt->timestamp;
      tp->altitude  = wpt->altitude;
      tp->speed     = wpt->speed;
      tp->course    = wpt->course;
      tp->fix_mode  = wpt->fix_mode;
      tp->nsats     = wpt->nsats;
      tp->hdop      = wpt->hdop;
      tp->vdop      = wpt->vdop;
      tp->pdop      = wpt->pdop;
      trk->trackpoints = g_list_prepend ( trk->trackpoints, tp );
      count++;
    }
  }
  g_list_free_full ( gl, g_free );
  trk->trackpoints = g_list_reverse ( trk->trackpoints );

  // Remove the used Waypoints
  gl = g_hash_table_get_values ( vtl->waypoints );
  for ( GList *it = g_list_first(gl); it != NULL; it = g_list_next(it) ) {
    VikWaypoint *wpt = VIK_WAYPOINT(it->data);
    // NB Removal quite slow as performing it one-by-one and on each delete it's updating the treeview
    //  especially if hundreds of waypoints, but so be it.
    if ( wpt->visible ) {
      (void)trw_layer_delete_waypoint ( vtl, wpt );
    }
  }
  g_list_free ( gl ); // NB not 'free_full' as wp entries already freed
  trw_layer_calculate_bounds_waypoints ( vtl );

  vik_trw_layer_add_track ( vtl, name, trk );
  vik_track_calculate_bounds ( trk );
  g_free ( name );

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_convert_to_waypoints ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  guint count = 1;
  GList *tp_iter;
  tp_iter = trk->trackpoints;
  while ( tp_iter ) {
    VikTrackpoint *tp = VIK_TRACKPOINT(tp_iter->data);
    VikWaypoint *wpt = vik_waypoint_new();
    wpt->timestamp = tp->timestamp;
    wpt->coord     = tp->coord;
    wpt->altitude  = tp->altitude;
    wpt->speed     = tp->speed;
    wpt->course    = tp->course;
    wpt->fix_mode  = tp->fix_mode;
    wpt->nsats     = tp->nsats;
    wpt->hdop      = tp->hdop;
    wpt->vdop      = tp->vdop;
    wpt->pdop      = tp->pdop;
    gchar *name = g_strdup_printf ( "%s%05d", trk->name, count++ );
    vik_trw_layer_add_waypoint ( vtl, name, wpt );
    g_free ( name );
    tp_iter = tp_iter->next;
  }

  // Converting may lose some information, so don't always delete
  gboolean perform_delete = TRUE;
  if ( trk->comment || trk->description || trk->source || trk->type )
    perform_delete = FALSE;

  if ( perform_delete ) {
    if ( trk->is_route )
      vik_trw_layer_delete_route ( vtl, trk );
    else
      vik_trw_layer_delete_track ( vtl, trk );
  }

  trw_layer_calculate_bounds_waypoints ( vtl );

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_convert_track_route ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  // Converting a track to a route can be a bit more complicated,
  //  so give a chance to change our minds:
  if ( !trk->is_route &&
       ( ( vik_track_get_segment_count ( trk ) > 1 ) ||
         ( vik_track_get_average_speed ( trk ) > 0.0 ) ) ) {

    if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                _("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), NULL ) )
      return;
}

  // Copy it
  VikTrack *trk_copy = vik_track_copy ( trk, TRUE );

  // Convert
  trk_copy->is_route = !trk_copy->is_route;

  // Delete old one and then add new one
  if ( trk->is_route ) {
    vik_trw_layer_delete_route ( vtl, trk );
    vik_trw_layer_add_track ( vtl, NULL, trk_copy );
  }
  else {
    // Extra route conversion bits...
    (void)vik_track_merge_segments ( trk_copy );
    vik_track_to_routepoints ( trk_copy );

    vik_trw_layer_delete_track ( vtl, trk );
    vik_trw_layer_add_route ( vtl, NULL, trk_copy );
  }

  // Update in case color of track / route changes when moving between sublayers
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_anonymize_times ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( track )
    vik_track_anonymize_times ( track );
}

static void trw_layer_interpolate_times ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( track )
    vik_track_interpolate_times ( track );
}

/**
 * trw_layer_rotate:
 *
 * Shift start/end points around.
 * Particularly intended for circular routes were you want to adjust where the start/end point is.
 * Would be easier for the user to select positionally where they want
 *  the new start/end point, but this is more effort to program.
 * So here at least you can repeatedly try shifting it around by various values,
 *  until you get the start/end point where you want it.
 *
 * ATM Only on routes and tracks without timestamps,
 *  otherwise things get messy if there's timestamps as the implicit ordering by time would get broken.
 */
static void trw_layer_rotate ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( track ) {

    guint count = (guint)vik_track_get_tp_count ( track );
    if ( count < 2 ) {
      g_warning ( "%s: Not enough points", __FUNCTION__ );
    }

    // Check first and last points are 'reasonably' close,
    // otherwise warn about not being a 'circular route'
    GList *seg;
    VikTrackpoint *tp;
    seg = g_list_first ( track->trackpoints );
    tp = VIK_TRACKPOINT(seg->data);

    if ( !isnan(tp->timestamp) ) {
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This track has timestamps, so this operation is not allowed") );
      return;
    }

    GList *end = g_list_last ( track->trackpoints );
    VikTrackpoint *tp2 = VIK_TRACKPOINT(end->data);
    gdouble diff = vik_coord_diff ( &tp->coord, &(tp2->coord) );

    if ( diff > 1000.0 ) {
      if ( !a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                 _("The first and last points are quite far apart, this operation is intended for 'circular' routes.\n"
                                   "Do you wish to continue?"), NULL ) ) {
        return;
      }
    }
    
    gint shifts = a_dialog_get_non_zero_number ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                 _("Rotate"),
                                                 _("Shift by N points:"),
                                                 0,      // Default value
                                                 -count, // Min
                                                 count,  // Max
                                                 count/50 ? count/50 : 1);    // Step

    if ( !shifts )
      return;

    // Maintain the first segment
    // remove marker
    tp->newsegment = FALSE;
    
    if ( shifts > 0 ) {
      for ( gint shift = 0; shift < shifts; shift++ ) {
        // Repeatly move first point to last position
        GList *first = g_list_first ( track->trackpoints );
        gpointer tp = first->data;
        track->trackpoints = g_list_remove_link ( track->trackpoints, first );
        track->trackpoints = g_list_append ( track->trackpoints, tp );
      }
    } else {
      for ( gint shift = 0; shift > shifts; shift-- ) {
        // Repeatly move last point to first position
        GList *last = g_list_last ( track->trackpoints );
        gpointer tp = last->data;
        track->trackpoints = g_list_remove_link ( track->trackpoints, last );
        track->trackpoints = g_list_prepend ( track->trackpoints, tp );
      }
    }

    // Restore segment
    seg = g_list_first ( track->trackpoints );
    tp = VIK_TRACKPOINT(seg->data);
    tp->newsegment = TRUE;
    
    vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
}

/**
 * trw_layer_track_rename:
 *
 * Suggest new name based on track characteristics.
 *
 * Mainly to address Etrex naming if the track hasn't been reset/auto archived
 *  - everything is saved under '<first date after reset>'
 * such that splitting a track recording over multi days can become something like:
 * 20 DEC 20#1
 * 20 DEC 20#2 --> But might be on the 30th - so would be nice to be e.g. '30 DEC 20'
 * 20 DEC 20#3 --> But could be the 2nd Jan the following year - so would be nice to be e.g. '02 JAN 21'
 *
 * This intelligence could be put into the split operation,
 * but for now has to be manually requested
 */
static void trw_layer_track_rename ( menu_array_sublayer values )
{
#define VIK_SETTINGS_TRACK_RENAME_FMT "track_rename_format"
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack*)g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack*)g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  GtkWidget *dialog = gtk_dialog_new_with_buttons ( _("Rename"),
                                                    VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_STOCK_CANCEL,
                                                    GTK_RESPONSE_REJECT,
                                                    _("Preview"),
                                                    GTK_RESPONSE_APPLY,
                                                    GTK_STOCK_OK,
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL );

  GtkWidget *lname = gtk_label_new ( _("Old Name:") );
  GtkWidget *aname = gtk_label_new ( trk->name );

  // Default - read from settings
  gchar *dfmt = NULL;
  if ( ! a_settings_get_string ( VIK_SETTINGS_TRACK_RENAME_FMT, &dfmt ) )
    dfmt = g_strdup ( "%d %b %y %H:%M" );

  GtkWidget *entry = ui_entry_new ( dfmt, GTK_ENTRY_ICON_SECONDARY );
  gtk_widget_set_tooltip_text ( entry, _("Transform name using strftime() format") );

  GtkWidget *nname = gtk_label_new ( NULL );

  GtkWidget *hbox_name = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(hbox_name), lname, FALSE, FALSE, 2 );
  gtk_box_pack_end ( GTK_BOX(hbox_name), aname, TRUE, TRUE, 2 );

  GtkWidget *lfmt = gtk_label_new ( _("Format:") );
  GtkWidget *hbox_fmt = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(hbox_fmt), lfmt, FALSE, FALSE, 2 );
  gtk_box_pack_end ( GTK_BOX(hbox_fmt), entry, TRUE, TRUE, 2 );

  GtkWidget *lnn = gtk_label_new ( _("New Name:") );
  GtkWidget *hbox_new = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX(hbox_new), lnn, FALSE, FALSE, 2 );
  gtk_box_pack_end ( GTK_BOX(hbox_new), nname, TRUE, TRUE, 2 );

  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox_name, TRUE, TRUE, 2 );
  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox_fmt, TRUE, TRUE, 2 );
  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox_new, TRUE, TRUE, 2 );

  gtk_widget_show_all ( dialog );

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_REJECT );
  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  gint response;
  do {
    // Preview...
    gchar date_buf[128];
    date_buf[0] = '-'; date_buf[1] = '-'; date_buf[2] = '\0';
    if ( trk->trackpoints && !isnan(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp) ) {
      time_t time = round(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp);
      const gchar *fmt = gtk_entry_get_text ( GTK_ENTRY(entry) );
      if ( fmt && strlen(fmt) == 0 ) {
        // Reset if entry is blank
        gtk_entry_set_text ( GTK_ENTRY(entry), "%d %b %y %H:%M" );
        fmt = gtk_entry_get_text ( GTK_ENTRY(entry) );
      }
      if ( fmt )
        strftime ( date_buf, sizeof(date_buf), fmt, localtime(&time) );
    }
    gtk_label_set_text ( GTK_LABEL(nname), date_buf );

    response = gtk_dialog_run (GTK_DIALOG (dialog));

  } while ( response == GTK_RESPONSE_APPLY );

  if ( response == GTK_RESPONSE_ACCEPT ) {
    const gchar *newname = gtk_label_get_text(GTK_LABEL(nname));
    if ( newname && strlen(newname) && g_strcmp0("--", newname) ) {
      (void)trw_layer_sublayer_rename_request ( vtl,
                                                newname,
                                                values[MA_VLP],
                                                MA_SUBTYPE,
                                                values[MA_SUBLAYER_ID],
                                                values[MA_TV_ITER] );
      // Save format for use next time (even if the same as the default)
      const gchar *sfmt = gtk_entry_get_text ( GTK_ENTRY(entry) );
      if ( sfmt )
        a_settings_set_string ( VIK_SETTINGS_TRACK_RENAME_FMT, sfmt );
    }
  }

  gtk_widget_destroy ( dialog );
}

static void trw_layer_extend_track_end ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  vtl->current_track = track;
  vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, track->is_route ? TOOL_CREATE_ROUTE : TOOL_CREATE_TRACK);

  if ( track->trackpoints )
    goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vik_track_get_tp_last(track)->coord) );
}

/**
 * extend a track using route finder
 */
static void trw_layer_extend_track_end_route_finder ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikTrack *track = g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  if ( !track )
    return;

  vik_window_enable_layer_tool ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), VIK_LAYER_TRW, TOOL_ROUTE_FINDER );
  vtl->current_track = track;

  if ( track->trackpoints )
      goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &vik_track_get_tp_last(track)->coord );
}

/**
 *
 */
static gboolean trw_layer_dem_test ( VikTrwLayer *vtl, VikLayersPanel *vlp )
{
  // If have a vlp then perform a basic test to see if any DEM info available...
  if ( vlp ) {
    GList *dems = vik_layers_panel_get_all_layers_of_type (vlp, VIK_LAYER_DEM, TRUE); // Includes hidden DEM layer types

    if ( !g_list_length(dems) ) {
      a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No DEM layers available, thus no DEM values can be applied.") );
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * apply_dem_data_common:
 *
 * A common function for applying the DEM values and reporting the results.
 */
static void apply_dem_data_common ( VikTrwLayer *vtl, VikLayersPanel *vlp, VikTrack *track, gboolean skip_existing_elevations )
{
  if ( !trw_layer_dem_test ( vtl, vlp ) )
    return;

  gulong changed = vik_track_apply_dem_data ( track, skip_existing_elevations );
  // Inform user how much was changed
  gchar str[64];
  const gchar *tmp_str = ngettext("%ld point adjusted", "%ld points adjusted", changed);
  g_snprintf(str, 64, tmp_str, changed);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str);

  if ( changed )
    vik_layer_emit_update ( VIK_LAYER(vtl) );
}

static void trw_layer_apply_dem_data_all ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( track )
    apply_dem_data_common ( vtl, values[MA_VLP], track, FALSE );
}

static void trw_layer_apply_dem_data_only_missing ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( track )
    apply_dem_data_common ( vtl, values[MA_VLP], track, TRUE );
}

/**
 * smooth_it:
 *
 * A common function for applying the elevation smoothing and reporting the results.
 */
static void smooth_it ( VikTrwLayer *vtl, VikTrack *track, gboolean flat )
{
  gulong changed = vik_track_smooth_missing_elevation_data ( track, flat );
  // Inform user how much was changed
  gchar str[64];
  const gchar *tmp_str = ngettext("%ld point adjusted", "%ld points adjusted", changed);
  g_snprintf(str, 64, tmp_str, changed);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str);
}

/**
 *
 */
static void trw_layer_missing_elevation_data_interp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  smooth_it ( vtl, track, FALSE );
}

static void trw_layer_missing_elevation_data_flat ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  smooth_it ( vtl, track, TRUE );
}

/**
 * Commonal helper function
 */
static void wp_changed_message ( VikTrwLayer *vtl, guint changed )
{
  gchar str[64];
  const gchar *tmp_str = ngettext("%ld waypoint changed", "%ld waypoints changed", changed);
  g_snprintf(str, 64, tmp_str, changed);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str);
}

static void trw_layer_apply_dem_data_wpt_all ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikLayersPanel *vlp = (VikLayersPanel *)values[MA_VLP];

  if ( !trw_layer_dem_test ( vtl, vlp ) )
    return;

  guint changed = 0;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    // Single Waypoint
    VikWaypoint *wp = (VikWaypoint *) g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
    if ( wp )
      changed = (guint)vik_waypoint_apply_dem_data ( wp, FALSE );
  }
  else {
    // All waypoints
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init ( &iter, vtl->waypoints );
    while ( g_hash_table_iter_next (&iter, &key, &value) ) {
      VikWaypoint *wp = VIK_WAYPOINT(value);
      changed = changed + (gint)vik_waypoint_apply_dem_data ( wp, FALSE );
    }
  }
  wp_changed_message ( vtl, changed );
}

static void trw_layer_apply_dem_data_wpt_only_missing ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikLayersPanel *vlp = (VikLayersPanel *)values[MA_VLP];

  if ( !trw_layer_dem_test ( vtl, vlp ) )
    return;

  guint changed = 0;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    // Single Waypoint
    VikWaypoint *wp = (VikWaypoint *) g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
    if ( wp )
      changed = (guint)vik_waypoint_apply_dem_data ( wp, TRUE );
  }
  else {
    // All waypoints
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init ( &iter, vtl->waypoints );
    while ( g_hash_table_iter_next (&iter, &key, &value) ) {
      VikWaypoint *wp = VIK_WAYPOINT(value);
      changed = changed + (guint)vik_waypoint_apply_dem_data ( wp, TRUE );
    }
  }
  wp_changed_message ( vtl, changed );
}

static void trw_layer_goto_track_endpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;
  if ( !track->trackpoints )
    return;

  VikTrackpoint *tp = vik_track_get_tp_last (track );
  trw_layer_select_trackpoint ( vtl, track, tp, TRUE );
  goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(tp->coord) );
}

static void trw_layer_goto_track_max_speed ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  VikTrackpoint* vtp = vik_track_get_tp_by_max_speed ( track, vik_trw_layer_get_prefer_gps_speed(vtl) );
  if ( !vtp )
    return;
  trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
  goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord));
}

static void trw_layer_goto_track_max_alt ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  VikTrackpoint* vtp = vik_track_get_tp_by_max_alt ( track );
  if ( !vtp )
    return;
  trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
  goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord));
}

static void trw_layer_goto_track_min_alt ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  VikTrackpoint* vtp = vik_track_get_tp_by_min_alt ( track );
  if ( !vtp )
    return;
  trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
  goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord));
}

static void trw_layer_goto_track_max_hr ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track = (VikTrack *)g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  if ( track ) {
    VikTrackpoint* vtp = vik_track_get_tp_by_max_heart_rate ( track );
    if ( vtp ) {
      trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
      goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord) );
    }
  }
}

static void trw_layer_goto_track_max_cad ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track = (VikTrack *)g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  if ( track ) {
    VikTrackpoint* vtp = vik_track_get_tp_by_max_cadence ( track );
    if ( vtp ) {
      trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
      goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord) );
    }
  }
}

static void trw_layer_goto_track_min_temp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track = (VikTrack *)g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  if ( track ) {
    VikTrackpoint* vtp = vik_track_get_tp_by_min_temp ( track );
    if ( vtp ) {
      trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
      goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord) );
    }
  }
}

static void trw_layer_goto_track_max_temp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track = (VikTrack *)g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  if ( track ) {
    VikTrackpoint* vtp = vik_track_get_tp_by_max_temp ( track );
    if ( vtp ) {
      trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
      goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord) );
    }
  }
}

static void trw_layer_goto_track_max_power ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track = (VikTrack *)g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  if ( track ) {
    VikTrackpoint* vtp = vik_track_get_tp_by_max_power ( track );
    if ( vtp ) {
      trw_layer_select_trackpoint ( vtl, track, vtp, TRUE );
      goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(vtp->coord) );
    }
  }
}

/*
 * Automatically change the viewport to center on the track and zoom to see the extent of the track
 */
void vik_trw_layer_center_view_track ( VikTrwLayer *vtl, VikTrack *trk, VikViewport *vvp, VikLayersPanel *vlp )
{
  if ( trk && trk->trackpoints ) {
    struct LatLon maxmin[2] = { {0,0}, {0,0} };
    trw_layer_find_maxmin_tracks ( NULL, trk, maxmin );
    trw_layer_zoom_to_show_latlons ( vtl, vvp, maxmin );
    if ( vlp )
      vik_layers_panel_emit_update ( vlp );
    else
      vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
}

/*
 * Automatically change the viewport to center on the track and zoom to see the extent of the track
 */
static void trw_layer_auto_track_view ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( trk->visible )
    vik_treeview_item_set_visible_tree ( VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter) );

  vik_trw_layer_center_view_track ( vtl, trk, values[MA_VVP], VIK_LAYERS_PANEL(values[MA_VLP]) );
}

/*
 * Refine the selected track/route with a routing engine.
 * The routing engine is selected by the user, when requestiong the job.
 */
static void trw_layer_route_refine ( menu_array_sublayer values )
{
  static gint last_engine = 0;
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikTrack *trk;

  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( trk && trk->trackpoints )
  {
    /* Check size of the route */
    int nb = vik_track_get_tp_count(trk);
    if (nb > 100) {
      GtkWidget *dialog = gtk_message_dialog_new (VIK_GTK_WINDOW_FROM_LAYER (vtl),
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_WARNING,
                                                  GTK_BUTTONS_OK_CANCEL,
                                                  _("Refining a track with many points (%d) is unlikely to yield sensible results. Do you want to Continue?"),
                                                  nb);
      gint response = gtk_dialog_run ( GTK_DIALOG(dialog) );
      gtk_widget_destroy ( dialog );
      if (response != GTK_RESPONSE_OK )
        return;
    }
    /* Select engine from dialog */
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Refine Route with Routing Engine..."),
                                                  VIK_GTK_WINDOW_FROM_LAYER (vtl),
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
    GtkWidget *label = gtk_label_new ( _("Select routing engine") );
    gtk_widget_show_all(label);

    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, TRUE, TRUE, 0 );

    GtkWidget * combo = vik_routing_ui_selector_new ( (Predicate)vik_routing_engine_supports_refine, NULL );
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo), last_engine);
    gtk_widget_show_all(combo);

    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), combo, TRUE, TRUE, 0 );

    gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

    if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
    {
        /* Dialog validated: retrieve selected engine and do the job */
        last_engine = gtk_combo_box_get_active ( GTK_COMBO_BOX(combo) );
        VikRoutingEngine *routing = vik_routing_ui_selector_get_nth (combo, last_engine);

        /* Change cursor */
        vik_window_set_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );

        /* Force saving track */
        /* FIXME: remove or rename this hack */
        vtl->route_finder_check_added_track = TRUE;

        /* the job */
        vik_routing_engine_refine (routing, vtl, trk);

        /* FIXME: remove or rename this hack */
        if ( vtl->route_finder_added_track )
          vik_track_calculate_bounds ( vtl->route_finder_added_track );

        vtl->route_finder_added_track = NULL;
        vtl->route_finder_check_added_track = FALSE;

        vik_layer_emit_update ( VIK_LAYER(vtl) );

        /* Restore cursor */
        vik_window_clear_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
    }
    gtk_widget_destroy ( dialog );
  }
}

static void trw_layer_edit_trackpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
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
  VikTrack *exclude;
  gboolean with_timestamps;
} twt_udata;
static void find_tracks_with_timestamp_type(gpointer key, gpointer value, gpointer udata)
{
  twt_udata *user_data = udata;
  VikTrackpoint *p1, *p2;
  VikTrack *trk = VIK_TRACK(value);
  if (trk == user_data->exclude) {
    return;
  }

  if (trk->trackpoints) {
    p1 = vik_track_get_tp_first(trk);
    p2 = vik_track_get_tp_last(trk);

    if ( user_data->with_timestamps ) {
      if (isnan(p1->timestamp) || isnan(p2->timestamp)) {
	return;
      }
    }
    else {
      // Don't add tracks with timestamps when getting non timestamp tracks
      if (!isnan(p1->timestamp) || !isnan(p2->timestamp)) {
	return;
      }
    }
  }

  *(user_data->result) = g_list_prepend(*(user_data->result), key);
}

/**
 * find_nearby_tracks_by_time:
 *
 * Called for each track in track hash table.
 *  If the original track (in user_data[1]) is close enough (threshold period in user_data[2])
 *  to the current track, then the current track is added to the list in user_data[0]
 */
static void find_nearby_tracks_by_time (gpointer key, gpointer value, gpointer user_data)
{
  VikTrack *trk = VIK_TRACK(value);

  GList **nearby_tracks = ((gpointer *)user_data)[0];
  VikTrack *orig_trk = VIK_TRACK(((gpointer *)user_data)[1]);

  if ( !orig_trk || !orig_trk->trackpoints )
    return;

  /* outline: 
   * detect reasons for not merging, and return
   * if no reason is found not to merge, then do it.
   */

  twt_udata *udata = user_data;
  // Exclude the original track from the compiled list
  if (trk == udata->exclude) {
    return;
  }

  gdouble t1 = vik_track_get_tp_first(orig_trk)->timestamp;
  gdouble t2 = vik_track_get_tp_last(orig_trk)->timestamp;

  if (trk->trackpoints) {

    VikTrackpoint *p1 = vik_track_get_tp_first(trk);
    VikTrackpoint *p2 = vik_track_get_tp_last(trk);

    if (isnan(p1->timestamp) || isnan(p2->timestamp)) {
      //g_print("no timestamp\n");
      return;
    }

    guint threshold = GPOINTER_TO_UINT (((gpointer *)user_data)[2]);
    //g_print("Got track named %s, times %d, %d\n", trk->name, p1->timestamp, p2->timestamp);
    if (! (fabs(t1 - p2->timestamp) < threshold ||
      /*  p1 p2      t1 t2 */
      fabs(p1->timestamp - t2) < threshold)
      /*  t1 t2      p1 p2 */
    ) {
      return;
    }
  }

  *nearby_tracks = g_list_prepend(*nearby_tracks, value);
}

/* comparison function used to sort tracks; a and b are hash table keys */
/* Not actively used - can be restored if needed
static gint track_compare(gconstpointer a, gconstpointer b, gpointer user_data)
{
  GHashTable *tracks = user_data;
  gdouble t1, t2;

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
  gdouble t1 = VIK_TRACKPOINT(a)->timestamp, t2 = VIK_TRACKPOINT(b)->timestamp;
  
  if (t1 < t2) return -1;
  if (t1 > t2) return 1;
  return 0;
}

/**
 * comparison function which can be used to sort tracks or waypoints by name
 */
static gint sort_alphabetically (gconstpointer a, gconstpointer b)
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
static void trw_layer_merge_with_other ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  GList *other_tracks = NULL;
  GHashTable *ght_tracks;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    ght_tracks = vtl->routes;
  else
    ght_tracks = vtl->tracks;

  VikTrack *track = (VikTrack *) g_hash_table_lookup ( ght_tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  if ( !track->trackpoints )
    return;

  twt_udata udata;
  udata.result = &other_tracks;
  udata.exclude = track;
  // Allow merging with 'similar' time type time tracks
  // i.e. either those times, or those without
  udata.with_timestamps = !isnan(vik_track_get_tp_first(track)->timestamp);

  g_hash_table_foreach(ght_tracks, find_tracks_with_timestamp_type, (gpointer)&udata);
  other_tracks = g_list_reverse(other_tracks);

  if ( !other_tracks ) {
    if ( udata.with_timestamps )
      a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other tracks with timestamps in this layer found"));
    else
      a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other tracks without timestamps in this layer found"));
    return;
  }

  // Sort alphabetically for user presentation
  // Convert into list of names for usage with dialog function
  // TODO: Need to consider how to work best when we can have multiple tracks the same name...
  GList *other_tracks_names = NULL;
  GList *iter = g_list_first ( other_tracks );
  while ( iter ) {
    other_tracks_names = g_list_append ( other_tracks_names, VIK_TRACK(g_hash_table_lookup (ght_tracks, iter->data))->name );
    iter = g_list_next ( iter );
  }

  other_tracks_names = g_list_sort (other_tracks_names, sort_alphabetically);

  GList *merge_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                other_tracks_names,
                                                TRUE,
                                                _("Merge with..."),
                                                track->is_route ? _("Select route to merge with") : _("Select track to merge with"));
  g_list_free(other_tracks);
  g_list_free(other_tracks_names);

  if (merge_list)
  {
    GList *l;
    for (l = merge_list; l != NULL; l = g_list_next(l)) {
      VikTrack *merge_track;
      if ( track->is_route )
        merge_track = vik_trw_layer_get_route ( vtl, l->data );
      else
        merge_track = vik_trw_layer_get_track ( vtl, l->data );

      if (merge_track) {
        vik_track_steal_and_append_trackpoints ( track, merge_track );
        if ( track->is_route )
          vik_trw_layer_delete_route (vtl, merge_track);
        else
          vik_trw_layer_delete_track (vtl, merge_track);
        track->trackpoints = g_list_sort(track->trackpoints, trackpoint_compare);
      }
    }
    for (l = merge_list; l != NULL; l = g_list_next(l))
      g_free(l->data);
    g_list_free(merge_list);

    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

// c.f. trw_layer_sorted_track_id_by_name_list
//  but don't add the specified track to the list (normally current track)
static void trw_layer_sorted_track_id_by_name_list_exclude_self (const gpointer id, const VikTrack *trk, gpointer udata)
{
  twt_udata *user_data = udata;

  // Skip self
  if (trk == user_data->exclude) {
    return;
  }

  // Sort named list alphabetically
  *(user_data->result) = g_list_insert_sorted (*(user_data->result), trk->name, sort_alphabetically);
}

/**
 * Join - this allows combining 'tracks' and 'track routes'
 *  i.e. doesn't care about whether tracks have consistent timestamps
 * ATM can only append one track at a time to the currently selected track
 */
static void trw_layer_append_track ( menu_array_sublayer values )
{

  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  GHashTable *ght_tracks;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    ght_tracks = vtl->routes;
  else
    ght_tracks = vtl->tracks;

  trk = (VikTrack *) g_hash_table_lookup ( ght_tracks, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  GList *other_tracks_names = NULL;

  // Sort alphabetically for user presentation
  // Convert into list of names for usage with dialog function
  // TODO: Need to consider how to work best when we can have multiple tracks the same name...
  twt_udata udata;
  udata.result = &other_tracks_names;
  udata.exclude = trk;

  g_hash_table_foreach(ght_tracks, (GHFunc) trw_layer_sorted_track_id_by_name_list_exclude_self, (gpointer)&udata);

  // Note the limit to selecting one track only
  //  this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
  //  (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically)
  GList *append_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                 other_tracks_names,
                                                 FALSE,
                                                 trk->is_route ? _("Append Route"): _("Append Track"),
                                                 trk->is_route ? _("Select the route to append after the current route") :
                                                                 _("Select the track to append after the current track") );

  g_list_free(other_tracks_names);

  // It's a list, but shouldn't contain more than one other track!
  if ( append_list ) {
    GList *l;
    for (l = append_list; l != NULL; l = g_list_next(l)) {
      // TODO: at present this uses the first track found by name,
      //  which with potential multiple same named tracks may not be the one selected...
      VikTrack *append_track;
      if ( trk->is_route )
        append_track = vik_trw_layer_get_route ( vtl, l->data );
      else
        append_track = vik_trw_layer_get_track ( vtl, l->data );

      if ( append_track ) {
        vik_track_steal_and_append_trackpoints ( trk, append_track );
        if ( trk->is_route )
          vik_trw_layer_delete_route (vtl, append_track);
        else
          vik_trw_layer_delete_track (vtl, append_track);
      }
    }
    for (l = append_list; l != NULL; l = g_list_next(l))
      g_free(l->data);
    g_list_free(append_list);

    // Routes can only have one segment
    if ( trk->is_route ) {
      (void)vik_track_merge_segments ( trk );
    }

    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

/**
 * Very similar to trw_layer_append_track for joining
 * but this allows selection from the 'other' list
 * If a track is selected, then is shows routes and joins the selected one
 * If a route is selected, then is shows tracks and joins the selected one
 */
static void trw_layer_append_other ( menu_array_sublayer values )
{

  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  GHashTable *ght_mykind, *ght_others;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
    ght_mykind = vtl->routes;
    ght_others = vtl->tracks;
  }
  else {
    ght_mykind = vtl->tracks;
    ght_others = vtl->routes;
  }

  trk = (VikTrack *) g_hash_table_lookup ( ght_mykind, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  GList *other_tracks_names = NULL;

  // Sort alphabetically for user presentation
  // Convert into list of names for usage with dialog function
  // TODO: Need to consider how to work best when we can have multiple tracks the same name...
  twt_udata udata;
  udata.result = &other_tracks_names;
  udata.exclude = trk;

  g_hash_table_foreach(ght_others, (GHFunc) trw_layer_sorted_track_id_by_name_list_exclude_self, (gpointer)&udata);

  // Note the limit to selecting one track only
  //  this is to control the ordering of appending tracks, i.e. the selected track always goes after the current track
  //  (otherwise with multiple select the ordering would not be controllable by the user - automatically being alphabetically)
  GList *append_list = a_dialog_select_from_list(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                 other_tracks_names,
                                                 FALSE,
                                                 trk->is_route ? _("Append Track"): _("Append Route"),
                                                 trk->is_route ? _("Select the track to append after the current route") :
                                                                 _("Select the route to append after the current track") );

  g_list_free(other_tracks_names);

  // It's a list, but shouldn't contain more than one other track!
  if ( append_list ) {
    GList *l;
    for (l = append_list; l != NULL; l = g_list_next(l)) {
      // TODO: at present this uses the first track found by name,
      //  which with potential multiple same named tracks may not be the one selected...

      // Get FROM THE OTHER TYPE list
      VikTrack *append_track;
      if ( trk->is_route )
        append_track = vik_trw_layer_get_track ( vtl, l->data );
      else
        append_track = vik_trw_layer_get_route ( vtl, l->data );

      if ( append_track ) {

        if ( !append_track->is_route &&
             ( ( vik_track_get_segment_count ( append_track ) > 1 ) ||
               ( vik_track_get_average_speed ( append_track ) > 0.0 ) ) ) {

          if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                      _("Converting a track to a route removes extra track data such as segments, timestamps, etc...\nDo you want to continue?"), NULL ) ) {
	    (void)vik_track_merge_segments ( append_track );
	    vik_track_to_routepoints ( append_track );
	  }
          else {
            break;
          }
        }

        vik_track_steal_and_append_trackpoints ( trk, append_track );

	// Delete copied which is FROM THE OTHER TYPE list
        if ( trk->is_route )
          vik_trw_layer_delete_track (vtl, append_track);
	else
          vik_trw_layer_delete_route (vtl, append_track);
      }
    }
    for (l = append_list; l != NULL; l = g_list_next(l))
      g_free(l->data);
    g_list_free(append_list);

    // Routes can only have one segment
    if ( trk->is_route ) {
      (void)vik_track_merge_segments ( trk );
    }

    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

/* merge by segments */
static void trw_layer_merge_by_segment ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  guint segments = vik_track_merge_segments ( trk );
  vik_layer_emit_update ( VIK_LAYER(vtl) );
  // Any gaps previously in tracks may be too small to notice that they've now gone,
  //  so put up a message to confirm what has happened.
  gchar str[64];
  const gchar *tmp_str = ngettext("%d segment merged", "%d segments merged", segments);
  g_snprintf(str, 64, tmp_str, segments);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str );
}

/* merge by time routine */
static void trw_layer_merge_by_timestamp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];

  GList *tracks_with_timestamp = NULL;
  VikTrack *orig_trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  if (orig_trk->trackpoints &&
      isnan(vik_track_get_tp_first(orig_trk)->timestamp)) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. This track does not have timestamp"));
    return;
  }

  twt_udata udata;
  udata.result = &tracks_with_timestamp;
  udata.exclude = orig_trk;
  udata.with_timestamps = TRUE;
  g_hash_table_foreach(vtl->tracks, find_tracks_with_timestamp_type, (gpointer)&udata);
  tracks_with_timestamp = g_list_reverse(tracks_with_timestamp);

  if (!tracks_with_timestamp) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Failed. No other track in this layer has timestamp"));
    return;
  }
  g_list_free(tracks_with_timestamp);

  static guint threshold_in_minutes = 1;
  if (!a_dialog_time_threshold(VIK_GTK_WINDOW_FROM_LAYER(vtl),
                               _("Merge Threshold..."),
                               _("Merge when time between tracks less than:"),
                               &threshold_in_minutes)) {
    return;
  }

  // keep attempting to merge all tracks until no merges within the time specified is possible
  gboolean attempt_merge = TRUE;
  GList *nearby_tracks = NULL;
  GList *trps;
  static gpointer params[3];

  while ( attempt_merge ) {

    // Don't try again unless tracks have changed
    attempt_merge = FALSE;

    trps = orig_trk->trackpoints;
    if ( !trps )
      return;

    if (nearby_tracks) {
      g_list_free(nearby_tracks);
      nearby_tracks = NULL;
    }

    params[0] = &nearby_tracks;
    params[1] = orig_trk;
    params[2] = GUINT_TO_POINTER (threshold_in_minutes*60); // In seconds

    /* get a list of adjacent-in-time tracks */
    g_hash_table_foreach(vtl->tracks, find_nearby_tracks_by_time, params);

    /* merge them */
    GList *l = nearby_tracks;
    while ( l ) {
      /* remove trackpoints from merged track, delete track */
      vik_track_steal_and_append_trackpoints ( orig_trk, VIK_TRACK(l->data) );
      vik_trw_layer_delete_track (vtl, VIK_TRACK(l->data));

      // Tracks have changed, therefore retry again against all the remaining tracks
      attempt_merge = TRUE;

      l = g_list_next(l);
    }

    orig_trk->trackpoints = g_list_sort(orig_trk->trackpoints, trackpoint_compare);
  }

  g_list_free(nearby_tracks);

  if ( values[MA_VLP] )
    vik_layers_panel_calendar_update ( VIK_LAYERS_PANEL(values[MA_VLP]) );

  vik_layer_emit_update( VIK_LAYER(vtl) );
}

/**
 * Split a track at the currently selected trackpoint into a new segment
 */
static void trw_layer_split_create_segments ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];

  if ( vtl->current_tpl && vtl->current_tp_track && !vtl->current_tp_track->is_route ) {
    if ( vtl->current_tpl->next && vtl->current_tpl->prev ) {
        VIK_TRACKPOINT(vtl->current_tpl->data)->newsegment = TRUE;
        vik_layer_emit_update(VIK_LAYER(vtl));
    }
  }
}

/**
 * Split a track at the currently selected trackpoint
 */
static void trw_layer_split_at_selected_trackpoint ( VikTrwLayer *vtl, gint subtype )
{
  if ( !vtl->current_tpl )
    return;

  if ( vtl->current_tpl->next && vtl->current_tpl->prev ) {
    gchar *name = trw_layer_new_unique_sublayer_name(vtl, subtype, vtl->current_tp_track->name);
    if ( name ) {
      VikTrack *tr = vik_track_copy ( vtl->current_tp_track, FALSE );
      GList *newglist = g_list_alloc ();
      newglist->prev = NULL;
      newglist->next = vtl->current_tpl->next;
      newglist->data = vik_trackpoint_copy(VIK_TRACKPOINT(vtl->current_tpl->data));
      tr->trackpoints = newglist;

      vtl->current_tpl->next->prev = newglist; /* end old track here */
      vtl->current_tpl->next = NULL;

      // Bounds of the selected track changed due to the split
      vik_track_calculate_bounds ( vtl->current_tp_track );

      vtl->current_tpl = newglist; /* change tp to first of new track. */
      vtl->current_tp_track = tr;

      if ( tr->is_route )
        vik_trw_layer_add_route ( vtl, name, tr );
      else
        vik_trw_layer_add_track ( vtl, name, tr );

      // Bounds of the new track created by the split
      vik_track_calculate_bounds ( tr );

      vik_layer_emit_update(VIK_LAYER(vtl));
    }
    g_free ( name );
  }
}

/* split by time routine */
static void trw_layer_split_by_timestamp ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  GList *trps = track->trackpoints;
  GList *iter;
  GList *newlists = NULL;
  GList *newtps = NULL;
  static guint thr = 1;

  gdouble ts, prev_ts;

  if ( !trps )
    return;

  if (!a_dialog_time_threshold(VIK_GTK_WINDOW_FROM_LAYER(vtl), 
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

    // Check for unordered time points - this is quite a rare occurence - unless one has reversed a track.
    if (ts < prev_ts) {
      gchar tmp_str[64];
      time_t lt = round (ts);
      strftime ( tmp_str, sizeof(tmp_str), "%c", localtime(&lt) );
      if ( a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                _("Can not split track due to trackpoints not ordered in time - such as at %s.\n\nGoto this trackpoint?"),
                                tmp_str ) ) {
        goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(VIK_TRACKPOINT(iter->data)->coord) );
      }
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
  // Only bother updating if the split results in new tracks
  if (g_list_length (newlists) > 1) {
    while (iter) {
      gchar *new_tr_name;
      VikTrack *tr;

      tr = vik_track_copy ( track, FALSE );
      tr->trackpoints = (GList *)(iter->data);

      new_tr_name = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, track->name);
      vik_trw_layer_add_track(vtl, new_tr_name, tr);
      g_free ( new_tr_name );
      vik_track_calculate_bounds ( tr );
      iter = g_list_next(iter);
    }
    // Remove original track and then update the display
    vik_trw_layer_delete_track (vtl, track);
    vik_layer_emit_update(VIK_LAYER(vtl));

    if ( values[MA_VLP] )
      vik_layers_panel_calendar_update ( VIK_LAYERS_PANEL(values[MA_VLP]) );
  }
  g_list_free(newlists);
}

/**
 * Split a track by the number of points as specified by the user
 */
static void trw_layer_split_by_n_points ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !track )
    return;

  // Check valid track
  GList *trps = track->trackpoints;
  if ( !trps )
    return;

  guint points = a_dialog_get_positive_number ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                _("Split Every Nth Point"),
                                                _("Split on every Nth point:"),
                                                250,   // Default value as per typical limited track capacity of various GPS devices
                                                2,     // Min
                                                65536, // Max
                                                5 );    // Step
  // Was a valid number returned?
  if (!points)
    return;

  // Now split...
  GList *iter;
  GList *newlists = NULL;
  GList *newtps = NULL;
  guint count = 0;
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
  // Only bother updating if the split results in new tracks
  if (g_list_length (newlists) > 1) {
    while (iter) {
      gchar *new_tr_name;
      VikTrack *tr;

      tr = vik_track_copy ( track, FALSE );
      tr->trackpoints = (GList *)(iter->data);

      if ( track->is_route ) {
        new_tr_name = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_ROUTE, track->name);
        vik_trw_layer_add_route(vtl, new_tr_name, tr);
      }
      else {
        new_tr_name = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, track->name);
        vik_trw_layer_add_track(vtl, new_tr_name, tr);
      }
      g_free ( new_tr_name );
      vik_track_calculate_bounds ( tr );

      iter = g_list_next(iter);
    }
    // Remove original track and then update the display
    if ( track->is_route )
      vik_trw_layer_delete_route (vtl, track);
    else
      vik_trw_layer_delete_track (vtl, track);
    vik_layer_emit_update(VIK_LAYER(vtl));
  }
  g_list_free(newlists);
}

/**
 * Split a track at the currently selected trackpoint
 */
static void trw_layer_split_at_trackpoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  gint subtype = GPOINTER_TO_INT (values[MA_SUBTYPE]);
  trw_layer_split_at_selected_trackpoint ( vtl, subtype );
}

/**
 * Split a track by its segments
 * Routes do not have segments so don't call this for routes
 */
static void trw_layer_split_segments ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk = g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  guint ntracks;

  VikTrack **tracks = vik_track_split_into_segments (trk, &ntracks);
  gchar *new_tr_name;
  guint i;
  for ( i = 0; i < ntracks; i++ ) {
    if ( tracks[i] ) {
      new_tr_name = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, trk->name);
      vik_trw_layer_add_track ( vtl, new_tr_name, tracks[i] );
      g_free ( new_tr_name );
    }
  }
  if ( tracks ) {
    g_free ( tracks );
    // Remove original track
    vik_trw_layer_delete_track ( vtl, trk );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    if ( values[MA_VLP] )
      vik_layers_panel_calendar_update ( VIK_LAYERS_PANEL(values[MA_VLP]) );
  }
  else {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Can not split track as it has no segments"));
  }
}
/* end of split/merge routines */

static void trw_layer_trackpoint_selected_delete ( VikTrwLayer *vtl, VikTrack *trk )
{
  GList *new_tpl;

  // Find available adjacent trackpoint
  if ( (new_tpl = vtl->current_tpl->next) || (new_tpl = vtl->current_tpl->prev) ) {
    if ( VIK_TRACKPOINT(vtl->current_tpl->data)->newsegment && vtl->current_tpl->next )
      VIK_TRACKPOINT(vtl->current_tpl->next->data)->newsegment = TRUE; /* don't concat segments on del */

    // Delete current trackpoint
    vik_trackpoint_free ( vtl->current_tpl->data );
    trk->trackpoints = g_list_delete_link ( trk->trackpoints, vtl->current_tpl );

    // Set to current to the available adjacent trackpoint
    vtl->current_tpl = new_tpl;

    if ( vtl->current_tp_track ) {
      vik_track_calculate_bounds ( vtl->current_tp_track );
    }
  }
  else {
    // Delete current trackpoint
    vik_trackpoint_free ( vtl->current_tpl->data );
    trk->trackpoints = g_list_delete_link ( trk->trackpoints, vtl->current_tpl );
    trw_layer_cancel_current_tp ( vtl, FALSE );
  }
}

/**
 * Delete the selected point
 */
static void trw_layer_delete_point_selected ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  if ( !vtl->current_tpl )
    return;

  trw_layer_trackpoint_selected_delete ( vtl, trk );

  // Track has been updated so update tps:
  trw_layer_cancel_tps_of_track ( vtl, trk );

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Delete the currently selected trackpoint in the currently selected track
 */
void vik_trw_layer_delete_trackpoint_selected ( VikTrwLayer *vtl )
{
  g_return_if_fail ( IS_VIK_TRW_LAYER(vtl) );

  if ( vtl->current_tp_track && vtl->current_tpl ) {
    trw_layer_trackpoint_selected_delete ( vtl, vtl->current_tp_track );

    if ( vtl->tpwin && vtl->current_tpl )
      // Reset dialog with the available adjacent trackpoint
      my_tpwin_set_tp ( vtl );

    vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
}

/**
 * Delete specified waypoint in layer
 */
void vik_trw_layer_delete_waypoint ( VikTrwLayer *vtl, VikWaypoint *wpt )
{
  g_return_if_fail ( IS_VIK_TRW_LAYER(vtl) );
  g_return_if_fail ( wpt != NULL );

  if ( trw_layer_delete_waypoint(vtl, wpt) )
    vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Delete adjacent track points at the same position
 * AKA Delete Dulplicates on the Properties Window
 */
static void trw_layer_delete_points_same_position ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  gulong removed = vik_track_remove_dup_points ( trk );

  // Track has been updated so update tps:
  trw_layer_cancel_tps_of_track ( vtl, trk );

  // Inform user how much was deleted as it's not obvious from the normal view
  gchar str[64];
  const gchar *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
  g_snprintf(str, 64, tmp_str, removed);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Delete adjacent track points with the same timestamp
 * Normally new tracks that are 'routes' won't have any timestamps so should be OK to clean up the track
 */
static void trw_layer_delete_points_same_time ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( !trk )
    return;

  gulong removed = vik_track_remove_same_time_points ( trk );

  // Track has been updated so update tps:
  trw_layer_cancel_tps_of_track ( vtl, trk );

  // Inform user how much was deleted as it's not obvious from the normal view
  gchar str[64];
  const gchar *tmp_str = ngettext("Deleted %ld point", "Deleted %ld points", removed);
  g_snprintf(str, 64, tmp_str, removed);
  a_dialog_info_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), str);

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Insert a point
 */
static void trw_layer_insert_point_after ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( ! track )
    return;

  trw_layer_insert_tp_beside_current_tp ( vtl, FALSE, GPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE );
}

static void trw_layer_insert_point_before ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( ! track )
    return;

  trw_layer_insert_tp_beside_current_tp ( vtl, TRUE, GPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE );
}

/**
 * Reverse a track
 */
static void trw_layer_reverse ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = (VikTrwLayer *)values[MA_VTL];
  VikTrack *track;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    track = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    track = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );

  if ( ! track )
    return;

  vik_track_reverse ( track );
 
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * Open a program at the specified date
 * Mainly for RedNotebook - http://rednotebook.sourceforge.net/
 * But could work with any program that accepts a command line of --date=<date>
 * FUTURE: Allow configuring of command line options + date format
 */
static void trw_layer_diary_open ( VikTrwLayer *vtl, const gchar *date_str )
{
  GError *err = NULL;
  gchar *cmd = g_strdup_printf ( "%s %s%s", diary_program, "--date=", date_str );
  if ( ! g_spawn_command_line_async ( cmd, &err ) ) {
    a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Could not launch %s to open file."), diary_program );
    g_warning ( "%s", err->message );
    g_error_free ( err );
  }
  g_free ( cmd );
}

/**
 * Open a diary at the date of the track or waypoint
 */
static void trw_layer_diary ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  if ( GPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
    VikTrack *trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
    if ( ! trk )
      return;

    gchar date_buf[20];
    date_buf[0] = '\0';
    if ( trk->trackpoints && !isnan(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp) ) {
      time_t time = round ( VIK_TRACKPOINT(trk->trackpoints->data)->timestamp );
      strftime (date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&time));
      trw_layer_diary_open ( vtl, date_buf );
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This track has no date information.") );
  }
  else if ( GPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    VikWaypoint *wpt = (VikWaypoint *) g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
    if ( ! wpt )
      return;

    gchar date_buf[20];
    date_buf[0] = '\0';
    if ( !isnan(wpt->timestamp) ) {
      time_t time = round ( wpt->timestamp );
      strftime (date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&time));
      trw_layer_diary_open ( vtl, date_buf );
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This waypoint has no date information.") );
  }
}

/**
 * Open a program at the specified date
 * Mainly for Stellarium - http://stellarium.org/
 * But could work with any program that accepts the same command line options...
 * FUTURE: Allow configuring of command line options + format or parameters
 */
static void trw_layer_astro_open ( VikTrwLayer *vtl, const gchar *date_str, const gchar *time_str, const gchar *lat_str, const gchar *lon_str, const gchar *alt_str )
{
  GError *err = NULL;
  gchar *tmp;
  gint fd = g_file_open_tmp ( "vik-astro-XXXXXX.ini", &tmp, &err );
  if (fd < 0) {
    g_warning ( "%s: Failed to open temporary file: %s", __FUNCTION__, err->message );
    g_clear_error ( &err );
    return;
  }
  gchar *cmd = g_strdup_printf ( "%s %s %s %s %s %s %s %s %s %s %s %s %s %s",
                                  astro_program, "-c", tmp, "--full-screen no", "--sky-date", date_str, "--sky-time", time_str, "--latitude", lat_str, "--longitude", lon_str, "--altitude", alt_str );
  g_warning ( "%s", cmd );
  if ( ! g_spawn_command_line_async ( cmd, &err ) ) {
    a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Could not launch %s"), astro_program );
    g_warning ( "%s", err->message );
    g_error_free ( err );
  }
  util_add_to_deletion_list ( tmp );
  g_free ( tmp );
  g_free ( cmd );
}

// Format of stellarium lat & lon seems designed to be particularly awkward
//  who uses ' & " in the parameters for the command line?!
// -1d4'27.48"
// +53d58'16.65"
static gchar *convert_to_dms ( gdouble dec )
{
  gdouble tmp;
  gchar sign_c = ' ';
  gint val_d, val_m;
  gdouble val_s;
  gchar *result = NULL;

  if ( dec > 0 )
    sign_c = '+';
  else if ( dec < 0 )
    sign_c = '-';
  else // Nul value
    sign_c = ' ';

  // Degrees
  tmp = fabs(dec);
  val_d = (gint)tmp;

  // Minutes
  tmp = (tmp - val_d) * 60;
  val_m = (gint)tmp;

  // Seconds
  val_s = (tmp - val_m) * 60;

  // Format
  result = g_strdup_printf ( "%c%dd%d\\\'%.4f\\\"", sign_c, val_d, val_m, val_s );
  return result;
}

/**
 * Open an astronomy program at the date & position of the track center, trackpoint or waypoint
 */
static void trw_layer_astro ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  if ( GPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
    VikTrack *trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
    if ( ! trk )
      return;

    VikTrackpoint *tp = NULL;
    if ( vtl->current_tpl )
      // Current Trackpoint
      tp = VIK_TRACKPOINT(vtl->current_tpl->data);
    else if ( trk->trackpoints )
      // Otherwise first trackpoint
      tp = VIK_TRACKPOINT(trk->trackpoints->data);
    else
      // Give up
      return;

    if ( !isnan(tp->timestamp) ) {
      gchar date_buf[20];
      time_t time = round ( tp->timestamp );
      strftime (date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&time));
      gchar time_buf[20];
      strftime (time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&time));
      struct LatLon ll;
      vik_coord_to_latlon ( &tp->coord, &ll );
      gchar *lat_str = convert_to_dms ( ll.lat );
      gchar *lon_str = convert_to_dms ( ll.lon );
      gchar alt_buf[20];
      snprintf (alt_buf, sizeof(alt_buf), "%d", (gint)round(tp->altitude) );
      trw_layer_astro_open ( vtl, date_buf, time_buf, lat_str, lon_str, alt_buf);
      g_free ( lat_str );
      g_free ( lon_str );
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This track has no date information.") );
  }
  else if ( GPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    VikWaypoint *wpt = (VikWaypoint *) g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
    if ( ! wpt )
      return;

    if ( !isnan(wpt->timestamp) ) {
      gchar date_buf[20];
      time_t time = round ( wpt->timestamp );
      strftime (date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&time));
      gchar time_buf[20];
      strftime (time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&time));
      struct LatLon ll;
      vik_coord_to_latlon ( &wpt->coord, &ll );
      gchar *lat_str = convert_to_dms ( ll.lat );
      gchar *lon_str = convert_to_dms ( ll.lon );
      gchar alt_buf[20];
      snprintf (alt_buf, sizeof(alt_buf), "%d", (gint)round(wpt->altitude) );
      trw_layer_astro_open ( vtl, date_buf, time_buf, lat_str, lon_str, alt_buf );
      g_free ( lat_str );
      g_free ( lon_str );
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("This waypoint has no date information.") );
  }
}

/**
 * Similar to trw_layer_enum_item, but this uses a sorted method
 */
/* Currently unused
static void trw_layer_sorted_name_list(gpointer key, gpointer value, gpointer udata)
{
  GList **list = (GList**)udata;
  // *list = g_list_prepend(*all, key); //unsorted method
  // Sort named list alphabetically
  *list = g_list_insert_sorted (*list, key, sort_alphabetically);
}
*/

/**
 * Now Waypoint specific sort
 */
static void trw_layer_sorted_wp_id_by_name_list (const gpointer id, const VikWaypoint *wp, gpointer udata)
{
  GList **list = (GList**)udata;
  // Sort named list alphabetically
  *list = g_list_insert_sorted (*list, wp->name, sort_alphabetically);
}

/**
 * Track specific sort
 */
static void trw_layer_sorted_track_id_by_name_list (const gpointer id, const VikTrack *trk, gpointer udata)
{
  GList **list = (GList**)udata;
  // Sort named list alphabetically
  *list = g_list_insert_sorted (*list, trk->name, sort_alphabetically);
}


typedef struct {
  gboolean    has_same_track_name;
  const gchar *same_track_name;
} same_track_name_udata;

static gint check_tracks_for_same_name ( gconstpointer aa, gconstpointer bb, gpointer udata )
{
  const gchar* namea = (const gchar*) aa;
  const gchar* nameb = (const gchar*) bb;

  // the test
  gint result = strcmp ( namea, nameb );

  if ( result == 0 ) {
    // Found two names the same
    same_track_name_udata *user_data = udata;
    user_data->has_same_track_name = TRUE;
    user_data->same_track_name = namea;
  }

  // Leave ordering the same
  return 0;
}

/**
 * Find out if any tracks have the same name in this hash table
 */
static gboolean trw_layer_has_same_track_names ( GHashTable *ht_tracks )
{
  // Sort items by name, then compare if any next to each other are the same

  GList *track_names = NULL;
  g_hash_table_foreach ( ht_tracks, (GHFunc) trw_layer_sorted_track_id_by_name_list, &track_names );

  // No tracks
  if ( ! track_names )
    return FALSE;

  same_track_name_udata udata;
  udata.has_same_track_name = FALSE;

  // Use sort routine to traverse list comparing items
  // Don't care how this list ends up ordered ( doesn't actually change ) - care about the returned status
  GList *dummy_list = g_list_sort_with_data ( track_names, check_tracks_for_same_name, &udata );
  // Still no tracks...
  if ( ! dummy_list )
    return FALSE;

  return udata.has_same_track_name;
}

/**
 * Force unqiue track names for the track table specified
 * Note the panel is a required parameter to enable the update of the names displayed
 * Specify if on tracks or else on routes
 */
static void vik_trw_layer_uniquify_tracks ( VikTrwLayer *vtl, VikLayersPanel *vlp, GHashTable *track_table, gboolean ontrack )
{
  // . Search list for an instance of repeated name
  // . get track of this name
  // . create new name
  // . rename track & update equiv. treeview iter
  // . repeat until all different

  same_track_name_udata udata;

  GList *track_names = NULL;
  udata.has_same_track_name = FALSE;
  udata.same_track_name = NULL;

  g_hash_table_foreach ( track_table, (GHFunc) trw_layer_sorted_track_id_by_name_list, &track_names );

  // No tracks
  if ( ! track_names )
    return;

  GList *dummy_list1 = g_list_sort_with_data ( track_names, check_tracks_for_same_name, &udata );

  // Still no tracks...
  if ( ! dummy_list1 )
    return;

  while ( udata.has_same_track_name ) {

    // Find a track with the same name
    VikTrack *trk;
    if ( ontrack )
      trk = vik_trw_layer_get_track ( vtl, (gpointer) udata.same_track_name );
    else
      trk = vik_trw_layer_get_route ( vtl, (gpointer) udata.same_track_name );

    if ( ! trk ) {
      // Broken :(
      g_critical("Houston, we've had a problem.");
      vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, 
                                  _("Internal Error in vik_trw_layer_uniquify_tracks") );
      return;
    }

    // Rename it
    gchar *newname = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, udata.same_track_name );
    vik_track_set_name ( trk, newname );

    trku_udata udataU;
    udataU.trk  = trk;
    udataU.uuid = NULL;

    // Need want key of it for treeview update
    gpointer trkf = g_hash_table_find ( track_table, (GHRFunc) trw_layer_track_find_uuid, &udataU );

    if ( trkf && udataU.uuid ) {

      GtkTreeIter *it;
      if ( ontrack )
	it = g_hash_table_lookup ( vtl->tracks_iters, udataU.uuid );
      else
	it = g_hash_table_lookup ( vtl->routes_iters, udataU.uuid );

      if ( it ) {
        vik_treeview_item_set_name ( VIK_LAYER(vtl)->vt, it, newname );
        if ( ontrack )
          vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter), vtl->track_sort_order );
        else
          vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->routes_iter), vtl->track_sort_order );
      }
    }
    g_free ( newname );

    // Start trying to find same names again...
    track_names = NULL;
    g_hash_table_foreach ( track_table, (GHFunc) trw_layer_sorted_track_id_by_name_list, &track_names );
    udata.has_same_track_name = FALSE;
    GList *dummy_list2 = g_list_sort_with_data ( track_names, check_tracks_for_same_name, &udata );

    // No tracks any more - give up searching
    if ( ! dummy_list2 )
      udata.has_same_track_name = FALSE;
  }

  // Update
  vik_layers_panel_emit_update ( vlp );
}

static void trw_layer_sort_order_specified ( VikTrwLayer *vtl, guint sublayer_type, vik_layer_sort_order_t order )
{
  GtkTreeIter *iter;

  switch (sublayer_type) {
  case VIK_TRW_LAYER_SUBLAYER_TRACKS:
    iter = &(vtl->tracks_iter);
    vtl->track_sort_order = order;
    break;
  case VIK_TRW_LAYER_SUBLAYER_ROUTES:
    iter = &(vtl->routes_iter);
    vtl->track_sort_order = order;
    break;
  default: // VIK_TRW_LAYER_SUBLAYER_WAYPOINTS:
    iter = &(vtl->waypoints_iter);
    vtl->wp_sort_order = order;
    break;
  }

  vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, iter, order );
}

static void trw_layer_sort_order_a2z ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  trw_layer_sort_order_specified ( vtl, GPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_ALPHABETICAL_ASCENDING );
}

static void trw_layer_sort_order_z2a ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  trw_layer_sort_order_specified ( vtl, GPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_ALPHABETICAL_DESCENDING );
}

static void trw_layer_sort_order_timestamp_ascend ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  trw_layer_sort_order_specified ( vtl, GPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_DATE_ASCENDING );
}

static void trw_layer_sort_order_timestamp_descend ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  trw_layer_sort_order_specified ( vtl, GPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_DATE_DESCENDING );
}

static void trw_layer_sort_order_123 ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  trw_layer_sort_order_specified ( vtl, GPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_NUMBER_ASCENDING );
}

static void trw_layer_sort_order_321 ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  trw_layer_sort_order_specified ( vtl, GPOINTER_TO_INT(values[MA_SUBTYPE]), VL_SO_NUMBER_DESCENDING );
}

/**
 *
 */
static void trw_layer_delete_tracks_from_selection ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  GList *all = NULL;

  // Ensure list of track names offered is unique
  if ( trw_layer_has_same_track_names ( vtl->tracks ) ) {
    if ( a_dialog_yes_or_no_suppress ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			      _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL ) ) {
      vik_trw_layer_uniquify_tracks ( vtl, VIK_LAYERS_PANEL(values[MA_VLP]), vtl->tracks, TRUE );
    }
    else
      return;
  }

  // Sort list alphabetically for better presentation
  g_hash_table_foreach(vtl->tracks, (GHFunc) trw_layer_sorted_track_id_by_name_list, &all);

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
      // This deletes first trk it finds of that name (but uniqueness is enforced above)
      trw_layer_delete_track_by_name (vtl, l->data, vtl->tracks);
    }
    g_list_free(delete_list);
    // Reset layer timestamps in case they have now changed
    vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );

    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

/**
 *
 */
static void trw_layer_delete_routes_from_selection ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  GList *all = NULL;

  // Ensure list of track names offered is unique
  if ( trw_layer_has_same_track_names ( vtl->routes ) ) {
    if ( a_dialog_yes_or_no_suppress ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                              _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL ) ) {
      vik_trw_layer_uniquify_tracks ( vtl, VIK_LAYERS_PANEL(values[MA_VLP]), vtl->routes, FALSE );
    }
    else
      return;
  }

  // Sort list alphabetically for better presentation
  g_hash_table_foreach(vtl->routes, (GHFunc) trw_layer_sorted_track_id_by_name_list, &all);

  if ( ! all ) {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No routes found"));
    return;
  }

  // Get list of items to delete from the user
  GList *delete_list = a_dialog_select_from_list ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                   all,
                                                   TRUE,
                                                   _("Delete Selection"),
                                                   _("Select routes to delete") );
  g_list_free(all);

  // Delete requested routes
  // since specificly requested, IMHO no need for extra confirmation
  if ( delete_list ) {
    GList *l;
    for (l = delete_list; l != NULL; l = g_list_next(l)) {
      // This deletes first route it finds of that name (but uniqueness is enforced above)
      trw_layer_delete_track_by_name (vtl, l->data, vtl->routes);
    }
    g_list_free(delete_list);
    vik_layer_emit_update( VIK_LAYER(vtl) );
  }
}

typedef struct {
  gboolean    has_same_waypoint_name;
  const gchar *same_waypoint_name;
} same_waypoint_name_udata;

static gint check_waypoints_for_same_name ( gconstpointer aa, gconstpointer bb, gpointer udata )
{
  const gchar* namea = (const gchar*) aa;
  const gchar* nameb = (const gchar*) bb;

  // the test
  gint result = strcmp ( namea, nameb );

  if ( result == 0 ) {
    // Found two names the same
    same_waypoint_name_udata *user_data = udata;
    user_data->has_same_waypoint_name = TRUE;
    user_data->same_waypoint_name = namea;
  }

  // Leave ordering the same
  return 0;
}

/**
 * Find out if any waypoints have the same name in this layer
 */
gboolean trw_layer_has_same_waypoint_names ( VikTrwLayer *vtl )
{
  // Sort items by name, then compare if any next to each other are the same

  GList *waypoint_names = NULL;
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_sorted_wp_id_by_name_list, &waypoint_names );

  // No waypoints
  if ( ! waypoint_names )
    return FALSE;

  same_waypoint_name_udata udata;
  udata.has_same_waypoint_name = FALSE;

  // Use sort routine to traverse list comparing items
  // Don't care how this list ends up ordered ( doesn't actually change ) - care about the returned status
  GList *dummy_list = g_list_sort_with_data ( waypoint_names, check_waypoints_for_same_name, &udata );
  // Still no waypoints...
  if ( ! dummy_list )
    return FALSE;

  return udata.has_same_waypoint_name;
}

/**
 * Force unqiue waypoint names for this layer
 * Note the panel is a required parameter to enable the update of the names displayed
 */
static void vik_trw_layer_uniquify_waypoints ( VikTrwLayer *vtl, VikLayersPanel *vlp )
{
  // . Search list for an instance of repeated name
  // . get waypoint of this name
  // . create new name
  // . rename waypoint & update equiv. treeview iter
  // . repeat until all different

  same_waypoint_name_udata udata;

  GList *waypoint_names = NULL;
  udata.has_same_waypoint_name = FALSE;
  udata.same_waypoint_name = NULL;

  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_sorted_wp_id_by_name_list, &waypoint_names );

  // No waypoints
  if ( ! waypoint_names )
    return;

  GList *dummy_list1 = g_list_sort_with_data ( waypoint_names, check_waypoints_for_same_name, &udata );

  // Still no waypoints...
  if ( ! dummy_list1 )
    return;

  while ( udata.has_same_waypoint_name ) {

    // Find a waypoint with the same name
    VikWaypoint *waypoint = vik_trw_layer_get_waypoint ( vtl, (gpointer) udata.same_waypoint_name );

    if ( ! waypoint ) {
      // Broken :(
      g_critical("Houston, we've had a problem.");
      vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, 
                                  _("Internal Error in vik_trw_layer_uniquify_waypoints") );
      return;
    }

    // Rename it
    gchar *newname = trw_layer_new_unique_sublayer_name ( vtl, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, udata.same_waypoint_name );

    trw_layer_waypoint_rename ( vtl, waypoint, newname );

    g_free (newname);

    // Start trying to find same names again...
    waypoint_names = NULL;
    g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_sorted_wp_id_by_name_list, &waypoint_names );
    udata.has_same_waypoint_name = FALSE;
    GList *dummy_list2 = g_list_sort_with_data ( waypoint_names, check_waypoints_for_same_name, &udata );

    // No waypoints any more - give up searching
    if ( ! dummy_list2 )
      udata.has_same_waypoint_name = FALSE;
  }

  // Update
  vik_layers_panel_emit_update ( vlp );
}

/**
 *
 */
static void trw_layer_delete_waypoints_from_selection ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  GList *all = NULL;

  // Ensure list of waypoint names offered is unique
  if ( trw_layer_has_same_waypoint_names ( vtl ) ) {
    if ( a_dialog_yes_or_no_suppress ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
			      _("Multiple entries with the same name exist. This method only works with unique names. Force unique names now?"), NULL ) ) {
      vik_trw_layer_uniquify_waypoints ( vtl, VIK_LAYERS_PANEL(values[MA_VLP]) );
    }
    else
      return;
  }

  // Sort list alphabetically for better presentation
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_sorted_wp_id_by_name_list, &all);
  if ( ! all ) {
    a_dialog_error_msg (VIK_GTK_WINDOW_FROM_LAYER(vtl),	_("No waypoints found"));
    return;
  }

  all = g_list_sort(all, sort_alphabetically);

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
      // This deletes first waypoint it finds of that name (but uniqueness is enforced above)
      trw_layer_delete_waypoint_by_name (vtl, l->data);
    }
    g_list_free(delete_list);

    trw_layer_calculate_bounds_waypoints ( vtl );
    // Reset layer timestamp in case it has now changed
    vik_treeview_item_set_timestamp ( vtl->vl.vt, &vtl->vl.iter, trw_layer_get_timestamp(vtl) );
    vik_layer_emit_update( VIK_LAYER(vtl) );
  }

}

/**
 * Only deletes first copy of each duplicated waypoint
 * Thus call repeatedly to remove all duplicates
 */
static guint trw_layer_delete_duplicate_waypoints_main ( VikTrwLayer *vtl )
{
  GHashTableIter iter;
  gpointer key, value;

  guint delete_count = 0;
  guint sz = g_hash_table_size ( vtl->waypoints );

  for ( int ii = 0; ii < (sz - delete_count); ii++ ) {
    g_hash_table_iter_init ( &iter, vtl->waypoints );
    g_hash_table_iter_next ( &iter, &key, &value );
    // Progress to the nth waypoint
    for ( int jj = 0; jj < ii; jj++ ) {
      g_hash_table_iter_next ( &iter, &key, &value );
    }
    VikWaypoint *wpt1 = VIK_WAYPOINT(value);
    if ( wpt1 ) {
      // Compare against rest of the list
      gboolean done = FALSE;
      while ( !done && g_hash_table_iter_next (&iter, &key, &value) ) {
        VikWaypoint *wpt2 = VIK_WAYPOINT(value);
        // Just how many other fields is it sensible to compare? altitude, comment, etc ???
        if ( vik_coord_equalish(&(wpt1->coord), &(wpt2->coord)) &&
             !g_strcmp0(wpt1->symbol, wpt2->symbol) ) {

          GtkTreeIter *it = g_hash_table_lookup ( vtl->waypoints_iters, key );
          if ( it ) {
              delete_waypoint_low_level ( vtl, wpt2, key, it );
              // Have to exit now as hash table changed
              //  (as otherwise continuing iterating over it can go badly wrong)
              done = TRUE;
              delete_count++;
	  }
        }
      }
    }
  }
  return delete_count;
}

/**
 *
 */
static void trw_layer_delete_duplicate_waypoints ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  guint delete_count = 0;
  guint cnt = 0;
  // Continually call until nothing more deleted
  do {
    cnt = trw_layer_delete_duplicate_waypoints_main ( vtl );
    delete_count += cnt;
  } while ( cnt );

  if ( delete_count ) {
    // Inform user how much was changed
    gchar str[64];
    const gchar *tmp_str = ngettext("%ld waypoint deleted", "%ld waypoints deleted", delete_count);
    g_snprintf(str, 64, tmp_str, delete_count);
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), str );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
  else {
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No duplicates found") );
  }
}

/**
 *
 */
static gboolean waypoint_change_time_from_comment ( VikWaypoint *wp )
{
  gboolean changed = FALSE;
  // Only change waypoints without a time
  if ( isnan(wp->timestamp) ) {
    GTimeVal tv;
    // See also g_date_time_new_from_iso8601() but glib 2.56 needed
    if ( g_time_val_from_iso8601(wp->comment, &tv) ) {
      gdouble d1 = tv.tv_sec;
      gdouble d2 = (gdouble)tv.tv_usec/G_USEC_PER_SEC;
      wp->timestamp = (d1 < 0) ? d1 - d2 : d1 + d2;
      changed = TRUE;
    } else {
#ifdef HAVE_STRPTIME
      // Some old Garmins used things like "24-JUL-12 18:45:08"
      gchar *time_format = NULL;
      if ( ! a_settings_get_string("gpx_comment_time_format", &time_format) )
        time_format = g_strdup ( "%d-%B-%y %H:%M:%S" );

      struct tm tm = {0};
      if ( strptime(wp->comment, time_format, &tm) ) {
        time_t thetime = util_timegm ( &tm );
        wp->timestamp = (gdouble)thetime;
        changed = TRUE;
      }
      g_free ( time_format );
#endif
    }
  }
  return changed;
}

/**
 *
 */
static void trw_layer_waypoints_set_time_from_comment ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikWaypoint *wp;
  guint changed = 0;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    // Single Waypoint
    wp = (VikWaypoint*)g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
    if ( wp ) {
      changed = (guint)waypoint_change_time_from_comment ( wp );
      if ( changed ) {
        trw_layer_treeview_waypoint_align_time ( vtl, wp );
      }
    }
  }
  else {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init ( &iter, vtl->waypoints );
    while ( g_hash_table_iter_next (&iter, &key, &value) ) {
      wp = VIK_WAYPOINT(value);
      if ( waypoint_change_time_from_comment(wp) ) {
        trw_layer_treeview_waypoint_align_time ( vtl, wp );
        changed++;
      }
    }
  }
  if ( changed ) {
    gchar str[64];
    const gchar *tmp_str = ngettext ( "%ld waypoint adjusted", "%ld waypoints adjusted", changed );
    g_snprintf ( str, 64, tmp_str, changed );
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), str );
  }
}

/**
 *
 */
static void trw_layer_iter_visibility_toggle ( gpointer id, GtkTreeIter *it, VikTreeview *vt )
{
  vik_treeview_item_toggle_visible ( vt, it );
}

/**
 *
 */
static void trw_layer_iter_visibility ( gpointer id, GtkTreeIter *it, gpointer vis_data[2] )
{
  vik_treeview_item_set_visible ( (VikTreeview*)vis_data[0], it, GPOINTER_TO_INT (vis_data[1]) );
}

/**
 *
 */
static void trw_layer_waypoints_visibility ( gpointer id, VikWaypoint *wp, gpointer on_off )
{
  wp->visible = GPOINTER_TO_INT (on_off);
}

/**
 *
 */
static void trw_layer_waypoints_toggle_visibility ( gpointer id, VikWaypoint *wp )
{
  wp->visible = !wp->visible;
}

/**
 *
 */
static void trw_layer_waypoints_visibility_off ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gpointer vis_data[2] = { VIK_LAYER(vtl)->vt, GINT_TO_POINTER(FALSE) };
  g_hash_table_foreach ( vtl->waypoints_iters, (GHFunc) trw_layer_iter_visibility, vis_data );
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_waypoints_visibility, vis_data[1] );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_waypoints_visibility_on ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gpointer vis_data[2] = { VIK_LAYER(vtl)->vt, GINT_TO_POINTER(TRUE) };
  g_hash_table_foreach ( vtl->waypoints_iters, (GHFunc) trw_layer_iter_visibility, vis_data );
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_waypoints_visibility, vis_data[1] );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_waypoints_visibility_toggle ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  g_hash_table_foreach ( vtl->waypoints_iters, (GHFunc) trw_layer_iter_visibility_toggle, VIK_LAYER(vtl)->vt );
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) trw_layer_waypoints_toggle_visibility, NULL );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_tracks_visibility ( gpointer id, VikTrack *trk, gpointer on_off )
{
  trk->visible = GPOINTER_TO_INT (on_off);
}

/**
 *
 */
static void trw_layer_tracks_toggle_visibility ( gpointer id, VikTrack *trk )
{
  trk->visible = !trk->visible;
}

/**
 *
 */
static void trw_layer_tracks_visibility_off ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gpointer vis_data[2] = { VIK_LAYER(vtl)->vt, GINT_TO_POINTER(FALSE) };
  g_hash_table_foreach ( vtl->tracks_iters, (GHFunc) trw_layer_iter_visibility, vis_data );
  g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_tracks_visibility, vis_data[1] );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_tracks_visibility_on ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gpointer vis_data[2] = { VIK_LAYER(vtl)->vt, GINT_TO_POINTER(TRUE) };
  g_hash_table_foreach ( vtl->tracks_iters, (GHFunc) trw_layer_iter_visibility, vis_data );
  g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_tracks_visibility, vis_data[1] );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_tracks_visibility_toggle ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  g_hash_table_foreach ( vtl->tracks_iters, (GHFunc) trw_layer_iter_visibility_toggle, VIK_LAYER(vtl)->vt );
  g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_tracks_toggle_visibility, NULL );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_routes_visibility_off ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gpointer vis_data[2] = { VIK_LAYER(vtl)->vt, GINT_TO_POINTER(FALSE) };
  g_hash_table_foreach ( vtl->routes_iters, (GHFunc) trw_layer_iter_visibility, vis_data );
  g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_tracks_visibility, vis_data[1] );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_routes_visibility_on ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  gpointer vis_data[2] = { VIK_LAYER(vtl)->vt, GINT_TO_POINTER(TRUE) };
  g_hash_table_foreach ( vtl->routes_iters, (GHFunc) trw_layer_iter_visibility, vis_data );
  g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_tracks_visibility, vis_data[1] );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
static void trw_layer_routes_visibility_toggle ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  g_hash_table_foreach ( vtl->routes_iters, (GHFunc) trw_layer_iter_visibility_toggle, VIK_LAYER(vtl)->vt );
  g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_tracks_toggle_visibility, NULL );
  // Redraw
  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 * vik_trw_layer_build_waypoint_list_t:
 *
 * Helper function to construct a list of #vik_trw_waypoint_list_t
 */
GList *vik_trw_layer_build_waypoint_list_t ( VikTrwLayer *vtl, GList *waypoints )
{
  GList *waypoints_and_layers = NULL;
  // build waypoints_and_layers list
  while ( waypoints ) {
    vik_trw_waypoint_list_t *vtdl = g_malloc (sizeof(vik_trw_waypoint_list_t));
    vtdl->wpt = VIK_WAYPOINT(waypoints->data);
    vtdl->vtl = vtl;
    waypoints_and_layers = g_list_prepend ( waypoints_and_layers, vtdl );
    waypoints = g_list_next ( waypoints );
  }
  return waypoints_and_layers;
}

/**
 * trw_layer_create_waypoint_list:
 *
 * Create the latest list of waypoints with the associated layer(s)
 *  Although this will always be from a single layer here
 */
static GList* trw_layer_create_waypoint_list ( VikLayer *vl, gpointer user_data )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  GList *waypoints = g_hash_table_get_values ( vik_trw_layer_get_waypoints(vtl) );

  return vik_trw_layer_build_waypoint_list_t ( vtl, waypoints );
}

/**
 * trw_layer_analyse_close:
 *
 * Stuff to do on dialog closure
 */
static void trw_layer_analyse_close ( GtkWidget *dialog, gint resp, VikLayer* vl )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  gtk_widget_destroy ( dialog );
  vtl->tracks_analysis_dialog = NULL;
}

/**
 * vik_trw_layer_build_track_list_t:
 *
 * Helper function to construct a list of #vik_trw_and_track_t
 */
GList *vik_trw_layer_build_track_list_t ( VikTrwLayer *vtl, GList *tracks )
{
  GList *tracks_and_layers = NULL;
  // build tracks_and_layers list
  while ( tracks ) {
    vik_trw_and_track_t *vtdl = g_malloc (sizeof(vik_trw_and_track_t));
    vtdl->trk = VIK_TRACK(tracks->data);
    vtdl->vtl = vtl;
    tracks_and_layers = g_list_prepend ( tracks_and_layers, vtdl );
    tracks = g_list_next ( tracks );
  }
  return tracks_and_layers;
}

/**
 * trw_layer_create_track_list:
 *
 * Create the latest list of tracks with the associated layer(s)
 *  Although this will always be from a single layer here
 */
static GList* trw_layer_create_track_list ( VikLayer *vl, gpointer user_data )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  GList *tracks = NULL;
  if ( GPOINTER_TO_INT(user_data) == VIK_TRW_LAYER_SUBLAYER_TRACKS )
    tracks = g_hash_table_get_values ( vik_trw_layer_get_tracks(vtl) );
  else
    tracks = g_hash_table_get_values ( vik_trw_layer_get_routes(vtl) );

  return vik_trw_layer_build_track_list_t ( vtl, tracks );
}

static void trw_layer_tracks_stats ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // There can only be one!
  if ( vtl->tracks_analysis_dialog )
    return;

  vtl->tracks_analysis_dialog = vik_trw_layer_analyse_this ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                             VIK_LAYER(vtl)->name,
                                                             VIK_LAYER(vtl),
                                                             GINT_TO_POINTER(VIK_TRW_LAYER_SUBLAYER_TRACKS),
                                                             trw_layer_create_track_list,
                                                             trw_layer_analyse_close );
}

/**
 *
 */
static void trw_layer_routes_stats ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  // There can only be one!
  if ( vtl->tracks_analysis_dialog )
    return;

  vtl->tracks_analysis_dialog = vik_trw_layer_analyse_this ( VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                             VIK_LAYER(vtl)->name,
                                                             VIK_LAYER(vtl),
                                                             GINT_TO_POINTER(VIK_TRW_LAYER_SUBLAYER_ROUTES),
                                                             trw_layer_create_track_list,
                                                             trw_layer_analyse_close );
}

static void trw_layer_goto_waypoint ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikWaypoint *wp = g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
  if ( wp )
    goto_coord ( values[MA_VLP], vtl, values[MA_VVP], &(wp->coord) );
}

static void trw_layer_waypoint_gc_webpage ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikWaypoint *wp = g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
  if ( !wp )
    return;
  gchar *webpage = g_strdup_printf("http://www.geocaching.com/seek/cache_details.aspx?wp=%s", wp->name );
  open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), webpage);
  g_free ( webpage );
}

static void trw_layer_waypoint_webpage ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);
  VikWaypoint *wp = g_hash_table_lookup ( vtl->waypoints, values[MA_SUBLAYER_ID] );
  if ( !wp )
    return;
  if ( wp->url ) {
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), wp->url);
  } else if ( !strncmp(wp->comment, "http", 4) ) {
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), wp->comment);
  } else if ( !strncmp(wp->description, "http", 4) ) {
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(vtl)), wp->description);
  }
}

static const gchar* trw_layer_sublayer_rename_request ( VikTrwLayer *l, const gchar *newname, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter )
{
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
  {
    VikWaypoint *wp = g_hash_table_lookup ( l->waypoints, sublayer );

    // No actual change to the name supplied
    if ( wp->name )
      if (strcmp(newname, wp->name) == 0 )
       return NULL;

    VikWaypoint *wpf = vik_trw_layer_get_waypoint ( l, newname );

    if ( wpf ) {
      // An existing waypoint has been found with the requested name
      if ( ! a_dialog_yes_or_no_suppress ( VIK_GTK_WINDOW_FROM_LAYER(l),
           _("A waypoint with the name \"%s\" already exists. Really rename to the same name?"),
           newname ) )
        return NULL;
    }

    // Update WP name and refresh the treeview
    vik_waypoint_set_name (wp, newname);

    vik_treeview_item_set_name ( VIK_LAYER(l)->vt, iter, newname );
    vik_treeview_sort_children ( VIK_LAYER(l)->vt, &(l->waypoints_iter), l->wp_sort_order );

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp) );

    return newname;
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
  {
    VikTrack *trk = g_hash_table_lookup ( l->tracks, sublayer );

    // No actual change to the name supplied
    if ( trk->name )
      if (strcmp(newname, trk->name) == 0)
	return NULL;

    VikTrack *trkf = vik_trw_layer_get_track ( l, (gpointer) newname );

    if ( trkf ) {
      // An existing track has been found with the requested name
      if ( ! a_dialog_yes_or_no_suppress ( VIK_GTK_WINDOW_FROM_LAYER(l),
          _("A track with the name \"%s\" already exists. Really rename to the same name?"),
          newname ) )
        return NULL;
    }
    // Update track name and refresh GUI parts
    vik_track_set_name (trk, newname);

    // Update any subwindows that could be displaying this track which has changed name
    // Only one Track Edit Window
    if ( l->current_tp_track == trk && l->tpwin ) {
      vik_trw_layer_tpwin_set_track_name ( l->tpwin, newname );
    }
    // Property Dialog of the track
    vik_trw_layer_propwin_update ( trk );

    vik_treeview_item_set_name ( VIK_LAYER(l)->vt, iter, newname );
    vik_treeview_sort_children ( VIK_LAYER(l)->vt, &(l->tracks_iter), l->track_sort_order );

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp) );

    return newname;
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    VikTrack *trk = g_hash_table_lookup ( l->routes, sublayer );

    // No actual change to the name supplied
    if ( trk->name )
      if (strcmp(newname, trk->name) == 0)
        return NULL;

    VikTrack *trkf = vik_trw_layer_get_route ( l, (gpointer) newname );

    if ( trkf ) {
      // An existing track has been found with the requested name
      if ( ! a_dialog_yes_or_no_suppress ( VIK_GTK_WINDOW_FROM_LAYER(l),
          _("A route with the name \"%s\" already exists. Really rename to the same name?"),
          newname ) )
        return NULL;
    }
    // Update track name and refresh GUI parts
    vik_track_set_name (trk, newname);

    // Update any subwindows that could be displaying this track which has changed name
    // Only one Track Edit Window
    if ( l->current_tp_track == trk && l->tpwin ) {
      vik_trw_layer_tpwin_set_track_name ( l->tpwin, newname );
    }
    // Property Dialog of the track
    vik_trw_layer_propwin_update ( trk );

    vik_treeview_item_set_name ( VIK_LAYER(l)->vt, iter, newname );
    vik_treeview_sort_children ( VIK_LAYER(l)->vt, &(l->routes_iter), l->track_sort_order );

    vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vlp) );

    return newname;
  }
  return NULL;
}

static gboolean is_valid_geocache_name ( gchar *str )
{
  gint len = strlen ( str );
  return len >= 3 && len <= 7 && str[0] == 'G' && str[1] == 'C' && isalnum(str[2]) && (len < 4 || isalnum(str[3])) && (len < 5 || isalnum(str[4])) && (len < 6 || isalnum(str[5])) && (len < 7 || isalnum(str[6]));
}

#ifndef WINDOWS
static void trw_layer_track_use_with_filter ( menu_array_sublayer values )
{
  VikTrack *trk = g_hash_table_lookup ( VIK_TRW_LAYER(values[MA_VTL])->tracks, values[MA_SUBLAYER_ID] );
  a_acquire_set_filter_track ( trk );
}
#endif

#ifdef VIK_CONFIG_GOOGLE
static gboolean is_valid_google_route ( VikTrwLayer *vtl, const gpointer track_id )
{
  VikTrack *tr = g_hash_table_lookup ( vtl->routes, track_id );
  return ( tr && tr->comment && strlen(tr->comment) > 7 && !strncmp(tr->comment, "from:", 5) );
}

static void trw_layer_google_route_webpage ( menu_array_sublayer values )
{
  VikTrack *tr = g_hash_table_lookup ( VIK_TRW_LAYER(values[MA_VTL])->routes, values[MA_SUBLAYER_ID] );
  if ( tr ) {
    gchar *escaped = g_uri_escape_string ( tr->comment, NULL, TRUE );
    gchar *webpage = g_strdup_printf("http://maps.google.com/maps?f=q&hl=en&q=%s", escaped );
    open_url(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(values[MA_VTL])), webpage);
    g_free ( escaped );
    g_free ( webpage );
  }
}
#endif

/* vlp can be NULL if necessary - i.e. right-click from a tool */
/* viewpoint is now available instead */
static gboolean trw_layer_sublayer_add_menu_items ( VikTrwLayer *l, GtkMenu *menu, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter, VikViewport *vvp )
{
  static menu_array_sublayer data;
  GtkWidget *item;
  gboolean rv = FALSE;
  gboolean may_have_extensions = (l->gpx_version != GPX_V1_0);

  data[MA_VTL]         = l;
  data[MA_VLP]         = vlp;
  data[MA_SUBTYPE]     = GINT_TO_POINTER (subtype);
  data[MA_SUBLAYER_ID] = sublayer;
  data[MA_CONFIRM]     = GINT_TO_POINTER (1); // Confirm delete request
  data[MA_VVP]         = vvp;
  data[MA_TV_ITER]     = iter;
  data[MA_MISC]        = NULL; // For misc purposes - maybe track or waypoint

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT || subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    rv = TRUE;

    GtkWidget *itemprop = vu_menu_add_item ( menu, NULL, GTK_STOCK_PROPERTIES, G_CALLBACK(trw_layer_properties_item), data );

    if (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) {
      VikTrack *tr = g_hash_table_lookup ( l->tracks, sublayer );
      if (tr && tr->property_dialog)
        gtk_widget_set_sensitive(GTK_WIDGET(itemprop), FALSE );
    }
    if (subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE) {
      VikTrack *tr = g_hash_table_lookup ( l->routes, sublayer );
      if (tr && tr->property_dialog)
        gtk_widget_set_sensitive(GTK_WIDGET(itemprop), FALSE );
    }

    (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_CUT, G_CALLBACK(trw_layer_cut_item_cb), data );
    (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_COPY, G_CALLBACK(trw_layer_copy_item_cb), data );
    (void)vu_menu_add_item ( menu, NULL, GTK_STOCK_DELETE, G_CALLBACK(trw_layer_delete_item), data );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
    {
      // Always create separator as now there is always at least the transform menu option
      (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator

      // could be a right-click using the tool
      if ( vlp != NULL ) {
        (void)vu_menu_add_item ( menu, _("_Goto"), GTK_STOCK_JUMP_TO, G_CALLBACK(trw_layer_goto_waypoint), data );
      }

      VikWaypoint *wp = g_hash_table_lookup ( VIK_TRW_LAYER(l)->waypoints, sublayer );

      if ( wp && wp->name ) {
        if ( is_valid_geocache_name ( wp->name ) ) {
          (void)vu_menu_add_item ( menu, _("_Visit Geocache Webpage"), NULL, G_CALLBACK(trw_layer_waypoint_gc_webpage), data );
        }
#ifdef VIK_CONFIG_GEOTAG
        GtkWidget *itemgi = vu_menu_add_item ( menu, _("Geotag _Images..."), VIK_ICON_GLOBE, G_CALLBACK(trw_layer_geotagging_waypoint), data );
        gtk_widget_set_tooltip_text (itemgi, _("Geotag multiple images against this waypoint"));
#endif
      }

      if ( wp && wp->image )
      {
        // Set up image paramater
        data[MA_MISC] = wp->image;
        (void)vu_menu_add_item ( menu, _("_Show Picture..."), VIK_ICON_SHOW_PICTURE, G_CALLBACK(trw_layer_show_picture), data );

#ifdef VIK_CONFIG_GEOTAG
        GtkMenu *geotag_submenu = GTK_MENU(gtk_menu_new());
        GtkWidget *itemg = vu_menu_add_item ( menu, _("Update Geotag on _Image"), GTK_STOCK_REFRESH, NULL, NULL );
        gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemg), GTK_WIDGET(geotag_submenu) );

        (void)vu_menu_add_item ( geotag_submenu, _("_Update"), NULL, G_CALLBACK(trw_layer_geotagging_waypoint_mtime_update), data );
        (void)vu_menu_add_item ( geotag_submenu, _("Update and _Keep File Timestamp"), NULL, G_CALLBACK(trw_layer_geotagging_waypoint_mtime_keep), data );
#endif
      }

      if ( wp ) {
        if ( wp->url ||
             ( wp->comment && !strncmp(wp->comment, "http", 4) ) ||
             ( wp->description && !strncmp(wp->description, "http", 4) )) {
          GtkWidget *webpg = vu_menu_add_item ( menu, _("Visit _Webpage"), GTK_STOCK_NETWORK, G_CALLBACK(trw_layer_waypoint_webpage), data );
          if ( wp->url_name )
            gtk_widget_set_tooltip_text ( webpg, wp->url_name );
        }
      }
    }
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
    GtkWidget *itempaste = vu_menu_add_item ( menu, NULL, GTK_STOCK_PASTE, G_CALLBACK(trw_layer_paste_item_cb), data );
    // TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type
    if ( a_clipboard_type ( ) == VIK_CLIPBOARD_DATA_SUBLAYER )
      gtk_widget_set_sensitive ( itempaste, TRUE );
    else
      gtk_widget_set_sensitive ( itempaste, FALSE );

    (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator
  }

  if ( vlp && (subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) ) {
    rv = TRUE;
    (void)vu_menu_add_item ( menu, _("_New Waypoint..."), GTK_STOCK_NEW, G_CALLBACK(trw_layer_new_wp), data );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS ) {
    (void)vu_menu_add_item ( menu, _("_View All Waypoints"), GTK_STOCK_ZOOM_FIT, G_CALLBACK(trw_layer_auto_waypoints_view), data );
    (void)vu_menu_add_item ( menu, _("Goto _Waypoint..."), GTK_STOCK_JUMP_TO, G_CALLBACK(trw_layer_goto_wp), data );
    (void)vu_menu_add_item ( menu, _("Delete _All Waypoints"), GTK_STOCK_REMOVE, G_CALLBACK(trw_layer_delete_all_waypoints), data );
    (void)vu_menu_add_item ( menu, _("_Delete Waypoints From Selection..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_delete_waypoints_from_selection), data );
    (void)vu_menu_add_item ( menu, _("Delete Duplicate Waypoints"), GTK_STOCK_DELETE, G_CALLBACK(trw_layer_delete_duplicate_waypoints), data );

    GtkMenu *vis_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemvis = vu_menu_add_item ( menu, _("_Visibility"), VIK_ICON_CHECKBOX, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemvis), GTK_WIDGET(vis_submenu) );

    (void)vu_menu_add_item ( vis_submenu, _("_Show All Waypoints"), GTK_STOCK_APPLY, G_CALLBACK(trw_layer_waypoints_visibility_on), data );
    (void)vu_menu_add_item ( vis_submenu, _("_Hide All Waypoints"), GTK_STOCK_CLEAR, G_CALLBACK(trw_layer_waypoints_visibility_off), data );
    (void)vu_menu_add_item ( vis_submenu, _("_Toggle"), GTK_STOCK_REFRESH, G_CALLBACK(trw_layer_waypoints_visibility_toggle), data );

    (void)vu_menu_add_item ( menu, _("_List Waypoints..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_waypoint_list_dialog), data );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS ) {
    rv = TRUE;

    if ( l->current_track && !l->current_track->is_route ) {
      (void)vu_menu_add_item ( menu, _("_Finish Track"), NULL, G_CALLBACK(trw_layer_finish_track), data );
      (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator
    }

    (void)vu_menu_add_item ( menu, _("_View All Tracks"), GTK_STOCK_ZOOM_FIT, G_CALLBACK(trw_layer_auto_tracks_view), data );
    GtkWidget *itemnew = vu_menu_add_item ( menu, _("_New Track"), GTK_STOCK_NEW, G_CALLBACK(trw_layer_edit_track), data );
    // Make it available only when a new track *not* already in progress
    gtk_widget_set_sensitive ( itemnew, ! (gboolean)GPOINTER_TO_INT(l->current_track) );
    (void)vu_menu_add_item ( menu, _("Delete _All Tracks"), GTK_STOCK_REMOVE, G_CALLBACK(trw_layer_delete_all_tracks), data );
    (void)vu_menu_add_item ( menu, _("_Delete Tracks From Selection..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_delete_tracks_from_selection), data );

    GtkMenu *vis_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemvis = vu_menu_add_item ( menu, _("_Visibility"), VIK_ICON_CHECKBOX, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemvis), GTK_WIDGET(vis_submenu) );

    (void)vu_menu_add_item ( vis_submenu, _("_Show All Tracks"), GTK_STOCK_APPLY, G_CALLBACK(trw_layer_tracks_visibility_on), data );
    (void)vu_menu_add_item ( vis_submenu, _("_Hide All Tracks"), GTK_STOCK_CLEAR, G_CALLBACK(trw_layer_tracks_visibility_off), data );
    (void)vu_menu_add_item ( vis_submenu, _("_Toggle"), GTK_STOCK_REFRESH, G_CALLBACK(trw_layer_tracks_visibility_toggle), data );

    (void)vu_menu_add_item ( menu, _("_List Tracks..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_track_list_dialog_single), data );
    (void)vu_menu_add_item ( menu, _("_Statistics"), GTK_STOCK_INFO, G_CALLBACK(trw_layer_tracks_stats), data );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
    rv = TRUE;

    if ( l->current_track && l->current_track->is_route ) {
      // Reuse finish track method
      (void)vu_menu_add_item ( menu, _("_Finish Route"), NULL, G_CALLBACK(trw_layer_finish_track), data );
      (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator
    }

    (void)vu_menu_add_item ( menu, _("_View All Routes"), GTK_STOCK_ZOOM_FIT, G_CALLBACK(trw_layer_auto_routes_view), data );
    GtkWidget *itemnew = vu_menu_add_item ( menu, _("_New Route"), GTK_STOCK_NEW, G_CALLBACK(trw_layer_edit_route), data );
    // Make it available only when a new track *not* already in progress
    gtk_widget_set_sensitive ( itemnew, ! (gboolean)GPOINTER_TO_INT(l->current_track) );
    (void)vu_menu_add_item ( menu, _("Delete _All Routes"), GTK_STOCK_REMOVE, G_CALLBACK(trw_layer_delete_all_routes), data );
    (void)vu_menu_add_item ( menu, _("_Delete Routes From Selection..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_delete_routes_from_selection), data );

    GtkMenu *vis_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemvis = vu_menu_add_item ( menu, _("_Visibility"), NULL, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemvis), GTK_WIDGET(vis_submenu) );

    (void)vu_menu_add_item ( vis_submenu, _("_Show All Routes"), GTK_STOCK_APPLY, G_CALLBACK(trw_layer_routes_visibility_on), data );
    (void)vu_menu_add_item ( vis_submenu, _("_Hide All Routes"), GTK_STOCK_CLEAR, G_CALLBACK(trw_layer_routes_visibility_off), data );
    (void)vu_menu_add_item ( vis_submenu, _("_Toggle"), GTK_STOCK_REFRESH, G_CALLBACK(trw_layer_routes_visibility_toggle), data );

    (void)vu_menu_add_item ( menu, _("_List Routes..."), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_track_list_dialog_single), data );
    (void)vu_menu_add_item ( menu, _("_Statistics"), NULL, G_CALLBACK(trw_layer_routes_stats), data );
  }


  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
    GtkMenu *submenu_sort = GTK_MENU(gtk_menu_new());
    GtkWidget *itemsort = vu_menu_add_item ( menu, _("_Sort"), GTK_STOCK_REFRESH, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemsort), GTK_WIDGET(submenu_sort) );

    (void)vu_menu_add_item ( submenu_sort, _("Name _Ascending"), GTK_STOCK_SORT_ASCENDING, G_CALLBACK(trw_layer_sort_order_a2z), data );
    (void)vu_menu_add_item ( submenu_sort, _("Name _Descending"), GTK_STOCK_SORT_DESCENDING, G_CALLBACK(trw_layer_sort_order_z2a), data );
    (void)vu_menu_add_item ( submenu_sort, _("Date Ascending"), GTK_STOCK_SORT_ASCENDING, G_CALLBACK(trw_layer_sort_order_timestamp_ascend), data );
    (void)vu_menu_add_item ( submenu_sort, _("Date Descending"), GTK_STOCK_SORT_DESCENDING, G_CALLBACK(trw_layer_sort_order_timestamp_descend), data );
    if ( subtype != VIK_TRW_LAYER_SUBLAYER_WAYPOINTS ) {
      (void)vu_menu_add_item ( submenu_sort, _("Number Ascending"), GTK_STOCK_SORT_ASCENDING, G_CALLBACK(trw_layer_sort_order_123), data );
      (void)vu_menu_add_item ( submenu_sort, _("Number Descending"), GTK_STOCK_SORT_DESCENDING, G_CALLBACK(trw_layer_sort_order_321), data );
    }
  }

  GtkMenu *upload_submenu = GTK_MENU(gtk_menu_new());

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
    (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator
    if ( l->current_track ) {
      (void)vu_menu_add_item ( menu, (!l->current_track->is_route) ? _("_Finish Track") : _("_Finish Route"),
                               NULL, G_CALLBACK(trw_layer_finish_track), data );
      (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator
    }

    (void)vu_menu_add_item ( menu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("_View Track") : _("_View Route"),
                             GTK_STOCK_ZOOM_FIT, G_CALLBACK(trw_layer_auto_track_view), data );

    (void)vu_menu_add_item ( menu, _("_Statistics"), GTK_STOCK_INFO, G_CALLBACK(trw_layer_track_statistics), data );

    GtkMenu *goto_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemgoto = vu_menu_add_item ( menu, _("_Goto"), GTK_STOCK_JUMP_TO, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemgoto), GTK_WIDGET(goto_submenu) );

    (void)vu_menu_add_item ( goto_submenu, _("_Startpoint"), GTK_STOCK_GOTO_FIRST, G_CALLBACK(trw_layer_goto_track_startpoint), data );
    (void)vu_menu_add_item ( goto_submenu, _("\"_Center\""), GTK_STOCK_JUMP_TO, G_CALLBACK(trw_layer_goto_track_center), data );
    (void)vu_menu_add_item ( goto_submenu, _("_Endpoint"), GTK_STOCK_GOTO_LAST, G_CALLBACK(trw_layer_goto_track_endpoint), data );
    (void)vu_menu_add_item ( goto_submenu, _("_Highest Altitude"), GTK_STOCK_GOTO_TOP, G_CALLBACK(trw_layer_goto_track_max_alt), data );
    (void)vu_menu_add_item ( goto_submenu, _("_Lowest Altitude"), GTK_STOCK_GOTO_BOTTOM, G_CALLBACK(trw_layer_goto_track_min_alt), data );

    // Routes don't have speeds or dates or similar
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      (void)vu_menu_add_item ( goto_submenu, _("_Maximum Speed"), GTK_STOCK_MEDIA_FORWARD, G_CALLBACK(trw_layer_goto_track_max_speed), data );
      if ( vlp )
        (void)vu_menu_add_item ( goto_submenu, _("_Date"), GTK_STOCK_JUMP_TO, G_CALLBACK(trw_layer_goto_track_date), data );

      // Only display if potentially have them
      if ( may_have_extensions ) {
        (void)vu_menu_add_item ( goto_submenu, _("Maximum _Heart Rate"), NULL, G_CALLBACK(trw_layer_goto_track_max_hr), data );
        (void)vu_menu_add_item ( goto_submenu, _("Maximum Cadence"), NULL, G_CALLBACK(trw_layer_goto_track_max_cad), data );
        (void)vu_menu_add_item ( goto_submenu, _("Maximum Power"), NULL, G_CALLBACK(trw_layer_goto_track_max_power), data );
        (void)vu_menu_add_item ( goto_submenu, _("Minimum Temperature"), NULL, G_CALLBACK(trw_layer_goto_track_min_temp), data );
        (void)vu_menu_add_item ( goto_submenu, _("Maximum Temperature"), NULL, G_CALLBACK(trw_layer_goto_track_max_temp), data );
      }
    }

    (void)vu_menu_add_item ( goto_submenu, _("_Previous point"), GTK_STOCK_GO_BACK, G_CALLBACK(trw_layer_goto_track_prev_point), data );
    (void)vu_menu_add_item ( goto_submenu, _("_Next point"), GTK_STOCK_GO_FORWARD, G_CALLBACK(trw_layer_goto_track_next_point), data );

    GtkMenu *combine_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemcomb = vu_menu_add_item ( menu, _("Co_mbine"), GTK_STOCK_CONNECT, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemcomb), GTK_WIDGET(combine_submenu) );

    // Routes don't have times or segments...
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      (void)vu_menu_add_item ( combine_submenu, _("_Merge By Time..."), NULL, G_CALLBACK(trw_layer_merge_by_timestamp), data );
      (void)vu_menu_add_item ( combine_submenu, _("Merge _Segments"), NULL, G_CALLBACK(trw_layer_merge_by_segment), data );
      (void)vu_menu_add_item ( combine_submenu, _("Merge _With Other Tracks..."), NULL, G_CALLBACK(trw_layer_merge_with_other), data );
    }

    (void)vu_menu_add_item ( combine_submenu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("_Append Track...") : _("_Append Route..."),
                             NULL, G_CALLBACK(trw_layer_append_track), data );
    (void)vu_menu_add_item ( combine_submenu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("Append _Route...") : _("Append _Track..."),
                             NULL, G_CALLBACK(trw_layer_append_other), data );

    GtkMenu *split_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemsplit = vu_menu_add_item ( menu, _("_Split"), GTK_STOCK_DISCONNECT, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemsplit), GTK_WIDGET(split_submenu) );

    // Routes don't have times or segments...
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      (void)vu_menu_add_item ( split_submenu, _("_Split By Time..."), NULL, G_CALLBACK(trw_layer_split_by_timestamp), data );
      // ATM always enable this entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy
      (void)vu_menu_add_item ( split_submenu, _("Split Se_gments"), NULL, G_CALLBACK(trw_layer_split_segments), data );
    }

    (void)vu_menu_add_item ( split_submenu, _("Split By _Number of Points..."), NULL, G_CALLBACK(trw_layer_split_by_n_points), data );
    GtkWidget *itemsnp = vu_menu_add_item ( split_submenu, _("Split at _Trackpoint"), NULL, G_CALLBACK(trw_layer_split_at_trackpoint), data );
    // Make it available only when a trackpoint is selected.
    gtk_widget_set_sensitive ( itemsnp, (gboolean)GPOINTER_TO_INT(l->current_tpl) );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      GtkWidget *itemsns = vu_menu_add_item ( split_submenu, _("_Create Segment at Trackpoint"), NULL, G_CALLBACK(trw_layer_split_create_segments), data );
      // Make it available only when a trackpoint is selected.
      gtk_widget_set_sensitive ( itemsns, (gboolean)GPOINTER_TO_INT(l->current_tpl) );
    }

    GtkMenu *insert_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *iteminsert = vu_menu_add_item ( menu, _("_Insert Points"), GTK_STOCK_ADD, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(iteminsert), GTK_WIDGET(insert_submenu) );

    GtkWidget *itemib = vu_menu_add_item ( insert_submenu, _("Insert Point _Before Selected Point"), NULL, G_CALLBACK(trw_layer_insert_point_before), data );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( itemib, (gboolean)GPOINTER_TO_INT(l->current_tpl) );
    GtkWidget *itemia = vu_menu_add_item ( insert_submenu, _("Insert Point _After Selected Point"), NULL, G_CALLBACK(trw_layer_insert_point_after), data );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( itemia, (gboolean)GPOINTER_TO_INT(l->current_tpl) );

    GtkMenu *delete_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemdelete = vu_menu_add_item ( menu, _("Delete Poi_nts"), GTK_STOCK_DELETE, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemdelete), GTK_WIDGET(delete_submenu) );

    GtkWidget *itemdsp = vu_menu_add_item ( delete_submenu, _("Delete _Selected Point"), GTK_STOCK_DELETE, G_CALLBACK(trw_layer_delete_point_selected), data );
    // Make it available only when a trackpoint is selected.
    gtk_widget_set_sensitive ( itemdsp, (gboolean)GPOINTER_TO_INT(l->current_tpl) );
    (void)vu_menu_add_item ( delete_submenu, _("Delete Points With The Same _Position"), NULL, G_CALLBACK(trw_layer_delete_points_same_position), data);
    (void)vu_menu_add_item ( delete_submenu, _("Delete Points With The Same _Time"), NULL, G_CALLBACK(trw_layer_delete_points_same_time), data );

    GtkMenu *transform_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemtransform = vu_menu_add_item ( menu, _("_Transform"), GTK_STOCK_CONVERT, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemtransform), GTK_WIDGET(transform_submenu) );

    GtkMenu *dem_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemdem = vu_menu_add_item ( transform_submenu, _("_Apply DEM Data"), "vik-icon-DEM Download", NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemdem), GTK_WIDGET(dem_submenu) );

    GtkWidget *itemow = vu_menu_add_item ( dem_submenu, _("_Overwrite"), NULL, G_CALLBACK(trw_layer_apply_dem_data_all), data );
    gtk_widget_set_tooltip_text ( itemow, _("Overwrite any existing elevation values with DEM values") );

    GtkWidget *itemke = vu_menu_add_item ( dem_submenu, _("_Keep Existing"), NULL, G_CALLBACK(trw_layer_apply_dem_data_only_missing), data );
    gtk_widget_set_tooltip_text ( itemke, _("Keep existing elevation values, only attempt for missing values") );

    GtkMenu *smooth_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemsmooth = vu_menu_add_item ( transform_submenu, _("_Smooth Missing Elevation Data"), NULL, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemsmooth), GTK_WIDGET(smooth_submenu) );

    GtkWidget *itemintp = vu_menu_add_item ( smooth_submenu, _("_Interpolated"), NULL, G_CALLBACK(trw_layer_missing_elevation_data_interp), data );
    gtk_widget_set_tooltip_text ( itemintp, _("Interpolate between known elevation values to derive values for the missing elevations") );

    GtkWidget *itemflat = vu_menu_add_item ( smooth_submenu, _("_Flat"), NULL, G_CALLBACK(trw_layer_missing_elevation_data_flat), data );
    gtk_widget_set_tooltip_text ( itemflat, _("Set unknown elevation values to the last known value") );

    (void)vu_menu_add_item ( transform_submenu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("C_onvert to a Route") : _("C_onvert to a Track"),
                             GTK_STOCK_CONVERT, G_CALLBACK(trw_layer_convert_track_route), data );

    (void)vu_menu_add_item ( transform_submenu, _("Convert to Waypoints"),
                             GTK_STOCK_CONVERT, G_CALLBACK(trw_layer_convert_to_waypoints), data );

    // Routes don't have timestamps - so these are only available for tracks
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      GtkWidget *itemat = vu_menu_add_item ( transform_submenu, _("_Anonymize Times"), NULL, G_CALLBACK(trw_layer_anonymize_times), data );
      gtk_widget_set_tooltip_text ( itemat, _("Shift timestamps to a relative offset from 1901-01-01") );

      GtkWidget *itemit = vu_menu_add_item ( transform_submenu, _("_Interpolate Times"), NULL, G_CALLBACK(trw_layer_interpolate_times), data );
      gtk_widget_set_tooltip_text ( itemit, _("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed") );
    } 
    GtkWidget *itemro = vu_menu_add_item ( transform_submenu, _("_Rotate..."), NULL, G_CALLBACK(trw_layer_rotate), data );
    gtk_widget_set_tooltip_text ( itemro, _("Shift trackpoints to move the first points to the end") );

    (void)vu_menu_add_item ( transform_submenu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("_Reverse Track") : _("_Reverse Route"),
                             GTK_STOCK_GO_BACK, G_CALLBACK(trw_layer_reverse), data );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      (void)vu_menu_add_item ( transform_submenu, _("Refine Route..."), GTK_STOCK_FIND, G_CALLBACK(trw_layer_route_refine), data );
    } else
      (void)vu_menu_add_item ( transform_submenu, _("Rename..."), GTK_STOCK_EDIT, G_CALLBACK(trw_layer_track_rename), data );

    /* ATM This function is only available via the layers panel, due to the method in finding out the maps in use */
    if ( vlp ) {
      (void)vu_menu_add_item ( menu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("Down_load Maps Along Track...") : _("Down_load Maps Along Route..."),
                               "vik-icon-Maps Download", G_CALLBACK(trw_layer_download_map_along_track_cb), data );
    }

    (void)vu_menu_add_item ( menu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("_Export Track as GPX...") : _("_Export Route as GPX..."),
                             GTK_STOCK_HARDDISK, G_CALLBACK(trw_layer_export_gpx_track), data );
    (void)vu_menu_add_item ( menu, (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) ? _("E_xtend Track End") : _("E_xtend Route End"),
                             GTK_STOCK_ADD, G_CALLBACK(trw_layer_extend_track_end), data );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      (void)vu_menu_add_item ( menu, _("Extend _Using Route Finder"), "vik-icon-Route Finder", G_CALLBACK(trw_layer_extend_track_end_route_finder), data );
    }
  }

  // ATM can't upload a single waypoint but can do waypoints to a GPS
  if ( subtype != VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    //GtkMenu *upload_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemupload = vu_menu_add_item ( menu, _("_Upload"), GTK_STOCK_GO_UP, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemupload), GTK_WIDGET(upload_submenu) );
    (void)vu_menu_add_item ( upload_submenu, _("_Upload to GPS..."), GTK_STOCK_GO_FORWARD, G_CALLBACK(trw_layer_gps_upload_any), data );
  }

  GtkMenu *external_submenu = create_external_submenu ( menu );

  // These are only made available if a suitable program is installed
  if ( (have_astro_program || have_diary_program) &&
       (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) ) {

    if ( have_diary_program ) {
      GtkWidget *item = vu_menu_add_item ( external_submenu, _("_Diary"), GTK_STOCK_SPELL_CHECK, G_CALLBACK(trw_layer_diary), data );
      gtk_widget_set_tooltip_text ( item, _("Open diary program at this date") );
    }

    if ( have_astro_program ) {
      GtkWidget *item = vu_menu_add_item ( external_submenu, _("_Astronomy"), NULL, G_CALLBACK(trw_layer_astro), data );
      gtk_widget_set_tooltip_text (item, _("Open astronomy program at this date and location") );
    }
  }

  if ( l->current_tpl || l->current_wp ) {
    // For the selected point
    VikCoord *vc;
    if ( l->current_tpl )
      vc = &(VIK_TRACKPOINT(l->current_tpl->data)->coord);
    else
      vc = &(l->current_wp->coord);
    vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), GTK_MENU (external_submenu), vc );
  }
  else {
    // Otherwise for the selected sublayer
    // TODO: Should use selected items centre - rather than implicitly using the current viewport
    vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), GTK_MENU (external_submenu), NULL );
  }

#ifdef VIK_CONFIG_GOOGLE
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE && is_valid_google_route ( l, sublayer ) ) {
    (void)vu_menu_add_item ( menu, _("_View Google Directions"), GTK_STOCK_NETWORK, G_CALLBACK(trw_layer_google_route_webpage), data );
  }
#endif

  // Some things aren't usable with routes
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
#ifdef VIK_CONFIG_OPENSTREETMAP
    data[MA_MISC] = g_hash_table_lookup ( l->tracks, sublayer);
    (void)vu_menu_add_item ( upload_submenu, _("Upload to _OSM..."), GTK_STOCK_GO_UP, G_CALLBACK(trw_layer_osm_traces_upload_track_cb), data );
#endif

    // Currently filter with functions all use shellcommands and thus don't work in Windows
#ifndef WINDOWS
    if ( a_babel_available() )
      (void)vu_menu_add_item ( menu, _("Use with _Filter"), GTK_STOCK_INDEX, G_CALLBACK(trw_layer_track_use_with_filter), data );
#endif

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
    (void)vu_menu_add_item ( menu, _("Geotag _Images..."), VIK_ICON_GLOBE, G_CALLBACK(trw_layer_geotagging_track), data );
#endif
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
    // Only show on viewport popmenu when a trackpoint is selected
    if ( ! vlp && l->current_tpl ) {
      (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator
      (void)vu_menu_add_item ( menu, _("_Edit Trackpoint"), GTK_STOCK_PROPERTIES, G_CALLBACK(trw_layer_edit_trackpoint), data );
    }
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    GtkMenu *transform_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemtransform = vu_menu_add_item ( menu, _("_Transform"), GTK_STOCK_CONVERT, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemtransform), GTK_WIDGET(transform_submenu) );

    (void)vu_menu_add_item ( transform_submenu, _("Set Time from Comment"), NULL, G_CALLBACK(trw_layer_waypoints_set_time_from_comment), data );

    GtkMenu *dem_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemdem = vu_menu_add_item ( transform_submenu, _("_Apply DEM Data"), "vik-icon-DEM Download", NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemdem), GTK_WIDGET(dem_submenu) );

    GtkWidget *itemow = vu_menu_add_item ( dem_submenu, _("_Overwrite"), NULL, G_CALLBACK(trw_layer_apply_dem_data_wpt_all), data );
    gtk_widget_set_tooltip_text ( itemow, _("Overwrite any existing elevation values with DEM values") );

    GtkWidget *itemke = vu_menu_add_item ( dem_submenu, _("_Keep Existing"), NULL, G_CALLBACK(trw_layer_apply_dem_data_wpt_only_missing), data );
    gtk_widget_set_tooltip_text ( itemke, _("Keep existing elevation values, only attempt for missing values") );

    // Can't make a track from just one waypoint!
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS )
      (void)vu_menu_add_item ( transform_submenu, _("Convert to Track"), GTK_STOCK_CONVERT, G_CALLBACK(trw_layer_convert_to_track), data );
  }

  gtk_widget_show_all ( GTK_WIDGET(menu) );

  return rv;
}

/**
 *
 */
static void trw_layer_insert_tp_beside_current_tp ( VikTrwLayer *vtl, gboolean before, gboolean is_route )
{
  // sanity check
  if (!vtl->current_tpl)
    return;

  VikTrackpoint *tp_current = VIK_TRACKPOINT(vtl->current_tpl->data);
  VikTrackpoint *tp_other = NULL;

  if ( before ) {
    if (!vtl->current_tpl->prev)
      return;
    tp_other = VIK_TRACKPOINT(vtl->current_tpl->prev->data);
  } else {
    if (!vtl->current_tpl->next)
      return;
    tp_other = VIK_TRACKPOINT(vtl->current_tpl->next->data);
  }

  // Use current and other trackpoints to form a new track point which is inserted into the tracklist
  if ( tp_other ) {

    VikTrackpoint *tp_new = vik_trackpoint_new();
    struct LatLon ll_current, ll_other;
    vik_coord_to_latlon ( &tp_current->coord, &ll_current );
    vik_coord_to_latlon ( &tp_other->coord, &ll_other );

    /* main positional interpolation */
    struct LatLon ll_new = { (ll_current.lat+ll_other.lat)/2, (ll_current.lon+ll_other.lon)/2 };
    vik_coord_load_from_latlon ( &(tp_new->coord), vtl->coord_mode, &ll_new );

    /* Now other properties that can be interpolated */
    tp_new->altitude = (tp_current->altitude + tp_other->altitude) / 2;

    if (!isnan(tp_current->timestamp) && !isnan(tp_other->timestamp)) {
      /* Note here the division is applied to each part, then added
	 This was to avoid potential overflow issues with potential 32bit times, but it's now always a 64bit double so it doesn't matter anymore */
      tp_new->timestamp = (tp_current->timestamp/2) + (tp_other->timestamp/2);
    }

    if (tp_current->speed != NAN && tp_other->speed != NAN)
      tp_new->speed = (tp_current->speed + tp_other->speed)/2;

    /* TODO - improve interpolation of course, as it may not be correct.
       if courses in degrees are 350 + 020, the mid course more likely to be 005 (not 185)
       [similar applies if value is in radians] */
    if (tp_current->course != NAN && tp_other->course != NAN)
      tp_new->course = (tp_current->course + tp_other->course)/2;

    /* DOP / sat values remain at defaults as they do not seem applicable to a dreamt up point */

    // Insert new point into the appropriate trackpoint list, either before or after the current trackpoint as directed   
    VikTrack *trk = vtl->current_tp_track;
    if ( !trk )
      return;

    gint index =  g_list_index ( trk->trackpoints, tp_current );
    if ( index > -1 ) {
      if ( !before )
        index = index + 1;
      // NB no recalculation of bounds since it is inserted between points
      trk->trackpoints = g_list_insert ( trk->trackpoints, tp_new, index );
    }
  }

  vik_layer_emit_update ( VIK_LAYER(vtl) );
}

/**
 *
 */
void vik_trw_layer_insert_tp_beside_current_tp ( VikTrwLayer *vtl, gboolean before )
{
  g_return_if_fail ( IS_VIK_TRW_LAYER(vtl) );

  if ( vtl->current_tp_track ) {
    trw_layer_insert_tp_beside_current_tp ( vtl, before, vtl->current_tp_track->is_route );
  }
}

static void trw_layer_cancel_current_tp ( VikTrwLayer *vtl, gboolean destroy )
{
  if ( vtl->tpwin )
  {
    if ( destroy)
    {
      vik_trw_layer_tpwin_destroy ( vtl->tpwin );
      vtl->tpwin = NULL;
    }
    else
      vik_trw_layer_tpwin_set_empty ( vtl->tpwin );
  }
  if ( vtl->current_tpl )
  {
    vtl->current_tpl = NULL;
    vtl->current_tp_track = NULL;
    vik_layer_emit_update(VIK_LAYER(vtl));
  }
}

static void my_tpwin_set_tp ( VikTrwLayer *vtl )
{
  VikTrack *trk = vtl->current_tp_track;
  VikCoord vc;
  // Notional center of a track is simply an average of the bounding box extremities
  struct LatLon center = { (trk->bbox.north+trk->bbox.south)/2, (trk->bbox.east+trk->bbox.west)/2 };
  vik_coord_load_from_latlon ( &vc, vtl->coord_mode, &center );
  vik_trw_layer_tpwin_set_tp ( vtl->tpwin, vtl->current_tpl, trk->name, vtl->current_tp_track->is_route );
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
    trw_layer_split_at_selected_trackpoint ( vtl, vtl->current_tp_track->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK );
    my_tpwin_set_tp ( vtl );
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_DELETE )
  {
    VikTrack *tr = vtl->current_tp_track;
    if ( !tr )
      return;

    trw_layer_trackpoint_selected_delete ( vtl, tr );

    if ( vtl->current_tpl )
      // Reset dialog with the available adjacent trackpoint
      my_tpwin_set_tp ( vtl );

    vik_layer_emit_update(VIK_LAYER(vtl));
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_FORWARD && vtl->current_tpl->next )
  {
    if ( vtl->current_tp_track ) {
      vtl->current_tpl = vtl->current_tpl->next;
      my_tpwin_set_tp ( vtl );
    }
    vik_layer_emit_update(VIK_LAYER(vtl)); /* TODO longone: either move or only update if tp is inside drawing window */
    trw_layer_graph_draw_tp ( vtl );
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_BACK && vtl->current_tpl->prev )
  {
    if ( vtl->current_tp_track ) {
      vtl->current_tpl = vtl->current_tpl->prev;
      my_tpwin_set_tp ( vtl );
    }
    vik_layer_emit_update(VIK_LAYER(vtl));
    trw_layer_graph_draw_tp ( vtl );
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_INSERT && vtl->current_tpl->next )
  {
    if ( vtl->current_tp_track ) {
      trw_layer_insert_tp_beside_current_tp ( vtl, FALSE, vtl->current_tp_track->is_route );
    }
  }
  else if ( response == VIK_TRW_LAYER_TPWIN_DATA_CHANGED )
    vik_layer_emit_update(VIK_LAYER(vtl));
}

/**
 * trw_layer_dialog_shift:
 * @vertical: The reposition strategy. If Vertical moves dialog vertically, otherwise moves it horizontally
 *
 * Try to reposition a dialog if it's over the specified coord
 *  so to not obscure the item of interest
 */
void trw_layer_dialog_shift ( VikTrwLayer *vtl, GtkWindow *dialog, VikCoord *coord, gboolean vertical )
{
  GtkWindow *parent = VIK_GTK_WINDOW_FROM_LAYER(vtl); //i.e. the main window

  // Attempt force dialog to be shown so we can find out where it is more reliably...
  while ( gtk_events_pending() )
    gtk_main_iteration ();

  // get parent window position & size
  gint win_pos_x, win_pos_y;
  gtk_window_get_position ( parent, &win_pos_x, &win_pos_y );

  gint win_size_x, win_size_y;
  gtk_window_get_size ( parent, &win_size_x, &win_size_y );

  // get own dialog size
  gint dia_size_x, dia_size_y;
  gtk_window_get_size ( dialog, &dia_size_x, &dia_size_y );

  // get own dialog position
  gint dia_pos_x, dia_pos_y;
  gtk_window_get_position ( dialog, &dia_pos_x, &dia_pos_y );

  // Dialog not 'realized'/positioned - so can't really do any repositioning logic
  if ( dia_pos_x > 2 && dia_pos_y > 2 ) {

    VikViewport *vvp = vik_window_viewport ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );

    gint vp_xx, vp_yy; // In viewport pixels
    vik_viewport_coord_to_screen ( vvp, coord, &vp_xx, &vp_yy );

    // Work out the 'bounding box' in pixel terms of the dialog and only move it when over the position

    gint dest_x = 0;
    gint dest_y = 0;
    if ( gtk_widget_translate_coordinates ( GTK_WIDGET(vvp), GTK_WIDGET(parent), 0, 0, &dest_x, &dest_y ) ) {

      // Transform Viewport pixels into absolute pixels
      gint tmp_xx = vp_xx + dest_x + win_pos_x - 10;
      gint tmp_yy = vp_yy + dest_y + win_pos_y - 10;

      // Is dialog over the point (to within an  ^^ edge value)
      if ( (tmp_xx > dia_pos_x) && (tmp_xx < (dia_pos_x + dia_size_x)) &&
           (tmp_yy > dia_pos_y) && (tmp_yy < (dia_pos_y + dia_size_y)) ) {

        if ( vertical ) {
	  // Shift up<->down
          gint hh = vik_viewport_get_height ( vvp );

          // Consider the difference in viewport to the full window
          gint offset_y = dest_y;
          // Add difference between dialog and window sizes
          offset_y += win_pos_y + (hh/2 - dia_size_y)/2;

          if ( vp_yy > hh/2 ) {
            // Point in bottom half, move window to top half
            gtk_window_move ( dialog, dia_pos_x, offset_y );
          }
          else {
            // Point in top half, move dialog down
            gtk_window_move ( dialog, dia_pos_x, hh/2 + offset_y );
          }
	}
	else {
	  // Shift left<->right
          gint ww = vik_viewport_get_width ( vvp );

          // Consider the difference in viewport to the full window
          gint offset_x = dest_x;
          // Add difference between dialog and window sizes
          offset_x += win_pos_x + (ww/2 - dia_size_x)/2;

          if ( vp_xx > ww/2 ) {
            // Point on right, move window to left
            gtk_window_move ( dialog, offset_x, dia_pos_y );
          }
          else {
            // Point on left, move right
            gtk_window_move ( dialog, ww/2 + offset_x, dia_pos_y );
          }
	}
      }
    }
  }
}

/**
 * Should only be otherwise used by viktrwlayer_propwin
 **/
void trw_layer_tpwin_init ( VikTrwLayer *vtl )
{
  if ( ! vtl->tpwin )
  {
    vtl->tpwin = vik_trw_layer_tpwin_new ( VIK_GTK_WINDOW_FROM_LAYER(vtl) );
    g_signal_connect_swapped ( GTK_DIALOG(vtl->tpwin), "response", G_CALLBACK(trw_layer_tpwin_response), vtl );
    /* connect signals -- DELETE SIGNAL VERY IMPORTANT TO SET TO NULL */
    g_signal_connect_swapped ( vtl->tpwin, "delete-event", G_CALLBACK(trw_layer_cancel_current_tp), vtl );

    gtk_widget_show_all ( GTK_WIDGET(vtl->tpwin) );

    if ( vtl->current_tpl ) {
      // get tp pixel position
      VikTrackpoint *tp = VIK_TRACKPOINT(vtl->current_tpl->data);

      // Shift up<->down to try not to obscure the trackpoint.
      trw_layer_dialog_shift ( vtl, GTK_WINDOW(vtl->tpwin), &(tp->coord), TRUE );
    }
  }

  if ( vtl->current_tpl )
    if ( vtl->current_tp_track )
      my_tpwin_set_tp ( vtl );
  /* set layer name and TP data */
}

gboolean trw_layer_tpwin_is_shown ( VikTrwLayer *vtl )
{
  return (vtl->tpwin != NULL);
}

/***************************************************************************
 ** Tool code
 ***************************************************************************/

/*** Utility data structures and functions ****/

typedef struct {
  gint x, y;
  gint closest_x, closest_y;
  guint size;
  gboolean draw_images;
  gboolean draw_symbols;
  gpointer closest_wp_id;
  VikWaypoint *closest_wp;
  VikViewport *vvp;
} WPSearchParams;

typedef struct {
  gint x, y;
  gint closest_x, closest_y;
  guint size;
  gpointer closest_track_id;
  VikTrackpoint *closest_tp;
  VikViewport *vvp;
  GList *closest_tpl;
  LatLonBBox bbox;
} TPSearchParams;

static void waypoint_search_closest_tp ( gpointer id, VikWaypoint *wp, WPSearchParams *params )
{
  gint x, y;
  if ( !wp->visible )
    return;

  vik_viewport_coord_to_screen ( params->vvp, &(wp->coord), &x, &y );

  // If waypoint has an image then use the image size to select
  if ( params->draw_images && wp->image ) {
    gint slackx, slacky;
    slackx = wp->image_width / 2;
    slacky = wp->image_height / 2;

    if (    x <= params->x + slackx && x >= params->x - slackx
         && y <= params->y + slacky && y >= params->y - slacky ) {
      params->closest_wp_id = id;
      params->closest_wp = wp;
      params->closest_x = x;
      params->closest_y = y;
    }
  }
  else if ( params->draw_symbols && wp->symbol && wp->symbol_pixbuf ) {
    if ( abs(x-params->x) <= gdk_pixbuf_get_width(wp->symbol_pixbuf)/2 && abs(y-params->y) <= gdk_pixbuf_get_height(wp->symbol_pixbuf)/2 &&
         ((!params->closest_wp) ||        /* was the old waypoint we already found closer than this one? */
	     abs(x - params->x)+abs(y - params->y) < abs(x - params->closest_x)+abs(y - params->closest_y)) )
    {
      params->closest_wp_id = id;
      params->closest_wp = wp;
      params->closest_x = x;
      params->closest_y = y;
    }
  }
  else if ( abs (x - params->x) <= params->size && abs (y - params->y) <= params->size &&
	    ((!params->closest_wp) ||        /* was the old waypoint we already found closer than this one? */
	     abs(x - params->x)+abs(y - params->y) < abs(x - params->closest_x)+abs(y - params->closest_y)))
    {
      params->closest_wp_id = id;
      params->closest_wp = wp;
      params->closest_x = x;
      params->closest_y = y;
    }
}

static void track_search_closest_tp ( gpointer id, VikTrack *t, TPSearchParams *params )
{
  GList *tpl = t->trackpoints;
  VikTrackpoint *tp;

  if ( !t->visible )
    return;

  if ( ! BBOX_INTERSECT ( t->bbox, params->bbox ) )
    return;

  while (tpl)
  {
    gint x, y;
    tp = VIK_TRACKPOINT(tpl->data);

    vik_viewport_coord_to_screen ( params->vvp, &(tp->coord), &x, &y );
 
    if ( abs (x - params->x) <= params->size && abs (y - params->y) <= params->size &&
        ((!params->closest_tp) ||        /* was the old trackpoint we already found closer than this one? */
          abs(x - params->x)+abs(y - params->y) < abs(x - params->closest_x)+abs(y - params->closest_y)))
    {
      params->closest_track_id = id;
      params->closest_tp = tp;
      params->closest_tpl = tpl;
      params->closest_x = x;
      params->closest_y = y;
    }
    tpl = tpl->next;
  }
}

// ATM: Leave this as 'Track' only.
//  Not overly bothered about having a snap to route trackpoint capability
static VikTrackpoint *closest_tp_in_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y )
{
  TPSearchParams params;
  params.x = x;
  params.y = y;
  params.size = MAX(5, vtl->drawpoints_size*2);
  params.vvp = vvp;
  params.closest_track_id = NULL;
  params.closest_tp = NULL;
  params.bbox = vik_viewport_get_bbox ( params.vvp );
  g_hash_table_foreach ( vtl->tracks, (GHFunc) track_search_closest_tp, &params);
  return params.closest_tp;
}

static VikWaypoint *closest_wp_in_interval ( VikTrwLayer *vtl, VikViewport *vvp, gint x, gint y )
{
  WPSearchParams params;
  params.x = x;
  params.y = y;
  params.size = MAX(5, vtl->wp_size*2);
  params.vvp = vvp;
  params.draw_images = vtl->drawimages;
  params.draw_symbols = vtl->wp_draw_symbols;
  params.closest_wp = NULL;
  params.closest_wp_id = NULL;
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) waypoint_search_closest_tp, &params);
  return params.closest_wp;
}


// Some forward declarations
static void marker_begin_move ( tool_ed_t *t, gint x, gint y );
static void marker_moveto ( tool_ed_t *t, gint x, gint y );
static void marker_end_move ( tool_ed_t *t );
//

static gboolean trw_layer_select_move ( VikTrwLayer *vtl, GdkEventMotion *event, VikViewport *vvp, tool_ed_t* t )
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
      VikTrackpoint *tp = closest_tp_in_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    // snap to WP
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_interval ( vtl, vvp, event->x, event->y );
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
    // Prevent accidental (small) shifts when specific movement has not been requested
    //  (as the click release has occurred within the click object detection area)
    if ( !t->moving )
      return FALSE;

    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    // snap to TP
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    // snap to WP
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_interval ( vtl, vvp, event->x, event->y );
      if ( wp )
        new_coord = wp->coord;
    }

    marker_end_move ( t );

    // Determine if working on a waypoint or a trackpoint
    if ( t->is_waypoint ) {
      // Update waypoint position
      vtl->current_wp->coord = new_coord;
      (void)vik_waypoint_apply_dem_data ( vtl->current_wp, FALSE );
      trw_layer_calculate_bounds_waypoints ( vtl );
      // Reset waypoint pointer
      vtl->current_wp    = NULL;
      vtl->current_wp_id = NULL;
    }
    else {
      if ( vtl->current_tpl ) {
        VIK_TRACKPOINT(vtl->current_tpl->data)->coord = new_coord;
        (void)vik_trackpoint_apply_dem_data ( VIK_TRACKPOINT(vtl->current_tpl->data) );

        if ( vtl->current_tp_track )
          vik_track_calculate_bounds ( vtl->current_tp_track );

        if ( vtl->tpwin )
          if ( vtl->current_tp_track )
            my_tpwin_set_tp ( vtl );
        // NB don't reset the selected trackpoint, thus ensuring it's still in the tpwin
      }
    }

    // Selected item may have moved, so an update() not just a redraw()
    vik_layer_emit_update ( VIK_LAYER(vtl) );
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

  if ( !vtl->tracks_visible && !vtl->waypoints_visible && !vtl->routes_visible )
    return FALSE;

  LatLonBBox bbox = vik_viewport_get_bbox ( vvp );

  // Go for waypoints first as these often will be near a track, but it's likely the wp is wanted rather then the track

  if ( vtl->waypoints_visible && BBOX_INTERSECT (vtl->waypoints_bbox, bbox ) ) {
    WPSearchParams wp_params;
    wp_params.size = MAX(5, vtl->wp_size*2);
    wp_params.vvp = vvp;
    wp_params.x = event->x;
    wp_params.y = event->y;
    wp_params.draw_images = vtl->drawimages;
    wp_params.draw_symbols = vtl->wp_draw_symbols;
    wp_params.closest_wp_id = NULL;
    wp_params.closest_wp = NULL;

    g_hash_table_foreach ( vtl->waypoints, (GHFunc) waypoint_search_closest_tp, &wp_params);

    if ( wp_params.closest_wp )  {

      // Select
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->waypoints_iters, wp_params.closest_wp_id ), TRUE );

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

      vtl->current_wp =    wp_params.closest_wp;
      vtl->current_wp_id = wp_params.closest_wp_id;

      if ( event->type == GDK_2BUTTON_PRESS ) {
        if ( vtl->current_wp->image ) {
          menu_array_sublayer values;
          values[MA_VTL] = vtl;
          values[MA_MISC] = vtl->current_wp->image;
          trw_layer_show_picture ( values );
        }
      }

      // Change waypoint in dialog
      if ( vtl->wpwin ) {
        vtl->wpwin_wpt = vtl->current_wp;
        vtl->wpwin = vik_trw_layer_wpwin_show ( VIK_GTK_WINDOW_FROM_LAYER(vtl), vtl->wpwin, vtl->wpwin_wpt->name, vtl, vtl->wpwin_wpt, vtl->coord_mode, FALSE );
      }

      // Selection change only (no change to the layer)
      vik_layer_redraw ( VIK_LAYER(vtl) );
      return TRUE;
    }
  }

  // Used for both track and route lists
  TPSearchParams tp_params;
  tp_params.size = MAX(5, vtl->drawpoints_size*2);
  tp_params.vvp = vvp;
  tp_params.x = event->x;
  tp_params.y = event->y;
  tp_params.closest_track_id = NULL;
  tp_params.closest_tp = NULL;
  tp_params.closest_tpl = NULL;
  tp_params.bbox = bbox;

  if (vtl->tracks_visible) {
    g_hash_table_foreach ( vtl->tracks, (GHFunc) track_search_closest_tp, &tp_params);

    if ( tp_params.closest_tp )  {

      // Always select + highlight the track
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->tracks_iters, tp_params.closest_track_id ), TRUE );

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
      vtl->current_tp_track = g_hash_table_lookup ( vtl->tracks, tp_params.closest_track_id );

      set_statusbar_msg_info_trkpt ( vtl, tp_params.closest_tp );

      if ( vtl->tpwin )
        my_tpwin_set_tp ( vtl );

      // Selection change only (no change to the layer)
      vik_layer_redraw ( VIK_LAYER(vtl) );
      trw_layer_graph_draw_tp ( vtl );
      return TRUE;
    }
  }

  // Try again for routes
  if (vtl->routes_visible) {
    g_hash_table_foreach ( vtl->routes, (GHFunc) track_search_closest_tp, &tp_params);

    if ( tp_params.closest_tp )  {

      // Always select + highlight the track
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->routes_iters, tp_params.closest_track_id ), TRUE );

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
      vtl->current_tp_track = g_hash_table_lookup ( vtl->routes, tp_params.closest_track_id );

      set_statusbar_msg_info_trkpt ( vtl, tp_params.closest_tp );

      if ( vtl->tpwin )
        my_tpwin_set_tp ( vtl );

      // Selection change only (no change to the layer)
      vik_layer_redraw ( VIK_LAYER(vtl) );
      trw_layer_graph_draw_tp ( vtl );
      return TRUE;
    }
  }

  /* these aren't the droids you're looking for */
  vtl->current_wp    = NULL;
  vtl->current_wp_id = NULL;
  trw_layer_cancel_current_tp ( vtl, FALSE );

  // Blank info
  vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))), VIK_STATUSBAR_INFO, "" );

  return FALSE;
}

static gboolean trw_layer_show_selected_viewport_menu ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;

  if ( !vtl->tracks_visible && !vtl->waypoints_visible && !vtl->routes_visible )
    return FALSE;

  /* Post menu for the currently selected item */

  /* See if a track is selected */
  VikTrack *track = (VikTrack*)vik_window_get_selected_track ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) );
  if ( track && track->visible ) {

    if ( track->name ) {

      if ( vtl->track_right_click_menu )
        g_object_ref_sink ( G_OBJECT(vtl->track_right_click_menu) );

      vtl->track_right_click_menu = GTK_MENU ( gtk_menu_new () );
      
      trku_udata udataU;
      udataU.trk  = track;
      udataU.uuid = NULL;

      gpointer trkf;
      if ( track->is_route )
        trkf = g_hash_table_find ( vtl->routes, (GHRFunc) trw_layer_track_find_uuid, &udataU );
      else
        trkf = g_hash_table_find ( vtl->tracks, (GHRFunc) trw_layer_track_find_uuid, &udataU );

      if ( trkf && udataU.uuid ) {

        GtkTreeIter *iter;
        if ( track->is_route )
          iter = g_hash_table_lookup ( vtl->routes_iters, udataU.uuid );
        else
          iter = g_hash_table_lookup ( vtl->tracks_iters, udataU.uuid );

        trw_layer_sublayer_add_menu_items ( vtl,
                                            vtl->track_right_click_menu,
                                            NULL,
                                            track->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK,
                                            udataU.uuid,
                                            iter,
                                            vvp );
      }
      // Using '0' is more reliable for activating submenu items than using 'event->button' anyway.
      gtk_menu_popup ( vtl->track_right_click_menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time() );

      return TRUE;
    }
  }

  /* See if a waypoint is selected */
  VikWaypoint *waypoint = (VikWaypoint*)vik_window_get_selected_waypoint ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl) );
  if ( waypoint && waypoint->visible ) {
    if ( waypoint->name ) {

      if ( vtl->wp_right_click_menu )
        g_object_ref_sink ( G_OBJECT(vtl->wp_right_click_menu) );

      vtl->wp_right_click_menu = GTK_MENU ( gtk_menu_new () );

      wpu_udata udata;
      udata.wp   = waypoint;
      udata.uuid = NULL;

      gpointer wpf = g_hash_table_find ( vtl->waypoints, (GHRFunc) trw_layer_waypoint_find_uuid, (gpointer) &udata );

      if ( wpf && udata.uuid ) {
        GtkTreeIter *iter = g_hash_table_lookup ( vtl->waypoints_iters, udata.uuid );

        trw_layer_sublayer_add_menu_items ( vtl,
                                            vtl->wp_right_click_menu,
                                            NULL,
                                            VIK_TRW_LAYER_SUBLAYER_WAYPOINT,
                                            udata.uuid,
                                            iter,
                                            vvp );
      }
      // Using '0' is more reliable for activating submenu items than using 'event->button' anyway.
      gtk_menu_popup ( vtl->wp_right_click_menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time() );

      return TRUE;
    }
  }

  return FALSE;
}

#if !GTK_CHECK_VERSION (3,0,0)
/* background drawing hook, to be passed the viewport */
static gboolean tool_sync_done = TRUE;

static gboolean tool_sync(gpointer data)
{
  VikViewport *vvp = data;
  vik_viewport_sync(vvp, NULL);
  tool_sync_done = TRUE;
  return FALSE;
}
#endif

static void marker_begin_move ( tool_ed_t *t, gint x, gint y )
{
  t->holding = TRUE;
  gdk_color_parse ( "black", &t->color );
#if GTK_CHECK_VERSION (3,0,0)
  t->gc = vik_viewport_surface_tool_create ( t->vvp );
  if ( t->gc ) {
    // Equivalent to GDK_INVERT to only draw the new bit each time
    cairo_set_operator ( t->gc, CAIRO_OPERATOR_DEST_ATOP );
    ui_cr_set_color ( t->gc, "#000000" );
    cairo_set_line_width ( t->gc, 2*vik_viewport_get_scale(t->vvp) );
  }
#else
  t->gc = vik_viewport_new_gc (t->vvp, "black", 2*vik_viewport_get_scale(t->vvp));
  gdk_gc_set_function ( t->gc, GDK_INVERT );
#endif
  vik_viewport_sync(t->vvp, t->gc);
  t->oldx = x;
  t->oldy = y;
  t->moving = FALSE;
}

static void marker_moveto ( tool_ed_t *t, gint x, gint y )
{
  VikViewport *vvp =  t->vvp;
  // Even if the waypoint marker is not a square (e.g. a circle),
  // for simplicity still draw the new virtual location as a square
  //  using the same code for the trackpoint move drawing
  const guint8 rsize = t->is_waypoint ?
    VIK_TRW_LAYER(t->vtl)->wp_size :
    VIK_TRW_LAYER(t->vtl)->drawpoints_size*2;
  vik_viewport_draw_rectangle ( vvp, t->gc, FALSE, t->oldx-rsize, t->oldy-rsize, rsize*2, rsize*2, &t->color );
  vik_viewport_draw_rectangle ( vvp, t->gc, FALSE, x-rsize, y-rsize, rsize*2, rsize*2, &t->color );
  t->oldx = x;
  t->oldy = y;
  t->moving = TRUE;

#if GTK_CHECK_VERSION (3,0,0)
  gtk_widget_queue_draw ( GTK_WIDGET(vvp) );
#else
  if (tool_sync_done) {
    (void)g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, tool_sync, vvp, NULL);
    tool_sync_done = FALSE;
  }
#endif
}

static void marker_end_move ( tool_ed_t *t )
{
#if GTK_CHECK_VERSION (3,0,0)
  tool_edit_remove_image ( t );
#else
  ui_gc_unref ( t->gc );
#endif
  t->holding = FALSE;
  t->moving = FALSE;
}

/*** Edit waypoint ****/

static VikLayerToolFuncStatus tool_edit_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  WPSearchParams params;
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;
  t->vtl = vtl;
  t->is_waypoint = TRUE;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;

  if ( t->holding ) {
    return VIK_LAYER_TOOL_ACK;
  }

  if ( !vtl->vl.visible || !vtl->waypoints_visible )
    return VIK_LAYER_TOOL_IGNORED;

  if ( vtl->current_wp && vtl->current_wp->visible )
  {
    /* first check if current WP is within area (other may be 'closer', but we want to move the current) */
    gint x, y;
    vik_viewport_coord_to_screen ( vvp, &(vtl->current_wp->coord), &x, &y );

    if ( abs(x - (int)round(event->x)) <= (vtl->wp_size*2) &&
         abs(y - (int)round(event->y)) <= (vtl->wp_size*2) )
    {
      if ( event->button == 3 )
        vtl->waypoint_rightclick = TRUE; /* remember that we're clicking; other layers will ignore release signal */
      else {
	marker_begin_move(t, event->x, event->y);
      }
      return VIK_LAYER_TOOL_ACK;
    }
  }

  params.size = MAX(5, vtl->wp_size*2);
  params.vvp = vvp;
  params.x = event->x;
  params.y = event->y;
  params.draw_images = vtl->drawimages;
  params.draw_symbols = vtl->wp_draw_symbols;
  params.closest_wp_id = NULL;
  params.closest_wp = NULL;
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) waypoint_search_closest_tp, &params);
  if ( vtl->current_wp && (vtl->current_wp == params.closest_wp) )
  {
    if ( event->button == 3 )
      vtl->waypoint_rightclick = TRUE; /* remember that we're clicking; other layers will ignore release signal */
    else
      marker_begin_move(t, event->x, event->y);
    return VIK_LAYER_TOOL_IGNORED;
  }
  else if ( params.closest_wp )
  {
    if ( event->button == 3 )
      vtl->waypoint_rightclick = TRUE; /* remember that we're clicking; other layers will ignore release signal */
    else
      vtl->waypoint_rightclick = FALSE;

    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->waypoints_iters, params.closest_wp_id ), TRUE );

    vtl->current_wp = params.closest_wp;
    vtl->current_wp_id = params.closest_wp_id;

    /* could make it so don't update if old WP is off screen and new is null but oh well */
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return VIK_LAYER_TOOL_ACK;
  }

  vtl->current_wp = NULL;
  vtl->current_wp_id = NULL;
  vtl->waypoint_rightclick = FALSE;
  vik_layer_emit_update ( VIK_LAYER(vtl) );
  return VIK_LAYER_TOOL_IGNORED;
}

static VikLayerToolFuncStatus tool_edit_waypoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;

  if ( t->holding ) {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    /* snap to WP */
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_interval ( vtl, vvp, event->x, event->y );
      if ( wp && wp != vtl->current_wp )
        new_coord = wp->coord;
    }
    
    { 
      gint x, y;
      vik_viewport_coord_to_screen ( vvp, &new_coord, &x, &y );

      marker_moveto ( t, x, y );
    } 
    return VIK_LAYER_TOOL_ACK;
  }
  return VIK_LAYER_TOOL_IGNORED;
}

static VikLayerToolFuncStatus tool_edit_waypoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;
  
  if ( t->holding && event->button == 1 )
  {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_interval ( vtl, vvp, event->x, event->y );
      if ( tp )
        new_coord = tp->coord;
    }

    /* snap to WP */
    if ( event->state & GDK_SHIFT_MASK )
    {
      VikWaypoint *wp = closest_wp_in_interval ( vtl, vvp, event->x, event->y );
      if ( wp && wp != vtl->current_wp )
        new_coord = wp->coord;
    }

    marker_end_move ( t );

    vtl->current_wp->coord = new_coord;

    trw_layer_calculate_bounds_waypoints ( vtl );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return VIK_LAYER_TOOL_ACK;
  }
  /* PUT IN RIGHT PLACE!!! */
  if ( event->button == 3 && vtl->waypoint_rightclick )
  {
    if ( vtl->wp_right_click_menu )
      g_object_ref_sink ( G_OBJECT(vtl->wp_right_click_menu) );
    if ( vtl->current_wp ) {
      vtl->wp_right_click_menu = GTK_MENU ( gtk_menu_new () );
      trw_layer_sublayer_add_menu_items ( vtl, vtl->wp_right_click_menu, NULL, VIK_TRW_LAYER_SUBLAYER_WAYPOINT, vtl->current_wp_id, g_hash_table_lookup ( vtl->waypoints_iters, vtl->current_wp_id ), vvp );
      // Using '0' is more reliable for activating submenu items than using 'event->button'.
      // Possibly https://bugzilla.gnome.org/show_bug.cgi?id=695488
      gtk_menu_popup ( vtl->wp_right_click_menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time() );
    }
    vtl->waypoint_rightclick = FALSE;
  }
  return VIK_LAYER_TOOL_IGNORED;
}

/*** Edit track or route (lots of common functionality) ****/

#if !GTK_CHECK_VERSION (3,0,0)
typedef struct {
  VikTrwLayer *vtl;
  GdkDrawable *drawable;
  GdkGC *gc;
  GdkPixmap *pixmap;
} draw_sync_t;

/*
 * Draw specified pixmap
 */
static gboolean draw_sync ( gpointer data )
{
  draw_sync_t *ds = (draw_sync_t*) data;
  // Sometimes don't want to draw
  //  normally because another update has taken precedent such as panning the display
  //   which means this pixmap is no longer valid
  if ( ds->vtl->draw_sync_do ) {
    gdk_draw_drawable (ds->drawable,
                       ds->gc,
                       ds->pixmap,
                       0, 0, 0, 0, -1, -1);
    ds->vtl->draw_sync_done = TRUE;
  }
  g_free ( ds );
  return FALSE;
}
#endif

/**
 * Draw a specific trackpoint in its own seperate gc
 * ATM This is intended to be called from the embedded graph
 */
void vik_trw_layer_trackpoint_draw ( VikTrwLayer *vtl, VikViewport *vvp, VikTrack *trk, VikTrackpoint *tpt )
{
  if ( !trk || !tpt ) {
    vik_viewport_surface_tool_destroy ( vvp );

    if ( a_vik_get_auto_trackpoint_select() )
      // Full update as selected trackpoint has probably changed
      vik_layer_emit_update ( VIK_LAYER(vtl) );
    else
      // Basic refresh to clear any previous 'highlighted' trackpoint drawing
      vik_layer_redraw ( VIK_LAYER(vtl) );
    return;
  }

  // NB using g_list_find() is not particularly efficient (to go from a VikTrackpoint* to the GList* entry)
  //  as it potentially searching the entire list, and this function can be called a lot.
  // Hence another reason for the config open

  // Don't change the current selected/edit trackpoint if
  //  the Trackpoint Edit dialog is open
  //  or if configured not to change that trackpoint
  if ( !vtl->tpwin &&
       a_vik_get_auto_trackpoint_select() ) {
    trw_layer_select_trackpoint ( vtl, trk, tpt, FALSE );
  }

  // Workout the colour (NB ignoring 'by speed' mode as that requires more information)
  const gchar *color;
  if ( vtl->drawmode == DRAWMODE_BY_TRACK )
    color = gdk_color_to_string ( &(trk->color) );
  else
    color = gdk_color_to_string ( &(vtl->track_color) );
  if ( vik_viewport_get_draw_highlight(vvp) )
    color = vik_viewport_get_highlight_color ( vvp );

  gint xd, yd;
  vik_viewport_coord_to_screen ( vvp, &(tpt->coord), &xd, &yd );
  gint tp_size = vtl->drawpoints_size * 2;

#if GTK_CHECK_VERSION (3,0,0)
  // Draw on existing tool surface if available
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));
  VikToolInterface *vti = (VikToolInterface*)vik_window_get_active_tool_interface ( vw );
  cairo_t *cr = NULL;
  if ( vti )
    if ( vti->create == (VikToolConstructorFunc)tool_edit_create ) {
      tool_ed_t *te = (tool_ed_t*)vik_window_get_active_tool_data ( vw );
      if ( te && te->gc ) {
        cr = te->gc;
        tool_edit_remove_image ( te );
      }
    }
  // Otherwise create surface to draw on
  if ( !cr )
    cr = vik_viewport_surface_tool_create ( vvp );
  if ( cr ) {
    ui_cr_set_color ( cr, color );
    ui_cr_draw_rectangle ( cr, TRUE, xd-tp_size, yd-tp_size, 2*tp_size, 2*tp_size );
    cairo_stroke ( cr );
    gtk_widget_queue_draw ( GTK_WIDGET(vvp) );
  }
#else
  static GdkPixmap *pixmap = NULL;

  gint w2, h2;
  // Need to check in case window has been resized
  gint w1 = vik_viewport_get_width ( vvp );
  gint h1 = vik_viewport_get_height ( vvp );

  if ( !pixmap ) {
    pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
  }
  gdk_drawable_get_size ( pixmap, &w2, &h2 );
  if ( w1 != w2 || h1 != h2 ) {
    g_object_unref ( G_OBJECT(pixmap) );
    pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
  }

  GdkGC *tp_gc = vik_viewport_new_gc ( vvp, color, 1 );
  gdk_draw_drawable ( pixmap,
                      tp_gc,
                      vik_viewport_get_pixmap(vvp),
                      0, 0, 0, 0, -1, -1 );
  gdk_draw_rectangle ( pixmap, tp_gc, TRUE, xd-tp_size, yd-tp_size, 2*tp_size, 2*tp_size );
  g_object_unref ( G_OBJECT(tp_gc) );

  draw_sync_t *passalong;
  passalong = g_new0 ( draw_sync_t, 1 ); // freed by draw_sync()
  passalong->vtl = vtl;
  passalong->pixmap = pixmap;
  passalong->drawable = gtk_widget_get_window ( GTK_WIDGET(vvp) );
  passalong->gc = vtl->track_graph_point_gc;

  // draw pixmap when we have time to
  (void)g_idle_add_full ( G_PRIORITY_HIGH_IDLE + 10, draw_sync, passalong, NULL );
  vtl->draw_sync_done = FALSE;
#endif
}

static gchar* distance_string (gdouble distance)
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
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
      g_sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
    } else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
      g_sprintf(str, "%d yards", (int)(distance*1.0936133));
    } else {
      g_sprintf(str, "%d NM", (int)VIK_METERS_TO_NAUTICAL_MILES(distance));
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
static void statusbar_write (gdouble distance, gdouble elev_gain, gdouble elev_loss, gdouble last_step, gdouble angle, VikTrwLayer *vtl )
{
  // Only show elevation data when track has some elevation properties
  gchar str_gain_loss[64];
  str_gain_loss[0] = '\0';
  gchar str_last_step[64];
  str_last_step[0] = '\0';
  gchar *str_total = distance_string (distance);
  
  if ( (elev_gain > 0.1) || (elev_loss > 0.1) ) {
    if ( a_vik_get_units_height () == VIK_UNITS_HEIGHT_METRES )
      g_sprintf(str_gain_loss, _(" - Gain %dm:Loss %dm"), (int)elev_gain, (int)elev_loss);
    else
      g_sprintf(str_gain_loss, _(" - Gain %dft:Loss %dft"), (int)VIK_METERS_TO_FEET(elev_gain), (int)VIK_METERS_TO_FEET(elev_loss));
  }
  
  if ( last_step > 0 ) {
      gchar *tmp = distance_string (last_step);
      g_sprintf(str_last_step, _(" - Bearing %3.1f° - Step %s"), RAD2DEG(angle), tmp);
      g_free ( tmp );
  }
  
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));

  // Write with full gain/loss information
  gchar *msg = g_strdup_printf ( _("Total %s%s%s"), str_total, str_last_step, str_gain_loss );
  vik_statusbar_set_message ( vik_window_get_statusbar (vw), VIK_STATUSBAR_INFO, msg );
  g_free ( msg );
  g_free ( str_total );
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

  statusbar_write (distance, elev_gain, elev_loss, 0, 0, vtl);
}

// select a track point
static gboolean tool_select_tp ( VikTrwLayer *vtl, TPSearchParams *params, gboolean search_tracks, gboolean search_routes )
{
  if ( vtl->tracks_visible && search_tracks )
    g_hash_table_foreach ( vtl->tracks, (GHFunc) track_search_closest_tp, params);

  if ( params->closest_tp )
  {
    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->tracks_iters, params->closest_track_id ), TRUE );
    vtl->current_tpl = params->closest_tpl;
    vtl->current_tp_track = g_hash_table_lookup ( vtl->tracks, params->closest_track_id );
    set_statusbar_msg_info_trkpt ( vtl, params->closest_tp );
    // Selection change only (no change to the layer)
    vik_layer_redraw ( VIK_LAYER(vtl) );
    trw_layer_graph_draw_tp ( vtl );
    return TRUE;
  }

  if ( vtl->routes_visible && search_routes )
    g_hash_table_foreach ( vtl->routes, (GHFunc) track_search_closest_tp, params);

  if ( params->closest_tp )
  {
    vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, g_hash_table_lookup ( vtl->routes_iters, params->closest_track_id ), TRUE );
    vtl->current_tpl = params->closest_tpl;
    vtl->current_tp_track = g_hash_table_lookup ( vtl->routes, params->closest_track_id );
    set_statusbar_msg_info_trkpt ( vtl, params->closest_tp );
    // Selection change only (no change to the layer)
    vik_layer_redraw ( VIK_LAYER(vtl) );
    trw_layer_graph_draw_tp ( vtl );
    return TRUE;
  }

  return FALSE;
}

// Select the given track in the tree view
static void tool_select_track ( VikTrwLayer *vtl, VikTrack *trk )
{
  trku_udata udata;
  udata.trk  = trk;
  udata.uuid = NULL;

  if ( trk->is_route )
  {
    gpointer trkf = g_hash_table_find ( vtl->routes, (GHRFunc) trw_layer_track_find_uuid, &udata );
    if ( trkf && udata.uuid ) {
      GtkTreeIter *it = g_hash_table_lookup ( vtl->routes_iters, udata.uuid );
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, it, TRUE );
    }
  }
  else
  {
    gpointer trkf = g_hash_table_find ( vtl->tracks, (GHRFunc) trw_layer_track_find_uuid, &udata );
    if ( trkf && udata.uuid ) {
      GtkTreeIter *it = g_hash_table_lookup ( vtl->tracks_iters, udata.uuid );
      vik_treeview_select_iter ( VIK_LAYER(vtl)->vt, it, TRUE );
    }
  }
}

// Add route to target to selected route, return TRUE on success
static gboolean tool_plot_route ( VikTrwLayer *vtl, VikCoord *target )
{
  // make sure we have a route with at least one point to extend
  if ( ! vtl->current_track  || ! vtl->current_track->is_route || ! vik_track_get_tp_first ( vtl->current_track ) )
    return FALSE;
  
  struct LatLon start, end;

  VikTrackpoint *tp_start = vik_track_get_tp_last ( vtl->current_track );
  vik_coord_to_latlon ( &(tp_start->coord), &start );
  vik_coord_to_latlon ( target, &end );

  vtl->route_finder_append = TRUE;  // merge tracks.

  // update UI to let user know what's going on
  VikStatusbar *sb = vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)));
  VikRoutingEngine *engine = vik_routing_default_engine ( );
  if ( ! engine ) {
      vik_statusbar_set_message ( sb, VIK_STATUSBAR_INFO, "Cannot plan route without a default routing engine." );
      return FALSE;
  }
  gchar *msg = g_strdup_printf ( _("Querying %s for route between (%.3f, %.3f) and (%.3f, %.3f)."),
                                 vik_routing_engine_get_label ( engine ),
                                 start.lat, start.lon, end.lat, end.lon );
  vik_statusbar_set_message ( sb, VIK_STATUSBAR_INFO, msg );
  g_free ( msg );
  vik_window_set_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );


  /* Give GTK a change to display the new status bar before querying the web */
  while ( gtk_events_pending ( ) )
    gtk_main_iteration ( );

  gboolean find_status = vik_routing_default_find ( vtl, start, end );

  if ( find_status && vtl->current_track ) {
    gulong chgd = vik_track_apply_dem_data ( vtl->current_track, TRUE );
    g_debug ( "%s %ld points changed for DEM", __FUNCTION__, chgd );
  }

  /* Update UI to say we're done */
  vik_window_clear_busy_cursor ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)) );
  msg = ( find_status ) ? g_strdup_printf ( _("%s returned route between (%.3f, %.3f) and (%.3f, %.3f)."),
                          vik_routing_engine_get_label ( engine ),
                          start.lat, start.lon, end.lat, end.lon )
                        : g_strdup_printf ( _("Error getting route from %s."),
                                            vik_routing_engine_get_label ( engine ) );
  vik_statusbar_set_message ( sb, VIK_STATUSBAR_INFO, msg );
  g_free ( msg );

  return find_status;
}

static gboolean tool_plot_route_pending ( VikTrwLayer *vtl )
{
  // Extra protection in case vtl removed
  if ( IS_VIK_TRW_LAYER(vtl) ) {
    gboolean do_update = FALSE;
    do_update = tool_plot_route ( vtl, &vtl->route_finder_request_coord ) ;
    if ( vtl->route_finder_end ) {
      vtl->current_track = NULL;
      do_update = TRUE;
    }
    vtl->route_finder_end = FALSE;
    vtl->route_finder_timer_id = 0;
    if ( do_update )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
  return FALSE;
}


static VikLayerToolFuncStatus tool_edit_track_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data )
{
  tool_ed_t *te = data;
  VikViewport *vvp = te->vvp;
  /* if we haven't sync'ed yet, we don't have time to do more. */
  if ( vtl->draw_sync_done && vtl->current_track && vtl->current_track->trackpoints ) {
    VikTrackpoint *last_tpt = vik_track_get_tp_last(vtl->current_track);

    /* Find out actual distance of current track */
    gdouble distance = vik_track_get_length (vtl->current_track);

    // Now add distance to where the pointer is //
    VikCoord coord;
    struct LatLon ll;
    vik_viewport_screen_to_coord ( vvp, (gint) event->x, (gint) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );
    gdouble last_step = vik_coord_diff( &coord, &(last_tpt->coord));
    distance = distance + last_step;

    int w1, h1, w2, h2;
    // Need to check in case window has been resized
    w1 = vik_viewport_get_width(vvp);
    h1 = vik_viewport_get_height(vvp);

#if GTK_CHECK_VERSION (3,0,0)
    if ( te->gc ) {
      cairo_surface_t *surface = vik_viewport_surface_tool_get ( te->vvp );
      if ( surface ) {
        w2 = cairo_image_surface_get_width ( surface );
        h2 = cairo_image_surface_get_height ( surface );
        if ( w1 != w2 || h1 != h2 )
          tool_edit_remove_image ( te );
      }
    }
    if ( !te->gc )
      te->gc = vik_viewport_surface_tool_create ( te->vvp );
    else
      ui_cr_clear ( te->gc );

    ui_cr_set_color ( te->gc, "red" );
    cairo_set_line_width ( te->gc, 2*vik_viewport_get_scale(te->vvp) );
    ui_cr_set_dash ( te->gc );

    gint x1, y1;
    vik_viewport_coord_to_screen ( vvp, &(last_tpt->coord), &x1, &y1 );
    ui_cr_draw_line ( te->gc, x1, y1, event->x, event->y );
    cairo_stroke ( te->gc );

    //
    // Display of the distance 'tooltip' during track creation is controlled by a preference
    //
    if ( a_vik_get_create_track_tooltip() ) {

      gchar *str = distance_string (distance);

      PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(vvp), NULL);
      pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(vvp))->font_desc);
      pango_layout_set_text (pl, str, -1);
      gint wd, hd;
      pango_layout_get_pixel_size ( pl, &wd, &hd );

      gint xd,yd;
      // offset from cursor a bit depending on font size
      xd = event->x + 10*vik_viewport_get_scale(vvp);
      yd = event->y - hd;

      ui_cr_label_with_bg ( te->gc, xd, yd, wd, hd, pl );

      g_object_unref ( G_OBJECT ( pl ) );
      g_free (str);
    }

    gtk_widget_queue_draw ( GTK_WIDGET(vvp) );
#else
    static GdkPixmap *pixmap = NULL;
    if (!pixmap) {
      pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
    }
    gdk_drawable_get_size (pixmap, &w2, &h2);
    if (w1 != w2 || h1 != h2) {
      g_object_unref ( G_OBJECT ( pixmap ) );
      pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
    }

    // Reset to background
    gdk_draw_drawable (pixmap,
                       vtl->current_track_newpoint_gc,
                       vik_viewport_get_pixmap(vvp),
                       0, 0, 0, 0, -1, -1);

    draw_sync_t *passalong;
    gint x1, y1;

    vik_viewport_coord_to_screen ( vvp, &(last_tpt->coord), &x1, &y1 );

    // FOR SCREEN OVERLAYS WE MUST DRAW INTO THIS PIXMAP (when using the reset method)
    //  otherwise using vik_viewport_draw_* functions puts the data into the base pixmap,
    //  thus when we come to reset to the background it would include what we have already drawn!!
    gdk_draw_line ( pixmap,
                    vtl->current_track_newpoint_gc,
                    x1, y1, event->x, event->y );
    // Using this reset method is more reliable than trying to undraw previous efforts via the GDK_INVERT method

    //
    // Display of the distance 'tooltip' during track creation is controlled by a preference
    //
    if ( a_vik_get_create_track_tooltip() ) {

      gchar *str = distance_string (distance);

      PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(vvp), NULL);
      pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(vvp))->font_desc);
      pango_layout_set_text (pl, str, -1);
      gint wd, hd;
      pango_layout_get_pixel_size ( pl, &wd, &hd );

      gint xd,yd;
      // offset from cursor a bit depending on font size
      xd = event->x + 10;
      yd = event->y - hd;

      // Create a background block to make the text easier to read over the background map
      GdkGC *background_block_gc = vik_viewport_new_gc ( vvp, "#cccccc", 1);
      gdk_draw_rectangle (pixmap, background_block_gc, TRUE, xd-2, yd-2, wd+4, hd+2);
      gdk_draw_layout (pixmap, vtl->current_track_newpoint_gc, xd, yd, pl);

      g_object_unref ( G_OBJECT ( pl ) );
      g_object_unref ( G_OBJECT ( background_block_gc ) );
      g_free (str);
    }

    passalong = g_new0(draw_sync_t,1); // freed by draw_sync()
    passalong->vtl = vtl;
    passalong->pixmap = pixmap;
    passalong->drawable = gtk_widget_get_window(GTK_WIDGET(vvp));
    passalong->gc = vtl->current_track_newpoint_gc;

    // draw pixmap when we have time to
    (void)g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, draw_sync, passalong, NULL);
    vtl->draw_sync_done = FALSE;
#endif

    // Get elevation data
    gdouble elev_gain, elev_loss;
    vik_track_get_total_elevation_gain ( vtl->current_track, &elev_gain, &elev_loss);

    // Adjust elevation data (if available) for the current pointer position
    gdouble elev_new;
    elev_new = (gdouble) a_dems_get_elev_by_coord ( &coord, VIK_DEM_INTERPOL_BEST );
    if ( elev_new != VIK_DEM_INVALID_ELEVATION ) {
      if ( !isnan(last_tpt->altitude) ) {
	// Adjust elevation of last track point
	if ( elev_new > last_tpt->altitude )
	  // Going up
	  elev_gain += elev_new - last_tpt->altitude;
	else
	  // Going down
	  elev_loss += last_tpt->altitude - elev_new;
      }
    }

    gdouble angle;
    gdouble baseangle;
    vik_viewport_compute_bearing ( vvp, x1, y1, event->x, event->y, &angle, &baseangle );

    // Update statusbar with full gain/loss information
    statusbar_write (distance, elev_gain, elev_loss, last_step, angle, vtl);

    return VIK_LAYER_TOOL_ACK_GRAB_FOCUS;
  }
  return VIK_LAYER_TOOL_ACK;
}

// NB vtl->current_track must be valid
static void undo_trackpoint_add ( VikTrwLayer *vtl )
{
  // 'undo'
  if ( vtl->current_track->trackpoints ) {
    // TODO rework this...
    //vik_trackpoint_free ( vik_track_get_tp_last (vtl->current_track) );
    GList *last = g_list_last(vtl->current_track->trackpoints);
    g_free ( last->data );
    vtl->current_track->trackpoints = g_list_remove_link ( vtl->current_track->trackpoints, last );

    vik_track_calculate_bounds ( vtl->current_track );
  }
}

static gboolean tool_edit_track_key_press ( VikTrwLayer *vtl, GdkEventKey *event, gpointer data )
{
  gboolean mods = event->state & gtk_accelerator_get_default_mod_mask ();
  if ( vtl->current_track && event->keyval == GDK_KEY_Escape && !mods ) {
    // Bin track if not very useful
    remove_current_track_if_not_enough_points ( vtl );
    vtl->current_track = NULL;
    clear_tool_draw ( vtl, vik_window_get_active_tool_data(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))) );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return TRUE;
  } else if ( vtl->current_track && event->keyval == GDK_KEY_BackSpace && !mods ) {
    undo_trackpoint_add ( vtl );
    update_statusbar ( vtl );
    clear_tool_draw ( vtl, vik_window_get_active_tool_data(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))) );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return TRUE;
  } else if ( event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R ) {
    tool_ed_t *te = data;
    GdkWindow *gdkw = gtk_widget_get_window(GTK_WIDGET(te->vvp));
    gdk_window_set_cursor ( gdkw, vtl->crosshair_cursor );
    return TRUE;
  }

  return FALSE;
}

static gboolean tool_edit_track_key_release ( VikTrwLayer *vtl, GdkEventKey *event, gpointer data )
{
  if ( event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R ) {
    VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));
    // this resets to standard tool cursor
    vik_window_clear_busy_cursor ( vw );
    return TRUE;
  }
  return FALSE;
}

/*
 * Common function to handle trackpoint button requests on either a route or a track
 *  . enables adding a point via normal click
 *  . enables removal of last point via right click
 *  . finishing of the track or route via double clicking
 */
static VikLayerToolFuncStatus tool_edit_track_or_route_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, gboolean newsegment, tool_ed_t *te )
{
  VikTrackpoint *tp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;

  if ( event->button == 2 ) {
    // As the display is panning, the new track pixmap is now invalid so don't draw it
    //  otherwise this drawing done results in flickering back to an old image
    vtl->draw_sync_do = FALSE;
    return VIK_LAYER_TOOL_IGNORED;
  }

  if ( event->button == 3 )
  {
    if ( !vtl->current_track )
      return VIK_LAYER_TOOL_IGNORED;
    undo_trackpoint_add ( vtl );
    update_statusbar ( vtl );
    clear_tool_draw ( vtl, te );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return VIK_LAYER_TOOL_ACK;
  }

  if ( event->type == GDK_2BUTTON_PRESS )
  {
    /* subtract last (duplicate from double click) tp then end */
    if ( vtl->current_track && vtl->current_track->trackpoints && vtl->ct_x1 == vtl->ct_x2 && vtl->ct_y1 == vtl->ct_y2 )
    {
      /* undo last, then end */
      undo_trackpoint_add ( vtl );
      vtl->current_track = NULL;
      clear_tool_draw ( vtl, te );
    }
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return VIK_LAYER_TOOL_ACK;
  }

  tp = vik_trackpoint_new();
  vik_viewport_screen_to_coord ( vvp, event->x, event->y, &(tp->coord) );

  /* snap to other TP */
  if ( event->state & GDK_CONTROL_MASK )
  {
    VikTrackpoint *other_tp = closest_tp_in_interval ( vtl, vvp, event->x, event->y );
    if ( other_tp )
      tp->coord = other_tp->coord;
  }

  tp->newsegment = newsegment;

  if ( vtl->current_track ) {
    vik_track_add_trackpoint ( vtl->current_track, tp, TRUE ); // Ensure bounds is updated
    /* Auto attempt to get elevation from DEM data (if it's available) */
    (void)vik_trackpoint_apply_dem_data ( tp );
  }

  vtl->ct_x1 = vtl->ct_x2;
  vtl->ct_y1 = vtl->ct_y2;
  vtl->ct_x2 = event->x;
  vtl->ct_y2 = event->y;

  vik_layer_emit_update ( VIK_LAYER(vtl) );
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus tool_edit_track_new ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  gboolean newsegment = FALSE;
  // ----------------------------------------------------- if current is a route - switch to new track
  if ( event->button == 1 && ( ! vtl->current_track || (vtl->current_track && vtl->current_track->is_route ) ))
  {
    gchar *name = trw_layer_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_TRACK, _("Track"));
    if ( a_vik_get_ask_for_create_track_name() ) {
      gchar *newname = a_dialog_new_track ( VIK_GTK_WINDOW_FROM_LAYER(vtl), name, FALSE );
      if ( !newname ) {
        g_free ( name );
        return FALSE;
      }
      name = newname;
    }
    edit_track_create_common ( vtl, name );
    g_free ( name );
    newsegment = TRUE;
  }
  return tool_edit_track_or_route_click ( vtl, event, vvp, newsegment, NULL );
}

static VikLayerToolFuncStatus tool_edit_track_or_route_split ( VikTrwLayer *vtl, TPSearchParams *params, gboolean is_track )
{
  if ( tool_select_tp ( vtl, params, is_track, ! is_track ) )
  {
    VikTrack *origin_tp_track = vtl->current_tp_track;

    trw_layer_split_at_selected_trackpoint ( vtl, is_track ? VIK_TRW_LAYER_SUBLAYER_TRACK : VIK_TRW_LAYER_SUBLAYER_ROUTE );

    vtl->current_track = origin_tp_track;
    vtl->current_tpl = NULL;
    vtl->current_tp_track = NULL;

    vik_layer_emit_update(VIK_LAYER(vtl));
    return VIK_LAYER_TOOL_ACK;
  }
  return VIK_LAYER_TOOL_IGNORED;
}

// attempt to join to a track/route
// plot a route to join if in route finder tool
static VikLayerToolFuncStatus tool_edit_track_or_route_join ( VikTrwLayer *vtl, TPSearchParams *params, gboolean in_route_finder )
{
  if ( vtl->current_track == NULL )
    return VIK_LAYER_TOOL_IGNORED;

  VikTrack *origin_track = vtl->current_track;
  gboolean is_route = origin_track->is_route;

  if ( tool_select_tp ( vtl, params, ! is_route, is_route ) )
  {
    // don't join to self
    if ( vtl->current_tp_track == origin_track )
      return VIK_LAYER_TOOL_IGNORED;

    if ( in_route_finder )
    {
      VikCoord *target = &(VIK_TRACKPOINT(vtl->current_tpl->data)->coord);
      if ( ! tool_plot_route ( vtl, target ) )
        return VIK_LAYER_TOOL_IGNORED;
    }

    trw_layer_split_at_selected_trackpoint ( vtl, is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK );
    vik_track_steal_and_append_trackpoints ( origin_track, vtl->current_tp_track );
    VIK_TRACKPOINT(vtl->current_tpl->data)->newsegment = FALSE;

    if ( is_route )
      vik_trw_layer_delete_route ( vtl, vtl->current_tp_track );
    else
      vik_trw_layer_delete_track ( vtl, vtl->current_tp_track );

    // Leave newly joined track selected
    tool_select_track ( vtl, origin_track );
    vtl->current_tpl = NULL;
    vtl->current_tp_track = NULL;

    vik_layer_emit_update( VIK_LAYER(vtl) );
    return VIK_LAYER_TOOL_ACK;
  }
  return VIK_LAYER_TOOL_IGNORED;
}

static VikLayerToolFuncStatus tool_edit_route_new ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  gboolean newsegment = FALSE;

  // -------------------------- if current is a track - switch to new route,
  if ( event->button == 1 && ( ! vtl->current_track ||
                               (vtl->current_track && !vtl->current_track->is_route ) ) )
  {
    gchar *name = trw_layer_new_unique_sublayer_name(vtl, VIK_TRW_LAYER_SUBLAYER_ROUTE, _("Route"));
    if ( a_vik_get_ask_for_create_track_name() ) {
      gchar *newname = a_dialog_new_track ( VIK_GTK_WINDOW_FROM_LAYER(vtl), name, TRUE );
      if ( !newname ) {
        g_free ( name );
        return FALSE;
      }
      name = newname;
    }
    edit_route_create_common ( vtl, name );
    g_free ( name );
    newsegment = TRUE;
  }
  return tool_edit_track_or_route_click ( vtl, event, vvp, newsegment, NULL );
}

// Try the following:
//   If not editing a track
//     If click is on an existing trackpoint,
//       split the track and continue editing from trackpoint
//     Else create new tracka
//   Else
//     Try to join existing track
//     If not, create new track point
static VikLayerToolFuncStatus tool_edit_track_or_route_click_dispatch ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp, gboolean is_track, tool_ed_t *te )
{
  VikLayerToolFuncStatus ans = VIK_LAYER_TOOL_IGNORED;
  // goto is evil ;)
  // but here I think for simple flow control to get to a commonal exit point it is sensible usage
  //  (whereas before it would return from any of the goto uses)

  if ( event->button != 1 ) {
    ans = tool_edit_track_or_route_click ( vtl, event, vvp, FALSE, te );
    goto my_end;
  }

  TPSearchParams params;
  params.size = MAX(5, vtl->drawpoints_size*2);
  params.vvp = vvp;
  params.x = event->x;
  params.y = event->y;
  params.closest_track_id = NULL;
  params.closest_tp = NULL;
  params.closest_tpl = NULL;
  params.bbox = vik_viewport_get_bbox ( vvp );

  // if we're not already editing a track/route
  // (is_track == is_route means we want a track, but have a route, or vice versa)
  if ( ! vtl->current_track || (vtl->current_track && vtl->current_track->is_route == is_track ) )
  {
    // attach to existing if shift pressed
    if ( event->state & GDK_SHIFT_MASK ) {
      ans = tool_edit_track_or_route_split ( vtl, &params, is_track );
      goto my_end;
    }

    // else, new track or route
    if ( is_track ) {
      ans = tool_edit_track_new ( vtl, event, vvp );
      goto my_end;
    } else {
      ans = tool_edit_route_new ( vtl, event, vvp );
      goto my_end;
    }
  }
  else
  {
    // try to join existing if shift pressed
    if ( event->state & GDK_SHIFT_MASK ) {
      ans = tool_edit_track_or_route_join ( vtl, &params, FALSE );
    goto my_end;
    } else {
      ans = tool_edit_track_or_route_click ( vtl, event, vvp, FALSE, te );
      goto my_end;
    }
  }

 my_end:
  // Show properties (this is done after point(s) have been added so it gets the latest track information)
  if ( vtl->current_track ) {
    VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl));
    gpointer gw = vik_window_get_graphs_widgets ( vw );
    (void)show_graphs_for_track(gw, vw, vtl, vtl->current_track );
  }
  return ans;
}

static VikLayerToolFuncStatus tool_edit_track_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *te = data;
  VikViewport *vvp = te->vvp;
  return tool_edit_track_or_route_click_dispatch ( vtl, event, vvp, TRUE, te );
}

static VikLayerToolFuncStatus tool_edit_track_release ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  if ( event->button == 2 ) {
    // Pan moving ended - enable potential point drawing again
    vtl->draw_sync_do = TRUE;
    vtl->draw_sync_done = TRUE;
  }
  return VIK_LAYER_TOOL_ACK;
}

static void tool_edit_route_finder_activate ( VikTrwLayer *vtl, tool_ed_t *te )
{
  VikStatusbar *sb = vik_window_get_statusbar ( te->vw );
  VikRoutingEngine *engine = vik_routing_default_engine ( );
  if ( engine ) {
    gchar *msg = g_strdup_printf ( _("Route Finder using: %s"), vik_routing_engine_get_label(engine) );
    vik_statusbar_set_message ( sb, VIK_STATUSBAR_INFO, msg );
    g_free ( msg );
  }
}

static void tool_edit_track_deactivate ( VikTrwLayer *ignore, tool_ed_t *te )
{
  tool_edit_remove_image ( te );
  vik_viewport_sync ( te->vvp, NULL );
}

static VikLayerToolFuncStatus tool_edit_route_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;
  return tool_edit_track_or_route_click_dispatch ( vtl, event, vvp, FALSE, t );
}

/*** New waypoint ****/

static gpointer tool_new_waypoint_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static VikLayerToolFuncStatus tool_new_waypoint_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  VikCoord coord;
  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return FALSE;
  vik_viewport_screen_to_coord ( vvp, event->x, event->y, &coord );
  if ( vik_trw_layer_new_waypoint (vtl, VIK_GTK_WINDOW_FROM_LAYER(vtl), &coord) ) {
    trw_layer_calculate_bounds_waypoints ( vtl );
    if ( VIK_LAYER(vtl)->visible )
      vik_layer_emit_update ( VIK_LAYER(vtl) );
  }
  return VIK_LAYER_TOOL_ACK;
}


/*** Edit trackpoint ****/

/**
 * tool_edit_trackpoint_click:
 *
 * On 'initial' click: search for the nearest trackpoint or routepoint and store it as the current trackpoint
 * Then update the viewport, statusbar and edit dialog to draw the point as being selected and it's information.
 * On subsequent clicks: (as the current trackpoint is defined) and the click is very near the same point
 *  then initiate the move operation to drag the point to a new destination.
 * NB The current trackpoint will get reset elsewhere.
 */
static VikLayerToolFuncStatus tool_edit_trackpoint_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;
  t->vtl = vtl;
  TPSearchParams params;
  params.size = MAX(5, vtl->drawpoints_size*2);
  params.vvp = vvp;
  params.x = event->x;
  params.y = event->y;
  params.closest_track_id = NULL;
  params.closest_tp = NULL;
  params.closest_tpl = NULL;
  params.bbox = vik_viewport_get_bbox ( vvp );

  if ( event->button != 1 ) 
    return VIK_LAYER_TOOL_IGNORED;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;

  if ( !vtl->vl.visible || !(vtl->tracks_visible || vtl->routes_visible) )
    return VIK_LAYER_TOOL_IGNORED;

  if ( vtl->current_tpl )
  {
    /* first check if it is within range of prev. tp. and if current_tp track is shown. (if it is, we are moving that trackpoint.) */
    VikTrackpoint *tp = VIK_TRACKPOINT(vtl->current_tpl->data);
    VikTrack *current_tr = vtl->current_tp_track;
    if ( !current_tr )
      return VIK_LAYER_TOOL_IGNORED;

    gint x, y;
    vik_viewport_coord_to_screen ( vvp, &(tp->coord), &x, &y );

    if ( current_tr->visible && 
         abs(x - (int)round(event->x)) < (vtl->drawpoints_size*2) &&
         abs(y - (int)round(event->y)) < (vtl->drawpoints_size*2) ) {
      marker_begin_move ( t, event->x, event->y );
      return VIK_LAYER_TOOL_ACK;
    }

  }

  if ( tool_select_tp ( vtl, &params, TRUE, TRUE ) )
  {
    trw_layer_tpwin_init ( vtl );
    return VIK_LAYER_TOOL_ACK;
  }

  /* these aren't the droids you're looking for */
  return VIK_LAYER_TOOL_IGNORED;
}

static VikLayerToolFuncStatus tool_edit_trackpoint_move ( VikTrwLayer *vtl, GdkEventMotion *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;

  if ( t->holding )
  {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_interval ( vtl, vvp, event->x, event->y );
      if ( tp && tp != vtl->current_tpl->data )
        new_coord = tp->coord;
    }
    //    VIK_TRACKPOINT(vtl->current_tpl->data)->coord = new_coord;
    { 
      gint x, y;
      vik_viewport_coord_to_screen ( vvp, &new_coord, &x, &y );
      marker_moveto ( t, x, y );
    } 

    return VIK_LAYER_TOOL_ACK;
  }
  return VIK_LAYER_TOOL_IGNORED;
}

static VikLayerToolFuncStatus tool_edit_trackpoint_release ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  tool_ed_t *t = data;
  VikViewport *vvp = t->vvp;

  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;
  if ( event->button != 1) 
    return VIK_LAYER_TOOL_IGNORED;

  if ( t->holding ) {
    VikCoord new_coord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &new_coord );

    /* snap to TP */
    if ( event->state & GDK_CONTROL_MASK )
    {
      VikTrackpoint *tp = closest_tp_in_interval ( vtl, vvp, event->x, event->y );
      if ( tp && tp != vtl->current_tpl->data )
        new_coord = tp->coord;
    }

    VIK_TRACKPOINT(vtl->current_tpl->data)->coord = new_coord;
    if ( vtl->current_tp_track )
      vik_track_calculate_bounds ( vtl->current_tp_track );

    marker_end_move ( t );

    /* diff dist is diff from orig */
    if ( vtl->tpwin )
      if ( vtl->current_tp_track )
        my_tpwin_set_tp ( vtl );

    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return VIK_LAYER_TOOL_ACK;
  }
  return VIK_LAYER_TOOL_IGNORED;
}


/*** Extended Route Finder ***/

static void tool_extended_route_finder_undo ( VikTrwLayer *vtl )
{
  clear_tool_draw ( vtl, vik_window_get_active_tool_data(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))) );

  VikCoord *new_end;
  new_end = vik_track_cut_back_to_double_point ( vtl->current_track );
  if ( new_end ) {
    g_free ( new_end );
    vik_layer_emit_update ( VIK_LAYER(vtl) );

    /* remove last ' to:...' */
    if ( vtl->current_track->comment ) {
      gchar *last_to = strrchr ( vtl->current_track->comment, 't' );
      if ( last_to && (last_to - vtl->current_track->comment > 1) ) {
        gchar *new_comment = g_strndup ( vtl->current_track->comment,
                                         last_to - vtl->current_track->comment - 1);
        vik_track_set_comment_no_copy ( vtl->current_track, new_comment );
      }
    }
  }
}

static VikLayerToolFuncStatus tool_extended_route_finder_click ( VikTrwLayer *vtl, GdkEventButton *event, gpointer data )
{
  if ( !vtl ) return VIK_LAYER_TOOL_IGNORED;
  tool_ed_t *te = data;
  VikViewport *vvp = te->vvp;
  if ( event->button == 3 && vtl->current_track ) {
    tool_extended_route_finder_undo ( vtl );
  }
  else if ( event->button == 2 ) {
     vtl->draw_sync_do = FALSE;
     return VIK_LAYER_TOOL_IGNORED;
  }
  // if we started the track but via undo deleted all the track points, begin again
  else if ( vtl->current_track && vtl->current_track->is_route && ! vik_track_get_tp_first ( vtl->current_track ) ) {
    return tool_edit_track_or_route_click ( vtl, event, vvp, TRUE, te );
  }
  else if ( ( vtl->current_track && vtl->current_track->is_route ) ) {
    if ( event->state & GDK_SHIFT_MASK )
    {
      TPSearchParams params;
      params.size = MAX(5, vtl->drawpoints_size*2);
      params.vvp = vvp;
      params.x = event->x;
      params.y = event->y;
      params.closest_track_id = NULL;
      params.closest_tp = NULL;
      params.closest_tpl = NULL;
      params.bbox = vik_viewport_get_bbox ( vvp );

      (void)tool_edit_track_or_route_join ( vtl, &params, TRUE );
    }
    else
    {
      vik_viewport_screen_to_coord ( vvp, event->x, event->y, &vtl->route_finder_request_coord );

      if ( vtl->route_finder_timer_id )
        g_source_remove ( vtl->route_finder_timer_id );
      vtl->route_finder_timer_id= 0;

      vtl->ct_x2 = event->x;
      vtl->ct_y2 = event->y;

      vtl->route_finder_end = FALSE;
      // NB as per normal track/route editing, double click ends this edit
      //  but only after including this new point
      if ( event->type == GDK_2BUTTON_PRESS ) {
        vtl->route_finder_end = TRUE;
        // However if double click on the endpoint, then end without adding another point
        if ( vtl->current_track && vtl->current_track->trackpoints &&
             vtl->ct_x1 == vtl->ct_x2 && vtl->ct_y1 == vtl->ct_y2 ) {
          vtl->current_track = NULL;
          clear_tool_draw ( vtl, te );
          vik_layer_emit_update ( VIK_LAYER(vtl) );
          return VIK_LAYER_TOOL_ACK;
        }
      }

      vtl->ct_x1 = vtl->ct_x2;
      vtl->ct_y1 = vtl->ct_y2;

      // Get double click time
      GtkSettings *gs = gtk_widget_get_settings ( GTK_WIDGET(vvp) );
      GValue dct = G_VALUE_INIT;
      g_value_init ( &dct, G_TYPE_INT );
      g_object_get_property ( G_OBJECT(gs), "gtk-double-click-time", &dct );
      // Give chance for a double click to occur
      gint timer = g_value_get_int ( &dct ) + 50;
      vtl->route_finder_timer_id = g_timeout_add ( timer, (GSourceFunc)tool_plot_route_pending, vtl );
    }
  } else {
    vtl->current_track = NULL;

    // create a new route where we will add the planned route to
    return tool_edit_route_click( vtl, event, te );
  }
  return VIK_LAYER_TOOL_ACK;
}

static gboolean tool_extended_route_finder_key_press ( VikTrwLayer *vtl, GdkEventKey *event, gpointer data )
{
  if ( vtl->current_track && event->keyval == GDK_KEY_Escape ) {
    // Bin track if not very useful
    remove_current_track_if_not_enough_points ( vtl );
    vtl->current_track = NULL;
    clear_tool_draw ( vtl, vik_window_get_active_tool_data(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl))) );
    vik_layer_emit_update ( VIK_LAYER(vtl) );
    return TRUE;
  } else if ( vtl->current_track && event->keyval == GDK_KEY_BackSpace ) {
    tool_extended_route_finder_undo ( vtl );
    return TRUE;
  } else if ( event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R ) {
    tool_ed_t *te = data;
    GdkWindow *gdkw = gtk_widget_get_window(GTK_WIDGET(te->vvp));
    gdk_window_set_cursor ( gdkw, vtl->crosshair_cursor );
    return TRUE;
  }

  return FALSE;
}



/*** Show picture ****/

static gpointer tool_show_picture_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

/* Params are: vvp, event, last match found or NULL */
static void tool_show_picture_wp ( const gpointer id, VikWaypoint *wp, gpointer params[3] )
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

static void trw_layer_show_picture ( menu_array_sublayer values )
{
  /* thanks to the Gaim people for showing me ShellExecute and g_spawn_command_line_async */
#ifdef WINDOWS
  ShellExecute(NULL, "open", (char *) values[MA_MISC], NULL, NULL, SW_SHOWNORMAL);
#else /* WINDOWS */
  GError *err = NULL;
  gchar *quoted_file = g_shell_quote ( (gchar *) values[MA_MISC] );
  gchar *cmd = g_strdup_printf ( "%s %s", a_vik_get_image_viewer(), quoted_file );
  g_free ( quoted_file );
  if ( ! g_spawn_command_line_async ( cmd, &err ) )
    {
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER(values[MA_VTL]), _("Could not launch %s to open file."), a_vik_get_image_viewer() );
      g_error_free ( err );
    }
  g_free ( cmd );
#endif /* WINDOWS */
}

static VikLayerToolFuncStatus tool_show_picture_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  gpointer params[3] = { vvp, event, NULL };
  if (!vtl || vtl->vl.type != VIK_LAYER_TRW)
    return VIK_LAYER_TOOL_IGNORED;
  g_hash_table_foreach ( vtl->waypoints, (GHFunc) tool_show_picture_wp, params );
  if ( params[2] )
  {
    static menu_array_sublayer values;
    values[MA_VTL] = vtl;
    values[MA_MISC] = params[2];
    trw_layer_show_picture ( values );
    return VIK_LAYER_TOOL_ACK; // found a match
  }
  else
    return VIK_LAYER_TOOL_IGNORED; // go through other layers, searching for a match
}

/***************************************************************************
 ** End tool code 
 ***************************************************************************/


static void image_wp_make_list ( const gpointer id, VikWaypoint *wp, GSList **pics )
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
    vik_layer_emit_update ( VIK_LAYER(tctd->vtl) ); // NB update from background thread

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

void trw_layer_verify_thumbnails ( VikTrwLayer *vtl )
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
      a_background_thread ( BACKGROUND_POOL_LOCAL,
                            VIK_GTK_WINDOW_FROM_LAYER(vtl),
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

static const gchar* my_track_colors ( gint ii )
{
  static const gchar* colors[VIK_TRW_LAYER_TRACK_GCS] = {
    "#2d870a",
    "#135D34",
    "#0a8783",
    "#0e4d87",
    "#05469f",
    "#695CBB",
    "#2d059f",
    "#4a059f",
    "#5A171A",
    "#96059f"
  };
  // Fast and reliable way of returning a colour
  return colors[(ii % VIK_TRW_LAYER_TRACK_GCS)];
}

static void trw_layer_track_alloc_colors ( VikTrwLayer *vtl )
{
  GHashTableIter iter;
  gpointer key, value;

  gint ii = 0;
  // Tracks
  g_hash_table_iter_init ( &iter, vtl->tracks );

  while ( g_hash_table_iter_next (&iter, &key, &value) ) {

    // Tracks get a random spread of colours if not already assigned
    if ( ! VIK_TRACK(value)->has_color ) {
      if ( vtl->drawmode == DRAWMODE_ALL_SAME_COLOR )
        VIK_TRACK(value)->color = vtl->track_color;
      else {
        gdk_color_parse ( my_track_colors (ii), &(VIK_TRACK(value)->color) );
      }
      VIK_TRACK(value)->has_color = TRUE;
    }

    trw_layer_update_treeview ( vtl, VIK_TRACK(value), FALSE );

    ii++;
    if (ii > VIK_TRW_LAYER_TRACK_GCS)
      ii = 0;
  }

  // Routes
  ii = 0;
  g_hash_table_iter_init ( &iter, vtl->routes );

  while ( g_hash_table_iter_next (&iter, &key, &value) ) {

    // Routes get an intermix of reds
    if ( ! VIK_TRACK(value)->has_color ) {
      if ( ii )
        gdk_color_parse ( "#FF0000" , &(VIK_TRACK(value)->color) ); // Red
      else
        gdk_color_parse ( "#B40916" , &(VIK_TRACK(value)->color) ); // Dark Red
      VIK_TRACK(value)->has_color = TRUE;
    }

    trw_layer_update_treeview ( vtl, VIK_TRACK(value), FALSE );

    ii = !ii;
  }
}

/*
 * (Re)Calculate the bounds of the waypoints in this layer,
 * This should be called whenever waypoints are changed
 */
void trw_layer_calculate_bounds_waypoints ( VikTrwLayer *vtl )
{
  struct LatLon topleft = { 0.0, 0.0 };
  struct LatLon bottomright = { 0.0, 0.0 };
  struct LatLon ll;

  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init ( &iter, vtl->waypoints );

  // Set bounds to first point
  if ( g_hash_table_iter_next (&iter, &key, &value) ) {
    vik_coord_to_latlon ( &(VIK_WAYPOINT(value)->coord), &topleft );
    vik_coord_to_latlon ( &(VIK_WAYPOINT(value)->coord), &bottomright );
  }

  // Ensure there is another point...
  if ( g_hash_table_size ( vtl->waypoints ) > 1 ) {

    while ( g_hash_table_iter_next (&iter, &key, &value) ) {

      // See if this point increases the bounds.
      vik_coord_to_latlon ( &(VIK_WAYPOINT(value)->coord), &ll );

      if ( ll.lat > topleft.lat) topleft.lat = ll.lat;
      if ( ll.lon < topleft.lon) topleft.lon = ll.lon;
      if ( ll.lat < bottomright.lat) bottomright.lat = ll.lat;
      if ( ll.lon > bottomright.lon) bottomright.lon = ll.lon;
    }
  }

  vtl->waypoints_bbox.north = topleft.lat;
  vtl->waypoints_bbox.east = bottomright.lon;
  vtl->waypoints_bbox.south = bottomright.lat;
  vtl->waypoints_bbox.west = topleft.lon;
}

static void trw_layer_calculate_bounds_track ( gpointer id, VikTrack *trk )
{
  vik_track_calculate_bounds ( trk );
}

void trw_layer_calculate_bounds_tracks ( VikTrwLayer *vtl )
{
  g_hash_table_foreach ( vtl->tracks, (GHFunc) trw_layer_calculate_bounds_track, NULL );
  g_hash_table_foreach ( vtl->routes, (GHFunc) trw_layer_calculate_bounds_track, NULL );
}

static void trw_layer_sort_all ( VikTrwLayer *vtl )
{
  if ( ! VIK_LAYER(vtl)->vt )
    return;

  // Obviously need 2 to tango - sorting with only 1 (or less) is a lonely activity!
  if ( g_hash_table_size (vtl->tracks) > 1 )
    vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->tracks_iter), vtl->track_sort_order );

  if ( g_hash_table_size (vtl->routes) > 1 )
    vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->routes_iter), vtl->track_sort_order );

  if ( g_hash_table_size (vtl->waypoints) > 1 )
    vik_treeview_sort_children ( VIK_LAYER(vtl)->vt, &(vtl->waypoints_iter), vtl->wp_sort_order );
}

/**
 * Get the earliest timestamp available from all tracks
 */
static gdouble trw_layer_get_timestamp_tracks ( VikTrwLayer *vtl )
{
  gdouble timestamp = NAN;
  GList *gl = g_hash_table_get_values ( vtl->tracks );
  gl = g_list_sort ( gl, vik_track_compare_timestamp );
  gl = g_list_first ( gl );

  if ( gl ) {
    // Only need to check the first track as they have been sorted by time
    VikTrack *trk = (VikTrack*)gl->data;
    // Assume trackpoints already sorted by time
    VikTrackpoint *tpt = vik_track_get_tp_first(trk);
    if ( tpt && !isnan(tpt->timestamp) ) {
      timestamp = tpt->timestamp;
    }
    g_list_free ( gl );
  }
  return timestamp;
}

/**
 * Get the earliest timestamp available from all waypoints
 */
static gdouble trw_layer_get_timestamp_waypoints ( VikTrwLayer *vtl )
{
  gdouble timestamp = NAN;
  GList *gl = g_hash_table_get_values ( vtl->waypoints );
  GList *iter;
  for (iter = g_list_first (gl); iter != NULL; iter = g_list_next (iter)) {
    VikWaypoint *wpt = (VikWaypoint*)iter->data;
    if ( !isnan(wpt->timestamp) ) {
      // When timestamp not set yet - use the first value encountered
      if ( isnan(timestamp) )
        timestamp = wpt->timestamp;
      else if ( timestamp > wpt->timestamp )
        timestamp = wpt->timestamp;
    }
  }
  g_list_free ( gl );

  return timestamp;
}

/**
 * Get the earliest timestamp available for this layer
 */
static gdouble trw_layer_get_timestamp ( VikTrwLayer *vtl )
{
  gdouble timestamp_tracks = trw_layer_get_timestamp_tracks ( vtl );
  gdouble timestamp_waypoints = trw_layer_get_timestamp_waypoints ( vtl );
  // NB routes don't have timestamps - hence they are not considered

  if ( isnan(timestamp_tracks) && isnan(timestamp_waypoints) ) {
    // Fallback to get time from the metadata when no other timestamps available
    // NB Seconds resolution from metadata is plenty accurate enough for this usage
    GTimeVal gtv;
    if ( vtl->metadata && vtl->metadata->timestamp && g_time_val_from_iso8601 ( vtl->metadata->timestamp, &gtv ) )
      return (gdouble)gtv.tv_sec;
  }
  if ( !isnan(timestamp_tracks) && isnan(timestamp_waypoints) )
    return timestamp_tracks;
  if ( !isnan(timestamp_tracks) && !isnan(timestamp_waypoints) && (timestamp_tracks < timestamp_waypoints) )
    return timestamp_tracks;
  return timestamp_waypoints;
}

static void trw_layer_post_read ( VikTrwLayer *vtl, VikViewport *vvp, gboolean from_file )
{
  if ( VIK_LAYER(vtl)->realized )
    trw_layer_verify_thumbnails ( vtl );
  trw_layer_track_alloc_colors ( vtl );

  GHashTableIter iter;
  gpointer key, value;

  if ( vtl->auto_dem ) {
    g_hash_table_iter_init ( &iter, vtl->tracks );
    while ( g_hash_table_iter_next ( &iter, &key, &value ) ) {
      VikTrack *trk = VIK_TRACK(value);
      (void)vik_track_apply_dem_data ( trk, FALSE );
    }
  }

  if ( vtl->auto_dedupl ) {
    g_hash_table_iter_init ( &iter, vtl->tracks );
    while ( g_hash_table_iter_next ( &iter, &key, &value ) ) {
      VikTrack *trk = VIK_TRACK(value);
      gulong count = vik_track_remove_dup_points(trk);
      if ( count )
        g_debug ( "%s: Auto removed %ld duplicate points", __FUNCTION__, count );
    }
  }

  trw_layer_calculate_bounds_waypoints ( vtl );
  trw_layer_calculate_bounds_tracks ( vtl );

  // Apply treeview sort after loading all the tracks for this layer
  //  (rather than sorted insert on each individual track additional)
  //  and after subsequent changes to the properties as the specified order may have changed.
  //  since the sorting of a treeview section is now very quick
  // NB sorting is also performed after every name change as well to maintain the list order
  trw_layer_sort_all ( vtl );

  // Setting metadata time if not otherwise set
  if ( vtl->metadata ) {

    gboolean need_to_set_time = TRUE;
    if ( vtl->metadata->timestamp ) {
      need_to_set_time = FALSE;
      if ( !g_strcmp0(vtl->metadata->timestamp, "" ) )
        need_to_set_time = TRUE;
    }

    if ( need_to_set_time ) {
      GTimeVal timestamp;
      gdouble ts = trw_layer_get_timestamp ( vtl );
      if ( isnan(ts) )
        // No time found - so use 'now' for the metadata time
        g_get_current_time ( &timestamp );
      else
        timestamp.tv_sec = (glong)ts;
      timestamp.tv_usec = 0;

      vtl->metadata->timestamp = g_time_val_to_iso8601 ( &timestamp );
    }
  }
}

VikCoordMode vik_trw_layer_get_coord_mode ( VikTrwLayer *vtl )
{
  return vtl->coord_mode;
}

/**
 * Uniquify the whole layer
 * Also requires the layers panel as the names shown there need updating too
 * Returns whether the operation was successful or not
 */
gboolean vik_trw_layer_uniquify ( VikTrwLayer *vtl, VikLayersPanel *vlp )
{
  if ( vtl && vlp ) {
    vik_trw_layer_uniquify_tracks ( vtl, vlp, vtl->tracks, TRUE );
    vik_trw_layer_uniquify_tracks ( vtl, vlp, vtl->routes, FALSE );
    vik_trw_layer_uniquify_waypoints ( vtl, vlp );
    return TRUE;
  }
  return FALSE;
}

static void waypoint_convert ( const gpointer id, VikWaypoint *wp, VikCoordMode *dest_mode )
{
  vik_coord_convert ( &(wp->coord), *dest_mode );
}

static void track_convert ( const gpointer id, VikTrack *tr, VikCoordMode *dest_mode )
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
    g_hash_table_foreach ( vtl->routes, (GHFunc) track_convert, &dest_mode );
  }
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
    GList *fiter = fillins;
    while (fiter) {
      cur_coord = (VikCoord *)(fiter->data);
      vik_coord_set_area(cur_coord, &wh, &tl, &br);
      rect = g_malloc(sizeof(Rect));
      rect->tl = tl;
      rect->br = br;
      rect->center = *cur_coord;
      rects_to_download = g_list_prepend(rects_to_download, rect);
      fiter = fiter->next;
    }
  }

  for (rect_iter = rects_to_download; rect_iter; rect_iter = rect_iter->next) {
    vik_maps_layer_download_section (vml, vvp, &(((Rect *)(rect_iter->data))->tl), &(((Rect *)(rect_iter->data))->br), zoom_level);
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

static void trw_layer_download_map_along_track_cb ( menu_array_sublayer values )
{
  VikMapsLayer *vml;
  gint selected_map;
  gchar *zoomlist[] = {"0.125", "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", NULL };
  gdouble zoom_vals[] = {0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
  gint selected_zoom, default_zoom;

  VikTrwLayer *vtl = values[MA_VTL];
  VikLayersPanel *vlp = values[MA_VLP];
  VikTrack *trk;
  if ( GPOINTER_TO_INT (values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_ROUTE )
    trk = (VikTrack *) g_hash_table_lookup ( vtl->routes, values[MA_SUBLAYER_ID] );
  else
    trk = (VikTrack *) g_hash_table_lookup ( vtl->tracks, values[MA_SUBLAYER_ID] );
  if ( !trk )
    return;

  VikViewport *vvp = vik_window_viewport((VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(vtl)));

  GList *vmls = vik_layers_panel_get_all_layers_of_type(vlp, VIK_LAYER_MAPS, TRUE); // Includes hidden map layer types
  int num_maps = g_list_length(vmls);

  if (!num_maps) {
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No map layer in use. Create one first") );
    return;
  }

  // Convert from list of vmls to list of names. Allowing the user to select one of them
  gchar **map_names = g_malloc_n(1 + num_maps, sizeof(gpointer));
  VikMapsLayer **map_layers = g_malloc_n(1 + num_maps, sizeof(gpointer));

  gchar **np = map_names;
  VikMapsLayer **lp = map_layers;
  int i;
  for (i = 0; i < num_maps; i++) {
    vml = (VikMapsLayer *)(vmls->data);
    *lp++ = vml;
    *np++ = vik_maps_layer_get_map_label(vml);
    vmls = vmls->next;
  }
  // Mark end of the array lists
  *lp = NULL;
  *np = NULL;

  gdouble cur_zoom = vik_viewport_get_zoom(vvp);
  for (default_zoom = 0; default_zoom < G_N_ELEMENTS(zoom_vals); default_zoom++) {
    if (cur_zoom == zoom_vals[default_zoom])
      break;
  }
  default_zoom = (default_zoom == G_N_ELEMENTS(zoom_vals)) ? G_N_ELEMENTS(zoom_vals) - 1 : default_zoom;

  if (!a_dialog_map_n_zoom(VIK_GTK_WINDOW_FROM_LAYER(vtl), map_names, 0, zoomlist, default_zoom, &selected_map, &selected_zoom))
    goto done;

  vik_track_download_map(trk, map_layers[selected_map], vvp, zoom_vals[selected_zoom]);

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
  vtl->highest_wp_number = 0;
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
    vtl->highest_wp_number--;

    g_snprintf(buf,4,"%03d", vtl->highest_wp_number );
    /* search down until we find something that *does* exist */

    while ( vtl->highest_wp_number > 0 && ! vik_trw_layer_get_waypoint ( vtl, buf )) {
      vtl->highest_wp_number--;
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

/**
 * trw_layer_create_track_list_both:
 *
 * Create the latest list of tracks and routes
 */
static GList* trw_layer_create_track_list_both ( VikLayer *vl, gpointer user_data )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(vl);
  GList *tracks = g_hash_table_get_values ( vik_trw_layer_get_tracks ( vtl ) );
  tracks = g_list_concat ( tracks, g_hash_table_get_values ( vik_trw_layer_get_routes ( vtl ) ) );

  return vik_trw_layer_build_track_list_t ( vtl, tracks );
}

static void trw_layer_track_list_dialog_single ( menu_array_sublayer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  gchar *title = NULL;
  if ( GPOINTER_TO_INT(values[MA_SUBTYPE]) == VIK_TRW_LAYER_SUBLAYER_TRACKS )
    title = g_strdup_printf ( _("%s: Track List"), VIK_LAYER(vtl)->name );
  else
    title = g_strdup_printf ( _("%s: Route List"), VIK_LAYER(vtl)->name );

  vik_trw_layer_track_list_show_dialog ( title, VIK_LAYER(vtl), values[MA_SUBTYPE], trw_layer_create_track_list, FALSE );
  g_free ( title );
}

static void trw_layer_track_list_dialog ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  gchar *title = g_strdup_printf ( _("%s: Track and Route List"), VIK_LAYER(vtl)->name );
  vik_trw_layer_track_list_show_dialog ( title, VIK_LAYER(vtl), NULL, trw_layer_create_track_list_both, FALSE );
  g_free ( title );
}

static void trw_layer_waypoint_list_dialog ( menu_array_layer values )
{
  VikTrwLayer *vtl = VIK_TRW_LAYER(values[MA_VTL]);

  gchar *title = g_strdup_printf ( _("%s: Waypoint List"), VIK_LAYER(vtl)->name );
  vik_trw_layer_waypoint_list_show_dialog ( title, VIK_LAYER(vtl), NULL, trw_layer_create_waypoint_list, FALSE );
  g_free ( title );
}

static void trw_write_file ( VikTrwLayer *trw, FILE *f, const gchar *dirpath )
{
  if ( trw->external_layer == VIK_TRW_LAYER_EXTERNAL ) {
    trw_write_file_external ( trw, f, dirpath );
  } else if ( trw->external_layer != VIK_TRW_LAYER_EXTERNAL_NO_WRITE ) {
    a_gpspoint_write_file( trw, f, dirpath );
  }
}

gboolean trw_read_file ( VikTrwLayer *trw, FILE *f, const gchar *dirpath )
{
  if ( trw->external_layer != VIK_TRW_LAYER_INTERNAL ) {
    return trw_read_file_external ( trw, f, dirpath );
  } else {
    return a_gpspoint_read_file( trw, f, dirpath );
  }
}

static void trw_write_file_external ( VikTrwLayer *trw, FILE *f, const gchar *dirpath )
{
  g_assert ( trw != NULL && trw->external_file != NULL );

  // if never loaded or not for writing, no need to rewrite
  if ( trw->external_layer != VIK_TRW_LAYER_EXTERNAL || ! trw->external_loaded )
    return;

  gboolean success = a_file_export ( trw, trw->external_file, FILE_TYPE_GPX, NULL, TRUE );

  if ( ! success ) {
    gchar *msg = g_strdup_printf ( _("Could not write external layer %s to %s, please fix and save before exiting or data will be lost"), VIK_LAYER(trw)->name, trw->external_file );
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(trw), msg );
    g_free ( msg );
  }
}

static gboolean trw_read_file_external ( VikTrwLayer *trw, FILE *f, const gchar *dirpath )
{
  g_assert ( trw != NULL && trw->external_file != NULL && f != NULL );

  g_free ( trw->external_dirpath );
  trw->external_dirpath = g_strdup ( dirpath );

  // leave loading to trw_layer_draw function
  trw->external_loaded = FALSE;

  // read ~EndLayerData
  static char line_buffer[15];
  (void)! fgets(line_buffer, 15, f); // Not worried about file read errors
  gboolean success = ( strlen(line_buffer) >= 13 && strncmp ( line_buffer, "~EndLayerData", 13 ) == 0 );

  return success;
}

static gboolean trw_load_external_layer ( VikTrwLayer *trw )
{
  g_assert ( trw != NULL && trw->external_file != NULL );

  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(trw));
  gchar *extfile_full = util_make_absolute_filename ( trw->external_file, trw->external_dirpath );
  gchar *extfile = extfile_full ? extfile_full : trw->external_file;

  gboolean failed = TRUE;
  FILE *ext_f = g_fopen ( extfile, "r" );
  if ( ext_f ) {
    vik_window_set_busy_cursor ( vw );

    gchar *dirpath = g_path_get_dirname ( extfile );
    failed = ! a_gpx_read_file ( trw, ext_f, dirpath, FALSE );
    g_free ( dirpath );
    fclose ( ext_f );

    vik_window_clear_busy_cursor ( vw );
  }

  trw->external_loaded = ! failed;

  if ( failed ) {
    gchar *msg = g_strdup_printf ( _("WARNING: issues encountered loading external layer %s from %s"), VIK_LAYER(trw)->name, extfile );
    vik_statusbar_set_message ( vik_window_get_statusbar ( vw ), VIK_STATUSBAR_INFO, msg );
    g_free ( msg );
  }

  g_free ( extfile_full );

  return ! failed;
}

void trw_ensure_layer_loaded ( VikTrwLayer *trw )
{
  if ( trw->external_layer != VIK_TRW_LAYER_INTERNAL && ! trw->external_loaded ) {
    // set to true for now else the load will trigger redraws that will
    // trigger reloads...
    // trw_load_external_layer will set this to false if the load fails
    trw->external_loaded = TRUE;
    trw_load_external_layer ( trw );
    trw_layer_post_read ( trw, NULL, FALSE );
  }
}

/**
 * Convert layer to an external layer.
 * Set as a read only layer (i.e. don't write back to file by default)
 */
void trw_layer_replace_external ( VikTrwLayer *trw, const gchar *external_file )
{
  trw->external_layer = VIK_TRW_LAYER_EXTERNAL_NO_WRITE;
  trw_update_layer_icon ( trw );
  g_free ( trw->external_file );
  trw->external_file = g_strdup ( external_file );
  trw->external_loaded = TRUE;
}

static void trw_update_layer_icon ( VikTrwLayer *trw )
{
  if ( ! VIK_LAYER(trw)->vt )
    return;

  GdkPixbuf *buf;
  switch ( trw->external_layer ) {
    case VIK_TRW_LAYER_EXTERNAL: buf = ui_get_icon ( "viktrwlayer_external", 16 ); break;
    case VIK_TRW_LAYER_EXTERNAL_NO_WRITE: buf = ui_get_icon ( "viktrwlayer_external_nowrite", 16 ); break;
    default: buf = ui_get_icon ( "viktrwlayer", 16 ); break;
  }
  vik_treeview_item_set_icon ( VIK_LAYER(trw)->vt, &(VIK_LAYER(trw)->iter), buf );
  g_object_unref ( buf );
}

/*** Splitter ***/

static gpointer tool_splitter_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static VikLayerToolFuncStatus tool_splitter_click ( VikTrwLayer *vtl, GdkEventButton *event, VikViewport *vvp )
{
  TPSearchParams params;
  params.size = MAX(5, vtl->drawpoints_size*2);
  params.vvp = vvp;
  params.x = event->x;
  params.y = event->y;
  params.closest_track_id = NULL;
  params.closest_tp = NULL;
  params.closest_tpl = NULL;
  params.bbox = vik_viewport_get_bbox ( vvp );

  if ( tool_select_tp ( vtl, &params, TRUE, TRUE ) )
  {
    trw_layer_split_at_selected_trackpoint ( vtl, vtl->current_tp_track->is_route ? VIK_TRW_LAYER_SUBLAYER_ROUTE : VIK_TRW_LAYER_SUBLAYER_TRACK );
    return VIK_LAYER_TOOL_ACK;
  }

  return VIK_LAYER_TOOL_IGNORED;
}
