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
#include <math.h>

#include "globals.h"
#include "preferences.h"
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

static gchar * params_degree_formats[] = {N_("DDD"), N_("DMM"), N_("DMS"), N_("Raw"), NULL};
static gchar * params_units_distance[] = {N_("Kilometres"), N_("Miles"), N_("Nautical Miles"), NULL};
static gchar * params_units_speed[] = {N_("km/h"), N_("mph"), N_("m/s"), N_("knots"), N_("s/km"), N_("min/km"), N_("s/mi"), N_("min/mi"), NULL};
static gchar * params_units_height[] = {N_("Metres"), N_("Feet"), NULL};
static gchar * params_units_temp[] = {N_("Celsius"), N_("Fahrenheit"), NULL};
static VikLayerParamScale params_scales_lat[] = { {-90.0, 90.0, 0.05, 2} };
static VikLayerParamScale params_scales_long[] = { {-180.0, 180.0, 0.05, 2} };
static gchar * params_time_ref_frame[] = {N_("Locale"), N_("World"), N_("UTC"), NULL};

static VikLayerParamData deg_format_default ( void ) { return VIK_LPD_UINT(VIK_DEGREE_FORMAT_DMS); }
// Maintain the default location to New York
static VikLayerParamData lat_default ( void ) { return VIK_LPD_DOUBLE(40.714490); }
static VikLayerParamData lon_default ( void ) { return VIK_LPD_DOUBLE(-74.007130); }

static VikLayerParam general_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "degree_format", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Degree format:"), VIK_LAYER_WIDGET_COMBOBOX, params_degree_formats, NULL, NULL, deg_format_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_distance", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Distance units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_distance, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_speed", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Speed units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_speed, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_height", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Height units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_height, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_temperature", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Temperature units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_temp, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use large waypoint icons:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "default_latitude", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Default latitude:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales_lat, NULL, NULL, lat_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "default_longitude", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Default longitude:"), VIK_LAYER_WIDGET_SPINBUTTON, params_scales_long, NULL, NULL, lon_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "time_reference_frame", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Time Display:"), VIK_LAYER_WIDGET_COMBOBOX, params_time_ref_frame, NULL,
    N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object."), NULL, NULL, NULL },
};

/* External/Export Options */

static gchar * params_kml_export_units[] = {"Metric", "Statute", "Nautical", NULL};
static gchar * params_gpx_export_trk_sort[] = {N_("Alphabetical"), N_("Time"), N_("Creation"), NULL };
static gchar * params_gpx_export_wpt_symbols[] = {N_("Title Case"), N_("Lowercase"), NULL};

static VikLayerParamData trk_sort_default ( void ) { return VIK_LPD_UINT(VIK_GPX_EXPORT_TRK_SORT_TIME); }
static VikLayerParamData ext_gpx_1_default ( void ) {
  VikLayerParamData data; data.s = g_strdup ( "josm" ); return data; }
static VikLayerParamData ext_gpx_2_default ( void ) {
  VikLayerParamData data; data.s = g_strdup ( "merkaartor" ); return data; }
#ifndef WINDOWS
static VikLayerParamData img_viewer_default ( void ) {
  VikLayerParamData data; data.s = g_strdup ( "xdg-open" ); return data; }
#endif

static VikLayerParam io_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "kml_export_units", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("KML File Export Units:"), VIK_LAYER_WIDGET_COMBOBOX, params_kml_export_units, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "kml_export_track", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("KML File Export Track:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Controls whether the <Track> tag is created"), vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "kml_export_points", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("KML File Export Points:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Controls whether placemarks are created for every trackpoint"), vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_track_sort", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("GPX Track Order:"), VIK_LAYER_WIDGET_COMBOBOX, params_gpx_export_trk_sort, NULL, NULL, trk_sort_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("GPX Waypoint Symbols:"), VIK_LAYER_WIDGET_COMBOBOX, params_gpx_export_wpt_symbols, NULL,
      N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices"), NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_creator", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("GPX Creator:"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL,
      N_("The creator value when writing a GPX file. Otherwise when blank a default is used."), NULL, NULL, NULL },
#ifndef WINDOWS
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "image_viewer", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Image Viewer:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL, NULL, img_viewer_default, NULL, NULL },
#endif
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_1", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 1:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL, NULL, ext_gpx_1_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_2", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 2:"), VIK_LAYER_WIDGET_FILEENTRY, NULL, NULL, NULL, ext_gpx_2_default, NULL, NULL },
};

static gchar * params_vik_fileref[] = {N_("Absolute"), N_("Relative"), NULL};
static VikLayerParamScale params_recent_files[] = { {-1, 25, 1, 0} };
static gchar * params_pos_type[] = {N_("None"), N_("Bottom"), N_("Middle"), N_("Top"), NULL};

// Seemingly GTK's default for the number of recent files
static VikLayerParamData rcnt_files_default ( void ) { return VIK_LPD_INT(10); }
static VikLayerParamData rlr_lbl_pos_default ( void ) { return VIK_LPD_UINT(VIK_POSITIONAL_MIDDLE); }

