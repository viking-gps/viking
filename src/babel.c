/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

/**
 * SECTION:babel
 * @short_description: running external programs and redirecting to TRWLayers.
 *
 * GPSBabel may not be necessary for everything -- for instance,
 *   use a_babel_convert_from_shellcommand() with input_file_type == %NULL
 *   for an external program that outputs GPX.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viking.h"
#include "gpx.h"
#include "babel.h"
#include "preferences.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

/* TODO in the future we could have support for other shells (change command strings), or not use a shell at all */
#define BASH_LOCATION "/bin/bash"

/**
 * List of supported protocols.
 */
const gchar *PROTOS[] = { "http://", "https://", "ftp://", NULL };

/**
 * Path to gpsbabel
 */
static gchar *gpsbabel_loc = NULL;

/**
 * Path to unbuffer
 */
static gchar *unbuffer_loc = NULL;

/**
 * List of file formats supported by gpsbabel.
 */
GList *a_babel_file_list;

/**
 * List of device supported by gpsbabel.
 */
GList *a_babel_device_list;

/**
 * Run a function on all file formats supporting a given mode.
 */
void a_babel_foreach_file_with_mode (BabelMode mode, GFunc func, gpointer user_data)
{
  GList *current;
  for ( current = g_list_first (a_babel_file_list) ;
        current != NULL ;
        current = g_list_next (current) )
  {
    BabelFile *currentFile = current->data;
    /* Check compatibility of modes */
    gboolean compat = TRUE;
    if (mode.waypointsRead  && ! currentFile->mode.waypointsRead)  compat = FALSE;
    if (mode.waypointsWrite && ! currentFile->mode.waypointsWrite) compat = FALSE;
    if (mode.tracksRead     && ! currentFile->mode.tracksRead)     compat = FALSE;
    if (mode.tracksWrite    && ! currentFile->mode.tracksWrite)    compat = FALSE;
    if (mode.routesRead     && ! currentFile->mode.routesRead)     compat = FALSE;
    if (mode.routesWrite    && ! currentFile->mode.routesWrite)    compat = FALSE;
    /* Do call */
    if (compat)
      func (currentFile, user_data);
  }
}

/**
 * a_babel_foreach_file_read_any:
 * @func:      The function to be called on any file format with a read method
 * @user_data: Data passed into the function
 *
 * Run a function on all file formats with any kind of read method
 *  (which is almost all but not quite - e.g. with GPSBabel v1.4.4 - PalmDoc is write only waypoints)
 */
void a_babel_foreach_file_read_any (GFunc func, gpointer user_data)
{
  GList *current;
  for ( current = g_list_first (a_babel_file_list) ;
        current != NULL ;
        current = g_list_next (current) )
  {
    BabelFile *currentFile = current->data;
    // Call function when any read mode found
    if ( currentFile->mode.waypointsRead ||
         currentFile->mode.tracksRead ||
         currentFile->mode.routesRead)
      func (currentFile, user_data);
  }
}

/**
 * a_babel_convert:
 * @vt:        The TRW layer to modify. All data will be deleted, and replaced by what gpsbabel outputs.
 * @babelargs: A string containing gpsbabel command line filter options. No file types or names should
 *             be specified.
 * @cb:        A callback function.
 * @user_data: passed along to cb
 * @not_used:  Must use NULL
 *
 * This function modifies data in a trw layer using gpsbabel filters.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %TRUE on success
 */
gboolean a_babel_convert( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, gpointer user_data, gpointer not_used )
{
  gboolean ret = FALSE;
  gchar *bargs = g_strconcat(babelargs, " -i gpx", NULL);
  gchar *name_src = a_gpx_write_tmp_file ( vt, NULL );

  if ( name_src ) {
    ret = a_babel_convert_from ( vt, bargs, name_src, cb, user_data, not_used );
    g_remove(name_src);
    g_free(name_src);
  }

  g_free(bargs);
  return ret;
}

/**
 * Perform any cleanup actions once GPSBabel has completed running
 */
static void babel_watch ( GPid pid,
                          gint status,
                          gpointer user_data )
{
  g_spawn_close_pid ( pid );
}

