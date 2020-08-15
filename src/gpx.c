/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2009, Tal B <tal.bav@gmail.com>
 * Copyright (c) 2012-2020, Rob Norris <rw_norris@hotmail.com>
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
#include "gpx.h"
#include "viking.h"
#include <expat.h>
#include "misc/gtkhtml-private.h"

typedef enum {
        tt_unknown = 0,

        tt_gpx,
        tt_gpx_name,
        tt_gpx_desc,
        tt_gpx_author,
        tt_gpx_url,
        tt_gpx_time,
        tt_gpx_keywords,
        tt_gpx_extensions,      // GPX 1.1
        tt_gpx_an_extension,    // Generic GPX 1.1 extensions

        tt_wpt,
        tt_wpt_cmt,
        tt_wpt_desc,
        tt_wpt_src,
        tt_wpt_type,
        tt_wpt_name,
        tt_wpt_ele,
        tt_wpt_sym,
        tt_wpt_time,
        tt_wpt_course,          // Not in GPX 1.1 (although can be in extensions in 1.1)
        tt_wpt_speed,           // Not in GPX 1.1 (although can be in extensions in 1.1)
        tt_wpt_magvar,
        tt_wpt_geoidheight,
        tt_wpt_url,
        tt_wpt_url_name,
        tt_wpt_link,            /* New in GPX 1.1 */
        tt_wpt_fix,
        tt_wpt_sat,
        tt_wpt_hdop,
        tt_wpt_vdop,
        tt_wpt_pdop,
        tt_wpt_ageofdgpsdata,
        tt_wpt_dgpsid,

        tt_wpt_extensions,      // GPX 1.1
        tt_wpt_an_extension,    // Generic GPX 1.1 extensions

        tt_trk,
        tt_trk_name,
        tt_trk_cmt,
        tt_trk_desc,
        tt_trk_src,
        tt_trk_number,
        tt_trk_type,

        tt_trk_extensions,      // GPX 1.1
        tt_trk_an_extension,    // Generic GPX 1.1 extensions

        tt_rte,

        tt_trk_trkseg,
        tt_trk_trkseg_trkpt,
        tt_trk_trkseg_trkpt_ele,
        tt_trk_trkseg_trkpt_time,
        tt_trk_trkseg_trkpt_name,
        /* extended */
        tt_trk_trkseg_trkpt_course,        // Not in GPX 1.1 (although can be in extensions in 1.1)
        tt_trk_trkseg_trkpt_speed,         // Not in GPX 1.1 (although can be in extensions in 1.1)
        tt_trk_trkseg_trkpt_fix,
        tt_trk_trkseg_trkpt_sat,

        tt_trk_trkseg_trkpt_hdop,
        tt_trk_trkseg_trkpt_vdop,
        tt_trk_trkseg_trkpt_pdop,

        tt_trk_trkseg_trkpt_extensions,    // GPX 1.1
        tt_trk_trkseg_trkpt_an_extension,  // Generic GPX 1.1 extensions

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

static tag_mapping tag_path_map[] = {

        { tt_gpx, "/gpx" },
        { tt_gpx_name, "/gpx/name" },
        { tt_gpx_desc, "/gpx/desc" },
        { tt_gpx_author, "/gpx/author" },
        { tt_gpx_url, "/gpx/url" },
        { tt_gpx_time, "/gpx/time" },
        { tt_gpx_keywords, "/gpx/keywords" },

        // GPX 1.1 variant - basic properties moved into metadata namespace
        { tt_gpx_name, "/gpx/metadata/name" },
        { tt_gpx_desc, "/gpx/metadata/desc" },
        { tt_gpx_author, "/gpx/metadata/author/name" },
        { tt_gpx_time, "/gpx/metadata/time" },
        { tt_gpx_url, "/gpx/metadata/link" },
        { tt_gpx_keywords, "/gpx/metadata/keywords" },

        { tt_wpt, "/gpx/wpt" },

        { tt_waypoint, "/loc/waypoint" },
        { tt_waypoint_coord, "/loc/waypoint/coord" },
        { tt_waypoint_name, "/loc/waypoint/name" },

        { tt_wpt_ele, "/gpx/wpt/ele" },
        { tt_wpt_time, "/gpx/wpt/time" },
        { tt_wpt_name, "/gpx/wpt/name" },
        { tt_wpt_course, "/gpx/wpt/course" },
        { tt_wpt_speed, "/gpx/wpt/speed" },
        { tt_wpt_magvar, "/gpx/wpt/magvar" },
        { tt_wpt_geoidheight, "/gpx/wpt/geoidheight" },
        { tt_wpt_cmt, "/gpx/wpt/cmt" },
        { tt_wpt_desc, "/gpx/wpt/desc" },
        { tt_wpt_src, "/gpx/wpt/src" },
        { tt_wpt_type, "/gpx/wpt/type" },
        { tt_wpt_sym, "/gpx/wpt/sym" },
        { tt_wpt_sym, "/loc/waypoint/type" },
        { tt_wpt_url, "/gpx/wpt/url" },
        { tt_wpt_url_name, "/gpx/wpt/url_name" },            // GPX 1.0 only
        { tt_wpt_link, "/gpx/wpt/link" },                    /* GPX 1.1 */
        { tt_wpt_fix,  "/gpx/wpt/fix" },
        { tt_wpt_sat, "/gpx/wpt/sat" },
        { tt_wpt_hdop, "/gpx/wpt/hdop" },
        { tt_wpt_vdop, "/gpx/wpt/vdop" },
        { tt_wpt_pdop, "/gpx/wpt/pdop" },
        { tt_wpt_ageofdgpsdata, "/gpx/wpt/ageofdgpsdata" },
        { tt_wpt_dgpsid, "/gpx/wpt/dgpsid" },

        { tt_trk, "/gpx/trk" },
        { tt_trk_name, "/gpx/trk/name" },
        { tt_trk_cmt, "/gpx/trk/cmt" },
        { tt_trk_desc, "/gpx/trk/desc" },
        { tt_trk_src, "/gpx/trk/src" },
        { tt_trk_number, "/gpx/trk/number" },
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
        { tt_trk_number, "/gpx/rte/number" },
        { tt_trk_type, "/gpx/rte/type" },  // NB 'Proposed' in GPX 1.0 and properly in 1.1
        { tt_trk_trkseg_trkpt, "/gpx/rte/rtept" },
        { tt_trk_trkseg_trkpt_name, "/gpx/rte/rtept/name" },
        { tt_trk_trkseg_trkpt_ele, "/gpx/rte/rtept/ele" },

        {0}
};


static tag_mapping extension_tag_path_map[] = {
  { tt_trk_trkseg_trkpt_an_extension, "/gpx/trk/trkseg/trkpt/extensions/" }, // Anything in this namespace
  { tt_trk_trkseg_trkpt_extensions, "/gpx/trk/trkseg/trkpt/extensions" },
  { tt_trk_an_extension, "/gpx/trk/extensions/" }, // Anything in this namespace
  { tt_trk_extensions, "/gpx/trk/extensions" },
  { tt_wpt_an_extension, "/gpx/wpt/extensions/" }, // Anything in this namespace
  { tt_wpt_extensions, "/gpx/wpt/extensions" },
  { tt_gpx_an_extension, "/gpx/extensions/" }, // Anything in this namespace
  { tt_gpx_extensions, "/gpx/extensions" },
  { 0 }
};

// Note in this lookup we just compare the first n characters
//  enabling to match with anything in that xml tree
//  since the order of the lookup will find the generic match first
static tag_type get_tag_extension ( const char *tt )
{
  tag_mapping *tm;
  for ( tm = extension_tag_path_map; tm->tag_type != 0; tm++ )
    if ( 0 == g_ascii_strncasecmp(tm->tag_name, tt, strlen(tm->tag_name)) )
      return tm->tag_type;
  return tt_unknown;
}

static tag_type get_tag(const char *t)
{
        tag_mapping *tm;
        for (tm = tag_path_map; tm->tag_type != 0; tm++)
                if (0 == strcmp(tm->tag_name, t))
                        return tm->tag_type;
        return tt_unknown;
}

/******************************************/

static tag_type current_tag = tt_unknown;
static GString *xpath = NULL;

/* current ("c_") objects */
static VikTrackpoint *c_tp = NULL;
static VikWaypoint *c_wp = NULL;
static VikTrack *c_tr = NULL;
static VikTRWMetadata *c_md = NULL;
static GString *c_cdata = NULL;
static GString *c_ext = NULL;
static GString *c_trkpt_ext = NULL;

static gchar *c_wp_name = NULL;
static gchar *c_tr_name = NULL;

/* temporary things so we don't have to create them lots of times */
static const gchar *c_slat, *c_slon;
static struct LatLon c_ll;

/* specialty flags / etc */
static gboolean f_tr_newseg;
static const gchar *c_link = NULL;
static guint unnamed_waypoints = 0;
static guint unnamed_tracks = 0;
static guint unnamed_routes = 0;

typedef struct {
	VikTrwLayer *vtl;
	const gchar *dirpath;
	gboolean append;
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

/**
 * Get the full gpx header,
 *  except for some attributes that we handle separately
 */
static GString *get_header ( const char **attr )
{
  // Rebuild the header per attribute
  GString *gs = g_string_new ( NULL );

  guint count = 0;
  while ( *attr ) {
    // Skip parts the we handle separately
    if ( g_strcmp0(*attr, "version") == 0 ) {
      attr += 2;
      continue;
    }
    if ( g_strcmp0(*attr, "creator") == 0 ) {
      attr += 2;
      continue;
    }

    if ( count % 2 )
      g_string_append_printf ( gs, "\"%s\"", *attr );
    else
      g_string_append_printf ( gs, " %s=", *attr );

    count++;
    attr += 1;
  }
  return gs;
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

// ATM not bothering with any other potential values one might come across e.g. distance
//  Presumably distance would be useful for static workouts e.g. on a running machine
//   but such things aren't the purview of Viking - it is not really a fitness tracker
// Even if some of these other extension values are more fitness related
typedef enum {
  ext_unknown = 0,
  ext_tp_heart_rate,
  ext_tp_cadence,
  ext_tp_speed,
  ext_tp_course,
  ext_tp_temp,
  ext_tp_power,
  ext_trk_color,
} tag_type_ext;

typedef struct tag_mapping_ext {
  tag_type_ext tag_type;
  const char *tag_name;
} tag_mapping_ext;

// NB Could use a hashtable for this lookup but there's not many keys
//  nor is this performed too often, so keep similarily to the rest of this file
// 'gpxdata:*' is typically from Cluetrust extensions gpxdata10.xsd
// 'gpxtpx:*; is typically from Garmin extensions TrackPointExtensionv1.xsd
// gpxx:TrackExtension/gpxx:DisplayColor Garmin GpxExtensionsv3.xsd
// pwr:PowerInWatts / gpxpx:PowerInWatts Garmin PowerExtensionv1.xsd
// gpxd:color JOSM gpx-drawing-extensions-1.0.xsd
static tag_mapping_ext extention_trackpoints_map[] = {
  { ext_tp_heart_rate, "gpxtpx:hr" },
  { ext_tp_heart_rate, "gpxdata:hr" },
  { ext_tp_cadence, "gpxdata:cadence" },
  { ext_tp_cadence, "gpxtpx:cad" },
  { ext_tp_speed, "gpxdata:speed" },
  { ext_tp_speed, "gpxtpx:speed" },
  { ext_tp_course, "gpxtpx:course" },
  { ext_tp_course, "gpxdata:course" },
  { ext_tp_temp, "gpxdata:temp" },
  { ext_tp_temp, "gpxtpx:atemp" },
  { ext_tp_power, "gpxdata:power" }, // e.g. openambit2gpx.py
  { ext_tp_power, "pwr:PowerInWatts" },
  { ext_tp_power, "gpxpx:PowerInWatts" },
  { ext_trk_color, "gpxx:DisplayColor" },
  { ext_trk_color, "gpxd:color" },
  { ext_trk_color, "color" }, // e.g. OsmAnd?
};

static tag_type_ext get_tag_ext_specific (const char *tt)
{
 tag_mapping_ext *tm;
 for ( tm = extention_trackpoints_map; tm->tag_type != 0; tm++ )
   if ( 0 == g_strcmp0(tm->tag_name, tt) )
     return tm->tag_type;
 return ext_unknown;
}

GString *gs_ext;

// Reprocess the extension text to extract tags we handle
static void ext_start_element ( GMarkupParseContext *context,
                                const gchar         *element_name,
                                const gchar        **attribute_names,
                                const gchar        **attribute_values,
                                gpointer             user_data,
                                GError             **error )
{
  g_string_erase ( gs_ext, 0, -1 ); // Reset the tmp string buffer
}

// NB Text is not null terminated
static void ext_text ( GMarkupParseContext *context,
                       const gchar         *text,
                       gsize                text_len,
                       gpointer             user_data,
                       GError             **error )
{
  // Store tag contents
  g_string_append_len ( gs_ext, text, text_len );
}

// Main trackpoint extension processing here
static void ext_end_element ( GMarkupParseContext *context,
                              const gchar         *element_name,
                              gpointer             user_data,
                              GError             **error )
{
  // If it is any of the extended tags we are interested in,
  //  then use the text stored in the string buffer to set the appropriate track or trackpoint value
  tag_type tag = get_tag_ext_specific ( element_name );
  switch ( tag ) {
  case ext_tp_heart_rate:
    if ( c_tp ) c_tp->heart_rate = atoi ( gs_ext->str ); // bpm
    break;
  case ext_tp_cadence:
    if ( c_tp ) c_tp->cadence = atoi ( gs_ext->str ); // RPM
    break;
  case ext_tp_speed:
    if ( c_tp ) c_tp->speed = g_ascii_strtod ( gs_ext->str, NULL ); // m/s
    break;
  case ext_tp_course:
    if ( c_tp ) c_tp->course = g_ascii_strtod ( gs_ext->str, NULL ); // Degrees
    break;
  case ext_tp_temp:
    if ( c_tp ) c_tp->temp = g_ascii_strtod ( gs_ext->str, NULL ); // Degrees Celsius
    break;
  case ext_tp_power:
    if ( c_tp ) c_tp->power = atoi ( gs_ext->str ); // Watts
    break;
  case ext_trk_color:
    if ( c_tr ) {
      GdkColor gclr;
      if ( gdk_color_parse ( gs_ext->str, &gclr ) ) {
        c_tr->has_color = TRUE;
        c_tr->color = gclr;
      }
    }
    break;
  default:
    break;
  }
  g_string_erase ( gs_ext, 0, -1 );
}

GMarkupParser gparser;
GMarkupParseContext *gcontext;

static void track_or_trackpoint_extension_process ( gchar *str )
{
  if ( !str )
    return;

  // Parse xml fragment to extract extension tag values
  GError *error = NULL;
  if ( !g_markup_parse_context_parse ( gcontext, str, strlen(str), &error ) )
    g_warning ( "%s: parse error %s on:%s", __FUNCTION__, error ? error->message : "???", str );

  if ( !g_markup_parse_context_end_parse ( gcontext, &error) )
    g_warning ( "%s: error %s occurred on end of:%s", __FUNCTION__, error ? error->message : "???", str );
}

static void extension_append_attributions ( GString *gs, const char *el, const char **attr )
{
  guint count = 0;
  g_string_append_printf ( gs, "<%s", el );
  while ( *attr ) {
    if ( count % 2 )
      g_string_append_printf ( gs, "\"%s\"", *attr );
    else
      g_string_append_printf ( gs, " %s=", *attr );
    count++;
    attr += 1;
  }
  g_string_append_c ( gs, '>' );
}

static void gpx_start(UserDataT *ud, const char *el, const char **attr)
{
  static const gchar *tmp;
  VikTrwLayer *vtl = ud->vtl;

  g_string_append_c ( xpath, '/' );
  g_string_append ( xpath, el );
  current_tag = get_tag ( xpath->str );
  if ( current_tag == tt_unknown )
    current_tag = get_tag_extension ( xpath->str );

  switch ( current_tag ) {

     case tt_gpx:
       {
         c_md = vik_trw_metadata_new();
         // Store creator information if possible
         const gchar *crt = get_attr ( attr, "creator" );
         if ( crt ) {
           // If there is an actual description field it will overwrite this value
           c_md->description = g_strdup_printf ( _("Created by: %s"), crt );
         }

         const gchar *version = get_attr ( attr, "version" );
         // When appending a file to a layer,
         //  don't downgrade from 1.1 -> 1.0,
         //  but allow going from 1.0 -> 1.1
         // For new layers always apply the version
         gpx_version_t gvt = GPX_V1_1; // Default
         if ( g_strcmp0(version, "1.0") == 0 )
           gvt = GPX_V1_0;
         if ( ud->append ) {
           if ( vik_trw_layer_get_gpx_version(vtl) == GPX_V1_0 )
             vik_trw_layer_set_gpx_version ( vtl, gvt );
         }
         else
           vik_trw_layer_set_gpx_version ( vtl, gvt );

	 GString *gs = get_header ( attr );
	 vik_trw_layer_set_gpx_header ( vtl, gs->str );
	 g_string_free ( gs, FALSE ); // NB string now owned by vtl
       }
       break;
     case tt_wpt:
       if ( set_c_ll( attr ) ) {
         c_wp = vik_waypoint_new ();
         if ( get_attr ( attr, "hidden" ) )
           c_wp->visible = FALSE;

         vik_coord_load_from_latlon ( &(c_wp->coord), vik_trw_layer_get_coord_mode ( vtl ), &c_ll );
       }
       break;

     case tt_trk:
     case tt_rte:
       c_tr = vik_track_new ();
       c_tr->is_route = (current_tag == tt_rte) ? TRUE : FALSE;
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

     case tt_gpx_url:
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
     case tt_wpt_course:
     case tt_wpt_speed:
     case tt_wpt_magvar:
     case tt_wpt_geoidheight:
     case tt_wpt_url:
     case tt_wpt_url_name:
     case tt_wpt_fix:
     case tt_wpt_sat:
     case tt_wpt_hdop:
     case tt_wpt_vdop:
     case tt_wpt_pdop:
     case tt_wpt_ageofdgpsdata:
     case tt_wpt_dgpsid:
     case tt_trk_cmt:
     case tt_trk_desc:
     case tt_trk_src:
     case tt_trk_number:
     case tt_trk_type:
     case tt_trk_name:
       g_string_erase ( c_cdata, 0, -1 ); /* clear the cdata buffer */
       break;

     case tt_waypoint:
       c_wp = vik_waypoint_new ();
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
        
     case tt_gpx_extensions:
     case tt_wpt_extensions:
     case tt_trk_extensions:
       g_string_erase ( c_ext, 0, -1 ); // clear the buffer
       break;      
     case tt_trk_trkseg_trkpt_extensions:
       g_string_erase ( c_trkpt_ext, 0, -1 ); // clear the buffer
       break;
     case tt_gpx_an_extension:
     case tt_wpt_an_extension:
     case tt_trk_an_extension:
       extension_append_attributions ( c_ext, el, attr );
       break;
     case tt_trk_trkseg_trkpt_an_extension:
       extension_append_attributions ( c_trkpt_ext, el, attr );
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

     case tt_gpx_url:
       if ( c_md->url )
         g_free ( c_md->url );
       if ( c_link ) {
         c_md->url = g_strdup ( c_link );
         c_link = NULL;
       } else if ( c_cdata->len > 0 ) {
         c_md->url = g_strdup ( c_cdata->str );
         g_string_erase ( c_cdata, 0, -1 );
       }
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

     case tt_wpt_url_name:
       vik_waypoint_set_url_name ( c_wp, c_cdata->str );
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

     case tt_wpt_course:
       c_wp->course = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_speed:
       c_wp->speed = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_magvar:
       c_wp->magvar = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_geoidheight:
       c_wp->geoidheight = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_fix:
       if (!strcmp("2d", c_cdata->str))
         c_wp->fix_mode = VIK_GPS_MODE_2D;
       else if (!strcmp("3d", c_cdata->str))
         c_wp->fix_mode = VIK_GPS_MODE_3D;
       else if (!strcmp("dgps", c_cdata->str))
         c_wp->fix_mode = VIK_GPS_MODE_DGPS;
       else if (!strcmp("pps", c_cdata->str))
         c_wp->fix_mode = VIK_GPS_MODE_PPS;
       else
         c_wp->fix_mode = VIK_GPS_MODE_NOT_SEEN;
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_sat:
       c_wp->nsats = atoi ( c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_hdop:
       c_wp->hdop = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_vdop:
       c_wp->vdop = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_pdop:
       c_wp->pdop = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_ageofdgpsdata:
       c_wp->ageofdgpsdata = g_ascii_strtod ( c_cdata->str, NULL );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_wpt_dgpsid:
       c_wp->dgpsid = atoi ( c_cdata->str );
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

     case tt_trk_number:
       c_tr->number = atoi ( c_cdata->str );
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
	 gdouble d1 = wp_time.tv_sec;
	 gdouble d2 = (gdouble)wp_time.tv_usec/G_USEC_PER_SEC;
         c_wp->timestamp = (d1 < 0) ? d1 - d2 : d1 + d2;
       }
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_name:
       vik_trackpoint_set_name ( c_tp, c_cdata->str );
       g_string_erase ( c_cdata, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_time:
       if ( g_time_val_from_iso8601(c_cdata->str, &tp_time) ) {
	 gdouble d1 = tp_time.tv_sec;
	 gdouble d2 = (gdouble)tp_time.tv_usec/G_USEC_PER_SEC;
         c_tp->timestamp = (d1 < 0) ? d1 - d2 : d1 + d2;
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

     case tt_gpx_an_extension:
     case tt_wpt_an_extension:
     case tt_trk_an_extension:
       g_string_append_printf ( c_ext, "</%s>", el );
       break;
     case tt_trk_trkseg_trkpt_an_extension:
       g_string_append_printf ( c_trkpt_ext, "</%s>", el );
       break;

     case tt_trk_extensions:
       if ( current_tag == tt_trk_extensions )
         track_or_trackpoint_extension_process ( c_ext->str );
       vik_track_set_extensions ( c_tr, c_ext->str );
       g_string_erase ( c_ext, 0, -1 );
       break;

     case tt_gpx_extensions:
       vik_trw_layer_set_gpx_extensions ( vtl, c_ext->str );
       g_string_erase ( c_ext, 0, -1 );
       break;

     case tt_wpt_extensions:
       vik_waypoint_set_extensions ( c_wp, c_ext->str );
       g_string_erase ( c_ext, 0, -1 );
       break;

     case tt_trk_trkseg_trkpt_extensions:
       vik_trackpoint_set_extensions ( c_tp, c_trkpt_ext->str );
       track_or_trackpoint_extension_process ( c_trkpt_ext->str );
       g_string_erase ( c_trkpt_ext, 0, -1 );
       break;

     default: break;
  }

  current_tag = get_tag ( xpath->str );
  if ( current_tag == tt_unknown )
    current_tag = get_tag_extension ( xpath->str );
}

static void gpx_cdata(void *dta, const XML_Char *s, int len)
{
  switch ( current_tag ) {
    case tt_gpx_name:
    case tt_gpx_author:
    case tt_gpx_desc:
    case tt_gpx_keywords:
    case tt_gpx_time:
    case tt_gpx_url:
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
    case tt_wpt_url_name:
    case tt_wpt_link:
    case tt_wpt_course:
    case tt_wpt_speed:
    case tt_wpt_magvar:
    case tt_wpt_geoidheight:
    case tt_wpt_fix:
    case tt_wpt_sat:
    case tt_wpt_hdop:
    case tt_wpt_vdop:
    case tt_wpt_pdop:
    case tt_wpt_ageofdgpsdata:
    case tt_wpt_dgpsid:
    case tt_trk_cmt:
    case tt_trk_desc:
    case tt_trk_src:
    case tt_trk_number:
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

    case tt_trk_trkseg_trkpt_an_extension:
    case tt_trk_trkseg_trkpt_extensions:
      g_string_append_len ( c_trkpt_ext, s, len );
      break;
    case tt_trk_an_extension:
    case tt_trk_extensions:
    case tt_wpt_an_extension:
    case tt_wpt_extensions:
    case tt_gpx_an_extension:
    case tt_gpx_extensions:
      g_string_append_len ( c_ext, s, len );
      break;

    default: break;  /* ignore cdata from other things */
  }
}

// make like a "stack" of tag names
// like gpspoint's separated like /gpx/wpt/whatever
// @append: Whether the read is to append to the vtl (or otherwise a new layer)
//  i.e. primarily to decide what to do regarding appending files with different GPX versions
gboolean a_gpx_read_file( VikTrwLayer *vtl, FILE *f, const gchar* dirpath, gboolean append ) {
  XML_Parser parser = XML_ParserCreate(NULL);
  int done=0, len;
  enum XML_Status status = XML_STATUS_ERROR;

  UserDataT *ud = g_malloc (sizeof(UserDataT));
  ud->vtl     = vtl;
  ud->dirpath = dirpath;
  ud->append  = append;

  XML_SetElementHandler(parser, (XML_StartElementHandler) gpx_start, (XML_EndElementHandler) gpx_end);
  XML_SetUserData(parser, ud);
  XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) gpx_cdata);

  // Secondary parser for trackpoint extension fragments
  //  seems to work better on xml fragments compared to expat,
  //  and also we can reuse a single parser,
  //  rather than having to create an expat parser each time on each <extension> tag group
  gparser.start_element = &ext_start_element;
  gparser.end_element = &ext_end_element;
  gparser.text = &ext_text;
  gparser.passthrough = NULL;
  gparser.error = NULL;
  gcontext = g_markup_parse_context_new ( &gparser, 0, NULL, NULL );

  gchar buf[4096];

  g_assert ( f != NULL && vtl != NULL );

  xpath = g_string_new ( "" );
  c_cdata = g_string_new ( "" );
  c_ext = g_string_new ( NULL );
  c_trkpt_ext = g_string_new ( NULL );
  gs_ext = g_string_new ( NULL );

  unnamed_waypoints = 1;
  unnamed_tracks = 1;
  unnamed_routes = 1;

  while (!done) {
    len = fread(buf, 1, sizeof(buf)-7, f);
    done = feof(f) || !len;
    status = XML_Parse(parser, buf, len, done);
  }

  gboolean ans = (status != XML_STATUS_ERROR);
  if ( !ans ) {
    g_warning ( "%s: XML error %s at line %ld", __FUNCTION__, XML_ErrorString(XML_GetErrorCode(parser)), XML_GetCurrentLineNumber(parser) );
  }

  XML_ParserFree (parser);
  g_free ( ud );
  g_string_free ( xpath, TRUE );
  g_string_free ( c_cdata, TRUE );
  g_string_free ( gs_ext, TRUE );
  g_markup_parse_context_free ( gcontext );

  return ans;
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

typedef struct {
  const char *rgb;
  const char *color_name;
} allowed_color_names_t;

// Names from GpxExtensionsv3.xsd
// RGB values taken from gdk_color_parse()
// Then stored here so we can perform a lookup to find a nearest match given any RGB
allowed_color_names_t allowed_color_names[] = {
  { "#000000", "Black" },
  { "#8B0000", "DarkRed" },
  { "#013220", "DarkGreen" },
  { "#9B870C", "DarkYellow" },
  { "#00008B", "DarkBlue" },
  { "#8B008B", "DarkMagenta" },
  { "#008B8B", "DarkCyan" },
  { "#D3D3D3", "LightGray" },
  { "#A9A9A9", "DarkGray" },
  { "#FF0000", "Red" },
  { "#00FF00", "Green" },
  { "#FFFF00", "Yellow" },
  { "#0000FF", "Blue" },
  { "#FF00FF", "Magenta" },
  { "#00FFFF", "Cyan" },
  { "#FFFFFF", "White" },
  // NB No support for "Transparent"
  { NULL, NULL }
};

/**
 * Currently only Garmin DisplayColor is supported
 *
 * Look for a colour that matches by the sum of distance squared for each RGB value
 */
static const gchar * nearest_colour_string ( GdkColor color )
{
  // Furthest away default value
  gdouble distance = (255.0 * sqrt ( 3.0 ) );

  guint ii = 0;
  guint answer = 0;
  for ( allowed_color_names_t *acn = allowed_color_names; acn->rgb; acn++) {
    GdkColor ac;
    if ( gdk_color_parse ( acn->rgb, &ac ) ) {
      // First check for exact matches
      if ( (ac.red == color.red) && (ac.green == color.green) && (ac.blue == color.blue) ) {
        answer = ii;
        break;
      }
      else {
        gdouble newdist = sqrt ( pow ((ac.red/256 - color.red/256), 2 ) +
                                 pow ((ac.green/256 - color.green/256), 2 ) +
                                 pow ((ac.blue/256 - color.blue/256), 2 ) );
        if ( newdist < distance ) {
          answer = ii;
          distance = newdist;
        }
      }
    }
    ii++;
  }
  return allowed_color_names[answer].color_name;
}

static void write_double ( FILE *ff, guint spaces, const gchar *tag, gdouble value )
{
  if ( !isnan(value) ) {
    gchar buf[COORDS_STR_BUFFER_SIZE];
    a_coords_dtostr_buffer ( value, buf );
    fprintf ( ff, "%*s<%s>%s</%s>\n", spaces, "", tag, buf, tag );
  }
}

// Value must positive to be written otherwise it is ignored
static void write_positive_uint ( FILE *ff, guint spaces, const gchar *tag, guint value )
{
  if ( value )
    fprintf ( ff, "%*s<%s>%d</%s>\n", spaces, "", tag, value, tag );
}

static void write_string ( FILE *ff, guint spaces, const gchar *tag, const gchar *value )
{
  if ( value && strlen(value) ) {
    gchar *tmp = entitize ( value );
    fprintf ( ff, "%*s<%s>%s</%s>\n", spaces, "", tag, tmp, tag );
    g_free ( tmp );
  }
}

static void write_string_as_is ( FILE *ff, guint spaces, const gchar *tag, const gchar *value )
{
  if ( value && strlen(value) ) {
    fprintf ( ff, "%*s<%s>%s</%s>\n", spaces, "", tag, value, tag );
  }
}

#define WPT_SPACES 2

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

  write_double ( f, WPT_SPACES, "ele", wp->altitude );

  if ( !isnan(wp->timestamp) ) {
    GTimeVal timestamp;
    timestamp.tv_sec = wp->timestamp;
    timestamp.tv_usec = abs((wp->timestamp-(gint64)wp->timestamp)*G_USEC_PER_SEC);

    gchar *time_iso8601 = g_time_val_to_iso8601 ( &timestamp );
    if ( time_iso8601 != NULL )
      fprintf ( f, "  <time>%s</time>\n", time_iso8601 );
    g_free ( time_iso8601 );
  }

  if ( !context->options || (context->options && context->options->version == GPX_V1_0) ) {
    write_double ( f, WPT_SPACES, "course", wp->course );
    write_double ( f, WPT_SPACES, "speed", wp->speed );
  }
  write_double ( f, WPT_SPACES, "magvar", wp->magvar );
  write_double ( f, WPT_SPACES, "geoidheight", wp->geoidheight );

  // Sanity clause
  if ( wp->name )
    tmp = entitize ( wp->name );
  else
    tmp = g_strdup ("waypoint");

  fprintf ( f, "  <name>%s</name>\n", tmp );
  g_free ( tmp);

  write_string ( f, WPT_SPACES, "cmt", wp->comment );
  write_string ( f, WPT_SPACES, "desc", wp->description );
  write_string ( f, WPT_SPACES, "src", wp->source );

  if ( wp->url && context->options && context->options->version == GPX_V1_1 ) {
    fprintf ( f, "  <link href=\"%s\"></link>\n", wp->url );
  } else {
    write_string ( f, WPT_SPACES, "url", wp->url );
    write_string ( f, WPT_SPACES, "urlname", wp->url_name );
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
  write_string ( f, WPT_SPACES, "type", wp->type );

  if ( wp->fix_mode == VIK_GPS_MODE_2D )
    fprintf ( f, "  <fix>2d</fix>\n" );
  else if ( wp->fix_mode == VIK_GPS_MODE_3D )
    fprintf ( f, "  <fix>3d</fix>\n" );
  else if ( wp->fix_mode == VIK_GPS_MODE_DGPS )
    fprintf ( f, "  <fix>dgps</fix>\n" );
  else if ( wp->fix_mode == VIK_GPS_MODE_PPS )
    fprintf ( f, "  <fix>pps</fix>\n" );

  write_positive_uint ( f, WPT_SPACES, "sat", wp->nsats );
  write_double ( f, WPT_SPACES, "hdop", wp->hdop );
  write_double ( f, WPT_SPACES, "vdop", wp->vdop );
  write_double ( f, WPT_SPACES, "pdop", wp->pdop );
  write_double ( f, WPT_SPACES, "ageofdgpsdata", wp->ageofdgpsdata );
  write_positive_uint ( f, WPT_SPACES, "dgpsid", wp->dgpsid );

  write_string_as_is ( f, WPT_SPACES, "extensions", wp->extensions );

  fprintf ( f, "</wpt>\n" );
}

#define TRKPT_SPACES 4
#define TRKPT_EXT_SPACES 8
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
  gchar *time_iso8601;
  vik_coord_to_latlon ( &(tp->coord), &ll );

  // No such thing as a rteseg! So make sure we don't put them in
  if ( context->options && !context->options->is_route && tp->newsegment )
    fprintf ( f, "  </trkseg>\n  <trkseg>\n" );

  a_coords_dtostr_buffer ( ll.lat, s_lat );
  a_coords_dtostr_buffer ( ll.lon, s_lon );
  fprintf ( f, "  <%spt lat=\"%s\" lon=\"%s\">\n", (context->options && context->options->is_route) ? "rte" : "trk", s_lat, s_lon );

  if ( !isnan(tp->altitude) )
  {
    a_coords_dtostr_buffer ( tp->altitude, s_alt );
    fprintf ( f, "    <ele>%s</ele>\n", s_alt );
  }
  else if ( context->options != NULL && context->options->force_ele )
  {
    fprintf ( f, "    <ele>0</ele>\n" );
  }
  
  time_iso8601 = NULL;
  if ( !isnan(tp->timestamp) ) {
    GTimeVal timestamp;
    timestamp.tv_sec = tp->timestamp;
    timestamp.tv_usec = abs((tp->timestamp-(gint64)tp->timestamp)*G_USEC_PER_SEC);
  
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

  if ( !context->options || (context->options && context->options->version == GPX_V1_0) ) {
    write_double ( f, TRKPT_SPACES, "course", tp->course );
    write_double ( f, TRKPT_SPACES, "speed", tp->speed );
  }
  write_string ( f, TRKPT_SPACES, "name", tp->name );

  if (tp->fix_mode == VIK_GPS_MODE_2D)
    fprintf ( f, "    <fix>2d</fix>\n");
  if (tp->fix_mode == VIK_GPS_MODE_3D)
    fprintf ( f, "    <fix>3d</fix>\n");
  if (tp->fix_mode == VIK_GPS_MODE_DGPS)
    fprintf ( f, "    <fix>dgps</fix>\n");
  if (tp->fix_mode == VIK_GPS_MODE_PPS)
    fprintf ( f, "    <fix>pps</fix>\n");

  write_positive_uint ( f, TRKPT_SPACES, "sat", tp->nsats );
  write_double ( f, TRKPT_SPACES, "hdop", tp->hdop );
  write_double ( f, TRKPT_SPACES, "vdop", tp->vdop );
  write_double ( f, TRKPT_SPACES, "pdop", tp->pdop );

  // If have the raw extensions - then save that (which should include all of the individual values we use)
  if ( tp->extensions )
    write_string_as_is ( f, TRKPT_SPACES, "extensions", tp->extensions );
  else {
    // Otherwise write the individual values we are supporting (in Garmin TrackPointExtension/v2 format)
    if ( context->options && context->options->version == GPX_V1_1 ) {
      if ( !isnan(tp->speed) || !isnan(tp->course) ||
           !isnan(tp->temp) || tp->heart_rate || tp->cadence != VIK_TRKPT_CADENCE_NONE ) {
        fprintf ( f, "    <extensions>\n");
        fprintf ( f, "      <gpxtpx:TrackPointExtension>\n");
        write_double ( f, TRKPT_EXT_SPACES, "gpxtpx:atemp", tp->temp );
        write_positive_uint ( f, TRKPT_EXT_SPACES, "gpxtpx:hr", tp->heart_rate );
        if ( tp->cadence != VIK_TRKPT_CADENCE_NONE )
          fprintf ( f, "%*s<%s>%d</%s>\n", TRKPT_EXT_SPACES, "", "gpxtpx:cad", tp->cadence, "gpxtpx:cad" );
        write_double ( f, TRKPT_EXT_SPACES, "gpxtpx:speed", tp->speed );
        write_double ( f, TRKPT_EXT_SPACES, "gpxtpx:course", tp->course );
        fprintf ( f, "      </gpxtpx:TrackPointExtension>\n");
        fprintf ( f, "    </extensions>\n");
      }
    }
  }
  fprintf ( f, "  </%spt>\n", (context->options && context->options->is_route) ? "rte" : "trk" );
}

#define TRK_SPACES 2

static void write_track_extension_color_only ( FILE *ff, VikTrack *trk )
{
  fprintf ( ff, "  <extensions><gpxx:TrackExtension><gpxx:DisplayColor>%s</gpxx:DisplayColor></gpxx:TrackExtension></extensions>\n", nearest_colour_string(trk->color) );
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

  write_string ( f, TRK_SPACES, "cmt", t->comment );
  write_string ( f, TRK_SPACES, "desc", t->description );
  write_string ( f, TRK_SPACES, "src", t->source );
  write_positive_uint ( f, TRK_SPACES, "number", t->number );
  write_string ( f, TRK_SPACES, "type", t->type );

  // ATM Track Colour is the only extension Viking supports editing
  //  thus if there is some other track extension Viking will not add in the color,
  //  but will add it if there is no current track extension.
  if ( t->extensions ) {
    gboolean write_as_is = !t->has_color;
    if ( !write_as_is ) {
      gchar* text = g_strdup ( t->extensions );
      g_strstrip ( text );
      if ( g_str_has_prefix(text, "<gpxx:TrackExtension><gpxx:DisplayColor>") ) {
        if ( g_str_has_suffix(text, "</gpxx:DisplayColor></gpxx:TrackExtension>") )
          write_track_extension_color_only ( f, t );
        else
          write_as_is = TRUE;
      }
      else
        write_as_is = TRUE;
      g_free ( text );
    }
    if ( write_as_is )
      write_string_as_is ( f, TRK_SPACES, "extensions", t->extensions );
  }
  else {
    if ( context->options && context->options->version == GPX_V1_1 )
      if ( t->has_color )
        write_track_extension_color_only ( f, t );
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

static void gpx_write_header( FILE *f, VikTrwLayer *vtl, GpxWritingContext *context )
{
  // Allow overriding the creator value
  // E.g. if something actually cares about it, see for example:
  //   http://strava.github.io/api/v3/uploads/
  gchar *creator = g_strdup(a_vik_gpx_export_creator());
  if ( g_strcmp0(creator, "") == 0 ) {
    g_free(creator);
    creator = g_strdup_printf("Viking %s -- %s", PACKAGE_VERSION, PACKAGE_URL);
  }

  fprintf(f, "<?xml version=\"1.0\"?>\n");
  if ( context->options && context->options->version == GPX_V1_1 ) {
    fprintf(f, "<gpx version=\"1.1\"\n");
    fprintf(f, "creator=\"%s\"\n", creator);
    // If we already have a ready to use header then use that
    gchar *header = NULL;
    if ( vtl )
      header = vik_trw_layer_get_gpx_header ( vtl );
    if ( header )
      fprintf(f, "%s%s", header, ">\n");
    else
      // Otherwise write a load of xmlns stuff, even if we don't actually end up using any extensions
      fprintf(f, "xmlns=\"http://www.topografix.com/GPX/1/1\" "
                 "xmlns:gpxx=\"http://www.garmin.com/xmlschemas/GpxExtensions/v3\" "
                 "xmlns:wptx1=\"http://www.garmin.com/xmlschemas/WaypointExtension/v1\" "
                 "xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v2\" "
                 "xmlns:gpxpx=\"http://www.garmin.com/xmlschemas/PowerExtension/v1\" "
                 "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                 "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd http://www.garmin.com/xmlschemas/GpxExtensions/v3 http://www8.garmin.com/xmlschemas/GpxExtensionsv3.xsd http://www.garmin.com/xmlschemas/WaypointExtension/v1 http://www8.garmin.com/xmlschemas/WaypointExtensionv1.xsd http://www.garmin.com/xmlschemas/TrackPointExtension/v2 http://www.garmin.com/xmlschemas/TrackPointExtensionv2.xsd http://www.garmin.com/xmlschemas/PowerExtensionv1.xsd\">\n");
  } else {
    fprintf(f, "<gpx version=\"1.0\"\n");
    fprintf(f, "creator=\"%s\"\n", creator);
    fprintf(f,"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
              "xmlns=\"http://www.topografix.com/GPX/1/0\"\n"
              "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n");
  }
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

  gpx_write_header ( f, vtl, &context );

  //gchar *tmp;
  const gchar *name = vik_layer_get_name(VIK_LAYER(vtl));
  write_string ( f, TRK_SPACES, "name", name );

  VikTRWMetadata *md = vik_trw_layer_get_metadata (vtl);
  if ( md ) {
    if ( options && options->version == GPX_V1_1 ) {
      fprintf ( f, "  <metadata>\n" );
      if ( md->author && strlen(md->author) > 0 )
        fprintf ( f, "    <author><name>%s</name></author>\n", md->author );
      write_string ( f, 4, "desc", md->description );
      if ( md->url && strlen(md->url) > 0 )
        fprintf ( f, "    <link href=\"%s\"></link>\n", md->url );
      write_string ( f, 4, "keywords", md->keywords );
      fprintf ( f, "  </metadata>\n" );
    }
    else {
      write_string ( f, TRK_SPACES, "author", md->author );
      write_string ( f, TRK_SPACES, "desc", md->description );
      write_string ( f, TRK_SPACES, "url", md->url );
      write_string ( f, TRK_SPACES, "time", md->timestamp );
      write_string ( f, TRK_SPACES, "keywords", md->keywords );
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
  GpxWritingOptions opt_tmp = { FALSE, FALSE, FALSE, FALSE, vik_trw_layer_get_gpx_version(vtl) };
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

  if ( opt_tmp.version == GPX_V1_1 ) {
    gchar *ext = vik_trw_layer_get_gpx_extensions ( vtl );
    if ( ext )
      write_string_as_is ( f, 0, "extensions", ext );
  }

  gpx_write_footer ( f );
}

void a_gpx_write_track_file ( VikTrack *trk, FILE *f, GpxWritingOptions *options )
{
  GpxWritingContext context = {options, f};
  gpx_write_header ( f, NULL, &context );
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
