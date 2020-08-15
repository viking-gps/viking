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
#include <string.h>
#include <stdlib.h>

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
#include "dir.h"
#include "file.h"
#include "globals.h"
#include "curl_download.h"
#include "settings.h"

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

static size_t curl_get_etag_func(void *ptr, size_t size, size_t nmemb, void *stream)
{
#define ETAG_KEYWORD "ETag: "
#define ETAG_LEN (sizeof(ETAG_KEYWORD)-1)
  CurlDownloadOptions *cdo = (CurlDownloadOptions*)stream;
  size_t len = size*nmemb;
  char *str = g_strstr_len((const char*)ptr, len, ETAG_KEYWORD);
  if (str) {
    char *etag_str = str + ETAG_LEN;
    char *end_str = g_strstr_len(etag_str, len - ETAG_LEN, "\r\n");
    if (etag_str && end_str) {
      cdo->new_etag = g_strndup(etag_str, end_str - etag_str);
      g_debug("%s: ETAG found: %s", __FUNCTION__, cdo->new_etag);
    }
  }
  return nmemb;
}

static int curl_progress_func(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
  return a_background_testcancel(NULL);
}

static gint curl_ssl_verifypeer = 1; // https://curl.haxx.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html
static gchar* curl_cainfo = NULL;    // https://curl.haxx.se/libcurl/c/CURLOPT_CAINFO.html

/* This should to be called from main() to make sure thread safe */
void curl_download_init()
{
  curl_global_init(CURL_GLOBAL_ALL);
  curl_download_user_agent = g_strdup_printf ("%s/%s %s", PACKAGE, VERSION, curl_version());

#ifdef CURL_NO_SSL_VERIFYPEER
  curl_ssl_verifypeer = 0;
#endif
  gboolean tmp;
  if ( a_settings_get_boolean ( "curl_ssl_verifypeer", &tmp ) )
    curl_ssl_verifypeer = tmp;
  gchar *str = NULL;
  if ( a_settings_get_string ( "curl_cainfo", &str ) ) {
    curl_cainfo = g_strdup ( str );
    g_free ( str );
  }
}

/* This should to be called from main() to make sure thread safe */
void curl_download_uninit()
{
  curl_global_cleanup();
  g_free ( curl_cainfo );
}

/**
 *
 * Common curl options
 */
