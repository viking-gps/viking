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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <time.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "coords.h"
#include "vikcoord.h"
#include "viktrack.h"
#include "globals.h"
#include "dems.h"

VikTrack *vik_track_new()
{
  VikTrack *tr = g_malloc0 ( sizeof ( VikTrack ) );
  tr->ref_count = 1;
  return tr;
}

void vik_track_set_comment_no_copy(VikTrack *tr, gchar *comment)
{
  if ( tr->comment )
    g_free ( tr->comment );
  tr->comment = comment;
}


void vik_track_set_comment(VikTrack *tr, const gchar *comment)
{
  if ( tr->comment )
    g_free ( tr->comment );

  if ( comment && comment[0] != '\0' )
    tr->comment = g_strdup(comment);
  else
    tr->comment = NULL;
}

void vik_track_ref(VikTrack *tr)
{
  tr->ref_count++;
}

void vik_track_set_property_dialog(VikTrack *tr, GtkWidget *dialog)
{
  /* Warning: does not check for existing dialog */
  tr->property_dialog = dialog;
}

void vik_track_clear_property_dialog(VikTrack *tr)
{
  tr->property_dialog = NULL;
}

void vik_track_free(VikTrack *tr)
{
  if ( tr->ref_count-- > 1 )
    return;

  if ( tr->comment )
    g_free ( tr->comment );
  g_list_foreach ( tr->trackpoints, (GFunc) g_free, NULL );
  g_list_free( tr->trackpoints );
  if (tr->property_dialog)
    if ( GTK_IS_WIDGET(tr->property_dialog) )
      gtk_widget_destroy ( GTK_WIDGET(tr->property_dialog) );
  g_free ( tr );
}

VikTrack *vik_track_copy ( const VikTrack *tr )
{
  VikTrack *new_tr = vik_track_new();
  VikTrackpoint *new_tp;
  GList *tp_iter = tr->trackpoints;
  new_tr->visible = tr->visible;
  new_tr->trackpoints = NULL;
  while ( tp_iter )
  {
    new_tp = g_malloc ( sizeof ( VikTrackpoint ) );
    *new_tp = *((VikTrackpoint *)(tp_iter->data));
    new_tr->trackpoints = g_list_append ( new_tr->trackpoints, new_tp );
    tp_iter = tp_iter->next;
  }
  vik_track_set_comment(new_tr,tr->comment);
  return new_tr;
}

VikTrackpoint *vik_trackpoint_new()
{
  VikTrackpoint *tp = g_malloc0(sizeof(VikTrackpoint));
  tp->speed = NAN;
  tp->course = NAN;
  tp->altitude = VIK_DEFAULT_ALTITUDE;
  tp->hdop = VIK_DEFAULT_DOP;
  tp->vdop = VIK_DEFAULT_DOP;
  tp->pdop = VIK_DEFAULT_DOP;
  return tp;
}

void vik_trackpoint_free(VikTrackpoint *tp)
{
  g_free(tp);
}

VikTrackpoint *vik_trackpoint_copy(VikTrackpoint *tp)
{
  VikTrackpoint *rv = vik_trackpoint_new();
  *rv = *tp;
  return rv;
}

gdouble vik_track_get_length(const VikTrack *tr)
{
  gdouble len = 0.0;
  if ( tr->trackpoints )
  {
    GList *iter = tr->trackpoints->next;
    while (iter)
    {
      if ( ! VIK_TRACKPOINT(iter->data)->newsegment )
        len += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
                                &(VIK_TRACKPOINT(iter->prev->data)->coord) );
      iter = iter->next;
    }
  }
  return len;
}

gdouble vik_track_get_length_including_gaps(const VikTrack *tr)
{
  gdouble len = 0.0;
  if ( tr->trackpoints )
  {
    GList *iter = tr->trackpoints->next;
    while (iter)
    {
      len += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
                              &(VIK_TRACKPOINT(iter->prev->data)->coord) );
      iter = iter->next;
    }
  }
  return len;
}

