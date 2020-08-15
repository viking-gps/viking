/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
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

#include "kml.h"
#include "viking.h"
#include <expat.h>
#include "ctype.h"

typedef struct {
	GString *c_cdata;
	gboolean use_cdata;
	gchar *name;
	gchar *desc;
	gboolean vis;
	gdouble timestamp; // Waypoints only
	VikTrwLayer *vtl;
	VikWaypoint *waypoint;
	VikTrack *track;
	VikTrackpoint *trackpoint;
	GList *tracks;     // VikTracks
	GList *timestamps; // gdoubles
	GList *hrs;        // guints
	GList *cads;       // guints
	GList *temps;      // gdoubles
	GQueue *gq_start;
	GQueue *gq_end;
	XML_Parser parser;
} xml_data;

static guint unnamed_waypoints = 0;
static guint unnamed_tracks = 0;
static guint unnamed_routes = 0;

// Various helper functions

static void parse_tag_reset ( xml_data *xd )
{
	XML_SetElementHandler ( xd->parser, (XML_StartElementHandler)g_queue_pop_head(xd->gq_start), (XML_EndElementHandler)g_queue_pop_head(xd->gq_end) );
}

static void end_leaf_tag ( xml_data *xd )
{
	g_string_erase ( xd->c_cdata, 0, -1 );
	xd->use_cdata = FALSE;
	XML_SetEndElementHandler ( xd->parser, (XML_EndElementHandler)g_queue_pop_head(xd->gq_end) );
}

static void setup_to_read_next_level_tag ( xml_data *xd, gpointer old_start_func, gpointer old_end_func, gpointer new_start_func, gpointer new_end_func )
{
	if ( g_queue_peek_head(xd->gq_start) != old_start_func )
		g_queue_push_head ( xd->gq_start, old_start_func );
	if ( g_queue_peek_head(xd->gq_end) != old_end_func )
		g_queue_push_head ( xd->gq_end, old_end_func );
	XML_SetElementHandler ( xd->parser, (XML_StartElementHandler)new_start_func, (XML_EndElementHandler)new_end_func );
}

static const char *get_attr ( const char **attr, const char *key )
{
  while ( *attr ) {
    if ( g_strcmp0(*attr,key) == 0 )
      return *(attr + 1);
    attr += 2;
  }
  return NULL;
}

// Start of all the tag processing elements

static void name_end ( xml_data *xd, const char *el )
{
	xd->name = g_strdup ( xd->c_cdata->str );
	end_leaf_tag ( xd );
}

static void description_end ( xml_data *xd, const char *el )
{
	xd->desc = g_strdup ( xd->c_cdata->str );
	end_leaf_tag ( xd );
}

static void visibility_end ( xml_data *xd, const char *el )
{
	xd->vis = TRUE;
	if ( g_strcmp0(xd->c_cdata->str, "0") == 0 )
		xd->vis = FALSE;
	end_leaf_tag ( xd );
}

// A tag which should only contain cdata (i.e. no further tags)
static void setup_to_read_leaf_tag ( xml_data *xd, gpointer old_end_func, gpointer new_end_func )
{
	// Save old end function if different
	if ( g_queue_peek_head(xd->gq_end) != old_end_func )
		g_queue_push_head ( xd->gq_end, old_end_func );
	// Register new end function
	XML_SetEndElementHandler ( xd->parser, (XML_EndElementHandler)new_end_func );
	// Clear buffer and turn on
	g_string_erase ( xd->c_cdata, 0, -1 );
	xd->use_cdata = TRUE;
}

static void timestamp_when_end ( xml_data *xd, const char *el )
{
	GTimeVal gtv;
	if ( g_time_val_from_iso8601(xd->c_cdata->str, &gtv) ) {
		gdouble d1 = gtv.tv_sec;
		gdouble d2 = (gdouble)gtv.tv_usec/G_USEC_PER_SEC;
		xd->timestamp = (d1 < 0) ? d1 - d2 : d1 + d2;
	}
	end_leaf_tag ( xd );
}

static void timestamp_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "TimeStamp" ) == 0 ) {
		parse_tag_reset ( xd );
	}
}

