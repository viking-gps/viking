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
#include <math.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "vikutils.h"

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
			if ( !isnan(trkpt->speed) && trkpt_prev ) {
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
