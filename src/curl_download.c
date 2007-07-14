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

#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>

#include "file.h"
#include "curl_download.h"

static gchar *get_cookie_file(gboolean init)
{
  static gchar *cookie_file = NULL;
  static GMutex *mutex = NULL;

  if (init) { /* to make sure  it's thread safe */
    mutex = g_mutex_new();
    static gchar *cookie_fn = "cookies.txt";
    const gchar *viking_dir = a_get_viking_dir();
    cookie_file = g_strdup_printf("%s/%s", viking_dir, cookie_fn);
    unlink(cookie_file);
    return NULL;
  }

  g_assert(cookie_file != NULL);

  g_mutex_lock(mutex);
  if (access(cookie_file, F_OK)) {  /* file not there */
    FILE * out_file = fopen("/dev/null", "w");
    CURLcode res;
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://maps.google.com/"); /* google.com sets "PREF" cookie */
    curl_easy_setopt ( curl, CURLOPT_FILE, out_file );
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      g_warning("%s() Curl perform failed: %s\n", __PRETTY_FUNCTION__,
          curl_easy_strerror(res));
      unlink(cookie_file);
    }
    curl_easy_cleanup(curl);
  }
  g_mutex_unlock(mutex);

  return(cookie_file);
}

/* This should to be called from main() to make sure thread safe */
void curl_download_init()
{
  curl_global_init(CURL_GLOBAL_ALL);
  get_cookie_file(TRUE);
}

int curl_download_uri ( const char *uri, FILE *f, DownloadOptions *options )
{
  CURL *curl;
  CURLcode res = CURLE_FAILED_INIT;
  const gchar *cookie_file;

  curl = curl_easy_init ();
  if ( curl )
    {
      curl_easy_setopt ( curl, CURLOPT_URL, uri );
      curl_easy_setopt ( curl, CURLOPT_FILE, f );
      if (options != NULL && options->referer != NULL)
        curl_easy_setopt ( curl, CURLOPT_REFERER, options->referer);
      curl_easy_setopt ( curl, CURLOPT_USERAGENT, "viking/" VERSION " libcurl/7.15.4" );
      if (cookie_file = get_cookie_file(FALSE))
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
      res = curl_easy_perform ( curl );
      curl_easy_cleanup ( curl );
    }
  return(res);
}

int curl_download_get_url ( const char *hostname, const char *uri, FILE *f, DownloadOptions *options )
{
  int ret;
  gchar *full = NULL;

  /* Compose the full url */
  full = g_strdup_printf ( "http://%s%s", hostname, uri );
  ret = curl_download_uri ( full, f, options );
  g_free ( full );
  full = NULL;

  return (ret ? -2 : 0);   /* -2 HTTP error */
}
#endif /* HAVE_LIB_CURL */