static void timestamp_start ( xml_data *xd, const char *el, const char **attr )
{
	// Ignore 'extrude' and 'altitudeMode'
	if ( g_strcmp0 ( el, "when" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, timestamp_end, timestamp_when_end );
	}
}

static void set_vc_to_ll ( xml_data *xd, VikCoord *vc, VikTrwLayer *vtl, gdouble lat, gdouble lon )
{
	// Remember KML coordinates are the 'lon,lat(,alt)' order
	struct LatLon c_ll;
	if ( lat < -90.0 || lat > 90.0 ) {
		g_warning ( "%s: Invalid latitude value %f at line %ld", G_STRLOC, lat, XML_GetCurrentLineNumber(xd->parser) );
		c_ll.lat = 0.0;
	} else
		c_ll.lat = lat;

	if ( lon < -180.0 || lon > 180.0 ) {
		g_warning ( "%s: Invalid longitude value %f at line %ld", G_STRLOC, lon, XML_GetCurrentLineNumber(xd->parser) );
		c_ll.lon = 0.0;
	} else
		c_ll.lon = lon;

	vik_coord_load_from_latlon ( vc, vik_trw_layer_get_coord_mode(vtl), &c_ll );
}

static void point_coordinates_end ( xml_data *xd, const char *el )
{
	if ( xd->waypoint ) {
		gchar **vals = g_strsplit ( xd->c_cdata->str, ",", -1 );
		guint nn = g_strv_length ( vals );
		if ( nn < 2 || nn > 3  )
			g_warning ( "%s: expected 2 or 3 coordinate parts but got %d at line %ld", G_STRLOC, nn, XML_GetCurrentLineNumber(xd->parser) );
		else {
			// Remember KML coordinates are the 'lon,lat(,alt)' order
			gdouble lat = g_ascii_strtod ( vals[1], NULL );
			gdouble lon = g_ascii_strtod ( vals[0], NULL );
			set_vc_to_ll ( xd, &(xd->waypoint->coord), xd->vtl, lat, lon );
			if ( nn == 3 )
				// ATM altitude is always interpreted to be in absolute mode (to sea level)
				xd->waypoint->altitude = g_ascii_strtod ( vals[2], NULL );
		}
		g_strfreev ( vals );
	}
	else
		g_warning ( "%s: no waypoint", G_STRLOC );

	end_leaf_tag ( xd );
}

static void point_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "Point" ) == 0 ) {
		if ( xd->waypoint ) {
			if ( xd->name && strlen(xd->name) > 0 ) {
				vik_waypoint_set_name ( xd->waypoint, xd->name );
			} else {
				xd->waypoint->hide_name = TRUE;
				gchar *name = g_strdup_printf ( "WP%04d", unnamed_waypoints++ );
				vik_waypoint_set_name ( xd->waypoint, name );
				g_free ( name );
			}
			if ( xd->desc ) {
				vik_waypoint_set_description ( xd->waypoint, xd->desc );
			}
			xd->waypoint->visible = xd->vis;
			xd->waypoint->timestamp = xd->timestamp;
			vik_trw_layer_filein_add_waypoint ( xd->vtl, NULL, xd->waypoint );
		}
		parse_tag_reset ( xd );
	}
}

static void point_start ( xml_data *xd, const char *el, const char **attr )
{
	// Ignore 'extrude' and 'altitudeMode'
	if ( g_strcmp0 ( el, "coordinates" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, point_end, point_coordinates_end );
	}
	xd->waypoint = vik_waypoint_new();
}