/**
 * babel_general_convert:
 * @args: The command line arguments passed to GPSBabel
 * @cb: callback that is run for each line of GPSBabel output and at completion of the run
 *      callback may be NULL
 * @user_data: passed along to cb
 *
 * The function to actually invoke the GPSBabel external command
 *
 * Returns: %TRUE on successful invocation of GPSBabel command
 */
static gboolean babel_general_convert( BabelStatusFunc cb, gchar **args, gpointer user_data )
{
  gboolean ret = FALSE;
  GPid pid;
  GError *error = NULL;
  gint babel_stdout;

  if (!g_spawn_async_with_pipes (NULL, args, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL, &babel_stdout, NULL, &error)) {
    g_warning ("Async command failed: %s", error->message);
    g_error_free(error);
    ret = FALSE;
  } else {

    gchar line[512];
    FILE *diag;
    diag = fdopen(babel_stdout, "r");
    setvbuf(diag, NULL, _IONBF, 0);

    while (fgets(line, sizeof(line), diag)) {
      if ( cb )
        cb(BABEL_DIAG_OUTPUT, line, user_data);
    }
    if ( cb )
      cb(BABEL_DONE, NULL, user_data);
    fclose(diag);
    diag = NULL;

    g_child_watch_add ( pid, (GChildWatchFunc) babel_watch, NULL );

    ret = TRUE;
  }
    
  return ret;
}

/**
 * babel_general_convert_from:
 * @vtl: The TrackWaypoint Layer to save the data into
 *   If it is null it signifies that no data is to be processed,
 *    however the gpsbabel command is still ran as it can be for non-data related options eg:
 *    for use with the power off command - 'command_off'
 * @cb: callback that is run upon new data from STDOUT (?)
 *     (TODO: STDERR would be nice since we usually redirect STDOUT)
 * @user_data: passed along to cb
 *
 * Runs args[0] with the arguments and uses the GPX module
 * to import the GPX data into layer vt. Assumes that upon
 * running the command, the data will appear in the (usually
 * temporary) file name_dst.
 *
 * Returns: %TRUE on success
 */
static gboolean babel_general_convert_from( VikTrwLayer *vt, BabelStatusFunc cb, gchar **args, const gchar *name_dst, gpointer user_data )
{
  gboolean ret = FALSE;
  FILE *f = NULL;
    
  if (babel_general_convert(cb, args, user_data)) {

    /* No data actually required but still need to have run gpsbabel anyway
       - eg using the device power command_off */
    if ( vt == NULL )
      return TRUE;

    f = g_fopen(name_dst, "r");
    if (f) {
      ret = a_gpx_read_file ( vt, f );
      fclose(f);
      f = NULL;
    }
  }
    
  return ret;
}

/**
 * a_babel_convert_from_filter:
 * @vt:           The TRW layer to place data into. Duplicate items will be overwritten.
 * @babelargs:    A string containing gpsbabel command line options. This string
 *                must include the input file type (-i) option.
 * @from          the file name to convert from
 * @babelfilters: A string containing gpsbabel filter command line options 
 * @cb:	          Optional callback function. Same usage as in a_babel_convert().
 * @user_data:    passed along to cb
 * @not_used:     Must use NULL
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %TRUE on success
 */
gboolean a_babel_convert_from_filter( VikTrwLayer *vt, const char *babelargs, const char *from, const char *babelfilters, BabelStatusFunc cb, gpointer user_data, gpointer not_used )
{
  int i,j;
  int fd_dst;
  gchar *name_dst = NULL;
  gboolean ret = FALSE;
  gchar *args[64];

  if ((fd_dst = g_file_open_tmp("tmp-viking.XXXXXX", &name_dst, NULL)) >= 0) {
    g_debug ("%s: temporary file: %s", __FUNCTION__, name_dst);
    close(fd_dst);

    if (gpsbabel_loc ) {
      gchar **sub_args = g_strsplit(babelargs, " ", 0);
      gchar **sub_filters = NULL;

      i = 0;
      if (unbuffer_loc)
        args[i++] = unbuffer_loc;
      args[i++] = gpsbabel_loc;
      for (j = 0; sub_args[j]; j++) {
        /* some version of gpsbabel can not take extra blank arg */
        if (sub_args[j][0] != '\0')
          args[i++] = sub_args[j];
      }
      args[i++] = "-f";
      args[i++] = (char *)from;
      if (babelfilters) {
        sub_filters = g_strsplit(babelfilters, " ", 0);
        for (j = 0; sub_filters[j]; j++) {
          /* some version of gpsbabel can not take extra blank arg */
          if (sub_filters[j][0] != '\0')
            args[i++] = sub_filters[j];
        }
      }
      args[i++] = "-o";
      args[i++] = "gpx";
      args[i++] = "-F";
      args[i++] = name_dst;
      args[i] = NULL;

      ret = babel_general_convert_from ( vt, cb, args, name_dst, user_data );

      g_strfreev(sub_args);
      if (sub_filters)
          g_strfreev(sub_filters);
    } else
      g_critical("gpsbabel not found in PATH");
    g_remove(name_dst);
    g_free(name_dst);
  }

  return ret;
}

