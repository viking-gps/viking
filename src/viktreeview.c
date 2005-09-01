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

#include <gtk/gtk.h>
#include <string.h>

#include "viking.h"

#include "config.h"

#define TREEVIEW_GET(model,iter,what,dest) gtk_tree_model_get(GTK_TREE_MODEL(model),(iter),(what),(dest),-1)

enum {
  VT_ITEM_EDITED_SIGNAL,
  VT_ITEM_TOGGLED_SIGNAL,
  VT_LAST_SIGNAL
};

static guint treeview_signals[VT_LAST_SIGNAL] = { 0, 0 };

static GObjectClass *parent_class;

enum
{
  NAME_COLUMN = 0,
  VISIBLE_COLUMN,
  ICON_COLUMN,
  /* invisible */

  TYPE_COLUMN,
  ITEM_PARENT_COLUMN,
  ITEM_POINTER_COLUMN,
  ITEM_DATA_COLUMN,
  HAS_VISIBLE_COLUMN,
  EDITABLE_COLUMN,
  /* properties dialog, delete, rename, etc. */
  NUM_COLUMNS
};

struct _VikTreeview {
  GtkTreeView treeview;
  GtkTreeModel *model;

  GdkPixbuf *layer_type_icons[VIK_LAYER_NUM_TYPES];
};

/* TODO: find, make "static" and put up here all non-"a_" functions */
static void treeview_class_init ( VikTreeviewClass *klass );
static void treeview_init ( VikTreeview *vt );
static void treeview_finalize ( GObject *gob );
static void treeview_add_columns ( VikTreeview *vt );

GType vik_treeview_get_type (void)
{
  static GType vt_type = 0;

  if (!vt_type)
  {
    static const GTypeInfo vt_info = 
    {
      sizeof (VikTreeviewClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) treeview_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikTreeview),
      0,
      (GInstanceInitFunc) treeview_init,
    };
    vt_type = g_type_register_static ( GTK_TYPE_TREE_VIEW, "VikTreeview", &vt_info, 0 );
  }

  return vt_type;
}

static void treeview_class_init ( VikTreeviewClass *klass )
{
  /* Destructor */
  GObjectClass *object_class;
                                                                                                                                 
  object_class = G_OBJECT_CLASS (klass);
                                                                                                                                 
  object_class->finalize =  treeview_finalize;
                                                                                                                                 
  parent_class = g_type_class_peek_parent (klass);

  treeview_signals[VT_ITEM_EDITED_SIGNAL] = g_signal_new ( "item_edited", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikTreeviewClass, item_edited), NULL, NULL, 
    g_cclosure_marshal_VOID__UINT_POINTER, G_TYPE_NONE, 2, GTK_TYPE_TREE_ITER, G_TYPE_STRING);
  /* VOID__UINT_POINTER: kinda hack-ish, but it works. */

  treeview_signals[VT_ITEM_TOGGLED_SIGNAL] = g_signal_new ( "item_toggled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikTreeviewClass, item_toggled), NULL, NULL,
    g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, GTK_TYPE_TREE_ITER );
}

static void treeview_edited_cb (GtkCellRendererText *cell, gchar *path_str, const gchar *new_name, VikTreeview *vt)
{
  GtkTreeIter iter;

  /* get type and data */
  vik_treeview_get_iter_from_path_str ( vt, &iter, path_str );

  g_signal_emit ( G_OBJECT(vt), treeview_signals[VT_ITEM_EDITED_SIGNAL], 0, &iter, new_name, 0 );
}

static void treeview_toggled_cb (GtkCellRendererToggle *cell, gchar *path_str, VikTreeview *vt)
{
  GtkTreeIter iter;

  /* get type and data */
  vik_treeview_get_iter_from_path_str ( vt, &iter, path_str );

  g_signal_emit ( G_OBJECT(vt), 
treeview_signals[VT_ITEM_TOGGLED_SIGNAL], 0, &iter );
}

