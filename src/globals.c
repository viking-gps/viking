/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2010-2013, Rob Norris <rw_norris@hotmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib/gi18n.h>

#include "globals.h"
#include "preferences.h"
#include "math.h"
#include "dir.h"

gboolean vik_debug = FALSE;
gboolean vik_verbose = FALSE;
gboolean vik_version = FALSE;

/**
 * viking_version_to_number:
 * @version:  The string of the Viking version.
 *            This should be in the form of N.N.N.N, where the 3rd + 4th numbers are optional
 *            Often you'll want to pass in VIKING_VERSION
 *
 * Returns: a single number useful for comparison
 */
gint viking_version_to_number ( gchar *version )
{
  // Basic method, probably can be improved
  gint version_number = 0;
  gchar** parts = g_strsplit ( version, ".", 0 );
  gint part_num = 0;
  gchar *part = parts[part_num];
  // Allow upto 4 parts to the version number
  while ( part && part_num < 4 ) {
    // Allow each part to have upto 100
    version_number = version_number + ( atol(part) * pow(100, 3-part_num) );
    part_num++;
    part = parts[part_num];
  }
  g_strfreev ( parts );
  return version_number;
}

static gchar * params_degree_formats[] = {"DDD", "DMM", "DMS", N_("Raw"), NULL};
static gchar * params_units_distance[] = {N_("Kilometres"), N_("Miles"), N_("Nautical Miles"), NULL};
static gchar * params_units_speed[] = {"km/h", "mph", "m/s", "knots", NULL};
static gchar * params_units_height[] = {"Metres", "Feet", NULL};
static VikLayerParamScale params_scales_lat[] = { {-90.0, 90.0, 0.05, 2} };
static VikLayerParamScale params_scales_long[] = { {-180.0, 180.0, 0.05, 2} };
static gchar * params_time_ref_frame[] = {N_("Locale"), N_("World"), N_("UTC"), NULL};

static VikLayerParam general_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "degree_format", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Degree format:"), VIK_LAYER_WIDGET_COMBOBOX, params_degree_formats, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_distance", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Distance units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_distance, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_speed", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Speed units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_speed, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_height", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Height units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_height, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use large waypoint icons:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "default_latitude", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Default latitude:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales_lat, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "default_longitude", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Default longitude:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales_long, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "time_reference_frame", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Time Display:"), VIK_LAYER_WIDGET_COMBOBOX, params_time_ref_frame, NULL,
    N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object."), NULL, NULL, NULL },
};

/* External/Export Options */

static gchar * params_kml_export_units[] = {"Metric", "Statute", "Nautical", NULL};
static gchar * params_gpx_export_trk_sort[] = {N_("Alphabetical"), N_("Time"), N_("Creation"), NULL };
static gchar * params_gpx_export_wpt_symbols[] = {N_("Title Case"), N_("Lowercase"), NULL};

static VikLayerParam io_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "kml_export_units", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("KML File Export Units:"), VIK_LAYER_WIDGET_COMBOBOX, params_kml_export_units, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_track_sort", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("GPX Track Order:"), VIK_LAYER_WIDGET_COMBOBOX, params_gpx_export_trk_sort, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("GPX Waypoint Symbols:"), VIK_LAYER_WIDGET_COMBOBOX, params_gpx_export_wpt_symbols, NULL,
      N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices"), NULL, NULL, NULL },
};

#ifndef WINDOWS
static VikLayerParam io_prefs_non_windows[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "image_viewer", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Image Viewer:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};
#endif

static VikLayerParam io_prefs_external_gpx[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_1", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 1:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_2", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 2:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};

static gchar * params_vik_fileref[] = {N_("Absolute"), N_("Relative"), NULL};
static VikLayerParamScale params_recent_files[] = { {-1, 25, 1, 0} };

static VikLayerParam prefs_advanced[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "save_file_reference_mode", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Save File Reference Mode:"), VIK_LAYER_WIDGET_COMBOBOX, params_vik_fileref, NULL,
    N_("When saving a Viking .vik file, this determines how the directory paths of filenames are written."), NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "ask_for_create_track_name", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Ask for Name before Track Creation:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "create_track_tooltip", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Tooltip during Track Creation:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files", VIK_LAYER_PARAM_INT, VIK_LAYER_GROUP_NONE, N_("The number of recent files:"), VIK_LAYER_WIDGET_SPINBUTTON, params_recent_files, NULL,
    N_("Only applies to new windows or on application restart. -1 means all available files."), NULL, NULL, NULL },
};