gulong vik_track_get_tp_count(const VikTrack *tr)
{
  gulong num = 0;
  GList *iter = tr->trackpoints;
  while ( iter )
  {
    num++;
    iter = iter->next;
  }
  return num;
}

gulong vik_track_get_dup_point_count ( const VikTrack *tr )
{
  gulong num = 0;
  GList *iter = tr->trackpoints;
  while ( iter )
  {
    if ( iter->next && vik_coord_equals ( &(VIK_TRACKPOINT(iter->data)->coord),
                       &(VIK_TRACKPOINT(iter->next->data)->coord) ) )
      num++;
    iter = iter->next;
  }
  return num;
}

void vik_track_remove_dup_points ( VikTrack *tr )
{
  GList *iter = tr->trackpoints;
  while ( iter )
  {
    if ( iter->next && vik_coord_equals ( &(VIK_TRACKPOINT(iter->data)->coord),
                       &(VIK_TRACKPOINT(iter->next->data)->coord) ) )
    {
      g_free ( iter->next->data );
      tr->trackpoints = g_list_delete_link ( tr->trackpoints, iter->next );
    }
    else
      iter = iter->next;
  }
}

guint vik_track_get_segment_count(const VikTrack *tr)
{
  guint num = 1;
  GList *iter = tr->trackpoints;
  if ( !iter )
    return 0;
  while ( (iter = iter->next) )
  {
    if ( VIK_TRACKPOINT(iter->data)->newsegment )
      num++;
  }
  return num;
}

VikTrack **vik_track_split_into_segments(VikTrack *t, guint *ret_len)
{
  VikTrack **rv;
  VikTrack *tr;
  guint i;
  guint segs = vik_track_get_segment_count(t);
  GList *iter;

  if ( segs < 2 )
  {
    *ret_len = 0;
    return NULL;
  }

  rv = g_malloc ( segs * sizeof(VikTrack *) );
  tr = vik_track_copy ( t );
  rv[0] = tr;
  iter = tr->trackpoints;

  i = 1;
  while ( (iter = iter->next) )
  {
    if ( VIK_TRACKPOINT(iter->data)->newsegment )
    {
      iter->prev->next = NULL;
      iter->prev = NULL;
      rv[i] = vik_track_new();
      if ( tr->comment )
        vik_track_set_comment ( rv[i], tr->comment );
      rv[i]->visible = tr->visible;
      rv[i]->trackpoints = iter;
      i++;
    }
  }
  *ret_len = segs;
  return rv;
}

void vik_track_reverse ( VikTrack *tr )
{
  GList *iter;
  tr->trackpoints = g_list_reverse(tr->trackpoints);

  /* fix 'newsegment' */
  iter = g_list_last ( tr->trackpoints );
  while ( iter )
  {
    if ( ! iter->next ) /* last segment, was first, cancel newsegment. */
      VIK_TRACKPOINT(iter->data)->newsegment = FALSE;
    if ( ! iter->prev ) /* first segment by convention has newsegment flag. */
      VIK_TRACKPOINT(iter->data)->newsegment = TRUE;
    else if ( VIK_TRACKPOINT(iter->data)->newsegment && iter->next )
    {
      VIK_TRACKPOINT(iter->next->data)->newsegment = TRUE;
      VIK_TRACKPOINT(iter->data)->newsegment = FALSE;
    }
    iter = iter->prev;
  }
}

gdouble vik_track_get_average_speed(const VikTrack *tr)
{
  gdouble len = 0.0;
  guint32 time = 0;
  if ( tr->trackpoints )
  {
    GList *iter = tr->trackpoints->next;
    while (iter)
    {
      if ( VIK_TRACKPOINT(iter->data)->has_timestamp && 
          VIK_TRACKPOINT(iter->prev->data)->has_timestamp &&
          (! VIK_TRACKPOINT(iter->data)->newsegment) )
      {
        len += vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
                                &(VIK_TRACKPOINT(iter->prev->data)->coord) );
        time += ABS(VIK_TRACKPOINT(iter->data)->timestamp - VIK_TRACKPOINT(iter->prev->data)->timestamp);
      }
      iter = iter->next;
    }
  }
  return (time == 0) ? 0 : ABS(len/time);
}

