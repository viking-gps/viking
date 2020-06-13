/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2017, Rob Norris <rw_norris@hotmail.com>
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
 * Dependencies in this file can be on anything.
 * For functions with simple system dependencies put it in util.c
 */
#include "viking.h"
#include "compression.h"
#include "download.h"
#include "vikmapslayer.h"
#include "settings.h"
#include "dir.h"
#include "degrees_converters.h"
#include "misc/kdtree.h"
#include "misc/gtkhtml-private.h"

#define FMT_MAX_NUMBER_CODES 9

/**
 * vu_trackpoint_formatted_message:
 * @format_code:  String describing the message to generate
 * @trkpt:        The trackpoint for which the message is generated about
 * @trkpt_prev:   A trackpoint (presumed previous) for interpolating values with the other trackpoint (such as speed)
 * @trk:          The track in which the trackpoints reside
 * @climb:        Vertical speed (Out of band (i.e. not in a trackpoint) value for display currently only for GPSD usage)
 *
 *  TODO: One day replace this cryptic format code with some kind of tokenizer parsing
 *    thus would make it more user friendly and maybe even GUI controlable.
 * However for now at least there is some semblance of user control
 */
gchar* vu_trackpoint_formatted_message ( gchar *format_code, VikTrackpoint *trkpt, VikTrackpoint *trkpt_prev, VikTrack *trk, gdouble climb )
{
	if ( !trkpt )
		return NULL;

	gint len = 0;
	if ( format_code )
		len = strlen ( format_code );
	if ( len > FMT_MAX_NUMBER_CODES )
		len = FMT_MAX_NUMBER_CODES;

	gchar* values[FMT_MAX_NUMBER_CODES];
	int i;
	for ( i = 0; i < FMT_MAX_NUMBER_CODES; i++ ) {
		values[i] = '\0';
	}

	vik_units_speed_t speed_units = a_vik_get_units_speed ();
	gchar *speed_units_str = vu_speed_units_text ( speed_units );

	gchar *separator = g_strdup ( " | " );

	for ( i = 0; i < len; i++ ) {
		switch ( g_ascii_toupper ( format_code[i] ) ) {
		case 'G': values[i] = g_strdup ( _("GPSD") ); break; // GPS Preamble
		case 'K': values[i] = g_strdup ( _("Trkpt") ); break; // Trkpt Preamble

		case 'S': {
			gdouble speed = 0.0;
			gchar *speedtype = NULL;
			if ( isnan(trkpt->speed) && trkpt_prev ) {
				if ( !isnan(trkpt->timestamp) && !isnan(trkpt_prev->timestamp) ) {
					if ( trkpt->timestamp != trkpt_prev->timestamp ) {

						// Work out from previous trackpoint location and time difference
						speed = vik_coord_diff(&(trkpt->coord), &(trkpt_prev->coord)) / ABS(trkpt->timestamp - trkpt_prev->timestamp);
						speedtype = g_strdup ( "*" ); // Interpolated
					}
					else
						speedtype = g_strdup ( "**" );
				}
				else
					speedtype = g_strdup ( "**" );
			}
			else {
				speed = trkpt->speed;
				speedtype = g_strdup ( "" );
			}
			speed = vu_speed_convert ( speed_units, speed );
			values[i] = g_strdup_printf ( _("%sSpeed%s %.1f%s"), separator, speedtype, speed, speed_units_str );
			g_free ( speedtype );
			break;
		}

		case 'B': {
			gdouble speed = 0.0;
			gchar *speedtype = NULL;
			if ( isnan(climb) && trkpt_prev ) {
				if ( !isnan(trkpt->timestamp) && !isnan(trkpt_prev->timestamp) ) {
					if ( trkpt->timestamp != trkpt_prev->timestamp ) {
						// Work out from previous trackpoint altitudes and time difference
						// 'speed' can be negative if going downhill
						speed = (trkpt->altitude - trkpt_prev->altitude) / ABS(trkpt->timestamp - trkpt_prev->timestamp);
						speedtype = g_strdup ( "*" ); // Interpolated
					}
					else
						speedtype = g_strdup ( "**" ); // Unavailable
				}
				else
					speedtype = g_strdup ( "**" );
			}
			else {
				speed = climb;
				speedtype = g_strdup ( "" );
			}
			speed = vu_speed_convert ( speed_units, speed );
			// Go for 2dp as expect low values for vertical speeds
			values[i] = g_strdup_printf ( _("%sClimb%s %.2f%s"), separator, speedtype, speed, speed_units_str );
			g_free ( speedtype );
			break;
		}

		case 'A': {
			if ( isnan(trkpt->altitude) ) {
				values[i] = g_strdup_printf ( _("%sAlt --"), separator );
			} else {
				vik_units_height_t height_units = a_vik_get_units_height ();
				switch (height_units) {
				case VIK_UNITS_HEIGHT_FEET:
					values[i] = g_strdup_printf ( _("%sAlt %dfeet"), separator, (int)round(VIK_METERS_TO_FEET(trkpt->altitude)) );
					break;
				default:
					//VIK_UNITS_HEIGHT_METRES:
					values[i] = g_strdup_printf ( _("%sAlt %dm"), separator, (int)round(trkpt->altitude) );
					break;
				}
			}
			break;
		}

		case 'C': {
			gint heading = isnan(trkpt->course) ? 0 : (gint)round(trkpt->course);
			values[i] = g_strdup_printf ( _("%sCourse %03d%s" ), separator, heading, DEGREE_SYMBOL );
			break;
		}

		case 'P': {
			if ( trkpt_prev ) {
				gint diff = (gint) round ( vik_coord_diff ( &(trkpt->coord), &(trkpt_prev->coord) ) );

				gchar *dist_units_str = NULL;
				vik_units_distance_t dist_units = a_vik_get_units_distance ();
				// expect the difference between track points to be small hence use metres or yards
				switch (dist_units) {
				case VIK_UNITS_DISTANCE_MILES:
				case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
					dist_units_str = g_strdup ( _("yards") );
					break;
				default:
					// VIK_UNITS_DISTANCE_KILOMETRES:
					dist_units_str = g_strdup ( _("m") );
					break;
				}

				values[i] = g_strdup_printf ( _("%sDistance diff %d%s"), separator, diff, dist_units_str );

				g_free ( dist_units_str );
			}
			break;
		}

		case 'T': {
			gchar *msg;
			if ( !isnan(trkpt->timestamp) ) {
				// Compact date time format
				time_t time = round ( trkpt->timestamp );
				msg = vu_get_time_string ( &time, "%x %X", &(trkpt->coord), NULL );
			}
			else
				msg = g_strdup ("--");
			values[i] = g_strdup_printf ( _("%sTime %s"), separator, msg );
			g_free ( msg );
			break;
		}

		case 'M': {
			if ( trkpt_prev ) {
				if ( !isnan(trkpt->timestamp) && !isnan(trkpt_prev->timestamp) ) {
					time_t t_diff = trkpt->timestamp - trkpt_prev->timestamp;
					values[i] = g_strdup_printf ( _("%sTime diff %lds"), separator, t_diff );
				}
			}
			break;
		}

		case 'X': values[i] = g_strdup_printf ( _("%sNo. of Sats %d"), separator, trkpt->nsats ); break;

		case 'F': {
			if ( trk ) {
				// Distance to the end 'Finish' (along the track)
				gdouble distd =	vik_track_get_length_to_trackpoint (trk, trkpt);
				gdouble diste =	vik_track_get_length_including_gaps ( trk );
				gdouble dist = diste - distd;
				gchar *dist_units_str = NULL;
				vik_units_distance_t dist_units = a_vik_get_units_distance ();
				switch (dist_units) {
				case VIK_UNITS_DISTANCE_MILES:
					dist_units_str = g_strdup ( _("miles") );
					dist = VIK_METERS_TO_MILES(dist);
					break;
				case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
					dist_units_str = g_strdup ( _("NM") );
					dist = VIK_METERS_TO_NAUTICAL_MILES(dist);
					break;
				default:
					// VIK_UNITS_DISTANCE_KILOMETRES:
					dist_units_str = g_strdup ( _("km") );
					dist = dist / 1000.0;
					break;
				}
				values[i] = g_strdup_printf ( _("%sTo End %.2f%s"), separator, dist, dist_units_str );
				g_free ( dist_units_str );
			}
			break;
		}

		case 'D': {
			if ( trk ) {
				// Distance from start (along the track)
				gdouble distd =	vik_track_get_length_to_trackpoint (trk, trkpt);
				gchar *dist_units_str = NULL;
				vik_units_distance_t dist_units = a_vik_get_units_distance ();
				switch (dist_units) {
				case VIK_UNITS_DISTANCE_MILES:
					dist_units_str = g_strdup ( _("miles") );
					distd = VIK_METERS_TO_MILES(distd);
					break;
				case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
					dist_units_str = g_strdup ( _("NM") );
					distd = VIK_METERS_TO_NAUTICAL_MILES(distd);
					break;
				default:
					// VIK_UNITS_DISTANCE_KILOMETRES:
					dist_units_str = g_strdup ( _("km") );
					distd = distd / 1000.0;
					break;
				}
				values[i] = g_strdup_printf ( _("%sDistance along %.2f%s"), separator, distd, dist_units_str );
				g_free ( dist_units_str );
			}
			break;
		}

		case 'L': {
			// Location (Lat/Long)
			gchar *lat = NULL, *lon = NULL;
			struct LatLon ll;
			vik_coord_to_latlon (&(trkpt->coord), &ll);
			a_coords_latlon_to_string ( &ll, &lat, &lon );
			values[i] = g_strdup_printf ( "%s%s %s", separator, lat, lon );
			g_free ( lat );
			g_free ( lon );
			break;
		}

		case 'N': // Name of track
			if ( trk )
				values[i] = g_strdup_printf ( _("%sTrack: %s"), separator, trk->name );
			break;

		case 'E': // Name of trackpoint if available
			if ( trkpt->name )
				values[i] = g_strdup_printf ( "%s%s", separator, trkpt->name );
			else
				values[i] = g_strdup ( "" );
			break;

		default:
			break;
		}
	}

	g_free ( separator );
	g_free ( speed_units_str );

	gchar *msg = g_strconcat ( values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8], NULL );

	for ( i = 0; i < FMT_MAX_NUMBER_CODES; i++ ) {
		g_free ( values[i] );
	}
	
	return msg;
}

