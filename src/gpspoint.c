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

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "viking.h"

#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdlib.h>
/* strtod */


static void a_gpspoint_write_track ( const gchar *name, VikTrack *t, FILE *f );
static void a_gpspoint_write_trackpoint ( VikTrackpoint *tp, FILE *f );
static void a_gpspoint_write_waypoint ( const gchar *name, VikWaypoint *wp, FILE *f );


/* outline for file gpspoint.c

reading file:

take a line.
get first tag, if not type, skip it.
if type, record type.  if waypoint list, etc move on. if track, make a new track, make it current track, add it, etc.
if waypoint, read on and store to the waypoint.
if trackpoint, make trackpoint, store to current track (error / skip if none)

*/

/* Thanks to etrex-cache's gpsbabel's gpspoint.c for starting me off! */

static char line_buffer[2048];

#define GPSPOINT_TYPE_NONE 0
#define GPSPOINT_TYPE_WAYPOINT 1
#define GPSPOINT_TYPE_TRACKPOINT 2
/* #define GPSPOINT_TYPE_ROUTEPOINT 3 */
#define GPSPOINT_TYPE_TRACK 4

/* #define GPSPOINT_TYPE_ROUTE 5 */

static VikTrack *current_track; /* pointer to pointer to first GList */

static gint line_type = GPSPOINT_TYPE_NONE;
static struct LatLon line_latlon;
static gchar *line_name;
static gchar *line_comment;
static gchar *line_image;
static gchar *line_symbol;
static gboolean line_newsegment = FALSE;
static gboolean line_has_timestamp = FALSE;
static time_t line_timestamp = 0;
static gdouble line_altitude = VIK_DEFAULT_ALTITUDE;
static gboolean line_visible = TRUE;

static gboolean line_extended = FALSE;
static gdouble line_speed = NAN;
static gdouble line_course = NAN;
static gint line_sat = 0;
static gint line_fix = 0;
/* other possible properties go here */


static void gpspoint_process_tag ( const gchar *tag, gint len );
static void gpspoint_process_key_and_value ( const gchar *key, gint key_len, const gchar *value, gint value_len );

static gchar *slashdup(const gchar *str)
{
  guint16 len = strlen(str);
  guint16 need_bs_count, i, j;
  gchar *rv;
  for ( i = 0, need_bs_count = 0; i < len; i++ )
    if ( str[i] == '\\' || str[i] == '"' )
      need_bs_count++;
  rv = g_malloc ( (len+need_bs_count+1) * sizeof(gchar) );
  for ( i = 0, j = 0; i < len; i++, j++ )
  {
    if ( str[i] == '\\' || str[i] == '"' )
      rv[j++] = '\\';
    rv[j] = str[i];
  }
  rv[j] = '\0';
  return rv;
}

static gchar *deslashndup ( const gchar *str, guint16 len )
{
  guint16 i,j, bs_count, new_len;
  gboolean backslash = FALSE;
  gchar *rv;

  if ( len < 1 )
    return NULL;

  for ( i = 0, bs_count = 0; i < len; i++ )
   if ( str[i] == '\\' )
   {
     bs_count++;
     i++;
   }

  if ( str[i-1] == '\\' && (len == 1 || str[i-2] != '\\') )
    bs_count--;

  new_len = len - bs_count;
  rv = g_malloc ( (new_len+1) * sizeof(gchar) );
  for ( i = 0, j = 0; i < len && j < new_len; i++ )
    if ( str[i] == '\\' && !backslash )
      backslash = TRUE;
    else
    {
      rv[j++] = str[i];
      backslash = FALSE;
    }

  rv[new_len] = '\0';
  return rv;
}