gdouble vik_track_get_max_speed(const VikTrack *tr)
{
  gdouble maxspeed = 0.0, speed = 0.0;
  if ( tr->trackpoints )
  {
    GList *iter = tr->trackpoints->next;
    while (iter)
    {
      if ( VIK_TRACKPOINT(iter->data)->has_timestamp && 
          VIK_TRACKPOINT(iter->prev->data)->has_timestamp &&
          (! VIK_TRACKPOINT(iter->data)->newsegment) )
      {
        speed =  vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord), &(VIK_TRACKPOINT(iter->prev->data)->coord) )
                 / ABS(VIK_TRACKPOINT(iter->data)->timestamp - VIK_TRACKPOINT(iter->prev->data)->timestamp);
        if ( speed > maxspeed )
          maxspeed = speed;
      }
      iter = iter->next;
    }
  }
  return maxspeed;
}

void vik_track_convert ( VikTrack *tr, VikCoordMode dest_mode )
{
  GList *iter = tr->trackpoints;
  while (iter)
  {
    vik_coord_convert ( &(VIK_TRACKPOINT(iter->data)->coord), dest_mode );
    iter = iter->next;
  }
}

/* I understood this when I wrote it ... maybe ... Basically it eats up the
 * proper amounts of length on the track and averages elevation over that. */
gdouble *vik_track_make_elevation_map ( const VikTrack *tr, guint16 num_chunks )
{
  gdouble *pts;
  gdouble total_length, chunk_length, current_dist, current_area_under_curve, current_seg_length, dist_along_seg = 0.0;
  gdouble altitude1, altitude2;
  guint16 current_chunk;
  gboolean ignore_it = FALSE;

  GList *iter = tr->trackpoints;

  if (!iter || !iter->next) /* zero- or one-point track */
	  return NULL;

  { /* test if there's anything worth calculating */
    gboolean okay = FALSE;
    while ( iter )
    {
      if ( VIK_TRACKPOINT(iter->data)->altitude != VIK_DEFAULT_ALTITUDE ) {
        okay = TRUE; break;
      }
      iter = iter->next;
    }
    if ( ! okay )
      return NULL;
  }

  iter = tr->trackpoints;

  g_assert ( num_chunks < 16000 );

  pts = g_malloc ( sizeof(gdouble) * num_chunks );

  total_length = vik_track_get_length_including_gaps ( tr );
  chunk_length = total_length / num_chunks;

  /* Zero chunk_length (eg, track of 2 tp with the same loc) will cause crash */
  if (chunk_length <= 0) {
    g_free(pts);
    return NULL;
  }

  current_dist = 0.0;
  current_area_under_curve = 0;
  current_chunk = 0;
  current_seg_length = 0;

  current_seg_length = vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
      &(VIK_TRACKPOINT(iter->next->data)->coord) );
  altitude1 = VIK_TRACKPOINT(iter->data)->altitude;
  altitude2 = VIK_TRACKPOINT(iter->next->data)->altitude;
  dist_along_seg = 0;

  while ( current_chunk < num_chunks ) {

    /* go along current seg */
    if ( current_seg_length && (current_seg_length - dist_along_seg) > chunk_length ) {
      dist_along_seg += chunk_length;

      /*        /
       *   pt2 *
       *      /x       altitude = alt_at_pt_1 + alt_at_pt_2 / 2 = altitude1 + slope * dist_value_of_pt_inbetween_pt1_and_pt2
       *     /xx   avg altitude = area under curve / chunk len
       *pt1 *xxx   avg altitude = altitude1 + (altitude2-altitude1)/(current_seg_length)*(dist_along_seg + (chunk_len/2))
       *   / xxx
       *  /  xxx
       **/

      if ( ignore_it )
        pts[current_chunk] = VIK_DEFAULT_ALTITUDE;
      else
        pts[current_chunk] = altitude1 + (altitude2-altitude1)*((dist_along_seg - (chunk_length/2))/current_seg_length);

      current_chunk++;
    } else {
      /* finish current seg */
      if ( current_seg_length ) {
        gdouble altitude_at_dist_along_seg = altitude1 + (altitude2-altitude1)/(current_seg_length)*dist_along_seg;
        current_dist = current_seg_length - dist_along_seg;
        current_area_under_curve = current_dist*(altitude_at_dist_along_seg + altitude2)*0.5;
      } else { current_dist = current_area_under_curve = 0; } /* should only happen if first current_seg_length == 0 */

      /* get intervening segs */
      iter = iter->next;
      while ( iter && iter->next ) {
        current_seg_length = vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
            &(VIK_TRACKPOINT(iter->next->data)->coord) );
        altitude1 = VIK_TRACKPOINT(iter->data)->altitude;
        altitude2 = VIK_TRACKPOINT(iter->next->data)->altitude;
        ignore_it = VIK_TRACKPOINT(iter->next->data)->newsegment;

        if ( chunk_length - current_dist >= current_seg_length ) {
          current_dist += current_seg_length;
          current_area_under_curve += current_seg_length * (altitude1+altitude2) * 0.5;
          iter = iter->next;
        } else {
          break;
        }
      }

      /* final seg */
      dist_along_seg = chunk_length - current_dist;
      if ( ignore_it || !iter->next ) {
        pts[current_chunk] = current_area_under_curve / current_dist;
        if (!iter->next) {
          int i;
          for (i = current_chunk + 1; i < num_chunks; i++)
            pts[i] = pts[current_chunk];
          break;
        }
      } 
      else {
        current_area_under_curve += dist_along_seg * (altitude1 + (altitude2 - altitude1)*dist_along_seg/current_seg_length);
        pts[current_chunk] = current_area_under_curve / chunk_length;
      }

      current_dist = 0;
      current_chunk++;
    }
  }

  return pts;
}


