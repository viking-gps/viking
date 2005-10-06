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
#include "mapcache.h"
#include "background.h"

#include <string.h>

#define MAX_WINDOWS 1024

static guint window_count = 0;

static VikWindow *new_window ();
static void open_window ( VikWindow *vw, const gchar **files );
static void destroy( GtkWidget *widget,
                     gpointer   data );


/* Another callback */
static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    if ( ! --window_count )
      gtk_main_quit ();
}

static VikWindow *new_window ()
{
  if ( window_count < MAX_WINDOWS )
  {
    VikWindow *vw = vik_window_new ();

    g_signal_connect (G_OBJECT (vw), "destroy",
		      G_CALLBACK (destroy), NULL);
    g_signal_connect (G_OBJECT (vw), "newwindow",
		      G_CALLBACK (new_window), NULL);
    g_signal_connect (G_OBJECT (vw), "openwindow",
		      G_CALLBACK (open_window), NULL);

    gtk_widget_show_all ( GTK_WIDGET(vw) );

    window_count++;

    return vw;
  }
  return NULL;
}

static void open_window ( VikWindow *vw, const gchar **files )
{
  VikWindow *newvw = new_window();
  gboolean change_fn = (!files[1]); /* only change fn if one file */
  if ( newvw )
    while ( *files ) {
      vik_window_open_file ( newvw, *(files++), change_fn );
      files++;
    }
}

int main( int argc, char *argv[] )
{
  VikWindow *first_window;
  gboolean dashdash_already = FALSE;
  int i = 0;

  g_thread_init ( NULL );
  gdk_threads_init ();

  gtk_init (&argc, &argv);

  a_mapcache_init ();
  a_background_init ();

  first_window = new_window();

  gdk_threads_enter ();
  while ( ++i < argc ) {
    if ( strcmp(argv[i],"--") == 0 && !dashdash_already )
      dashdash_already = TRUE; /* hack to open '-' */
    else
      vik_window_open_file ( first_window, argv[i], argc == 2 );
  }

  gtk_main ();
  gdk_threads_leave ();

  a_mapcache_uninit ();

  return 0;
}
