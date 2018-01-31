// Copyright: CC0
// Compare timegm() with util_timegm()
//  NB Need to force util_timegm() to use our fallback version
//  Otherwise it will be using timegm()
#include <glib.h>
#include <glib/gstdio.h>
#include <time.h>
#include "util.h"

// run like - 'for now':
//  ./test_time $(date +"%Y:%m:%d-%H:%M:%S")
// Or manually substitute interesting dates as appropriate
//  e.g.  '2400:03:01-10:11:12'
int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2,36,0)
  g_type_init ();
#endif

  if ( !argv[1] ) {
    g_printerr ( "Nothing specified\n" );
    return 1;
  }

  struct tm Time;
  Time.tm_wday = 0;
  Time.tm_yday = 0;
  Time.tm_isdst = 0; // there is no DST in UTC

  sscanf(argv[1], "%d:%d:%d-%d:%d:%d",
                  &Time.tm_year, &Time.tm_mon,
                  &Time.tm_mday, &Time.tm_hour,
                  &Time.tm_min, &Time.tm_sec);

  Time.tm_year -= 1900;
  Time.tm_mon  -= 1;

  time_t thetime = util_timegm ( &Time );

  if ( thetime != timegm ( &Time ) ) {
    g_printf ("%s - %lu - %lu\n", argv[1], thetime, timegm(&Time));
    g_printf ("diff = %ld\n", (thetime - timegm(&Time)));
    return 1;
  }

  return 0;
}
