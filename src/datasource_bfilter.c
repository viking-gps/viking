/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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
#include <string.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"


/************************************ Simplify ***********************************/

static void datasource_bfilter_simplify_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, gpointer not_used );

/* TODO: shell_escape stuff */
/* TODO: name is useless for filters */

/* spin button scales */
VikLayerParamScale simplify_params_scales[] = {
  {1, 10000, 10, 0},
};

VikLayerParam bfilter_simplify_params[] = {
  { VIK_LAYER_NUM_TYPES, "numberofpoints", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Max number of points:"), VIK_LAYER_WIDGET_SPINBUTTON, simplify_params_scales, NULL, NULL, NULL, NULL },
};

VikLayerParamData bfilter_simplify_params_defaults[] = {
  /* Annoyingly 'C' cannot initialize unions properly */
  /* It's dependent on the standard used or the compiler support... */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L || __GNUC__
  { .i = 100 },
#else
  { 100 },
#endif
};

VikDataSourceInterface vik_datasource_bfilter_simplify_interface = {
  N_("Simplify All Tracks..."),
  N_("Simplified Tracks"),
  VIK_DATASOURCE_CREATENEWLAYER,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
  TRUE,
  FALSE, /* keep dialog open after success */
  TRUE,
  NULL, NULL, NULL,
  (VikDataSourceGetCmdStringFunc)	datasource_bfilter_simplify_get_cmd_string,
  (VikDataSourceProcessFunc)        a_babel_convert_from_shellcommand,
  NULL, NULL, NULL,
  (VikDataSourceOffFunc) NULL,

  bfilter_simplify_params,
  sizeof(bfilter_simplify_params)/sizeof(bfilter_simplify_params[0]),
  bfilter_simplify_params_defaults,
  NULL,
  0
};


static void datasource_bfilter_simplify_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, gpointer not_used )
{
  *input_file_type = NULL;
  *cmd = g_strdup_printf ( "gpsbabel -i gpx -f %s -x simplify,count=%d -o gpx -F -", input_filename, paramdatas[0].u );
}

/************************************ Duplicate Location ***********************************/

static void datasource_bfilter_dup_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, gpointer not_used );

/* TODO: shell_escape stuff */
/* TODO: name is useless for filters */


VikDataSourceInterface vik_datasource_bfilter_dup_interface = {
  N_("Remove Duplicate Waypoints"),
  N_("Remove Duplicate Waypoints"),
  VIK_DATASOURCE_CREATENEWLAYER,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
  TRUE,
  FALSE, /* keep dialog open after success */
  TRUE,
  NULL, NULL, NULL,
  (VikDataSourceGetCmdStringFunc)	datasource_bfilter_dup_get_cmd_string,
  (VikDataSourceProcessFunc)        a_babel_convert_from_shellcommand,
  NULL, NULL, NULL,
  (VikDataSourceOffFunc) NULL,

  NULL, 0, NULL, NULL, 0
};


static void datasource_bfilter_dup_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, gpointer not_used )
{
  *input_file_type = NULL;
  *cmd = g_strdup_printf ( "gpsbabel -i gpx -f %s -x duplicate,location -o gpx -F -", input_filename );
}


/************************************ Polygon ***********************************/

static void datasource_bfilter_polygon_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, const gchar *input_track_filename, gpointer not_used );

/* TODO: shell_escape stuff */
/* TODO: name is useless for filters */


VikDataSourceInterface vik_datasource_bfilter_polygon_interface = {
  N_("Waypoints Inside This"),
  N_("Polygonized Layer"),
  VIK_DATASOURCE_CREATENEWLAYER,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK,
  TRUE,
  FALSE, /* keep dialog open after success */
  TRUE,
  NULL, NULL, NULL,
  (VikDataSourceGetCmdStringFunc)	datasource_bfilter_polygon_get_cmd_string,
  (VikDataSourceProcessFunc)        a_babel_convert_from_shellcommand,
  NULL, NULL, NULL,
  (VikDataSourceOffFunc) NULL,

  NULL,
  0,
  NULL,
  NULL,
  0
};


static void datasource_bfilter_polygon_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, const gchar *input_track_filename, gpointer not_used )
{
  *input_file_type = NULL;
  *cmd = g_strdup_printf ( "gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,file=- -o gpx -F -", input_track_filename, input_filename );
}

/************************************ Exclude Polygon ***********************************/

static void datasource_bfilter_exclude_polygon_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, const gchar *input_track_filename, gpointer not_used );

/* TODO: shell_escape stuff */
/* TODO: name is useless for filters */


VikDataSourceInterface vik_datasource_bfilter_exclude_polygon_interface = {
  N_("Waypoints Outside This"),
  N_("Polygonzied Layer"),
  VIK_DATASOURCE_CREATENEWLAYER,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK,
  TRUE,
  FALSE, /* keep dialog open after success */
  TRUE,
  NULL, NULL, NULL,
  (VikDataSourceGetCmdStringFunc)	datasource_bfilter_exclude_polygon_get_cmd_string,
  (VikDataSourceProcessFunc)        a_babel_convert_from_shellcommand,
  NULL, NULL, NULL,
  (VikDataSourceOffFunc) NULL,

  NULL,
  0,
  NULL,
  NULL,
  0
};


static void datasource_bfilter_exclude_polygon_get_cmd_string ( VikLayerParamData *paramdatas, gchar **cmd, gchar **input_file_type, const gchar *input_filename, const gchar *input_track_filename, gpointer not_used )
{
  *input_file_type = NULL;
  *cmd = g_strdup_printf ( "gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,exclude,file=- -o gpx -F -", input_track_filename, input_filename );
}

