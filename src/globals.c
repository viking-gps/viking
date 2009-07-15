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

#define VIKING_PREFERENCES_GROUP_KEY "viking.globals"
#define VIKING_PREFERENCES_NAMESPACE "viking.globals."

static gchar * params_degree_formats[] = {"DDD", "DMM", "DMS", NULL};

static VikLayerParam prefs[] = {
  { VIKING_PREFERENCES_NAMESPACE "degree_format", VIK_LAYER_PARAM_UINT, VIK_DEGREE_FORMAT_DMS, N_("Degree format:"), VIK_LAYER_WIDGET_COMBOBOX, params_degree_formats, NULL },
};

void a_vik_preferences_init ()
{
  a_preferences_register_group ( VIKING_PREFERENCES_GROUP_KEY, "Global preferences" );

  VikLayerParamData tmp;
  tmp.u = VIK_DEGREE_FORMAT_DMS;
  a_preferences_register(prefs, tmp, VIKING_PREFERENCES_GROUP_KEY);
}

vik_degree_format_t a_vik_get_degree_format ( )
{
  vik_degree_format_t format;
  format = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "degree_format")->u;
  return format;
  /* TODO use preferences */
  return VIK_DEGREE_FORMAT_DMS;
}
