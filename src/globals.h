/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef __VIKING_GLOBALS_H
#define __VIKING_GLOBALS_H

#include <glib.h>

G_BEGIN_DECLS

#define PROJECT "Viking"
#define VIKING_VERSION PACKAGE_VERSION
#define VIKING_VERSION_NAME "This Name For Rent"
#define VIKING_URL PACKAGE_URL

#define ALTI_TO_MPP 1.4017295
#define MPP_TO_ALTI 0.7134044

#define VIK_FEET_IN_METER 3.2808399
#define VIK_METERS_TO_FEET(X) ((X)*VIK_FEET_IN_METER)
#define VIK_FEET_TO_METERS(X) ((X)/VIK_FEET_IN_METER)

#define VIK_MILES_IN_METER 0.000621371192
#define VIK_METERS_TO_MILES(X) ((X)*VIK_MILES_IN_METER)
#define VIK_MILES_TO_METERS(X) ((X)/VIK_MILES_IN_METER)

/* MPS - Metres Per Second */
/* MPH - Metres Per Hour */
#define VIK_MPH_IN_MPS 2.23693629
#define VIK_MPS_TO_MPH(X) ((X)*VIK_MPH_IN_MPS)
#define VIK_MPH_TO_MPS(X) ((X)/VIK_MPH_IN_MPS)

/* KPH - Kilometres Per Hour */
#define VIK_KPH_IN_MPS 3.6
#define VIK_MPS_TO_KPH(X) ((X)*VIK_KPH_IN_MPS)
#define VIK_KPH_TO_MPS(X) ((X)/VIK_KPH_IN_MPS)

#define VIK_KNOTS_IN_MPS 1.94384449
#define VIK_MPS_TO_KNOTS(X) ((X)*VIK_KNOTS_IN_MPS)
#define VIK_KNOTS_TO_MPS(X) ((X)/VIK_KNOTS_IN_MPS)

#define VIK_DEFAULT_ALTITUDE 0.0
#define VIK_DEFAULT_DOP 0.0

#define VIK_GTK_WINDOW_FROM_WIDGET(x) GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(x)))
#define VIK_GTK_WINDOW_FROM_LAYER(x) VIK_GTK_WINDOW_FROM_WIDGET(VIK_LAYER(x)->vt)

#define DEG2RAD(x) ((x)*(M_PI/180))
#define RAD2DEG(x) ((x)*(180/M_PI))

/* mercator projection, latitude conversion (degrees) */
#define MERCLAT(x) (RAD2DEG(log(tan((0.25 * M_PI) + (0.5 * DEG2RAD(x))))))
#define DEMERCLAT(x) (RAD2DEG(atan(sinh(DEG2RAD(x)))))

/* Some command line options */
extern gboolean vik_debug;
extern gboolean vik_verbose;
extern gboolean vik_version;

/* Allow comparing versions */
gint viking_version_to_number ( gchar *version );

/* Very first run */
gboolean a_vik_very_first_run ();

/* Global preferences */
void a_vik_preferences_init ();

/* Coord display preferences */
typedef enum {
  VIK_DEGREE_FORMAT_DDD,
  VIK_DEGREE_FORMAT_DMM,
  VIK_DEGREE_FORMAT_DMS,
} vik_degree_format_t;

vik_degree_format_t a_vik_get_degree_format ( );

/* Distance preferences */
typedef enum {
  VIK_UNITS_DISTANCE_KILOMETRES,
  VIK_UNITS_DISTANCE_MILES,
} vik_units_distance_t;

vik_units_distance_t a_vik_get_units_distance ( );

/* Speed preferences */
typedef enum {
  VIK_UNITS_SPEED_KILOMETRES_PER_HOUR,
  VIK_UNITS_SPEED_MILES_PER_HOUR,
  VIK_UNITS_SPEED_METRES_PER_SECOND,
  VIK_UNITS_SPEED_KNOTS,
} vik_units_speed_t;

