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

G_BEGIN_DECLS

/* most of this file is an architechtural flaw. */

#define a_dialog_info_msg(win,info) a_dialog_msg(win,GTK_MESSAGE_INFO,info,NULL)
#define a_dialog_warning_msg(win,info) a_dialog_msg(win,GTK_MESSAGE_WARNING,info,NULL)
#define a_dialog_error_msg(win,info) a_dialog_msg(win,GTK_MESSAGE_ERROR,info,NULL)

#define a_dialog_info_msg_extra(win,info,extra) a_dialog_msg(win,GTK_MESSAGE_INFO,info,extra)
#define a_dialog_error_msg_extra(win,info,extra) a_dialog_msg(win,GTK_MESSAGE_ERROR,info,extra)

GtkWidget *a_dialog_create_label_vbox ( gchar **texts, int label_count, gint spacing, gint padding );

void a_dialog_msg ( GtkWindow *parent, gint type, const gchar *info, const gchar *extra );

void a_dialog_response_accept ( GtkDialog *dialog );

void a_dialog_list ( GtkWindow *parent, const gchar *title, GArray *array, gint padding );

void a_dialog_about ( GtkWindow *parent );

/* okay, everthing below here is an architechtural flaw. */
gboolean a_dialog_goto_latlon ( GtkWindow *parent, struct LatLon *ll, const struct LatLon *old );
gboolean a_dialog_goto_utm ( GtkWindow *parent, struct UTM *utm, const struct UTM *old );

gchar *a_dialog_new_track ( GtkWindow *parent, gchar *default_name, gboolean is_route );

gchar *a_dialog_get_date ( GtkWindow *parent, const gchar *title );
gboolean a_dialog_yes_or_no ( GtkWindow *parent, const gchar *message, const gchar *extra );
gboolean a_dialog_custom_zoom ( GtkWindow *parent, gdouble *xmpp, gdouble *ympp );
gboolean a_dialog_time_threshold ( GtkWindow *parent, gchar *title_text, gchar *label_text, guint *thr );

guint a_dialog_get_positive_number ( GtkWindow *parent, gchar *title_text, gchar *label_text, guint default_num, guint min, guint max, guint step );

void a_dialog_choose_dir ( GtkWidget *entry );

gboolean a_dialog_map_n_zoom(GtkWindow *parent, gchar *mapnames[], gint default_map, gchar *zoom_list[], gint default_zoom, gint *selected_map, gint *selected_zoom);

GList *a_dialog_select_from_list ( GtkWindow *parent, GList *names, gboolean multiple_selection_allowed, const gchar *title, const gchar *msg );

void a_dialog_license ( GtkWindow *parent, const gchar *map, const gchar *license, const gchar *url);

G_END_DECLS

#endif