/**
 * a_babel_convert_from:
 * @vt:        The TRW layer to place data into. Duplicate items will be overwritten.
 * @babelargs: A string containing gpsbabel command line options.  This string
 *             must include the input file type (-i) option.
 * @from:      The file name to convert from
 * @cb:	       Optional callback function. Same usage as in a_babel_convert().
 * @user_data: passed along to cb
 * @not_used:  Must use NULL
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %TRUE on success
 */
gboolean a_babel_convert_from( VikTrwLayer *vt, const char *babelargs, const char *from, BabelStatusFunc cb, gpointer user_data, gpointer not_used )
{
  return a_babel_convert_from_filter ( vt, babelargs, from, NULL, cb, user_data, not_used ); 
}
/**
 * a_babel_convert_from_shellcommand:
 * @vt: The #VikTrwLayer where to insert the collected data
 * @input_cmd: the command to run
 * @cb:	       Optional callback function. Same usage as in a_babel_convert().
 * @user_data: passed along to cb
 * @not_used:  Must use NULL
 *
 * Runs the input command in a shell (bash) and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, doesn't use GPSBabel. Input must be GPX (or Geocaching *.loc)
 *
 * Uses babel_general_convert_from() to actually run the command. This function
 * prepares the command and temporary file, and sets up the arguments for bash.
 */
gboolean a_babel_convert_from_shellcommand ( VikTrwLayer *vt, const char *input_cmd, const char *input_file_type, BabelStatusFunc cb, gpointer user_data, gpointer not_used )
{
  int fd_dst;
  gchar *name_dst = NULL;
  gboolean ret = FALSE;
  gchar **args;  

  if ((fd_dst = g_file_open_tmp("tmp-viking.XXXXXX", &name_dst, NULL)) >= 0) {
    g_debug ("%s: temporary file: %s", __FUNCTION__, name_dst);
    gchar *shell_command;
    if ( input_file_type )
      shell_command = g_strdup_printf("%s | %s -i %s -f - -o gpx -F %s",
        input_cmd, gpsbabel_loc, input_file_type, name_dst);
    else
      shell_command = g_strdup_printf("%s > %s", input_cmd, name_dst);

    g_debug("%s: %s", __FUNCTION__, shell_command);
    close(fd_dst);

    args = g_malloc(sizeof(gchar *)*4);
    args[0] = BASH_LOCATION;
    args[1] = "-c";
    args[2] = shell_command;
    args[3] = NULL;

    ret = babel_general_convert_from ( vt, cb, args, name_dst, user_data );
    g_free ( args );
    g_free ( shell_command );
    g_remove(name_dst);
    g_free(name_dst);
  }

  return ret;
}

/**
 * a_babel_convert_from_url:
 * @vt: The #VikTrwLayer where to insert the collected data
 * @url: the URL to fetch
 * @babelfilters: the filter arguments to pass to gpsbabel
 * @cb:	       Optional callback function. Same usage as in a_babel_convert().
 * @user_data: passed along to cb
 * @options:   download options. Maybe NULL.
 *
 * Download the file pointed by the URL and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, input must be GPX.
 * If input_file_type and babelfilters are %NULL, gpsbabel is not used.
 *
 * Returns: %TRUE on successful invocation of GPSBabel or read of the GPX
 *
 */
