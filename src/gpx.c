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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE /* glibc2 needs this */

#include "gpx.h"
#include "viking.h"
#include <expat.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <glib.h>
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <time.h>

typedef enum {
        tt_unknown = 0,

        tt_gpx,

        tt_wpt,
        tt_wpt_desc,
        tt_wpt_name,
        tt_wpt_ele,
	tt_wpt_sym,
        tt_wpt_link,            /* New in GPX 1.1 */

        tt_trk,
        tt_trk_desc,
        tt_trk_name,

        tt_trk_trkseg,
        tt_trk_trkseg_trkpt,
        tt_trk_trkseg_trkpt_ele,
        tt_trk_trkseg_trkpt_time,
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
} GpxWritingContext;

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
        { tt_wpt_sym, "/gpx/wpt/sym" },
        { tt_wpt_sym, "/loc/waypoint/type" },
        { tt_wpt_link, "/gpx/wpt/link" },                    /* GPX 1.1 */

        { tt_trk, "/gpx/trk" },
        { tt_trk, "/gpx/rte" },
        { tt_trk_name, "/gpx/trk/name" },
        { tt_trk_desc, "/gpx/trk/desc" },
        { tt_trk_trkseg, "/gpx/trk/trkseg" },
        { tt_trk_trkseg_trkpt, "/gpx/trk/trkseg/trkpt" },
        { tt_trk_trkseg_trkpt, "/gpx/rte/rtept" },
        { tt_trk_trkseg_trkpt_ele, "/gpx/trk/trkseg/trkpt/ele" },
        { tt_trk_trkseg_trkpt_time, "/gpx/trk/trkseg/trkpt/time" },
	/* extended */
	{ tt_trk_trkseg_trkpt_course, "/gpx/trk/trkseg/trkpt/course" },
        { tt_trk_trkseg_trkpt_speed, "/gpx/trk/trkseg/trkpt/speed" },
        { tt_trk_trkseg_trkpt_fix, "/gpx/trk/trkseg/trkpt/fix" },
        { tt_trk_trkseg_trkpt_sat, "/gpx/trk/trkseg/trkpt/sat" },

        { tt_trk_trkseg_trkpt_hdop, "/gpx/trk/trkseg/trkpt/hdop" },
        { tt_trk_trkseg_trkpt_vdop, "/gpx/trk/trkseg/trkpt/vdop" },
        { tt_trk_trkseg_trkpt_pdop, "/gpx/trk/trkseg/trkpt/pdop" },
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
    c_ll.lat = g_ascii_strtod(c_slat, NULL);
    c_ll.lon = g_ascii_strtod(c_slon, NULL);
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
  static GTimeVal tp_time;

  g_string_truncate ( xpath, xpath->len - strlen(el) - 1 );

  switch ( current_tag ) {

     case tt_waypoint:
     case tt_wpt:
       if ( ! c_wp_name )
         c_wp_name = g_strdup_printf("VIKING_WP%d", unnamed_waypoints++);
       vik_trw_layer_filein_add_waypoint ( vtl, c_wp_name, c_wp );
       g_free ( c_wp_name );
       c_wp = NULL;
       c_wp_name = NULL;
       break;

     case tt_trk:
       if ( ! c_tr_name )
         c_tr_name = g_strdup_printf("VIKING_TR%d", unnamed_waypoints++);
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
       vik_waypoint_set_comment ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_link:
       vik_waypoint_set_image ( c_wp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_sym: {
       gchar *tmp_lower = g_utf8_strdown(c_cdata->str, -1); /* for things like <type>Geocache</type> */
       vik_waypoint_set_symbol ( c_wp, tmp_lower );
       g_free ( tmp_lower );
       g_string_erase ( c_cdata, 0, -1 );
       break;
       }

     case tt_trk_desc:
       vik_track_set_comment ( c_tr, c_cdata->str );
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
       else  /* TODO: more fix modes here */
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
    case tt_wpt_name:
    case tt_trk_name:
    case tt_wpt_ele:
    case tt_trk_trkseg_trkpt_ele:
    case tt_wpt_desc:
    case tt_wpt_sym:
    case tt_wpt_link:
    case tt_trk_desc:
    case tt_trk_trkseg_trkpt_time:
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
 
  XML_ParserFree (parser);
  g_string_free ( xpath, TRUE );
  g_string_free ( c_cdata, TRUE );
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

static void gpx_write_waypoint ( const gchar *name, VikWaypoint *wp, GpxWritingContext *context ) 
{
  FILE *f = context->file;
  static struct LatLon ll;
  gchar *s_lat,*s_lon;
  gchar *tmp;
  vik_coord_to_latlon ( &(wp->coord), &ll );
  s_lat = a_coords_dtostr( ll.lat );
  s_lon = a_coords_dtostr( ll.lon );
  fprintf ( f, "<wpt lat=\"%s\" lon=\"%s\"%s>\n",
               s_lat, s_lon, wp->visible ? "" : " hidden=\"hidden\"" );
  g_free ( s_lat );
  g_free ( s_lon );

  tmp = entitize ( name );
  fprintf ( f, "  <name>%s</name>\n", tmp );
  g_free ( tmp);

  if ( wp->altitude != VIK_DEFAULT_ALTITUDE )
  {
    tmp = a_coords_dtostr ( wp->altitude );
    fprintf ( f, "  <ele>%s</ele>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->comment )
  {
    tmp = entitize(wp->comment);
    fprintf ( f, "  <desc>%s</desc>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->image )
  {
    tmp = entitize(wp->image);
    fprintf ( f, "  <link>%s</link>\n", tmp );
    g_free ( tmp );
  }
  if ( wp->symbol ) 
  {
    tmp = entitize(wp->symbol);
    fprintf ( f, "  <sym>%s</sym>\n", tmp);
    g_free ( tmp );
  }

  fprintf ( f, "</wpt>\n" );
}

static void gpx_write_trackpoint ( VikTrackpoint *tp, GpxWritingContext *context )
{
  FILE *f = context->file;
  static struct LatLon ll;
  gchar *s_lat,*s_lon, *s_alt, *s_dop;
  gchar *time_iso8601;
  vik_coord_to_latlon ( &(tp->coord), &ll );

  if ( tp->newsegment )
    fprintf ( f, "  </trkseg>\n  <trkseg>\n" );

  s_lat = a_coords_dtostr( ll.lat );
  s_lon = a_coords_dtostr( ll.lon );
  fprintf ( f, "  <trkpt lat=\"%s\" lon=\"%s\">\n", s_lat, s_lon );
  g_free ( s_lat ); s_lat = NULL;
  g_free ( s_lon ); s_lon = NULL;

  s_alt = NULL;
  if ( tp->altitude != VIK_DEFAULT_ALTITUDE )
  {
    s_alt = a_coords_dtostr ( tp->altitude );
  }
  else if ( context->options != NULL && context->options->force_ele )
  {
    s_alt = a_coords_dtostr ( 0 );
  }
  if (s_alt != NULL)
    fprintf ( f, "    <ele>%s</ele>\n", s_alt );
  g_free ( s_alt ); s_alt = NULL;
  
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
    gchar *s_course = a_coords_dtostr(tp->course);
    fprintf ( f, "    <course>%s</course>\n", s_course );
    g_free(s_course);
  }
  if (!isnan(tp->speed)) {
    gchar *s_speed = a_coords_dtostr(tp->speed);
    fprintf ( f, "    <speed>%s</speed>\n", s_speed );
    g_free(s_speed);
  }
  if (tp->fix_mode == VIK_GPS_MODE_2D)
    fprintf ( f, "    <fix>2d</fix>\n");
  if (tp->fix_mode == VIK_GPS_MODE_3D)
    fprintf ( f, "    <fix>3d</fix>\n");
  if (tp->nsats > 0)
    fprintf ( f, "    <sat>%d</sat>\n", tp->nsats );

  s_dop = NULL;
  if ( tp->hdop != VIK_DEFAULT_DOP )
  {
    s_dop = a_coords_dtostr ( tp->hdop );
  }
  if (s_dop != NULL)
    fprintf ( f, "    <hdop>%s</hdop>\n", s_dop );
  g_free ( s_dop ); s_dop = NULL;

  if ( tp->vdop != VIK_DEFAULT_DOP )
  {
    s_dop = a_coords_dtostr ( tp->vdop );
  }
  if (s_dop != NULL)
    fprintf ( f, "    <vdop>%s</vdop>\n", s_dop );
  g_free ( s_dop ); s_dop = NULL;

  if ( tp->pdop != VIK_DEFAULT_DOP )
  {
    s_dop = a_coords_dtostr ( tp->pdop );
  }
  if (s_dop != NULL)
    fprintf ( f, "    <pdop>%s</pdop>\n", s_dop );
  g_free ( s_dop ); s_dop = NULL;


  fprintf ( f, "  </trkpt>\n" );
}


static void gpx_write_track ( const gchar *name, VikTrack *t, GpxWritingContext *context )
{
  FILE *f = context->file;
  gchar *tmp;
  gboolean first_tp_is_newsegment = FALSE; /* must temporarily make it not so, but we want to restore state. not that it matters. */

  tmp = entitize ( name );
  fprintf ( f, "<trk%s>\n  <name>%s</name>\n", t->visible ? "" : " hidden=\"hidden\"", tmp );
  g_free ( tmp );

  if ( t->comment )
  {
    tmp = entitize ( t->comment );
    fprintf ( f, "  <desc>%s</desc>\n", tmp );
    g_free ( tmp );
  }

  fprintf ( f, "  <trkseg>\n" );

  if ( t->trackpoints && t->trackpoints->data ) {
    first_tp_is_newsegment = VIK_TRACKPOINT(t->trackpoints->data)->newsegment;
    VIK_TRACKPOINT(t->trackpoints->data)->newsegment = FALSE; /* so we won't write </trkseg><trkseg> already */
    g_list_foreach ( t->trackpoints, (GFunc) gpx_write_trackpoint, context );
    VIK_TRACKPOINT(t->trackpoints->data)->newsegment = first_tp_is_newsegment; /* restore state */
  }

  fprintf ( f, "</trkseg>\n</trk>\n" );
}

static void gpx_write_header( FILE *f )
{
  fprintf(f, "<?xml version=\"1.0\"?>\n"
          "<gpx version=\"1.0\" creator=\"Viking -- http://viking.sf.net/\"\n"
          "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
          "xmlns=\"http://www.topografix.com/GPX/1/0\"\n"
          "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n");
}

static void gpx_write_footer( FILE *f )
{
  fprintf(f, "</gpx>\n");
}



typedef struct {
  VikWaypoint *wp;
  const gchar *name;
} gpx_waypoint_and_name;

typedef struct {
  gpx_waypoint_and_name *wps;
  guint i;
  guint n_wps;
} gpx_gather_waypoints_passalong_t;

/* Type to hold name of track and timestamp of first trackpoint */
typedef struct {
  time_t first_timestamp;
  const gchar *name;
} gpx_track_and_timestamp;

typedef struct {
  gpx_track_and_timestamp *trks;
  guint i;
  guint n_trks;
} gpx_gather_tracks_passalong_t;

static void gpx_collect_waypoint ( const gchar *name, VikWaypoint *wp, gpx_gather_waypoints_passalong_t *passalong )
{
  if ( passalong->i < passalong->n_wps ) {
    passalong->wps[passalong->i].name = name;
    passalong->wps[passalong->i].wp = wp;
    passalong->i++;
  }
}

/* Function to collect a track and the first timestamp in the list */
static void gpx_collect_track (const gchar *name, VikTrack *track, gpx_gather_tracks_passalong_t *passalong)
{
  if (passalong->i < passalong->n_trks)
  {
    passalong->trks[passalong->i].name = name;
    if (track && track->trackpoints && track->trackpoints->data)
    {
      VikTrackpoint *first_point = (VikTrackpoint *)track->trackpoints->data;
      passalong->trks[passalong->i].first_timestamp = first_point->timestamp;
    }
    else
    {
      passalong->trks[passalong->i].first_timestamp = 0;
    }
    passalong->i++;
  }
}

static int gpx_waypoint_and_name_compar(const void *x, const void *y)
{
  gpx_waypoint_and_name *a = (gpx_waypoint_and_name *)x;
  gpx_waypoint_and_name *b = (gpx_waypoint_and_name *)y;
  return strcmp(a->name,b->name);
}

/* Function to compare two tracks by their first timestamp */
static int gpx_track_and_timestamp_compar(const void *x, const void *y)
{
  gpx_track_and_timestamp *a = (gpx_track_and_timestamp *)x;
  gpx_track_and_timestamp *b = (gpx_track_and_timestamp *)y;
  if (a->first_timestamp < b->first_timestamp)
  {
    return -1;
  }
  if (a->first_timestamp > b->first_timestamp)
  {
    return 1;
  }
  return 0;
}

void a_gpx_write_file( VikTrwLayer *vtl, FILE *f )
{
	a_gpx_write_file_options(NULL, vtl, f);
}

void a_gpx_write_file_options ( GpxWritingOptions *options, VikTrwLayer *vtl, FILE *f )
{
  GpxWritingContext context = { options, f };
  int i;

  gpx_write_header ( f );


  gpx_gather_waypoints_passalong_t passalong;
  passalong.n_wps = g_hash_table_size ( vik_trw_layer_get_waypoints ( vtl ) );
  passalong.i = 0;
  passalong.wps = g_new(gpx_waypoint_and_name,passalong.n_wps);
  g_hash_table_foreach ( vik_trw_layer_get_waypoints ( vtl ), (GHFunc) gpx_collect_waypoint, &passalong );
  /* gather waypoints in a list, then sort */
  qsort(passalong.wps, passalong.n_wps, sizeof(gpx_waypoint_and_name), gpx_waypoint_and_name_compar);
  for ( i = 0; i < passalong.n_wps; i++ )
    gpx_write_waypoint ( passalong.wps[i].name, passalong.wps[i].wp, &context);
  g_free ( passalong.wps );

  gpx_gather_tracks_passalong_t passalong_tracks;
  passalong_tracks.n_trks = g_hash_table_size ( vik_trw_layer_get_tracks (vtl) );
  passalong_tracks.i = 0;
  passalong_tracks.trks = g_new(gpx_track_and_timestamp,passalong_tracks.n_trks);
  g_hash_table_foreach (vik_trw_layer_get_tracks(vtl), (GHFunc) gpx_collect_track, &passalong_tracks);
  /* Sort by timestamp */
  qsort(passalong_tracks.trks, passalong_tracks.n_trks, sizeof(gpx_track_and_timestamp), gpx_track_and_timestamp_compar);
  for (i=0;i<passalong_tracks.n_trks; i++)
  {
    gpx_write_track(passalong_tracks.trks[i].name, (VikTrack *)g_hash_table_lookup(vik_trw_layer_get_tracks(vtl), passalong_tracks.trks[i].name), &context);
  }
  g_free ( passalong_tracks.trks );
  gpx_write_footer ( f );
}

void a_gpx_write_track_file ( const gchar *name, VikTrack *t, FILE *f )
{
  a_gpx_write_track_file_options ( NULL, name, t, f );
}

void a_gpx_write_track_file_options ( GpxWritingOptions *options, const gchar *name, VikTrack *t, FILE *f )
{
  GpxWritingContext context = {options, f};
  gpx_write_header ( f );
  gpx_write_track ( name, t, &context );
  gpx_write_footer ( f );
}
