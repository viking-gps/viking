/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011-2014, Rob Norris <rw_norris@hotmail.com>
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
 *
 * The intial implementation uses libexif, which keeps Viking a pure C program.
 * Now libgexiv2 is available (in C as a wrapper around the more powerful libexiv2 [C++]) so this is the preferred build.
 *  The attentative reader will notice the use of gexiv2 is a lot simpler as well.
 * For the time being the libexif code + build is still made available.
 */
#include <string.h>
#include "geotag_exif.h"
#include "config.h"
#include "globals.h"
#include "file.h"

#include <sys/stat.h>
#include <utime.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#ifdef HAVE_LIBGEXIV2
#include <gexiv2/gexiv2.h>
#endif
#ifdef HAVE_LIBEXIF
#include <libexif/exif-data.h>
#include "libjpeg/jpeg-data.h"
#endif

#ifdef HAVE_LIBGEXIV2
/**
 * Attempt to get a single comment from the various exif fields
 */
static gchar* geotag_get_exif_comment ( GExiv2Metadata *gemd )
{
	//
	// Try various options to create a comment
	//
	if ( gexiv2_metadata_has_tag ( gemd, "Exif.Image.ImageDescription" ) )
		return g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Image.ImageDescription" ) );

	if ( gexiv2_metadata_has_tag ( gemd, "Exif.Image.XPComment" ) )
		return g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Image.XPComment" ) );

	if ( gexiv2_metadata_has_tag ( gemd, "Exif.Image.XPSubject" ) )
		return g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Image.XPSubject" ) );

	if ( gexiv2_metadata_has_tag ( gemd, "Exif.Image.DateTimeOriginal" ) )
		return g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Image.DateTimeOriginal" ) );

	// Otherwise nothing found
	return NULL;
}
#endif

#ifdef HAVE_LIBEXIF
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

static struct LatLon get_latlon ( ExifData *ed )
{
	struct LatLon ll = { 0.0, 0.0 };
	const struct LatLon ll0 = { 0.0, 0.0 };

	gchar str[128];
	ExifEntry *ee;
	//
	// Lat & Long is necessary to form a waypoint.
	//
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LATITUDE);
	if ( ! ( ee && ee->components == 3 && ee->format == EXIF_FORMAT_RATIONAL ) )
		return ll0;

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
		return ll0;

	ll.lon = Rational2Double ( ee->data,
							   exif_format_get_size(ee->format),
							   exif_data_get_byte_order(ed) );

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LONGITUDE_REF);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		if ( str[0] == 'W' )
			ll.lon = -ll.lon;
	}

	return ll;
}
#endif

/**
 * a_geotag_get_position:
 *
 * @filename: The (JPG) file with EXIF information in it
 *
 * Returns: The position in LatLon format.
 *  It will be 0,0 if some kind of failure occurs.
 */
struct LatLon a_geotag_get_position ( const gchar *filename )
{
	struct LatLon ll = { 0.0, 0.0 };

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new ();
	if ( gexiv2_metadata_open_path ( gemd, filename, NULL ) ) {
		gdouble lat;
		gdouble lon;
		gdouble alt;
		if ( gexiv2_metadata_get_gps_info ( gemd, &lon, &lat, &alt ) ) {
			ll.lat = lat;
			ll.lon = lon;
		}
	}
	gexiv2_metadata_free  ( gemd );
#else
#ifdef HAVE_LIBEXIF
	// open image with libexif
	ExifData *ed = exif_data_new_from_file ( filename );

	// Detect EXIF load failure
	if ( !ed )
		return ll;

	ExifEntry *ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_VERSION_ID);
	// Confirm this has a GPS Id - normally "2.0.0.0" or "2.2.0.0"
	if ( ! ( ee && ee->components == 4 ) )
		goto MyReturn0;

	ll = get_latlon ( ed );

MyReturn0:
	// Finished with EXIF
	exif_data_free ( ed );
#endif
#endif

	return ll;
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

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new ();
	if ( gexiv2_metadata_open_path ( gemd, filename, NULL ) ) {
		gdouble lat;
		gdouble lon;
		gdouble alt;
		if ( gexiv2_metadata_get_gps_info ( gemd, &lon, &lat, &alt ) ) {
			struct LatLon ll;
			ll.lat = lat;
			ll.lon = lon;

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

			if ( gexiv2_metadata_has_tag ( gemd, "Exif.Image.XPTitle" ) )
				*name = g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Image.XPTitle" ) );
			wp->comment = geotag_get_exif_comment ( gemd );

			vik_waypoint_set_image ( wp, filename );
		}
	}
	gexiv2_metadata_free ( gemd );
