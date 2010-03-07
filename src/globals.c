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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "globals.h"
#include "preferences.h"

gboolean vik_debug = FALSE;
gboolean vik_verbose = FALSE;
gboolean vik_version = FALSE;
gboolean vik_use_small_wp_icons = FALSE;

static gchar * params_degree_formats[] = {"DDD", "DMM", "DMS", NULL};
static gchar * params_units_distance[] = {"Kilometres", "Miles", NULL};
static gchar * params_units_speed[] = {"km/h", "mph", "m/s", NULL};
static gchar * params_units_height[] = {"Metres", "Feet", NULL};

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
