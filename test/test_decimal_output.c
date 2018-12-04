// Copyright: CC0
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>
#include "coords.h"

void out ( double dd )
{
  char* str = a_coords_dtostr ( dd );
  printf ( "%s\n", str );
  free ( str );
}

int main( int argc, char *argv[] )
{
#if ! GLIB_CHECK_VERSION (2, 36, 0)
  g_type_init();
#endif

  if ( !argv[1] ) {
    g_printerr ( "No number specified\n" );
    return 1;
  }

  double dd = strtod ( argv[1], NULL );
  out ( dd );
  return 0;
}
