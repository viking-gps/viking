// Copyright: CC0
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>
#include "viklayer.h"
#include "viklayer_defaults.h"
#include "settings.h"
#include "preferences.h"
#include "globals.h"
#include "geojson.h"
#include "gpx.h"

// Run as:
// ./json_osrm_to_gpx OSRMresult.txt outfile.gpx
//

int main( int argc, char *argv[] )
{
  if ( argc != 3 ) {
    g_printerr ( "No input and output files specified\n" );
    return 1;
  }

  // Some stuff must be initialized as it gets auto used
  a_settings_init ();
  a_preferences_init ();
  a_vik_preferences_init ();
  a_layer_defaults_init ();

  VikLayer *vl = vik_layer_create (VIK_LAYER_TRW, NULL, FALSE);
  VikTrwLayer *trw = VIK_TRW_LAYER (vl);

  gboolean success = a_geojson_read_file_OSRM ( trw, argv[1] );
  if ( success ) {
    FILE *ff = g_fopen ( argv[2], "w" );
    a_gpx_write_file ( trw, ff, NULL, NULL );
    fclose ( ff );
  }

  g_object_unref ( vl );

  vik_trwlayer_uninit ();
  a_layer_defaults_uninit ();
  a_preferences_uninit ();
  a_settings_uninit ();

  // Convert to exit status
  return success ? 0 : 1;
}
