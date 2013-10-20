/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#include <math.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "vikutils.h"
#include "globals.h"
#include "download.h"
#include "preferences.h"
#include "vikmapslayer.h"
#include "settings.h"
#include "util.h"

#define FMT_MAX_NUMBER_CODES 9

/**
 * vu_trackpoint_formatted_message:
 * @format_code:  String describing the message to generate
 * @trkpt:        The trackpoint for which the message is generated about
 * @trkpt_prev:   A trackpoint (presumed previous) for interpolating values with the other trackpoint (such as speed)
 * @trk:          The track in which the trackpoints reside
 *
 *  TODO: One day replace this cryptic format code with some kind of tokenizer parsing
 *    thus would make it more user friendly and maybe even GUI controlable.
 * However for now at least there is some semblance of user control
 */
gchar* vu_trackpoint_formatted_message ( gchar *format_code, VikTrackpoint *trkpt, VikTrackpoint *trkpt_prev, VikTrack *trk )
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

	gchar *speed_units_str = NULL;
	vik_units_speed_t speed_units = a_vik_get_units_speed ();
	switch (speed_units) {
	case VIK_UNITS_SPEED_MILES_PER_HOUR:
		speed_units_str = g_strdup ( _("mph") );
		break;
	case VIK_UNITS_SPEED_METRES_PER_SECOND:
		speed_units_str = g_strdup ( _("m/s") );
		break;
	case VIK_UNITS_SPEED_KNOTS:
		speed_units_str = g_strdup ( _("knots") );
		break;
	default:
		// VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
		speed_units_str = g_strdup ( _("km/h") );
		break;
	}

	gchar *separator = g_strdup ( " | " );

	for ( i = 0; i < len; i++ ) {
		switch ( g_ascii_toupper ( format_code[i] ) ) {
		case 'G': values[i] = g_strdup ( _("GPSD") ); break; // GPS Preamble
		case 'K': values[i] = g_strdup ( _("Trkpt") ); break; // Trkpt Preamble

		case 'S': {
			gdouble speed = 0.0;
			gchar *speedtype = NULL;
			if ( isnan(trkpt->speed) && trkpt_prev ) {
				if ( trkpt->has_timestamp && trkpt_prev->has_timestamp ) {
					if ( trkpt->timestamp == trkpt_prev->timestamp ) {

						// Work out from previous trackpoint location and time difference
						speed = vik_coord_diff(&(trkpt->coord), &(trkpt_prev->coord)) / ABS(trkpt->timestamp - trkpt_prev->timestamp);

						switch (speed_units) {
						case VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
							speed = VIK_MPS_TO_KPH(speed);
							break;
						case VIK_UNITS_SPEED_MILES_PER_HOUR:
							speed = VIK_MPS_TO_MPH(speed);
							break;
						case VIK_UNITS_SPEED_KNOTS:
							speed = VIK_MPS_TO_KNOTS(speed);
							break;
						default:
							// VIK_UNITS_SPEED_METRES_PER_SECOND:
							// Already in m/s so nothing to do
							break;
						}
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

			values[i] = g_strdup_printf ( _("%sSpeed%s %.1f%s"), separator, speedtype, speed, speed_units_str );
			g_free ( speedtype );
			break;
		}

		case 'A': {
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
			break;
		}

		case 'C': {
			gint heading = isnan(trkpt->course) ? 0 : (gint)round(trkpt->course);
			values[i] = g_strdup_printf ( _("%sCourse %03d\302\260" ), separator, heading );
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
			gchar tmp[64];
			tmp[0] = '\0';
			if ( trkpt->has_timestamp ) {
				// Compact date time format
				strftime (tmp, sizeof(tmp), "%x %X", localtime(&(trkpt->timestamp)));
			}
			else
				g_snprintf (tmp, sizeof(tmp), "--");
			values[i] = g_strdup_printf ( _("%sTime %s"), separator, tmp );
			break;
		}

		case 'M': {
			if ( trkpt_prev ) {
				if ( trkpt->has_timestamp && trkpt_prev->has_timestamp ) {
					time_t t_diff = trkpt->timestamp - trkpt_prev->timestamp;
					values[i] = g_strdup_printf ( _("%sTime diff %lds"), separator, t_diff );
				}
			}
			break;
		}

		case 'X': values[i] = g_strdup_printf ( _("%sNo. of Sats %d"), separator, trkpt->nsats ); break;

		case 'D': {
			if ( trk ) {
				// Distance from start (along the track)
				gdouble distd =	vik_track_get_length_to_trackpoint (trk, trkpt);
				gchar *dist_units_str = NULL;
				vik_units_distance_t dist_units = a_vik_get_units_distance ();
				// expect the difference between track points to be small hence use metres or yards
				switch (dist_units) {
				case VIK_UNITS_DISTANCE_MILES:
					dist_units_str = g_strdup ( _("miles") );
					distd = VIK_METERS_TO_MILES(distd);
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
		if ( values[i] != '\0' )
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
	DownloadMapOptions options = { FALSE, FALSE, NULL, 5, NULL, NULL, NULL };
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
#if GLIB_CHECK_VERSION (2, 32, 0)
		g_thread_try_new ( "latest_version_thread", (GThreadFunc)latest_version_thread, window, NULL );
#else
		g_thread_create ( (GThreadFunc)latest_version_thread, window, FALSE, NULL );
#endif
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
	if ( a_vik_very_first_run () ) {

		GtkWidget *win = gtk_window_new ( GTK_WINDOW_TOPLEVEL );

		if ( a_dialog_yes_or_no ( GTK_WINDOW(win),
		                          _("This appears to be Viking's very first run.\n\nDo you wish to enable automatic internet features?\n\nIndividual settings can be controlled in the Preferences."), NULL ) )
			auto_features = TRUE;
	}

	if ( auto_features ) {
		// Set Maps to autodownload
		// Ensure the default is true
		maps_layer_set_autodownload_default ( TRUE );

		// Simplistic repeat of preference settings
		//  Only the name & type are important for setting a preference via this 'external' way

		// Enable auto add map +
		// Enable IP lookup
		VikLayerParam pref_add_map[] = { { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, NULL, VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, NULL, NULL, NULL, }, };
		VikLayerParam pref_startup_method[] = { { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, NULL, VIK_LAYER_WIDGET_COMBOBOX, NULL, NULL, NULL, NULL, NULL, NULL}, };

		VikLayerParamData vlp_data;
		vlp_data.b = TRUE;
		a_preferences_run_setparam ( vlp_data, pref_add_map );

		vlp_data.u = VIK_STARTUP_METHOD_AUTO_LOCATION;
		a_preferences_run_setparam ( vlp_data, pref_startup_method );

		// Only on Windows make checking for the latest version on by default
		// For other systems it's expected a Package manager or similar controls the installation, so leave it off
#ifdef WINDOWS
		VikLayerParam pref_startup_version_check[] = { { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, NULL, VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, NULL, }, };
		vlp_data.b = TRUE;
		a_preferences_run_setparam ( vlp_data, pref_startup_version_check );
#endif

		// Ensure settings are saved for next time
		a_preferences_save_to_file ();
	}
}
