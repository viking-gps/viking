/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2020, Rob Norris <rw_norris@hotmail.com>
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

// Code inspired from gpx.[ch] file implementation
// TCX Specification:
// https://www8.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd
// http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2

#include "tcx.h"
#include "viking.h"
#include <expat.h>

typedef enum {
	tt_unknown = 0,

	tt_tcx,
	tt_tcx_name,
	tt_tcx_cmt,
	tt_tcx_creator,

	tt_wpt,
	tt_wpt_cmt,
	tt_wpt_name,
	tt_wpt_ele,
	tt_wpt_time,
	tt_wpt_pos_lat,
	tt_wpt_pos_lon,

	tt_trk,
	tt_trk_trkseg_trkpt,
	tt_trk_trkseg_trkpt_pos_lat,
	tt_trk_trkseg_trkpt_pos_lon,
	tt_trk_trkseg_trkpt_ele,
	tt_trk_trkseg_trkpt_time,
	tt_trk_trkseg_trkpt_cadence,
	tt_trk_trkseg_trkpt_hr,
	tt_trk_trkseg_trkpt_power,
	tt_trk_trkseg_trkpt_speed,
} tag_type;

typedef struct tag_mapping {
	tag_type tag_type;     // enum from above for this tag
	const char *tag_name;  // xpath-ish tag name
} tag_mapping;

// NB A TCX file can contain multiple courses, which we'll treat as seperate TRW layers
// Each course can contain multiple tracks (which appear *not* to have an individual name)
// The course notes and creator will be put in a TRW layer metadata block
static tag_mapping tag_path_map[] = {
	{ tt_tcx, "/TrainingCenterDatabase/Courses/Course" },
	{ tt_tcx_name, "/TrainingCenterDatabase/Courses/Course/Name" },
	{ tt_tcx_cmt, "/TrainingCenterDatabase/Courses/Course/Notes" },
	{ tt_tcx_creator, "/TrainingCenterDatabase/Courses/Course/Creator/Name" },

	{ tt_wpt, "/TrainingCenterDatabase/Courses/Course/CoursePoint" },
	{ tt_wpt_ele, "/TrainingCenterDatabase/Courses/Course/CoursePoint/AltitudeMeters" },
	{ tt_wpt_time, "/TrainingCenterDatabase/Courses/Course/CoursePoint/Time" },
	{ tt_wpt_name, "/TrainingCenterDatabase/Courses/Course/CoursePoint/Name" },
	{ tt_wpt_cmt, "/TrainingCenterDatabase/Courses/Course/CoursePoint/Notes" },
	{ tt_wpt_pos_lat, "/TrainingCenterDatabase/Courses/Course/CoursePoint/Position/LatitudeDegrees" },
	{ tt_wpt_pos_lon, "/TrainingCenterDatabase/Courses/Course/CoursePoint/Position/LongitudeDegrees" },

//  Main kinds of TCX files this author typically encounters
	{ tt_trk,                      "/TrainingCenterDatabase/Courses/Course/Track" },
	{ tt_trk_trkseg_trkpt,         "/TrainingCenterDatabase/Courses/Course/Track/Trackpoint" },
	{ tt_trk_trkseg_trkpt_ele,     "/TrainingCenterDatabase/Courses/Course/Track/Trackpoint/AltitudeMeters" },
	{ tt_trk_trkseg_trkpt_time,    "/TrainingCenterDatabase/Courses/Course/Track/Trackpoint/Time" },
	{ tt_trk_trkseg_trkpt_pos_lat, "/TrainingCenterDatabase/Courses/Course/Track/Trackpoint/Position/LatitudeDegrees" },
	{ tt_trk_trkseg_trkpt_pos_lon, "/TrainingCenterDatabase/Courses/Course/Track/Trackpoint/Position/LongitudeDegrees" },

// There are repeats of Track elements but at a different XML layout tree position
//  currently don't care about various sport types
	{ tt_tcx,                      "/TrainingCenterDatabase/Activities/Activity/Lap" },
	{ tt_tcx_cmt,                  "/TrainingCenterDatabase/Activities/Activity/Notes" },
	{ tt_trk,                      "/TrainingCenterDatabase/Activities/Activity/Lap/Track" },
	{ tt_trk_trkseg_trkpt,         "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint" },
	{ tt_trk_trkseg_trkpt_ele,     "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/AltitudeMeters" },
	{ tt_trk_trkseg_trkpt_time,    "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Time" },
	{ tt_trk_trkseg_trkpt_pos_lat, "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Position/LatitudeDegrees" },
	{ tt_trk_trkseg_trkpt_pos_lon, "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Position/LongitudeDegrees" },
	{ tt_trk_trkseg_trkpt_cadence, "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Cadence" },
	{ tt_trk_trkseg_trkpt_cadence, "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Extensions/RunCadence" },
	{ tt_trk_trkseg_trkpt_hr,      "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/HeartRateBpm/Value" },
	{ tt_trk_trkseg_trkpt_power,   "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Extensions/ns3:TPX/ns3:Watts" },
	{ tt_trk_trkseg_trkpt_power,   "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Extensions/Watts" },
	{ tt_trk_trkseg_trkpt_speed,   "/TrainingCenterDatabase/Activities/Activity/Lap/Track/Trackpoint/Extensions/ns3:TPX/ns3:Speed" },

	{0}
};