static void linestring_coordinates_end ( xml_data *xd, const char *el )
{
	if ( xd->track ) {
		gchar *ptr = xd->c_cdata->str;
		if ( ptr ) {
			int len = strlen(ptr);
			gchar *endptr = ptr + len;
			gchar *cp;
			for ( cp = ptr; cp < endptr; cp++ )
				if ( !isspace(*cp) )
					break;

			gboolean newseg = TRUE;
			int val = 0;
			gdouble values[3];
			gchar *vp;
			for ( vp = cp; cp <= endptr; cp++ ) {
				if ( *cp == ',' ) {
					// Get string before this comma
					gchar *str = g_malloc0 ( cp-vp+1 );
					strncpy ( str, vp, cp-vp );
					values[val] = g_ascii_strtod ( str, NULL );
					g_free ( str );
					val++;
					vp = cp + 1; // +1 for the next one after the comma
				} else if ( cp == NULL || isspace(*cp) ) {
					if ( val < 1 || val > 2 )
						// Not enough or too many coordinate parts
						goto end;
					// Otherwise the value is to end of text block
					//  (should be the last coordinate part)
					gchar *str = g_malloc0 ( cp-vp+1 );
					strncpy ( str, vp, cp-vp );
					values[val] = g_ascii_strtod ( str, NULL );
					g_free ( str );

					VikTrackpoint *tp = vik_trackpoint_new();
					// Remember KML coordinates are the 'lon,lat(,alt)' order
					set_vc_to_ll ( xd, &(tp->coord), xd->vtl, values[1], values[0] );
					if ( val == 2 )
						// ATM altitude is always interpreted to be in absolute mode (to sea level)
						tp->altitude = values[2];
					if ( newseg ) {
						tp->newsegment = TRUE;
						newseg = FALSE;
					}

					xd->track->trackpoints = g_list_prepend ( xd->track->trackpoints, tp );

					// Consume any extra space to get to the next coordinate part
					while ( cp != NULL && isspace(*cp) )
						cp++;
					val = 0;
					vp = cp;
				}
			}
		}
	}
	else
		g_warning ( "%s: no track", G_STRLOC );

end:
	end_leaf_tag ( xd );
}

static void linestring_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "LineString" ) == 0 ) {
		if ( xd->track ) {
			if ( xd->name && strlen(xd->name) > 0 ) {
				vik_track_set_name ( xd->track, xd->name );
			} else {
				gchar *name = g_strdup_printf ( "TRK%04d", unnamed_tracks++ );
				vik_track_set_name ( xd->track, name );
				g_free ( name );
			}
			if ( xd->desc ) {
				vik_track_set_description ( xd->track, xd->desc );
			}
			xd->track->trackpoints = g_list_reverse ( xd->track->trackpoints );
			xd->track->visible = xd->vis;
			vik_trw_layer_filein_add_track ( xd->vtl, NULL, xd->track );
		}
		parse_tag_reset ( xd );
	}
}

static void linestring_start ( xml_data *xd, const char *el, const char **attr )
{
	// ATM ignoring at least 'extrude', 'tessellate' & 'altitudeMode'
	if ( g_strcmp0 ( el, "coordinates" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, linestring_end, linestring_coordinates_end );
	}
}

// hardly any different to linestring handling
static void linearring_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "LinearRing" ) == 0 ) {
		if ( xd->track ) {
			if ( xd->name && strlen(xd->name) > 0 ) {
				vik_track_set_name ( xd->track, xd->name );
			} else {
				gchar *name = g_strdup_printf ( "TRK%04d", unnamed_tracks++ );
				vik_track_set_name ( xd->track, name );
				g_free ( name );
			}
			if ( xd->desc ) {
				vik_track_set_description ( xd->track, xd->desc );
			}
			xd->track->trackpoints = g_list_reverse ( xd->track->trackpoints );
			vik_trw_layer_filein_add_track ( xd->vtl, NULL, xd->track );
		}
		parse_tag_reset ( xd );
	}
}

static void linearring_start ( xml_data *xd, const char *el, const char **attr )
{
	// ATM ignoring at least 'extrude', 'tessellate' & 'altitudeMode'
	if ( g_strcmp0 ( el, "coordinates" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, linearring_end, linestring_coordinates_end );
	}
}

static void placemark_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "Placemark" ) == 0 ) {
		// Reset
		xd->vis = TRUE;
		g_free ( xd->name );
		xd->name = NULL;
		g_free ( xd->desc );
		xd->desc = NULL;
		xd->timestamp = NAN;

		parse_tag_reset ( xd );
	}
}