typedef struct {
	GtkWindow *window; // Layer needed for redrawing
	gchar *version;    // Image list
} new_version_thread_data;

static gboolean new_version_available_message ( new_version_thread_data *nvtd )
{
	// Only a simple goto website option is offered
	// Trying to do an installation update is platform specific
	if ( a_dialog_yes_or_no ( nvtd->window,
	                        _("There is a newer version of Viking available: %s\n\nDo you wish to go to Viking's website now?"), nvtd->version ) )
		// NB 'VIKING_URL' redirects to the Wiki, here we want to go the main site.
		open_url ( nvtd->window, "http://sourceforge.net/projects/viking/" );

	g_free ( nvtd->version );
	g_free ( nvtd );
	return FALSE;
}

#define VIK_SETTINGS_VERSION_CHECKED_DATE "version_checked_date"

static void latest_version_thread ( GtkWindow *window )
{
	// Need to allow a few redirects, as SF file is often served from different server
	DownloadFileOptions options = { FALSE, FALSE, NULL, 5, NULL, NULL, NULL, NULL };
	gchar *filename = a_download_uri_to_tmp_file ( "http://sourceforge.net/projects/viking/files/VERSION", &options );
	//gchar *filename = g_strdup ( "VERSION" );
	if ( !filename ) {
		return;
	}

	GMappedFile *mf = g_mapped_file_new ( filename, FALSE, NULL );
	if ( !mf )
		return;

	gchar *text = g_mapped_file_get_contents ( mf );

	gint latest_version = viking_version_to_number ( text );
	gint my_version = viking_version_to_number ( VIKING_VERSION );

	g_debug ( "The lastest version is: %s", text );

	if ( my_version < latest_version ) {
		new_version_thread_data *nvtd = g_malloc ( sizeof(new_version_thread_data) );
		nvtd->window = window;
		nvtd->version = g_strdup ( text );
		gdk_threads_add_idle ( (GSourceFunc) new_version_available_message, nvtd );
	}
	else
		g_debug ( "Running the lastest version: %s", VIKING_VERSION );

	g_mapped_file_unref ( mf );
	if ( filename ) {
		g_remove ( filename );
		g_free ( filename );
	}

	// Update last checked time
	GTimeVal time;
	g_get_current_time ( &time );
	a_settings_set_string ( VIK_SETTINGS_VERSION_CHECKED_DATE, g_time_val_to_iso8601(&time) );
}