static tag_type get_tag ( const char *tt )
{
	tag_mapping *tm;
	for ( tm = tag_path_map; tm->tag_type != 0; tm++ )
		if ( 0 == strcmp(tm->tag_name, tt) )
			return tm->tag_type;
	return tt_unknown;
}

static tag_type current_tag = tt_unknown;
static GString *xpath = NULL;
static GString *c_cdata = NULL;

// current ("c_") objects
static VikTrackpoint *c_tp = NULL;
static VikWaypoint *c_wp = NULL;
static VikTrack *c_tr = NULL;
static VikTrwLayer *c_vtl = NULL;
static VikTRWMetadata *c_md = NULL;

static gchar *c_wp_name = NULL;
static gchar *c_tr_name = NULL;
static gboolean has_layer_name = FALSE;

// temporary things so we don't have to create them lots of times
static struct LatLon c_ll;

// specialty flags / etc
static gboolean f_tr_newseg;
static guint unnamed_waypoints = 0;
static guint unnamed_tracks = 0;
static guint unnamed_layers = 0;

typedef struct {
	VikAggregateLayer *val;
	VikViewport *vvp;
	const gchar *filename;
} UserDataT;

static void tcx_start ( UserDataT *ud, const char *el, const char **attr )
{
	g_string_append_c ( xpath, '/' );
	g_string_append ( xpath, el );
	current_tag = get_tag ( xpath->str );

	switch ( current_tag ) {

		case tt_tcx: {
			c_vtl = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, ud->vvp, FALSE ));
			// Always force V1.1, since we may read in 'extended' data like cadence, etc...
			vik_trw_layer_set_gpx_version ( c_vtl, GPX_V1_1 );
			c_md = vik_trw_metadata_new();
			break;
		}

		case tt_wpt:
			c_wp = vik_waypoint_new ();
			c_ll.lat = NAN;
			c_ll.lon = NAN;
			break;

		case tt_trk:
			c_tr = vik_track_new ();
			f_tr_newseg = TRUE;
			break;

		case tt_trk_trkseg_trkpt:
			c_tp = vik_trackpoint_new ();
			c_ll.lat = NAN;
			c_ll.lon = NAN;
			break;

		case tt_tcx_creator:
		case tt_tcx_cmt:
		case tt_tcx_name:
		case tt_trk_trkseg_trkpt_pos_lat:
		case tt_trk_trkseg_trkpt_pos_lon:
		case tt_trk_trkseg_trkpt_ele:
		case tt_trk_trkseg_trkpt_time:
		case tt_trk_trkseg_trkpt_cadence:
		case tt_trk_trkseg_trkpt_hr:
		case tt_trk_trkseg_trkpt_power:
		case tt_trk_trkseg_trkpt_speed:
		case tt_wpt_cmt:
		case tt_wpt_name:
		case tt_wpt_ele:
		case tt_wpt_time:
		case tt_wpt_pos_lat:
		case tt_wpt_pos_lon:
			g_string_erase ( c_cdata, 0, -1 ); // clear the cdata buffer
			break;

		default: break;
	}
}

