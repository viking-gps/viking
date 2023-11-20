// Copyright: CC0
//
// Test program to check reading of files via the a_file_load() method
// NB loading of .vik files are tested more thoroughly via other test programs

#include <gtk/gtk.h>
#include <stdio.h>
#include "viklayer.h"
#include "viklayer_defaults.h"
#include "settings.h"
#include "preferences.h"
#include "download.h"
#include "globals.h"
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
  a_download_init();
  modules_init();

  modules_post_init();

  // Seems to work without an $DISPLAY
  // Also get lots of warnings about no actual drawing GCs available
  // but for file processing this seems to be good enough
  VikAggregateLayer* agg = vik_aggregate_layer_new ();
  VikViewport* vp = vik_viewport_new ();

  VikLoadType_t lt = a_file_load ( agg, vp, NULL, argv[1], TRUE, FALSE, NULL );

  g_object_unref ( agg );

  modules_uninit();
  a_download_uninit();
  a_layer_defaults_uninit ();
  a_vik_preferences_uninit ();
  a_preferences_uninit ();
  a_settings_uninit ();

  // Was it some kind of 'success'?
  if ( lt == LOAD_TYPE_OTHER_SUCCESS ||
       lt == LOAD_TYPE_VIK_SUCCESS ||
       lt == LOAD_TYPE_OTHER_FAILURE_NON_FATAL ||
       lt == LOAD_TYPE_OTHER_SUCCESS )
    return 0;

  // Otherwise a failure and so return a failing code
  if ( lt == LOAD_TYPE_READ_FAILURE )
    return 42;

  return lt;
}
