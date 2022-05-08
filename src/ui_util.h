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
#include "vik_compat.h"

G_BEGIN_DECLS

void open_url(GtkWindow *parent, const gchar * url);
void new_email(GtkWindow *parent, const gchar * address);

GtkWidget *ui_button_new_with_image(const gchar *stock_id, const gchar *text);
gint ui_get_gtk_settings_integer(const gchar *property_name, gint default_value);
GtkWidget *ui_lookup_widget(GtkWidget *widget, const gchar *widget_name);
GtkWidget* ui_label_new_selectable ( const gchar* text );
GtkWidget *ui_entry_new ( const gchar *str, GtkEntryIconPosition position );
void ui_entry_set_text ( GtkWidget *widget, const gchar *str );
GtkWidget *ui_spin_button_new ( GtkAdjustment *adjustment,
                                gdouble climb_rate,
                                guint digits );
GtkWidget *ui_attach_to_table ( GtkTable *table, int i, char *mylabel, GtkWidget *content, gchar *value_potentialURL, gboolean embolden );

GdkPixbuf *ui_pixbuf_new ( GdkColor *color, guint width, guint height );
GdkPixbuf *ui_pixbuf_set_alpha ( GdkPixbuf *pixbuf, guint8 alpha );
GdkPixbuf *ui_pixbuf_scale_alpha ( GdkPixbuf *pixbuf, guint8 alpha );
GdkPixbuf *ui_pixbuf_rotate_full ( GdkPixbuf *pixbuf, gdouble degrees );

void ui_add_recent_file ( const gchar *filename );

void ui_format_1f_cell_data_func ( GtkTreeViewColumn *col,
                                   GtkCellRenderer   *renderer,
                                   GtkTreeModel      *model,
                                   GtkTreeIter       *iter,
                                   gpointer           user_data );

GtkTreeViewColumn *ui_new_column_text ( const gchar *title, GtkCellRenderer *renderer, GtkWidget *view, gint column_runner );

gboolean ui_tree_model_number_tooltip_cb ( GtkWidget    *widget,
                                           gint          x,
                                           gint          y,
                                           gboolean      keyboard_mode,
                                           GtkTooltip   *tooltip,
                                           GtkTreeModel *tree_model );

void ui_load_icons ( void );
GdkPixbuf *ui_get_icon ( const gchar *name, guint size );

void ui_cr_draw_layout ( cairo_t *cr, gdouble xx, gdouble yy, PangoLayout *layout );

void ui_cr_draw_line ( cairo_t *cr, gdouble x1, gdouble y1, gdouble x2, gdouble y2 );

void ui_cr_draw_rectangle ( cairo_t *cr, gboolean fill, gdouble xx, gdouble yy, gdouble ww, gdouble hh );

void ui_cr_set_color ( cairo_t *cr, const gchar *name );

void ui_cr_set_dash ( cairo_t *cr );

void ui_cr_clear ( cairo_t *cr );

void ui_cr_surface_paint ( cairo_t *cr, cairo_surface_t *surface );

void ui_cr_label_with_bg (cairo_t *cr, gint xd, gint yd, gint wd, gint hd, PangoLayout *pl);

void ui_gc_unref ( GdkGC *gc );

G_END_DECLS

#endif