VikTreeview *vik_treeview_new ()
{
  return VIK_TREEVIEW ( g_object_new ( VIK_TREEVIEW_TYPE, NULL ) );
}

gint vik_treeview_item_get_type ( VikTreeview *vt, GtkTreeIter *iter )
{
  gint rv;
  TREEVIEW_GET ( vt->model, iter, TYPE_COLUMN, &rv );
  return rv;
}

gint vik_treeview_item_get_data ( VikTreeview *vt, GtkTreeIter *iter )
{
  gint rv;
  TREEVIEW_GET ( vt->model, iter, ITEM_DATA_COLUMN, &rv );
  return rv;
}

gpointer vik_treeview_item_get_pointer ( VikTreeview *vt, GtkTreeIter *iter )
{
  gpointer rv;
  TREEVIEW_GET ( vt->model, iter, ITEM_POINTER_COLUMN, &rv );
  return rv;
}

void vik_treeview_item_set_pointer ( VikTreeview *vt, GtkTreeIter *iter, gpointer pointer )
{
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, ITEM_POINTER_COLUMN, pointer, -1 );
}

gpointer vik_treeview_item_get_parent ( VikTreeview *vt, GtkTreeIter *iter )
{
  gpointer rv;
  TREEVIEW_GET ( vt->model, iter, ITEM_PARENT_COLUMN, &rv );
  return rv;
}

void vik_treeview_get_iter_from_path_str ( VikTreeview *vt, GtkTreeIter *iter, const gchar *path_str )
{
  gtk_tree_model_get_iter_from_string ( GTK_TREE_MODEL(vt->model), iter, path_str );
}

static void treeview_add_columns ( VikTreeview *vt )
{
  gint col_offset;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_text_new ();
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (treeview_edited_cb), vt);

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, "Layer Name",
							    renderer, "text",
							    NAME_COLUMN,
							    "editable", EDITABLE_COLUMN,
							    NULL);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
				   GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 100);
  gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);
  gtk_tree_view_column_set_resizable (GTK_TREE_VIEW_COLUMN (column), TRUE);

  renderer = gtk_cell_renderer_pixbuf_new ();

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, "Type",
							    renderer, "pixbuf",
							    ICON_COLUMN,
							    NULL);


  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 33);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
				   GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);


  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);

  g_signal_connect (renderer, "toggled", G_CALLBACK (treeview_toggled_cb), vt);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, "Visible",
							    renderer,
							    "active",
							    VISIBLE_COLUMN,
							    "visible",
							    HAS_VISIBLE_COLUMN,
							    "activatable",
							    HAS_VISIBLE_COLUMN,
							    NULL);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 40);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
				   GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

}

void treeview_init ( VikTreeview *vt )
{
  guint16 i;

  vt->model = GTK_TREE_MODEL(gtk_tree_store_new ( NUM_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN ));

  /* create tree view */
  gtk_tree_view_set_model ( GTK_TREE_VIEW(vt), vt->model );
  treeview_add_columns ( vt );
  g_object_unref (vt->model);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (vt), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (vt)),
                               GTK_SELECTION_SINGLE);

  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
    vt->layer_type_icons[i] = vik_layer_load_icon ( i ); /* if icon can't be loaded, it will be null and simply not be shown. */

}

gboolean vik_treeview_item_get_parent_iter ( VikTreeview *vt, GtkTreeIter *iter,  GtkTreeIter *parent )
{
  return gtk_tree_model_iter_parent ( GTK_TREE_MODEL(vt->model), parent, iter );
}