#else
#ifdef HAVE_LIBEXIF
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

	ll = get_latlon ( ed );

	// Hopefully won't have valid images at 0,0!
	if ( ll.lat == 0.0 && ll.lon == 0.0 )
		goto MyReturn;

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
#endif
#endif

	return wp;
}

/**
 * a_geotag_waypoint_positioned:
 * @filename: The image file to process
 * @coord:    The location for positioning the Waypoint
 * @name:     Returns a name for the Waypoint (can be NULL)
 * @waypoint: An existing waypoint to update (can be NULL to generate a new waypoint)
 *
 * Returns: An allocated waypoint if the input waypoint is NULL,
 *  otherwise the passed in waypoint is updated
 *
 *  Here EXIF processing is used to get non position related information (i.e. just the comment)
 *
 */
VikWaypoint* a_geotag_waypoint_positioned ( const gchar *filename, VikCoord coord, gdouble alt, gchar **name, VikWaypoint *wp )
{
	*name = NULL;
	if ( wp == NULL ) {
		// Need to create waypoint
		wp = vik_waypoint_new();
		wp->visible = TRUE;
	}
	wp->coord = coord;
	wp->altitude = alt;

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new ();
	if ( gexiv2_metadata_open_path ( gemd, filename, NULL ) ) {
			wp->comment = geotag_get_exif_comment ( gemd );
			if ( gexiv2_metadata_has_tag ( gemd, "Exif.Image.XPTitle" ) )
				*name = g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Image.XPTitle" ) );
	}
	gexiv2_metadata_free ( gemd );
#else
#ifdef HAVE_LIBEXIF
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
#endif
#endif

	vik_waypoint_set_image ( wp, filename );

	return wp;
}

/**
 * a_geotag_get_exif_date_from_file:
 * @filename: The image file to process
 * @has_GPS_info: Returns whether the file has existing GPS information
 *
 * Returns: An allocated string with the date and time in EXIF_DATE_FORMAT, otherwise NULL if some kind of failure
 *
 *  Here EXIF processing is used to get time information
 *
 */
gchar* a_geotag_get_exif_date_from_file ( const gchar *filename, gboolean *has_GPS_info )
{
	gchar* datetime = NULL;
	*has_GPS_info = FALSE;

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new ();
	if ( gexiv2_metadata_open_path ( gemd, filename, NULL ) ) {
		gdouble lat, lon;
		*has_GPS_info = ( gexiv2_metadata_get_gps_longitude(gemd,&lon) && gexiv2_metadata_get_gps_latitude(gemd,&lat) );

		// Prefer 'Photo' version over 'Image'
		if ( gexiv2_metadata_has_tag ( gemd, "Exif.Photo.DateTimeOriginal" ) )
			datetime = g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Photo.DateTimeOriginal" ) );
		else
			datetime = g_strdup ( gexiv2_metadata_get_tag_interpreted_string ( gemd, "Exif.Image.DateTimeOriginal" ) );
	}
	gexiv2_metadata_free ( gemd );
#else
#ifdef HAVE_LIBEXIF
	ExifData *ed = exif_data_new_from_file ( filename );

	// Detect EXIF load failure
	if ( !ed )
		return datetime;

	gchar str[128];
	ExifEntry *ee;

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_DATE_TIME_ORIGINAL);
	if ( ee ) {
		exif_entry_get_value ( ee, str, 128 );
		datetime = g_strdup ( str );
	}

	// Check GPS Info

	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_VERSION_ID);
	// Confirm this has a GPS Id - normally "2.0.0.0" or "2.2.0.0"
	if ( ee && ee->components == 4 )
		*has_GPS_info = TRUE;

	// Check other basic GPS fields exist too
	// I have encountered some images which have just the EXIF_TAG_GPS_VERSION_ID but nothing else
	// So to confirm check more EXIF GPS TAGS:
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LATITUDE);
	if ( !ee )
		*has_GPS_info = FALSE;
	ee = exif_content_get_entry (ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LONGITUDE);
	if ( !ee )
		*has_GPS_info = FALSE;

	exif_data_free ( ed );
