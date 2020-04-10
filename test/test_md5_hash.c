// Copyright: CC0
// Print md5 hash of the given string
#include <glib.h>
#include <glib/gstdio.h>
#include "md5_hash.h"

int main(int argc, char *argv[])
{
  if ( !argv[1] ) {
    g_printerr ( "Nothing specified\n" );
    return 1;
  }

  gchar *hash = md5_hash ( argv[1] );
  g_printf ( "%s\n", hash );
  g_free ( hash );

  return 0;
}

