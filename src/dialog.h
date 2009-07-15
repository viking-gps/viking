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

#ifndef _VIKING_DIALOG_H
#define _VIKING_DIALOG_H

#include <glib.h>
#include <gtk/gtk.h>

#include "coords.h"
#include "vikwaypoint.h"
#include "vikcoord.h"

/* most of this file is an architechtural flaw. */

#define a_dialog_info_msg(win,info) a_dialog_msg(win,GTK_MESSAGE_INFO,info,NULL)
#define a_dialog_warning_msg(win,info) a_dialog_msg(win,GTK_MESSAGE_WARNING,info,NULL)
#define a_dialog_error_msg(win,info) a_dialog_msg(win,GTK_MESSAGE_ERROR,info,NULL)

#define a_dialog_info_msg_extra(win,info,extra) a_dialog_msg(win,GTK_MESSAGE_INFO,info,extra)
#define a_dialog_error_msg_extra(win,info,extra) a_dialog_msg(win,GTK_MESSAGE_ERROR,info,extra)

GtkWidget *a_dialog_create_label_vbox ( gchar **texts, int label_count );

void a_dialog_msg ( GtkWindow *parent, gint type, const gchar *info, const gchar *extra );

void a_dialog_response_accept ( GtkDialog *dialog );

void a_dialog_about ( GtkWindow *parent );

/* okay, everthing below here is an architechtural flaw. */
gboolean a_dialog_goto_latlon ( GtkWindow *parent, struct LatLon *ll, const struct LatLon *old );
gboolean a_dialog_goto_utm ( GtkWindow *parent, struct UTM *utm, const struct UTM *old );

/* if *dest is non-null, uses it as a default and frees it */
gboolean a_dialog_new_waypoint ( GtkWindow *parent, gchar **dest, VikWaypoint *wp, GHashTable *waypoints, VikCoordMode coord_mode );

gchar *a_dialog_new_track ( GtkWindow *parent, GHashTable *tracks );

gboolean a_dialog_overwrite ( GtkWindow *parent, const gchar *message, const gchar *extra );
gboolean a_dialog_custom_zoom ( GtkWindow *parent, gdouble *xmpp, gdouble *ympp );
gboolean a_dialog_time_threshold ( GtkWindow *parent, gchar *title_text, gchar *label_text, guint *thr );

void a_dialog_choose_dir ( GtkWidget *entry );

gboolean a_dialog_map_n_zoom(GtkWindow *parent, gchar *mapnames[], gint default_map, gchar *zoom_list[], gint default_zoom, gint *selected_map, gint *selected_zoom);

GList *a_dialog_select_from_list ( GtkWindow *parent, GHashTable *tracks, GList *track_names, gboolean multiple_selection_allowed, const gchar *title, const gchar *msg );
#endif
