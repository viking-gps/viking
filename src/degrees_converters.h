
#ifndef _VIKING_CONVERTER_H
#define _VIKING_CONVERTER_H

#include <glib.h>

gchar *convert_lat_dec_to_dmm(gdouble lat);
gchar *convert_lon_dec_to_dmm(gdouble lon);

gchar *convert_lat_dec_to_dms(gdouble lat);
gchar *convert_lon_dec_to_dms(gdouble lon);

gdouble convert_dms_to_dec(const gchar *dms);

#endif
