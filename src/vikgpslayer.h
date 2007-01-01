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

#ifndef _VIKING_GPSLAYER_H
#define _VIKING_GPSLAYER_H

#include <time.h>

#include "viklayer.h"

#define VIK_GPS_LAYER_TYPE            (vik_gps_layer_get_type ())
#define VIK_GPS_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_GPS_LAYER_TYPE, VikGpsLayer))
#define VIK_GPS_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_GPS_LAYER_TYPE, VikGpsLayerClass))
#define IS_VIK_GPS_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_GPS_LAYER_TYPE))
#define IS_VIK_GPS_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_GPS_LAYER_TYPE))

typedef struct _VikGpsLayerClass VikGpsLayerClass;
struct _VikGpsLayerClass
{
  VikLayerClass vik_layer_class;
};

GType vik_gps_layer_get_type ();

typedef struct _VikGpsLayer VikGpsLayer;

VikGpsLayer *vik_gps_layer_new ();
void vik_gps_layer_add_layer ( VikGpsLayer *val, VikLayer *l );
void vik_gps_layer_insert_layer ( VikGpsLayer *val, VikLayer *l, GtkTreeIter *replace_layer );
void vik_gps_layer_move_layer ( VikGpsLayer *val, GtkTreeIter *child_iter, gboolean up );
void vik_gps_layer_draw ( VikGpsLayer *val, gpointer data );
void vik_gps_layer_free ( VikGpsLayer *val );
void vik_gps_layer_clear ( VikGpsLayer *val );
gboolean vik_gps_layer_delete ( VikGpsLayer *val, GtkTreeIter *iter );
VikGpsLayer *vik_gps_layer_create (VikViewport *vp);

/* returns: 0 = success, 1 = none appl. found, 2 = found but rejected */
// guint vik_gps_layer_tool ( VikGpsLayer *val, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp);

VikLayer *vik_gps_layer_get_top_visible_layer_of_type ( VikGpsLayer *val, gint type );
void vik_gps_layer_realize ( VikGpsLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter );
gboolean vik_gps_layer_load_layers ( VikGpsLayer *val, FILE *f, gpointer vp );
gboolean vik_gps_layer_is_empty ( VikGpsLayer *val );

const GList *vik_gps_layer_get_children ( VikGpsLayer *val );


#endif