gboolean a_babel_convert_from_url_filter ( VikTrwLayer *vt, const char *url, const char *input_type, const char *babelfilters, BabelStatusFunc cb, gpointer user_data, DownloadMapOptions *options )
{
  // If no download options specified, use defaults:
  DownloadMapOptions myoptions = { FALSE, FALSE, NULL, 2, NULL, NULL, NULL };
  if ( options )
    myoptions = *options;
  gint fd_src;
  int fetch_ret;
  gboolean ret = FALSE;
  gchar *name_src = NULL;
  gchar *babelargs = NULL;

  g_debug("%s: input_type=%s url=%s", __FUNCTION__, input_type, url);

  if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) >= 0) {
    g_debug ("%s: temporary file: %s", __FUNCTION__, name_src);
    close(fd_src);
    g_remove(name_src);

    fetch_ret = a_http_download_get_url(url, "", name_src, &myoptions, NULL);
    if (fetch_ret == DOWNLOAD_SUCCESS) {
      if (input_type != NULL || babelfilters != NULL) {
        babelargs = (input_type) ? g_strdup_printf(" -i %s", input_type) : g_strdup("");
        ret = a_babel_convert_from_filter( vt, babelargs, name_src, babelfilters, NULL, NULL, NULL );
      } else {
        /* Process directly the retrieved file */
        g_debug("%s: directly read GPX file %s", __FUNCTION__, name_src);
        FILE *f = g_fopen(name_src, "r");
        if (f) {
          ret = a_gpx_read_file ( vt, f );
          fclose(f);
          f = NULL;
        }
      }
    }
    util_remove(name_src);
    g_free(babelargs);
    g_free(name_src);
  }

  return ret;
}

/**
 * a_babel_convert_from_url:
 * @vt: The #VikTrwLayer where to insert the collected data
 * @url: the URL to fetch
 * @cb:	       Optional callback function. Same usage as in a_babel_convert().
 * @user_data: passed along to cb
 * @options:   download options. Maybe NULL.
 *
 * Download the file pointed by the URL and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, doesn't use GPSBabel. Input must be GPX.
 *
 * Returns: %TRUE on successful invocation of GPSBabel or read of the GPX
 *
 */
gboolean a_babel_convert_from_url ( VikTrwLayer *vt, const char *url, const char *input_type, BabelStatusFunc cb, gpointer user_data, DownloadMapOptions *options )
{
  return a_babel_convert_from_url_filter ( vt, url, input_type, NULL, cb, user_data, options );
}

/**
 * a_babel_convert_from_url_or_shell:
 * @vt: The #VikTrwLayer where to insert the collected data
 * @url: the URL to fetch
 * @cb:	       Optional callback function. Same usage as in a_babel_convert().
 * @user_data: passed along to cb
 * @options:   download options. Maybe NULL.
 *
 * Download the file pointed by the URL and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, doesn't use GPSBabel. Input must be GPX.
 *
 * Returns: %TRUE on successful invocation of GPSBabel or read of the GPX
 *
 */
gboolean a_babel_convert_from_url_or_shell ( VikTrwLayer *vt, const char *input, const char *input_type, BabelStatusFunc cb, gpointer user_data, DownloadMapOptions *options )
{
  
  /* Check nature of input */
  gboolean isUrl = FALSE;
  int i = 0;
  for (i = 0 ; PROTOS[i] != NULL ; i++)
  {
    const gchar *proto = PROTOS[i];
    if (strncmp (input, proto, strlen(proto)) == 0)
    {
      /* Procotol matches: save result */
      isUrl = TRUE;
    }
  }
  
  /* Do the job */
  if (isUrl)
    return a_babel_convert_from_url (vt, input, input_type, cb, user_data, options);
  else
    return a_babel_convert_from_shellcommand (vt, input, input_type, cb, user_data, options);
}

static gboolean babel_general_convert_to( VikTrwLayer *vt, VikTrack *trk, BabelStatusFunc cb, gchar **args, const gchar *name_src, gpointer user_data )
{
  // Now strips out invisible tracks and waypoints
  if (!a_file_export(vt, name_src, FILE_TYPE_GPX, trk, FALSE)) {
    g_critical("Error exporting to %s", name_src);
    return FALSE;
  }
       
  return babel_general_convert (cb, args, user_data);
}

