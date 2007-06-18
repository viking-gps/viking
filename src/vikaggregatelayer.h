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

#ifndef _VIKING_AGGREGATELAYER_H
#define _VIKING_AGGREGATELAYER_H

#include <time.h>

#include "viklayer.h"

#define VIK_AGGREGATE_LAYER_TYPE            (vik_aggregate_layer_get_type ())
#define VIK_AGGREGATE_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_AGGREGATE_LAYER_TYPE, VikAggregateLayer))
#define VIK_AGGREGATE_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_AGGREGATE_LAYER_TYPE, VikAggregateLayerClass))
#define IS_VIK_AGGREGATE_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_AGGREGATE_LAYER_TYPE))
#define IS_VIK_AGGREGATE_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_AGGREGATE_LAYER_TYPE))

typedef struct _VikAggregateLayerClass VikAggregateLayerClass;
struct _VikAggregateLayerClass
{
  VikLayerClass vik_layer_class;
};

GType vik_aggregate_layer_get_type ();

typedef struct _VikAggregateLayer VikAggregateLayer;

VikAggregateLayer *vik_aggregate_layer_new ();
void vik_aggregate_layer_add_layer ( VikAggregateLayer *val, VikLayer *l );
void vik_aggregate_layer_insert_layer ( VikAggregateLayer *val, VikLayer *l, GtkTreeIter *replace_layer );
void vik_aggregate_layer_move_layer ( VikAggregateLayer *val, GtkTreeIter *child_iter, gboolean up );
void vik_aggregate_layer_draw ( VikAggregateLayer *val, gpointer data );
void vik_aggregate_layer_free ( VikAggregateLayer *val );
void vik_aggregate_layer_clear ( VikAggregateLayer *val );
gboolean vik_aggregate_layer_delete ( VikAggregateLayer *val, GtkTreeIter *iter );
VikAggregateLayer *vik_aggregate_layer_create (VikViewport *vp);

/* returns: 0 = success, 1 = none appl. found, 2 = found but rejected */
// guint vik_aggregate_layer_tool ( VikAggregateLayer *val, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp);

VikLayer *vik_aggregate_layer_get_top_visible_layer_of_type ( VikAggregateLayer *val, gint type );
void vik_aggregate_layer_realize ( VikAggregateLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter );
gboolean vik_aggregate_layer_load_layers ( VikAggregateLayer *val, FILE *f, gpointer vp );
gboolean vik_aggregate_layer_is_empty ( VikAggregateLayer *val );

const GList *vik_aggregate_layer_get_children ( VikAggregateLayer *val );
GList *vik_aggregate_layer_get_all_layers_of_type(VikAggregateLayer *val, GList *layers, gint type);


#endif
