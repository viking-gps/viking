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

/* babel.c: running external programs and redirecting to TRWLayers.
 * GPSBabel may not be necessary for everything -- for instance,
 *   use a_babel_convert_from_shellcommand with input_file_type == NULL
 *   for an external program that outputs GPX.
 */

#include "viking.h"
#include "gpx.h"
#include "babel.h"
#include <sys/wait.h>

/* in the future we could have support for other shells (change command strings), or not use a shell at all */
#define BASH_LOCATION "/bin/bash"

gboolean a_babel_convert( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, gpointer user_data )
{
  int fd_src;
  FILE *f;
  gchar *name_src;
  gboolean ret = FALSE;
  gchar *bargs = g_strconcat(babelargs, " -i gpx", NULL);

  if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) < 0) {
    ret = FALSE;
  } else {
    f = fdopen(fd_src, "w");
    a_gpx_write_file(vt, f);
    fclose(f);
    ret = a_babel_convert_from ( vt, bargs, cb, name_src, user_data );
  }

  g_free(bargs);
  remove(name_src);
  g_free(name_src);
  return ret;
}

/* Runs args[0] with the arguments and uses the GPX module
 * to import the GPX data into layer vt. Assumes that upon
 * running the command, the data will appear in the (usually
 * temporary) file name_dst.
 *
 * cb: callback that is run upon new data from STDOUT (?)
 *     (TODO: STDERR would be nice since we usually redirect STDOUT)
 * user_data: passed along to cb
 *
 * returns TRUE on success
 */
gboolean babel_general_convert_from( VikTrwLayer *vt, BabelStatusFunc cb, gchar **args, const gchar *name_dst, gpointer user_data )
{
  gboolean ret;
  GPid pid;
  gint babel_stdin, babel_stdout, babel_stderr;
  FILE *f;


  if (!g_spawn_async_with_pipes (NULL, args, NULL, 0, NULL, NULL, &pid, &babel_stdin, &babel_stdout, &babel_stderr, NULL)) {
    //    if (!g_spawn_async_with_pipes (NULL, args, NULL, 0, NULL, NULL, NULL, &babel_stdin, &babel_stdout, NULL, NULL)) {
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
    waitpid(pid, NULL, 0);
    g_spawn_close_pid(pid);

    f = fopen(name_dst, "r");
    a_gpx_read_file ( vt, f );
     fclose(f);
    ret = TRUE;
  }
    
  return ret;
}

gboolean a_babel_convert_from( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, const char *from, gpointer user_data )
{
  int fd_dst;
  gchar *name_dst;
  gchar *cmd;
  gboolean ret = FALSE;
  gchar **args;  

  if ((fd_dst = g_file_open_tmp("tmp-viking.XXXXXX", &name_dst, NULL)) < 0) {
    ret = FALSE;
  } else {
    gchar *gpsbabel_loc;
    close(fd_dst);

    gpsbabel_loc = g_find_program_in_path("gpsbabel");

    if (gpsbabel_loc ) {
      gchar *unbuffer_loc = g_find_program_in_path("unbuffer");
      cmd = g_strdup_printf ( "%s%s%s %s -o gpx %s %s",
			      unbuffer_loc ? unbuffer_loc : "",
			      unbuffer_loc ? " " : "",
			      gpsbabel_loc,
			      babelargs,
			      from,
			      name_dst );

      if ( unbuffer_loc )
        g_free ( unbuffer_loc );

      args = g_strsplit(cmd, " ", 0);
      ret = babel_general_convert_from ( vt, cb, args, name_dst, user_data );
      g_strfreev(args);
      g_free ( cmd );
    }
  }

  remove(name_dst);
  g_free(name_dst);
  return ret;
}

/* Runs the input command in a shell (bash) and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is NULL, doesn't use GPSBabel. Input must be GPX (or Geocaching *.loc)
 *
 * Uses babel_general_convert_from to actually run the command. This function
 * prepares the command and temporary file, and sets up the arguments for bash.
 */
gboolean a_babel_convert_from_shellcommand ( VikTrwLayer *vt, const char *input_cmd, const char *input_file_type, BabelStatusFunc cb, gpointer user_data )
{
  int fd_dst;
  gchar *name_dst;
  gboolean ret = FALSE;
  gchar **args;  

  if ((fd_dst = g_file_open_tmp("tmp-viking.XXXXXX", &name_dst, NULL)) < 0) {
    ret = FALSE;
  } else {
    gchar *shell_command;
    if ( input_file_type )
      shell_command = g_strdup_printf("%s | gpsbabel -i %s -f - -o gpx -F %s", input_cmd, input_file_type, name_dst);
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
  }

  remove(name_dst);
  g_free(name_dst);
  return ret;
}

gboolean babel_general_convert_to( VikTrwLayer *vt, BabelStatusFunc cb, gchar **args, const gchar *name_src, gpointer user_data )
{
  gboolean ret;
  GPid pid;
  gint babel_stdin, babel_stdout, babel_stderr;

  if (!a_file_export(vt, name_src, FILE_TYPE_GPX)) {
    g_warning("%s(): error exporting to %s", __FUNCTION__, name_src);
    return(FALSE);
  }

  if (!g_spawn_async_with_pipes (NULL, args, NULL, 0, NULL, NULL, &pid, &babel_stdin, &babel_stdout, &babel_stderr, NULL)) {
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
    waitpid(pid, NULL, 0);
    g_spawn_close_pid(pid);

    ret = TRUE;
  }
    
  return ret;
}

gboolean a_babel_convert_to( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, const char *to, gpointer user_data )
{
  int fd_src;
  gchar *name_src;
  gchar *cmd;
  gboolean ret = FALSE;
  gchar **args;  

  if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) < 0) {
    ret = FALSE;
  } else {
    gchar *gpsbabel_loc;
    close(fd_src);

    gpsbabel_loc = g_find_program_in_path("gpsbabel");

    if (gpsbabel_loc ) {
      gchar *unbuffer_loc = g_find_program_in_path("unbuffer");
      cmd = g_strdup_printf ( "%s%s%s %s -i gpx %s %s",
			      unbuffer_loc ? unbuffer_loc : "",
			      unbuffer_loc ? " " : "",
			      gpsbabel_loc,
			      babelargs,
			      name_src,
			      to);

      if ( unbuffer_loc )
        g_free ( unbuffer_loc );
      g_debug ( "%s: %s", __FUNCTION__, cmd );
      args = g_strsplit(cmd, " ", 0);
      ret = babel_general_convert_to ( vt, cb, args, name_src, user_data );
      g_strfreev(args);
      g_free ( cmd );
    }
  }

  remove(name_src);
  g_free(name_src);
  return ret;
}