// For some unknown reason Track coordinates use a ' ' seperator,
//  whereas linestrings (and points) use a ','
static void track_coordinates_end ( xml_data *xd, const char *el )
{
	if ( xd->trackpoint && xd->track ) {
		gchar **vals = g_strsplit ( xd->c_cdata->str, " ", -1 );
		guint nn = g_strv_length ( vals );
		if ( nn < 2 || nn > 3  )
			g_warning ( "%s: expected 2 or 3 coordinate parts but got %d at line %ld", G_STRLOC, nn, XML_GetCurrentLineNumber(xd->parser) );
		else {
			// Remember KML coordinates are the 'lon,lat(,alt)' order
			gdouble lat = g_ascii_strtod ( vals[1], NULL );
			gdouble lon = g_ascii_strtod ( vals[0], NULL );
			set_vc_to_ll ( xd, &(xd->trackpoint->coord), xd->vtl, lat, lon );
			if ( nn == 3 )
				// ATM altitude is always interpreted to be in absolute mode (to sea level)
				xd->trackpoint->altitude = g_ascii_strtod ( vals[2], NULL );

			xd->track->trackpoints = g_list_prepend ( xd->track->trackpoints, xd->trackpoint );
			xd->track->visible = xd->vis;
			xd->vis = TRUE;
		}
		g_strfreev ( vals );
	}
	else
		g_warning ( "%s: no trackpoint", G_STRLOC );

	end_leaf_tag ( xd );
}

static void track_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "gx:Track" ) == 0 ) {
		if ( xd->track ) {
			if ( xd->name && strlen(xd->name) > 0 ) {
				vik_track_set_name ( xd->track, xd->name );
				g_free ( xd->name );
				xd->name = NULL;
			} else {
				gchar *name = g_strdup_printf ( "TRK%04d", unnamed_tracks++ );
				vik_track_set_name ( xd->track, name );
				g_free ( name );
			}
			if ( xd->desc ) {
				vik_track_set_description ( xd->track, xd->desc );
				g_free ( xd->desc );
				xd->desc = NULL;
			}
			xd->track->trackpoints = g_list_reverse ( xd->track->trackpoints );
			xd->timestamps = g_list_reverse ( xd->timestamps );
			xd->hrs = g_list_reverse ( xd->hrs );
			xd->cads = g_list_reverse ( xd->cads );
			xd->temps = g_list_reverse ( xd->temps );

			gulong num_points = g_list_length ( xd->track->trackpoints );

			// Assign times
			gulong ntimes = g_list_length ( xd->timestamps );
			if ( ntimes ) {
				if ( ntimes == num_points ) {
					GList *lts = xd->timestamps;
					for (GList *ltp = xd->track->trackpoints; ltp != NULL; ltp = ltp->next ) {
						gdouble *dd = (gdouble*)lts->data;
						VIK_TRACKPOINT(ltp->data)->timestamp = *dd;
						lts = lts->next;
					}
				} else
					g_warning ( "%s: trackpoint count vs timestamp count differ %ld vs %ld at line %ld",
					            G_STRLOC, num_points, ntimes, XML_GetCurrentLineNumber(xd->parser) );
			}
			g_list_free_full ( xd->timestamps, g_free );
			xd->timestamps = NULL;

			// Assign heart rate
			gulong nhrs = g_list_length ( xd->hrs );
			if ( nhrs ) {
				if ( nhrs == num_points ) {
					GList *lts = xd->hrs;
					for (GList *ltp = xd->track->trackpoints; ltp != NULL; ltp = ltp->next ) {
						VIK_TRACKPOINT(ltp->data)->heart_rate = GPOINTER_TO_UINT(lts->data);
						lts = lts->next;
					}
				} else
					g_warning ( "%s: trackpoint count vs heart rate count differ %ld vs %ld at line %ld",
					            G_STRLOC, num_points, nhrs, XML_GetCurrentLineNumber(xd->parser) );
			}
			g_list_free ( xd->hrs ); // NB no data in list has been allocated

			// Assign cadence
			gulong ncads = g_list_length ( xd->cads );
			if ( ncads ) {
				if ( ncads == num_points ) {
					GList *lts = xd->cads;
					for (GList *ltp = xd->track->trackpoints; ltp != NULL; ltp = ltp->next ) {
						VIK_TRACKPOINT(ltp->data)->cadence = GPOINTER_TO_UINT(lts->data);
						lts = lts->next;
					}
				} else
					g_warning ( "%s: trackpoint count vs cadence count differ %ld vs %ld at line %ld",
					            G_STRLOC, num_points, ncads, XML_GetCurrentLineNumber(xd->parser) );
			}
			g_list_free ( xd->cads ); // NB no data in list has been allocated

			// Assign temps
			gulong ntemps = g_list_length ( xd->temps );
			if ( ntemps ) {
				if ( ntemps == num_points ) {
					GList *lts = xd->temps;
					for (GList *ltp = xd->track->trackpoints; ltp != NULL; ltp = ltp->next ) {
						gdouble *dd = (gdouble*)lts->data;
						VIK_TRACKPOINT(ltp->data)->temp = *dd;
						lts = lts->next;
					}
				} else
					g_warning ( "%s: trackpoint count vs temp count differ %ld vs %ld at line %ld",
					            G_STRLOC, num_points, ntemps, XML_GetCurrentLineNumber(xd->parser) );
			}
			g_list_free_full ( xd->temps, g_free );
			xd->temps = NULL;

			// Set first (and only) segment
			VikTrackpoint *tpt = vik_track_get_tp_first ( xd->track );
			if ( tpt )
				tpt->newsegment = TRUE;

			// Add it or wait if reading multi tracks
			if ( !xd->tracks )
				vik_trw_layer_filein_add_track ( xd->vtl, NULL, xd->track );
		}
		parse_tag_reset ( xd );
	}
}

