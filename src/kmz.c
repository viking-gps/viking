/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#include "kmz.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_ZIP_H
#include <zip.h>
#endif
#include "coords.h"
#include "fileutils.h"
#include <stdio.h>
#include <glib/gstdio.h>

#ifdef HAVE_ZIP_H
/**
 * Simple KML 'file' with a Ground Overlay
 *
 * See https://developers.google.com/kml/documentation/kmlreference
 *
 * AFAIK the projection is always in Web Mercator
 * Probably for normal use case of not too large an area coverage (on a Garmin device) the projection is near enough...
 */
// Hopefully image_filename will not break the XML file tag structure
static gchar* doc_kml_str ( const gchar *name, const gchar *image_filename, gdouble north, gdouble south, gdouble east, gdouble west )
{
	gchar *tmp_n = a_coords_dtostr ( north );
	gchar *tmp_s = a_coords_dtostr ( south );
	gchar *tmp_e = a_coords_dtostr ( east );
	gchar *tmp_w = a_coords_dtostr ( west );

	gchar *doc_kml = g_strdup_printf (
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" xmlns:kml=\"http://www.opengis.net/kml/2.2\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
		"<GroundOverlay>\n"
		"  <name>%s</name>\n"
		"  <Icon>\n"
		"    <href>%s</href>\n"
		"  </Icon>\n"
		"  <LatLonBox>\n"
		"    <north>%s</north>\n"
		"    <south>%s</south>\n"
		"    <east>%s</east>\n"
		"    <west>%s</west>\n"
		"    <rotation>0</rotation>\n" // Rotation always zero
		"  </LatLonBox>\n"
		"</GroundOverlay>\n"
		"</kml>\n",
		name, image_filename, tmp_n, tmp_s, tmp_e, tmp_w );

	g_free ( tmp_n );
	g_free ( tmp_s );
	g_free ( tmp_e );
	g_free ( tmp_w );

	return doc_kml;
}
#endif

/**
 * kmz_save_file:
 *
 * @pixbuf:   The image to save
 * @filename: Save the KMZ as this filename
 * @north:    Top latitude in degrees
 * @east:     Right most longitude in degrees
 * @south:    Bottom latitude in degrees
 * @west:     Left most longitude in degrees
 *
 * Returns:
 *  -1 if KMZ not supported (this shouldn't happen)
 *   0 on success
 *   >0 some kind of error
 *
 * Mostly intended for use with as a Custom Map on a Garmin
 *
 * See http://garminbasecamp.wikispaces.com/Custom+Maps
 *
 * The KMZ is a zipped file containing a KML file with the associated image
 */
int kmz_save_file ( GdkPixbuf *pixbuf, const gchar* filename, gdouble north, gdouble east, gdouble south, gdouble west )
{
#ifdef HAVE_ZIP_H
// Older libzip compatibility:
#ifndef zip_source_t
typedef struct zip_source zip_source_t;
#endif

	int ans = ZIP_ER_OK;
	gchar *image_filename = "image.jpg";

	// Generate KMZ file (a zip file)
	struct zip* archive = zip_open ( filename, ZIP_CREATE | ZIP_TRUNCATE, &ans );
	if ( !archive ) {
		g_warning ( "Unable to create archive: '%s' Error code %d", filename, ans );
		goto finish;
	}

	// Generate KML file
	gchar *dk = doc_kml_str ( a_file_basename(filename), image_filename, north, south, east, west );
	int dkl = strlen ( dk );

	// KML must be named doc.kml in the kmz file
	zip_source_t *src_kml = zip_source_buffer ( archive, dk, dkl, 0 );
	zip_file_add ( archive, "doc.kml", src_kml, ZIP_FL_OVERWRITE );

	GError *error = NULL;
	gchar *buffer;
	gsize blen;
	gdk_pixbuf_save_to_buffer ( pixbuf, &buffer, &blen, "jpeg", &error, "x-dpi", "72", "y-dpi", "72", NULL );
	if ( error ) {
		g_warning ( "Save to buffer error: %s", error->message );
		g_error_free (error);
		zip_discard ( archive );
		ans = 130;
		goto kml_cleanup;
	}

	zip_source_t *src_img = zip_source_buffer ( archive, buffer, (int)blen, 0 );
	zip_file_add ( archive, image_filename, src_img, ZIP_FL_OVERWRITE );
	// NB Only store as limited use trying to (further) compress a JPG
	zip_set_file_compression ( archive, 1, ZIP_CM_STORE, 0 );

	ans = zip_close ( archive );

	g_free ( buffer );

 kml_cleanup:
	g_free ( dk );
 finish:
	return ans;
#else
	return -1;
#endif
}