vik_units_speed_t a_vik_get_units_speed ( );

/* Height (Depth) preferences */
typedef enum {
  VIK_UNITS_HEIGHT_METRES,
  VIK_UNITS_HEIGHT_FEET,
} vik_units_height_t;

vik_units_height_t a_vik_get_units_height ( );

gboolean a_vik_get_use_large_waypoint_icons ( );

/* Location preferences */
typedef enum {
  VIK_LOCATION_LAT,
  VIK_LOCATION_LONG,
} vik_location_t;

gdouble a_vik_get_default_lat ( );
gdouble a_vik_get_default_long ( );

/* KML export preferences */
typedef enum {
  VIK_KML_EXPORT_UNITS_METRIC,
  VIK_KML_EXPORT_UNITS_STATUTE,
  VIK_KML_EXPORT_UNITS_NAUTICAL,
} vik_kml_export_units_t;

vik_kml_export_units_t a_vik_get_kml_export_units ( );

typedef enum {
  VIK_GPX_EXPORT_TRK_SORT_ALPHA,
  VIK_GPX_EXPORT_TRK_SORT_TIME,
  VIK_GPX_EXPORT_TRK_SORT_CREATION,
} vik_gpx_export_trk_sort_t;

vik_gpx_export_trk_sort_t a_vik_get_gpx_export_trk_sort ( );

typedef enum {
  VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE,
  VIK_GPX_EXPORT_WPT_SYM_NAME_LOWERCASE,
} vik_gpx_export_wpt_sym_name_t;

vik_gpx_export_wpt_sym_name_t a_vik_gpx_export_wpt_sym_name ( );

#ifndef WINDOWS
/* Windows automatically uses the system defined viewer
   ATM for other systems need to specify the program to use */
const gchar* a_vik_get_image_viewer ( );
#endif

const gchar* a_vik_get_external_gpx_program_1 ( );

const gchar* a_vik_get_external_gpx_program_2 ( );

/* File reference preferences - mainly in saving of a viking file */
typedef enum {
  VIK_FILE_REF_FORMAT_ABSOLUTE,
  VIK_FILE_REF_FORMAT_RELATIVE,
} vik_file_ref_format_t;

vik_file_ref_format_t a_vik_get_file_ref_format ( );

gboolean a_vik_get_create_track_tooltip ( );

gboolean a_vik_get_restore_window_state ( );

gboolean a_vik_get_add_default_map_layer ( );

typedef enum {
  VIK_STARTUP_METHOD_HOME_LOCATION,
  VIK_STARTUP_METHOD_LAST_LOCATION,
  VIK_STARTUP_METHOD_SPECIFIED_FILE,
  VIK_STARTUP_METHOD_AUTO_LOCATION,
} vik_startup_method_t;

vik_startup_method_t a_vik_get_startup_method ( );

const gchar *a_vik_get_startup_file ( );

gboolean a_vik_get_check_version ( );

/* Group for global preferences */
#define VIKING_PREFERENCES_GROUP_KEY "viking.globals"
#define VIKING_PREFERENCES_NAMESPACE "viking.globals."

/* Another group of global preferences,
  but in a separate section to try to keep things organized */
/* AKA Export/External Prefs */
#define VIKING_PREFERENCES_IO_GROUP_KEY "viking.io"
#define VIKING_PREFERENCES_IO_NAMESPACE "viking.io."

/* Group for global preferences - but 'advanced'
   User changeable but only for those that need it */
#define VIKING_PREFERENCES_ADVANCED_GROUP_KEY "viking.advanced"
#define VIKING_PREFERENCES_ADVANCED_NAMESPACE "viking.advanced."

#define VIKING_PREFERENCES_STARTUP_GROUP_KEY "viking.startup"
#define VIKING_PREFERENCES_STARTUP_NAMESPACE "viking.startup."

G_END_DECLS

#endif
