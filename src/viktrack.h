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

/* todo important: put these in their own header file, maybe.probably also rename */

#define VIK_TRACK(x) ((VikTrack *)(x))
#define VIK_TRACKPOINT(x) ((VikTrackpoint *)(x))

typedef struct _VikTrackpoint VikTrackpoint;
struct _VikTrackpoint {
  VikCoord coord;
  gboolean newsegment;
  gboolean has_timestamp;
  time_t timestamp;
  gdouble altitude;
};

typedef struct _VikTrack VikTrack;
struct _VikTrack {
  GList *trackpoints;
  gboolean visible;
  gchar *comment;
  guint8 ref_count;
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
VikCoord *vik_track_get_closest_tp_by_percentage_dist ( VikTrack *tr, gdouble reldist );
gdouble *vik_track_make_speed_map ( const VikTrack *tr, guint16 num_chunks );


#endif