static void tcx_end ( UserDataT *ud, const char *el )
{
	static GTimeVal tp_time;
	static GTimeVal wp_time;
	VikTrwLayer *vtl = c_vtl; 

	g_string_truncate ( xpath, xpath->len - strlen(el) - 1 );

	switch ( current_tag ) {

		case tt_tcx:
			if ( c_vtl ) {
				if ( vik_trw_layer_is_empty(c_vtl) ) {
					// free up layer
					g_warning ( "%s: No useable geo data found in %s", __FUNCTION__, vik_layer_get_name(VIK_LAYER(c_vtl)) );
					g_object_unref ( c_vtl );
				} else {
					// Add it
					if ( !has_layer_name ) {
						unnamed_layers++;
						gchar *name = g_strdup_printf ( "%s %04d", a_file_basename(ud->filename), unnamed_layers );
						vik_layer_rename ( VIK_LAYER(c_vtl), name );
						g_free ( name );
					}
					vik_layer_post_read ( VIK_LAYER(c_vtl), ud->vvp, TRUE );
					vik_aggregate_layer_add_layer ( ud->val, VIK_LAYER(c_vtl), FALSE );
					vik_trw_layer_set_metadata ( c_vtl, c_md );
					// TODO - only really need to do this once at the end on the aggregate layer, but no functionality for this yet
					vik_trw_layer_auto_set_view ( c_vtl, ud->vvp );
				}
				c_md = NULL;
				c_vtl = NULL;
				has_layer_name = FALSE;
			}
			break;

		case tt_tcx_name:
			if ( c_vtl ) {
				vik_layer_rename ( VIK_LAYER(c_vtl), c_cdata->str );
				has_layer_name = TRUE;
			}
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_tcx_creator:
			if ( c_md ) {
				if ( c_md->author )
					g_free ( c_md->author );
				c_md->author = g_strdup ( c_cdata->str );
			}
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_tcx_cmt:
			if ( c_md ) {
				if ( c_md->description )
					g_free ( c_md->description );
				c_md->description = g_strdup ( c_cdata->str );
			}
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_wpt:
			if ( !c_wp_name )
				c_wp_name = g_strdup_printf ( _("Waypoint%04d"), unnamed_waypoints++ );

			if ( !isnan(c_ll.lat) && !isnan(c_ll.lon) ) {
				vik_coord_load_from_latlon ( &(c_wp->coord), vik_trw_layer_get_coord_mode(vtl), &c_ll );
				vik_trw_layer_filein_add_waypoint ( vtl, c_wp_name, c_wp );
			} else {
				g_warning ( "%s: Missing a coordinate value for %s", __FUNCTION__, c_wp_name );
				vik_waypoint_free ( c_wp ); 
			}

			g_free ( c_wp_name );
			c_wp = NULL;
			c_wp_name = NULL;
			break;

		case tt_trk:
			if ( c_vtl ) {
				c_tr_name = g_strdup_printf ( _("Track%03d"), unnamed_tracks++ );
				c_tr->trackpoints = g_list_reverse ( c_tr->trackpoints );
				vik_trw_layer_filein_add_track ( vtl, c_tr_name, c_tr );
			}
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

		case tt_wpt_ele:
			c_wp->altitude = g_ascii_strtod ( c_cdata->str, NULL );
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_trk_trkseg_trkpt_ele:
			c_tp->altitude = g_ascii_strtod ( c_cdata->str, NULL );
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_wpt_cmt:
			vik_waypoint_set_comment ( c_wp, c_cdata->str );
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

		case tt_trk_trkseg_trkpt_time:
			if ( g_time_val_from_iso8601(c_cdata->str, &tp_time) ) {
				gdouble d1 = tp_time.tv_sec;
				gdouble d2 = (gdouble)tp_time.tv_usec/G_USEC_PER_SEC;
				c_tp->timestamp = (d1 < 0) ? d1 - d2 : d1 + d2;
			}
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_trk_trkseg_trkpt_pos_lat: {
			gdouble dd = g_ascii_strtod ( c_cdata->str, NULL );
			if ( dd < -90.0 || dd > 90.0 )
				g_warning ( "%s: Invalid trkpt latitude value %.6f", __FUNCTION__, dd );
			else
				c_ll.lat = dd;
			}
			break;

		case tt_trk_trkseg_trkpt_pos_lon: {
			gdouble dd = g_ascii_strtod ( c_cdata->str, NULL );
			if ( dd < -180.0 || dd > 180.0 )
				g_warning ( "%s: Invalid trkpt longitude value %.6f", __FUNCTION__, dd );
			else
				c_ll.lon = dd;
			}
			break;

		case tt_trk_trkseg_trkpt:
			if ( !isnan(c_ll.lat) && !isnan(c_ll.lon) ) {
				vik_coord_load_from_latlon ( &(c_tp->coord), vik_trw_layer_get_coord_mode(vtl), &c_ll );
				if ( f_tr_newseg ) {
					c_tp->newsegment = TRUE;
					f_tr_newseg = FALSE;
				}
				c_tr->trackpoints = g_list_prepend ( c_tr->trackpoints, c_tp );
			} else {
				g_warning ( "%s: Missing a coordinate value", __FUNCTION__ );
				vik_trackpoint_free ( c_tp );
			}
			c_tp = NULL;
			break;

		case tt_wpt_pos_lat: {
			gdouble dd = g_ascii_strtod ( c_cdata->str, NULL );
			if ( dd < -90.0 || dd > 90.0 )
				g_warning ( "%s: Invalid wpt latitude value %.6f", __FUNCTION__, dd );
			else
				c_ll.lat = dd;
			}
			break;

		case tt_wpt_pos_lon: {
			gdouble dd = g_ascii_strtod ( c_cdata->str, NULL );
			if ( dd < -180.0 || dd > 180.0 )
				g_warning ( "%s: Invalid wpt longitude value %.6f", __FUNCTION__, dd );
			else
				c_ll.lon = dd;
			}
			break;

		case tt_trk_trkseg_trkpt_cadence:
			c_tp->cadence = atoi ( c_cdata->str );
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_trk_trkseg_trkpt_hr:
			c_tp->heart_rate = atoi ( c_cdata->str );
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_trk_trkseg_trkpt_power:
			c_tp->power = g_ascii_strtod ( c_cdata->str, NULL );
			g_string_erase ( c_cdata, 0, -1 );
			break;

		case tt_trk_trkseg_trkpt_speed:
			c_tp->speed = g_ascii_strtod ( c_cdata->str, NULL );
			g_string_erase ( c_cdata, 0, -1 );
			break;

	        default: break;
	}

	current_tag = get_tag ( xpath->str );
}

