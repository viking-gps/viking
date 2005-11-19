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

#include "viking.h"
#include "gpx.h"
#include "babel.h"
#include "sys/wait.h"

gboolean a_babel_convert( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb )
{
  int fd_src;
  FILE *f;
  gchar *name_src;
  gboolean ret = FALSE;
  gchar *bargs = g_strconcat(babelargs, " -i gpx");

  if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) < 0) {
    ret = FALSE;
  } else {
    f = fdopen(fd_src, "w");
    a_gpx_write_file(vt, f);
    fclose(f);
    ret = a_babel_convert_from ( vt, bargs, cb, name_src );
  }

  g_free(bargs);
  remove(name_src);
  g_free(name_src);
  return ret;
}

gboolean a_babel_convert_from( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, const char *from )
{
  int fd_dst;
  FILE *f;
  gchar *name_dst;
  gchar cmd[1024];
  gboolean ret = FALSE;
  gchar **args;  
  GPid pid;

  gint babel_stdin, babel_stdout, babel_stderr;

  if ((fd_dst = g_file_open_tmp("tmp-viking.XXXXXX", &name_dst, NULL)) < 0) {
    ret = FALSE;
  } else {
    close(fd_dst);

    g_stpcpy(cmd, "/usr/local/bin/gpsbabel ");
    g_strlcat(cmd, babelargs, sizeof(cmd));
    g_strlcat(cmd, " -o gpx ", sizeof(cmd));
    g_strlcat(cmd, from, sizeof(cmd));
    g_strlcat(cmd, " ", sizeof(cmd));
    g_strlcat(cmd, name_dst, sizeof(cmd));

    args = g_strsplit(cmd, " ", 0);

    if (!g_spawn_async_with_pipes (NULL, args, NULL, 0, NULL, NULL, &pid, &babel_stdin, &babel_stdout, &babel_stderr, NULL)) {
      //    if (!g_spawn_async_with_pipes (NULL, args, NULL, 0, NULL, NULL, NULL, &babel_stdin, &babel_stdout, NULL, NULL)) {
      ret = FALSE;
    } else {
      gchar line[512];
      FILE *diag;
      diag = fdopen(babel_stdout, "r");
      setvbuf(diag, NULL, _IONBF, 0);

      while (fgets(line, sizeof(line), diag)) {
	cb(BABEL_DIAG_OUTPUT, line);
      }
      cb(BABEL_DONE, NULL);
      fclose(diag);
      waitpid(pid, NULL, 0);
      g_spawn_close_pid(pid);

      f = fopen(name_dst, "r");
      a_gpx_read_file ( vt, f );
      fclose(f);
      ret = TRUE;
    }
    
    g_strfreev(args);
  }

  remove(name_dst);
  g_free(name_dst);
  return ret;
}
