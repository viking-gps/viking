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
#include "viking.h"
#ifdef HAVE_EXPAT_H
#include <expat.h>
#endif
#include "vikgeoreflayer.h"

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

typedef enum {
	tt_unknown = 0,
	tt_kml,
	tt_kml_go,
	tt_kml_go_name,
	tt_kml_go_image,
	tt_kml_go_latlonbox,
	tt_kml_go_latlonbox_n,
	tt_kml_go_latlonbox_e,
	tt_kml_go_latlonbox_s,
	tt_kml_go_latlonbox_w,
} xtag_type;

typedef struct {
	xtag_type tag_type;              /* enum from above for this tag */
	const char *tag_name;           /* xpath-ish tag name */
} xtag_mapping;

#ifdef HAVE_ZIP_H
// Older libzip compatibility:
#ifndef zip_t
typedef struct zip zip_t;
typedef struct zip_file zip_file_t;
#endif
#ifndef ZIP_RDONLY
#define ZIP_RDONLY 0
#endif

#ifdef HAVE_EXPAT_H
typedef struct {
	GString *xpath;
	GString *c_cdata;
	xtag_type current_tag;
	gchar *name;
	gchar *image; // AKA icon
	gdouble north;
	gdouble east;
	gdouble south;
	gdouble west;
	zip_t *archive;
	struct zip_stat* zs;
	VikViewport *vvp;
	VikLayersPanel *vlp;
} xml_data;

// NB No support for orientation ATM
static xtag_mapping xtag_path_map[] = {
	{ tt_kml,                "/kml" },
	{ tt_kml_go,             "/kml/GroundOverlay" },
	{ tt_kml_go_name,        "/kml/GroundOverlay/name" },
	{ tt_kml_go_image,       "/kml/GroundOverlay/Icon/href" },
	{ tt_kml_go_latlonbox,   "/kml/GroundOverlay/LatLonBox" },
	{ tt_kml_go_latlonbox_n, "/kml/GroundOverlay/LatLonBox/north" },
	{ tt_kml_go_latlonbox_e, "/kml/GroundOverlay/LatLonBox/east" },
	{ tt_kml_go_latlonbox_s, "/kml/GroundOverlay/LatLonBox/south" },
	{ tt_kml_go_latlonbox_w, "/kml/GroundOverlay/LatLonBox/west" },
	// Some KML files have a 'Document' level
	{ tt_kml_go,             "/kml/Document/GroundOverlay" },
	{ tt_kml_go_name,        "/kml/Document/GroundOverlay/name" },
	{ tt_kml_go_image,       "/kml/Document/GroundOverlay/Icon/href" },
	{ tt_kml_go_latlonbox,   "/kml/Document/GroundOverlay/LatLonBox" },
	{ tt_kml_go_latlonbox_n, "/kml/Document/GroundOverlay/LatLonBox/north" },
	{ tt_kml_go_latlonbox_e, "/kml/Document/GroundOverlay/LatLonBox/east" },
	{ tt_kml_go_latlonbox_s, "/kml/Document/GroundOverlay/LatLonBox/south" },
	{ tt_kml_go_latlonbox_w, "/kml/Document/GroundOverlay/LatLonBox/west" },
};

// NB Don't be pedantic about matching case of strings for tags
static xtag_type get_tag ( const char *t )
{
	xtag_mapping *tm;
	for (tm = xtag_path_map; tm->tag_type != 0; tm++)
		if (tm->tag_name != NULL)
			if (0 == g_ascii_strcasecmp(tm->tag_name, t))
				return tm->tag_type;
	return tt_unknown;
}

static void kml_start ( xml_data *xd, const char *el, const char **attr )
{
	g_string_append_c ( xd->xpath, '/' );
	g_string_append ( xd->xpath, el );

	xd->current_tag = get_tag ( xd->xpath->str );
	switch ( xd->current_tag ) {
	case tt_kml_go:
		// Reset values
		xd->name = NULL;
		xd->image = NULL;
		xd->north = NAN;
		xd->south = NAN;
		xd->east = NAN;
		xd->west = NAN;
		break;
	case tt_kml_go_name:
	case tt_kml_go_image:
	case tt_kml_go_latlonbox_n:
	case tt_kml_go_latlonbox_s:
	case tt_kml_go_latlonbox_e:
	case tt_kml_go_latlonbox_w:
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	default: break;  // ignore cdata from other things
	}
}

