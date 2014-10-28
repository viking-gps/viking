#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "geotag_exif.h"
#include "vikcoord.h"
 
int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2,36,0)
  g_type_init ();
#endif

  int answer = 0;
  if ( argv[1] ) {
    struct LatLon ll = { 51.179489, -1.826217 };
    VikCoord vc;
    vik_coord_load_from_latlon ( &vc, VIK_COORD_LATLON, &ll );
    answer = a_geotag_write_exif_gps ( argv[1], vc, 0.0, TRUE );
  }
 
  return answer;
}

