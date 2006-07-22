#include <math.h>
#include <glib.h>

#include <stdio.h>

gchar *convert_lat_dec_to_dms(gdouble lat)
{
  gdouble tmp;
  gchar lat_c;
  gint lat_d, lat_m;
  gdouble lat_s;
  gchar *result = NULL;

  /* North ? South ? */
  if ( lat > 0 )
    lat_c = 'N';
  else
    lat_c = 'S';

  /* Degree */
  tmp = fabs(lat);
  lat_d = (gint)tmp;

  /* Minutes */
  tmp = (tmp - lat_d) * 60;
  lat_m = (gint)tmp;

  /* Minutes */
  lat_s = (tmp - lat_m) * 60;

  /* Format */
  /* todo : replace "deg" substring by "°" as UTF-8 */
  result = g_strdup_printf ( "%c %d° %d' %f",
                             lat_c, lat_d, lat_m, lat_s );
  return result;
}

gchar *convert_lon_dec_to_dms(gdouble lon)
{
  gdouble tmp;
  gchar lon_c;
  gint lon_d, lon_m;
  gdouble lon_s;
  gchar *result = NULL;

  /* North ? South ? */
  if ( lon > 0 )
    lon_c = 'E';
  else
    lon_c = 'W';

  /* Degree */
  tmp = fabs(lon);
  lon_d = (gint)tmp;

  /* Minutes */
  tmp = (tmp - lon_d) * 60;
  lon_m = (gint)tmp;

  /* Minutes */
  lon_s = (tmp - lon_m) * 60;

  /* Format */
  /* todo : replace "deg" substring by "°" as UTF-8 */
  result = g_strdup_printf ( "%c %d° %d' %f\"",
                             lon_c, lon_d, lon_m, lon_s );
  return result;
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
		gchar *ptr, *endptr;

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