#define VIK_SETTINGS_VERSION_CHECK_PERIOD "version_check_period_days"

/**
 * vu_check_latest_version:
 * @window: Somewhere where we may need use the display to inform the user about the version status
 *
 * Periodically checks the released latest VERSION file on the website to compare with the running version
 *
 */
void vu_check_latest_version ( GtkWindow *window )
{
	if ( ! a_vik_get_check_version () )
		return;

	gboolean do_check = FALSE;

	gint check_period;
	if ( ! a_settings_get_integer ( VIK_SETTINGS_VERSION_CHECK_PERIOD, &check_period ) ) {
		check_period = 14;
	}

	// Get last checked date...
	GDate *gdate_last = g_date_new();
	GDate *gdate_now = g_date_new();
	GTimeVal time_last;
	gchar *last_checked_date = NULL;

	// When no previous date available - set to do the version check
	if ( a_settings_get_string ( VIK_SETTINGS_VERSION_CHECKED_DATE, &last_checked_date) ) {
		if ( g_time_val_from_iso8601 ( last_checked_date, &time_last ) ) {
			g_date_set_time_val ( gdate_last, &time_last );
		}
		else
			do_check = TRUE;
	}
	else
		do_check = TRUE;
	g_free ( last_checked_date );

	GTimeVal time_now;
	g_get_current_time ( &time_now );
	g_date_set_time_val ( gdate_now, &time_now );

	if ( ! do_check ) {
		// Dates available so do the comparison
		g_date_add_days ( gdate_last, check_period );
		if ( g_date_compare ( gdate_last, gdate_now ) < 0 )
			do_check = TRUE;
	}

	g_date_free ( gdate_last );
	g_date_free ( gdate_now );

	if ( do_check ) {
		g_thread_try_new ( "latest_version_thread", (GThreadFunc)latest_version_thread, window, NULL );
	}
}

/**
 * vu_set_auto_features_on_first_run:
 *
 *  Ask the user's opinion to set some of Viking's default behaviour
 */