static gchar * params_startup_methods[] = {N_("Home Location"), N_("Last Location"), N_("Specified File"), N_("Auto Location"), NULL};

static VikLayerParam startup_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "restore_window_state", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Restore Window Setup:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Restore window size and layout"), NULL, NULL, NULL},
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Add a Default Map Layer:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values."), NULL, NULL, NULL},
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Startup Method:"), VIK_LAYER_WIDGET_COMBOBOX, params_startup_methods, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_file", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Startup File:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL,
    N_("The default file to load on startup. Only applies when the startup method is set to 'Specified File'"), NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Check For New Version:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Periodically check to see if a new version of Viking is available"), NULL, NULL, NULL },
};
/* End of Options static stuff */

/**
 * Detect when Viking is run for the very first time
 * Call this very early in the startup sequence to ensure subsequent correct results
 * The return value is cached, since later on the test will no longer be true
 */
gboolean a_vik_very_first_run ()
{
  static gboolean vik_very_first_run_known = FALSE;
  static gboolean vik_very_first_run = FALSE;

  // use cached result if available
  if ( vik_very_first_run_known )
    return vik_very_first_run;

  gchar *dir = a_get_viking_dir_no_create();
  // NB: will need extra logic if default dir gets changed e.g. from ~/.viking to ~/.config/viking
  if ( dir ) {
    // If directory exists - Viking has been run before
    vik_very_first_run = ! g_file_test ( dir, G_FILE_TEST_EXISTS );
    g_free ( dir );
  }
  else
    vik_very_first_run = TRUE;
  vik_very_first_run_known = TRUE;

  return vik_very_first_run;
}

