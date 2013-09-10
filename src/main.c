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
#include "babel.h"
#include "curl_download.h"
#include "preferences.h"
#include "viklayer_defaults.h"
#include "globals.h"
#include "vikmapslayer.h"
#include "vikrouting.h"
#include "vikutils.h"

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

/* FIXME LOCALEDIR must be configured by ./configure --localedir */
/* But something does not work actually. */
/* So, we need to redefine this variable on windows. */
#ifdef WINDOWS
#undef LOCALEDIR
#define LOCALEDIR "locale"
#endif

#ifdef HAVE_X11_XLIB_H
#include "X11/Xlib.h"
#endif

#if GLIB_CHECK_VERSION (2, 32, 0)
/* Callback to log message */
static void log_debug(const gchar *log_domain,
                      GLogLevelFlags log_level,
                      const gchar *message,
                      gpointer user_data)
{
  g_print("** (viking): DEBUG: %s\n", message);
}
#else
/* Callback to mute log message */
static void mute_log(const gchar *log_domain,
                     GLogLevelFlags log_level,
                     const gchar *message,
                     gpointer user_data)
{
  /* Nothing to do, we just want to mute */
}
#endif

#if HAVE_X11_XLIB_H
static int myXErrorHandler(Display *display, XErrorEvent *theEvent)
{
  g_fprintf (stderr,
             _("Ignoring Xlib error: error code %d request code %d\n"),
             theEvent->error_code,
             theEvent->request_code);
  // No exit on X errors!
  //  mainly to handle out of memory error when requesting large pixbuf from user request
  //  see vikwindow.c::save_image_file ()
  return 0;
}
#endif

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

#if ! GLIB_CHECK_VERSION (2, 32, 0)
  g_thread_init ( NULL );
#endif
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
    g_printf ("%s %s\nCopyright (c) 2003-2008 Evan Battaglia\nCopyright (c) 2008-2012 Viking's contributors\n", PACKAGE_NAME, PACKAGE_VERSION);
    return EXIT_SUCCESS;
  }

#if GLIB_CHECK_VERSION (2, 32, 0)
  if (vik_debug)
    g_log_set_handler (NULL, G_LOG_LEVEL_DEBUG, log_debug, NULL);
#else
  if (!vik_debug)
    g_log_set_handler (NULL, G_LOG_LEVEL_DEBUG, mute_log, NULL);
#endif

#if HAVE_X11_XLIB_H
  XSetErrorHandler(myXErrorHandler);
#endif

  // Discover if this is the very first run
  a_vik_very_first_run ();

  a_settings_init ();
  a_preferences_init ();

  a_vik_preferences_init ();

  a_layer_defaults_init ();

  a_download_init();
  curl_download_init();

  a_babel_init ();

  /* Init modules/plugins */
  modules_init();

  maps_layer_init ();
  a_mapcache_init ();
  a_background_init ();

#ifdef VIK_CONFIG_GEOCACHES
  a_datasource_gc_init();
#endif

  vik_routing_prefs_init();

  /* Set the icon */
  main_icon = gdk_pixbuf_from_pixdata(&viking_pixbuf, FALSE, NULL);
  gtk_window_set_default_icon(main_icon);

  gdk_threads_enter ();

  // Ask for confirmation of default settings on first run
  vu_set_auto_features_on_first_run ();

  /* Create the first window */
  first_window = vik_window_new_window();

  vu_check_latest_version ( GTK_WINDOW(first_window) );

  while ( ++i < argc ) {
    if ( strcmp(argv[i],"--") == 0 && !dashdash_already )
      dashdash_already = TRUE; /* hack to open '-' */
    else {
      VikWindow *newvw = first_window;
      gboolean change_filename = (i == 1);

      // Open any subsequent .vik files in their own window
      if ( i > 1 && check_file_magic_vik ( argv[i] ) ) {
        newvw = vik_window_new_window ();
        change_filename = TRUE;
      }

      vik_window_open_file ( newvw, argv[i], change_filename );
    }
  }

  vik_window_new_window_finish ( first_window );

  gtk_main ();
  gdk_threads_leave ();

  a_babel_uninit ();

  a_background_uninit ();
  a_mapcache_uninit ();
  a_dems_uninit ();
  a_layer_defaults_uninit ();
  a_preferences_uninit ();
  a_settings_uninit ();

  curl_download_uninit();

  return 0;
}