void vik_track_get_total_elevation_gain(const VikTrack *tr, gdouble *up, gdouble *down)
{
  gdouble diff;
  *up = *down = 0;
  if ( tr->trackpoints && VIK_TRACKPOINT(tr->trackpoints->data)->altitude != VIK_DEFAULT_ALTITUDE )
  {
    GList *iter = tr->trackpoints->next;
    while (iter)
    {
      diff = VIK_TRACKPOINT(iter->data)->altitude - VIK_TRACKPOINT(iter->prev->data)->altitude;
      if ( diff > 0 )
        *up += diff;
      else
        *down -= diff;
      iter = iter->next;
    }
  } else
    *up = *down = VIK_DEFAULT_ALTITUDE;
}


/* by Alex Foobarian */
gdouble *vik_track_make_speed_map ( const VikTrack *tr, guint16 num_chunks )
{
  gdouble *v, *s, *t;
  gdouble duration, chunk_dur;
  time_t t1, t2;
  int i, pt_count, numpts, index;
  GList *iter;

  if ( ! tr->trackpoints )
    return NULL;

  g_assert ( num_chunks < 16000 );

  t1 = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  t2 = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;
  duration = t2 - t1;

  if ( !t1 || !t2 || !duration )
    return NULL;

  if (duration < 0) {
    g_warning("negative duration: unsorted trackpoint timestamps?");
    return NULL;
  }
  pt_count = vik_track_get_tp_count(tr);

  v = g_malloc ( sizeof(gdouble) * num_chunks );
  chunk_dur = duration / num_chunks;

  s = g_malloc(sizeof(double) * pt_count);
  t = g_malloc(sizeof(double) * pt_count);

  iter = tr->trackpoints->next;
  numpts = 0;
  s[0] = 0;
  t[0] = VIK_TRACKPOINT(iter->prev->data)->timestamp;
  numpts++;
  while (iter) {
    s[numpts] = s[numpts-1] + vik_coord_diff ( &(VIK_TRACKPOINT(iter->prev->data)->coord), &(VIK_TRACKPOINT(iter->data)->coord) );
    t[numpts] = VIK_TRACKPOINT(iter->data)->timestamp;
    numpts++;
    iter = iter->next;
  }

  /* In the following computation, we iterate through periods of time of duration chunk_dur.
   * The first period begins at the beginning of the track.  The last period ends at the end of the track.
   */
  index = 0; /* index of the current trackpoint. */
  for (i = 0; i < num_chunks; i++) {
    /* we are now covering the interval from t[0] + i*chunk_dur to t[0] + (i+1)*chunk_dur.
     * find the first trackpoint outside the current interval, averaging the speeds between intermediate trackpoints.
     */
    if (t[0] + i*chunk_dur >= t[index]) {
      gdouble acc_t = 0, acc_s = 0;
      numpts = 0;
      while (t[0] + i*chunk_dur >= t[index]) {
	acc_s += (s[index+1]-s[index]);
	acc_t += (t[index+1]-t[index]);
	index++;
	numpts++;
      }
      v[i] = acc_s/acc_t;
    } 
    else if (i) {
      v[i] = v[i-1];
    }
    else {
      v[i] = 0;
    }
  }
  g_free(s);
  g_free(t);
  return v;
}

