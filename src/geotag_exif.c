/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
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

/*
 * This uses EXIF information from images to create waypoints at those positions
 * TODO: allow writing of image location:
 *  . Via correlation with a track (c.f. gpscorrelate) (multiple images)
 *  . Via screen position (individual image) on an existing waypoint
 *
 * For the implementation I have chosen to use libexif, which keeps Viking a pure C program
 * For an alternative implementation (a la gpscorrelate), one could use libeviv2 but it appears to be C++ only.
 */
#include <string.h>
#include "geotag_exif.h"
#include "globals.h"
#include "file.h"

#include <libexif/exif-data.h>

/**
 * Attempt to get a single comment from the various exif fields
 */
static gchar* geotag_get_exif_comment ( ExifData *ed )
{
	gchar str[128];
	ExifEntry *ee;
	//
	// Try various options to create a comment
	//
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_IMAGE_DESCRIPTION);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		return g_strdup ( str );
	}

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_COMMENT);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		return g_strdup ( str );
	}

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_SUBJECT);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		return g_strdup ( str );
	}

	// Consider using these for existing GPS info??
	//#define EXIF_TAG_GPS_TIME_STAMP        0x0007
	//#define EXIF_TAG_GPS_DATE_STAMP         0x001d
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_DATE_TIME_ORIGINAL);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		return g_strdup ( str );
	}
	
	// Otherwise nothing found
	return NULL;
}

/**
 * Handles 3 part location Rationals
 * Handles 1 part rational (must specify 0 for the offset)
 */
static gdouble Rational2Double ( unsigned char *data, int offset, ExifByteOrder order )
{
	// Explaination from GPS Correlate 'exif-gps.cpp' v 1.6.1
	// What we are trying to do here is convert the three rationals:
	//    dd/v mm/v ss/v
	// To a decimal
	//    dd.dddddd...
	// dd/v is easy: result = dd/v.
	// mm/v is harder:
	//    mm
	//    -- / 60 = result.
	//     v
	// ss/v is sorta easy.
	//     ss
	//     -- / 3600 = result
	//      v
	// Each part is added to the final number.
	gdouble ans;
	ExifRational er;
	er = exif_get_rational (data, order);
	ans = (gdouble)er.numerator / (gdouble)er.denominator;
	if (offset <= 0)
		return ans;

	er = exif_get_rational (data+(1*offset), order);
	ans = ans + ( ( (gdouble)er.numerator / (gdouble)er.denominator ) / 60.0 );
	er = exif_get_rational (data+(2*offset), order);
	ans = ans + ( ( (gdouble)er.numerator / (gdouble)er.denominator ) / 3600.0 );

	return ans;
}

/**
 * a_geotag_create_waypoint_from_file:
 * @filename: The image file to process
 * @vcmode:   The current location mode to use in the positioning of Waypoint
 * @name:     Returns a name for the Waypoint (can be NULL)
 *
 * Returns: An allocated Waypoint or NULL if Waypoint could not be generated (e.g. no EXIF info)
 *
 */
