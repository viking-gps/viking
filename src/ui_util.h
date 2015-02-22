/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007-2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_UI_UTIL_H
#define _VIKING_UI_UTIL_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void open_url(GtkWindow *parent, const gchar * url);
void new_email(GtkWindow *parent, const gchar * address);

GtkWidget *ui_button_new_with_image(const gchar *stock_id, const gchar *text);
gint ui_get_gtk_settings_integer(const gchar *property_name, gint default_value);
GtkWidget *ui_lookup_widget(GtkWidget *widget, const gchar *widget_name);
GtkWidget* ui_label_new_selectable ( const gchar* text );

GdkPixbuf *ui_pixbuf_set_alpha ( GdkPixbuf *pixbuf, guint8 alpha );
void ui_add_recent_file ( const gchar *filename );

G_END_DECLS

#endif
