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
#include "logging.h"
#include "vikmapslayer.h"
#include "vikgeoreflayer.h"
#include "viktrwlayer_propwin.h"
#include "vikrouting.h"
#include "toolbar.h"
#include "thumbnails.h"
#include "viktrwlayer_export.h"
#include "modules.h"

/* FIXME LOCALEDIR must be configured by ./configure --localedir */
/* But something does not work actually. */
/* So, we need to redefine this variable on windows. */
#ifdef WINDOWS
#undef LOCALEDIR
#define LOCALEDIR "locale"
#endif

// Default values that won't actually get applied unless changed by command line parameter values
static gdouble latitude = 0.0;
static gdouble longitude = 0.0;
static gint zoom_level_osm = -1;
static gint map_id = -1;
static gboolean external = FALSE;

/* Options */
static GOptionEntry entries[] = 
{
  { "debug", 'd', 0, G_OPTION_ARG_NONE, &vik_debug, N_("Enable debug output"), NULL },
  { "verbose", 'V', 0, G_OPTION_ARG_NONE, &vik_verbose, N_("Enable verbose output"), NULL },
  { "version", 'v', 0, G_OPTION_ARG_NONE, &vik_version, N_("Show version"), NULL },
  { "latitude", 0, 0, G_OPTION_ARG_DOUBLE, &latitude, N_("Latitude in decimal degrees"), NULL },
  { "longitude", 0, 0, G_OPTION_ARG_DOUBLE, &longitude, N_("Longitude in decimal degrees"), NULL },
  { "zoom", 'z', 0, G_OPTION_ARG_INT, &zoom_level_osm, N_("Zoom Level (OSM). Value can be 0 - 22"), NULL },
  { "map", 'm', 0, G_OPTION_ARG_INT, &map_id, N_("Add a map layer by id value. Use 0 for the default map."), NULL },
  { "external", 'e', 0, G_OPTION_ARG_NONE, &external, N_("Load all GPX files in external mode."), NULL },
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

  gui_initialized = gtk_init_with_args (&argc, &argv, "files+", entries, NULL, &error);
  if (!gui_initialized)
  {
    /* check if we have an error message */
    if (error == NULL)
    {
      /* no error message, the GUI initialization failed */
      const gchar *display_name = gdk_get_display_arg_name ();
      (void)g_fprintf (stderr, "Failed to open display: %s\n", (display_name != NULL) ? display_name : " ");
    }
    else
    {
      /* yep, there's an error, so print it */
      (void)g_fprintf (stderr, "Parsing command line options failed: %s\n", error->message);
      g_error_free (error);
      (void)g_fprintf (stderr, "Run \"%s --help\" to see the list of recognized options.\n",argv[0]);
    }
    return EXIT_FAILURE;
  }
   
  if (vik_version)
  {
    (void)g_printf (_("%s %s\nCopyright (c) 2003-2008 Evan Battaglia\nCopyright (c) 2008-%s Viking's contributors\n"), PACKAGE_NAME, PACKAGE_VERSION, THEYEAR);
    return EXIT_SUCCESS;
  }

  // Ensure correct capitalization of the program name
  g_set_application_name ("Viking");

  a_logging_init ();

  // Discover if this is the very first run
  a_vik_very_first_run ();

  a_settings_init ();
  a_preferences_init ();
  a_thumbnails_init ();

 /*
  * First stage initialization
  *
  * Should not use a_preferences_get() yet
  *
  * Since the first time a_preferences_get() is called it loads any preferences values from disk,
  *  but of course for preferences not registered yet it can't actually understand them
  *  so subsequent initial attempts to get those preferences return the default value, until the values have changed
  */
  a_vik_preferences_init ();

  a_layer_defaults_init ();

  a_download_init();
  curl_download_init();

  a_babel_init ();

  /* Init modules/plugins */
  modules_init();

  vik_georef_layer_init ();
  maps_layer_init ();
  a_mapcache_init ();
  a_background_init ();

  a_toolbar_init();
  vik_routing_prefs_init();
  vik_trw_layer_export_init();
  vik_trw_layer_propwin_init();

  // Registration of preferences has now been done
  a_preferences_finished_registering();

  /*
   * Second stage initialization
   *
   * Can now use a_preferences_get()
   */
  a_background_post_init ();
  a_babel_post_init ();
  modules_post_init ();

  // May need to initialize the Positonal TimeZone lookup
  if ( a_vik_get_time_ref_frame() == VIK_TIME_REF_WORLD )
    vu_setup_lat_lon_tz_lookup();

  /* Set the icon */
  main_icon = gdk_pixbuf_from_pixdata(&viking_pixbuf, FALSE, NULL);
  gtk_window_set_default_icon(main_icon);

  // Ask for confirmation of default settings on first run
  vu_set_auto_features_on_first_run ();

  /* Create the first window */
  first_window = vik_window_new_window();

  a_logging_update();

  vu_check_latest_version ( GTK_WINDOW(first_window) );

  // Load startup file first so that subsequent files are loaded on top
  // Especially so that new tracks+waypoints will be above any maps in a startup file
  if ( a_vik_get_startup_method () == VIK_STARTUP_METHOD_SPECIFIED_FILE ) {
    gboolean load_startup_file = TRUE;
    // When a viking file is to be loaded via the command line
    //  then we'll skip loading any startup file
    int jj = 0;
    while ( ++jj < argc ) {
      if ( check_file_magic_vik(argv[jj]) )
        load_startup_file = FALSE;
    }
    if ( load_startup_file )
      vik_window_open_file ( first_window, a_vik_get_startup_file(), TRUE, TRUE, TRUE, TRUE, FALSE );
  }

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

      vik_window_open_file ( newvw, argv[i], change_filename, (i==1), (i+1 == argc), TRUE, external );
    }
  }

  vik_window_new_window_finish ( first_window );

  vu_command_line ( first_window, latitude, longitude, zoom_level_osm, map_id );

  gtk_main ();

  vik_trwlayer_uninit ();
  a_babel_uninit ();
  a_toolbar_uninit ();
  a_background_uninit ();
  a_mapcache_uninit ();
  a_dems_uninit ();
  a_layer_defaults_uninit ();
  a_thumbnails_uninit ();
  a_preferences_uninit ();
  a_settings_uninit ();

  modules_uninit();

  curl_download_uninit();

  vu_finalize_lat_lon_tz_lookup ();

  // Clean up any temporary files
  util_remove_all_in_deletion_list ();

  return 0;
}