#endif
#endif
	return datetime;
}


#ifdef HAVE_LIBEXIF
/**! If the entry doesn't exist, create it.
 * Based on exif command line action_create_value function in exif 0.6.20
 */
static ExifEntry* my_exif_create_value (ExifData *ed, ExifTag tag, ExifIfd ifd)
{
	ExifEntry *e = exif_content_get_entry (ed->ifd[ifd], tag);
	if ( !e ) {
	    e = exif_entry_new ();
	    exif_content_add_entry (ed->ifd[ifd], e);

		exif_entry_initialize (e, tag);

		// exif_entry_initialize doesn't seem to do much, especially for the GPS tags
		//   so have to setup fields ourselves:
		e->tag = tag;

		if ( tag == EXIF_TAG_GPS_VERSION_ID ) {
			e->format = EXIF_FORMAT_BYTE;
			e->components = 4;
			e->size = sizeof (char) * e->components;
			if ( e->data )
				g_free (e->data);
			e->data = g_malloc (e->size);
		}
		if ( tag == EXIF_TAG_GPS_MAP_DATUM ||
			 tag == EXIF_TAG_GPS_LATITUDE_REF || tag == EXIF_TAG_GPS_LONGITUDE_REF ||
			 tag == EXIF_TAG_GPS_PROCESSING_METHOD ) {
			e->format = EXIF_FORMAT_ASCII;
			// NB Allocation is handled later on when the actual string used is known
		}
		if ( tag == EXIF_TAG_GPS_LATITUDE || tag == EXIF_TAG_GPS_LONGITUDE ) {
			e->format = EXIF_FORMAT_RATIONAL;
			e->components = 3;
			e->size = sizeof (ExifRational) * e->components;
			if ( e->data )
				g_free (e->data);
			e->data = g_malloc (e->size);
		}
		if ( tag == EXIF_TAG_GPS_ALTITUDE ) {
			e->format = EXIF_FORMAT_RATIONAL;
			e->components = 1;
			e->size = sizeof (ExifRational) * e->components;
			if ( e->data )
				g_free (e->data);
			e->data = g_malloc (e->size);
		}
		if ( tag == EXIF_TAG_GPS_ALTITUDE_REF ) {
			e->components = 1;
			e->size = sizeof (char) * e->components;
			if ( e->data )
				g_free (e->data);
			e->data = g_malloc (e->size);
		}
	    /* The entry has been added to the IFD, so we can unref it */
	    //exif_entry_unref(e);
		// Crashes later on, when saving to jpeg if the above unref is enabled!!
		// ?Some other malloc problem somewhere?
	}
	return e;
}

/** Heavily based on convert_arg_to_entry from exif command line tool.
 *  But without ExifLog, exitting, use of g_* io functions
 *   and can take a gdouble value instead of a string
 */
