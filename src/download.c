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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>


#include "download.h"

#include "curl_download.h"
#include "preferences.h"
#include "globals.h"

static gboolean check_file_first_line(FILE* f, gchar *patterns[])
{
  gchar **s;
  gchar *bp;
  fpos_t pos;
  gchar buf[33];
  size_t nr;

  memset(buf, 0, sizeof(buf));
  fgetpos(f, &pos);
  rewind(f);
  nr = fread(buf, 1, sizeof(buf) - 1, f);
  fsetpos(f, &pos);
  for (bp = buf; (bp < (buf + sizeof(buf) - 1)) && (nr > (bp - buf)); bp++) {
    if (!(isspace(*bp)))
      break;
  }
  if ((bp >= (buf + sizeof(buf) -1)) || ((bp - buf) >= nr))
    return FALSE;
  for (s = patterns; *s; s++) {
    if (strncasecmp(*s, bp, strlen(*s)) == 0)
      return TRUE;
  }
  return FALSE;
}

gboolean a_check_html_file(FILE* f)
{
  gchar * html_str[] = {
    "<html",
    "<!DOCTYPE html",
    "<head",
    "<title",
    NULL
  };

  return check_file_first_line(f, html_str);
}

gboolean a_check_map_file(FILE* f)
{
  /* FIXME no more true since a_check_kml_file */
  return !a_check_html_file(f);
}

gboolean a_check_kml_file(FILE* f)
{
  gchar * kml_str[] = {
    "<?xml",
    NULL
  };

  return check_file_first_line(f, kml_str);
}

static GList *file_list = NULL;
static GMutex *file_list_mutex = NULL;

/* spin button scales */
VikLayerParamScale params_scales[] = {
  {1, 86400*7, 60, 0},		/* download_tile_age */
};

