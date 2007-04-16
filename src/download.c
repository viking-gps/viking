/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "download.h"

#ifdef HAVE_LIBCURL
#include "curl_download.h"
#else
#include "http.h"
#endif

#ifdef WINDOWS

#include <io.h>
#define access(a,b) _access(a,b)
#define close(a) closesocket(a)

char *dirname ( char * dir )
{
  char *tmp = dir + strlen(dir) - 1;
  while ( tmp != dir && *tmp != '\\' )
    tmp--;
  *tmp = '\0';
  return dir;
}

#else

#include <unistd.h>
#include <sys/types.h>

/* dirname */
#include <libgen.h>

#endif 

static int download( const char *hostname, const char *uri, const char *fn, int sendhostname)
{
  FILE *f;
  int ret;

  /* Check file */
  if ( access ( fn, F_OK ) == 0 )
  {
    /* File exists: return */
    return -3;
  } else {
    if ( errno == ENOENT)
    {
      char *tmp = g_strdup ( fn );
#ifdef WINDOWS
      mkdir( dirname ( dirname ( tmp ) ) );
      g_free ( tmp ); tmp = g_strdup ( fn );
      mkdir( dirname ( tmp ) );
#else
      mkdir( dirname ( dirname ( tmp ) ), 0777 );
      g_free ( tmp ); tmp = g_strdup ( fn );
      mkdir( dirname ( tmp ), 0777 );
#endif
      g_free ( tmp );
    }
    if ( ! (f = fopen ( fn, "w+b" )) ) /* immediately open file so other threads won't -- prevents race condition */
      return -4;
  }

  /* Call the backend function */
#ifdef HAVE_LIBCURL
  ret = curl_download_get_url ( hostname, uri, f );
#else
  ret = http_download_get_url ( hostname, uri, f, 0, sendhostname );
#endif

  if (ret == -1 || ret == 1 || ret == -2)
  {
    fclose ( f );
    remove ( fn );
    return -1;
  }

  fclose ( f );
  return ret;
}

/* success = 0, -1 = couldn't connect, -2 HTTP error, -3 file exists, -4 couldn't write to file... */
/* uri: like "/uri.html?whatever" */
/* only reason for the "wrapper" is so we can do redirects. */
int a_http_download_get_url ( const char *hostname, const char *uri, const char *fn )
{
  return download ( hostname, uri, fn, 1 );
}

int a_http_download_get_url_nohostname ( const char *hostname, const char *uri, const char *fn )
{
  return download ( hostname, uri, fn, 0 );
}
