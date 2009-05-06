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

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include <curl/curl.h>

#include "background.h"
#include "file.h"
#include "globals.h"
#include "curl_download.h"

gchar *curl_download_user_agent;

/*
 * Even if writing to FILE* is supported by libcurl by default,
 * it seems that it is non-portable (win32 DLL specific).
 *
 * So, we provide our own trivial CURLOPT_WRITEFUNCTION.
 */
static size_t curl_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  return fwrite(ptr, size, nmemb, stream);
}

static int curl_progress_func(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
  return a_background_testcancel(NULL);
}

static gchar *get_cookie_file(gboolean init)
{
  static gchar *cookie_file = NULL;
  static GMutex *mutex = NULL;

  if (init) { /* to make sure  it's thread safe */
    mutex = g_mutex_new();
    static gchar *cookie_fn = "cookies.txt";
    const gchar *viking_dir = a_get_viking_dir();
    cookie_file = g_build_filename(viking_dir, cookie_fn, NULL);
    g_unlink(cookie_file);
    return NULL;
  }

  g_assert(cookie_file != NULL);

  g_mutex_lock(mutex);
  if (g_file_test(cookie_file, G_FILE_TEST_EXISTS) == FALSE) {  /* file not there */
    gchar * name_tmp = NULL;
    FILE * out_file = tmpfile();
    if (out_file == NULL) {
      // Something wrong with previous call (unsuported?)
      name_tmp = g_strdup_printf("%s.tmp", cookie_file);
      out_file = g_fopen(name_tmp, "w+b");
    }
    CURLcode res;
    CURL *curl = curl_easy_init();
    if (vik_verbose)
      curl_easy_setopt ( curl, CURLOPT_VERBOSE, 1 );
    curl_easy_setopt(curl, CURLOPT_URL, "http://maps.google.com/"); /* google.com sets "PREF" cookie */
    curl_easy_setopt ( curl, CURLOPT_WRITEDATA, out_file );
    curl_easy_setopt ( curl, CURLOPT_WRITEFUNCTION, curl_write_func);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      g_warning(_("%s() Curl perform failed: %s"), __PRETTY_FUNCTION__,
          curl_easy_strerror(res));
      g_unlink(cookie_file);
    }
    curl_easy_cleanup(curl);
    fclose(out_file);
    out_file = NULL;
    if (name_tmp != NULL) {
      g_remove(name_tmp);
      g_free(name_tmp);
      name_tmp = NULL;
    }
  }
  g_mutex_unlock(mutex);

  return(cookie_file);
}

/* This should to be called from main() to make sure thread safe */
void curl_download_init()
{
  curl_global_init(CURL_GLOBAL_ALL);
  get_cookie_file(TRUE);
  curl_download_user_agent = g_strdup_printf ("%s/%s %s", PACKAGE, VERSION, curl_version());
}

int curl_download_uri ( const char *uri, FILE *f, DownloadOptions *options )
{
  CURL *curl;
  CURLcode res = CURLE_FAILED_INIT;
  const gchar *cookie_file;

  g_debug("%s: uri=%s", __PRETTY_FUNCTION__, uri);

  curl = curl_easy_init ();
  if ( curl )
    {
      if (vik_verbose)
        curl_easy_setopt ( curl, CURLOPT_VERBOSE, 1 );
      curl_easy_setopt ( curl, CURLOPT_URL, uri );
      curl_easy_setopt ( curl, CURLOPT_WRITEDATA, f );
      curl_easy_setopt ( curl, CURLOPT_WRITEFUNCTION, curl_write_func);
      curl_easy_setopt ( curl, CURLOPT_NOPROGRESS, 0 );
      curl_easy_setopt ( curl, CURLOPT_PROGRESSDATA, NULL );
      curl_easy_setopt ( curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
      if (options != NULL) {
        if(options->referer != NULL)
          curl_easy_setopt ( curl, CURLOPT_REFERER, options->referer);
        if(options->follow_location != 0) {
          curl_easy_setopt ( curl, CURLOPT_FOLLOWLOCATION, 1);
          curl_easy_setopt ( curl, CURLOPT_MAXREDIRS, options->follow_location);
        }
      }
      curl_easy_setopt ( curl, CURLOPT_USERAGENT, curl_download_user_agent );
      if ((cookie_file = get_cookie_file(FALSE)) != NULL)
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
      res = curl_easy_perform ( curl );
      curl_easy_cleanup ( curl );
    }
  return(res);
}

int curl_download_get_url ( const char *hostname, const char *uri, FILE *f, DownloadOptions *options, gboolean ftp )
{
  int ret;
  gchar *full = NULL;

  /* Compose the full url */
  full = g_strdup_printf ( "%s://%s%s", (ftp?"ftp":"http"), hostname, uri );
  ret = curl_download_uri ( full, f, options );
  g_free ( full );
  full = NULL;

  return (ret ? -2 : 0);   /* -2 HTTP error */
}
