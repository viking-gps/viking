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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

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

  gboolean was_a_toggle;
};

/* TODO: find, make "static" and put up here all non-"a_" functions */
static void treeview_class_init ( VikTreeviewClass *klass );
static void treeview_init ( VikTreeview *vt );
static void treeview_finalize ( GObject *gob );
static void treeview_add_columns ( VikTreeview *vt );

static gboolean treeview_drag_data_received ( GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data );
static gboolean treeview_drag_data_delete ( GtkTreeDragSource *drag_source, GtkTreePath *path );

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
    gtk_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2, GTK_TYPE_POINTER, G_TYPE_POINTER);
  /* VOID__UINT_POINTER: kinda hack-ish, but it works. */

  treeview_signals[VT_ITEM_TOGGLED_SIGNAL] = g_signal_new ( "item_toggled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikTreeviewClass, item_toggled), NULL, NULL,
    g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, GTK_TYPE_POINTER );
}

static void treeview_edited_cb (GtkCellRendererText *cell, gchar *path_str, const gchar *new_name, VikTreeview *vt)
{
  GtkTreeIter iter;

  /* get type and data */
  vik_treeview_get_iter_from_path_str ( vt, &iter, path_str );

  g_signal_emit ( G_OBJECT(vt), treeview_signals[VT_ITEM_EDITED_SIGNAL], 0, &iter, new_name );
}

static void treeview_toggled_cb (GtkCellRendererToggle *cell, gchar *path_str, VikTreeview *vt)
{
  GtkTreeIter iter;

  /* get type and data */
  vik_treeview_get_iter_from_path_str ( vt, &iter, path_str );
  vt->was_a_toggle = TRUE;

  g_signal_emit ( G_OBJECT(vt), 
treeview_signals[VT_ITEM_TOGGLED_SIGNAL], 0, &iter );
}

/* Inspired by GTK+ test
 * http://git.gnome.org/browse/gtk+/tree/tests/testtooltips.c
 */
static gboolean
treeview_tooltip_cb (GtkWidget  *widget,
		     gint        x,
		     gint        y,
		     gboolean    keyboard_tip,
		     GtkTooltip *tooltip,
		     gpointer    data)
{
  GtkTreeIter iter;
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreePath *path = NULL;

  char buffer[256];

  if (!gtk_tree_view_get_tooltip_context (tree_view, &x, &y,
					  keyboard_tip,
					  &model, &path, &iter))
    return FALSE;

  /* ATM normally treeview doesn't call into layers - maybe another level of redirection required? */
  gint rv;
  gtk_tree_model_get (model, &iter, TYPE_COLUMN, &rv, -1);
  if ( rv == VIK_TREEVIEW_TYPE_SUBLAYER ) {

    gtk_tree_model_get (model, &iter, ITEM_DATA_COLUMN, &rv, -1);
    // No tooltips ATM for the immediate Tracks / Waypoints tree list
    if ( rv == 0 || rv == 1 )
      // VIK_TRW_LAYER_SUBLAYER_WAYPOINTS or VIK_TRW_LAYER_SUBLAYER_TRACKS
      return FALSE;

    gpointer sublayer;
    gtk_tree_model_get (model, &iter, ITEM_POINTER_COLUMN, &sublayer, -1);

    gpointer parent;
    gtk_tree_model_get (model, &iter, ITEM_PARENT_COLUMN, &parent, -1);

    g_snprintf (buffer, sizeof(buffer), "%s", vik_layer_sublayer_tooltip (VIK_LAYER(parent), rv, sublayer));
  }
  else if ( rv == VIK_TREEVIEW_TYPE_LAYER ) {
    gpointer layer;
    gtk_tree_model_get (model, &iter, ITEM_POINTER_COLUMN, &layer, -1);
    g_snprintf (buffer, sizeof(buffer), "%s", vik_layer_layer_tooltip (VIK_LAYER(layer)));
  }
  else
    return FALSE;

  // Don't display null strings :)
  if ( strncmp (buffer, "(null)", 6) == 0 ) {
    return FALSE;
  }
  else {
    gtk_tooltip_set_markup (tooltip, buffer);
  }

  gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

  gtk_tree_path_free (path);

  return TRUE;
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

gboolean vik_treeview_get_iter_from_path_str ( VikTreeview *vt, GtkTreeIter *iter, const gchar *path_str )
{
  return gtk_tree_model_get_iter_from_string ( GTK_TREE_MODEL(vt->model), iter, path_str );
}

static void treeview_add_columns ( VikTreeview *vt )
{
  gint col_offset;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  /* Layer column */
  renderer = gtk_cell_renderer_text_new ();
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (treeview_edited_cb), vt);

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, _("Layer Name"),
							    renderer, "text",
							    NAME_COLUMN,
							    "editable", EDITABLE_COLUMN,
							    NULL);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);

  /* Layer type */
  renderer = gtk_cell_renderer_pixbuf_new ();

  g_object_set (G_OBJECT (renderer), "xalign", 0.5, NULL);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, "",
							    renderer, "pixbuf",
							    ICON_COLUMN,
							    NULL);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_AUTOSIZE);

  /* Layer visible */
  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.5, NULL);

  g_signal_connect (renderer, "toggled", G_CALLBACK (treeview_toggled_cb), vt);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, "",
							    renderer,
							    "active",
							    VISIBLE_COLUMN,
							    "visible",
							    HAS_VISIBLE_COLUMN,
							    "activatable",
							    HAS_VISIBLE_COLUMN,
							    NULL);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_AUTOSIZE);


  g_object_set (GTK_TREE_VIEW (vt), "has-tooltip", TRUE, NULL);
  g_signal_connect (GTK_TREE_VIEW (vt), "query-tooltip", G_CALLBACK (treeview_tooltip_cb), vt);
}

