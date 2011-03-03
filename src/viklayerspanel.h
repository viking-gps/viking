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

#ifndef _VIKING_LAYERS_PANEL_H
#define _VIKING_LAYERS_PANEL_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "viklayer.h"
#include "vikaggregatelayer.h"

G_BEGIN_DECLS

#define VIK_LAYERS_PANEL_TYPE            (vik_layers_panel_get_type ())
#define VIK_LAYERS_PANEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_LAYERS_PANEL_TYPE, VikLayersPanel))
#define VIK_LAYERS_PANEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_LAYERS_PANEL_TYPE, VikLayersPanelClass))
#define IS_VIK_LAYERS_PANEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_LLAYERS_PANEL_TYPE))
#define IS_VIK_LAYERS_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_LAYERS_PANEL_TYPE))

typedef struct _VikLayersPanel VikLayersPanel;
typedef struct _VikLayersPanelClass VikLayersPanelClass;

struct _VikLayersPanelClass
{
  GtkVBoxClass vbox_class;

  void (* update) (VikLayersPanel *vlp);
};

GType vik_layers_panel_get_type ();
VikLayersPanel *vik_layers_panel_new ();
void vik_layers_panel_free ( VikLayersPanel *vlp );
void vik_layers_panel_add_layer ( VikLayersPanel *vlp, VikLayer *l );
void vik_layers_panel_draw_all ( VikLayersPanel *vlp );
void vik_layers_panel_draw_all_using_viewport ( VikLayersPanel *vlp, VikViewport *vvp );
VikLayer *vik_layers_panel_get_selected ( VikLayersPanel *vlp );
void vik_layers_panel_cut_selected ( VikLayersPanel *vlp );
void vik_layers_panel_copy_selected ( VikLayersPanel *vlp );
void vik_layers_panel_paste_selected ( VikLayersPanel *vlp );
void vik_layers_panel_delete_selected ( VikLayersPanel *vlp );
VikLayer *vik_layers_panel_get_layer_of_type ( VikLayersPanel *vlp, gint type );
void vik_layers_panel_set_viewport ( VikLayersPanel *vlp, VikViewport *vvp );
//gboolean vik_layers_panel_tool ( VikLayersPanel *vlp, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp );
VikViewport *vik_layers_panel_get_viewport ( VikLayersPanel *vlp );
void vik_layers_panel_emit_update ( VikLayersPanel *vlp );
VikLayer *vik_layers_panel_get_layer_of_type ( VikLayersPanel *vlp, gint type );
gboolean vik_layers_panel_properties ( VikLayersPanel *vlp );
gboolean vik_layers_panel_new_layer ( VikLayersPanel *vlp, gint type );
void vik_layers_panel_clear ( VikLayersPanel *vlp );
VikAggregateLayer *vik_layers_panel_get_top_layer ( VikLayersPanel *vlp );
void vik_layers_panel_change_coord_mode ( VikLayersPanel *vlp, VikCoordMode mode );
GList *vik_layers_panel_get_all_layers_of_type(VikLayersPanel *vlp, gint type, gboolean include_invisible);
VikTreeview *vik_layers_panel_get_treeview ( VikLayersPanel *vlp );

G_END_DECLS

#endif
