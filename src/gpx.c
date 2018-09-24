/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2009, Tal B <tal.bav@gmail.com>
 * Copyright (c) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE /* glibc2 needs this */

#include "gpx.h"
#include "viking.h"
#include "vikutils.h"
#include <expat.h>
#include "misc/gtkhtml-private.h"
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <time.h>

typedef enum {
        tt_unknown = 0,

        tt_gpx,
        tt_gpx_name,
        tt_gpx_desc,
        tt_gpx_author,
        tt_gpx_time,
        tt_gpx_keywords,

        tt_wpt,
        tt_wpt_cmt,
        tt_wpt_desc,
        tt_wpt_src,
        tt_wpt_type,
        tt_wpt_name,
        tt_wpt_ele,
        tt_wpt_sym,
        tt_wpt_time,
        tt_wpt_url,
        tt_wpt_link,            /* New in GPX 1.1 */

        tt_trk,
        tt_trk_cmt,
        tt_trk_desc,
        tt_trk_src,
        tt_trk_type,
        tt_trk_name,

        tt_rte,

        tt_trk_trkseg,
        tt_trk_trkseg_trkpt,
        tt_trk_trkseg_trkpt_ele,
        tt_trk_trkseg_trkpt_time,
        tt_trk_trkseg_trkpt_name,
        /* extended */
        tt_trk_trkseg_trkpt_course,
        tt_trk_trkseg_trkpt_speed,
        tt_trk_trkseg_trkpt_fix,
        tt_trk_trkseg_trkpt_sat,

        tt_trk_trkseg_trkpt_hdop,
        tt_trk_trkseg_trkpt_vdop,
        tt_trk_trkseg_trkpt_pdop,

        tt_waypoint,
        tt_waypoint_coord,
        tt_waypoint_name,
} tag_type;

typedef struct tag_mapping {
        tag_type tag_type;              /* enum from above for this tag */
        const char *tag_name;           /* xpath-ish tag name */
} tag_mapping;

typedef struct {
	GpxWritingOptions *options;
	FILE *file;
	const gchar *dirpath;
} GpxWritingContext;

/*
 * xpath(ish) mappings between full tag paths and internal identifiers.
 * These appear in the order they appear in the GPX specification.
 * If it's not a tag we explicitly handle, it doesn't go here.
 */

tag_mapping tag_path_map[] = {

        { tt_gpx, "/gpx" },
        { tt_gpx_name, "/gpx/name" },
        { tt_gpx_desc, "/gpx/desc" },
        { tt_gpx_time, "/gpx/time" },
        { tt_gpx_author, "/gpx/author" },
        { tt_gpx_keywords, "/gpx/keywords" },

        // GPX 1.1 variant - basic properties moved into metadata namespace
        { tt_gpx_name, "/gpx/metadata/name" },
        { tt_gpx_desc, "/gpx/metadata/desc" },
        { tt_gpx_time, "/gpx/metadata/time" },
        { tt_gpx_author, "/gpx/metadata/author/name" },
        { tt_gpx_keywords, "/gpx/metadata/keywords" },

        { tt_wpt, "/gpx/wpt" },

        { tt_waypoint, "/loc/waypoint" },
        { tt_waypoint_coord, "/loc/waypoint/coord" },
        { tt_waypoint_name, "/loc/waypoint/name" },

        { tt_wpt_ele, "/gpx/wpt/ele" },
        { tt_wpt_time, "/gpx/wpt/time" },
        { tt_wpt_name, "/gpx/wpt/name" },
        { tt_wpt_cmt, "/gpx/wpt/cmt" },
        { tt_wpt_desc, "/gpx/wpt/desc" },
        { tt_wpt_src, "/gpx/wpt/src" },
        { tt_wpt_type, "/gpx/wpt/type" },
        { tt_wpt_sym, "/gpx/wpt/sym" },
        { tt_wpt_sym, "/loc/waypoint/type" },
        { tt_wpt_url, "/gpx/wpt/url" },
        { tt_wpt_link, "/gpx/wpt/link" },                    /* GPX 1.1 */

        { tt_trk, "/gpx/trk" },
        { tt_trk_name, "/gpx/trk/name" },
        { tt_trk_cmt, "/gpx/trk/cmt" },
        { tt_trk_desc, "/gpx/trk/desc" },
        { tt_trk_src, "/gpx/trk/src" },
        { tt_trk_type, "/gpx/trk/type" },
        { tt_trk_trkseg, "/gpx/trk/trkseg" },
        { tt_trk_trkseg_trkpt, "/gpx/trk/trkseg/trkpt" },
        { tt_trk_trkseg_trkpt_ele, "/gpx/trk/trkseg/trkpt/ele" },
        { tt_trk_trkseg_trkpt_time, "/gpx/trk/trkseg/trkpt/time" },
        { tt_trk_trkseg_trkpt_name, "/gpx/trk/trkseg/trkpt/name" },
        /* extended */
        { tt_trk_trkseg_trkpt_course, "/gpx/trk/trkseg/trkpt/course" },
        { tt_trk_trkseg_trkpt_speed, "/gpx/trk/trkseg/trkpt/speed" },
        { tt_trk_trkseg_trkpt_fix, "/gpx/trk/trkseg/trkpt/fix" },
        { tt_trk_trkseg_trkpt_sat, "/gpx/trk/trkseg/trkpt/sat" },

        { tt_trk_trkseg_trkpt_hdop, "/gpx/trk/trkseg/trkpt/hdop" },
        { tt_trk_trkseg_trkpt_vdop, "/gpx/trk/trkseg/trkpt/vdop" },
        { tt_trk_trkseg_trkpt_pdop, "/gpx/trk/trkseg/trkpt/pdop" },

        { tt_rte, "/gpx/rte" },
        // NB Route reuses track point feature tags
        { tt_trk_name, "/gpx/rte/name" },
        { tt_trk_cmt, "/gpx/rte/cmt" },
        { tt_trk_desc, "/gpx/rte/desc" },
        { tt_trk_src, "/gpx/rte/src" },
        { tt_trk_trkseg_trkpt, "/gpx/rte/rtept" },
        { tt_trk_trkseg_trkpt_name, "/gpx/rte/rtept/name" },
        { tt_trk_trkseg_trkpt_ele, "/gpx/rte/rtept/ele" },

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
VikTRWMetadata *c_md = NULL;

gchar *c_wp_name = NULL;
gchar *c_tr_name = NULL;

/* temporary things so we don't have to create them lots of times */
const gchar *c_slat, *c_slon;
struct LatLon c_ll;

/* specialty flags / etc */
gboolean f_tr_newseg;
const gchar *c_link = NULL;
guint unnamed_waypoints = 0;
guint unnamed_tracks = 0;
guint unnamed_routes = 0;

typedef struct {
	VikTrwLayer *vtl;
	const gchar *dirpath;
} UserDataT;

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
    c_ll.lat = g_ascii_strtod(c_slat, NULL);
    c_ll.lon = g_ascii_strtod(c_slon, NULL);
    return TRUE;
  }
  return FALSE;
}