void a_vik_preferences_init ()
{
  g_debug ( "VIKING VERSION as number: %d", viking_version_to_number (VIKING_VERSION) );

  // Defaults for the options are setup here
  a_preferences_register_group ( VIKING_PREFERENCES_GROUP_KEY, _("General") );

  VikLayerParamData tmp;
  tmp.u = VIK_DEGREE_FORMAT_DMS;
  a_preferences_register(&general_prefs[0], tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.u = VIK_UNITS_DISTANCE_KILOMETRES;
  a_preferences_register(&general_prefs[1], tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.u = VIK_UNITS_SPEED_KILOMETRES_PER_HOUR;
  a_preferences_register(&general_prefs[2], tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.u = VIK_UNITS_HEIGHT_METRES;
  a_preferences_register(&general_prefs[3], tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.b = TRUE;
  a_preferences_register(&general_prefs[4], tmp, VIKING_PREFERENCES_GROUP_KEY);

  /* Maintain the default location to New York */
  tmp.d = 40.714490;
  a_preferences_register(&general_prefs[5], tmp, VIKING_PREFERENCES_GROUP_KEY);
  tmp.d = -74.007130;
  a_preferences_register(&general_prefs[6], tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.u = VIK_TIME_REF_LOCALE;
  a_preferences_register(&general_prefs[7], tmp, VIKING_PREFERENCES_GROUP_KEY);

  // New Tab
  a_preferences_register_group ( VIKING_PREFERENCES_STARTUP_GROUP_KEY, _("Startup") );

  tmp.b = FALSE;
  a_preferences_register(&startup_prefs[0], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

  tmp.b = FALSE;
  a_preferences_register(&startup_prefs[1], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

  tmp.u = VIK_STARTUP_METHOD_HOME_LOCATION;
  a_preferences_register(&startup_prefs[2], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

  tmp.s = "";
  a_preferences_register(&startup_prefs[3], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

  tmp.b = FALSE;
  a_preferences_register(&startup_prefs[4], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

  // New Tab
  a_preferences_register_group ( VIKING_PREFERENCES_IO_GROUP_KEY, _("Export/External") );

  tmp.u = VIK_KML_EXPORT_UNITS_METRIC;
  a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

  tmp.u = VIK_GPX_EXPORT_TRK_SORT_TIME;
  a_preferences_register(&io_prefs[1], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

  tmp.b = VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE;
  a_preferences_register(&io_prefs[2], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

#ifndef WINDOWS
  tmp.s = "xdg-open";
  a_preferences_register(&io_prefs_non_windows[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
#endif

  // JOSM for OSM editing around a GPX track
  tmp.s = "josm";
  a_preferences_register(&io_prefs_external_gpx[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
  // Add a second external program - another OSM editor by default
  tmp.s = "merkaartor";
  a_preferences_register(&io_prefs_external_gpx[1], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

  // 'Advanced' Properties
  a_preferences_register_group ( VIKING_PREFERENCES_ADVANCED_GROUP_KEY, _("Advanced") );

  tmp.u = VIK_FILE_REF_FORMAT_ABSOLUTE;
  a_preferences_register(&prefs_advanced[0], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

  tmp.b = TRUE;
  a_preferences_register(&prefs_advanced[1], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

  tmp.b = TRUE;
  a_preferences_register(&prefs_advanced[2], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

  tmp.i = 10; // Seemingly GTK's default for the number of recent files
  a_preferences_register(&prefs_advanced[3], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);
}

vik_degree_format_t a_vik_get_degree_format ( )
{
  vik_degree_format_t format;
  format = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "degree_format")->u;
  return format;
}

vik_units_distance_t a_vik_get_units_distance ( )
{
  vik_units_distance_t units;
  units = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_distance")->u;
  return units;
}

vik_units_speed_t a_vik_get_units_speed ( )
{
  vik_units_speed_t units;
  units = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_speed")->u;
  return units;
}

vik_units_height_t a_vik_get_units_height ( )
{
  vik_units_height_t units;
  units = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_height")->u;
  return units;
}

gboolean a_vik_get_use_large_waypoint_icons ( )
{
  gboolean use_large_waypoint_icons;
  use_large_waypoint_icons = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons")->b;
  return use_large_waypoint_icons;
}

gdouble a_vik_get_default_lat ( )
{
  gdouble data;
  data = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "default_latitude")->d;
  return data;
}

gdouble a_vik_get_default_long ( )
{
  gdouble data;
  data = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "default_longitude")->d;
  return data;
}

vik_time_ref_frame_t a_vik_get_time_ref_frame ( )
{
  return a_preferences_get(VIKING_PREFERENCES_NAMESPACE "time_reference_frame")->u;
}

/* External/Export Options */

vik_kml_export_units_t a_vik_get_kml_export_units ( )
{
  vik_kml_export_units_t units;
  units = a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "kml_export_units")->u;
  return units;
}

vik_gpx_export_trk_sort_t a_vik_get_gpx_export_trk_sort ( )
{
  vik_gpx_export_trk_sort_t sort;
  sort = a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_track_sort")->u;
  return sort;
}

vik_gpx_export_wpt_sym_name_t a_vik_gpx_export_wpt_sym_name ( )
{
  gboolean val;
  val = a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names")->b;
  return val;
}

#ifndef WINDOWS
const gchar* a_vik_get_image_viewer ( )
{
  return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "image_viewer")->s;
}
#endif

const gchar* a_vik_get_external_gpx_program_1 ( )
{
  return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_1")->s;
}

const gchar* a_vik_get_external_gpx_program_2 ( )
{
  return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_2")->s;
}

// Advanced Options
vik_file_ref_format_t a_vik_get_file_ref_format ( )
{
  vik_file_ref_format_t format;
  format = a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "save_file_reference_mode")->u;
  return format;
}

gboolean a_vik_get_ask_for_create_track_name ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "ask_for_create_track_name")->b;
}

gboolean a_vik_get_create_track_tooltip ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "create_track_tooltip")->b;
}

gint a_vik_get_recent_number_files ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files")->i;
}

// Startup Options
gboolean a_vik_get_restore_window_state ( )
{
  gboolean data;
  data = a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "restore_window_state")->b;
  return data;
}

gboolean a_vik_get_add_default_map_layer ( )
{
  gboolean data;
  data = a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer")->b;
  return data;
}

vik_startup_method_t a_vik_get_startup_method ( )
{
  vik_startup_method_t data;
  data = a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method")->u;
  return data;
}

const gchar *a_vik_get_startup_file ( )
{
  return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_file")->s;
}

gboolean a_vik_get_check_version ( )
{
  return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version")->b;
}
