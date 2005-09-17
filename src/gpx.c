/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 *
 * Some of the code adapted from GPSBabel 1.2.7
 * http://gpsbabel.sf.net/
 * Copyright (C) 2002, 2003, 2004, 2005 Robert Lipe, robertlipe@usa.net
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


#define _XOPEN_SOURCE /* glibc2 needs this */

#include "viking.h"
#include <expat.h>
#include <string.h>

typedef enum {
        tt_unknown = 0,

        tt_gpx,

        tt_wpt,
        tt_wpt_desc,
        tt_wpt_name,
        tt_wpt_ele,
        tt_wpt_link,            /* New in GPX 1.1 */

        tt_trk,
        tt_trk_desc,
        tt_trk_name,

        tt_trk_trkseg,
        tt_trk_trkseg_trkpt,
        tt_trk_trkseg_trkpt_ele,
        tt_trk_trkseg_trkpt_time,

        tt_waypoint,
        tt_waypoint_coord,
        tt_waypoint_name,
} tag_type;

typedef struct tag_mapping {
        tag_type tag_type;              /* enum from above for this tag */
        const char *tag_name;           /* xpath-ish tag name */
} tag_mapping;

/*
 * xpath(ish) mappings between full tag paths and internal identifers.
 * These appear in the order they appear in the GPX specification.
 * If it's not a tag we explictly handle, it doesn't go here.
 */

tag_mapping tag_path_map[] = {

        { tt_wpt, "/gpx/wpt" },

        { tt_waypoint, "/loc/waypoint" },
        { tt_waypoint_coord, "/loc/waypoint/coord" },
        { tt_waypoint_name, "/loc/waypoint/name" },

        { tt_wpt_ele, "/gpx/wpt/ele" },
        { tt_wpt_name, "/gpx/wpt/name" },
        { tt_wpt_desc, "/gpx/wpt/desc" },
        { tt_wpt_link, "/gpx/wpt/link" },                    /* GPX 1.1 */

        { tt_trk, "/gpx/trk" },
        { tt_trk_name, "/gpx/trk/name" },
        { tt_trk_desc, "/gpx/trk/desc" },
        { tt_trk_trkseg, "/gpx/trk/trkseg" },

        { tt_trk_trkseg_trkpt, "/gpx/trk/trkseg/trkpt" },
        { tt_trk_trkseg_trkpt_ele, "/gpx/trk/trkseg/trkpt/ele" },
        { tt_trk_trkseg_trkpt_time, "/gpx/trk/trkseg/trkpt/time" },

        {0}
};

static tag_type get_tag(const char *t)
{
        tag_mapping *tm;
        for (tm = tag_path_map; tm->tag_type != 0; tm++)
                if (0 == strcmp(tm->tag_name, t))
                        return tm->tag_type;
        return tt_unknown;
}

/******************************************/

tag_type current_tag = tt_unknown;
GString *xpath = NULL;
GString *c_cdata = NULL;

/* current ("c_") objects */
VikTrackpoint *c_tp = NULL;
VikWaypoint *c_wp = NULL;
VikTrack *c_tr = NULL;

gchar *c_wp_name = NULL;
gchar *c_tr_name = NULL;

/* temporary things so we don't have to create them lots of times */
const gchar *c_slat, *c_slon;
struct LatLon c_ll;

/* specialty flags / etc */
gboolean f_tr_newseg;
guint unnamed_waypoints = 0;
guint unnamed_tracks = 0;


static const char *get_attr ( const char **attr, const char *key )
{
  while ( *attr ) {
    if ( strcmp(*attr,key) == 0 )
      return *(attr + 1);
    attr += 2;
  }
  return NULL;
}

static gboolean set_c_ll ( const char **attr )
{
  if ( (c_slat = get_attr ( attr, "lat" )) && (c_slon = get_attr ( attr, "lon" )) ) {
    c_ll.lat = strtod(c_slat, NULL);
    c_ll.lon = strtod(c_slon, NULL);
    return TRUE;
  }
  return FALSE;
}

