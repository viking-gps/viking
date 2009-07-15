/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <glib.h>
#include <string.h>

#define DEGREE_SYMBOL "\302\260"

/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
static gchar *convert_dec_to_ddd(gdouble dec, gchar pos_c, gchar neg_c)
{
  gchar sign_c = ' ';
  gdouble val_d;
  gchar *result = NULL;

  if ( dec > 0 )
    sign_c = pos_c;
  else if ( dec < 0 )
    sign_c = neg_c;
  else /* Nul value */
    sign_c = ' ';

  /* Degree */
  val_d = fabs(dec);

  /* Format */
  result = g_strdup_printf ( "%c%f" DEGREE_SYMBOL, sign_c, val_d );
  return result;
}

gchar *convert_lat_dec_to_ddd(gdouble lat)
{
  return convert_dec_to_ddd(lat, 'N', 'S');
}

gchar *convert_lon_dec_to_ddd(gdouble lon)
{
  return convert_dec_to_ddd(lon, 'E', 'W');
}

/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
static gchar *convert_dec_to_dmm(gdouble dec, gchar pos_c, gchar neg_c)
{
  gdouble tmp;
  gchar sign_c = ' ';
  gint val_d;
  gdouble val_m;
  gchar *result = NULL;

  if ( dec > 0 )
    sign_c = pos_c;
  else if ( dec < 0 )
    sign_c = neg_c;
  else /* Nul value */
    sign_c = ' ';

  /* Degree */
  tmp = fabs(dec);
  val_d = (gint)tmp;

  /* Minutes */
  val_m = (tmp - val_d) * 60;

  /* Format */
  result = g_strdup_printf ( "%c%d" DEGREE_SYMBOL "%f'",
                             sign_c, val_d, val_m );
  return result;
}

gchar *convert_lat_dec_to_dmm(gdouble lat)
{
  return convert_dec_to_dmm(lat, 'N', 'S');
}

gchar *convert_lon_dec_to_dmm(gdouble lon)
{
  return convert_dec_to_dmm(lon, 'E', 'W');
}

/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
static gchar *convert_dec_to_dms(gdouble dec, gchar pos_c, gchar neg_c)
{
  gdouble tmp;
  gchar sign_c = ' ';
  gint val_d, val_m;
  gdouble val_s;
  gchar *result = NULL;

  if ( dec > 0 )
    sign_c = pos_c;
  else if ( dec < 0 )
    sign_c = neg_c;
  else /* Nul value */
    sign_c = ' ';

  /* Degree */
  tmp = fabs(dec);
  val_d = (gint)tmp;

  /* Minutes */
  tmp = (tmp - val_d) * 60;
  val_m = (gint)tmp;

  /* Minutes */
  val_s = (tmp - val_m) * 60;

  /* Format */
  result = g_strdup_printf ( "%c%d" DEGREE_SYMBOL "%d'%f\"",
                             sign_c, val_d, val_m, val_s );
  return result;
}

gchar *convert_lat_dec_to_dms(gdouble lat)
{
  return convert_dec_to_dms(lat, 'N', 'S');
}

gchar *convert_lon_dec_to_dms(gdouble lon)
{
  return convert_dec_to_dms(lon, 'E', 'W');
}

gdouble convert_dms_to_dec(const gchar *dms)
{
	gdouble d = 0.0; /* Degree */
	gdouble m = 0.0; /* Minutes */
	gdouble s = 0.0; /* Seconds */
	gint neg = FALSE;
	gdouble result;
	
	if (dms != NULL) {
		int nbFloat = 0;
		const gchar *ptr, *endptr;

		// Compute the sign
		// It is negative if:
		// - the '-' sign occurs
		// - it is a west longitude or south latitude
		if (strpbrk (dms, "-wWsS") != NULL)
		    neg = TRUE;

		// Peek the différent components
		endptr = dms;
		do {
			gdouble value;
			ptr = strpbrk (endptr, "0123456789,.");
			if (ptr != NULL) {
				value = g_strtod(ptr, &endptr);
      			nbFloat++;
      		switch(nbFloat) {
      			case 1:
      				d = value;
      				break;
      			case 2:
      				m = value;
      				break;
      			case 3:
      				s = value;
      				break;
      		}
			}
		} while (ptr != NULL && endptr != NULL);
	}
	
	// Compute the result
	result = d + m/60 + s/3600;
	
	if (neg) result = - result;
	
	return result;
}