static void convert_to_entry (const char *set_value, gdouble gdvalue, ExifEntry *e, ExifByteOrder o)
{
	unsigned int i, numcomponents;
	char *value_p = NULL;
	char *buf = NULL;
	/*
	 * ASCII strings are handled separately,
	 * since they don't require any conversion.
	 */
	if (e->format == EXIF_FORMAT_ASCII ||
	    e->tag == EXIF_TAG_USER_COMMENT) {
		if (e->data) g_free (e->data);
		e->components = strlen (set_value) + 1;
		if (e->tag == EXIF_TAG_USER_COMMENT)
			e->components += 8 - 1;
		e->size = sizeof (char) * e->components;
		e->data = g_malloc (e->size);
		if (!e->data) {
			g_warning (_("Not enough memory."));
			return;
		}
		if (e->tag == EXIF_TAG_USER_COMMENT) {
			/* assume ASCII charset */
			/* TODO: get this from the current locale */
			memcpy ((char *) e->data, "ASCII\0\0\0", 8);
			memcpy ((char *) e->data + 8, set_value,
					strlen (set_value));
		} else
			strcpy ((char *) e->data, set_value);
		return;
	}

	/*
	 * Make sure we can handle this entry
	 */
	if ((e->components == 0) && *set_value) {
		g_warning (_("Setting a value for this tag is unsupported!"));
		return;
	}

	gboolean use_string = (set_value != NULL);
	if ( use_string ) {
		/* Copy the string so we can modify it */
		buf = g_strdup (set_value);
		if (!buf)
			return;
		value_p = strtok (buf, " ");
	}

	numcomponents = e->components;
	for (i = 0; i < numcomponents; ++i) {
		unsigned char s;

		if ( use_string ) {
			if (!value_p) {
				g_warning (_("Too few components specified (need %d, found %d)\n"), numcomponents, i);
				return;
			}
			if (!isdigit(*value_p) && (*value_p != '+') && (*value_p != '-')) {
				g_warning (_("Numeric value expected\n"));
				return;
			}
		}

		s = exif_format_get_size (e->format);
		switch (e->format) {
		case EXIF_FORMAT_ASCII:
			g_warning (_("This shouldn't happen!"));
			return;
			break;
		case EXIF_FORMAT_SHORT:
			exif_set_short (e->data + (s * i), o, atoi (value_p));
			break;
		case EXIF_FORMAT_SSHORT:
			exif_set_sshort (e->data + (s * i), o, atoi (value_p));
			break;
		case EXIF_FORMAT_RATIONAL: {
			ExifRational er;

			double val = 0.0 ;
			if ( use_string && value_p )
				val = fabs (atol (value_p));
			else
				val = fabs (gdvalue);

			if ( i == 0 ) {
				// One (or first) part rational

				// Sneak peek into tag as location tags need rounding down to give just the degrees part
				if ( e->tag == EXIF_TAG_GPS_LATITUDE || e->tag == EXIF_TAG_GPS_LONGITUDE ) {
					er.numerator = (ExifLong) floor ( val );
					er.denominator = 1.0;
				}
				else {
					// I don't see any point in doing anything too complicated here,
					//   such as trying to work out the 'best' denominator
					// For the moment use KISS principle.
					// Fix a precision of 1/100 metre as that's more than enough for GPS accuracy especially altitudes!
					er.denominator = 100.0;
					er.numerator = (ExifLong) (val * er.denominator);
				}
			}

			// Now for Location 3 part rationals do Mins and Seconds format

			// Rounded down minutes
			if ( i == 1 ) {
				er.denominator = 1.0;
				er.numerator = (ExifLong) ( (int) floor ( ( val - floor (val) ) * 60.0 ) );
			}

			// Finally seconds
			if ( i == 2 ) {
				er.denominator = 100.0;

				// Fractional minute.
				double FracPart = ((val - floor(val)) * 60) - (double)(int) floor ( ( val - floor (val) ) * 60.0 );
				er.numerator = (ExifLong) ( (int)floor(FracPart * 6000) ); // Convert to seconds.
			}
			exif_set_rational (e->data + (s * i), o, er );
			break;
		}
		case EXIF_FORMAT_LONG:
			exif_set_long (e->data + (s * i), o, atol (value_p));
			break;
		case EXIF_FORMAT_SLONG:
			exif_set_slong (e->data + (s * i), o, atol (value_p));
			break;
		case EXIF_FORMAT_BYTE:
		case EXIF_FORMAT_SBYTE:
		case EXIF_FORMAT_UNDEFINED: /* treat as byte array */
			e->data[s * i] = atoi (value_p);
			break;
		case EXIF_FORMAT_FLOAT:
		case EXIF_FORMAT_DOUBLE:
		case EXIF_FORMAT_SRATIONAL:
		default:
			g_warning (_("Not yet implemented!"));
			return;
		}
		
		if ( use_string )
			value_p = strtok (NULL, " ");

	}

	g_free (buf);

	if ( use_string )
		if ( value_p )
			g_warning (_("Warning; Too many components specified!"));
}
#endif

/**
 * a_geotag_write_exif_gps:
 * @filename: The image file to save information in
 * @coord:    The location
 * @alt:      The elevation
 *
 * Returns: A value indicating success: 0, or some other value for failure
 *
 */