static void tcx_cdata ( void *dta, const XML_Char *ss, int len )
{
	switch ( current_tag ) {
		case tt_tcx_name:
		case tt_tcx_creator:
		case tt_tcx_cmt:
		case tt_wpt_name:
		case tt_wpt_ele:
		case tt_trk_trkseg_trkpt_ele:
		case tt_wpt_cmt:
		case tt_trk_trkseg_trkpt_time:
		case tt_wpt_time:
		case tt_trk_trkseg_trkpt_pos_lat:
		case tt_trk_trkseg_trkpt_pos_lon:
		case tt_wpt_pos_lat:
		case tt_wpt_pos_lon:
		case tt_trk_trkseg_trkpt_cadence:
		case tt_trk_trkseg_trkpt_hr:
		case tt_trk_trkseg_trkpt_power:
		case tt_trk_trkseg_trkpt_speed:
			g_string_append_len ( c_cdata, ss, len );
			break;
		default: break; // ignore cdata from other things
	}
}


/**
 * Returns TRUE on a successful file read
 *   NB The file of course could contain no actual geo data that we can use!
 * NB2 Filename is used in case a name from within the file itself can not be found
 *   as file access is via the FILE* stream methods
 */
gboolean a_tcx_read_file ( VikAggregateLayer *val, VikViewport *vvp, FILE *ff, const gchar* filename )
{
	XML_Parser parser = XML_ParserCreate ( NULL );
	int done=0, len;
	enum XML_Status status = XML_STATUS_ERROR;

	UserDataT *ud = g_malloc (sizeof(UserDataT));
	ud->val      = val;
	ud->vvp      = vvp;
	ud->filename = filename;

	XML_SetElementHandler ( parser, (XML_StartElementHandler)tcx_start, (XML_EndElementHandler)tcx_end );
	XML_SetUserData ( parser, ud );
	XML_SetCharacterDataHandler ( parser, (XML_CharacterDataHandler)tcx_cdata );

	gchar buf[4096];

	xpath = g_string_new ( "" );
	c_cdata = g_string_new ( "" );

	unnamed_waypoints = 1;
	unnamed_tracks = 1;

	while ( !done ) {
		len = fread ( buf, 1, sizeof(buf)-7, ff );
		done = feof ( ff ) || !len;
		status = XML_Parse ( parser, buf, len, done );
	}

	gboolean ans = (status != XML_STATUS_ERROR);
	if ( !ans ) {
		g_warning ( "%s: XML error %s at line %ld", __FUNCTION__, XML_ErrorString(XML_GetErrorCode(parser)), XML_GetCurrentLineNumber(parser) );
	}

	XML_ParserFree (parser);
	g_free ( ud );
	g_string_free ( xpath, TRUE );
	g_string_free ( c_cdata, TRUE );

	return ans;
}
