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

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "util.h"
#include "dialog.h"
#include "globals.h"
#include "download.h"
#include "preferences.h"
#include "vikmapslayer.h"
#include "settings.h"

/*
#ifdef WINDOWS
#include <windows.h>
#endif

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

void open_url(GtkWindow *parent, const gchar * url)
{
  GError *error = NULL;
  gtk_show_uri ( gtk_widget_get_screen (GTK_WIDGET(parent)), url, GDK_CURRENT_TIME, &error );
  if ( error ) {
    a_dialog_error_msg_extra ( parent, _("Could not launch web browser. %s"), error->message );
    g_error_free ( error );
  }
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

/**
 * split_string_from_file_on_equals:
 *
 * @buf: the input string
 * @key: newly allocated string that is before the '='
 * @val: newly allocated string after the '='
 *
 * Designed for file line processing, so it ignores strings beginning with special
 *  characters, such as '#'; returns false in such situations.
 * Also returns false if no equals character is found
 *
 * e.g. if buf = "GPS.parameter=42"
 *   key = "GPS.parameter"
 *   val = "42"
 */
gboolean split_string_from_file_on_equals ( const gchar *buf, gchar **key, gchar **val )
{
  // comments, special characters in viking file format
  if ( buf == NULL || buf[0] == '\0' || buf[0] == '~' || buf[0] == '=' || buf[0] == '#' )
    return FALSE;

  if ( ! strchr ( buf, '=' ) )
    return FALSE;

  gchar **strv = g_strsplit ( buf, "=", 2 );

  gint gi = 0;
  gchar *str = strv[gi];
  while ( str ) {
	if ( gi == 0 )
	  *key = g_strdup ( str );
	else
	  *val = g_strdup ( str );
    gi++;
    str = strv[gi];
  }

  g_strfreev ( strv );

  // Remove newline from val and also any other whitespace
  *key = g_strstrip ( *key );
  *val = g_strstrip ( *val );

  return TRUE;
}

/* 1 << (x) is like a 2**(x) */
#define GZ(x) (1<<(x))

static const gdouble scale_mpps[] = { 0.125, 0.25, 0.5, GZ(0), GZ(1), GZ(2), GZ(3), GZ(4), GZ(5), GZ(6), GZ(7), GZ(8), GZ(9),
                                      GZ(10), GZ(11), GZ(12), GZ(13), GZ(14), GZ(15), GZ(16), GZ(17) };

static const gint num_scales = (sizeof(scale_mpps) / sizeof(scale_mpps[0]));

#define ERROR_MARGIN 0.01
/**
 * mpp_to_zoom:
 *
 * Returns: a zoom level. see : http://wiki.openstreetmap.org/wiki/Zoom_levels
 */
guint8 mpp_to_zoom ( gdouble mpp )
{
  gint i;
  for ( i = 0; i < num_scales; i++ ) {
    if ( ABS(scale_mpps[i] - mpp) < ERROR_MARGIN ) {
      g_debug ( "mpp_to_zoom: %f -> %d", mpp, i );
      return 20-i;
    }
  }
  return 17; // a safe zoomed in default
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

  g_free ( nvtd->version );
  g_free ( nvtd );
  return FALSE;
}

#define VIK_SETTINGS_VERSION_CHECKED_DATE "version_checked_date"

static void latest_version_thread ( GtkWindow *window )
{
  // Need to allow a few redirects, as SF file is often served from different server
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

  // Update last checked time
  GTimeVal time;
  g_get_current_time ( &time );
  a_settings_set_string ( VIK_SETTINGS_VERSION_CHECKED_DATE, g_time_val_to_iso8601(&time) );
}

#define VIK_SETTINGS_VERSION_CHECK_PERIOD "version_check_period_days"

/**
 * check_latest_version:
 * @window: Somewhere where we may need use the display to inform the user about the version status
 *
 * Periodically checks the released latest VERSION file on the website to compare with the running version
 *
 */
void check_latest_version ( GtkWindow *window )
{
  if ( ! a_vik_get_check_version () )
    return;

  gboolean do_check = FALSE;

  gint check_period;
  if ( ! a_settings_get_integer ( VIK_SETTINGS_VERSION_CHECK_PERIOD, &check_period ) ) {
    check_period = 14;
  }

  // Get last checked date...
  GDate *gdate_last = g_date_new();
  GDate *gdate_now = g_date_new();
  GTimeVal time_last;
  gchar *last_checked_date = NULL;

  // When no previous date available - set to do the version check
  if ( a_settings_get_string ( VIK_SETTINGS_VERSION_CHECKED_DATE, &last_checked_date) ) {
    if ( g_time_val_from_iso8601 ( last_checked_date, &time_last ) ) {
      g_date_set_time_val ( gdate_last, &time_last );
    }
    else
      do_check = TRUE;
  }
  else
    do_check = TRUE;

  GTimeVal time_now;
  g_get_current_time ( &time_now );
  g_date_set_time_val ( gdate_now, &time_now );

  if ( ! do_check ) {
    // Dates available so do the comparison
    g_date_add_days ( gdate_last, check_period );
    if ( g_date_compare ( gdate_last, gdate_now ) < 0 )
      do_check = TRUE;
  }

  g_date_free ( gdate_last );
  g_date_free ( gdate_now );

  if ( do_check ) {
#if GLIB_CHECK_VERSION (2, 32, 0)
    g_thread_try_new ( "latest_version_thread", (GThreadFunc)latest_version_thread, window, NULL );
#else
    g_thread_create ( (GThreadFunc)latest_version_thread, window, FALSE, NULL );
#endif
  }
}

/**
 * set_auto_features_on_first_run:
 *
 *  Ask the user's opinion to set some of Viking's default behaviour
 */
void set_auto_features_on_first_run ( void )
{
  gboolean auto_features = FALSE;
  if ( a_vik_very_first_run () ) {

    GtkWidget *win = gtk_window_new ( GTK_WINDOW_TOPLEVEL );

    if ( a_dialog_yes_or_no ( GTK_WINDOW(win),
                              _("This appears to be Viking's very first run.\n\nDo you wish to enable automatic internet features?\n\nIndividual settings can be controlled in the Preferences."), NULL ) )
      auto_features = TRUE;
  }

  if ( auto_features ) {
    // Set Maps to autodownload
    // Ensure the default is true
    maps_layer_set_autodownload_default ( TRUE );

    // Simplistic repeat of preference settings
    //  Only the name & type are important for setting a preference via this 'external' way

    // Enable auto add map +
    // Enable IP lookup
    VikLayerParam pref_add_map[] = { { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, NULL, VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, NULL, }, };
    VikLayerParam pref_startup_method[] = { { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, NULL, VIK_LAYER_WIDGET_COMBOBOX, NULL, NULL, NULL, NULL, }, };

    VikLayerParamData vlp_data;
    vlp_data.b = TRUE;
    a_preferences_run_setparam ( vlp_data, pref_add_map );

    vlp_data.u = VIK_STARTUP_METHOD_AUTO_LOCATION;
    a_preferences_run_setparam ( vlp_data, pref_startup_method );

    // Only on Windows make checking for the latest version on by default
    // For other systems it's expected a Package manager or similar controls the installation, so leave it off
#ifdef WINDOWS
    VikLayerParam pref_startup_version_check[] = { { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, NULL, VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, NULL, }, };
    vlp_data.b = TRUE;
    a_preferences_run_setparam ( vlp_data, pref_startup_version_check );
#endif

    // Ensure settings are saved for next time
    a_preferences_save_to_file ();
  }
}