// Tricky to reuse timestamp_when_end()
// Since for tracks the <when></when> should be repeated for each trackpoint
//  so need to add to a list rather then a singular instance.
static void track_when_end ( xml_data *xd, const char *el )
{
	gdouble *tt = g_malloc0 ( sizeof(gdouble) );
	GTimeVal gtv;
	if ( g_time_val_from_iso8601(xd->c_cdata->str, &gtv) ) {
		gdouble d1 = gtv.tv_sec;
		gdouble d2 = (gdouble)gtv.tv_usec/G_USEC_PER_SEC;
		*tt = (d1 < 0) ? d1 - d2 : d1 + d2;
	} else {
		*tt = NAN;
	}
	xd->timestamps = g_list_prepend ( xd->timestamps, tt );
	end_leaf_tag ( xd );
}

static void value_cad_end ( xml_data *xd, const char *el )
{
	gdouble val = g_ascii_strtod ( xd->c_cdata->str, NULL );
	guint ival;
	if ( isnan(val) )
		ival = VIK_TRKPT_CADENCE_NONE;
	else
		ival = round ( val );	
	xd->cads = g_list_prepend ( xd->cads, GUINT_TO_POINTER(ival) );
	end_leaf_tag ( xd );
}

static void value_hr_end ( xml_data *xd, const char *el )
{
	gdouble val = g_ascii_strtod ( xd->c_cdata->str, NULL );
	guint ival;
	if ( isnan(val) )
		ival = 0;
	else
		ival = round ( val );	
	xd->hrs = g_list_prepend ( xd->hrs, GUINT_TO_POINTER(ival) );
	end_leaf_tag ( xd );
}

static void value_temp_end ( xml_data *xd, const char *el )
{
	gdouble *val = g_malloc0 ( sizeof(gdouble) );
	*val = g_ascii_strtod ( xd->c_cdata->str, NULL );
	xd->temps = g_list_prepend ( xd->temps, val );
	end_leaf_tag ( xd );
}

static void simplearraydata_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "gx:SimpleArrayData" ) == 0 )
		parse_tag_reset ( xd );
}

static void simplearraydata_cad_start ( xml_data *xd, const char *el, const char **attr )
{
	setup_to_read_leaf_tag ( xd, simplearraydata_end, value_cad_end );
}

static void simplearraydata_hr_start ( xml_data *xd, const char *el, const char **attr )
{
	setup_to_read_leaf_tag ( xd, simplearraydata_end, value_hr_end );
}

static void simplearraydata_temp_start ( xml_data *xd, const char *el, const char **attr )
{
	setup_to_read_leaf_tag ( xd, simplearraydata_end, value_temp_end );
}

static void schemadata_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "SchemaData" ) == 0 )
		parse_tag_reset ( xd );
}

