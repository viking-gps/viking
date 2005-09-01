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

#include "viking.h"
#include "vikaggregatelayer_pixmap.h"

#include <string.h>

#define DISCONNECT_UPDATE_SIGNAL(vl, val) g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, val)

static VikAggregateLayer *aggregate_layer_copy ( VikAggregateLayer *val, gpointer vp );
static void aggregate_layer_change_coord_mode ( VikAggregateLayer *val, VikCoordMode mode );

VikLayerInterface vik_aggregate_layer_interface = {
  "Aggregate",
  &aggregatelayer_pixbuf,

  NULL,
  0,

  NULL,
  0,
  NULL,
  0,

  (VikLayerFuncCreate)                  vik_aggregate_layer_create,
  (VikLayerFuncRealize)                 vik_aggregate_layer_realize,
  (VikLayerFuncPostRead)                NULL,
  (VikLayerFuncFree)                    vik_aggregate_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_aggregate_layer_draw,
  (VikLayerFuncChangeCoordMode)         aggregate_layer_change_coord_mode,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,

  (VikLayerFuncCopy)                    aggregate_layer_copy,

  (VikLayerFuncSetParam)                NULL,
  (VikLayerFuncGetParam)                NULL,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
};

struct _VikAggregateLayer {
  VikLayer vl;
  GList *children;
};

GType vik_aggregate_layer_get_type ()
{
  static GType val_type = 0;

  if (!val_type)
  {
    static const GTypeInfo val_info =
    {
      sizeof (VikAggregateLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikAggregateLayer),
      0,
      NULL /* instance init */
    };
    val_type = g_type_register_static ( VIK_LAYER_TYPE, "VikAggregateLayer", &val_info, 0 );
  }

  return val_type;
}

VikAggregateLayer *vik_aggregate_layer_create (VikViewport *vp)
{
  VikAggregateLayer *rv = vik_aggregate_layer_new ();
  vik_layer_rename ( VIK_LAYER(rv), vik_aggregate_layer_interface.name );
  return rv;
}

static VikAggregateLayer *aggregate_layer_copy ( VikAggregateLayer *val, gpointer vp )
{
  VikAggregateLayer *rv = vik_aggregate_layer_new ();
  VikLayer *child_layer;
  GList *child = val->children;
  while ( child )
  {
    child_layer = vik_layer_copy ( VIK_LAYER(child->data), vp );
    if ( child_layer )
      rv->children = g_list_append ( rv->children, child_layer );
      g_signal_connect_swapped ( G_OBJECT(child_layer), "update", G_CALLBACK(vik_layer_emit_update), rv );
    child = child->next;
  }
  return rv;
}

VikAggregateLayer *vik_aggregate_layer_new ()
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( g_object_new ( VIK_AGGREGATE_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(val), VIK_LAYER_AGGREGATE );
  val->children = NULL;
  return val;
}

void vik_aggregate_layer_insert_layer ( VikAggregateLayer *val, VikLayer *l, GtkTreeIter *replace_iter )
{
  GList *theone = g_list_find ( val->children, vik_treeview_item_get_pointer ( VIK_LAYER(val)->vt, replace_iter ) );
  GtkTreeIter iter;
  if ( VIK_LAYER(val)->realized )
  {
    vik_treeview_insert_layer ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter), &iter, l->name, val, l, l->type, l->type, replace_iter );
    if ( ! l->visible )
      vik_treeview_item_set_visible ( VIK_LAYER(val)->vt, &iter, FALSE );
    vik_layer_realize ( l, VIK_LAYER(val)->vt, &iter );

    if ( val->children == NULL )
      vik_treeview_expand ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter) );
  }
  val->children = g_list_insert ( val->children, l, g_list_position(val->children,theone)+1 );


  g_signal_connect_swapped ( G_OBJECT(l), "update", G_CALLBACK(vik_layer_emit_update), val );
}

void vik_aggregate_layer_add_layer ( VikAggregateLayer *val, VikLayer *l )
{
  GtkTreeIter iter;

  if ( VIK_LAYER(val)->realized )
  {
    vik_treeview_add_layer ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter), &iter, l->name, val, l, l->type, l->type);
    if ( ! l->visible )
      vik_treeview_item_set_visible ( VIK_LAYER(val)->vt, &iter, FALSE );
    vik_layer_realize ( l, VIK_LAYER(val)->vt, &iter );

    if ( val->children == NULL )
      vik_treeview_expand ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter) );
  }

  val->children = g_list_append ( val->children, l );
  g_signal_connect_swapped ( G_OBJECT(l), "update", G_CALLBACK(vik_layer_emit_update), val );
}

void vik_aggregate_layer_move_layer ( VikAggregateLayer *val, GtkTreeIter *child_iter, gboolean up )
{
  GList *theone, *first, *second;
  vik_treeview_move_item ( VIK_LAYER(val)->vt, child_iter, up );

  theone = g_list_find ( val->children, vik_treeview_item_get_pointer ( VIK_LAYER(val)->vt, child_iter ) );

  g_assert ( theone != NULL );

  /* the old switcheroo */
  if ( up && theone->next )
  {
    first = theone;
    second = theone->next;
  }
  else if ( !up && theone->prev )
  {
    first = theone->prev;
    second = theone;
  }
  else
    return;

  first->next = second->next;
  second->prev = first->prev;
  first->prev = second;
  second->next = first;

  /* second is now first */

  if ( second->prev )
    second->prev->next = second;
  if ( first->next )
    first->next->prev = first;

  if ( second->prev == NULL )
    val->children = second;
}