void a_gpspoint_read_file(VikTrwLayer *trw, FILE *f ) {
  VikCoordMode coord_mode = vik_trw_layer_get_coord_mode ( trw );
  gchar *tag_start, *tag_end;
  g_assert ( f != NULL && trw != NULL );
  line_type = 0;
  line_timestamp = 0;
  line_newsegment = FALSE;
  line_image = NULL;
  line_symbol = NULL;

  current_track = NULL;
  while (fgets(line_buffer, 2048, f))
  {
    gboolean inside_quote = 0;
    gboolean backslash = 0;

    line_buffer[strlen(line_buffer)-1] = '\0'; /* chop off newline */

    /* for gpspoint files wrapped inside */
    if ( strlen(line_buffer) >= 13 && strncmp ( line_buffer, "~EndLayerData", 13 ) == 0 )
      break;

/* each line: nullify stuff, make thing if nes, free name if ness */
    tag_start = line_buffer;
    for (;;)
    {
      /* my addition: find first non-whitespace character. if the null, skip line. */
      while (*tag_start != '\0' && isspace(*tag_start))
        tag_start++;
      if (tag_start == '\0')
        break;

      if (*tag_start == '#')
        break;

      tag_end = tag_start;
        if (*tag_end == '"')
          inside_quote = !inside_quote;
      while (*tag_end != '\0' && (!isspace(*tag_end) || inside_quote)) {
        tag_end++;
        if (*tag_end == '\\' && !backslash)
          backslash = TRUE;
        else if (backslash)
          backslash = FALSE;
        else if (*tag_end == '"')
          inside_quote = !inside_quote;
      }

      gpspoint_process_tag ( tag_start, tag_end - tag_start );

      if (*tag_end == '\0' )
        break;
      else
        tag_start = tag_end+1;
    }
    if (line_type == GPSPOINT_TYPE_WAYPOINT && line_name)
    {
      VikWaypoint *wp = vik_waypoint_new();
      wp->visible = line_visible;
      wp->altitude = line_altitude;

      vik_coord_load_from_latlon ( &(wp->coord), coord_mode, &line_latlon );

      vik_trw_layer_filein_add_waypoint ( trw, line_name, wp );
      g_free ( line_name );
      line_name = NULL;

      if ( line_comment )
      {
        vik_waypoint_set_comment ( wp, line_comment );
        line_comment = NULL;
      }

      if ( line_image )
      {
        vik_waypoint_set_image ( wp, line_image );
        line_image = NULL;
      }

      if ( line_symbol )
      {
        vik_waypoint_set_symbol ( wp, line_symbol );
	line_symbol = NULL;
      }
    }
    else if (line_type == GPSPOINT_TYPE_TRACK && line_name)
    {
      VikTrack *pl = vik_track_new();

      /* Thanks to Peter Jones for this Fix */
      if (!line_name) line_name = g_strdup("UNK");

      pl->visible = line_visible;

      if ( line_comment )
      {
        vik_track_set_comment ( pl, line_comment );
        line_comment = NULL;
      }

      pl->trackpoints = NULL;
      vik_trw_layer_filein_add_track ( trw, line_name, pl );
      g_free ( line_name );
      line_name = NULL;

      current_track = pl;
    }
    else if (line_type == GPSPOINT_TYPE_TRACKPOINT && current_track)
    {
      VikTrackpoint *tp = vik_trackpoint_new();
      vik_coord_load_from_latlon ( &(tp->coord), coord_mode, &line_latlon );
      tp->newsegment = line_newsegment;
      tp->has_timestamp = line_has_timestamp;
      tp->timestamp = line_timestamp;
      tp->altitude = line_altitude;
      if (line_extended) {
        tp->extended = TRUE;
        tp->speed = line_speed;
        tp->course = line_course;
        tp->nsats = line_sat;
        tp->fix_mode = line_fix;
      }
      else {
        tp->extended = FALSE;
      }
      current_track->trackpoints = g_list_append ( current_track->trackpoints, tp );
    }

    if (line_name) 
      g_free ( line_name );
    line_name = NULL;
    if (line_comment) 
      g_free ( line_comment );
    if (line_image)
      g_free ( line_image );
    if (line_symbol)
      g_free ( line_symbol );
    line_comment = NULL;
    line_image = NULL;
    line_symbol = NULL;
    line_type = GPSPOINT_TYPE_NONE;
    line_newsegment = FALSE;
    line_has_timestamp = FALSE;
    line_timestamp = 0;
    line_altitude = VIK_DEFAULT_ALTITUDE;
    line_visible = TRUE;
    line_symbol = NULL;

    line_extended = FALSE;
    line_speed = NAN;
    line_course = NAN;
    line_sat = 0;
    line_fix = 0;
  }
}

/* Tag will be of a few defined forms:
   ^[:alpha:]*=".*"$
   ^[:alpha:]*=.*$

   <invalid tag>

So we must determine end of tag name, start of value, end of value.
*/
static void gpspoint_process_tag ( const gchar *tag, gint len )
{
  const gchar *key_end, *value_start, *value_end;

  /* Searching for key end */
  key_end = tag;

  while (++key_end - tag < len)
    if (*key_end == '=')
      break;

  if (key_end - tag == len)
    return; /* no good */

  if (key_end - tag == len + 1)
    value_start = value_end = 0; /* size = 0 */
  else
  {
    value_start = key_end + 1; /* equal_sign plus one */

    if (*value_start == '"')
    {
      value_start++;
      if (*value_start == '"')
        value_start = value_end = 0; /* size = 0 */
      else
      {
        if (*(tag+len-1) == '"')
        value_end = tag + len - 1;
        else
          return; /* bogus */
      }
    }
    else
      value_end = tag + len; /* value start really IS value start. */

    gpspoint_process_key_and_value(tag, key_end - tag, value_start, value_end - value_start);
  }
}

