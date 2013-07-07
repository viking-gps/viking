/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _VIKING_DOWNLOAD_H
#define _VIKING_DOWNLOAD_H

#include <stdio.h>

G_BEGIN_DECLS

/* File content check */
typedef gboolean (*VikFileContentCheckerFunc) (FILE*);
gboolean a_check_map_file(FILE*);
gboolean a_check_html_file(FILE*);
gboolean a_check_kml_file(FILE*);
// Convert
void a_try_decompress_file (gchar *name);
typedef void (*VikFileContentConvertFunc) (gchar*); // filename (temporary)

typedef struct {
  /**
   * Check if the server has a more recent file than the one we have before downloading it
   * This uses http header If-Modified-Since
   */
  gboolean check_file_server_time;

  /**
   * Set if the server handle ETag
   */
  gboolean use_etag;

  /**
   * The REFERER string to use.
   * Could be NULL.
   */
  gchar *referer;

  /**
   * follow_location specifies the number of retries
   * to follow a redirect while downloading a page.
   */
  glong follow_location;
  
  /**
   * File content checker.
   */
  VikFileContentCheckerFunc check_file;

  /**
   * If need to authenticate on download
   *  format: 'username:password'
   */
  gchar *user_pass;

  /**
   * File manipulation if necessary such as uncompressing the downloaded file.
   */
  VikFileContentConvertFunc convert_file;

} DownloadMapOptions;

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

} DownloadFileOptions;

void a_download_init(void);

/* TODO: convert to Glib */
int a_http_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *opt, void *handle );
int a_ftp_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *opt, void *handle );
void *a_download_handle_init ();
void a_download_handle_cleanup ( void *handle );

gchar *a_download_uri_to_tmp_file ( const gchar *uri, DownloadMapOptions *options );

/* Error messages returned by download functions */
enum { DOWNLOAD_NO_ERROR = 0,
       DOWNLOAD_NO_NEWER_FILE,
       DOWNLOAD_ERROR };

G_END_DECLS

#endif