static VikLayerParam prefs[] = {
  { VIKING_PREFERENCES_NAMESPACE "download_tile_age", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Tile age (s):"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[0], NULL, NULL },
};

void a_download_init (void)
{
	VikLayerParamData tmp;
	tmp.u = VIK_CONFIG_DEFAULT_TILE_AGE;
	a_preferences_register(prefs, tmp, VIKING_PREFERENCES_GROUP_KEY);

	file_list_mutex = g_mutex_new();
}

static gboolean lock_file(const char *fn)
{
	gboolean locked = FALSE;
	g_mutex_lock(file_list_mutex);
	if (g_list_find_custom(file_list, fn, (GCompareFunc)g_strcmp0) == NULL)
	{
		// The filename is not yet locked
		file_list = g_list_append(file_list, (gpointer)fn),
		locked = TRUE;
	}
	g_mutex_unlock(file_list_mutex);
	return locked;
}

static void unlock_file(const char *fn)
{
	g_mutex_lock(file_list_mutex);
	file_list = g_list_remove(file_list, (gconstpointer)fn);
	g_mutex_unlock(file_list_mutex);
}

static int download( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *options, gboolean ftp, void *handle)
{
  FILE *f;
  int ret;
  gchar *tmpfilename;
  gboolean failure = FALSE;
  DownloadFileOptions file_options = {0, NULL, NULL};

  /* Check file */
  if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) == TRUE )
  {
    if (options == NULL || (!options->check_file_server_time &&
                            !options->use_etag)) {
      /* Nothing to do as file already exists and we don't want to check server */
      return -3;
    }

    time_t tile_age = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "download_tile_age")->u;
    /* Get the modified time of this file */
    struct stat buf;
    g_stat ( fn, &buf );
    time_t file_time = buf.st_mtime;
    if ( (time(NULL) - file_time) < tile_age ) {
      /* File cache is too recent, so return */
      return -3;
    }

    if (options->check_file_server_time) {
      file_options.time_condition = file_time;
    }
    if (options->use_etag) {
      gchar *etag_filename = g_strdup_printf("%s.etag", fn);
      gsize etag_length = 0;
      g_file_get_contents (etag_filename, &(file_options.etag), &etag_length, NULL);
      g_free (etag_filename);
      etag_filename = NULL;

      /* check if etag is short enough */
      if (etag_length > 100) {
        g_free(file_options.etag);
        file_options.etag = NULL;
      }

      /* TODO: should check that etag is a valid string */
    }

  } else {
    gchar *dir = g_path_get_dirname ( fn );
    g_mkdir_with_parents ( dir , 0777 );
    g_free ( dir );
  }

  tmpfilename = g_strdup_printf("%s.tmp", fn);
  if (!lock_file ( tmpfilename ) )
  {
    g_debug("%s: Couldn't take lock on temporary file \"%s\"\n", __FUNCTION__, tmpfilename);
    g_free ( tmpfilename );
    if (options->use_etag)
      g_free ( file_options.etag );
    return -4;
  }
  f = g_fopen ( tmpfilename, "w+b" );  /* truncate file and open it */
  if ( ! f ) {
    g_warning("Couldn't open temporary file \"%s\": %s", tmpfilename, g_strerror(errno));
    g_free ( tmpfilename );
    if (options->use_etag)
      g_free ( file_options.etag );
    return -4;
  }

  /* Call the backend function */
  ret = curl_download_get_url ( hostname, uri, f, options, ftp, &file_options, handle );

  if (ret != DOWNLOAD_NO_ERROR && ret != DOWNLOAD_NO_NEWER_FILE) {
    g_debug("%s: download failed: curl_download_get_url=%d", __FUNCTION__, ret);
    failure = TRUE;
  }

  if (!failure && options != NULL && options->check_file != NULL && ! options->check_file(f)) {
    g_debug("%s: file content checking failed", __FUNCTION__);
    failure = TRUE;
  }

  fclose ( f );
  f = NULL;

  if (failure)
  {
    g_warning(_("Download error: %s"), fn);
    g_remove ( tmpfilename );
    unlock_file ( tmpfilename );
    g_free ( tmpfilename );
    if (options->use_etag) {
      g_free ( file_options.etag );
      g_free ( file_options.new_etag );
    }
    return -1;
  }

  if (options->use_etag) {
    if (file_options.new_etag) {
      /* server returned an etag value */
      gchar *etag_filename = g_strdup_printf("%s.etag", fn);
      g_file_set_contents (etag_filename, file_options.new_etag, -1, NULL);
      g_free (etag_filename);
      etag_filename = NULL;
    }
  }

  if (ret == DOWNLOAD_NO_NEWER_FILE)  {
    g_remove ( tmpfilename );
#if GLIB_CHECK_VERSION(2,18,0)
    g_utime ( fn, NULL ); /* update mtime of local copy */
#else
    utimes ( fn, NULL ); /* update mtime of local copy */
#endif
  } else {
    g_rename ( tmpfilename, fn ); /* move completely-downloaded file to permanent location */
  }
  unlock_file ( tmpfilename );
  g_free ( tmpfilename );

  if (options->use_etag) {
    g_free ( file_options.etag );
    g_free ( file_options.new_etag );
  }
  return 0;
}

/* success = 0, -1 = couldn't connect, -2 HTTP error, -3 file exists, -4 couldn't write to file... */
/* uri: like "/uri.html?whatever" */
/* only reason for the "wrapper" is so we can do redirects. */
int a_http_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *opt, void *handle )
{
  return download ( hostname, uri, fn, opt, FALSE, handle );
}

int a_ftp_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *opt, void *handle )
{
  return download ( hostname, uri, fn, opt, TRUE, handle );
}

void * a_download_handle_init ()
{
  return curl_download_handle_init ();
}

void a_download_handle_cleanup ( void *handle )
{
  curl_download_handle_cleanup ( handle );
}

/**
 * a_download_url_to_tmp_file:
 * @uri:         The URI (Uniform Resource Identifier)
 * @options:     Download options (maybe NULL)
 *
 * returns name of the temporary file created - NULL if unsuccessful
 *  this string needs to be freed once used
 *  the file needs to be removed once used
 */
gchar *a_download_uri_to_tmp_file ( const gchar *uri, DownloadMapOptions *options )
{
  FILE *tmp_file;
  int tmp_fd;
  gchar *tmpname;

  if ( (tmp_fd = g_file_open_tmp ("viking-download.XXXXXX", &tmpname, NULL)) == -1 ) {
    g_critical (_("couldn't open temp file"));
    return NULL;
  }

  tmp_file = fdopen(tmp_fd, "r+");

  if ( curl_download_uri ( uri, tmp_file, options, NULL, NULL ) ) {
    // error
    fclose ( tmp_file );
    g_remove ( tmpname );
    g_free ( tmpname );
    return NULL;
  }
  fclose ( tmp_file );

  return tmpname;
}
