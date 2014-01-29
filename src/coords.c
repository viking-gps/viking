/*
coords.c
borrowed from:
http://acme.com/software/coords/
I (Evan Battaglia <viking@greentorch.org>) have only made some small changes such as
renaming functions and defining LatLon and UTM structs.
2004-02-10 -- I also added a function of my own -- a_coords_utm_diff() -- that I felt belonged in coords.c
2004-02-21 -- I also added a_coords_utm_equal().
2005-11-23 -- Added a_coords_dtostr() for lack of a better place.

*/
/* coords.h - include file for coords routines
**
** Copyright © 2001 by Jef Poskanzer <jef@acme.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "coords.h"
#ifdef HAVE_VIKING
#include "viking.h"
#include "globals.h"
#else
#define DEG2RAD(x) ((x)*(M_PI/180))
#define RAD2DEG(x) ((x)*(180/M_PI))
#endif
#include "degrees_converters.h"

/**
 * Convert a double to a string WITHOUT LOCALE.
 *
 * Following GPX specifications, decimal values are xsd:decimal
 * So, they must use the period separator, not the localized one.
 *
 * The returned value must be freed by g_free.
 */
char *a_coords_dtostr ( double d )
{
  gchar *buffer = g_malloc(G_ASCII_DTOSTR_BUF_SIZE*sizeof(gchar));
  g_ascii_dtostr (buffer, G_ASCII_DTOSTR_BUF_SIZE, (gdouble) d);
  return buffer;
}

#define PIOVER180 0.01745329252

#define K0 0.9996

/* WGS-84 */
#define EquatorialRadius 6378137
#define EccentricitySquared 0.00669438

static char coords_utm_letter( double latitude );

int a_coords_utm_equal( const struct UTM *utm1, const struct UTM *utm2 )
{
  return ( utm1->easting == utm2->easting && utm1->northing == utm2->northing && utm1->zone == utm2->zone );
}

double a_coords_utm_diff( const struct UTM *utm1, const struct UTM *utm2 )
{
  static struct LatLon tmp1, tmp2;
  if ( utm1->zone == utm2->zone ) {
    return sqrt ( pow ( utm1->easting - utm2->easting, 2 ) + pow ( utm1->northing - utm2->northing, 2 ) );
  } else {
    a_coords_utm_to_latlon ( utm1, &tmp1 );
    a_coords_utm_to_latlon ( utm2, &tmp2 );
    return a_coords_latlon_diff ( &tmp1, &tmp2 );
  }
}

double a_coords_latlon_diff ( const struct LatLon *ll1, const struct LatLon *ll2 )
{
  static struct LatLon tmp1, tmp2;
  gdouble tmp3;
  tmp1.lat = ll1->lat * PIOVER180;
  tmp1.lon = ll1->lon * PIOVER180;
  tmp2.lat = ll2->lat * PIOVER180;
  tmp2.lon = ll2->lon * PIOVER180;
  tmp3 = EquatorialRadius * acos(sin(tmp1.lat)*sin(tmp2.lat)+cos(tmp1.lat)*cos(tmp2.lat)*cos(tmp1.lon-tmp2.lon));
  // For very small differences we can sometimes get NaN returned
  return isnan(tmp3)?0:tmp3;
}