/**
 * a_babel_convert_to:
 * @vt:             The TRW layer from which data is taken.
 * @track:          Operate on the individual track if specified. Use NULL when operating on a TRW layer
 * @babelargs:      A string containing gpsbabel command line options.  In addition to any filters, this string
 *                 must include the input file type (-i) option.
 * @to:             Filename or device the data is written to.
 * @cb:		   Optional callback function. Same usage as in a_babel_convert.
 * @user_data: passed along to cb
 *
 * Exports data using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %TRUE on successful invocation of GPSBabel command
 */
gboolean a_babel_convert_to( VikTrwLayer *vt, VikTrack *track, const char *babelargs, const char *to, BabelStatusFunc cb, gpointer user_data )
{
  int i,j;
  int fd_src;
  gchar *name_src = NULL;
  gboolean ret = FALSE;
  gchar *args[64];  

  if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) >= 0) {
    g_debug ("%s: temporary file: %s", __FUNCTION__, name_src);
    close(fd_src);

    if (gpsbabel_loc ) {
      gchar **sub_args = g_strsplit(babelargs, " ", 0);

      i = 0;
      if (unbuffer_loc)
        args[i++] = unbuffer_loc;
      args[i++] = gpsbabel_loc;
      args[i++] = "-i";
      args[i++] = "gpx";
      for (j = 0; sub_args[j]; j++)
        /* some version of gpsbabel can not take extra blank arg */
        if (sub_args[j][0] != '\0')
          args[i++] = sub_args[j];
      args[i++] = "-f";
      args[i++] = name_src;
      args[i++] = "-F";
      args[i++] = (char *)to;
      args[i] = NULL;

      ret = babel_general_convert_to ( vt, track, cb, args, name_src, user_data );

      g_strfreev(sub_args);
    } else
      g_critical("gpsbabel not found in PATH");
    g_remove(name_src);
    g_free(name_src);
  }

  return ret;
}

static void set_mode(BabelMode *mode, gchar *smode)
{
  mode->waypointsRead  = smode[0] == 'r';
  mode->waypointsWrite = smode[1] == 'w';
  mode->tracksRead     = smode[2] == 'r';
  mode->tracksWrite    = smode[3] == 'w';
  mode->routesRead     = smode[4] == 'r';
  mode->routesWrite    = smode[5] == 'w';
}

/**
 * load_feature_parse_line:
 * 
 * Load a single feature stored in the given line.
 */
static void load_feature_parse_line (gchar *line)
{
  gchar **tokens = g_strsplit ( line, "\t", 0 );
  if ( tokens != NULL
       && tokens[0] != NULL ) {
    if ( strcmp("serial", tokens[0]) == 0 ) {
      if ( tokens[1] != NULL
           && tokens[2] != NULL
           && tokens[3] != NULL
           && tokens[4] != NULL ) {
        BabelDevice *device = g_malloc ( sizeof (BabelDevice) );
        set_mode (&(device->mode), tokens[1]);
        device->name = g_strdup (tokens[2]);
        device->label = g_strndup (tokens[4], 50); // Limit really long label text
        a_babel_device_list = g_list_append (a_babel_device_list, device);
        g_debug ("New gpsbabel device: %s, %d%d%d%d%d%d(%s)",
        		device->name,
        		device->mode.waypointsRead, device->mode.waypointsWrite,
        		device->mode.tracksRead, device->mode.tracksWrite,
        		device->mode.routesRead, device->mode.routesWrite,
        			tokens[1]);
      } else {
        g_warning ( "Unexpected gpsbabel format string: %s", line);
      }
    } else if ( strcmp("file", tokens[0]) == 0 ) {
      if ( tokens[1] != NULL
           && tokens[2] != NULL
           && tokens[3] != NULL
           && tokens[4] != NULL ) {
        BabelFile *file = g_malloc ( sizeof (BabelFile) );
        set_mode (&(file->mode), tokens[1]);
        file->name = g_strdup (tokens[2]);
        file->ext = g_strdup (tokens[3]);
        file->label = g_strdup (tokens[4]);
        a_babel_file_list = g_list_append (a_babel_file_list, file);
        g_debug ("New gpsbabel file: %s, %d%d%d%d%d%d(%s)",
			file->name,
			file->mode.waypointsRead, file->mode.waypointsWrite,
			file->mode.tracksRead, file->mode.tracksWrite,
			file->mode.routesRead, file->mode.routesWrite,
			tokens[1]);
      } else {
        g_warning ( "Unexpected gpsbabel format string: %s", line);
      }
    } /* else: ignore */
  } else {
    g_warning ( "Unexpected gpsbabel format string: %s", line);
  }
  g_strfreev ( tokens );
}