static void schemadata_start ( xml_data *xd, const char *el, const char **attr )
{
	// Looking for cadence or heartrate or temperature
	if ( g_strcmp0 ( el, "gx:SimpleArrayData" ) == 0 ) {
		const gchar *name = get_attr ( attr, "name" );
		if ( g_strcmp0 ( name, "cadence" ) == 0 )
			setup_to_read_next_level_tag ( xd, schemadata_start, schemadata_end, simplearraydata_cad_start, simplearraydata_end );
		else if ( g_strcmp0 ( name, "heartrate" ) == 0 )
			setup_to_read_next_level_tag ( xd, schemadata_start, schemadata_end, simplearraydata_hr_start, simplearraydata_end );
		else if ( g_strcmp0 ( name, "temperature" ) == 0 )
			setup_to_read_next_level_tag ( xd, schemadata_start, schemadata_end, simplearraydata_temp_start, simplearraydata_end );
	}
}

static void extendeddata_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "ExtendedData" ) == 0 )
		parse_tag_reset ( xd );
}

static void extendeddata_start ( xml_data *xd, const char *el, const char **attr )
{
	if ( g_strcmp0 ( el, "SchemaData" ) == 0 )
		setup_to_read_next_level_tag ( xd, extendeddata_start, extendeddata_end, schemadata_start, schemadata_end );
}

static void track_start ( xml_data *xd, const char *el, const char **attr )
{
	// Ignore ''altitudeMode', 'gx:angles', 'Model'
	// Read values from when + ExtendedData into separate lists
	//  merging into the main trackpoints list once the track end is reached
	if ( g_strcmp0 ( el, "gx:coord" ) == 0 ) {
		xd->trackpoint = vik_trackpoint_new();
		setup_to_read_leaf_tag ( xd, track_end, track_coordinates_end );
	} else if ( g_strcmp0 ( el, "when" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, track_end, track_when_end );
	} else if ( g_strcmp0 ( el, "ExtendedData" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, track_start, track_end, extendeddata_start, extendeddata_end );
	}
}

static void multitrack_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "gx:MultiTrack" ) == 0 ) {
		// Join tracks together (but keep segments)
		parse_tag_reset ( xd );
		if ( xd->tracks ) {
			// Copy first track - so its not freed when the list of tracks are
			xd->track = vik_track_copy ( VIK_TRACK(xd->tracks->data), TRUE );
			guint count = 1;
			for ( GList *gl = xd->tracks; gl != NULL; gl = gl->next ) {
				// Don't append the first track to itself
				if ( count > 1 ) {
					vik_track_steal_and_append_trackpoints ( xd->track, VIK_TRACK(gl->data) );
				}
				count++;
			}
			vik_trw_layer_filein_add_track ( xd->vtl, NULL, xd->track );
			xd->track = NULL;
			g_list_free_full ( xd->tracks, (GDestroyNotify)vik_track_free );
		}
	}
}

static void multitrack_start ( xml_data *xd, const char *el, const char **attr )
{
	// Ignore ''altitudeMode', 'gx:interpolate'
	if ( g_strcmp0 ( el, "gx:Track" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, multitrack_start, multitrack_end, track_start, track_end );		
		xd->track = vik_track_new();
		xd->tracks = g_list_append ( xd->tracks, xd->track );
	}
}

static void placemark_start ( xml_data *xd, const char *el, const char **attr )
{
	// NB ignores <MultiGeometry> levels and reads anything found in it anyway
	if ( g_strcmp0 ( el, "name" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, placemark_end, name_end );
	} else if ( g_strcmp0 ( el, "visibility" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, placemark_end, visibility_end );
	} else if ( g_strcmp0 ( el, "description" ) == 0 ) {
		setup_to_read_leaf_tag ( xd, placemark_end, description_end );
	} else if ( g_strcmp0 ( el, "TimeStamp" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, placemark_start, placemark_end, timestamp_start, timestamp_end );
	} else if ( g_strcmp0 ( el, "Point" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, placemark_start, placemark_end, point_start, point_end );
	} else if ( g_strcmp0 ( el, "LineString" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, placemark_start, placemark_end, linestring_start, linestring_end );
		xd->track = vik_track_new();
		xd->track->is_route = TRUE;
	} else if ( g_strcmp0 ( el, "LinearRing" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, placemark_start, placemark_end, linearring_start, linearring_end );
		xd->track = vik_track_new();
		xd->track->is_route = TRUE;
	} else if ( g_strcmp0 ( el, "gx:Track" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, placemark_start, placemark_end, track_start, track_end );
		xd->track = vik_track_new();
	} else if ( g_strcmp0 ( el, "gx:MultiTrack" ) == 0 ) {
		setup_to_read_next_level_tag ( xd, placemark_start, placemark_end, multitrack_start, multitrack_end );
	} 
}

