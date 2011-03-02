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

#ifdef HAVE_CONFIG
#include "config.h"
#endif /* HAVE_CONFIG */

#include "viking.h"
#include "icons/icons.h"
#include "mapcache.h"
#include "background.h"
#include "dems.h"
#include "curl_download.h"
#include "preferences.h"
#include "globals.h"
#include "vikmapslayer.h"

#ifdef VIK_CONFIG_GEOCACHES
void a_datasource_gc_init();
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "modules.h"

#define MAX_WINDOWS 1024

/* FIXME LOCALEDIR must be configured by ./configure --localedir */
/* But something does not work actually. */
/* So, we need to redefine this variable on windows. */
#ifdef WINDOWS
#undef LOCALEDIR
#define LOCALEDIR "locale"
#endif

static guint window_count = 0;

static VikWindow *new_window ();
static void open_window ( VikWindow *vw, const gchar **files );
static void destroy( GtkWidget *widget,
                     gpointer   data );

/* Callback to mute log message */
static void mute_log(const gchar *log_domain,
                     GLogLevelFlags log_level,
                     const gchar *message,
                     gpointer user_data)
{
  /* Nothing to do, we just want to mute */
}

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
    }
}

/* Options */
static GOptionEntry entries[] = 
{
  { "debug", 'd', 0, G_OPTION_ARG_NONE, &vik_debug, N_("Enable debug output"), NULL },
  { "verbose", 'V', 0, G_OPTION_ARG_NONE, &vik_verbose, N_("Enable verbose output"), NULL },
  { "version", 'v', 0, G_OPTION_ARG_NONE, &vik_version, N_("Show version"), NULL },
  { NULL }
};

int main( int argc, char *argv[] )
{
  VikWindow *first_window;
  GdkPixbuf *main_icon;
  gboolean dashdash_already = FALSE;
  int i = 0;
  GError *error = NULL;
  gboolean gui_initialized;
	
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);  
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_thread_init ( NULL );
  gdk_threads_init ();

  gui_initialized = gtk_init_with_args (&argc, &argv, "files+", entries, NULL, &error);
  if (!gui_initialized)
  {
    /* check if we have an error message */
    if (error == NULL)
    {
      /* no error message, the GUI initialization failed */
      const gchar *display_name = gdk_get_display_arg_name ();
      g_fprintf (stderr, "Failed to open display: %s\n", (display_name != NULL) ? display_name : " ");
    }
    else
    {
      /* yep, there's an error, so print it */
      g_fprintf (stderr, "Parsing command line options failed: %s\n", error->message);
      g_error_free (error);
      g_fprintf (stderr, "Run \"%s --help\" to see the list of recognized options.\n",argv[0]);
    }
    return EXIT_FAILURE;
  }
   
  if (vik_version)
  {
    g_printf ("%s %s\nCopyright (c) 2003-2008 Evan Battaglia\nCopyright (c) 2008-2010 Viking's contributors\n", PACKAGE_NAME, PACKAGE_VERSION);
    return EXIT_SUCCESS;
  }

  if (!vik_debug)
    g_log_set_handler (NULL, G_LOG_LEVEL_DEBUG, mute_log, NULL);

  a_preferences_init ();

  a_vik_preferences_init ();

  a_download_init();
  curl_download_init();

  /* Init modules/plugins */
  modules_init();

  maps_layer_init ();
  a_mapcache_init ();
  a_background_init ();

#ifdef VIK_CONFIG_GEOCACHES
  a_datasource_gc_init();
#endif

  /* Set the icon */
  main_icon = gdk_pixbuf_from_pixdata(&viking_pixbuf, FALSE, NULL);
  gtk_window_set_default_icon(main_icon);

  /* Create the first window */
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

  a_background_uninit ();
  a_mapcache_uninit ();
  a_dems_uninit ();
  a_preferences_uninit ();

  curl_download_uninit();

  return 0;
}
