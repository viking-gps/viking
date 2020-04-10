// Copyright: CC0
#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "geotag_exif.h"
#include "vikcoord.h"
 
int main(int argc, char *argv[])
{
  if ( argv[1] ) {
    struct LatLon ll = a_geotag_get_position ( argv[1] );
    g_printf ( "%.6f %.6f\n", ll.lat, ll.lon );
  }
 
  return 0;
}