gboolean vik_treeview_move_item ( VikTreeview *vt, GtkTreeIter *iter, gboolean up )
{
  gint t = vik_treeview_item_get_type ( vt, iter );
  if ( t == VIK_TREEVIEW_TYPE_LAYER )
  {
    GtkTreeIter switch_iter;
    if (up)
    {
      /* iter to path to iter */
      GtkTreePath *path = gtk_tree_model_get_path ( vt->model, iter );
      if ( !gtk_tree_path_prev ( path ) || !gtk_tree_model_get_iter ( vt->model, &switch_iter, path ) )
      {
        gtk_tree_path_free ( path );
        return FALSE;
      }
      gtk_tree_path_free ( path );
    }
    else
    {
      switch_iter = *iter;
      if ( !gtk_tree_model_iter_next ( vt->model, &switch_iter ) )
        return FALSE;
    }
    gtk_tree_store_swap ( GTK_TREE_STORE(vt->model), iter, &switch_iter ); 
    return TRUE;
    /* now, the easy part. actually switching them, not the GUI */
  } /* if item is map */
  return FALSE;
}

gboolean vik_treeview_get_iter_at_pos ( VikTreeview *vt, GtkTreeIter *iter, gint x, gint y )
{
  GtkTreePath *path;
  gtk_tree_view_get_path_at_pos ( GTK_TREE_VIEW(vt), x, y, &path, NULL, NULL, NULL );
  if ( ! path )
    return FALSE;

  gtk_tree_model_get_iter (GTK_TREE_MODEL(vt->model), iter, path);
  gtk_tree_path_free ( path );
  return TRUE;
}

void vik_treeview_select_iter ( VikTreeview *vt, GtkTreeIter *iter )
{
  gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( GTK_TREE_VIEW ( vt ) ), iter );
}

gboolean vik_treeview_get_selected_iter ( VikTreeview *vt, GtkTreeIter *iter )
{
  return gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( GTK_TREE_VIEW ( vt ) ), NULL, iter );
}

void vik_treeview_item_delete ( VikTreeview *vt, GtkTreeIter *iter )
{
  gtk_tree_store_remove ( GTK_TREE_STORE(vt->model), iter );
}

/* Treeview Reform Project */

void vik_treeview_item_set_name ( VikTreeview *vt, GtkTreeIter *iter, const gchar *to )
{
  g_return_if_fail ( iter != NULL && to != NULL );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, NAME_COLUMN, to, -1);
}

void vik_treeview_item_set_visible ( VikTreeview *vt, GtkTreeIter *iter, gboolean to )
{
  g_return_if_fail ( iter != NULL );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, VISIBLE_COLUMN, to, -1 );
}

void vik_treeview_expand ( VikTreeview *vt, GtkTreeIter *iter )
{
  GtkTreePath *path;
  path = gtk_tree_model_get_path ( vt->model, iter );
  gtk_tree_view_expand_row ( GTK_TREE_VIEW(vt), path, FALSE );
  gtk_tree_path_free ( path );
}

void vik_treeview_item_select ( VikTreeview *vt, GtkTreeIter *iter )
{
  gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( GTK_TREE_VIEW ( vt ) ), iter );
}

void vik_treeview_add_layer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent,
                              gpointer item, gint data, gint icon_type )
{
  g_assert ( iter != NULL );
  g_assert ( icon_type < VIK_LAYER_NUM_TYPES );
  gtk_tree_store_prepend ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, NAME_COLUMN, name, VISIBLE_COLUMN, TRUE, 
    TYPE_COLUMN, VIK_TREEVIEW_TYPE_LAYER, ITEM_PARENT_COLUMN, parent, ITEM_POINTER_COLUMN, item, 
    ITEM_DATA_COLUMN, data, HAS_VISIBLE_COLUMN, TRUE, EDITABLE_COLUMN, TRUE,
    ICON_COLUMN, icon_type >= 0 ? vt->layer_type_icons[icon_type] : NULL, -1 );
}

void vik_treeview_insert_layer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent,
                              gpointer item, gint data, gint icon_type, GtkTreeIter *sibling )
{
  g_assert ( iter != NULL );
  g_assert ( icon_type < VIK_LAYER_NUM_TYPES );
  gtk_tree_store_insert_before ( GTK_TREE_STORE(vt->model), iter, parent_iter, sibling );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, NAME_COLUMN, name, VISIBLE_COLUMN, TRUE, 
    TYPE_COLUMN, VIK_TREEVIEW_TYPE_LAYER, ITEM_PARENT_COLUMN, parent, ITEM_POINTER_COLUMN, item, 
    ITEM_DATA_COLUMN, data, HAS_VISIBLE_COLUMN, TRUE, EDITABLE_COLUMN, TRUE,
    ICON_COLUMN, icon_type >= 0 ? vt->layer_type_icons[icon_type] : NULL, -1 );
}