static void gpx_start(VikTrwLayer *vtl, const char *el, const char **attr)
{
  static const gchar *tmp;

  g_string_append_c ( xpath, '/' );
  g_string_append ( xpath, el );
  current_tag = get_tag ( xpath->str );

  switch ( current_tag ) {

     case tt_wpt:
       if ( set_c_ll( attr ) ) {
         c_wp = vik_waypoint_new ();
         c_wp->altitude = VIK_DEFAULT_ALTITUDE;
         if ( ! get_attr ( attr, "hidden" ) )
           c_wp->visible = TRUE;

         vik_coord_load_from_latlon ( &(c_wp->coord), vik_trw_layer_get_coord_mode ( vtl ), &c_ll );
       }
       break;

     case tt_trk:
       c_tr = vik_track_new ();
       if ( ! get_attr ( attr, "hidden" ) )
         c_tr->visible = TRUE;
       break;

     case tt_trk_trkseg:
       f_tr_newseg = TRUE;
       break;

     case tt_trk_trkseg_trkpt:
       if ( set_c_ll( attr ) ) {
         c_tp = vik_trackpoint_new ();
         c_tp->altitude = VIK_DEFAULT_ALTITUDE;
         vik_coord_load_from_latlon ( &(c_tp->coord), vik_trw_layer_get_coord_mode ( vtl ), &c_ll );
         if ( f_tr_newseg ) {
           c_tp->newsegment = TRUE;
           f_tr_newseg = FALSE;
         }
         c_tr->trackpoints = g_list_append ( c_tr->trackpoints, c_tp );
       }
       break;

     case tt_trk_trkseg_trkpt_ele:
     case tt_trk_trkseg_trkpt_time:
     case tt_wpt_desc:
     case tt_wpt_name:
     case tt_wpt_ele:
     case tt_wpt_link:
     case tt_trk_desc:
     case tt_trk_name:
       g_string_erase ( c_cdata, 0, -1 ); /* clear the cdata buffer */
       break;

     case tt_waypoint:
       c_wp = vik_waypoint_new ();
       c_wp->altitude = VIK_DEFAULT_ALTITUDE;
       c_wp->visible = TRUE;
       break;

     case tt_waypoint_coord:
       if ( set_c_ll( attr ) )
         vik_coord_load_from_latlon ( &(c_wp->coord), vik_trw_layer_get_coord_mode ( vtl ), &c_ll );
       break;

     case tt_waypoint_name:
       if ( ( tmp = get_attr(attr, "id") ) ) {
         if ( c_wp_name )
           g_free ( c_wp_name );
         c_wp_name = g_strdup ( tmp );
       }
       g_string_erase ( c_cdata, 0, -1 ); /* clear the cdata buffer for description */
       break;
        
     default: break;
  }
}

static void gpx_end(VikTrwLayer *vtl, const char *el)
{
  static struct tm tm;
  g_string_truncate ( xpath, xpath->len - strlen(el) - 1 );

  switch ( current_tag ) {

     case tt_waypoint:
     case tt_wpt:
       if ( ! c_wp_name )
         c_wp_name = g_strdup_printf("VIKING_WP%d", unnamed_waypoints++);
       g_hash_table_insert ( vik_trw_layer_get_waypoints ( vtl ), c_wp_name, c_wp );
       c_wp = NULL;
       c_wp_name = NULL;
       break;

     case tt_trk:
       if ( ! c_tr_name )
         c_tr_name = g_strdup_printf("VIKING_TR%d", unnamed_waypoints++);
       g_hash_table_insert ( vik_trw_layer_get_tracks ( vtl ), c_tr_name, c_tr );
       c_tr = NULL;
       c_tr_name = NULL;
       break;

     case tt_wpt_name:
       if ( c_wp_name )
         g_free ( c_wp_name );
       c_wp_name = g_strdup ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_name:
       if ( c_tr_name )
         g_free ( c_tr_name );
       c_tr_name = g_strdup ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_ele:
       c_wp->altitude = strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_ele:
       c_tp->altitude = strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_waypoint_name: /* .loc name is really description. */
     case tt_wpt_desc:
       vik_waypoint_set_comment ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_link:
       vik_waypoint_set_image ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_desc:
       vik_track_set_comment ( c_tr, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_time:
       if ( strptime(c_cdata->str, "%Y-%m-%dT%H:%M:%SZ", &tm) != c_cdata->str ) { /* it read at least one char */
         c_tp->timestamp = mktime(&tm);
         c_tp->has_timestamp = TRUE;
       }
       g_string_erase ( c_cdata, 0, -1 );
       break;

     default: break;
  }

  current_tag = get_tag ( xpath->str );
}

static void gpx_cdata(void *dta, const XML_Char *s, int len)
{
  switch ( current_tag ) {
    case tt_wpt_name:
    case tt_trk_name:
    case tt_wpt_ele:
    case tt_trk_trkseg_trkpt_ele:
    case tt_wpt_desc:
    case tt_wpt_link:
    case tt_trk_desc:
    case tt_trk_trkseg_trkpt_time:
    case tt_waypoint_name: /* .loc name is really description. */
      g_string_append_len ( c_cdata, s, len );
      break;

    default: break;  /* ignore cdata from other things */
  }
}

// make like a "stack" of tag names
// like gpspoint's separated like /gpx/wpt/whatever

void a_gpx_read_file( VikTrwLayer *vtl, FILE *f ) {
  XML_Parser parser = XML_ParserCreate(NULL);
  int done=0, len;

  XML_SetElementHandler(parser, (XML_StartElementHandler) gpx_start, (XML_EndElementHandler) gpx_end);
  XML_SetUserData(parser, vtl); /* in the future we could remove all global variables */
  XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) gpx_cdata);

  gchar buf[4096];

  g_assert ( f != NULL && vtl != NULL );

  xpath = g_string_new ( "" );
  c_cdata = g_string_new ( "" );

  unnamed_waypoints = 0;
  unnamed_tracks = 0;

  while (!done) {
    len = fread(buf, 1, sizeof(buf)-7, f);
    done = feof(f) || !len;
    XML_Parse(parser, buf, len, done);
  }
 
  g_string_free ( xpath, TRUE );
  g_string_free ( c_cdata, TRUE );
}