void vu_set_auto_features_on_first_run ( void )
{
	gboolean auto_features = FALSE;
	gboolean set_defaults = FALSE;

	if ( a_vik_very_first_run () ) {

		GtkWidget *win = gtk_window_new ( GTK_WINDOW_TOPLEVEL );

		if ( a_dialog_yes_or_no ( GTK_WINDOW(win),
		                          _("This appears to be Viking's very first run.\n\nDo you wish to enable automatic internet features?\n\nIndividual settings can be controlled in the Preferences."), NULL ) )
			auto_features = TRUE;

		// Default to more standard cache layout for new users (well new installs at least)
		maps_layer_set_cache_default ( VIK_MAPS_CACHE_LAYOUT_OSM );
		set_defaults = TRUE;
	}

	if ( auto_features ) {
		// Set Maps to autodownload
		// Ensure the default is true
		maps_layer_set_autodownload_default ( TRUE );
		set_defaults = TRUE;

		// Enable auto add map +
		// Enable IP lookup
		VikLayerParam *pref_add_map = a_preferences_get_param ( VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer" );
		VikLayerParam *pref_startup_method = a_preferences_get_param ( VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method" );;
		if ( !pref_add_map || !pref_startup_method ) {
			g_critical ( "%s: preference not found", __FUNCTION__ );
			goto finish;
		}

		VikLayerParamData vlp_data;
		vlp_data.b = TRUE;
		a_preferences_run_setparam ( vlp_data, pref_add_map );

		vlp_data.u = VIK_STARTUP_METHOD_AUTO_LOCATION;
		a_preferences_run_setparam ( vlp_data, pref_startup_method );

		// Only on Windows make checking for the latest version on by default
		// For other systems it's expected a Package manager or similar controls the installation, so leave it off
#ifdef WINDOWS
		VikLayerParam *pref_startup_version_check = a_preferences_get_param ( VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version" );
		if ( !pref_startup_version_check ) {
			g_critical ( "%s: preference not found", __FUNCTION__ );
			goto finish;
		}

		vlp_data.b = TRUE;
		a_preferences_run_setparam ( vlp_data, pref_startup_version_check );
#endif

		// Ensure settings are saved for next time
		a_preferences_save_to_file ();
	}

 finish:
	// Ensure defaults are saved if changed
	if ( set_defaults )
		a_layer_defaults_save ();
}

/**
 * vu_get_canonical_filename:
 *
 * Returns: Canonical absolute filename
 *
 * Any time a path may contain a relative component, so need to prepend that directory it is relative to
 * Then resolve the full path to get the normal canonical filename
 */
gchar *vu_get_canonical_filename ( VikLayer *vl, const gchar *filename )
{
  gchar *canonical = NULL;
  if ( !filename )
    return NULL;

  if ( g_path_is_absolute ( filename ) )
    canonical = g_strdup ( filename );
  else {
    const gchar *vw_filename = vik_window_get_filename ( VIK_WINDOW_FROM_WIDGET (vl->vvp) );
    gchar *dirpath = NULL;
    if ( vw_filename )
      dirpath = g_path_get_dirname ( vw_filename );
    else
      dirpath = g_get_current_dir(); // Fallback - if here then probably can't create the correct path

    gchar *full = NULL;
    if ( g_path_is_absolute ( dirpath ) )
      full = g_strconcat ( dirpath, G_DIR_SEPARATOR_S, filename, NULL );
    else
      full = g_strconcat ( g_get_current_dir(), G_DIR_SEPARATOR_S, dirpath, G_DIR_SEPARATOR_S, filename, NULL );

    canonical = file_realpath_dup ( full ); // resolved
    g_free ( full );
    g_free ( dirpath );
  }

  return canonical;
}

static struct kdtree *kd = NULL;

/**
 * load_ll_tz_dir
 * @dir: The directory from which to load the latlontz.txt file
 *
 * Returns: The number of elements within the latlontz.txt loaded
 */
static gint load_ll_tz_dir ( const gchar *dir )
{
	gint inserted = 0;
	gchar *lltz = g_build_filename ( dir, "latlontz.txt", NULL );
	if ( g_access(lltz, R_OK) == 0 ) {
		gchar buffer[4096];
		long line_num = 0;
		FILE *ff = g_fopen ( lltz, "r" );
		if ( ff ) {
			while ( fgets ( buffer, 4096, ff ) ) {
				line_num++;
				gchar **components = g_strsplit (buffer, " ", 3);
				guint nn = g_strv_length ( components );
				if ( nn == 3 ) {
					double pt[2] = { g_ascii_strtod (components[0], NULL), g_ascii_strtod (components[1], NULL) };
					gchar *timezone = g_strchomp ( components[2] );
					if ( kd_insert ( kd, pt, timezone ) )
						g_critical ( "Insertion problem of %s for line %ld of latlontz.txt", timezone, line_num );
					else
						inserted++;
					// NB Don't free timezone as it's part of the kdtree data now
					g_free ( components[0] );
					g_free ( components[1] );
				} else {
					g_warning ( "Line %ld of latlontz.txt does not have 3 parts", line_num );
				}
				g_free ( components );
			}
			fclose ( ff );
		}
		else {
			g_warning ( "%s: Could not open %s", __FUNCTION__, lltz);
		}
	}
	g_free ( lltz );

	return inserted;
}

/**
 * vu_setup_lat_lon_tz_lookup:
 *
 * Can be called multiple times but only initializes the lookup once
 */
void vu_setup_lat_lon_tz_lookup ()
{
	// Only setup once
	if ( kd )
		return;

	kd = kd_create(2);

	// Look in the directories of data path
	gchar **data_dirs = a_get_viking_data_path();
	guint loaded = 0;
	// Process directories in reverse order for priority
	guint n_data_dirs = g_strv_length ( data_dirs );
	for (; n_data_dirs > 0; n_data_dirs--) {
		loaded += load_ll_tz_dir(data_dirs[n_data_dirs-1]);
	}
	g_strfreev ( data_dirs );

	g_debug ( "%s: Loaded %d elements", __FUNCTION__, loaded );
	if ( loaded == 0 )
		g_critical ( "%s: No lat/lon/timezones loaded", __FUNCTION__ );
}

/**
 * vu_finalize_lat_lon_tz_lookup:
 *
 * Clear memory used by the lookup.
 *  only call on program exit
 */
void vu_finalize_lat_lon_tz_lookup ()
{
	if ( kd ) {
		kd_data_destructor ( kd, g_free );
		kd_free ( kd );
	}
}

static double dist_sq( double *a1, double *a2, int dims ) {
  double dist_sq = 0, diff;
  while( --dims >= 0 ) {
    diff = (a1[dims] - a2[dims]);
    dist_sq += diff*diff;
  }
  return dist_sq;
}

static gchar* time_string_adjusted ( time_t *time, const gchar *format, gint offset_s )
{
	time_t *mytime = time;
	*mytime = *mytime + offset_s;
	gchar *str = g_malloc ( 64 );
	// Append asterisks to indicate use of simplistic model (i.e. no TZ)
	gchar *myformat = g_strdup_printf ( "%s **", format );
	strftime ( str, 64, myformat, gmtime(mytime) );
	g_free ( myformat );
	return str;
}

static gchar* time_string_tz ( time_t *time, const gchar *format, GTimeZone *tz )
{
	GDateTime *utc = g_date_time_new_from_unix_utc (*time);
	if ( !utc ) {
		g_warning ( "%s: result from g_date_time_new_from_unix_utc() is NULL", __FUNCTION__ );
		return NULL;
	}
	GDateTime *local = g_date_time_to_timezone ( utc, tz );
	if ( !local ) {
		g_date_time_unref ( utc );
		g_warning ( "%s: result from g_date_time_to_timezone() is NULL", __FUNCTION__ );
		return NULL;
	}
	gchar *str = g_date_time_format ( local, format );

	g_date_time_unref ( local );
	g_date_time_unref ( utc );
	return str;
}

#define VIK_SETTINGS_NEAREST_TZ_FACTOR "utils_nearest_tz_factor"
/**
 * vu_get_tz_at_location:
 *
 * @vc:     Position for which the time zone is desired
 *
 * Returns: TimeZone string of the nearest known location. String may be NULL.
 *
 * Use the k-d tree method (http://en.wikipedia.org/wiki/Kd-tree) to quickly retreive
 *  the nearest location to the given position.
 */
gchar* vu_get_tz_at_location ( const VikCoord* vc )
{
	gchar *tz = NULL;
	if ( !vc || !kd )
		return tz;

	struct LatLon ll;
	vik_coord_to_latlon ( vc, &ll );
	double pt[2] = { ll.lat, ll.lon };

	gdouble nearest;
	if ( !a_settings_get_double(VIK_SETTINGS_NEAREST_TZ_FACTOR, &nearest) )
		nearest = 1.0;

	struct kdres *presults = kd_nearest_range ( kd, pt, nearest );
	while( !kd_res_end( presults ) ) {
		double pos[2];
		gchar *ans = (gchar*)kd_res_item ( presults, pos );
		// compute the distance of the current result from the pt
		double dist = sqrt( dist_sq( pt, pos, 2 ) );
		if ( dist < nearest ) {
			//printf( "NEARER node at (%.3f, %.3f, %.3f) is %.3f away is %s\n", pos[0], pos[1], pos[2], dist, ans );
			nearest = dist;
			tz = ans;
		}
		kd_res_next ( presults );
	}
	g_debug ( "TZ lookup found %d results - picked %s", kd_res_size(presults), tz );
	kd_res_free ( presults );

	return tz;
}

/**
 * vu_get_time_string:
 *
 * @time_t: The time of which the string is wanted
 * @format  The format of the time string - such as "%c"
 * @vc:     Position of object for the time output - maybe NULL
 *          (only applicable for VIK_TIME_REF_WORLD)
 * @tz:     TimeZone string - maybe NULL.
 *          (only applicable for VIK_TIME_REF_WORLD)
 *          Useful to pass in the cached value from vu_get_tz_at_location() to save looking it up again for the same position
 *
 * Returns: A string of the time according to the time display property
 */
gchar* vu_get_time_string ( time_t *time, const gchar *format, const VikCoord* vc, const gchar *tz )
{
	if ( !format ) return NULL;
	gchar *str = NULL;
	switch ( a_vik_get_time_ref_frame() ) {
		case VIK_TIME_REF_UTC:
			str = g_malloc ( 64 );
			strftime ( str, 64, format, gmtime(time) ); // Always 'GMT'
			break;
		case VIK_TIME_REF_WORLD:
			if ( vc && !tz ) {
				// No timezone specified so work it out
				gchar *mytz = vu_get_tz_at_location ( vc );
				if ( mytz ) {
					GTimeZone *gtz = g_time_zone_new ( mytz );
					str = time_string_tz ( time, format, gtz );
					g_time_zone_unref ( gtz );
				}
				else {
					// No results (e.g. could be in the middle of a sea)
					// Fallback to simplistic method that doesn't take into account Timezones of countries.
					struct LatLon ll;
					vik_coord_to_latlon ( vc, &ll );
					str = time_string_adjusted ( time, format, round ( ll.lon / 15.0 ) * 3600 );
				}
			}
			else {
				// Use specified timezone
				GTimeZone *gtz = g_time_zone_new ( tz );
				str = time_string_tz ( time, format, gtz );
				g_time_zone_unref ( gtz );
			}
			break;
		default: // VIK_TIME_REF_LOCALE
			str = g_malloc ( 64 );
			strftime ( str, 64, format, localtime(time) );
			break;
	}
	return str;
}

/**
 * vu_command_line:
 *
 * Apply any startup values that have been specified from the command line
 * Values are defaulted in such a manner not to be applied when they haven't been specified
 *
 */
void vu_command_line ( VikWindow *vw, gdouble latitude, gdouble longitude, gint zoom_osm_level, gint map_id )
{
	if ( !vw )
		return;

	VikViewport *vvp = vik_window_viewport(vw);

	if ( latitude != 0.0 || longitude != 0.0 ) {
		struct LatLon ll;
		ll.lat = latitude;
		ll.lon = longitude;
		vik_viewport_set_center_latlon ( vvp, &ll, TRUE );
	}

	if ( zoom_osm_level >= 0 ) {
		// Convert OSM zoom level into Viking zoom level
		gdouble mpp = exp ( (17-zoom_osm_level) * log(2) );
		if ( mpp > 1.0 )
			mpp = round (mpp);
		vik_viewport_set_zoom ( vvp, mpp );
	}

	if ( map_id >= 0 ) {
		guint my_map_id = map_id;
		if ( my_map_id == 0 )
			my_map_id = vik_maps_layer_get_default_map_type ();

		// Don't add map layer if one already exists
		GList *vmls = vik_layers_panel_get_all_layers_of_type(vik_window_layers_panel(vw), VIK_LAYER_MAPS, TRUE);
		int num_maps = g_list_length(vmls);
		gboolean add_map = TRUE;

		for (int i = 0; i < num_maps; i++) {
			VikMapsLayer *vml = (VikMapsLayer*)(vmls->data);
			gint id = vik_maps_layer_get_map_type(vml);
			if ( my_map_id == id ) {
				add_map = FALSE;
				break;
			}
			vmls = vmls->next;
		}

		if ( add_map ) {
			VikMapsLayer *vml = VIK_MAPS_LAYER ( vik_layer_create(VIK_LAYER_MAPS, vvp, FALSE) );
			vik_maps_layer_set_map_type ( vml, my_map_id );
			vik_layer_rename ( VIK_LAYER(vml), _("Map") );
			vik_aggregate_layer_add_layer ( vik_layers_panel_get_top_layer(vik_window_layers_panel(vw)), VIK_LAYER(vml), TRUE );
			vik_layer_emit_update ( VIK_LAYER(vml) );
		}
	}
}

/**
 * Copy the displayed text of a widget (should be a GtkButton ATM)
 */
static void vu_copy_label ( GtkWidget *widget )
{
	a_clipboard_copy (VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, gtk_button_get_label(GTK_BUTTON(widget)), NULL );
}

/**
 * Generate a single entry menu to allow copying the displayed text of a widget (should be a GtkButton ATM)
 */
void vu_copy_label_menu ( GtkWidget *widget, guint button )
{
	GtkWidget *menu = gtk_menu_new();
	GtkWidget *item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_COPY, NULL );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(vu_copy_label), widget );
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );
	gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, button, gtk_get_current_event_time() );
}