void vik_treeview_add_sublayer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gpointer item,
                                 gint data, GdkPixbuf *icon, gboolean has_visible, gboolean editable )
{
  g_assert ( iter != NULL );

  gtk_tree_store_prepend ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, NAME_COLUMN, name, VISIBLE_COLUMN, TRUE, TYPE_COLUMN, VIK_TREEVIEW_TYPE_SUBLAYER, ITEM_PARENT_COLUMN, parent, ITEM_POINTER_COLUMN, item, ITEM_DATA_COLUMN, data, HAS_VISIBLE_COLUMN, has_visible, EDITABLE_COLUMN, editable, ICON_COLUMN, icon, -1 );
}


#ifdef VIK_CONFIG_ALPHABETIZED_TRW

void vik_treeview_sublayer_realphabetize ( VikTreeview *vt, GtkTreeIter *iter, const gchar *newname )
{
  GtkTreeIter search_iter, parent_iter;
  gchar *search_name;
  g_assert ( iter != NULL );

  gtk_tree_model_iter_parent ( vt->model, &parent_iter, iter );

  g_assert ( gtk_tree_model_iter_children ( vt->model, &search_iter, &parent_iter ) );

  do {
    gtk_tree_model_get ( vt->model, &search_iter, NAME_COLUMN, &search_name, -1 );
    if ( strcmp ( search_name, newname ) > 0 ) /* not >= or would trip on itself */
    {
      gtk_tree_store_move_before ( GTK_TREE_STORE(vt->model), iter, &search_iter );
      return;
    }
  } while ( gtk_tree_model_iter_next ( vt->model, &search_iter ) );

  gtk_tree_store_move_before ( GTK_TREE_STORE(vt->model), iter, NULL );
}

void vik_treeview_add_sublayer_alphabetized
                 ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gpointer item,
                   gint data, GdkPixbuf *icon, gboolean has_visible, gboolean editable )
{
  GtkTreeIter search_iter;
  gchar *search_name;
  g_assert ( iter != NULL );

  if ( gtk_tree_model_iter_children ( vt->model, &search_iter, parent_iter ) )
  {
    gboolean found_greater_string = FALSE;
    do {
      gtk_tree_model_get ( vt->model, &search_iter, NAME_COLUMN, &search_name, -1 );
      if ( strcmp ( search_name, name ) >= 0 )
      {
        gtk_tree_store_insert_before ( GTK_TREE_STORE(vt->model), iter, parent_iter, &search_iter );
        found_greater_string = TRUE;
        break;
      }
    } while ( gtk_tree_model_iter_next ( vt->model, &search_iter ) );

    if ( ! found_greater_string )
      gtk_tree_store_append ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  }
  else
    gtk_tree_store_prepend ( GTK_TREE_STORE(vt->model), iter, parent_iter );

  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, NAME_COLUMN, name, VISIBLE_COLUMN, TRUE, TYPE_COLUMN, VIK_TREEVIEW_TYPE_SUBLAYER, ITEM_PARENT_COLUMN, parent, ITEM_POINTER_COLUMN, item, ITEM_DATA_COLUMN, data, HAS_VISIBLE_COLUMN, has_visible, EDITABLE_COLUMN, editable, ICON_COLUMN, icon, -1 );
}

#endif

static void treeview_finalize ( GObject *gob )
{
  VikTreeview *vt = VIK_TREEVIEW ( gob );
  guint16 i;

  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
    if ( vt->layer_type_icons[i] != NULL )
      g_object_unref ( G_OBJECT(vt->layer_type_icons[i]) );

  G_OBJECT_CLASS(parent_class)->finalize(gob);
}