/* by Alex Foobarian */
VikTrackpoint *vik_track_get_closest_tp_by_percentage_dist ( VikTrack *tr, gdouble reldist, gdouble *meters_from_start )
{
  gdouble dist = vik_track_get_length_including_gaps(tr) * reldist;
  gdouble current_dist = 0.0;
  gdouble current_inc = 0.0;
  if ( tr->trackpoints )
  {
    GList *iter = tr->trackpoints->next;
    GList *last_iter = NULL;
    gdouble last_dist = 0.0;
    while (iter)
    {
      current_inc = vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
                                     &(VIK_TRACKPOINT(iter->prev->data)->coord) );
      last_dist = current_dist;
      current_dist += current_inc;
      if ( current_dist >= dist )
        break;
      last_iter = iter;
      iter = iter->next;
    }
    if (!iter) { /* passing the end the track */
      if (last_iter) {
        if (meters_from_start)
          *meters_from_start = last_dist;
        return(VIK_TRACKPOINT(last_iter->data));
      }
      else
        return NULL;
    }
    /* we've gone past the dist already, was prev trackpoint closer? */
    /* should do a vik_coord_average_weighted() thingy. */
    if ( iter->prev && abs(current_dist-current_inc-dist) < abs(current_dist-dist) ) {
      if (meters_from_start)
        *meters_from_start = last_dist;
      iter = iter->prev;
    }
    else
      if (meters_from_start)
        *meters_from_start = current_dist;

    return VIK_TRACKPOINT(iter->data);

  }
  return NULL;
}

VikTrackpoint *vik_track_get_closest_tp_by_percentage_time ( VikTrack *tr, gdouble reltime, time_t *seconds_from_start )
{
  time_t t_pos, t_start, t_end, t_total;
  t_start = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  t_end = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;
  t_total = t_end - t_start;

  t_pos = t_start + t_total * reltime;

  if ( !tr->trackpoints )
    return NULL;

  GList *iter = tr->trackpoints;

  while (iter) {
    if (VIK_TRACKPOINT(iter->data)->timestamp == t_pos)
      break;
    if (VIK_TRACKPOINT(iter->data)->timestamp > t_pos) {
      if (iter->prev == NULL)  /* first trackpoint */
        break;
      time_t t_before = t_pos - VIK_TRACKPOINT(iter->prev)->timestamp;
      time_t t_after = VIK_TRACKPOINT(iter->data)->timestamp - t_pos;
      if (t_before <= t_after)
        iter = iter->prev;
      break;
    }
    else if ((iter->next == NULL) && (t_pos < (VIK_TRACKPOINT(iter->data)->timestamp + 3))) /* last trackpoint: accommodate for round-off */
      break;
    iter = iter->next;
  }

  if (!iter)
    return NULL;
  if (seconds_from_start)
    *seconds_from_start = VIK_TRACKPOINT(iter->data)->timestamp - VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  return VIK_TRACKPOINT(iter->data);
}

