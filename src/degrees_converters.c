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
#include <string.h>
#include "degrees_converters.h"

#include <stdio.h>  // printf for debug

#define DEGREE_SYMBOL "\302\260"	// UTF-8: 0xc2 0xb0

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

  /* Seconds */
  val_s = (tmp - val_m) * 60;

  /* Format */
  result = g_strdup_printf ( "%c%d" DEGREE_SYMBOL "%d'%.4f\"",
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
				const gchar *tmpptr = endptr;
				value = g_strtod((const gchar *)ptr, (gchar **)&endptr);
				// Detect when endptr hasn't changed (which may occur if no conversion took place)
				//  particularly if the last character is a ',' or there are multiple '.'s like '5.5.'
				if ( endptr == tmpptr )
					break;
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
      			default: break;
      		}
			}
		} while (ptr != NULL && endptr != NULL);
	}
	
	// Compute the result
	result = d + m/60 + s/3600;
	
	if (neg) result = - result;
	
	return result;
}

gboolean convert_txt_to_lat_lon(const char *txt, gdouble *lat, gdouble *lon)
{
	GMatchInfo *match_info;

	// N51° 52.238¿ E5° 50.293¿
	GRegex *regex = g_regex_new("^\\s*([NSns])\\s*(\\d+)\302\260\\s*(\\d+\\.\\d+)(.)", 0, 0, NULL);
	if (g_regex_match(regex, txt, 0, &match_info)) {
		gchar *min_txt = g_match_info_fetch(match_info, 4);
		printf("\"%s\" did match \"%s\". Last char has code 0x", txt, g_regex_get_pattern(regex));
		gchar *ptr = min_txt;
		while (*ptr++ != 0) {
			printf("%02x", *ptr);
		}
		printf("\n");
		gunichar unichar = g_utf8_get_char_validated(min_txt, -1);
		printf("Unicode code point is 0x%x\n", unichar);
		g_free(min_txt);
	} else {
		printf("\"%s\" did NOT match \"%s\"\n", txt, g_regex_get_pattern(regex));
	}
	g_match_info_free(match_info);
	g_regex_unref(regex);

	regex = g_regex_new("^\\s*([NSns])\\s*(\\d+)\302\260\\s*(\\d+\\.\\d+)['¿]\\s*([EWew])\\s*(\\d+)\302\260\\s*(\\d+\\.\\d+)['¿]\\s*$", 0, 0, NULL);
	g_assert(regex);
	
	if (g_regex_match(regex, txt, 0, &match_info)) {
		g_assert(g_match_info_get_match_count(match_info) == 7);	// whole matched text plus six matched substrings
		gchar *n_or_s  = g_match_info_fetch(match_info, 1);
		gchar *lat_deg = g_match_info_fetch(match_info, 2);
		gchar *lat_min = g_match_info_fetch(match_info, 3);
		gchar *e_or_w  = g_match_info_fetch(match_info, 4);
		gchar *lon_deg = g_match_info_fetch(match_info, 5);
		gchar *lon_min = g_match_info_fetch(match_info, 6);

		*lat = g_ascii_strtod(lat_deg, NULL) + g_ascii_strtod(lat_min, NULL) / 60.0;
		if ( (n_or_s[0] == 'S') || (n_or_s[0] == 's') ) *lat = - *lat;

		*lon = g_ascii_strtod(lon_deg, NULL) + g_ascii_strtod(lon_min, NULL) / 60.0;
		if ( (e_or_w[0] == 'W') || (e_or_w[0] == 'w') ) *lon = - *lon;

		g_free(n_or_s); 
		g_free(lat_deg);
		g_free(e_or_w); 
		g_free(lon_deg);
		g_free(lon_min);
		g_match_info_free(match_info);
		g_regex_unref(regex);
		return TRUE;
	}
	g_match_info_free(match_info);
	g_regex_unref(regex);
	return FALSE;
}
