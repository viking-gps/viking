/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *    Viking - GPS data editor
 *    Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *    Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
 /*
  * Ideally dependencies should just be on Gtk,
  * see vikutils for things that further depend on other Viking types
  * see utils for things only depend on Glib
  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "util.h"
#include "dialog.h"

#ifdef WINDOWS
#include <windows.h>
#endif

/*
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
*/

// Annoyingly gtk_show_uri() doesn't work so resort to ShellExecute method
//   (non working at least in our Windows build with GTK+2.24.10 on Windows 7)

void open_url(GtkWindow *parent, const gchar * url)
{
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) url, NULL, ".\\", 0);
#else
  GError *error = NULL;
  gtk_show_uri ( gtk_widget_get_screen (GTK_WIDGET(parent)), url, GDK_CURRENT_TIME, &error );
  if ( error ) {
    a_dialog_error_msg_extra ( parent, _("Could not launch web browser. %s"), error->message );
    g_error_free ( error );
  }
#endif
}

void new_email(GtkWindow *parent, const gchar * address)
{
  gchar *uri = g_strdup_printf("mailto:%s", address);
  GError *error = NULL;
  gtk_show_uri ( gtk_widget_get_screen (GTK_WIDGET(parent)), uri, GDK_CURRENT_TIME, &error );
  if ( error ) {
    a_dialog_error_msg_extra ( parent, _("Could not create new email. %s"), error->message );
    g_error_free ( error );
  }
  /*
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) uri, NULL, ".\\", 0);
#else
  if (!spawn_command_line_async("xdg-email", uri))
    a_dialog_error_msg ( parent, _("Could not create new email.") );
#endif
  */
  g_free(uri);
  uri = NULL;
}
