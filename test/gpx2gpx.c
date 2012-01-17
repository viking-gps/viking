#include <stdio.h>
#include <gpx.h>
#include <viklayer.h>

int main(int argc, char *argv[])
{
  g_type_init ();
  VikLayer *vl = vik_layer_create (VIK_LAYER_TRW, NULL, NULL, 0);
  VikTrwLayer *trw = VIK_TRW_LAYER (vl);
  a_gpx_read_file(trw, stdin);
  a_gpx_write_file(trw, stdout);
  // NB no layer_free functions directly visible anymore
  //  automatically called by layers_panel_finalize cleanup in full Viking program
  return 0;
}
