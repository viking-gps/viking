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

#ifndef _VIKING_CURL_DOWNLOAD_H
#define _VIKING_CURL_DOWNLOAD_H

#include <stdio.h>

#include "download.h"

G_BEGIN_DECLS

/* Error messages returned by download functions */
typedef enum {
  CURL_DOWNLOAD_NO_ERROR = 0,
  CURL_DOWNLOAD_NO_NEWER_FILE,
  CURL_DOWNLOAD_ERROR,
  CURL_DOWNLOAD_ABORTED
} CURL_download_t;

typedef struct {
  /**
   * Time sent to server on header If-Modified-Since
   */
  time_t time_condition;
  /**
   * Etag sent by server on previous download
   */
  char *etag;
  /**
   * Etag sent by server on this download
   */
  char *new_etag;

} CurlDownloadOptions;

void curl_download_init ();
void curl_download_uninit ();
CURL_download_t curl_download_get_url ( const char *hostname, const char *uri, FILE *f, DownloadFileOptions *options, gboolean ftp, CurlDownloadOptions *curl_options, void *handle );
CURL_download_t curl_download_uri ( const char *uri, FILE *f, DownloadFileOptions *options, CurlDownloadOptions *curl_options, void *handle );
void * curl_download_handle_init ();
void curl_download_handle_cleanup ( void * handle );

char* curl_download_get_ptr ( const char *uri, DownloadFileOptions *options );

G_END_DECLS

#endif
