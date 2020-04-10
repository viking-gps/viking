#include <stdio.h>
#include "gpx.h"
#include "viklayer.h"
#include "viklayer_defaults.h"
#include "settings.h"
#include "preferences.h"
#include "globals.h"

int main(int argc, char *argv[])
{
  // Some stuff must be initialized as it gets auto used
  a_settings_init ();
  a_preferences_init ();
  a_vik_preferences_init ();
  a_layer_defaults_init ();

  VikLayer *vl = vik_layer_create (VIK_LAYER_TRW, NULL, FALSE);
  VikTrwLayer *trw = VIK_TRW_LAYER (vl);

  a_gpx_read_file(trw, stdin, NULL);
  a_gpx_write_file(trw, stdout, NULL, NULL);

  g_object_unref ( vl );

  vik_trwlayer_uninit ();

  a_layer_defaults_uninit ();
  a_preferences_uninit ();
  a_settings_uninit ();

  return 0;
}