void a_coords_latlon_to_utm( const struct LatLon *latlon, struct UTM *utm )
    {
    double latitude;
    double longitude;
    double lat_rad, long_rad;
    double long_origin, long_origin_rad;
    double eccPrimeSquared;
    double N, T, C, A, M;
    int zone;
    double northing, easting;

    longitude = latlon->lon;
    latitude = latlon->lat;

    /* We want the longitude within -180..180. */
    if ( longitude < -180.0 )
	longitude += 360.0;
    if ( longitude > 180.0 )
	longitude -= 360.0;

    /* Now convert. */
    lat_rad = DEG2RAD(latitude);
    long_rad = DEG2RAD(longitude);
    zone = (int) ( ( longitude + 180 ) / 6 ) + 1;
    if ( latitude >= 56.0 && latitude < 64.0 &&
	 longitude >= 3.0 && longitude < 12.0 )
	zone = 32;
    /* Special zones for Svalbard. */
    if ( latitude >= 72.0 && latitude < 84.0 )
	{
	if      ( longitude >= 0.0  && longitude <  9.0 ) zone = 31;
	else if ( longitude >= 9.0  && longitude < 21.0 ) zone = 33;
	else if ( longitude >= 21.0 && longitude < 33.0 ) zone = 35;
	else if ( longitude >= 33.0 && longitude < 42.0 ) zone = 37;
	}
    long_origin = ( zone - 1 ) * 6 - 180 + 3;	/* +3 puts origin in middle of zone */
    long_origin_rad = DEG2RAD(long_origin);
    eccPrimeSquared = EccentricitySquared / ( 1.0 - EccentricitySquared );
    N = EquatorialRadius / sqrt( 1.0 - EccentricitySquared * sin( lat_rad ) * sin( lat_rad ) );
    T = tan( lat_rad ) * tan( lat_rad );
    C = eccPrimeSquared * cos( lat_rad ) * cos( lat_rad );
    A = cos( lat_rad ) * ( long_rad - long_origin_rad );
    M = EquatorialRadius * ( ( 1.0 - EccentricitySquared / 4 - 3 * EccentricitySquared * EccentricitySquared / 64 - 5 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 256 ) * lat_rad - ( 3 * EccentricitySquared / 8 + 3 * EccentricitySquared * EccentricitySquared / 32 + 45 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 1024 ) * sin( 2 * lat_rad ) + ( 15 * EccentricitySquared * EccentricitySquared / 256 + 45 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 1024 ) * sin( 4 * lat_rad ) - ( 35 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 3072 ) * sin( 6 * lat_rad ) );
    easting =
	K0 * N * ( A + ( 1 - T + C ) * A * A * A / 6 + ( 5 - 18 * T + T * T + 72 * C - 58 * eccPrimeSquared ) * A * A * A * A * A / 120 ) + 500000.0;
    northing =
	K0 * ( M + N * tan( lat_rad ) * ( A * A / 2 + ( 5 - T + 9 * C + 4 * C * C ) * A * A * A * A / 24 + ( 61 - 58 * T + T * T + 600 * C - 330 * eccPrimeSquared ) * A * A * A * A * A * A / 720 ) );
    if ( latitude < 0.0 )
	northing += 10000000.0;  /* 1e7 meter offset for southern hemisphere */

    utm->northing = northing;
    utm->easting = easting;
    utm->zone = zone;
    utm->letter = coords_utm_letter( latitude );

    /* All done. */
    }


static char coords_utm_letter( double latitude )
    {
    /* This routine determines the correct UTM letter designator for the
    ** given latitude.  It returns 'Z' if the latitude is outside the UTM
    ** limits of 84N to 80S.
    */
    if ( latitude <= 84.0 && latitude >= 72.0 ) return 'X';
    else if ( latitude < 72.0 && latitude >= 64.0 ) return 'W';
    else if ( latitude < 64.0 && latitude >= 56.0 ) return 'V';
    else if ( latitude < 56.0 && latitude >= 48.0 ) return 'U';
    else if ( latitude < 48.0 && latitude >= 40.0 ) return 'T';
    else if ( latitude < 40.0 && latitude >= 32.0 ) return 'S';
    else if ( latitude < 32.0 && latitude >= 24.0 ) return 'R';
    else if ( latitude < 24.0 && latitude >= 16.0 ) return 'Q';
    else if ( latitude < 16.0 && latitude >= 8.0 ) return 'P';
    else if ( latitude <  8.0 && latitude >= 0.0 ) return 'N';
    else if ( latitude <  0.0 && latitude >= -8.0 ) return 'M';
    else if ( latitude < -8.0 && latitude >= -16.0 ) return 'L';
    else if ( latitude < -16.0 && latitude >= -24.0 ) return 'K';
    else if ( latitude < -24.0 && latitude >= -32.0 ) return 'J';
    else if ( latitude < -32.0 && latitude >= -40.0 ) return 'H';
    else if ( latitude < -40.0 && latitude >= -48.0 ) return 'G';
    else if ( latitude < -48.0 && latitude >= -56.0 ) return 'F';
    else if ( latitude < -56.0 && latitude >= -64.0 ) return 'E';
    else if ( latitude < -64.0 && latitude >= -72.0 ) return 'D';
    else if ( latitude < -72.0 && latitude >= -80.0 ) return 'C';
    else return 'Z';
    }