static void gpx_start(UserDataT *ud, const char *el, const char **attr)
{
  static const gchar *tmp;
  VikTrwLayer *vtl = ud->vtl;

  g_string_append_c ( xpath, '/' );
  g_string_append ( xpath, el );
  current_tag = get_tag ( xpath->str );

  switch ( current_tag ) {

     case tt_gpx:
       c_md = vik_trw_metadata_new();
       // Store creator information if possible
       const gchar *crt = get_attr ( attr, "creator" );
       if ( crt ) {
         // If there is an actual description field it will overwrite this value
         c_md->description = g_strdup_printf ( _("Created by: %s"), crt );
       }
       break;

     case tt_wpt:
       if ( set_c_ll( attr ) ) {
         c_wp = vik_waypoint_new ();
         c_wp->visible = TRUE;
         if ( get_attr ( attr, "hidden" ) )
           c_wp->visible = FALSE;

         vik_coord_load_from_latlon ( &(c_wp->coord), vik_trw_layer_get_coord_mode ( vtl ), &c_ll );
       }
       break;

     case tt_trk:
     case tt_rte:
       c_tr = vik_track_new ();
       vik_track_set_defaults ( c_tr );
       c_tr->is_route = (current_tag == tt_rte) ? TRUE : FALSE;
       c_tr->visible = TRUE;
       if ( get_attr ( attr, "hidden" ) )
         c_tr->visible = FALSE;
       break;

     case tt_trk_trkseg:
       f_tr_newseg = TRUE;
       break;

     case tt_trk_trkseg_trkpt:
       if ( set_c_ll( attr ) ) {
         c_tp = vik_trackpoint_new ();
         vik_coord_load_from_latlon ( &(c_tp->coord), vik_trw_layer_get_coord_mode ( vtl ), &c_ll );
         if ( f_tr_newseg ) {
           c_tp->newsegment = TRUE;
           f_tr_newseg = FALSE;
         }
         c_tr->trackpoints = g_list_prepend ( c_tr->trackpoints, c_tp );
       }
       break;

     case tt_wpt_link:
       c_link = get_attr ( attr, "href" );
       break;
     case tt_gpx_name:
     case tt_gpx_author:
     case tt_gpx_desc:
     case tt_gpx_keywords:
     case tt_gpx_time:
     case tt_trk_trkseg_trkpt_name:
     case tt_trk_trkseg_trkpt_ele:
     case tt_trk_trkseg_trkpt_time:
     case tt_wpt_cmt:
     case tt_wpt_desc:
     case tt_wpt_src:
     case tt_wpt_type:
     case tt_wpt_name:
     case tt_wpt_ele:
     case tt_wpt_time:
     case tt_wpt_url:
     case tt_trk_cmt:
     case tt_trk_desc:
     case tt_trk_src:
     case tt_trk_type:
     case tt_trk_name:
       g_string_erase ( c_cdata, 0, -1 ); /* clear the cdata buffer */
       break;

     case tt_waypoint:
       c_wp = vik_waypoint_new ();
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

// Allow user override / refinement of GPX tidying
#define VIK_SETTINGS_GPX_TIDY "gpx_tidy_points"
#define VIK_SETTINGS_GPX_TIDY_SPEED "gpx_tidy_points_max_speed"

/**
 *
 */
static void track_tidy_processing ( VikTrwLayer *vtl )
{
  // Default to automatically attempt tiding
  gboolean do_tidy = TRUE;
  gboolean btmp = TRUE;
  if ( a_settings_get_boolean ( VIK_SETTINGS_GPX_TIDY, &btmp ) )
     do_tidy = btmp;

  if ( do_tidy ) {
    // highly unlikely to be going faster than this, especially for the first point
    guint speed = 340; // Speed of Sound

    gint itmp = 0;
    if ( a_settings_get_integer ( VIK_SETTINGS_GPX_TIDY_SPEED, &itmp ) )
      speed = (guint)itmp;

    // NB bounds calculated in subsequent layer post read,
    //  so no need to do it now
    vik_trw_layer_tidy_tracks ( vtl, speed, FALSE );
  }
}

static void gpx_end(UserDataT *ud, const char *el)
{
  static GTimeVal tp_time;
  static GTimeVal wp_time;
  VikTrwLayer *vtl = ud->vtl;

  g_string_truncate ( xpath, xpath->len - strlen(el) - 1 );

  switch ( current_tag ) {

     case tt_gpx:
       vik_trw_layer_set_metadata ( vtl, c_md );
       c_md = NULL;

       // Essentially the end for a TrackWaypoint layer,
       //  so any specific GPX post processing can occur here
       track_tidy_processing ( vtl );

       break;

     case tt_gpx_name:
       vik_layer_rename ( VIK_LAYER(vtl), c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_gpx_author:
       if ( c_md->author )
         g_free ( c_md->author );
       c_md->author = g_strdup ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_gpx_desc:
       if ( c_md->description )
         g_free ( c_md->description );
       c_md->description = g_strdup ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_gpx_keywords:
       if ( c_md->keywords )
         g_free ( c_md->keywords );
       c_md->keywords = g_strdup ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_gpx_time:
       if ( c_md->timestamp )
         g_free ( c_md->timestamp );
       c_md->timestamp = g_strdup ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_waypoint:
     case tt_wpt:
       if ( ! c_wp_name )
         c_wp_name = g_strdup_printf("VIKING_WP%04d", unnamed_waypoints++);
       vik_trw_layer_filein_add_waypoint ( vtl, c_wp_name, c_wp );
       g_free ( c_wp_name );
       c_wp = NULL;
       c_wp_name = NULL;
       break;

     case tt_trk:
       if ( ! c_tr_name )
         c_tr_name = g_strdup_printf("VIKING_TR%03d", unnamed_tracks++);
       // Delibrate fall through
     case tt_rte:
       if ( ! c_tr_name )
         c_tr_name = g_strdup_printf("VIKING_RT%03d", unnamed_routes++);
       c_tr->trackpoints = g_list_reverse ( c_tr->trackpoints );
       vik_trw_layer_filein_add_track ( vtl, c_tr_name, c_tr );
       g_free ( c_tr_name );
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
       c_wp->altitude = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_ele:
       c_tp->altitude = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_waypoint_name: /* .loc name is really description. */
     case tt_wpt_desc:
       vik_waypoint_set_description ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_cmt:
       vik_waypoint_set_comment ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_src:
       vik_waypoint_set_source ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_type:
       vik_waypoint_set_type ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_url:
       vik_waypoint_set_url ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_link:
       if ( c_link ) {
         // Correct <link href="uri"></link> format
         if ( util_is_url(c_link) ) {
           vik_waypoint_set_url ( c_wp, c_link );
         }
         else {
           vu_waypoint_set_image_uri ( c_wp, c_link, ud->dirpath );
         }
       }
       else {
         // Fallback for incorrect GPX <link> format (probably from previous versions of Viking!)
         //  of the form <link>file</link>
         gchar *fn = util_make_absolute_filename ( c_cdata->str, ud->dirpath );
         vik_waypoint_set_image ( c_wp, fn ? fn : c_cdata->str );
         g_free ( fn );
       }
       c_link = NULL;
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_sym:
       vik_waypoint_set_symbol ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_desc:
       vik_track_set_description ( c_tr, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_src:
       vik_track_set_source ( c_tr, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_type:
       vik_track_set_type ( c_tr, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_cmt:
       vik_track_set_comment ( c_tr, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_time:
       if ( g_time_val_from_iso8601(c_cdata->str, &wp_time) ) {
         c_wp->timestamp = wp_time.tv_sec;
         c_wp->has_timestamp = TRUE;
       }
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_name:
       vik_trackpoint_set_name ( c_tp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_time:
       if ( g_time_val_from_iso8601(c_cdata->str, &tp_time) ) {
         c_tp->timestamp = tp_time.tv_sec;
         c_tp->has_timestamp = TRUE;
       }
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_course:
       c_tp->course = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_speed:
       c_tp->speed = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_fix:
       if (!strcmp("2d", c_cdata->str))
         c_tp->fix_mode = VIK_GPS_MODE_2D;
       else if (!strcmp("3d", c_cdata->str))
         c_tp->fix_mode = VIK_GPS_MODE_3D;
       else if (!strcmp("dgps", c_cdata->str))
         c_tp->fix_mode = VIK_GPS_MODE_DGPS;
       else if (!strcmp("pps", c_cdata->str))
         c_tp->fix_mode = VIK_GPS_MODE_PPS;
       else
         c_tp->fix_mode = VIK_GPS_MODE_NOT_SEEN;
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_sat:
       c_tp->nsats = atoi ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_hdop:
       c_tp->hdop = g_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_vdop:
       c_tp->vdop = g_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_pdop:
       c_tp->pdop = g_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     default: break;
  }

  current_tag = get_tag ( xpath->str );
}

static void gpx_cdata(void *dta, const XML_Char *s, int len)
{
  switch ( current_tag ) {
    case tt_gpx_name:
    case tt_gpx_author:
    case tt_gpx_desc:
    case tt_gpx_keywords:
    case tt_gpx_time:
    case tt_wpt_name:
    case tt_trk_name:
    case tt_wpt_ele:
    case tt_trk_trkseg_trkpt_ele:
    case tt_wpt_cmt:
    case tt_wpt_desc:
    case tt_wpt_src:
    case tt_wpt_type:
    case tt_wpt_sym:
    case tt_wpt_url:
    case tt_wpt_link:
    case tt_trk_cmt:
    case tt_trk_desc:
    case tt_trk_src:
    case tt_trk_type:
    case tt_trk_trkseg_trkpt_time:
    case tt_wpt_time:
    case tt_trk_trkseg_trkpt_name:
    case tt_trk_trkseg_trkpt_course:
    case tt_trk_trkseg_trkpt_speed:
    case tt_trk_trkseg_trkpt_fix:
    case tt_trk_trkseg_trkpt_sat:
    case tt_trk_trkseg_trkpt_hdop:
    case tt_trk_trkseg_trkpt_vdop:
    case tt_trk_trkseg_trkpt_pdop:
    case tt_waypoint_name: /* .loc name is really description. */
      g_string_append_len ( c_cdata, s, len );
      break;

    default: break;  /* ignore cdata from other things */
  }
}

// make like a "stack" of tag names
// like gpspoint's separated like /gpx/wpt/whatever

gboolean a_gpx_read_file( VikTrwLayer *vtl, FILE *f, const gchar* dirpath ) {
  XML_Parser parser = XML_ParserCreate(NULL);
  int done=0, len;
  enum XML_Status status = XML_STATUS_ERROR;

  UserDataT *ud = g_malloc (sizeof(UserDataT));
  ud->vtl     = vtl;
  ud->dirpath = dirpath;

  XML_SetElementHandler(parser, (XML_StartElementHandler) gpx_start, (XML_EndElementHandler) gpx_end);
  XML_SetUserData(parser, ud);
  XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) gpx_cdata);

  gchar buf[4096];

  g_assert ( f != NULL && vtl != NULL );

  xpath = g_string_new ( "" );
  c_cdata = g_string_new ( "" );

  unnamed_waypoints = 1;
  unnamed_tracks = 1;
  unnamed_routes = 1;

  while (!done) {
    len = fread(buf, 1, sizeof(buf)-7, f);
    done = feof(f) || !len;
    status = XML_Parse(parser, buf, len, done);
  }

  XML_ParserFree (parser);
  g_free ( ud );
  g_string_free ( xpath, TRUE );
  g_string_free ( c_cdata, TRUE );

  return status != XML_STATUS_ERROR;
}

/**** entitize from GPSBabel ****/
typedef struct {
        const char * text;
        const char * entity;
        int  not_html;
} entity_types;

static
entity_types stdentities[] =  {
        { "&",  "&amp;", 0 },
        { "'",  "&apos;", 1 },
        { "<",  "&lt;", 0 },
        { ">",  "&gt;", 0 },
        { "\"", "&quot;", 0 },
        { NULL, NULL, 0 }
};

void utf8_to_int( const char *cp, int *bytes, int *value )
{
        if ( (*cp & 0xe0) == 0xc0 ) {
                if ( (*(cp+1) & 0xc0) != 0x80 ) goto dodefault;
                *bytes = 2;
                *value = ((*cp & 0x1f) << 6) |
                        (*(cp+1) & 0x3f);
        }
        else if ( (*cp & 0xf0) == 0xe0 ) {
                if ( (*(cp+1) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+2) & 0xc0) != 0x80 ) goto dodefault;
                *bytes = 3;
                *value = ((*cp & 0x0f) << 12) |
                        ((*(cp+1) & 0x3f) << 6) |
                        (*(cp+2) & 0x3f);
        }
        else if ( (*cp & 0xf8) == 0xf0 ) {
                if ( (*(cp+1) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+2) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+3) & 0xc0) != 0x80 ) goto dodefault;
                *bytes = 4;
                *value = ((*cp & 0x07) << 18) |
                        ((*(cp+1) & 0x3f) << 12) |
                        ((*(cp+2) & 0x3f) << 6) |
                        (*(cp+3) & 0x3f);
        }
        else if ( (*cp & 0xfc) == 0xf8 ) {
                if ( (*(cp+1) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+2) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+3) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+4) & 0xc0) != 0x80 ) goto dodefault;
                *bytes = 5;
                *value = ((*cp & 0x03) << 24) |
                        ((*(cp+1) & 0x3f) << 18) |
                        ((*(cp+2) & 0x3f) << 12) |
                        ((*(cp+3) & 0x3f) << 6) |
                        (*(cp+4) & 0x3f);
        }
        else if ( (*cp & 0xfe) == 0xfc ) {
                if ( (*(cp+1) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+2) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+3) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+4) & 0xc0) != 0x80 ) goto dodefault;
                if ( (*(cp+5) & 0xc0) != 0x80 ) goto dodefault;
                *bytes = 6;
                *value = ((*cp & 0x01) << 30) |
                        ((*(cp+1) & 0x3f) << 24) |
                        ((*(cp+2) & 0x3f) << 18) |
                        ((*(cp+3) & 0x3f) << 12) |
                        ((*(cp+4) & 0x3f) << 6) |
                        (*(cp+5) & 0x3f);
        }
        else {
dodefault:
                *bytes = 1;
                *value = (unsigned char)*cp;
        }
}

