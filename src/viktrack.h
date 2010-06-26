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

/* todo important: put these in their own header file, maybe.probably also rename */

#define VIK_TRACK(x) ((VikTrack *)(x))
#define VIK_TRACKPOINT(x) ((VikTrackpoint *)(x))

typedef struct _VikTrackpoint VikTrackpoint;
struct _VikTrackpoint {
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

typedef struct _VikTrack VikTrack;
struct _VikTrack {
  GList *trackpoints;
  gboolean visible;
  gchar *comment;
  guint8 ref_count;
  GtkWidget *property_dialog;
};

VikTrack *vik_track_new();
void vik_track_set_comment(VikTrack *wp, const gchar *comment);
void vik_track_ref(VikTrack *tr);
void vik_track_free(VikTrack *tr);
VikTrack *vik_track_copy ( const VikTrack *tr );
void vik_track_set_comment_no_copy(VikTrack *tr, gchar *comment);
VikTrackpoint *vik_trackpoint_new();
void vik_trackpoint_free(VikTrackpoint *tp);
VikTrackpoint *vik_trackpoint_copy(VikTrackpoint *tp);
gdouble vik_track_get_length(const VikTrack *tr);
gdouble vik_track_get_length_including_gaps(const VikTrack *tr);
gulong vik_track_get_tp_count(const VikTrack *tr);
guint vik_track_get_segment_count(const VikTrack *tr);
VikTrack **vik_track_split_into_segments(VikTrack *tr, guint *ret_len);
void vik_track_reverse(VikTrack *tr);

gulong vik_track_get_dup_point_count ( const VikTrack *vt );
void vik_track_remove_dup_points ( VikTrack *vt );

gdouble vik_track_get_max_speed(const VikTrack *tr);
gdouble vik_track_get_average_speed(const VikTrack *tr);

void vik_track_convert ( VikTrack *tr, VikCoordMode dest_mode );
gdouble *vik_track_make_elevation_map ( const VikTrack *tr, guint16 num_chunks );
void vik_track_get_total_elevation_gain(const VikTrack *tr, gdouble *up, gdouble *down);
VikTrackpoint *vik_track_get_closest_tp_by_percentage_dist ( VikTrack *tr, gdouble reldist, gdouble *meters_from_start );
VikTrackpoint *vik_track_get_closest_tp_by_percentage_time ( VikTrack *tr, gdouble reldist, time_t *seconds_from_start );
VikTrackpoint *vik_track_get_tp_by_max_speed ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_by_max_alt ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_by_min_alt ( const VikTrack *tr );
gdouble *vik_track_make_speed_map ( const VikTrack *tr, guint16 num_chunks );
gboolean vik_track_get_minmax_alt ( const VikTrack *tr, gdouble *min_alt, gdouble *max_alt );
void vik_track_marshall ( VikTrack *tr, guint8 **data, guint *len);
VikTrack *vik_track_unmarshall (guint8 *data, guint datalen);

void vik_track_apply_dem_data ( VikTrack *tr);

/* appends t2 to t1, leaving t2 with no trackpoints */
void vik_track_steal_and_append_trackpoints ( VikTrack *t1, VikTrack *t2 );

/* starting at the end, looks backwards for the last "double point", a duplicate trackpoint.
 * this is indicative of magic scissors continued use. If there is no double point,
 * deletes all the trackpoints. returns new end of the track (or the start if
 * there are no double points)
 */
VikCoord *vik_track_cut_back_to_double_point ( VikTrack *tr );

void vik_track_set_property_dialog(VikTrack *tr, GtkWidget *dialog);
void vik_track_clear_property_dialog(VikTrack *tr);

#endif