void a_coords_utm_to_latlon( const struct UTM *utm, struct LatLon *latlon )
    {
    double northing, easting;
    int zone;
    char letter[100];
    double x, y;
    double eccPrimeSquared;
    double e1;
    double N1, T1, C1, R1, D, M;
    double long_origin;
    double mu, phi1_rad;
    int northernHemisphere;	/* 1 for northern hemisphere, 0 for southern */
    double latitude, longitude;

    northing = utm->northing;
    easting = utm->easting;
    zone = utm->zone;
    letter[0] = utm->letter;

    /* Now convert. */
    x = easting - 500000.0;	/* remove 500000 meter offset */
    y = northing;
    if ( ( *letter - 'N' ) >= 0 )
	northernHemisphere = 1;	/* northern hemisphere */
    else
	{
	northernHemisphere = 0;	/* southern hemisphere */
	y -= 10000000.0;	/* remove 1e7 meter offset */
	}
    long_origin = ( zone - 1 ) * 6 - 180 + 3;	/* +3 puts origin in middle of zone */
    eccPrimeSquared = EccentricitySquared / ( 1.0 - EccentricitySquared );
    e1 = ( 1.0 - sqrt( 1.0 - EccentricitySquared ) ) / ( 1.0 + sqrt( 1.0 - EccentricitySquared ) );
    M = y / K0;
    mu = M / ( EquatorialRadius * ( 1.0 - EccentricitySquared / 4 - 3 * EccentricitySquared * EccentricitySquared / 64 - 5 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 256 ) );
    phi1_rad = mu + ( 3 * e1 / 2 - 27 * e1 * e1 * e1 / 32 )* sin( 2 * mu ) + ( 21 * e1 * e1 / 16 - 55 * e1 * e1 * e1 * e1 / 32 ) * sin( 4 * mu ) + ( 151 * e1 * e1 * e1 / 96 ) * sin( 6 *mu );
    N1 = EquatorialRadius / sqrt( 1.0 - EccentricitySquared * sin( phi1_rad ) * sin( phi1_rad ) );
    T1 = tan( phi1_rad ) * tan( phi1_rad );
    C1 = eccPrimeSquared * cos( phi1_rad ) * cos( phi1_rad );
    R1 = EquatorialRadius * ( 1.0 - EccentricitySquared ) / pow( 1.0 - EccentricitySquared * sin( phi1_rad ) * sin( phi1_rad ), 1.5 );
    D = x / ( N1 * K0 );
    latitude = phi1_rad - ( N1 * tan( phi1_rad ) / R1 ) * ( D * D / 2 -( 5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * eccPrimeSquared ) * D * D * D * D / 24 + ( 61 + 90 * T1 + 298 * C1 + 45 * T1 * T1 - 252 * eccPrimeSquared - 3 * C1 * C1 ) * D * D * D * D * D * D / 720 );
    latitude = RAD2DEG(latitude);
    longitude = ( D - ( 1 + 2 * T1 + C1 ) * D * D * D / 6 + ( 5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * eccPrimeSquared + 24 * T1 * T1 ) * D * D * D * D * D / 120 ) / cos( phi1_rad );
    longitude = long_origin + RAD2DEG(longitude);

    /* Show results. */

    latlon->lat = latitude;
    latlon->lon = longitude;

    }

void a_coords_latlon_to_string ( const struct LatLon *latlon,
				 gchar **lat,
				 gchar **lon )
{
  g_return_if_fail ( latlon != NULL );
#ifdef HAVE_VIKING
  vik_degree_format_t format = a_vik_get_degree_format ();

  switch (format) {
  case VIK_DEGREE_FORMAT_DDD:
    *lat = convert_lat_dec_to_ddd ( latlon->lat );
    *lon = convert_lon_dec_to_ddd ( latlon->lon );
    break;
  case VIK_DEGREE_FORMAT_DMM:
    *lat = convert_lat_dec_to_dmm ( latlon->lat );
    *lon = convert_lon_dec_to_dmm ( latlon->lon );
    break;
  case VIK_DEGREE_FORMAT_DMS:
    *lat = convert_lat_dec_to_dms ( latlon->lat );
    *lon = convert_lon_dec_to_dms ( latlon->lon );
    break;
  default:
    g_critical("Houston, we've had a problem. format=%d", format);
  }
#else
  *lat = convert_lat_dec_to_ddd ( latlon->lat );
  *lon = convert_lon_dec_to_ddd ( latlon->lon );
#endif
}