static VikLayerParam prefs_advanced[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "save_file_reference_mode", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Save File Reference Mode:"), VIK_LAYER_WIDGET_COMBOBOX, params_vik_fileref, NULL,
    N_("When saving a Viking .vik file, this determines how the directory paths of filenames are written."), NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "ask_for_create_track_name", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Ask for Name before Track Creation:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "create_track_tooltip", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Tooltip during Track Creation:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "trw_layer_show_graph", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Graph for TrackWaypoint Layer:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, N_("Show graph automatically for a track or route if only one is in the layer"), vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files", VIK_LAYER_PARAM_INT, VIK_LAYER_GROUP_NONE, N_("The number of recent files:"), VIK_LAYER_WIDGET_SPINBUTTON, params_recent_files, NULL,
    N_("Only applies to new windows or on application restart. -1 means all available files."), rcnt_files_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "open_files_in_selected_layer", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Open files in selected layer:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Open files (but not .vik ones) into the selected TrackWaypoint layer."), vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "calendar_show_day_names", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show calendar day names:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "ruler_area_label_position", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Ruler area label position:"), VIK_LAYER_WIDGET_COMBOBOX, params_pos_type, NULL, NULL, rlr_lbl_pos_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "use_scroll_to_zoom", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use Scroll to Zoom:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Whether scroll events zoom or move the viewport"), vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "invert_scroll_direction", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Invert Scroll Direction:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Invert direction of scrolling, particularly for touchpad use"), vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "select_tool_double_click_to_zoom", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Select Tool Double Click to Zoom:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "auto_trackpoint_select", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Auto Select Trackpoint:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Select trackpoint from mouse over graph on main display"), vik_lpd_true_default, NULL, NULL },
};

static gchar * params_startup_methods[] = {N_("Home Location"), N_("Last Location"), N_("Specified File"), N_("Auto Location"), NULL};

static VikLayerParamData highlight_color_default ( void ) {
  VikLayerParamData data;
  if ( a_vik_very_first_run() )
    // New purple default - for better contrast with the default map colorscheme
    (void)gdk_color_parse ( "#B809A0", &data.c );
  else
    // Orange - maintain the previous default
    (void)gdk_color_parse ( "#EEA500", &data.c );
  return data;
}

static VikLayerParam startup_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "restore_window_state", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Restore Window Setup:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Restore window size and layout"), NULL, NULL, NULL},
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Add a Default Map Layer:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values."), vik_lpd_false_default, NULL, NULL},
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "highlight_color", VIK_LAYER_PARAM_COLOR, VIK_LAYER_GROUP_NONE, N_("Highlight Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, highlight_color_default, NULL, NULL },
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

  a_preferences_register_group ( VIKING_PREFERENCES_GROUP_KEY, _("General") );
  for ( guint ii = 0; ii < G_N_ELEMENTS(general_prefs); ii++ )
    a_preferences_register ( &general_prefs[ii], (VikLayerParamData){0}, VIKING_PREFERENCES_GROUP_KEY );
  // New Tab
  a_preferences_register_group ( VIKING_PREFERENCES_STARTUP_GROUP_KEY, _("Startup") );
  for ( guint ii = 0; ii < G_N_ELEMENTS(startup_prefs); ii++ )
    a_preferences_register ( &startup_prefs[ii], (VikLayerParamData){0}, VIKING_PREFERENCES_STARTUP_GROUP_KEY );
  // New Tab
  a_preferences_register_group ( VIKING_PREFERENCES_IO_GROUP_KEY, _("Export/External") );
  for ( guint ii = 0; ii < G_N_ELEMENTS(io_prefs); ii++ )
    a_preferences_register ( &io_prefs[ii], (VikLayerParamData){0}, VIKING_PREFERENCES_IO_GROUP_KEY );
  // 'Advanced' Properties
  a_preferences_register_group ( VIKING_PREFERENCES_ADVANCED_GROUP_KEY, _("Advanced") );
  for ( guint ii = 0; ii < G_N_ELEMENTS(prefs_advanced); ii++ )
    a_preferences_register ( &prefs_advanced[ii], (VikLayerParamData){0}, VIKING_PREFERENCES_ADVANCED_GROUP_KEY );
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

vik_units_temp_t a_vik_get_units_temp ( )
{
  return a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_temperature")->u;
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

gboolean a_vik_get_kml_export_track ( )
{
  return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "kml_export_track")->b;
}

gboolean a_vik_get_kml_export_points ( )
{
  return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "kml_export_points")->b;
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
  val = a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names")->u;
  return val;
}

const gchar* a_vik_gpx_export_creator ( )
{
  return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_creator")->s;
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

gboolean a_vik_get_show_graph_for_trwlayer ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "trw_layer_show_graph")->b;
}

gint a_vik_get_recent_number_files ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files")->i;
}

gboolean a_vik_get_open_files_in_selected_layer ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "open_files_in_selected_layer")->b;
}

gboolean a_vik_get_calendar_show_day_names ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "calendar_show_day_names")->b;
}

vik_positional_t a_vik_get_ruler_area_label_pos ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "ruler_area_label_position")->u;
}

gboolean a_vik_get_invert_scroll_direction ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "invert_scroll_direction")->b;
}

gboolean a_vik_get_scroll_to_zoom ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "use_scroll_to_zoom")->b;
}

gboolean a_vik_get_select_double_click_to_zoom ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "select_tool_double_click_to_zoom")->b;
}

gboolean a_vik_get_auto_trackpoint_select ( )
{
  return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "auto_trackpoint_select")->b;
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

GdkColor a_vik_get_startup_highlight_color ( )
{
  return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "highlight_color")->c;
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
