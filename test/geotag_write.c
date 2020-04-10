// Copyright: CC0
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "geotag_exif.h"
#include "vikcoord.h"
 
int main(int argc, char *argv[])
{
  int answer = 0;
  if ( argv[1] ) {
    struct LatLon ll = { 51.179489, -1.826217 };
    VikCoord vc;
    vik_coord_load_from_latlon ( &vc, VIK_COORD_LATLON, &ll );
    // NB sqrt(-1) is delibrate to generate a NaN value
    //  (so no image direction EXIF tags are generated)
    answer = a_geotag_write_exif_gps ( argv[1], vc, 0.0, sqrt(-1), WP_IMAGE_DIRECTION_REF_TRUE, TRUE );
  }
 
  return answer;
}