VikWaypoint* a_geotag_create_waypoint_from_file ( const gchar *filename, VikCoordMode vcmode, gchar **name )
{
	// Default return values (for failures)
	*name = NULL;
	VikWaypoint *wp = NULL;

	// TODO use log?
	//ExifLog *log = NULL;

	// open image with libexif
	ExifData *ed = exif_data_new_from_file ( filename );

	// Detect EXIF load failure
	if ( !ed )
		// return with no Waypoint
		return wp;

	struct LatLon ll;

	gchar str[128];
	ExifEntry *ee;

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_VERSION_ID);
	// Confirm this has a GPS Id - normally "2.0.0.0" or "2.2.0.0"
	if ( ! ( ee && ee->components == 4 ) )
		goto MyReturn;
	// Could test for these versions explicitly but may have byte order issues...
	//if ( ! ( ee->data[0] == 2 && ee->data[2] == 0 && ee->data[3] == 0 ) )
	//	goto MyReturn;


	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_MAP_DATUM);
	if ( ! ( ee && ee->components > 0 && ee->format == EXIF_FORMAT_ASCII ) )
		goto MyReturn;

	// If map datum specified - only deal in WGS-84 - the defacto standard
	if ( ee && ee->components > 0 ) {
		exif_entry_get_value ( ee, str, 128 );
		if ( strncmp (str, "WGS-84", 6) )
			goto MyReturn;
	}

	//
	// Lat & Long is necessary to form a waypoint.
	//
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LATITUDE);
	if ( ! ( ee && ee->components == 3 && ee->format == EXIF_FORMAT_RATIONAL ) )
		goto MyReturn;
  
	ll.lat = Rational2Double ( ee->data,
							   exif_format_get_size(ee->format),
							   exif_data_get_byte_order(ed) );

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LATITUDE_REF);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		if ( str[0] == 'S' )
			ll.lat = -ll.lat;
	}

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LONGITUDE);
	if ( ! ( ee && ee->components == 3 && ee->format == EXIF_FORMAT_RATIONAL ) )
		goto MyReturn;

	ll.lon = Rational2Double ( ee->data,
							   exif_format_get_size(ee->format),
							   exif_data_get_byte_order(ed) );

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LONGITUDE_REF);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		if ( str[0] == 'W' )
			ll.lon = -ll.lon;
	}

	//
	// Not worried if none of the other fields exist, as can default the values to something
	//

	gdouble alt = VIK_DEFAULT_ALTITUDE;
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_ALTITUDE);
	if ( ee && ee->components == 1 && ee->format == EXIF_FORMAT_RATIONAL ) {
		alt = Rational2Double ( ee->data,
								0,
								exif_data_get_byte_order(ed) );

		ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_ALTITUDE_REF);
		if ( ee && ee->components == 1 && ee->format == EXIF_FORMAT_BYTE && ee->data[0] == 1 )
			alt = -alt;
	}

	// Name
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_TITLE);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		*name = g_strdup ( str );
	}

	//
	// Now create Waypoint with acquired information
	//
	wp = vik_waypoint_new();
	wp->visible = TRUE;
	// Set info from exif values
	// Location
	vik_coord_load_from_latlon ( &(wp->coord), vcmode, &ll );
	// Altitude
	wp->altitude = alt;

	wp->comment = geotag_get_exif_comment ( ed );

	vik_waypoint_set_image ( wp, filename );

MyReturn:
	// Finished with EXIF
	exif_data_free ( ed );

	return wp;
}

/**
 * a_geotag_create_waypoint_positioned:
 * @filename: The image file to process
 * @coord:    The location for positioning the Waypoint
 * @name:     Returns a name for the Waypoint (can be NULL)
 *
 * Returns: An allocated Waypoint or NULL if Waypoint could not be generated
 *
 *  Here EXIF processing is used to get non position related information (i.e. just the comment)
 *
 */
VikWaypoint* a_geotag_create_waypoint_positioned ( const gchar *filename, VikCoord coord, gdouble alt, gchar **name )
{
	*name = NULL;
	VikWaypoint *wp = vik_waypoint_new();
	wp->visible = TRUE;
	wp->coord = coord;
	wp->altitude = alt;

	ExifData *ed = exif_data_new_from_file ( filename );

	// Set info from exif values
	if ( ed ) {
		wp->comment = geotag_get_exif_comment ( ed );

		gchar str[128];
		ExifEntry *ee;
		// Name
		ee = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_TITLE);
		if ( ee ) {
			exif_entry_get_value ( ee, str, 128 );
			*name = g_strdup ( str );
		}

		// Finished with EXIF
		exif_data_free ( ed );
	}

	vik_waypoint_set_image ( wp, filename );


	return wp;
}