static void top_end ( xml_data *xd, const char *el )
{
	if ( g_strcmp0 ( el, "kml" ) == 0 )
		XML_SetEndElementHandler ( xd->parser, (XML_EndElementHandler)g_queue_pop_head(xd->gq_end) );
}

static void top_start ( xml_data *xd, const char *el, const char **attr )
{
	// NB also finds Placemarks whereever they may be in Document or Folder levels as well
	if ( g_strcmp0 ( el, "Placemark" ) == 0 )
		setup_to_read_next_level_tag ( xd, top_start, top_end, placemark_start, placemark_end );
}

static void kml_start ( xml_data *xd, const char *el, const char **attr )
{
	if ( g_strcmp0 ( el, "kml" ) )
		XML_StopParser ( xd->parser, XML_FALSE );

	XML_SetStartElementHandler ( xd->parser, (XML_StartElementHandler)top_start );
}

static void kml_end ( xml_data *xd, const char *el )
{
	g_debug ( G_STRLOC );
}

static void kml_cdata ( xml_data *xd, const XML_Char *ss, int len )
{
	if ( xd->use_cdata ) {
		g_string_append_len ( xd->c_cdata, ss, len );
	}
}

/**
 * a_kml_read_file:
 * @FILE: The KML file to open
 * @VikTrwLayer: The Layer to put the geo data in
 *
 * Returns:
 *  -1 if KMZ not supported (this shouldn't happen)
 *  0 on success
 *  >0 <128 ZIP error code
 *  128 - No doc.kml file in KMZ
 *  129 - Couldn't understand the doc.kml file
 */
gboolean a_kml_read_file ( VikTrwLayer *vtl, FILE *ff )
{
	gchar buffer[4096];
	XML_Parser parser = XML_ParserCreate(NULL);
	enum XML_Status status = XML_STATUS_ERROR;

	unnamed_waypoints = 1;
	unnamed_tracks = 1;
	unnamed_routes = 1;

	xml_data *xd = g_malloc0 ( sizeof (xml_data) );
	// Set default values;
	xd->c_cdata = g_string_new ( "" );
	xd->vis = TRUE;
	xd->timestamp = NAN;
	xd->vtl = vtl;
	xd->gq_start = g_queue_new();
	xd->gq_end = g_queue_new();
	xd->parser = parser;

	// Always force V1.1, since we may read in 'extended' data like cadence, etc...
	vik_trw_layer_set_gpx_version ( vtl, GPX_V1_1 );

	// The premise of handling tags is thar for each level down the xml tree,
	//  we use an appropriate handler for that tag
	//   (which knows how to process the data being read in at that point)
	// And then when the end of the tag is reached, restore the previously active tag handlers
	g_queue_push_head ( xd->gq_start, kml_start );
	g_queue_push_head ( xd->gq_end, kml_end );
	XML_SetElementHandler ( parser, (XML_StartElementHandler)kml_start, (XML_EndElementHandler)kml_end );
	XML_SetUserData ( parser, xd );
	XML_SetCharacterDataHandler ( parser, (XML_CharacterDataHandler)kml_cdata);

	int done=0, len;
	while ( !done ) {
		len = fread ( buffer, 1, sizeof(buffer)-7, ff );
		done = feof ( ff ) || !len;
		status = XML_Parse ( parser, buffer, len, done );
	}

	gboolean ans = (status != XML_STATUS_ERROR);
	if ( !ans ) {
		g_warning ( "%s: XML error %s at line %ld", G_STRLOC, XML_ErrorString(XML_GetErrorCode(parser)), XML_GetCurrentLineNumber(parser) );
	}

	XML_ParserFree ( parser );

	g_queue_free ( xd->gq_start );
	g_queue_free ( xd->gq_end );
	g_string_free ( xd->c_cdata, TRUE );
	g_free ( xd );
	return ans;
}
