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

#ifndef _VIKING_TRACK_H
#define _VIKING_TRACK_H

#include <time.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "vikcoord.h"
#include "bbox.h"

G_BEGIN_DECLS

/* todo important: put these in their own header file, maybe.probably also rename */

#define VIK_TRACK(x) ((VikTrack *)(x))
#define VIK_TRACKPOINT(x) ((VikTrackpoint *)(x))

typedef struct _VikTrackpoint VikTrackpoint;
struct _VikTrackpoint {
  gchar* name;
  VikCoord coord;
  gboolean newsegment;
  gboolean has_timestamp;
  time_t timestamp;
  gdouble altitude;	/* VIK_DEFAULT_ALTITUDE if data unavailable */
  gdouble speed;  	/* NAN if data unavailable */
  gdouble course;   /* NAN if data unavailable */
  guint nsats;      /* number of satellites used. 0 if data unavailable */
#define VIK_GPS_MODE_NOT_SEEN	0	/* mode update not seen yet */
#define VIK_GPS_MODE_NO_FIX	1	/* none */
#define VIK_GPS_MODE_2D  	2	/* good for latitude/longitude */
#define VIK_GPS_MODE_3D  	3	/* good for altitude/climb too */
  gint fix_mode;    /* VIK_GPS_MODE_NOT_SEEN if data unavailable */
  gdouble hdop;     /* VIK_DEFAULT_DOP if data unavailable */
  gdouble vdop;     /* VIK_DEFAULT_DOP if data unavailable */
  gdouble pdop;     /* VIK_DEFAULT_DOP if data unavailable */
};

typedef enum {
  TRACK_DRAWNAME_NO=0,
  TRACK_DRAWNAME_CENTRE,
  TRACK_DRAWNAME_START,
  TRACK_DRAWNAME_END,
  TRACK_DRAWNAME_START_END,
  TRACK_DRAWNAME_START_END_CENTRE,
  NUM_TRACK_DRAWNAMES
} VikTrackDrawnameType;

// Instead of having a separate VikRoute type, routes are considered tracks
//  Thus all track operations must cope with a 'route' version
//  [track functions handle having no timestamps anyway - so there is no practical difference in most cases]
//  This is simpler than having to rewrite particularly every track function for route version
//   given that they do the same things
//  Mostly this matters in the display in deciding where and how they are shown
typedef struct _VikTrack VikTrack;
struct _VikTrack {
  GList *trackpoints;
  gboolean visible;
  gboolean is_route;
  VikTrackDrawnameType draw_name_mode;
  guint8 max_number_dist_labels;
  gchar *comment;
  gchar *description;
  guint8 ref_count;
  gchar *name;
  GtkWidget *property_dialog;
  gboolean has_color;
  GdkColor color;
  LatLonBBox bbox;
};

VikTrack *vik_track_new();
void vik_track_set_defaults(VikTrack *tr);
void vik_track_set_name(VikTrack *tr, const gchar *name);
void vik_track_set_comment(VikTrack *tr, const gchar *comment);
void vik_track_set_description(VikTrack *tr, const gchar *description);
void vik_track_ref(VikTrack *tr);
void vik_track_free(VikTrack *tr);
VikTrack *vik_track_copy ( const VikTrack *tr, gboolean copy_points );
void vik_track_set_comment_no_copy(VikTrack *tr, gchar *comment);
VikTrackpoint *vik_trackpoint_new();
void vik_trackpoint_free(VikTrackpoint *tp);
VikTrackpoint *vik_trackpoint_copy(VikTrackpoint *tp);
void vik_trackpoint_set_name(VikTrackpoint *tp, const gchar *name);

void vik_track_add_trackpoint(VikTrack *tr, VikTrackpoint *tp, gboolean recalculate);
gdouble vik_track_get_length_to_trackpoint (const VikTrack *tr, const VikTrackpoint *tp);
gdouble vik_track_get_length(const VikTrack *tr);
gdouble vik_track_get_length_including_gaps(const VikTrack *tr);
gulong vik_track_get_tp_count(const VikTrack *tr);
guint vik_track_get_segment_count(const VikTrack *tr);
VikTrack **vik_track_split_into_segments(VikTrack *tr, guint *ret_len);
guint vik_track_merge_segments(VikTrack *tr);
void vik_track_reverse(VikTrack *tr);
time_t vik_track_get_duration(const VikTrack *trk);

gulong vik_track_get_dup_point_count ( const VikTrack *vt );
gulong vik_track_remove_dup_points ( VikTrack *vt );
gulong vik_track_get_same_time_point_count ( const VikTrack *vt );
gulong vik_track_remove_same_time_points ( VikTrack *vt );

void vik_track_to_routepoints ( VikTrack *tr );

gdouble vik_track_get_max_speed(const VikTrack *tr);
gdouble vik_track_get_average_speed(const VikTrack *tr);
gdouble vik_track_get_average_speed_moving ( const VikTrack *tr, int stop_length_seconds );

void vik_track_convert ( VikTrack *tr, VikCoordMode dest_mode );
gdouble *vik_track_make_elevation_map ( const VikTrack *tr, guint16 num_chunks );
void vik_track_get_total_elevation_gain(const VikTrack *tr, gdouble *up, gdouble *down);
VikTrackpoint *vik_track_get_tp_by_dist ( VikTrack *trk, gdouble meters_from_start, gboolean get_next_point, gdouble *tp_metres_from_start );
VikTrackpoint *vik_track_get_closest_tp_by_percentage_dist ( VikTrack *tr, gdouble reldist, gdouble *meters_from_start );
VikTrackpoint *vik_track_get_closest_tp_by_percentage_time ( VikTrack *tr, gdouble reldist, time_t *seconds_from_start );
VikTrackpoint *vik_track_get_tp_by_max_speed ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_by_max_alt ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_by_min_alt ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_first ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_last ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_prev ( const VikTrack *tr, VikTrackpoint *tp );
gdouble *vik_track_make_gradient_map ( const VikTrack *tr, guint16 num_chunks );
gdouble *vik_track_make_speed_map ( const VikTrack *tr, guint16 num_chunks );
gdouble *vik_track_make_distance_map ( const VikTrack *tr, guint16 num_chunks );
gdouble *vik_track_make_elevation_time_map ( const VikTrack *tr, guint16 num_chunks );
gdouble *vik_track_make_speed_dist_map ( const VikTrack *tr, guint16 num_chunks );
gboolean vik_track_get_minmax_alt ( const VikTrack *tr, gdouble *min_alt, gdouble *max_alt );
void vik_track_marshall ( VikTrack *tr, guint8 **data, guint *len);
VikTrack *vik_track_unmarshall (guint8 *data, guint datalen);

void vik_track_calculate_bounds ( VikTrack *trk );

void vik_track_anonymize_times ( VikTrack *tr );
void vik_track_interpolate_times ( VikTrack *tr );
gulong vik_track_apply_dem_data ( VikTrack *tr, gboolean skip_existing );
void vik_track_apply_dem_data_last_trackpoint ( VikTrack *tr );
gulong vik_track_smooth_missing_elevation_data ( VikTrack *tr, gboolean flat );

void vik_track_steal_and_append_trackpoints ( VikTrack *t1, VikTrack *t2 );

VikCoord *vik_track_cut_back_to_double_point ( VikTrack *tr );

int vik_track_compare_timestamp (const void *x, const void *y);

void vik_track_set_property_dialog(VikTrack *tr, GtkWidget *dialog);
void vik_track_clear_property_dialog(VikTrack *tr);

G_END_DECLS

#endif