VikTrackpoint* vik_track_get_tp_by_max_speed ( const VikTrack *tr )
{
  gdouble maxspeed = 0.0, speed = 0.0;

  if ( !tr->trackpoints )
    return NULL;

  GList *iter = tr->trackpoints;
  VikTrackpoint *max_speed_tp = NULL;

  while (iter) {
    if (iter->prev) {
      if ( VIK_TRACKPOINT(iter->data)->has_timestamp &&
	   VIK_TRACKPOINT(iter->prev->data)->has_timestamp &&
	   (! VIK_TRACKPOINT(iter->data)->newsegment) ) {
	speed =  vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord), &(VIK_TRACKPOINT(iter->prev->data)->coord) )
	  / ABS(VIK_TRACKPOINT(iter->data)->timestamp - VIK_TRACKPOINT(iter->prev->data)->timestamp);
	if ( speed > maxspeed ) {
	  maxspeed = speed;
	  max_speed_tp = VIK_TRACKPOINT(iter->data);
	}
      }
    }
    iter = iter->next;
  }
  
  if (!max_speed_tp)
    return NULL;

  return max_speed_tp;
}

VikTrackpoint* vik_track_get_tp_by_max_alt ( const VikTrack *tr )
{
  gdouble maxalt = -5000.0;
  if ( !tr->trackpoints )
    return NULL;

  GList *iter = tr->trackpoints;
  VikTrackpoint *max_alt_tp = NULL;

  while (iter) {
    if ( VIK_TRACKPOINT(iter->data)->altitude > maxalt ) {
      maxalt = VIK_TRACKPOINT(iter->data)->altitude;
      max_alt_tp = VIK_TRACKPOINT(iter->data);
    }
    iter = iter->next;
  }

  if (!max_alt_tp)
    return NULL;

  return max_alt_tp;
}

VikTrackpoint* vik_track_get_tp_by_min_alt ( const VikTrack *tr )
{
  gdouble minalt = 25000.0;
  if ( !tr->trackpoints )
    return NULL;

  GList *iter = tr->trackpoints;
  VikTrackpoint *min_alt_tp = NULL;

  while (iter) {
    if ( VIK_TRACKPOINT(iter->data)->altitude < minalt ) {
      minalt = VIK_TRACKPOINT(iter->data)->altitude;
      min_alt_tp = VIK_TRACKPOINT(iter->data);
    }
    iter = iter->next;
  }

  if (!min_alt_tp)
    return NULL;

  return min_alt_tp;
}

gboolean vik_track_get_minmax_alt ( const VikTrack *tr, gdouble *min_alt, gdouble *max_alt )
{
  *min_alt = 25000;
  *max_alt = -5000;
  if ( tr && tr->trackpoints && tr->trackpoints->data && (VIK_TRACKPOINT(tr->trackpoints->data)->altitude != VIK_DEFAULT_ALTITUDE) ) {
    GList *iter = tr->trackpoints->next;
    gdouble tmp_alt;
    while (iter)
    {
      tmp_alt = VIK_TRACKPOINT(iter->data)->altitude;
      if ( tmp_alt > *max_alt )
        *max_alt = tmp_alt;
      if ( tmp_alt < *min_alt )
        *min_alt = tmp_alt;
      iter = iter->next;
    }
    return TRUE;
  }
  return FALSE;
}

void vik_track_marshall ( VikTrack *tr, guint8 **data, guint *datalen)
{
  GList *tps;
  GByteArray *b = g_byte_array_new();
  guint len;
  guint intp, ntp;

  g_byte_array_append(b, (guint8 *)tr, sizeof(*tr));

  /* we'll fill out number of trackpoints later */
  intp = b->len;
  g_byte_array_append(b, (guint8 *)&len, sizeof(len));

  tps = tr->trackpoints;
  ntp = 0;
  while (tps) {
    g_byte_array_append(b, (guint8 *)tps->data, sizeof(VikTrackpoint));
    tps = tps->next;
    ntp++;
  }
  *(guint *)(b->data + intp) = ntp;

  len = (tr->comment) ? strlen(tr->comment)+1 : 0; 
  g_byte_array_append(b, (guint8 *)&len, sizeof(len)); 
  if (tr->comment) g_byte_array_append(b, (guint8 *)tr->comment, len);

  *data = b->data;
  *datalen = b->len;
  g_byte_array_free(b, FALSE);
}