// Fwd declaration
static void ground_overlay_load ( xml_data *xd );

static void kml_end ( xml_data *xd, const char *el )
{
	g_string_truncate ( xd->xpath, xd->xpath->len - strlen(el) - 1 );

	switch ( xd->current_tag ) {
	case tt_kml_go:
		ground_overlay_load ( xd );
		break;
	case tt_kml_go_name:
		g_free ( xd->name );
		xd->name = g_strdup ( xd->c_cdata->str );
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	case tt_kml_go_image:
		g_free ( xd->image );
		xd->image = g_strdup ( xd->c_cdata->str );
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	case tt_kml_go_latlonbox_n:
		xd->north = g_ascii_strtod ( xd->c_cdata->str, NULL );
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	case tt_kml_go_latlonbox_s:
		xd->south = g_ascii_strtod ( xd->c_cdata->str, NULL );
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	case tt_kml_go_latlonbox_e:
		xd->east = g_ascii_strtod ( xd->c_cdata->str, NULL );
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	case tt_kml_go_latlonbox_w:
		xd->west = g_ascii_strtod ( xd->c_cdata->str, NULL );
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	default:
		break;
	}

	xd->current_tag = get_tag ( xd->xpath->str );
}

static void kml_cdata ( xml_data *xd, const XML_Char *s, int len )
{
	switch ( xd->current_tag ) {
	case tt_kml_go_name:
	case tt_kml_go_image:
	case tt_kml_go_latlonbox_n:
	case tt_kml_go_latlonbox_s:
	case tt_kml_go_latlonbox_e:
	case tt_kml_go_latlonbox_w:
		g_string_append_len ( xd->c_cdata, s, len );
		break;
	default: break;  // ignore cdata from other things
	}
}

/**
 * Create a vikgeoreflayer for each <GroundOverlay> in the kml file
 *
 */
static void ground_overlay_load ( xml_data *xd )
{
	// Some simple detection of broken position values
	if ( isnan(xd->north) || isnan(xd->west) ||
	     isnan(xd->south) || isnan(xd->east) ||
	     xd->north > 90.001 || xd->north < -90.001 ||
	     xd->south > 90.001 || xd->south < -90.001 ||
	     xd->west > 180.001 || xd->west < -180.001 ||
	     xd->east > 180.001 || xd->east < -180.001 ) {
		g_warning ("%s: %s %s: N%f,S%f E%f,W%f", __FUNCTION__, "invalid lat/lon values detected in",
			   xd->name, xd->north, xd->south, xd->east, xd->west );
		return;
	}

	GdkPixbuf *pixbuf = NULL;

	// Read zip for image...
	if ( xd->image ) {
		if ( zip_stat ( xd->archive, xd->image, ZIP_FL_NOCASE | ZIP_FL_ENC_GUESS, xd->zs ) == 0) {
			zip_file_t *zfi = zip_fopen_index ( xd->archive, xd->zs->index, 0 );
			// Don't know a way to create a pixbuf using streams.
			// Thus write out to file
			// Could read in chunks rather than one big buffer, but don't expect images to be that big
			char *ibuffer = g_malloc(xd->zs->size);
			int ilen = zip_fread ( zfi, ibuffer, xd->zs->size );
			if ( ilen != xd->zs->size ) {
				g_warning ( "Unable to read %s from zip file", xd->image );
			}
			else {
				gchar *image_file = util_write_tmp_file_from_bytes ( ibuffer, ilen );
				GError *error = NULL;
				pixbuf = gdk_pixbuf_new_from_file ( image_file, &error );
				if ( error ) {
					g_warning ("%s: %s", __FUNCTION__, error->message );
					g_error_free ( error );
				}
				else {
					util_remove ( image_file );
				}
				g_free ( image_file );
			}
			g_free ( ibuffer );
		}
		g_free ( xd->image );
	}

	if ( pixbuf ) {
		VikCoord vc_tl, vc_br;
		struct LatLon ll_tl, ll_br;
		ll_tl.lat = xd->north;
		ll_tl.lon = xd->west;
		ll_br.lat = xd->south;
		ll_br.lon = xd->east;
		vik_coord_load_from_latlon ( &vc_tl, vik_viewport_get_coord_mode(xd->vvp), &ll_tl );
		vik_coord_load_from_latlon ( &vc_br, vik_viewport_get_coord_mode(xd->vvp), &ll_br );

		VikGeorefLayer *vgl = vik_georef_layer_create ( xd->vvp, xd->vlp, xd->name ? xd->name : "GeoRef", pixbuf, &vc_tl, &vc_br );
		if ( vgl ) {
			VikAggregateLayer *top = vik_layers_panel_get_top_layer ( xd->vlp );
			vik_aggregate_layer_add_layer ( top, VIK_LAYER(vgl), FALSE );
		}
	}
}