/**
 * Work out the best zoom level for the LatLon area and set the viewport to that zoom level
 */
void vu_zoom_to_show_latlons ( VikCoordMode mode, VikViewport *vvp, struct LatLon maxmin[2] )
{
	/* First set the center [in case previously viewing from elsewhere] */
	/* Then loop through zoom levels until provided positions are in view */
	/* This method is not particularly fast - but should work well enough */
	struct LatLon average = { (maxmin[0].lat+maxmin[1].lat)/2, (maxmin[0].lon+maxmin[1].lon)/2 };
	VikCoord coord;
	vik_coord_load_from_latlon ( &coord, mode, &average );
	vik_viewport_set_center_coord ( vvp, &coord, TRUE );

	/* Convert into definite 'smallest' and 'largest' positions */
	struct LatLon minmin;
	if ( maxmin[0].lat < maxmin[1].lat )
		minmin.lat = maxmin[0].lat;
	else
		minmin.lat = maxmin[1].lat;

	struct LatLon maxmax;
	if ( maxmin[0].lon > maxmin[1].lon )
		maxmax.lon = maxmin[0].lon;
	else
		maxmax.lon = maxmin[1].lon;

	/* Never zoom in too far - generally not that useful, as too close ! */
	/* Always recalculate the 'best' zoom level */
	gdouble zoom = 1.0;
	vik_viewport_set_zoom ( vvp, zoom );

	gdouble min_lat, max_lat, min_lon, max_lon;
	/* Should only be a maximum of about 18 iterations from min to max zoom levels */
	while ( zoom <= VIK_VIEWPORT_MAX_ZOOM ) {
		vik_viewport_get_min_max_lat_lon ( vvp, &min_lat, &max_lat, &min_lon, &max_lon );
		/* NB I think the logic used in this test to determine if the bounds is within view
		   fails if track goes across 180 degrees longitude.
		   Hopefully that situation is not too common...
		   Mind you viking doesn't really do edge locations to well anyway */
		if ( min_lat < minmin.lat &&
		     max_lat > minmin.lat &&
		     min_lon < maxmax.lon &&
		     max_lon > maxmax.lon )
			/* Found within zoom level */
			break;

		/* Try next */
		zoom = zoom * 2;
		vik_viewport_set_zoom ( vvp, zoom );
	}
}

/**
 * Set the waypoint image given a URI
 */
void vu_waypoint_set_image_uri ( VikWaypoint *wp, const gchar *uri, const gchar *dirpath )
{
	gchar *filename = gtk_html_filename_from_uri ( uri );
	if ( g_path_is_absolute ( filename ) ) {
		vik_waypoint_set_image ( wp, filename );
	}
	else {
		// Try to form full path
		gchar *full = g_strconcat ( dirpath, G_DIR_SEPARATOR_S, filename, NULL );
		vik_waypoint_set_image ( wp, full );
		g_free ( full );
	}
}