static void select_cb(GtkTreeSelection *selection, gpointer data)
{
  VikTreeview *vt = data;
  gint type;
  GtkTreeIter iter, parent;
  VikLayer *vl;
  VikWindow * vw;

  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) return;
  type = vik_treeview_item_get_type( vt, &iter);

  while ( type != VIK_TREEVIEW_TYPE_LAYER ) {
    if ( ! vik_treeview_item_get_parent_iter ( vt, &iter, &parent ) )
      return;
    iter = parent;
    type = vik_treeview_item_get_type (vt, &iter );
  }

  vl = VIK_LAYER( vik_treeview_item_get_pointer ( vt, &iter ) );

  vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vl));
  vik_window_selected_layer(vw, vl);
}

static gboolean treeview_selection_filter(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer data)
{
  VikTreeview *vt = data;

  if (vt->was_a_toggle) {
    vt->was_a_toggle = FALSE;
    return FALSE;
  }

  return TRUE;
}

void treeview_init ( VikTreeview *vt )
{
  guint16 i;

  vt->was_a_toggle = FALSE;

  vt->model = GTK_TREE_MODEL(gtk_tree_store_new ( NUM_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN ));

  /* create tree view */
  gtk_tree_selection_set_select_function(gtk_tree_view_get_selection (GTK_TREE_VIEW(vt)), treeview_selection_filter, vt, NULL);

  gtk_tree_view_set_model ( GTK_TREE_VIEW(vt), vt->model );
  treeview_add_columns ( vt );
  g_object_unref (vt->model);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (vt), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (vt)),
                               GTK_SELECTION_SINGLE);

  /* Override treestore's dnd methods only; this is easier than deriving from GtkTreeStore. 
   * The downside is that all treestores will have this behavior, so this needs to be
   * changed if we add more treeviews in the future.  //Alex
   */
  if (1) {
    GtkTreeDragSourceIface *isrc;
    GtkTreeDragDestIface *idest;

    isrc = g_type_interface_peek (g_type_class_peek(G_OBJECT_TYPE(vt->model)), GTK_TYPE_TREE_DRAG_SOURCE);
    isrc->drag_data_delete = treeview_drag_data_delete;

    idest = g_type_interface_peek (g_type_class_peek(G_OBJECT_TYPE(vt->model)), GTK_TYPE_TREE_DRAG_DEST);
    idest->drag_data_received = treeview_drag_data_received;
  }      

  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
    vt->layer_type_icons[i] = vik_layer_load_icon ( i ); /* if icon can't be loaded, it will be null and simply not be shown. */

  gtk_tree_view_set_reorderable (GTK_TREE_VIEW(vt), TRUE);
  g_signal_connect(gtk_tree_view_get_selection (GTK_TREE_VIEW (vt)), "changed",
      G_CALLBACK(select_cb), vt);

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
  if (sibling) {
    gtk_tree_store_insert_before ( GTK_TREE_STORE(vt->model), iter, parent_iter, sibling );
  } else {
    gtk_tree_store_append ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  }
    
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
  gchar *search_name = NULL;
  g_assert ( iter != NULL );

  gtk_tree_model_iter_parent ( vt->model, &parent_iter, iter );

  g_assert ( gtk_tree_model_iter_children ( vt->model, &search_iter, &parent_iter ) );

  do {
    gtk_tree_model_get ( vt->model, &search_iter, NAME_COLUMN, &search_name, -1 );
    if ( strcmp ( search_name, newname ) > 0 ) /* not >= or would trip on itself */
    {
      gtk_tree_store_move_before ( GTK_TREE_STORE(vt->model), iter, &search_iter );
      g_free (search_name);
      search_name = NULL;
      return;
    }
    g_free (search_name);
    search_name = NULL;
  } while ( gtk_tree_model_iter_next ( vt->model, &search_iter ) );

  gtk_tree_store_move_before ( GTK_TREE_STORE(vt->model), iter, NULL );
}

