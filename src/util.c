/*
 *    Viking - GPS data editor
 *    Copyright (C) 2007 Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *    Based on:
 *    Copyright (C) 2003-2007 Leandro A. F. Pereira <leandro@linuxmag.com.br>
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

#include <glib/gi18n.h>

#include "dialog.h"

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
      g_sprintf(dst, "%%%02X", *src);
      dst += 3;
    }
  }
  *dst = '\0';

  return(esc_str);
}

