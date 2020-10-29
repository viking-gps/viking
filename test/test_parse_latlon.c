// Copyright: CC0
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include "coords.h"
#include "clipboard.h"

int main( int argc, char *argv[] )
{
#if ! GLIB_CHECK_VERSION (2, 36, 0)
  g_type_init();
#endif

  if ( !argv[1] ) {
    g_printerr ( "No text specified\n" );
    return 1;
  }

  struct LatLon coord;
  if ( clip_parse_latlon(argv[1], &coord) ) {
    // Ensure output uses decimal point for decimal separator
    (void)setlocale ( LC_ALL, "C" );
    printf ( "%0.5f %0.5f\n", coord.lat, coord.lon );
  } else
    return 1;

  return 0;
}