/**
 * Set a calendar widget to today's date
 */
void vu_calendar_set_to_today (GtkWidget *cal)
{
  GDateTime *now = g_date_time_new_now_local ();
  gtk_calendar_select_month ( GTK_CALENDAR(cal), g_date_time_get_month(now)-1, g_date_time_get_year(now) );
  gtk_calendar_select_day ( GTK_CALENDAR(cal), g_date_time_get_day_of_month(now) );
  g_date_time_unref ( now );
}

/**
 * vu_menu_add_item:
 *
 * @menu:     The menu to which this new menu item is added
 * @mnemonic: The accelerator string to use for the menu item e.g. "_New"
 *            May be null, typically for creating menu separators
 *            For stock icons, this string will override the Gtk default
 * @stock_icon: Display this icon on the menu item
 *              A GTK Stock icon name, or an own icon name that has been registered
 *               see a_register_icon() in vikwindow.c
 *              May be null, for no icon
 * @callback: The function to be called when the menu item is clicked
 *            A #GCallback invoked by a 'swapped' connect signal
 *            May be null, typically for creating sub menus
 * @user_data: Data to be used in the callback
 *            May be null, typically for creating sub menus
 *
 * A utility function to simplify creating menus
 *
 * Returns: The menu item created.
 */
GtkWidget* vu_menu_add_item ( const GtkMenu *menu,
                              const gchar* mnemonic,
                              const gchar* stock_icon,
                              const GCallback callback,
                              const gpointer user_data )
{
	GtkWidget *menu_item = gtk_menu_item_new();
	if ( mnemonic && stock_icon ) {
		menu_item = gtk_image_menu_item_new_with_mnemonic ( mnemonic );
		gtk_image_menu_item_set_image ( (GtkImageMenuItem*)menu_item, gtk_image_new_from_stock (stock_icon, GTK_ICON_SIZE_MENU) );
	}
	else if ( mnemonic )
		menu_item = gtk_menu_item_new_with_mnemonic ( mnemonic );
	else if ( stock_icon )
		menu_item = gtk_image_menu_item_new_from_stock ( stock_icon, NULL );
	if ( callback )
		g_signal_connect_swapped ( G_OBJECT(menu_item), "activate", callback, user_data );
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), menu_item );
	return menu_item;
}

/**
 * vu_speed_deconvert:
 *
 * Convert the speed from the specified units into m/s
 */
gdouble vu_speed_deconvert ( vik_units_speed_t speed_units, gdouble speed )
{
	gdouble my_speed = speed;
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: my_speed = VIK_KPH_TO_MPS(speed); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      my_speed = VIK_MPH_TO_MPS(speed); break;
	case VIK_UNITS_SPEED_KNOTS:               my_speed = VIK_KNOTS_TO_MPS(speed); break;
	case VIK_UNITS_SPEED_SECONDS_PER_KM:      my_speed = (3600.0 / speed) / VIK_KPH_IN_MPS; break;
	case VIK_UNITS_SPEED_MINUTES_PER_KM:      my_speed = (60.0 / speed) / VIK_KPH_IN_MPS; break;
	case VIK_UNITS_SPEED_SECONDS_PER_MILE:    my_speed = (5793.6384 / speed) / VIK_KPH_IN_MPS; break;
	case VIK_UNITS_SPEED_MINUTES_PER_MILE:    my_speed = (96.56064 / speed) / VIK_KPH_IN_MPS; break;
	default: break; // VIK_UNITS_SPEED_METRES_PER_SECOND:
	}
	return my_speed;
}

/**
 * vu_speed_convert:
 *
 * Convert the speed from m/s into the specified units
 */
gdouble vu_speed_convert ( vik_units_speed_t speed_units, gdouble speed )
{
	gdouble my_speed = speed;
	switch (speed_units) {
	case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR: my_speed = VIK_MPS_TO_KPH(speed); break;
	case VIK_UNITS_SPEED_MILES_PER_HOUR:      my_speed = VIK_MPS_TO_MPH(speed); break;
	case VIK_UNITS_SPEED_KNOTS:               my_speed = VIK_MPS_TO_KNOTS(speed); break;
	case VIK_UNITS_SPEED_SECONDS_PER_KM:      my_speed = VIK_MPS_TO_PACE_SPK(speed); break;
	case VIK_UNITS_SPEED_MINUTES_PER_KM:      my_speed = VIK_MPS_TO_PACE_MPK(speed); break;
	case VIK_UNITS_SPEED_SECONDS_PER_MILE:    my_speed = VIK_MPS_TO_PACE_SPM(speed); break;
	case VIK_UNITS_SPEED_MINUTES_PER_MILE:    my_speed = VIK_MPS_TO_PACE_MPM(speed); break;
	default: break; // VIK_UNITS_SPEED_METRES_PER_SECOND:
	}
	return my_speed;
}

/**
 * vu_speed_units_text:
 *
 * Free the returned string after use
 */
gchar* vu_speed_units_text ( vik_units_speed_t speed_units )
{
	gchar *speed_units_str = NULL;
	switch ( speed_units ) {
	case VIK_UNITS_SPEED_MILES_PER_HOUR:    speed_units_str = g_strdup ( _("mph") ); break;
	case VIK_UNITS_SPEED_METRES_PER_SECOND:	speed_units_str = g_strdup ( _("m/s") ); break;
	case VIK_UNITS_SPEED_KNOTS:             speed_units_str = g_strdup ( _("knots") ); break;
	case VIK_UNITS_SPEED_SECONDS_PER_KM:    speed_units_str = g_strdup ( _("s/km") ); break;
	case VIK_UNITS_SPEED_MINUTES_PER_KM:    speed_units_str = g_strdup ( _("min/km") );	break;
	case VIK_UNITS_SPEED_SECONDS_PER_MILE:  speed_units_str = g_strdup ( _("sec/mi") ); break;
	case VIK_UNITS_SPEED_MINUTES_PER_MILE:  speed_units_str = g_strdup ( _("min/mi") );	break;
		// VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
	default:                                speed_units_str = g_strdup ( _("km/h") ); break;
	}
	return speed_units_str;
}

/**
 * Commonal text for just the speed value
 */
