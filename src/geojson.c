/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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
 * See http://geojson.org/ for the specification
 *
 */

#include "geojson.h"
#include "gpx.h"
#include "globals.h"
#include "vikwindow.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

/**
 * Perform any cleanup actions once program has completed running
 */
static void my_watch ( GPid pid,
                       gint status,
                       gpointer user_data )
{
	g_spawn_close_pid ( pid );
}

/**
 * a_geojson_write_file:
 *
 * Returns TRUE if successfully written
 */
gboolean a_geojson_write_file ( VikTrwLayer *vtl, FILE *ff )
{
	gboolean result = FALSE;

	gchar *tmp_filename = a_gpx_write_tmp_file ( vtl, NULL );
	if ( !tmp_filename )
		return result;

	GPid pid;
	gint mystdout;

	// geojson program should be on the $PATH
	gchar **argv;
	argv = g_new (gchar*, 5);
	argv[0] = g_strdup (a_geojson_program_export());
	argv[1] = g_strdup ("-f");
	argv[2] = g_strdup ("gpx");
	argv[3] = g_strdup (tmp_filename);
	argv[4] = NULL;

	GError *error = NULL;
	// TODO: monitor stderr?
	if (!g_spawn_async_with_pipes (NULL, argv, NULL, (GSpawnFlags) G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL, &mystdout, NULL, &error)) {

		if ( IS_VIK_WINDOW ((VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(vtl)) ) {
			gchar* msg = g_strdup_printf ( _("%s command failed: %s"), argv[0], error->message );
			vik_window_statusbar_update ( (VikWindow*)VIK_GTK_WINDOW_FROM_LAYER(vtl), msg, VIK_STATUSBAR_INFO );
			g_free (msg);
		}
		else
			g_warning ("Async command failed: %s", error->message);

		g_error_free(error);
	}
	else {
		// Probably should use GIOChannels...
		gchar line[512];
		FILE *fout = fdopen(mystdout, "r");
		setvbuf(fout, NULL, _IONBF, 0);

		while (fgets(line, sizeof(line), fout)) {
			fprintf ( ff, "%s", line );
		}

		fclose(fout);

		g_child_watch_add ( pid, (GChildWatchFunc) my_watch, NULL );
		result = TRUE;
	}

	g_strfreev (argv);

	// Delete the temporary file
	(void)g_remove (tmp_filename);
	g_free (tmp_filename);

	return result;
}

//
// https://github.com/mapbox/togeojson
//
// https://www.npmjs.org/package/togeojson
//
// Tested with version 0.7.0
const gchar* a_geojson_program_export ( void )
{
	return "togeojson";
}

//
// https://github.com/tyrasd/togpx
//
// https://www.npmjs.org/package/togpx
//
// Tested with version 0.3.1
const gchar* a_geojson_program_import ( void )
{
	return "togpx";
}

/**
 * a_geojson_import_to_gpx:
 *
 * @filename: The source GeoJSON file
 *
 * Returns: The name of newly created temporary GPX file
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
gchar* a_geojson_import_to_gpx ( const gchar *filename )
{
	gchar *gpx_filename = NULL;
	GError *error = NULL;
	// Opening temporary file
	int fd = g_file_open_tmp("vik_geojson_XXXXXX.gpx", &gpx_filename, &error);
	if (fd < 0) {
		g_warning ( _("failed to open temporary file: %s"), error->message );
		g_clear_error ( &error );
		return NULL;
	}
	g_debug ( "%s: temporary file = %s", __FUNCTION__, gpx_filename );

	GPid pid;
	gint mystdout;

	// geojson program should be on the $PATH
	gchar **argv;
	argv = g_new (gchar*, 3);
	argv[0] = g_strdup (a_geojson_program_import());
	argv[1] = g_strdup (filename);
	argv[2] = NULL;

	FILE *gpxfile = fdopen (fd, "w");

	// TODO: monitor stderr?
	if (!g_spawn_async_with_pipes (NULL, argv, NULL, (GSpawnFlags) G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL, &mystdout, NULL, &error)) {
		g_warning ("Async command failed: %s", error->message);
		g_error_free(error);
	}
	else {
		// Probably should use GIOChannels...
		gchar line[512];
		FILE *fout = fdopen(mystdout, "r");
		setvbuf(fout, NULL, _IONBF, 0);

		while (fgets(line, sizeof(line), fout)) {
			fprintf ( gpxfile, "%s", line );
		}

		fclose(fout);
		g_child_watch_add ( pid, (GChildWatchFunc) my_watch, NULL );
	}

	fclose (gpxfile);

	g_strfreev (argv);

	return gpx_filename;
}

/**
 * Handle response from
 * http://project-osrm.org/docs/v5.23.0/api/#route-service
 * ATM only try to get the geometry out of the response
 * i.e. no handling of 'code' response or 'message'
 */
gboolean a_geojson_read_file_OSRM ( VikTrwLayer *vtl, const gchar *filename )
{
	JsonParser *jp = json_parser_new();
	GError *error = NULL;
	gboolean ans = json_parser_load_from_file ( jp, filename, &error );
	if ( error ) {
		g_warning ( "%s: parse load failed: %s", __FUNCTION__, error->message );
		g_error_free ( error );
		return FALSE;
	}

	JsonNode *result;
	JsonPath *path = json_path_new ();
	json_path_compile ( path, "$.routes..geometry..coordinates", NULL );
	result = json_path_match ( path, json_parser_get_root(jp) );

	JsonGenerator *generator = json_generator_new ();
	json_generator_set_root ( generator, result );

	// Not sure of a good way to use native glib types (or GObjects etc...)
	//  so simply hack apart this returned string
	gchar *str = json_generator_to_data ( generator, NULL );
	//g_debug ( "%s result: %s\n", __FUNCTION__, str );

	if ( strlen(str) > 8 ) {
		// Not sure how efficient this is; but should suffice for our use cases
		gchar *ptr = GINT_TO_POINTER(1);
		while ( ptr ) {
			// Remove the inbetween brackets
			ptr = g_strrstr ( str, "],[" );
			if ( ptr )
				// NB using memcpy() as compiler moans about not nul-terminated for strncpy(),
				//  but here we definitely want just a direct substitution
				//strncpy ( ptr, "   ", 3 );
				memcpy ( ptr, "   ", 3 );
		}

		// Tidy up beginning and ending brackets
		ptr = g_strrstr ( str, "]]]" );
		if ( ptr )
			memcpy ( ptr, "   ", 3 ); // as above		
		ptr = g_strrstr ( str, "[[[" );
		if ( ptr )
			memcpy ( ptr, "   ", 3 ); // as above

		gchar **coords = g_strsplit ( str, " ", -1 );

		gint gi = 0;
		gchar *coord = coords[0];
		if ( coord ) {
			ans = TRUE;
			VikCoordMode coord_mode = vik_trw_layer_get_coord_mode ( vtl );
			VikTrack *trk = vik_track_new ();
			trk->is_route = TRUE;
			VikTrackpoint *tp;
			struct LatLon ll;
			while ( coord ) {
				gchar **vals = g_strsplit ( coord, ",", -1 );
				guint nn = g_strv_length ( vals );
				if ( nn == 2 ) {
					tp = vik_trackpoint_new ();
					// Remember geojson coordinates are in the 'lon,lat' order
					ll.lat = g_ascii_strtod ( vals[1], NULL );
					ll.lon = g_ascii_strtod ( vals[0], NULL );
					vik_coord_load_from_latlon ( &tp->coord, coord_mode, &ll );
					trk->trackpoints = g_list_prepend ( trk->trackpoints, tp );
				}
				g_strfreev ( vals );
				//gi++;
				coord = coords[gi++];
			}
			trk->trackpoints = g_list_reverse ( trk->trackpoints );
            // Potentially could try to be more clever with the name...
			vik_trw_layer_filein_add_track ( vtl, N_("OSRM Route"), trk );
		}		
		g_strfreev ( coords );
	}
	g_object_unref ( jp );

	return ans;
}
