/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009-2010, Jocelyn Jaubert <jocelyn.jaubert@gmail.com>
 * Copyright (C) 2010, Sven Wegener <sven.wegener@stealer.net>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

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

gchar *curl_download_user_agent = NULL;

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

static size_t curl_get_etag_func(char *ptr, size_t size, size_t nmemb, void *stream)
{
#define ETAG_KEYWORD "ETag: "
#define ETAG_LEN (sizeof(ETAG_KEYWORD)-1)
  size_t len = size*nmemb;
  char *str = g_strstr_len((const char*)ptr, len, ETAG_KEYWORD);
  if (str) {
    char *etag_str = str + ETAG_LEN;
    char *end_str = g_strstr_len(etag_str, len - ETAG_LEN, "\r\n");
    if (etag_str && end_str) {
      stream = (void*) g_strndup(etag_str, end_str - etag_str);
      g_debug("%s: ETAG found: %s", __FUNCTION__, (gchar*)stream);
    }
  }
  return nmemb;
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

/* This should to be called from main() to make sure thread safe */
void curl_download_uninit()
{
  curl_global_cleanup();
}

int curl_download_uri ( const char *uri, FILE *f, DownloadMapOptions *options, DownloadFileOptions *file_options, void *handle )
{
  CURL *curl;
  struct curl_slist *curl_send_headers = NULL;
  CURLcode res = CURLE_FAILED_INIT;
  const gchar *cookie_file;

  g_debug("%s: uri=%s", __PRETTY_FUNCTION__, uri);

  curl = handle ? handle : curl_easy_init ();
  if ( !curl ) {
    return DOWNLOAD_ERROR;
  }
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
    if (file_options != NULL) {
      if(options->check_file_server_time && file_options->time_condition != 0) {
        /* if file exists, check against server if file is recent enough */
        curl_easy_setopt ( curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
        curl_easy_setopt ( curl, CURLOPT_TIMEVALUE, file_options->time_condition);
      }
      if (options->use_etag) {
        if (file_options->etag != NULL) {
          /* add an header on the HTTP request */
          char str[60];
          g_snprintf(str, 60, "If-None-Match: %s", file_options->etag);
          curl_send_headers = curl_slist_append(curl_send_headers, str);
          curl_easy_setopt ( curl, CURLOPT_HTTPHEADER , curl_send_headers);
        }
        /* store the new etag from the server in an option value */
        curl_easy_setopt ( curl, CURLOPT_WRITEHEADER, &(file_options->new_etag));
        curl_easy_setopt ( curl, CURLOPT_HEADERFUNCTION, curl_get_etag_func);
      }
    }
  }
  curl_easy_setopt ( curl, CURLOPT_USERAGENT, curl_download_user_agent );
  if ((cookie_file = get_cookie_file(FALSE)) != NULL)
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
  res = curl_easy_perform ( curl );

  if (res == 0) {
    glong response;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    if (response == 304) {         // 304 = Not Modified
      res = DOWNLOAD_NO_NEWER_FILE;
    } else if (response == 200 ||  // http: 200 = Ok
               response == 226) {  // ftp:  226 = sucess
      gdouble size;
      /* verify if curl sends us any data - this is a workaround on using CURLOPT_TIMECONDITION 
         when the server has a (incorrect) time earlier than the time on the file we already have */
      curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size);
      if (size == 0)
        res = DOWNLOAD_ERROR;
      else
        res = DOWNLOAD_NO_ERROR;
    } else {
      g_warning("%s: http response: %ld for uri %s\n", __FUNCTION__, response, uri);
      res = DOWNLOAD_ERROR;
    }
  } else {
    res = DOWNLOAD_ERROR;
  }
  if (!handle)
     curl_easy_cleanup ( curl );
  if (curl_send_headers) {
    curl_slist_free_all(curl_send_headers);
    curl_send_headers = NULL;
    curl_easy_setopt ( curl, CURLOPT_HTTPHEADER , NULL);
  }
  return res;
}

int curl_download_get_url ( const char *hostname, const char *uri, FILE *f, DownloadMapOptions *options, gboolean ftp, DownloadFileOptions *file_options, void *handle )
{
  int ret;
  gchar *full = NULL;

  /* Compose the full url */
  full = g_strdup_printf ( "%s://%s%s", (ftp?"ftp":"http"), hostname, uri );
  ret = curl_download_uri ( full, f, options, file_options, handle );
  g_free ( full );
  full = NULL;

  return ret;
}

void * curl_download_handle_init ()
{
  return curl_easy_init();
}

void curl_download_handle_cleanup ( void *handle )
{
  curl_easy_cleanup(handle);
}
