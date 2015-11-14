/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif
#include "compression.h"

#include "download.h"

#include "curl_download.h"
#include "preferences.h"
#include "globals.h"
#include "vik_compat.h"

static gboolean check_file_first_line(FILE* f, gchar *patterns[])
{
  gchar **s;
  gchar *bp;
  fpos_t pos;
  gchar buf[33];
  size_t nr;

  memset(buf, 0, sizeof(buf));
  if ( !fgetpos(f, &pos) )
    return FALSE;
  rewind(f);
  nr = fread(buf, 1, sizeof(buf) - 1, f);
  if ( !fgetpos(f, &pos) )
    return FALSE;
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
static VikLayerParamScale params_scales[] = {
  {1, 365, 1, 0},		/* download_tile_age */
};

static VikLayerParamData convert_to_display ( VikLayerParamData value )
{
  // From seconds into days
  return VIK_LPD_UINT ( value.u / 86400 );
}

static VikLayerParamData convert_to_internal ( VikLayerParamData value )
{
  // From days into seconds
  return VIK_LPD_UINT ( 86400 * value.u );
}

static VikLayerParam prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "download_tile_age", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Tile age (days):"), VIK_LAYER_WIDGET_SPINBUTTON, &params_scales[0], NULL, NULL, NULL, convert_to_display, convert_to_internal },
};

void a_download_init (void)
{
	VikLayerParamData tmp;
	tmp.u = VIK_CONFIG_DEFAULT_TILE_AGE / 86400; // Now in days
	a_preferences_register(prefs, tmp, VIKING_PREFERENCES_GROUP_KEY);
	file_list_mutex = vik_mutex_new();
}