void vik_treeview_add_sublayer_alphabetized
                 ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gpointer item,
                   gint data, GdkPixbuf *icon, gboolean has_visible, gboolean editable )
{
  GtkTreeIter search_iter;
  gchar *search_name = NULL;
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
        g_free (search_name);
        search_name = NULL;
        break;
      }
      g_free (search_name);
      search_name = NULL;
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

static gboolean treeview_drag_data_received (GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
  GtkTreeModel *tree_model;
  GtkTreeStore *tree_store;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL, *dest_cp = NULL;
  gboolean retval = FALSE;
  GtkTreeIter src_iter, root_iter, dest_iter, dest_parent;
  gint *i_src = NULL;
  VikTreeview *vt;
  VikLayer *vl;

  g_return_val_if_fail (GTK_IS_TREE_STORE (drag_dest), FALSE);

  tree_model = GTK_TREE_MODEL (drag_dest);
  tree_store = GTK_TREE_STORE (drag_dest);

  if (gtk_tree_get_row_drag_data (selection_data, &src_model, &src_path) && src_model == tree_model) {
    /* 
     * Copy src_path to dest.  There are two subcases here, depending on what 
     * is being dragged.
     * 
     * 1. src_path is a layer. In this case, interpret the drop 
     *    as a request to move the layer to a different aggregate layer.
     *    If the destination is not an aggregate layer, use the first 
     *    ancestor that is.
     *
     * 2. src_path is a sublayer.  In this case, find ancestors of 
     *    both source and destination nodes who are full layers,
     *    and call the move method of that layer type. 
     *
     */
    if (!gtk_tree_model_get_iter (src_model, &src_iter, src_path)) {
      goto out;
    }
    if (!gtk_tree_path_compare(src_path, dest)) {
      goto out;
    }

    i_src = gtk_tree_path_get_indices (src_path);
    dest_cp = gtk_tree_path_copy (dest);

    gtk_tree_model_get_iter_first(tree_model, &root_iter);
    TREEVIEW_GET(tree_model, &root_iter, ITEM_POINTER_COLUMN, &vl);
    vt = vl->vt;


    if (gtk_tree_path_get_depth(dest_cp)>1) { /* can't be sibling of top layer */
      VikLayer *vl_src, *vl_dest;

      /* Find the first ancestor that is a full layer, and store in dest_parent. 
       * In addition, put in dest_iter where Gtk wants us to insert the dragged object.
       * (Note that this may end up being an invalid iter). 
       */
      do {
	gtk_tree_path_up(dest_cp);
	dest_iter = dest_parent;
	gtk_tree_model_get_iter (src_model, &dest_parent, dest_cp);
      } while (gtk_tree_path_get_depth(dest_cp)>1 &&
	       vik_treeview_item_get_type(vt, &dest_parent) != VIK_TREEVIEW_TYPE_LAYER);

      
      g_assert ( vik_treeview_item_get_parent(vt, &src_iter) );
      vl_src = vik_treeview_item_get_parent(vt, &src_iter);
      vl_dest = vik_treeview_item_get_pointer(vt, &dest_parent);

      /* TODO: might want to allow different types, and let the clients handle how they want */
      if (vl_src->type == vl_dest->type && vik_layer_get_interface(vl_dest->type)->drag_drop_request) {
	//	g_print("moving an item from layer '%s' into layer '%s'\n", vl_src->name, vl_dest->name);
	vik_layer_get_interface(vl_dest->type)->drag_drop_request(vl_src, vl_dest, &src_iter, dest);
      }    
    }
  }

 out:
  if (dest_cp) 
    gtk_tree_path_free(dest_cp);
  if (src_path)
    gtk_tree_path_free (src_path);

  return retval;
}

/* 
 * This may not be necessary.
 */
static gboolean treeview_drag_data_delete ( GtkTreeDragSource *drag_source, GtkTreePath *path )
{
  gchar *s_dest = gtk_tree_path_to_string(path);
  g_print(_("delete data from %s\n"), s_dest);
  g_free(s_dest);
  return FALSE;
}

