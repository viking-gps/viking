/*
 *    Viking - GPS data editor
 *    Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *    Based on:
 *    Copyright (C) 2003-2007, Leandro A. F. Pereira <leandro@linuxmag.com.br>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, version 2.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WINDOWS
#include <windows.h>
#endif

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "util.h"
#include "dialog.h"
#include "globals.h"
#include "download.h"

#ifndef WINDOWS
static gboolean spawn_command_line_async(const gchar * cmd,
                                         const gchar * arg)
{
  gchar *cmdline = NULL;
  gboolean status;

  cmdline = g_strdup_printf("%s '%s'", cmd, arg);
  g_debug("Running: %s", cmdline);
    
  status = g_spawn_command_line_async(cmdline, NULL);

  g_free(cmdline);
 
  return status;
}
#endif

void open_url(GtkWindow *parent, const gchar * url)
{
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) url, NULL, ".\\", 0);
#else /* WINDOWS */
  const gchar *browsers[] = {
    "xdg-open", "gnome-open", "kfmclient openURL",
    "sensible-browser", "firefox", "epiphany",
    "iceweasel", "seamonkey", "galeon", "mozilla",
    "opera", "konqueror", "netscape", "links -g",
    "chromium-browser", "chromium", "chrome",
    "safari", "camino", "omniweb", "icab",
    NULL
  };
  gint i=0;
  
  const gchar *browser = g_getenv("BROWSER");
  if (browser == NULL || browser[0] == '\0') {
    /* $BROWSER not set -> use first entry */
    browser = browsers[i++];
  }
  do {
    if (spawn_command_line_async(browser, url)) {
      return;
    }

    browser = browsers[i++];
  } while(browser);
  
  a_dialog_error_msg ( parent, _("Could not launch web browser.") );
#endif /* WINDOWS */
}

void new_email(GtkWindow *parent, const gchar * address)
{
  gchar *uri = g_strdup_printf("mailto:%s", address);
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) uri, NULL, ".\\", 0);
#else /* WINDOWS */
  if (!spawn_command_line_async("xdg-email", uri))
    a_dialog_error_msg ( parent, _("Could not create new email.") );
#endif /* WINDOWS */
  g_free(uri);
  uri = NULL;
}

gchar *uri_escape(gchar *str)
{
  gchar *esc_str = g_malloc(3*strlen(str));
  gchar *dst = esc_str;
  gchar *src;

  for (src = str; *src; src++) {
    if (*src == ' ')
     *dst++ = '+';
    else if (g_ascii_isalnum(*src))
     *dst++ = *src;
    else {
      g_sprintf(dst, "%%%02hhX", *src);
      dst += 3;
    }
  }
  *dst = '\0';

  return(esc_str);
}


GList * str_array_to_glist(gchar* data[])
{
  GList *gl = NULL;
  gpointer * p;
  for (p = (gpointer)data; *p; p++)
    gl = g_list_prepend(gl, *p);
  return g_list_reverse(gl);
}

typedef struct {
  GtkWindow *window; // Layer needed for redrawing
  gchar *version;     // Image list
} new_version_thread_data;

static gboolean new_version_available_message ( new_version_thread_data *nvtd )
{
  // Only a simple goto website option is offered
  // Trying to do an installation update is platform specific
  if ( a_dialog_yes_or_no ( nvtd->window,
                            _("There is a newer version of Viking available: %s\n\nDo you wish to go to Viking's website now?"), nvtd->version ) )
    // NB 'VIKING_URL' redirects to the Wiki, here we want to go the main site.
    open_url ( nvtd->window, "http://sourceforge.net/projects/viking/" );
  //else
  //  increase amount of time between performing version checks
  g_free ( nvtd->version );
  g_free ( nvtd );
  return FALSE;
}

static void latest_version_thread ( GtkWindow *window )
{
  // Need to allow a few of redirects, as SF file is often served from different server
  DownloadMapOptions options = { FALSE, FALSE, NULL, 5, NULL, NULL };
  gchar *filename = a_download_uri_to_tmp_file ( "http://sourceforge.net/projects/viking/files/VERSION", &options );
  //gchar *filename = g_strdup ( "VERSION" );
  if ( !filename ) {
    return;
  }

  GMappedFile *mf = g_mapped_file_new ( filename, FALSE, NULL );
  if ( !mf )
    return;

  gchar *text = g_mapped_file_get_contents ( mf );

  gint latest_version = viking_version_to_number ( text );
  gint my_version = viking_version_to_number ( VIKING_VERSION );

  g_debug ( "The lastest version is: %s", text );

  if ( my_version < latest_version ) {
    new_version_thread_data *nvtd = g_malloc ( sizeof(new_version_thread_data) );
    nvtd->window = window;
    nvtd->version = g_strdup ( text );
    gdk_threads_add_idle ( (GSourceFunc) new_version_available_message, nvtd );
  }
  else
    g_debug ( "Running the lastest version: %s", VIKING_VERSION );

  g_mapped_file_unref ( mf );
  if ( filename ) {
    g_remove ( filename );
    g_free ( filename );
  }
}

/*
 * check_latest_version:
 * @window: Somewhere where we may need use the display to inform the user about the version status
 *
 * Periodically checks the released latest VERSION file on the website to compare with the running version
 *
 * ATM the plan is for a 1.4.2 release to be always on *just* for Windows platforms
 * Then in 1.5.X it will made entirely optional (default on for Windows)
 *  with a longer periodic check (enabled via state saving using the soon to be released 'settings' code)
 *
 */
void check_latest_version ( GtkWindow *window )
{
#ifdef WINDOWS
#if GLIB_CHECK_VERSION (2, 32, 0)
  g_thread_try_new ( "latest_version_thread", (GThreadFunc)latest_version_thread, window, NULL );
#else
  g_thread_create ( (GThreadFunc)latest_version_thread, window, FALSE, NULL );
#endif
#endif
}