void vu_speed_text_value ( gchar* buf, guint size, vik_units_speed_t speed_units, gdouble speed, gchar *format )
{
	switch (speed_units) {
	case VIK_UNITS_SPEED_MINUTES_PER_KM: // Fall through
	case VIK_UNITS_SPEED_MINUTES_PER_MILE: {
		// Mins per dist values -> prefer mins:secs output rather than decimal minutes
		// NB Thus the input format function parameter is ignored for these
		gint mins = (gint)speed;
		gdouble secs = (speed - (gdouble)mins) * 60.0;
		gint isecs = (gint)round(secs);
		if ( isecs == 60 ) {
			isecs = 0;
			mins++;
		}
		g_snprintf ( buf, size, "%d:%02d", mins, isecs );
		break;
	};
	default:
		g_snprintf ( buf, size, format, speed );
		break;
	}
}

/**
 * Commonal text for a speed readout
 */
void vu_speed_text ( gchar* buf, guint size, vik_units_speed_t speed_units, gdouble speed, gboolean convert, gchar *format, gboolean compact )
{
	if ( isnan(speed) ) {
		g_snprintf (buf, size, "--" );
		return;
	}

	gdouble my_speed = speed;
	if ( convert )
		my_speed = vu_speed_convert ( speed_units, speed );

	gchar *speed_units_str = vu_speed_units_text ( speed_units );

    gchar tbuf[16];
	vu_speed_text_value ( tbuf, sizeof(tbuf), speed_units, my_speed, format );

	gchar *full_str = NULL;
	if ( compact )
		full_str = g_strdup_printf ( "%s%s", tbuf, speed_units_str );
	else
		full_str = g_strdup_printf ( "%s %s", tbuf, speed_units_str );
	g_snprintf ( buf, size, full_str, my_speed );
	g_free ( full_str );

	g_free ( speed_units_str );
}

/**
 * vu_distance_units_text:
 *
 * Free the returned string after use
 */
gchar* vu_distance_units_text ( vik_units_distance_t distance_units )
{
	gchar *units_str = NULL;
	switch ( distance_units ) {
	case VIK_UNITS_DISTANCE_MILES:          units_str = g_strdup ( _("miles") ); break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:	units_str = g_strdup ( _("NM") ); break;
		// VIK_UNITS_DISTANCE_KILOMETRES:
	default:                                units_str = g_strdup ( _("km") ); break;
	}
	return units_str;
}

/**
 * vu_speed_deconvert:
 *
 * Convert the distance from the specified units into metres
 */
gdouble vu_distance_deconvert ( vik_units_distance_t distance_units, gdouble distance )
{
	gdouble my_distance = distance;
	switch ( distance_units ) {
	case VIK_UNITS_DISTANCE_MILES:          my_distance = VIK_MILES_TO_METERS(distance); break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES: my_distance = VIK_NAUTICAL_MILES_TO_METERS(distance); break;
	    //case VIK_UNITS_DISTANCE_KILOMETRES:
	default:                                my_distance = distance * 1000.0; break;
	}
	return my_distance;
}

/**
 * vu_distance_convert:
 *
 * Convert the distance in metres into the specified units
 */
gdouble vu_distance_convert ( vik_units_distance_t distance_units, gdouble distance )
{
	gdouble my_distance = distance;
	switch ( distance_units ) {
	case VIK_UNITS_DISTANCE_MILES:          my_distance = VIK_METERS_TO_MILES(distance); break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES: my_distance = VIK_METERS_TO_NAUTICAL_MILES(distance); break;
	    //case VIK_UNITS_DISTANCE_KILOMETRES:
	default:                                my_distance = distance / 1000.0; break;
	}
	return my_distance;
}

/**
 * Commonal text for a distance readout
 */
void vu_distance_text ( gchar* buf, guint size, vik_units_distance_t distance_units, gdouble distance, gboolean convert, gchar *format, gboolean compact )
{
	if ( isnan(distance) ) {
		g_snprintf ( buf, size, "--" );
		return;
	}

	gdouble my_distance = distance;
	if ( convert )
		my_distance = vu_distance_convert ( distance_units, distance );

	gchar *units_str = vu_distance_units_text ( distance_units );

    gchar tbuf[32];
	g_snprintf ( tbuf, sizeof(tbuf), format, my_distance );

	gchar *full_str = NULL;
	if ( compact )
		full_str = g_strdup_printf ( "%s%s", tbuf, units_str );
	else
		full_str = g_strdup_printf ( "%s %s", tbuf, units_str );
	g_snprintf ( buf, size, full_str );
	g_free ( full_str );
	g_free ( units_str );
}

/**
 * Commonal text for a distance readout
 * This version uses higher precision units
 *  i.e. metres or yards, rather than miles, kms etc...
 * So use for distances that are expected to be small
 */
void vu_distance_text_precision ( gchar* buf, guint size, vik_units_distance_t distance_units, gdouble distance, gchar *format )
{
	if ( isnan(distance) ) {
		g_snprintf ( buf, size, "--" );
		return;
	}

	gdouble my_distance = distance;
	gchar *units_str = NULL;
	switch ( distance_units ) {
	case VIK_UNITS_DISTANCE_MILES:
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		units_str = g_strdup ( _("yards") );
		my_distance = distance * 1.0936133;
		break;
		// VIK_UNITS_DISTANCE_KILOMETRES:
	default:
		units_str = g_strdup ( _("m") );
		break;
	}
	gchar tbuf[32];
	g_snprintf ( tbuf, sizeof(tbuf), format, my_distance );

	gchar *full_str = g_strdup_printf ( "%s %s", tbuf, units_str );
	g_snprintf ( buf, size, full_str );
	g_free ( full_str );
	g_free ( units_str );
}

// This is in vikutils due to speed units dependency
// Unfortunatelty can't simply use the .1f format
//  as want text for minutes per distance speed values
void vu_format_speed_cell_data_func ( GtkTreeViewColumn *col,
                                      GtkCellRenderer   *renderer,
                                      GtkTreeModel      *model,
                                      GtkTreeIter       *iter,
                                      gpointer           user_data )
{
	// Seems a little unecessary to call this each time
	// TODO determine a better way to pass this value around
	//  (but then probably need to deal with lifecycle of memory allocation...)
	vik_units_speed_t speed_units = a_vik_get_units_speed ();

	gdouble value;
	gchar buf[16];
	gint column = GPOINTER_TO_INT (user_data);
	gtk_tree_model_get ( model, iter, column, &value, -1 );

	// NB cell value already in the appropriate units for display
	vu_speed_text_value ( buf, sizeof(buf), speed_units, value, "%.1f" );

	g_object_set ( renderer, "text", buf, NULL );
}

// The last used directories
static gchar *last_folder_files_uri = NULL;

