/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
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
#ifndef __VIKING_UTILS_H
#define __VIKING_UTILS_H

#include <glib.h>
#include "viktrwlayer.h"
#include "globals.h"

G_BEGIN_DECLS

gchar* vu_trackpoint_formatted_message ( gchar *format_code, VikTrackpoint *trkpt, VikTrackpoint *trkpt_prev, VikTrack *trk, gdouble climb );

void vu_check_latest_version ( GtkWindow *window );

void vu_set_auto_features_on_first_run ( void );

gchar *vu_get_canonical_filename ( VikLayer *vl, const gchar *filename );

gchar* vu_get_time_string ( time_t *time, const gchar *format, const VikCoord *vc, const gchar *gtz );

gchar* vu_get_tz_at_location ( const VikCoord* vc );

void vu_setup_lat_lon_tz_lookup ();
void vu_finalize_lat_lon_tz_lookup ();

void vu_command_line ( VikWindow *vw, gdouble latitude, gdouble longitude, gint zoom_osm_level, gint map_id );

void vu_copy_label_menu ( GtkWidget *widget, guint button );

void vu_zoom_to_show_latlons ( VikCoordMode mode, VikViewport *vvp, struct LatLon maxmin[2] );

void vu_waypoint_set_image_uri ( VikWaypoint *wp, const gchar *uri, const gchar *dirpath );

void vu_calendar_set_to_today ( GtkWidget *cal );

GtkWidget* vu_menu_add_item ( const GtkMenu *menu,
                              const gchar* mnemonic,
                              const gchar* stock_icon,
                              const GCallback callback,
                              const gpointer user_data );

gchar* vu_speed_units_text ( vik_units_speed_t speed_units );
gdouble vu_speed_convert ( vik_units_speed_t speed_units, gdouble speed );
void vu_speed_text_value ( gchar* buf, guint size, vik_units_speed_t speed_units, gdouble speed, gchar *format );

void vu_speed_text ( gchar* buf, guint size, vik_units_speed_t speed_units, gdouble speed, gboolean convert, gchar *format );

GSList* vu_get_ui_selected_gps_files ( VikWindow *vw, gboolean external );

void vu_format_speed_cell_data_func ( GtkTreeViewColumn *col,
                                      GtkCellRenderer   *renderer,
                                      GtkTreeModel      *model,
                                      GtkTreeIter       *iter,
                                      gpointer           user_data );

void vu_finish ( void );

gchar* vu_get_last_folder_files_uri ();
void vu_set_last_folder_files_uri ( gchar *folder_uri );

G_END_DECLS

#endif