static void common_opts ( CURL *curl, const char *uri, DownloadFileOptions *options )
{
  g_debug ( "%s: uri=%s", __FUNCTION__, uri );
  if ( vik_verbose )
    curl_easy_setopt ( curl, CURLOPT_VERBOSE, 1 );
  curl_easy_setopt ( curl, CURLOPT_NOSIGNAL, 1 ); // Yep, we're a multi-threaded program so don't let signals mess it up!
  if ( options != NULL ) {
    if ( options->user_pass != NULL ) {
      curl_easy_setopt ( curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY );
      curl_easy_setopt ( curl, CURLOPT_USERPWD, options->user_pass );
    }
    if ( options->referer != NULL )
      curl_easy_setopt ( curl, CURLOPT_REFERER, options->referer );
    if ( options->follow_location != 0 ) {
      curl_easy_setopt ( curl, CURLOPT_FOLLOWLOCATION, 1 );
      curl_easy_setopt ( curl, CURLOPT_MAXREDIRS, options->follow_location );
    }
  }
  curl_easy_setopt ( curl, CURLOPT_URL, uri );
  curl_easy_setopt ( curl, CURLOPT_USERAGENT, curl_download_user_agent );
  // Allow download to be aborted (if called in a thread)
  curl_easy_setopt ( curl, CURLOPT_NOPROGRESS, 0 );
  curl_easy_setopt ( curl, CURLOPT_PROGRESSDATA, NULL );
  curl_easy_setopt ( curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
}

/**
 *
 */
CURL_download_t curl_download_uri ( const char *uri, FILE *f, DownloadFileOptions *options, CurlDownloadOptions *cdo, void *handle )
{
  CURL *curl;
  struct curl_slist *curl_send_headers = NULL;
  CURLcode res = CURLE_FAILED_INIT;

  curl = handle ? handle : curl_easy_init ();
  if ( !curl ) {
    return CURL_DOWNLOAD_ERROR;
  }
  common_opts ( curl, uri, options );
  curl_easy_setopt ( curl, CURLOPT_WRITEDATA, f );
  curl_easy_setopt ( curl, CURLOPT_WRITEFUNCTION, curl_write_func);
  if (options != NULL) {
    if (cdo != NULL) {
      if(options->check_file_server_time && cdo->time_condition != 0) {
        /* if file exists, check against server if file is recent enough */
        curl_easy_setopt ( curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
        curl_easy_setopt ( curl, CURLOPT_TIMEVALUE, cdo->time_condition);
      }
      if (options->use_etag) {
        if (cdo->etag != NULL) {
          /* add an header on the HTTP request */
          char str[60];
          g_snprintf(str, 60, "If-None-Match: %s", cdo->etag);
          curl_send_headers = curl_slist_append(curl_send_headers, str);
        }
        /* store the new etag from the server in an option value */
        curl_easy_setopt ( curl, CURLOPT_WRITEHEADER, cdo);
        curl_easy_setopt ( curl, CURLOPT_HEADERFUNCTION, curl_get_etag_func);
      }
    }
    if ( options->custom_http_headers) {
      gchar **headers = g_strsplit ( options->custom_http_headers, "\n", -1 );
      for (int ii = 0; ii < g_strv_length(headers); ii++)
          curl_send_headers = curl_slist_append ( curl_send_headers, headers[ii] );
      g_strfreev ( headers );
    }
  }
  curl_easy_setopt ( curl, CURLOPT_USERAGENT, curl_download_user_agent );

  curl_easy_setopt ( curl, CURLOPT_SSL_VERIFYPEER, curl_ssl_verifypeer );
  if ( curl_cainfo )
     curl_easy_setopt ( curl, CURLOPT_CAINFO, curl_cainfo );

  if ( curl_send_headers )
    curl_easy_setopt ( curl, CURLOPT_HTTPHEADER , curl_send_headers );

  res = curl_easy_perform ( curl );

  if (res == CURLE_OK) {
    glong response;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    if (response == 304) {         // 304 = Not Modified
      res = CURL_DOWNLOAD_NO_NEWER_FILE;
    } else if (response == 200 ||  // http: 200 = Ok
               response == 226) {  // ftp:  226 = sucess
      gdouble size;
      /* verify if curl sends us any data - this is a workaround on using CURLOPT_TIMECONDITION 
         when the server has a (incorrect) time earlier than the time on the file we already have */
      curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size);
      if (size == 0)
        res = CURL_DOWNLOAD_ERROR;
      else
        res = CURL_DOWNLOAD_NO_ERROR;
    } else {
      g_warning("%s: http response: %ld for uri %s", __FUNCTION__, response, uri);
      res = CURL_DOWNLOAD_ERROR;
    }
  } else {
    g_warning ( "%s: curl error: %d for uri %s", __FUNCTION__, res, uri );
    res = CURL_DOWNLOAD_ERROR;
  }
  if (curl_send_headers) {
    curl_slist_free_all(curl_send_headers);
    curl_send_headers = NULL;
    curl_easy_setopt ( curl, CURLOPT_HTTPHEADER , NULL);
  }
  if (!handle)
     curl_easy_cleanup ( curl );
  return res;
}

/**
 * curl_download_get_url:
 *  Either hostname and/or uri should be defined
 *
 */
CURL_download_t curl_download_get_url ( const char *hostname, const char *uri, FILE *f, DownloadFileOptions *options, gboolean ftp, CurlDownloadOptions *cdo, void *handle )
{
  gchar *full = NULL;

  if ( hostname && strstr ( hostname, "://" ) != NULL ) {
    if ( uri && strlen ( uri ) > 1 )
      // Simply append them together
      full = g_strdup_printf ( "%s%s", hostname, uri );
    else
      /* Already full url */
      full = (gchar *) hostname;
  }
  else if ( uri && strstr ( uri, "://" ) != NULL )
    /* Already full url */
    full = (gchar *) uri;
  else if ( hostname && uri )
    /* Compose the full url */
    full = g_strdup_printf ( "%s://%s%s", (ftp?"ftp":"http"), hostname, uri );
  else {
    return CURL_DOWNLOAD_ERROR;
  }

  CURL_download_t ret = curl_download_uri ( full, f, options, cdo, handle );
  // Only free newly allocated memory
  if ( hostname != full && uri != full )
    g_free ( full );

  return ret;
}


struct MemoryStruct {
  char *data;
  size_t size;
};

static size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)data;

  mem->data = (char *)realloc(mem->data, mem->size + realsize + 1);
  if (mem->data) {
    memcpy(&(mem->data[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
  }
  return realsize;
}

/**
 * Download data from an URL into a memory buffer
 *  (hence no need to save to a temporary file)
 * Ideal for when the expected returned data is a string
 *
 * Free the returned data after use
 */
char* curl_download_get_ptr ( const char *uri, DownloadFileOptions *options )
{
  struct MemoryStruct mem;
  CURL *curl = curl_easy_init ();
  if ( !curl )
    return NULL;

  mem.data = NULL;
  mem.size = 0;
  common_opts ( curl, uri, options );

  curl_easy_setopt ( curl, CURLOPT_WRITEDATA, (void *)&mem );
  curl_easy_setopt ( curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );

  CURLcode result = curl_easy_perform ( curl );

  curl_easy_cleanup ( curl );

  if ( result != CURLE_OK ) {
    g_warning ( "%s: curl error: %d for uri %s", __FUNCTION__, result, uri );
    return NULL;
  }
  else if ( vik_debug ) {
      glong response;
      curl_easy_getinfo ( curl, CURLINFO_RESPONSE_CODE, &response );
      gdouble size;
      curl_easy_getinfo ( curl, CURLINFO_SIZE_DOWNLOAD, &size );
      g_debug ( "%s: received %.0f bytes in response %ld", __FUNCTION__, size, response );
  }

  return mem.data;
}

void * curl_download_handle_init ()
{
  return curl_easy_init();
}

void curl_download_handle_cleanup ( void *handle )
{
  curl_easy_cleanup(handle);
}