void vik_aggregate_layer_draw ( VikAggregateLayer *val, gpointer data )
{
  g_list_foreach ( val->children, (GFunc)(vik_layer_draw), data );
}

static void aggregate_layer_change_coord_mode ( VikAggregateLayer *val, VikCoordMode mode )
{
  GList *iter = val->children;
  while ( iter )
  {
    vik_layer_change_coord_mode ( VIK_LAYER(iter->data), mode );
    iter = iter->next;
  }
}

static void disconnect_layer_signal ( VikLayer *vl, VikAggregateLayer *val )
{
  g_assert(DISCONNECT_UPDATE_SIGNAL(vl,val)==1);
}

void vik_aggregate_layer_free ( VikAggregateLayer *val )
{
  g_list_foreach ( val->children, (GFunc)(disconnect_layer_signal), val );
  g_list_foreach ( val->children, (GFunc)(g_object_unref), NULL );
  g_list_free ( val->children );
}

static void delete_layer_iter ( VikLayer *vl )
{
  if ( vl->realized )
    vik_treeview_item_delete ( vl->vt, &(vl->iter) );
}

void vik_aggregate_layer_clear ( VikAggregateLayer *val )
{
  g_list_foreach ( val->children, (GFunc)(disconnect_layer_signal), val );
  g_list_foreach ( val->children, (GFunc)(delete_layer_iter), NULL );
  g_list_foreach ( val->children, (GFunc)(g_object_unref), NULL );
  g_list_free ( val->children );
  val->children = NULL;
}

gboolean vik_aggregate_layer_delete ( VikAggregateLayer *val, GtkTreeIter *iter )
{
  VikLayer *l = VIK_LAYER( vik_treeview_item_get_pointer ( VIK_LAYER(val)->vt, iter ) );
  gboolean was_visible = l->visible;

  vik_treeview_item_delete ( VIK_LAYER(val)->vt, iter );
  val->children = g_list_remove ( val->children, l );
  g_assert(DISCONNECT_UPDATE_SIGNAL(l,val)==1);
  g_object_unref ( l );

  return was_visible;
}

/* returns 0 == we're good, 1 == didn't find any layers, 2 == got rejected */
guint vik_aggregate_layer_tool ( VikAggregateLayer *val, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp )
{
  GList *iter = val->children;
  gboolean found_rej = FALSE;
  if (!iter)
    return FALSE;
  while (iter->next)
    iter = iter->next;

  while ( iter )
  {
    /* if this layer "accepts" the tool call */
    if ( VIK_LAYER(iter->data)->visible && VIK_LAYER(iter->data)->type == layer_type )
    {
      if ( tool_func ( VIK_LAYER(iter->data), event, vvp ) )
        return 0;
      else
        found_rej = TRUE;
    }

    /* recursive -- try the same for the child aggregate layer. */
    else if ( VIK_LAYER(iter->data)->visible && VIK_LAYER(iter->data)->type == VIK_LAYER_AGGREGATE )
    {
      gint rv = vik_aggregate_layer_tool(VIK_AGGREGATE_LAYER(iter->data), layer_type, tool_func, event, vvp);
      if ( rv == 0 )
        return 0;
      else if ( rv == 2 )
        found_rej = TRUE;
    }
    iter = iter->prev;
  }
  return found_rej ? 2 : 1; /* no one wanted to accept the tool call in this layer */
}

VikLayer *vik_aggregate_layer_get_top_visible_layer_of_type ( VikAggregateLayer *val, gint type )
{
  VikLayer *rv;
  GList *ls = val->children;
  if (!ls)
    return NULL;
  while (ls->next)
    ls = ls->next;

  while ( ls )
  {
    if ( VIK_LAYER(ls->data)->visible && VIK_LAYER(ls->data)->type == type )
      return VIK_LAYER(ls->data);
    else if ( VIK_LAYER(ls->data)->visible && VIK_LAYER(ls->data)->type == VIK_LAYER_AGGREGATE )
    {
      rv = vik_aggregate_layer_get_top_visible_layer_of_type(VIK_AGGREGATE_LAYER(ls->data), type);
      if ( rv )
        return rv;
    }
    ls = ls->prev;
  }
  return NULL;
}

void vik_aggregate_layer_realize ( VikAggregateLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  GList *i = val->children;
  GtkTreeIter iter;
  while ( i )
  {
    vik_treeview_add_layer ( VIK_LAYER(val)->vt, layer_iter, &iter, VIK_LAYER(i->data)->name, val, 
        VIK_LAYER(i->data), VIK_LAYER(i->data)->type, VIK_LAYER(i->data)->type );
    if ( ! VIK_LAYER(i->data)->visible )
      vik_treeview_item_set_visible ( VIK_LAYER(val)->vt, &iter, FALSE );
    vik_layer_realize ( VIK_LAYER(i->data), VIK_LAYER(val)->vt, &iter );
    i = i->next;
  }
}

const GList *vik_aggregate_layer_get_children ( VikAggregateLayer *val )
{
  return val->children;
}

gboolean vik_aggregate_layer_is_empty ( VikAggregateLayer *val )
{
  if ( val->children )
    return FALSE;
  return TRUE;
}
