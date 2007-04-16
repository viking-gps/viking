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

#include <gtk/gtk.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#include "curl_download.h"

int curl_download_uri ( const char *uri, FILE *f )
{
#ifdef HAVE_LIBCURL
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init ();
  if ( curl )
    {
      curl_easy_setopt ( curl, CURLOPT_URL, uri );
      curl_easy_setopt ( curl, CURLOPT_FILE, f );
      res = curl_easy_perform ( curl );
      curl_easy_cleanup ( curl );
    }
#endif
}

int curl_download_get_url ( const char *hostname, const char *uri, FILE *f )
{
  int ret;
  gchar *full = NULL;

  /* Compose the full url */
  full = g_strdup_printf ( "http://%s%s", hostname, uri );
  ret = curl_download_uri ( full, f );
  g_free ( full );
  full = NULL;

  return ret;
}
