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

#ifndef _VIKING_TREEVIEW_H
#define _VIKING_TREEVIEW_H

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtktreeview.h>

G_BEGIN_DECLS

#define VIK_TREEVIEW_TYPE            (vik_treeview_get_type ())
#define VIK_TREEVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TREEVIEW_TYPE, VikTreeview))
#define VIK_TREEVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TREEVIEW_TYPE, VikTreeviewClass))
#define IS_VIK_TREEVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TREEVIEW_TYPE))
#define IS_VIK_TREEVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TREEVIEW_TYPE))

typedef struct _VikTreeview VikTreeview;
typedef struct _VikTreeviewClass VikTreeviewClass;

struct _VikTreeviewClass
{
  GtkTreeViewClass vbox_class;
  void (* item_edited) (VikTreeview *vt, GtkTreeIter *iter, const gchar *new_name);
  void (* item_toggled) (VikTreeview *vt,GtkTreeIter *iter);
};

enum {
 VIK_TREEVIEW_TYPE_LAYER = 0,
 VIK_TREEVIEW_TYPE_SUBLAYER
};

GType vik_treeview_get_type ();


VikTreeview *vik_treeview_new ();

GtkWidget *vik_treeview_get_widget ( VikTreeview *vt );

gint vik_treeview_item_get_data ( VikTreeview *vt, GtkTreeIter *iter );
gint vik_treeview_item_get_type ( VikTreeview *vt, GtkTreeIter *iter );
gpointer vik_treeview_item_get_pointer ( VikTreeview *vt, GtkTreeIter *iter );
void vik_treeview_item_set_pointer ( VikTreeview *vt, GtkTreeIter *iter, gpointer pointer );

gpointer vik_treeview_item_get_parent ( VikTreeview *vt, GtkTreeIter *iter );

void vik_treeview_select_iter ( VikTreeview *vt, GtkTreeIter *iter );
gboolean vik_treeview_get_selected_iter ( VikTreeview *vt, GtkTreeIter *iter );

void vik_treeview_item_set_name ( VikTreeview *vt, GtkTreeIter *iter, const gchar *to );
void vik_treeview_item_set_visible ( VikTreeview *vt, GtkTreeIter *iter, gboolean to );
void vik_treeview_item_delete ( VikTreeview *vt, GtkTreeIter *iter );

gboolean vik_treeview_get_iter_at_pos ( VikTreeview *vt, GtkTreeIter *iter, gint x, gint y );

gboolean vik_treeview_get_iter_from_path_str ( VikTreeview *vt, GtkTreeIter *iter, const gchar *path_str );
gboolean vik_treeview_move_item ( VikTreeview *vt, GtkTreeIter *iter, gboolean up );
void vik_treeview_item_select ( VikTreeview *vt, GtkTreeIter *iter );

gboolean vik_treeview_item_get_parent_iter ( VikTreeview *vt, GtkTreeIter *iter,  GtkTreeIter *parent );
void vik_treeview_expand_toplevel ( VikTreeview *vt );
void vik_treeview_expand ( VikTreeview *vt, GtkTreeIter *iter );

void vik_treeview_add_layer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent,
                              gpointer item, gint data, gint icon_type ); /* icon type: type of layer or -1 -> no icon */
void vik_treeview_insert_layer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent,
                              gpointer item, gint data, gint icon_type, GtkTreeIter *sibling );
void vik_treeview_add_sublayer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gpointer item,
                                 gint data, GdkPixbuf *icon, gboolean has_visible, gboolean editable );

gboolean vik_treeview_get_iter_with_name ( VikTreeview *vt, GtkTreeIter *iter, GtkTreeIter *parent_iter, const gchar *name );

#ifdef VIK_CONFIG_ALPHABETIZED_TRW
void vik_treeview_add_sublayer_alphabetized ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gpointer item,
                                 gint data, GdkPixbuf *icon, gboolean has_visible, gboolean editable );

void vik_treeview_sublayer_realphabetize ( VikTreeview *vt, GtkTreeIter *iter, const gchar *newname );
#endif

G_END_DECLS

#endif