void a_download_uninit (void)
{
	vik_mutex_free(file_list_mutex);
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

/**
 * Unzip a file - replacing the file with the unzipped contents of the self
 */
static void uncompress_zip ( gchar *name )
{
	GError *error = NULL;
	GMappedFile *mf;

	if ((mf = g_mapped_file_new ( name, FALSE, &error )) == NULL) {
		g_critical(_("Couldn't map file %s: %s"), name, error->message);
		g_error_free(error);
		return;
	}
	gchar *file_contents = g_mapped_file_get_contents ( mf );

	void *unzip_mem = NULL;
	gulong ucsize;

	if ((unzip_mem = unzip_file (file_contents, &ucsize)) == NULL) {
		g_mapped_file_unref ( mf );
		return;
	}

	// This overwrites any previous file contents
	if ( ! g_file_set_contents ( name, unzip_mem, ucsize, &error ) ) {
		g_critical ( "Couldn't write file '%s', because of %s", name, error->message );
		g_error_free ( error );
	}
}

/**
 * a_try_decompress_file:
 * @name:  The potentially compressed filename
 *
 * Perform magic to decide how which type of decompression to attempt
 */
void a_try_decompress_file (gchar *name)
{
#ifdef HAVE_MAGIC_H
#ifdef MAGIC_VERSION
	// Or magic_version() if available - probably need libmagic 5.18 or so
	//  (can't determine exactly which version the versioning became available)
	g_debug ("%s: magic version: %d", __FUNCTION__, MAGIC_VERSION );
#endif
	magic_t myt = magic_open ( MAGIC_CONTINUE|MAGIC_ERROR|MAGIC_MIME );
	gboolean zip = FALSE;
	gboolean bzip2 = FALSE;
	if ( myt ) {
#ifdef WINDOWS
		// We have to 'package' the magic database ourselves :(
		//  --> %PROGRAM FILES%\Viking\magic.mgc
		int ml = magic_load ( myt, ".\\magic.mgc" );
#else
		// Use system default
		int ml = magic_load ( myt, NULL );
#endif
		if ( ml == 0 ) {
			const char* magic = magic_file (myt, name);
			g_debug ("%s: magic output: %s", __FUNCTION__, magic );

			if ( g_strcmp0 (magic, "application/zip; charset=binary") == 0 )
				zip = TRUE;

			if ( g_strcmp0 (magic, "application/x-bzip2; charset=binary") == 0 )
				bzip2 = TRUE;
		}
		else {
			g_critical ("%s: magic load database failure", __FUNCTION__ );
		}

		magic_close ( myt );
	}

	if ( !(zip || bzip2) )
		return;

	if ( zip ) {
		uncompress_zip ( name );
	}
	else if ( bzip2 ) {
		gchar* bz2_name = uncompress_bzip2 ( name );
		if ( bz2_name ) {
			if ( g_remove ( name ) )
				g_critical ("%s: remove file failed [%s]", __FUNCTION__, name );
			if ( g_rename (bz2_name, name) )
				g_critical ("%s: file rename failed [%s] to [%s]", __FUNCTION__, bz2_name, name );
		}
	}

	return;
#endif
}

static DownloadResult_t download( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *options, gboolean ftp, void *handle)
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
      return DOWNLOAD_NOT_REQUIRED;
    }

    time_t tile_age = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "download_tile_age")->u;
    /* Get the modified time of this file */
    struct stat buf;
    (void)g_stat ( fn, &buf );
    time_t file_time = buf.st_mtime;
    if ( (time(NULL) - file_time) < tile_age ) {
      /* File cache is too recent, so return */
      return DOWNLOAD_NOT_REQUIRED;
    }

    if (options != NULL && options->check_file_server_time) {
      file_options.time_condition = file_time;
    }
    if (options != NULL && options->use_etag) {
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
    if ( g_mkdir_with_parents ( dir , 0777 ) != 0)
      g_warning ("%s: Failed to mkdir %s", __FUNCTION__, dir );
    g_free ( dir );
  }

  tmpfilename = g_strdup_printf("%s.tmp", fn);
  if (!lock_file ( tmpfilename ) )
  {
    g_debug("%s: Couldn't take lock on temporary file \"%s\"\n", __FUNCTION__, tmpfilename);
    g_free ( tmpfilename );
    if (options->use_etag)
      g_free ( file_options.etag );
    return DOWNLOAD_FILE_WRITE_ERROR;
  }
  f = g_fopen ( tmpfilename, "w+b" );  /* truncate file and open it */
  if ( ! f ) {
    g_warning("Couldn't open temporary file \"%s\": %s", tmpfilename, g_strerror(errno));
    g_free ( tmpfilename );
    if (options->use_etag)
      g_free ( file_options.etag );
    return DOWNLOAD_FILE_WRITE_ERROR;
  }

  /* Call the backend function */
  ret = curl_download_get_url ( hostname, uri, f, options, ftp, &file_options, handle );

  DownloadResult_t result = DOWNLOAD_SUCCESS;

  if (ret != CURL_DOWNLOAD_NO_ERROR && ret != CURL_DOWNLOAD_NO_NEWER_FILE) {
    g_debug("%s: download failed: curl_download_get_url=%d", __FUNCTION__, ret);
    failure = TRUE;
    result = DOWNLOAD_HTTP_ERROR;
  }

  if (!failure && options != NULL && options->check_file != NULL && ! options->check_file(f)) {
    g_debug("%s: file content checking failed", __FUNCTION__);
    failure = TRUE;
    result = DOWNLOAD_CONTENT_ERROR;
  }

  fclose ( f );
  f = NULL;

  if (failure)
  {
    g_warning(_("Download error: %s"), fn);
    if ( g_remove ( tmpfilename ) != 0 )
      g_warning( ("Failed to remove: %s"), tmpfilename);
    unlock_file ( tmpfilename );
    g_free ( tmpfilename );
    if ( options != NULL && options->use_etag ) {
      g_free ( file_options.etag );
      g_free ( file_options.new_etag );
    }
    return result;
  }

  if ( options != NULL && options->convert_file )
    options->convert_file ( tmpfilename );

  if ( options != NULL && options->use_etag ) {
    if (file_options.new_etag) {
      /* server returned an etag value */
      gchar *etag_filename = g_strdup_printf("%s.etag", fn);
      g_file_set_contents (etag_filename, file_options.new_etag, -1, NULL);
      g_free (etag_filename);
      etag_filename = NULL;
    }
  }

  if (ret == CURL_DOWNLOAD_NO_NEWER_FILE)  {
    (void)g_remove ( tmpfilename );
     // update mtime of local copy
     // Not security critical, thus potential Time of Check Time of Use race condition is not bad
     // coverity[toctou]
     if ( g_utime ( fn, NULL ) != 0 )
       g_warning ( "%s couldn't set time on: %s", __FUNCTION__, fn );
  } else {
     /* move completely-downloaded file to permanent location */
     if ( g_rename ( tmpfilename, fn ) )
        g_warning ("%s: file rename failed [%s] to [%s]", __FUNCTION__, tmpfilename, fn );
  }
  unlock_file ( tmpfilename );
  g_free ( tmpfilename );

  if ( options != NULL && options->use_etag ) {
    g_free ( file_options.etag );
    g_free ( file_options.new_etag );
  }
  return DOWNLOAD_SUCCESS;
}

/**
 * uri: like "/uri.html?whatever"
 * only reason for the "wrapper" is so we can do redirects.
 */
DownloadResult_t a_http_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *opt, void *handle )
{
  return download ( hostname, uri, fn, opt, FALSE, handle );
}

DownloadResult_t a_ftp_download_get_url ( const char *hostname, const char *uri, const char *fn, DownloadMapOptions *opt, void *handle )
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
  if ( !tmp_file )
    return NULL;

  if ( curl_download_uri ( uri, tmp_file, options, NULL, NULL ) ) {
    // error
    fclose ( tmp_file );
    (void)g_remove ( tmpname );
    g_free ( tmpname );
    return NULL;
  }
  fclose ( tmp_file );

  return tmpname;
}