VikTrack *vik_track_unmarshall (guint8 *data, guint datalen)
{
  guint len;
  VikTrack *new_tr = vik_track_new();
  VikTrackpoint *new_tp;
  guint ntp;
  gint i;

  /* only the visibility is needed */
  new_tr->visible = ((VikTrack *)data)->visible;
  data += sizeof(*new_tr);

  ntp = *(guint *)data;
  data += sizeof(ntp);

  for (i=0; i<ntp; i++) {
    new_tp = vik_trackpoint_new();
    memcpy(new_tp, data, sizeof(*new_tp));
    data += sizeof(*new_tp);
    new_tr->trackpoints = g_list_append(new_tr->trackpoints, new_tp);
  }

  len = *(guint *)data;
  data += sizeof(len);
  if (len) {
    new_tr->comment = g_strdup((gchar *)data);
  }
  return new_tr;
}

void vik_track_apply_dem_data ( VikTrack *tr )
{
  GList *tp_iter;
  gint16 elev;
  tp_iter = tr->trackpoints;
  while ( tp_iter ) {
    /* TODO: of the 4 possible choices we have for choosing an elevation
     * (trackpoint in between samples), choose the one with the least elevation change
     * as the last */
    elev = a_dems_get_elev_by_coord ( &(VIK_TRACKPOINT(tp_iter->data)->coord), VIK_DEM_INTERPOL_BEST );
    if ( elev != VIK_DEM_INVALID_ELEVATION )
      VIK_TRACKPOINT(tp_iter->data)->altitude = elev;
    tp_iter = tp_iter->next;
  }
}

/* appends t2 to t1, leaving t2 with no trackpoints */
void vik_track_steal_and_append_trackpoints ( VikTrack *t1, VikTrack *t2 )
{
  if ( t1->trackpoints ) {
    GList *tpiter = t1->trackpoints;
    while ( tpiter->next )
      tpiter = tpiter->next;
    tpiter->next = t2->trackpoints;
    t2->trackpoints->prev = tpiter;
  } else
    t1->trackpoints = t2->trackpoints;
  t2->trackpoints = NULL;
}

/* starting at the end, looks backwards for the last "double point", a duplicate trackpoint.
 * this is indicative of magic scissors continued use. If there is no double point,
 * deletes all the trackpoints. Returns the new end of the track (or the start if
 * there are no double points
 */
VikCoord *vik_track_cut_back_to_double_point ( VikTrack *tr )
{
  GList *iter = tr->trackpoints;
  VikCoord *rv;

  if ( !iter )
    return NULL;
  while ( iter->next )
    iter = iter->next;


  while ( iter->prev ) {
    if ( vik_coord_equals((VikCoord *)iter->data, (VikCoord *)iter->prev->data) ) {
      GList *prev = iter->prev;

      rv = g_malloc(sizeof(VikCoord));
      *rv = *((VikCoord *) iter->data);

      /* truncate trackpoint list */
      iter->prev = NULL; /* pretend it's the end */
      g_list_foreach ( iter, (GFunc) g_free, NULL );
      g_list_free( iter );

      prev->next = NULL;

      return rv;
    }
    iter = iter->prev;
  }

  /* no double point found! */
  rv = g_malloc(sizeof(VikCoord));
  *rv = *((VikCoord *) tr->trackpoints->data);
  g_list_foreach ( tr->trackpoints, (GFunc) g_free, NULL );
  g_list_free( tr->trackpoints );
  tr->trackpoints = NULL;
  return rv;
}

