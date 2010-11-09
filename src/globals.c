/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2010, Rob Norris <rw_norris@hotmail.com>
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

#include <glib/gi18n.h>

#include "globals.h"
#include "preferences.h"

gboolean vik_debug = FALSE;
gboolean vik_verbose = FALSE;
gboolean vik_version = FALSE;

static gchar * params_degree_formats[] = {"DDD", "DMM", "DMS", NULL};
static gchar * params_units_distance[] = {"Kilometres", "Miles", NULL};
static gchar * params_units_speed[] = {"km/h", "mph", "m/s", "knots", NULL};
static gchar * params_units_height[] = {"Metres", "Feet", NULL};
static VikLayerParamScale params_scales_lat[] = { {-90.0, 90.0, 0.05, 2} };
static VikLayerParamScale params_scales_long[] = { {-180.0, 180.0, 0.05, 2} };
 
static VikLayerParam prefs1[] = {
  { VIKING_PREFERENCES_NAMESPACE "degree_format", VIK_LAYER_PARAM_UINT, VIK_DEGREE_FORMAT_DMS, N_("Degree format:"), VIK_LAYER_WIDGET_COMBOBOX, params_degree_formats, NULL },
};

static VikLayerParam prefs2[] = {
  { VIKING_PREFERENCES_NAMESPACE "units_distance", VIK_LAYER_PARAM_UINT, VIK_UNITS_DISTANCE_KILOMETRES, N_("Distance units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_distance, NULL },
};

static VikLayerParam prefs3[] = {
  { VIKING_PREFERENCES_NAMESPACE "units_speed", VIK_LAYER_PARAM_UINT, VIK_UNITS_SPEED_KILOMETRES_PER_HOUR, N_("Speed units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_speed, NULL },
};

static VikLayerParam prefs4[] = {
  { VIKING_PREFERENCES_NAMESPACE "units_height", VIK_LAYER_PARAM_UINT, VIK_UNITS_HEIGHT_METRES, N_("Height units:"), VIK_LAYER_WIDGET_COMBOBOX, params_units_height, NULL },
};

static VikLayerParam prefs5[] = {
  { VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons", VIK_LAYER_PARAM_BOOLEAN, TRUE, N_("Use large waypoint icons:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL },
};

static VikLayerParam prefs6[] = {
  { VIKING_PREFERENCES_NAMESPACE "default_latitude", VIK_LAYER_PARAM_DOUBLE, VIK_LOCATION_LAT, N_("Default latitude:"),  VIK_LAYER_WIDGET_SPINBUTTON, params_scales_lat, NULL },
};
static VikLayerParam prefs7[] = {
  { VIKING_PREFERENCES_NAMESPACE "default_longitude", VIK_LAYER_PARAM_DOUBLE, VIK_LOCATION_LONG, N_("Default longitude:"),  VIK_LAYER_WIDGET_SPINBUTTON, params_scales_long, NULL },
};

void a_vik_preferences_init ()
{
  a_preferences_register_group ( VIKING_PREFERENCES_GROUP_KEY, "Global preferences" );

  VikLayerParamData tmp;
  tmp.u = VIK_DEGREE_FORMAT_DMS;
  a_preferences_register(prefs1, tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.u = VIK_UNITS_DISTANCE_KILOMETRES;
  a_preferences_register(prefs2, tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.u = VIK_UNITS_SPEED_KILOMETRES_PER_HOUR;
  a_preferences_register(prefs3, tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.u = VIK_UNITS_HEIGHT_METRES;
  a_preferences_register(prefs4, tmp, VIKING_PREFERENCES_GROUP_KEY);

  tmp.b = TRUE;
  a_preferences_register(prefs5, tmp, VIKING_PREFERENCES_GROUP_KEY);

  /* Maintain the default location to New York */
  tmp.d = 40.714490;
  a_preferences_register(prefs6, tmp, VIKING_PREFERENCES_GROUP_KEY);
  tmp.d = -74.007130;
  a_preferences_register(prefs7, tmp, VIKING_PREFERENCES_GROUP_KEY);
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