static void load_feature_cb (BabelProgressCode code, gpointer line, gpointer user_data)
{
  if (line != NULL)
    load_feature_parse_line (line);
}

static gboolean load_feature ()
{
  int i;
  gboolean ret = FALSE;
  gchar *args[4];  

  if ( gpsbabel_loc ) {
    i = 0;
    if ( unbuffer_loc )
      args[i++] = unbuffer_loc;
    args[i++] = gpsbabel_loc;
    args[i++] = "-^3";
    args[i] = NULL;

    ret = babel_general_convert (load_feature_cb, args, NULL);
  } else
    g_critical("gpsbabel not found in PATH");

  return ret;
}

static VikLayerParam prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpsbabel", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("GPSBabel:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL,
      N_("Allow setting the specific instance of GPSBabel. You must restart Viking for this value to take effect."), NULL, NULL, NULL },
};

/**
 * a_babel_init:
 * 
 * Just setup preferences first
 */
void a_babel_init ()
{
  // Set the defaults
  VikLayerParamData vlpd;
#ifdef WINDOWS
  // Basic guesses - could use %ProgramFiles% but this is simpler:
  if ( g_file_test ( "C:\\Program Files (x86)\\GPSBabel\\gpsbabel.exe", G_FILE_TEST_EXISTS ) )
    // 32 bit location on a 64 bit system
    vlpd.s = "C:\\Program Files (x86)\\GPSBabel\gpsbabel.exe";
  else
    vlpd.s = "C:\\Program Files\\GPSBabel\\gpsbabel.exe";
#else
  vlpd.s = "gpsbabel";
#endif
  a_preferences_register(&prefs[0], vlpd, VIKING_PREFERENCES_IO_GROUP_KEY);
}

/**
 * a_babel_post_init:
 *
 * Initialises babel module.
 * Mainly check existence of gpsbabel progam
 * and load all features available in that version.
 */
void a_babel_post_init ()
{
  // Read the current preference
  const gchar *gpsbabel = a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpsbabel")->s;
  // If setting is still the UNIX default then lookup in the path - otherwise attempt to use the specified value directly.
  if ( g_strcmp0 ( gpsbabel, "gpsbabel" ) == 0 ) {
    gpsbabel_loc = g_find_program_in_path( "gpsbabel" );
    if ( !gpsbabel_loc )
      g_critical( "gpsbabel not found in PATH" );
  }
  else
    gpsbabel_loc = (gchar*)gpsbabel;

  // Unlikely to package unbuffer on Windows so ATM don't even bother trying
  // Highly unlikely unbuffer is available on a Windows system otherwise
#ifndef WINDOWS
  unbuffer_loc = g_find_program_in_path( "unbuffer" );
  if ( !unbuffer_loc )
    g_warning( "unbuffer not found in PATH" );
#endif

  load_feature ();
}

/**
 * a_babel_uninit:
 * 
 * Free resources acquired by a_babel_init.
 */
void a_babel_uninit ()
{
  g_free ( gpsbabel_loc );
  g_free ( unbuffer_loc );

  if ( a_babel_file_list ) {
    GList *gl;
    for (gl = a_babel_file_list; gl != NULL; gl = g_list_next(gl)) {
      BabelFile *file = gl->data;
      g_free ( file->name );
      g_free ( file->ext );
      g_free ( file->label );
      g_free ( gl->data );
    }
    g_list_free ( a_babel_file_list );
  }

  if ( a_babel_device_list ) {
    GList *gl;
    for (gl = a_babel_device_list; gl != NULL; gl = g_list_next(gl)) {
      BabelDevice *device = gl->data;
      g_free ( device->name );
      g_free ( device->label );
      g_free ( gl->data );
    }
    g_list_free ( a_babel_device_list );
  }

}

/**
 * a_babel_available:
 *
 * Indicates if babel is available or not.
 *
 * Returns: true if babel available
 */
gboolean a_babel_available ()
{
  return a_babel_device_list != NULL;
}