static
char *
entitize(const char * str)
{
        int elen, ecount, nsecount;
        entity_types *ep;
        const char * cp;
        char * p, * tmp, * xstr;

        char tmpsub[20];
        int bytes = 0;
        int value = 0;
        ep = stdentities;
        elen = ecount = nsecount = 0;

        /* figure # of entity replacements and additional size. */
        while (ep->text) {
                cp = str;
                while ((cp = strstr(cp, ep->text)) != NULL) {
                        elen += strlen(ep->entity) - strlen(ep->text);
                        ecount++;
                        cp += strlen(ep->text);
                }
                ep++;
        }

        /* figure the same for other than standard entities (i.e. anything
         * that isn't in the range U+0000 to U+007F */
        for ( cp = str; *cp; cp++ ) {
                if ( *cp & 0x80 ) {

                        utf8_to_int( cp, &bytes, &value );
                        cp += bytes-1;
                        elen += sprintf( tmpsub, "&#x%x;", value ) - bytes;
                        nsecount++;
                }
        }

        /* enough space for the whole string plus entity replacements, if any */
        tmp = g_malloc((strlen(str) + elen + 1));
        strcpy(tmp, str);

        /* no entity replacements */
        if (ecount == 0 && nsecount == 0)
                return (tmp);

        if ( ecount != 0 ) {
                for (ep = stdentities; ep->text; ep++) {
                        p = tmp;
                        while ((p = strstr(p, ep->text)) != NULL) {
                                elen = strlen(ep->entity);

                                xstr = g_strdup(p + strlen(ep->text));

                                strcpy(p, ep->entity);
                                strcpy(p + elen, xstr);

                                g_free(xstr);

                                p += elen;
                        }
                }
        }

        if ( nsecount != 0 ) {
                p = tmp;
                while (*p) {
                        if ( *p & 0x80 ) {
                                utf8_to_int( p, &bytes, &value );
                                if ( p[bytes] ) {
                                        xstr = g_strdup( p + bytes );
                                }
                                else {
                                        xstr = NULL;
                                }
                                sprintf( p, "&#x%x;", value );
                                p = p+strlen(p);
                                if ( xstr ) {
                                        strcpy( p, xstr );
                                        g_free(xstr);
                                }
                        }
                        else {
                                p++;
                        }
                }
        }
        return (tmp);
}
/**** end GPSBabel code ****/