/*
value = NULL for none
*/
static void gpspoint_process_key_and_value ( const gchar *key, gint key_len, const gchar *value, gint value_len )
{
  if (key_len == 4 && strncasecmp( key, "type", key_len ) == 0 )
  {
    if (value == NULL)
      line_type = GPSPOINT_TYPE_NONE;
    else if (value_len == 5 && strncasecmp( value, "track", value_len ) == 0 )
      line_type = GPSPOINT_TYPE_TRACK;
    else if (value_len == 10 && strncasecmp( value, "trackpoint", value_len ) == 0 )
      line_type = GPSPOINT_TYPE_TRACKPOINT;
    else if (value_len == 8 && strncasecmp( value, "waypoint", value_len ) == 0 )
      line_type = GPSPOINT_TYPE_WAYPOINT;
    else
      /* all others are ignored */
      line_type = GPSPOINT_TYPE_NONE;
  }
  else if (key_len == 4 && strncasecmp( key, "name", key_len ) == 0 && value != NULL)
  {
    if (line_name == NULL)
    {
      line_name = g_strndup ( value, value_len );
    }
  }
  else if (key_len == 7 && strncasecmp( key, "comment", key_len ) == 0 && value != NULL)
  {
    if (line_comment == NULL)
      line_comment = deslashndup ( value, value_len );
  }
  else if (key_len == 5 && strncasecmp( key, "image", key_len ) == 0 && value != NULL)
  {
    if (line_image == NULL)
      line_image = deslashndup ( value, value_len );
  }
  else if (key_len == 8 && strncasecmp( key, "latitude", key_len ) == 0 && value != NULL)
  {
    line_latlon.lat = g_strtod(value, NULL);
  }
  else if (key_len == 9 && strncasecmp( key, "longitude", key_len ) == 0 && value != NULL)
  {
    line_latlon.lon = g_strtod(value, NULL);
  }
  else if (key_len == 8 && strncasecmp( key, "altitude", key_len ) == 0 && value != NULL)
  {
    line_altitude = g_strtod(value, NULL);
  }
  else if (key_len == 7 && strncasecmp( key, "visible", key_len ) == 0 && value[0] != 'y' && value[0] != 'Y' && value[0] != 't' && value[0] != 'T')
  {
    line_visible = FALSE;
  }
  else if (key_len == 6 && strncasecmp( key, "symbol", key_len ) == 0 && value != NULL)
  {
    line_symbol = g_strndup ( value, value_len );
  }
  else if (key_len == 8 && strncasecmp( key, "unixtime", key_len ) == 0 && value != NULL)
  {
    line_timestamp = g_strtod(value, NULL);
    if ( line_timestamp != 0x80000000 )
      line_has_timestamp = TRUE;
  }
  else if (key_len == 10 && strncasecmp( key, "newsegment", key_len ) == 0 && value != NULL)
  {
    line_newsegment = TRUE;
  }
  else if (key_len == 8 && strncasecmp( key, "extended", key_len ) == 0 && value != NULL)
  {
    line_extended = TRUE;
  }
  else if (key_len == 5 && strncasecmp( key, "speed", key_len ) == 0 && value != NULL)
  {
    line_speed = g_strtod(value, NULL);
  }
  else if (key_len == 6 && strncasecmp( key, "course", key_len ) == 0 && value != NULL)
  {
    line_course = g_strtod(value, NULL);
  }
  else if (key_len == 3 && strncasecmp( key, "sat", key_len ) == 0 && value != NULL)
  {
    line_sat = atoi(value);
  }
  else if (key_len == 3 && strncasecmp( key, "fix", key_len ) == 0 && value != NULL)
  {
    line_fix = atoi(value);
  }
}

