// Copyright: CC0
//
//run like:
// ./vik2vik < input.vik output.vik
//
#include <gtk/gtk.h>
#include <stdio.h>
#include "viklayer.h"
#include "viklayer_defaults.h"
#include "settings.h"
#include "preferences.h"
#include "globals.h"
#include "garminsymbols.h"
#include "gpspoint.h"
#include "file.h"
#include "modules.h"

int main(int argc, char *argv[])
{
  if ( argc != 2 )
    return argc;

  // Under GTK2, despite perhaps being undefined behaviour - it seemed to work without a $DISPLAY
  // But for GTK3 it doesn't work
#if GTK_CHECK_VERSION (3,0,0)
  gtk_init ( NULL, NULL );
#endif

  // Some stuff must be initialized as it gets auto used
  a_settings_init ();
  a_preferences_init ();
  a_vik_preferences_init ();
  a_layer_defaults_init ();
  modules_init();

  int result = 0;

  // Seems to work without an $DISPLAY
  // Also get lots of warnings about no actual drawing GCs available
  // but for file processing this seems to be good enough
  VikLoadType_t lt;
  VikAggregateLayer* agg = vik_aggregate_layer_new ();
  VikViewport* vp = vik_viewport_new ();

  lt = a_file_load_stream ( stdin, NULL, agg, vp, NULL, TRUE, FALSE, NULL, "NotUsedName" );
  if ( lt < LOAD_TYPE_VIK_FAILURE_NON_FATAL )
    result++;
  if ( !a_file_save(agg, vp, argv[1]) )
    result++;

  g_object_unref ( agg );

  vik_trwlayer_uninit ();
  a_layer_defaults_uninit ();
  a_preferences_uninit ();
  a_settings_uninit ();

  return result;
}
