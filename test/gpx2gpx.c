#include <stdio.h>
#include <gpx.h>

int main(int argc, char *argv[])
{
  VikTrwLayer *trw = NULL;
  g_type_init ();
  trw = vik_trw_layer_new(0);
  a_gpx_read_file(trw, stdin);
  a_gpx_write_file(trw, stdout);
  vik_trw_layer_free (trw);
  return 0;
}
