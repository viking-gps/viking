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

#include <glib.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "coords.h"
#include "vikcoord.h"
#include "viktrack.h"
#include "globals.h"

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

void vik_track_free(VikTrack *tr)
{
  if ( tr->ref_count-- > 1 )
    return;

  if ( tr->comment )
    g_free ( tr->comment );
  g_list_foreach ( tr->trackpoints, (GFunc) g_free, NULL );
  g_list_free( tr->trackpoints );
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
  return g_malloc0(sizeof(VikTrackpoint));
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



  g_assert ( num_chunks < 16000 );

  pts = g_malloc ( sizeof(gdouble) * num_chunks );

  total_length = vik_track_get_length_including_gaps ( tr );
  chunk_length = total_length / num_chunks;

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

typedef struct {
  double a, b, c, d;
} spline_coeff_t;

void compute_spline(int n, double *x, double *f, spline_coeff_t *p) 
{
  double *h, *alpha, *B, *m;
  int i;
  int orig_n = n;
  double new_x[3], new_f[3];
  
  if (n==0) return;
  if (n==1) {
    new_x[0] = x[0];
    new_f[0] = f[0];
    new_x[1] = x[0]+0.00001;
    new_f[1] = f[0];
    x = new_x;
    f = new_f;
    n = 3;
  }
  if (n==2) {
    new_x[0] = x[0];
    new_f[0] = f[0];
    new_x[1] = x[1];
    new_f[1] = f[1];
    new_x[2] = x[1] + x[1]-x[0];
    new_f[2] = f[1] + f[1]-f[0];
    x = new_x;
    f = new_f;
    n = 3;
  }
  
  /* we're solving a linear system of equations of the form Ax = B. 
   * The matrix a is tridiagonal and consists of coefficients in 
   * the h[] and alpha[] arrays. 
   */

  h = (double *)malloc(sizeof(double) * (n-1));
  for (i=0; i<n-1; i++) {
    h[i] = x[i+1]-x[i];
  }

  alpha = (double *)malloc(sizeof(double) * (n-2));
  for (i=0; i<n-2; i++) {
    alpha[i] = 2 * (h[i] + h[i+1]);
  }

  /* B[] is the vector on the right hand side of the equation */
  B = (double *)malloc(sizeof(double) * (n-2));
  for (i=0; i<n-2; i++) {
    B[i] = 6 * ((f[i+2] - f[i+1])/h[i+1] - (f[i+1] - f[i])/h[i]);
  }

  /* Now solve the n-2 by n-2 system */
  m = (double *)malloc(sizeof(double) * (n-2));
  for (i=1; i<=n-3; i++) {
    /*
    d0 = alpha 0   
    a0 = h1          
    c0 = h1          

      di = di - (ai-1 / di-1) * ci-1
      bi = bi - (ai-1 / di-1) * bi-1
      ;
    */
    alpha[i] = alpha[i] - (h[i]/alpha[i-1]) * h[i];
    B[i] = B[i] - (h[i]/alpha[i-1]) * B[i-1];
  }
  /*  xn-3 = bn-3 / dn-3; */
  m[n-3] = B[n-3]/alpha[n-3];
  for (i=n-4; i>=0; i--) {
    m[i] = (B[i]-h[i+1]*m[i+1])/alpha[i];
  }

  for (i=0; i<orig_n-1; i++) {
    double mi, mi1;
    mi = (i==(n-2)) ? 0 : m[i];
    mi1 = (i==0) ? 0 : m[i-1];

    p[i].a = f[i+1];
    p[i].b = (f[i+1] - f[i]) / h[i]  +  h[i] * (2*mi + mi1) / 6;
    p[i].c = mi/2;
    p[i].d = (mi-mi1)/(6*h[i]);
  }

  free(alpha);
  free(B);
  free(h);
  free(m);
}

/* by Alex Foobarian */
gdouble *vik_track_make_speed_map ( const VikTrack *tr, guint16 num_chunks )
{
  gdouble *v, *s, *t;
  gdouble duration, chunk_dur, T, s_prev, s_now;
  time_t t1, t2;
  int i, pt_count, numpts, spline;
  GList *iter;
  spline_coeff_t *p;
  GList *mytr;

  if ( ! tr->trackpoints )
    return NULL;

  g_assert ( num_chunks < 16000 );

#ifdef XXXXXXXXXXXXXXXXXX
  iter = tr->trackpoints;
  while (iter) {
    
  }
#endif /*XXXXXXXXXXXXXXXXXX*/

  t1 = VIK_TRACKPOINT(tr->trackpoints->data)->timestamp;
  t2 = VIK_TRACKPOINT(g_list_last(tr->trackpoints)->data)->timestamp;
  duration = t2 - t1;

  if ( !t1 || !t2 || !duration )
    return NULL;

  if (duration < 0) {
    fprintf(stderr, "negative duration: unsorted trackpoint timestamps?\n");
    return NULL;
  }
  pt_count = vik_track_get_tp_count(tr);

  v = g_malloc ( sizeof(gdouble) * num_chunks );
  chunk_dur = duration / num_chunks;

  s = g_malloc(sizeof(double) * pt_count);
  t = g_malloc(sizeof(double) * pt_count);
  p = g_malloc(sizeof(spline_coeff_t) * (pt_count-1));

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

  compute_spline(numpts, t, s, p);

  /*
  printf("Got spline\n");
  for (i=0; i<numpts-1; i++) {
    printf("a = %15f  b = %15f  c = %15f  d = %15f\n", p[i].a, p[i].b, p[i].c, p[i].d);
  }
  */

  /* the spline gives us distances at chunk_dur intervals. from these,
   * we obtain average speed in each interval.
   */
  spline = 0;
  T = t[spline];
  s_prev = 
      p[spline].d * pow(T - t[spline+1], 3) + 
      p[spline].c * pow(T - t[spline+1], 2) + 
      p[spline].b * (T - t[spline+1]) + 
      p[spline].a;
  for (i = 0; i < num_chunks; i++, T+=chunk_dur) {
    while (T > t[spline+1]) {
      spline++;
    }
    s_now = 
      p[spline].d * pow(T - t[spline+1], 3) + 
      p[spline].c * pow(T - t[spline+1], 2) + 
      p[spline].b * (T - t[spline+1]) + 
      p[spline].a;
    v[i] = (s_now - s_prev) / chunk_dur;
    s_prev = s_now;
    /* 
     * old method of averages
    v[i] = (s[spline+1]-s[spline])/(t[spline+1]-t[spline]);
    */
  }
  g_free(s);
  g_free(t);
  g_free(p);
  return v;
}

/* by Alex Foobarian */
VikTrackpoint *vik_track_get_closest_tp_by_percentage_dist ( VikTrack *tr, gdouble reldist )
{
  gdouble dist = vik_track_get_length_including_gaps(tr) * reldist;
  gdouble current_dist = 0.0;
  gdouble current_inc = 0.0;
  if ( tr->trackpoints )
  {
    GList *iter = tr->trackpoints->next;
    while (iter)
    {
      current_inc = vik_coord_diff ( &(VIK_TRACKPOINT(iter->data)->coord),
                                     &(VIK_TRACKPOINT(iter->prev->data)->coord) );
      current_dist += current_inc;
      if ( current_dist >= dist )
        break;
      iter = iter->next;
    }
    /* we've gone past the dist already, was prev trackpoint closer? */
    /* should do a vik_coord_average_weighted() thingy. */
    if ( iter->prev && abs(current_dist-current_inc-dist) < abs(current_dist-dist) )
      iter = iter->prev;

    return VIK_TRACKPOINT(iter->data);

  }
  return NULL;
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

