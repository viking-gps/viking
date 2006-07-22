#include "degrees_converters.h"

int main(int argc, char *argv[]) {
	int i;
	gdouble value;
	gchar *latDMS, *lonDMS;
	for (i=1 ; i < argc ; i++) {
		value = convert_dms_to_dec(argv[i]);
		latDMS = convert_lat_dec_to_dms(value);
		lonDMS = convert_lon_dec_to_dms(value);
		printf("'%s' -> %f %s %s\n", argv[i], value,
			   latDMS, lonDMS);
		free(latDMS); 
		free(lonDMS); 
	}
}