/* export GPX */

/**
 * Note that elements are written in the schema specification order
 */
static void gpx_write_waypoint ( VikWaypoint *wp, GpxWritingContext *context )
{
  // Don't write invisible waypoints when specified
  if (context->options && !context->options->hidden && !wp->visible)
    return;

  FILE *f = context->file;
  struct LatLon ll;
  gchar s_lat[COORDS_STR_BUFFER_SIZE];
  gchar s_lon[COORDS_STR_BUFFER_SIZE];
  gchar *tmp;
  vik_coord_to_latlon ( &(wp->coord), &ll );
  a_coords_dtostr_buffer ( ll.lat, s_lat );
  a_coords_dtostr_buffer ( ll.lon, s_lon );
  // NB 'hidden' is not part of any GPX standard - this appears to be a made up Viking 'extension'
  //  luckily most other GPX processing software ignores things they don't understand
  fprintf ( f, "<wpt lat=\"%s\" lon=\"%s\"%s>\n",
               s_lat, s_lon, wp->visible ? "" : " hidden=\"hidden\"" );

  if ( wp->altitude != VIK_DEFAULT_ALTITUDE )
  {
    gchar s_alt[COORDS_STR_BUFFER_SIZE];
    a_coords_dtostr_buffer ( wp->altitude, s_alt );
    fprintf ( f, "  <ele>%s</ele>\n", s_alt );
  }

  if ( wp->has_timestamp ) {
    GTimeVal timestamp;
    timestamp.tv_sec = wp->timestamp;
    timestamp.tv_usec = 0;

    gchar *time_iso8601 = g_time_val_to_iso8601 ( &timestamp );
    if ( time_iso8601 != NULL )
      fprintf ( f, "  <time>%s</time>\n", time_iso8601 );
    g_free ( time_iso8601 );
  }

  // Sanity clause
  if ( wp->name )
    tmp = entitize ( wp->name );
  else
    tmp = g_strdup ("waypoint");

  fprintf ( f, "  <name>%s</name>\n", tmp );
  g_free ( tmp);

  if ( wp->comment )
  {
    tmp = entitize(wp->comment);
    fprintf ( f, "  <cmt>%s</cmt>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->description )
  {
    tmp = entitize(wp->description);
    fprintf ( f, "  <desc>%s</desc>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->source )
  {
    tmp = entitize(wp->source);
    fprintf ( f, "  <src>%s</src>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->url )
  {
    tmp = entitize(wp->url);
    fprintf ( f, "  <url>%s</url>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->image )
  {
    gchar *tmp = NULL;
    if ( a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE ) {
      if ( context->dirpath ) {
        gchar *rtmp = g_strdup ( file_GetRelativeFilename ( (gchar*)context->dirpath, wp->image ) );
        if ( rtmp ) {
          tmp = gtk_html_filename_to_uri ( rtmp );
        }
      }
    }
    if ( !tmp )
      tmp = gtk_html_filename_to_uri ( wp->image );
    fprintf ( f, "  <link href=\"%s\"></link>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->symbol ) 
  {
    tmp = entitize(wp->symbol);
    if ( a_vik_gpx_export_wpt_sym_name ( ) ) {
       // Lowercase the symbol name
       gchar *tmp2 = g_utf8_strdown ( tmp, -1 );
       fprintf ( f, "  <sym>%s</sym>\n",  tmp2 );
       g_free ( tmp2 );
    }
    else
      fprintf ( f, "  <sym>%s</sym>\n", tmp);
    g_free ( tmp );
  }
  if ( wp->type )
  {
    tmp = entitize(wp->type);
    fprintf ( f, "  <type>%s</type>\n", tmp );
    g_free ( tmp );
  }

  fprintf ( f, "</wpt>\n" );
}

/**
 * Note that elements are written in the schema specification order
 */
static void gpx_write_trackpoint ( VikTrackpoint *tp, GpxWritingContext *context )
{
  FILE *f = context->file;
  struct LatLon ll;
  gchar s_lat[COORDS_STR_BUFFER_SIZE];
  gchar s_lon[COORDS_STR_BUFFER_SIZE];
  gchar s_alt[COORDS_STR_BUFFER_SIZE];
  gchar s_dop[COORDS_STR_BUFFER_SIZE];
  gchar *time_iso8601;
  vik_coord_to_latlon ( &(tp->coord), &ll );

  // No such thing as a rteseg! So make sure we don't put them in
  if ( context->options && !context->options->is_route && tp->newsegment )
    fprintf ( f, "  </trkseg>\n  <trkseg>\n" );

  a_coords_dtostr_buffer ( ll.lat, s_lat );
  a_coords_dtostr_buffer ( ll.lon, s_lon );
  fprintf ( f, "  <%spt lat=\"%s\" lon=\"%s\">\n", (context->options && context->options->is_route) ? "rte" : "trk", s_lat, s_lon );

  if ( tp->altitude != VIK_DEFAULT_ALTITUDE )
  {
    a_coords_dtostr_buffer ( tp->altitude, s_alt );
    fprintf ( f, "    <ele>%s</ele>\n", s_alt );
  }
  else if ( context->options != NULL && context->options->force_ele )
  {
    fprintf ( f, "    <ele>0</ele>\n" );
  }
  
  time_iso8601 = NULL;
  if ( tp->has_timestamp ) {
    GTimeVal timestamp;
    timestamp.tv_sec = tp->timestamp;
    timestamp.tv_usec = 0;
  
    time_iso8601 = g_time_val_to_iso8601 ( &timestamp );
  }
  else if ( context->options != NULL && context->options->force_time )
  {
    GTimeVal current;
    g_get_current_time ( &current );
  
    time_iso8601 = g_time_val_to_iso8601 ( &current );
  }
  if ( time_iso8601 != NULL )
    fprintf ( f, "    <time>%s</time>\n", time_iso8601 );
  g_free(time_iso8601);
  time_iso8601 = NULL;
  
  if (!isnan(tp->course)) {
    gchar s_course[COORDS_STR_BUFFER_SIZE];
    a_coords_dtostr_buffer ( tp->course, s_course );
    fprintf ( f, "    <course>%s</course>\n", s_course );
  }
  if (!isnan(tp->speed)) {
    gchar s_speed[COORDS_STR_BUFFER_SIZE];
    a_coords_dtostr_buffer ( tp->speed, s_speed );
    fprintf ( f, "    <speed>%s</speed>\n", s_speed );
  }

  if (tp->name) {
    gchar *s_name = entitize(tp->name);
    fprintf ( f, "    <name>%s</name>\n", s_name );
    g_free(s_name);
  }

  if (tp->fix_mode == VIK_GPS_MODE_2D)
    fprintf ( f, "    <fix>2d</fix>\n");
  if (tp->fix_mode == VIK_GPS_MODE_3D)
    fprintf ( f, "    <fix>3d</fix>\n");
  if (tp->fix_mode == VIK_GPS_MODE_DGPS)
    fprintf ( f, "    <fix>dgps</fix>\n");
  if (tp->fix_mode == VIK_GPS_MODE_PPS)
    fprintf ( f, "    <fix>pps</fix>\n");
  if (tp->nsats > 0)
    fprintf ( f, "    <sat>%d</sat>\n", tp->nsats );

  if ( tp->hdop != VIK_DEFAULT_DOP ) {
    a_coords_dtostr_buffer ( tp->hdop, s_dop );
    fprintf ( f, "    <hdop>%s</hdop>\n", s_dop );
  }

  if ( tp->vdop != VIK_DEFAULT_DOP ) {
    a_coords_dtostr_buffer ( tp->vdop, s_dop );
    fprintf ( f, "    <vdop>%s</vdop>\n", s_dop );
  }

  if ( tp->pdop != VIK_DEFAULT_DOP ) {
    a_coords_dtostr_buffer ( tp->pdop, s_dop );
    fprintf ( f, "    <pdop>%s</pdop>\n", s_dop );
  }

  fprintf ( f, "  </%spt>\n", (context->options && context->options->is_route) ? "rte" : "trk" );
}


static void gpx_write_track ( VikTrack *t, GpxWritingContext *context )
{
  // Don't write invisible tracks when specified
  if (context->options && !context->options->hidden && !t->visible)
    return;

  FILE *f = context->file;
  gchar *tmp;
  gboolean first_tp_is_newsegment = FALSE; /* must temporarily make it not so, but we want to restore state. not that it matters. */

  // Sanity clause
  if ( t->name )
    tmp = entitize ( t->name );
  else
    tmp = g_strdup ("track");

  // NB 'hidden' is not part of any GPX standard - this appears to be a made up Viking 'extension'
  //  luckily most other GPX processing software ignores things they don't understand
  fprintf ( f, "<%s%s>\n  <name>%s</name>\n",
	    t->is_route ? "rte" : "trk",
	    t->visible ? "" : " hidden=\"hidden\"",
	    tmp );
  g_free ( tmp );

  if ( t->comment )
  {
    tmp = entitize ( t->comment );
    fprintf ( f, "  <cmt>%s</cmt>\n", tmp );
    g_free ( tmp );
  }

  if ( t->description )
  {
    tmp = entitize ( t->description );
    fprintf ( f, "  <desc>%s</desc>\n", tmp );
    g_free ( tmp );
  }

  if ( t->source )
  {
    tmp = entitize ( t->source );
    fprintf ( f, "  <src>%s</src>\n", tmp );
    g_free ( tmp );
  }

  if ( t->type )
  {
    tmp = entitize ( t->type );
    fprintf ( f, "  <type>%s</type>\n", tmp );
    g_free ( tmp );
  }

  /* No such thing as a rteseg! */
  if ( !t->is_route )
    fprintf ( f, "  <trkseg>\n" );

  if ( t->trackpoints && t->trackpoints->data ) {
    first_tp_is_newsegment = VIK_TRACKPOINT(t->trackpoints->data)->newsegment;
    VIK_TRACKPOINT(t->trackpoints->data)->newsegment = FALSE; /* so we won't write </trkseg><trkseg> already */
    g_list_foreach ( t->trackpoints, (GFunc) gpx_write_trackpoint, context );
    VIK_TRACKPOINT(t->trackpoints->data)->newsegment = first_tp_is_newsegment; /* restore state */
  }

  /* NB apparently no such thing as a rteseg! */
  if (!t->is_route)
    fprintf ( f, "  </trkseg>\n");

  fprintf ( f, "</%s>\n", t->is_route ? "rte" : "trk" );
}

static void gpx_write_header( FILE *f )
{
  // Allow overriding the creator value
  // E.g. if something actually cares about it, see for example:
  //   http://strava.github.io/api/v3/uploads/
  gchar *creator = g_strdup(a_vik_gpx_export_creator());
  if ( g_strcmp0(creator, "") == 0 )
    creator = g_strdup_printf("Viking %s -- %s", PACKAGE_VERSION, PACKAGE_URL);

  fprintf(f, "<?xml version=\"1.0\"?>\n"
             "<gpx version=\"1.0\"\n");
  fprintf(f, "creator=\"%s\"\n", creator);
  fprintf(f,"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            "xmlns=\"http://www.topografix.com/GPX/1/0\"\n"
            "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n");
  g_free(creator);
}

static void gpx_write_footer( FILE *f )
{
  fprintf(f, "</gpx>\n");
}

static int gpx_waypoint_compare(const void *x, const void *y)
{
  VikWaypoint *a = (VikWaypoint *)x;
  VikWaypoint *b = (VikWaypoint *)y;
  return strcmp(a->name,b->name);
}

static int gpx_track_compare_name(const void *x, const void *y)
{
  VikTrack *a = (VikTrack *)x;
  VikTrack *b = (VikTrack *)y;
  return strcmp(a->name,b->name);
}

void a_gpx_write_file ( VikTrwLayer *vtl, FILE *f, GpxWritingOptions *options, const gchar* dirpath )
{
  GpxWritingContext context = { options, f, dirpath };

  gpx_write_header ( f );

  gchar *tmp;
  const gchar *name = vik_layer_get_name(VIK_LAYER(vtl));
  if ( name ) {
    tmp = entitize ( name );
    fprintf ( f, "  <name>%s</name>\n", tmp );
    g_free ( tmp );
  }

  VikTRWMetadata *md = vik_trw_layer_get_metadata (vtl);
  if ( md ) {
    if ( md->author && strlen(md->author) > 0 ) {
      tmp = entitize ( md->author );
      fprintf ( f, "  <author>%s</author>\n", tmp );
      g_free ( tmp );
    }
    if ( md->description && strlen(md->description) > 0) {
      tmp = entitize ( md->description );
      fprintf ( f, "  <desc>%s</desc>\n", tmp );
      g_free ( tmp );
    }
    if ( md->timestamp ) {
      tmp = entitize ( md->timestamp );
      fprintf ( f, "  <time>%s</time>\n", tmp );
      g_free ( tmp );
    }
    if ( md->keywords && strlen(md->keywords) > 0) {
      tmp = entitize ( md->keywords );
      fprintf ( f, "  <keywords>%s</keywords>\n", tmp );
      g_free ( tmp );
    }
  }

  if ( vik_trw_layer_get_waypoints_visibility(vtl) || (options && options->hidden) ) {
    // gather waypoints in a list, then sort
    GList *gl = g_hash_table_get_values ( vik_trw_layer_get_waypoints ( vtl ) );
    gl = g_list_sort ( gl, gpx_waypoint_compare );

    for (GList *iter = g_list_first (gl); iter != NULL; iter = g_list_next (iter)) {
      gpx_write_waypoint ( (VikWaypoint*)iter->data, &context );
    }
    g_list_free ( gl );
  }

  GList *gl = NULL;
  if ( vik_trw_layer_get_tracks_visibility(vtl) || (options && options->hidden) ) {
    //gl = g_hash_table_get_values ( vik_trw_layer_get_tracks ( vtl ) );
    // Forming the list manually seems to produce one that is more likely to be nearer to the creation order
    gpointer key, value;
    GHashTableIter ght_iter;
    g_hash_table_iter_init ( &ght_iter, vik_trw_layer_get_tracks ( vtl ) );
    while ( g_hash_table_iter_next (&ght_iter, &key, &value) ) {
      gl = g_list_prepend ( gl ,value );
    }
    gl = g_list_reverse ( gl );

    // Sort method determined by preference
    if ( a_vik_get_gpx_export_trk_sort() == VIK_GPX_EXPORT_TRK_SORT_TIME )
      gl = g_list_sort ( gl, vik_track_compare_timestamp );
    else if ( a_vik_get_gpx_export_trk_sort() == VIK_GPX_EXPORT_TRK_SORT_ALPHA )
      gl = g_list_sort ( gl, gpx_track_compare_name );
  }

  GList *glrte = NULL;
  // Routes sorted by name
  if ( vik_trw_layer_get_routes_visibility(vtl) || (options && options->hidden) ) {
    glrte = g_hash_table_get_values ( vik_trw_layer_get_routes ( vtl ) );
    glrte = g_list_sort ( glrte, gpx_track_compare_name );
  }

  // g_list_concat doesn't copy memory properly
  // so process each list separately

  GpxWritingContext context_tmp = context;
  GpxWritingOptions opt_tmp = { FALSE, FALSE, FALSE, FALSE };
  // Force trackpoints on tracks
  if ( !context.options )
    context_tmp.options = &opt_tmp;
  context_tmp.options->is_route = FALSE;

  // Loop around each list and write each one
  for (GList *iter = g_list_first (gl); iter != NULL; iter = g_list_next (iter)) {
    gpx_write_track ( (VikTrack*)iter->data, &context_tmp );
  }

  // Routes (to get routepoints)
  context_tmp.options->is_route = TRUE;
  for (GList *iter = g_list_first (glrte); iter != NULL; iter = g_list_next (iter)) {
    gpx_write_track ( (VikTrack*)iter->data, &context_tmp );
  }

  g_list_free ( gl );
  g_list_free ( glrte );

  gpx_write_footer ( f );
}

void a_gpx_write_track_file ( VikTrack *trk, FILE *f, GpxWritingOptions *options )
{
  GpxWritingContext context = {options, f};
  gpx_write_header ( f );
  gpx_write_track ( trk, &context );
  gpx_write_footer ( f );
}

/**
 * Common write of a temporary GPX file
 */
static gchar* write_tmp_file ( VikTrwLayer *vtl, VikTrack *trk, GpxWritingOptions *options )
{
	gchar *tmp_filename = NULL;
	GError *error = NULL;
	// Opening temporary file
	int fd = g_file_open_tmp("viking_XXXXXX.gpx", &tmp_filename, &error);
	if (fd < 0) {
		g_warning ( _("failed to open temporary file: %s"), error->message );
		g_clear_error ( &error );
		return NULL;
	}
	g_debug ("%s: temporary file = %s", __FUNCTION__, tmp_filename);

	FILE *ff = fdopen (fd, "w");

	if ( trk )
		a_gpx_write_track_file ( trk, ff, options );
	else
		a_gpx_write_file ( vtl, ff, options, NULL );

	fclose (ff);

	return tmp_filename;
}

/*
 * a_gpx_write_tmp_file:
 * @vtl:     The #VikTrwLayer to write
 * @options: Possible ways of writing the file data (can be NULL)
 *
 * Returns: The name of newly created temporary GPX file
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
gchar* a_gpx_write_tmp_file ( VikTrwLayer *vtl, GpxWritingOptions *options )
{
	return write_tmp_file ( vtl, NULL, options );
}

/*
 * a_gpx_write_track_tmp_file:
 * @trk:     The #VikTrack to write
 * @options: Possible ways of writing the file data (can be NULL)
 *
 * Returns: The name of newly created temporary GPX file
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
gchar* a_gpx_write_track_tmp_file ( VikTrack *trk, GpxWritingOptions *options )
{
	return write_tmp_file ( NULL, trk, options );
}