gint a_geotag_write_exif_gps ( const gchar *filename, VikCoord coord, gdouble alt, gboolean no_change_mtime )
{
	gint result = 0; // OK so far...

	// Save mtime for later use
	struct stat stat_save;
	if ( no_change_mtime )
		stat ( filename, &stat_save );

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new ();
	if ( gexiv2_metadata_open_path ( gemd, filename, NULL ) ) {
		struct LatLon ll;
		vik_coord_to_latlon ( &coord, &ll );
		if ( ! gexiv2_metadata_set_gps_info ( gemd, ll.lon, ll.lat, alt ) ) {
			result = 1; // Failed
		}
		else {
			GError *error = NULL;
			if ( ! gexiv2_metadata_save_file ( gemd, filename, &error ) ) {
				result = 2;
				g_warning ( "Write EXIF failure:%s" , error->message );
				g_error_free ( error );
			}
		}
	}
	gexiv2_metadata_free ( gemd );
#else
#ifdef HAVE_LIBEXIF
	/*
	  Appears libexif doesn't actually support writing EXIF data directly to files
	  Thus embed command line exif writing method within Viking
	  (for example this is done by Enlightment - http://www.enlightenment.org/ )
	  This appears to be JPEG only, but is probably 99% of our use case
	  Alternatively consider using libexiv2 and C++...
	*/

	// Actual EXIF settings here...
	JPEGData *jdata;

	/* Parse the JPEG file. */
	jdata = jpeg_data_new ();
	jpeg_data_load_file (jdata, filename);

	// Get current values
	ExifData *ed = exif_data_new_from_file ( filename );
	if ( !ed )
		ed = exif_data_new ();

	// Update ExifData with our new settings
	ExifEntry *ee;
	//
	// I don't understand it, but when saving the 'ed' nothing gets set after putting in the GPS ID tag - so it must come last
	// (unless of course there is some bug in the setting of the ID, that prevents subsequent tags)
	//

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_ALTITUDE, EXIF_IFD_GPS);
	convert_to_entry ( NULL, alt, ee, exif_data_get_byte_order(ed) );

	// byte 0 meaning "sea level" or 1 if the value is negative.
	ee = my_exif_create_value (ed, EXIF_TAG_GPS_ALTITUDE_REF, EXIF_IFD_GPS);
	convert_to_entry ( alt < 0.0 ? "1" : "0", 0.0, ee, exif_data_get_byte_order(ed) );

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_PROCESSING_METHOD, EXIF_IFD_GPS);
	// see http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/GPS.html
	convert_to_entry ( "MANUAL", 0.0, ee, exif_data_get_byte_order(ed) );

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_MAP_DATUM, EXIF_IFD_GPS);
	convert_to_entry ( "WGS-84", 0.0, ee, exif_data_get_byte_order(ed) );

	struct LatLon ll;
    vik_coord_to_latlon ( &coord, &ll );

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_LATITUDE_REF, EXIF_IFD_GPS);
	// N or S
	convert_to_entry ( ll.lat < 0.0 ? "S" : "N", 0.0, ee, exif_data_get_byte_order(ed) );

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_LATITUDE, EXIF_IFD_GPS);
	convert_to_entry ( NULL, ll.lat, ee, exif_data_get_byte_order(ed) );

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_LONGITUDE_REF, EXIF_IFD_GPS);
	// E or W
	convert_to_entry ( ll.lon < 0.0 ? "W" : "E", 0.0, ee, exif_data_get_byte_order(ed) );

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_LONGITUDE, EXIF_IFD_GPS);
	convert_to_entry ( NULL, ll.lon, ee, exif_data_get_byte_order(ed) );

	ee = my_exif_create_value (ed, EXIF_TAG_GPS_VERSION_ID, EXIF_IFD_GPS);
	//convert_to_entry ( "2 0 0 0", 0.0, ee, exif_data_get_byte_order(ed) );
	convert_to_entry ( "2 2 0 0", 0.0, ee, exif_data_get_byte_order(ed) );

	jpeg_data_set_exif_data (jdata, ed);

	if ( jdata ) {
		/* Save the modified image. */
		result = jpeg_data_save_file (jdata, filename);

		// Convert result from 1 for success, 0 for failure into our scheme
		result = !result;
		
		jpeg_data_unref (jdata);
	}
	else {
		// Epic fail - file probably not a JPEG
		result = 2;
	}

	exif_data_free ( ed );
#endif
#endif

	if ( no_change_mtime ) {
		// Restore mtime, using the saved value
		struct stat stat_tmp;
		struct utimbuf utb;
		stat ( filename, &stat_tmp );
		utb.actime = stat_tmp.st_atime;
		utb.modtime = stat_save.st_mtime;
		utime ( filename, &utb );
	}

	return result;
}