static void a_gpspoint_write_waypoint ( const gchar *name, VikWaypoint *wp, FILE *f )
{
  static struct LatLon ll;
  gchar *s_lat, *s_lon;
  vik_coord_to_latlon ( &(wp->coord), &ll );
  s_lat = a_coords_dtostr(ll.lat);
  s_lon = a_coords_dtostr(ll.lon);
  fprintf ( f, "type=\"waypoint\" latitude=\"%s\" longitude=\"%s\" name=\"%s\"", s_lat, s_lon, name );
  g_free ( s_lat ); 
  g_free ( s_lon );

  if ( wp->altitude != VIK_DEFAULT_ALTITUDE ) {
    gchar *s_alt = a_coords_dtostr(wp->altitude);
    fprintf ( f, " altitude=\"%s\"", s_alt );
    g_free(s_alt);
  }
  if ( wp->comment )
  {
    gchar *tmp_comment = slashdup(wp->comment);
    fprintf ( f, " comment=\"%s\"", tmp_comment );
    g_free ( tmp_comment );
  }
  if ( wp->image )
  {
    gchar *tmp_image = slashdup(wp->image);
    fprintf ( f, " image=\"%s\"", tmp_image );
    g_free ( tmp_image );
  }
  if ( wp->symbol )
  {
    fprintf ( f, " symbol=\"%s\"", wp->symbol );
  }
  if ( ! wp->visible )
    fprintf ( f, " visible=\"n\"" );
  fprintf ( f, "\n" );
}

static void a_gpspoint_write_trackpoint ( VikTrackpoint *tp, FILE *f )
{
  static struct LatLon ll;
  gchar *s_lat, *s_lon;
  vik_coord_to_latlon ( &(tp->coord), &ll );

  /* TODO: modify a_coords_dtostr() to accept (optional) buffer
   * instead of doing malloc/free everytime */
  s_lat = a_coords_dtostr(ll.lat);
  s_lon = a_coords_dtostr(ll.lon);
  fprintf ( f, "type=\"trackpoint\" latitude=\"%s\" longitude=\"%s\"", s_lat, s_lon );
  g_free ( s_lat ); 
  g_free ( s_lon );

  if ( tp->altitude != VIK_DEFAULT_ALTITUDE ) {
    gchar *s_alt = a_coords_dtostr(tp->altitude);
    fprintf ( f, " altitude=\"%s\"", s_alt );
    g_free(s_alt);
  }
  if ( tp->has_timestamp )
    fprintf ( f, " unixtime=\"%ld\"", tp->timestamp );
  if ( tp->newsegment )
    fprintf ( f, " newsegment=\"yes\"" );

  if (tp->extended) {
    fprintf ( f, " extended=\"yes\"" );
    if (!isnan(tp->speed)) {
      gchar *s_speed = a_coords_dtostr(tp->speed);
      fprintf ( f, " speed=\"%s\"", s_speed );
      g_free(s_speed);
    }
    if (!isnan(tp->course)) {
      gchar *s_course = a_coords_dtostr(tp->course);
      fprintf ( f, " course=\"%s\"", s_course );
      g_free(s_course);
    }
    if (tp->nsats > 0)
      fprintf ( f, " sat=\"%d\"", tp->nsats );
    if (tp->fix_mode > 0)
      fprintf ( f, " fix=\"%d\"", tp->fix_mode );
  }
  fprintf ( f, "\n" );
}


static void a_gpspoint_write_track ( const gchar *name, VikTrack *t, FILE *f )
{
  if ( t->comment )
  {
    gchar *tmp_comment = slashdup(t->comment);
    fprintf ( f, "type=\"track\" name=\"%s\" comment=\"%s\"%s\n", name, tmp_comment, t->visible ? "" : " visible=\"n\"" );
    g_free ( tmp_comment );
  }
  else
    fprintf ( f, "type=\"track\" name=\"%s\"%s\n", name, t->visible ? "" : " visible=\"n\"" );
  g_list_foreach ( t->trackpoints, (GFunc) a_gpspoint_write_trackpoint, f );
  fprintf ( f, "type=\"trackend\"\n" );
}

void a_gpspoint_write_file ( VikTrwLayer *trw, FILE *f )
{
  GHashTable *tracks = vik_trw_layer_get_tracks ( trw );
  GHashTable *waypoints = vik_trw_layer_get_waypoints ( trw );

  fprintf ( f, "type=\"waypointlist\"\n" );
  g_hash_table_foreach ( waypoints, (GHFunc) a_gpspoint_write_waypoint, f );
  fprintf ( f, "type=\"waypointlistend\"\n" );
  g_hash_table_foreach ( tracks, (GHFunc) a_gpspoint_write_track, f );
}
