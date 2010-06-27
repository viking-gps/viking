#include <stdio.h>
#include <stdlib.h>
#include "degrees_converters.h"

int main(int argc, char *argv[]) {
	int i;
	gdouble value;
	gchar *latDDD, *lonDDD;
	gchar *latDMM, *lonDMM;
	gchar *latDMS, *lonDMS;
	for (i=1 ; i < argc ; i++) {
		value = convert_dms_to_dec(argv[i]);
		latDDD = convert_lat_dec_to_ddd(value);
		lonDDD = convert_lon_dec_to_ddd(value);
		latDMM = convert_lat_dec_to_dmm(value);
		lonDMM = convert_lon_dec_to_dmm(value);
		latDMS = convert_lat_dec_to_dms(value);
		lonDMS = convert_lon_dec_to_dms(value);
		printf("'%s' -> %f %s %s %s %s %s %s\n", argv[i], value,
			   latDDD, lonDDD, latDMM, lonDMM, latDMS, lonDMS);
		free(latDMS); free(lonDMS);
		free(latDMM); free(lonDMM);
		free(latDDD); free(lonDDD);
	}
	return 0;
}