/**
 *
 */
static gboolean parse_kml ( const char* buffer, int len, VikViewport *vvp, VikLayersPanel *vlp, zip_t *archive, struct zip_stat* zs )
{
	XML_Parser parser = XML_ParserCreate(NULL);
	enum XML_Status status = XML_STATUS_ERROR;

	xml_data *xd = g_malloc ( sizeof (xml_data) );
	// Set default (invalid) values;
	xd->xpath = g_string_new ( "" );
	xd->c_cdata = g_string_new ( "" );
	xd->current_tag = tt_unknown;
	xd->north = NAN;
	xd->south = NAN;
	xd->east = NAN;
	xd->west = NAN;
	xd->name = NULL;
	xd->image = NULL;
	xd->archive = archive;
	xd->zs = zs;
	xd->vvp = vvp;
	xd->vlp = vlp;

	XML_SetElementHandler(parser, (XML_StartElementHandler) kml_start, (XML_EndElementHandler) kml_end);
	XML_SetUserData(parser, xd);
	XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) kml_cdata);

	status = XML_Parse(parser, buffer, len, TRUE);

	gboolean ans = (status != XML_STATUS_ERROR);
	if ( !ans ) {
		g_warning ( "%s: XML error %s at line %ld", __FUNCTION__, XML_ErrorString(XML_GetErrorCode(parser)), XML_GetCurrentLineNumber(parser) );
	}

	XML_ParserFree (parser);

	g_string_free ( xd->xpath, TRUE );
	g_string_free ( xd->c_cdata, TRUE );
	g_free ( xd );

	return ans;
}
#endif
#endif

/**
 * kmz_open_file:
 * @filename: The KMZ file to open
 * @vvp:      The #VikViewport
 * @vlp:      The #VikLayersPanel that the converted KMZ will be stored in
 *
 * Returns:
 *  -1 if KMZ not supported (this shouldn't happen)
 *  0 on success
 *  >0 <128 ZIP error code
 *  128 - No doc.kml file in KMZ
 *  129 - Couldn't understand the doc.kml file
 */
int kmz_open_file ( const gchar* filename, VikViewport *vvp, VikLayersPanel *vlp )
{
	// Unzip
#ifdef HAVE_ZIP_H

	int ans = ZIP_ER_OK;
	zip_t *archive = zip_open ( filename, ZIP_RDONLY, &ans );
	if ( !archive ) {
		g_warning ( "Unable to open archive: '%s' Error code %d", filename, ans );
		goto cleanup;
	}

	zip_int64_t zindex = zip_name_locate ( archive, "doc.kml", ZIP_FL_NOCASE | ZIP_FL_ENC_GUESS );
	if ( zindex == -1 ) {
		g_warning ( "Unable to find doc.kml" );
		goto kmz_cleanup;
	}

	struct zip_stat zs;
	if ( zip_stat_index( archive, zindex, 0, &zs ) == 0) {
		zip_file_t *zf = zip_fopen_index ( archive, zindex, 0 );
		char *buffer = g_malloc(zs.size);
		int len = zip_fread ( zf, buffer, zs.size );
		if ( len != zs.size ) {
			ans = 128;
			g_free ( buffer );
			g_warning ( "Unable to read doc.kml from zip file" );
			goto kmz_cleanup;
		}

		gboolean parsed = FALSE;
#ifdef HAVE_EXPAT_H
		parsed = parse_kml ( buffer, len, vvp, vlp, archive, &zs );
#endif
		g_free ( buffer );

		if ( !parsed ) {
			ans = 129;
		}
	}

kmz_cleanup:
	zip_discard ( archive ); // Close and ensure unchanged
 cleanup:
	return ans;
#else
	return -1;
#endif
}