gchar* vu_get_last_folder_files_uri ()
{
	return last_folder_files_uri;
}

void vu_set_last_folder_files_uri ( gchar *folder_uri )
{
	g_free ( last_folder_files_uri );
	last_folder_files_uri = folder_uri;
}

/**
 * Manually clear up any memory allocated here
 */
void vu_finish ( void )
{
	g_free ( last_folder_files_uri );
}

/**
 * vu_get_ui_selected_gps_files:
 * @external: Select filter allows only file types usuable for External Layers
 *            (i.e. basically just GPX files)
 *
 * Returned list must be freed after use
 *
 **/
GSList* vu_get_ui_selected_gps_files ( VikWindow *vw, gboolean external )
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Please select a GPS data file to open. "),
	                                                 GTK_WINDOW(vw),
	                                                 GTK_FILE_CHOOSER_ACTION_OPEN,
	                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                                 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                                 NULL);
	if ( last_folder_files_uri )
		gtk_file_chooser_set_current_folder_uri ( GTK_FILE_CHOOSER(dialog), last_folder_files_uri );

	GtkFileFilter *filter;
	// NB file filters are listed this way for alphabetical ordering
	//  (at least in the default English langauge)
	if ( !external ) {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name( filter, _("Bzip2 File") );
		gtk_file_filter_add_mime_type ( filter, "application/x-bzip2");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

#ifdef VIK_CONFIG_GEOCACHES
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name( filter, _("Geocaching") );
		gtk_file_filter_add_pattern ( filter, "*.loc" ); // No MIME type available
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
#endif

		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name( filter, _("Google Earth") );
		gtk_file_filter_add_mime_type ( filter, "application/vnd.google-earth.kml+xml");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
	}

	// NB The only named filter for 'External' types ATM
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name( filter, _("GPX") );
	gtk_file_filter_add_mime_type ( filter, "gpx+xml");
	gtk_file_filter_add_pattern ( filter, "*.gpx" );
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
	if ( external )
		gtk_file_chooser_set_filter ( GTK_FILE_CHOOSER(dialog), filter );

	if ( !external ) {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name ( filter, _("JPG") );
		gtk_file_filter_add_mime_type ( filter, "image/jpeg");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name( filter, _("TCX") );
		gtk_file_filter_add_pattern ( filter, "*.tcx" );
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name( filter, _("Zip File") );
		gtk_file_filter_add_mime_type ( filter, "application/zip");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

		// Viking type always second from last
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name( filter, _("Viking") );
		gtk_file_filter_add_pattern ( filter, "*.vik" );
		gtk_file_filter_add_pattern ( filter, "*.viking" );
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
	}

	// NB could have filters for gpspoint (*.gps,*.gpsoint?) + gpsmapper (*.gsm,*.gpsmapper?)
	// However assume this are barely used and thus not worthy of inclusion
	//   as they'll just make the options too many and have no clear file pattern
	//   one can always use the all option
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name( filter, _("All") );
	gtk_file_filter_add_pattern ( filter, "*" );
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
	// Default to any file - same as before open filters were added
	if ( !external )
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER(dialog), filter);

	gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER(dialog), TRUE );
	gtk_window_set_transient_for ( GTK_WINDOW(dialog), GTK_WINDOW(vw) );
	gtk_window_set_destroy_with_parent ( GTK_WINDOW(dialog), TRUE );

	GSList *files = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {
	    vu_set_last_folder_files_uri ( gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(dialog)) );
		files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(dialog) );
	}
	gtk_widget_destroy ( dialog );

	return files;
}

/**
 * The sort uses the fact that the UUIDs will simply increase
 */
static int sort_hash_compare ( gconstpointer aa, gconstpointer bb, gpointer order )
{
	SortTRWHashT *sa = (SortTRWHashT*)aa;
	SortTRWHashT *sb = (SortTRWHashT*)bb;

	// Sort 'none' AKA creation order - use the UUID
	if ( GPOINTER_TO_INT(order) == VL_SO_NONE )
		return sa->uuid > sb->uuid;

	gint answer = -1;

	if ( GPOINTER_TO_INT(order) < VL_SO_DATE_ASCENDING ) {
		// Alphabetical comparison
		// Default ascending order
		answer = g_strcmp0 ( sa->name, sb->name );
		// Invert sort order for descending order
		if ( GPOINTER_TO_INT(order) == VL_SO_ALPHABETICAL_DESCENDING )
			answer = -answer;
	}
	else if ( GPOINTER_TO_INT(order) < VL_SO_NUMBER_ASCENDING ) {
		// Date comparison
		gboolean ans = ( sa->timestamp > sb->timestamp );
		if ( ans )
			answer = 1;
		// Invert sort order for descending order
		if ( GPOINTER_TO_INT(order) == VL_SO_DATE_DESCENDING )
			answer = -answer;
	} else {
		// Number comparison
		gboolean ans = ( sa->number > sb->number );
		if ( ans )
			answer = 1;
		// Invert sort order for descending order
		if ( GPOINTER_TO_INT(order) == VL_SO_NUMBER_DESCENDING )
			answer = -answer;
	}
	return answer;
}

/**
 * Sort only applies to HashTables as used in TrackWaypoint layers
 * (i.e. waypoints, tracks & routes)
 *
 * Returns: A Glist of SortTRWHashT entries
 */
GList* vu_sorted_list_from_hash_table ( GHashTable *hash_table, vik_layer_sort_order_t order, VikTRWDataTypeT type )
{
	GList *gl = NULL;
	gpointer key, value;
	GHashTableIter iter;
	g_hash_table_iter_init ( &iter, hash_table );
	while ( g_hash_table_iter_next (&iter, &key, &value) ) {
		SortTRWHashT *st = g_malloc (sizeof(SortTRWHashT));
		st->uuid = GPOINTER_TO_UINT(key);
		st->type = type;
		if ( type == VIKING_WAYPOINT ) {
			VikWaypoint *wp = VIK_WAYPOINT(value);
			st->name = wp->name;
			st->timestamp = wp->timestamp;
		} else {
			// TODO Get first timestamp from trackpoints (ATM this sort is not used)
			VikTrack *trk = VIK_TRACK(value);
			st->name = trk->name;
			st->timestamp = 0;
		}
		st->data = value;
		gl = g_list_prepend ( gl, st );
	}
	gl = g_list_sort_with_data ( gl, sort_hash_compare, GINT_TO_POINTER(order) );
	return gl;
}
