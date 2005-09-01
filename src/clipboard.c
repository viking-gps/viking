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

/* static for a very good reason -- share between windows */

#define DATA_NONE 0
#define DATA_LAYER 1
#define DATA_SUBLAYER 2

guint8 type = DATA_NONE;
gint subtype;
guint16 layer_type;
gpointer clipboard;

void a_clipboard_copy ( VikLayersPanel *vlp )
{
  VikLayer *sel = vik_layers_panel_get_selected ( vlp );
  GtkTreeIter iter;
  if ( ! sel )
    return;

  vik_treeview_get_selected_iter ( sel->vt, &iter );

  if ( clipboard )
    a_clipboard_uninit();

  if ( vik_treeview_item_get_type ( sel->vt, &iter ) == VIK_TREEVIEW_TYPE_SUBLAYER )
  {
    layer_type = sel->type;
    if ( vik_layer_get_interface(layer_type)->copy_item && (clipboard = vik_layer_get_interface(layer_type)->
        copy_item(sel,subtype=vik_treeview_item_get_data(sel->vt,&iter),vik_treeview_item_get_pointer(sel->vt,&iter)) ))
      type = DATA_SUBLAYER;
  }
  else
  {
    clipboard = sel;
    g_object_ref ( G_OBJECT(sel) );
    type = DATA_LAYER;
  }
}

gboolean a_clipboard_paste ( VikLayersPanel *vlp )
{
  if ( clipboard && type == DATA_LAYER )
  {
    /* oooh, _private_ ... so sue me, there's no other way to do this. */
    if ( G_OBJECT(clipboard)->ref_count == 1 )
    { /* optimization -- if layer has been deleted, don't copy the layer. */
      g_object_ref ( G_OBJECT(clipboard) );
      vik_layers_panel_add_layer ( vlp, VIK_LAYER(clipboard) );
      return TRUE;
    }
    else
    {
      VikLayer *new_layer = vik_layer_copy ( VIK_LAYER(clipboard), vik_layers_panel_get_viewport(vlp) );
      if ( new_layer )
      {
        vik_layers_panel_add_layer ( vlp, new_layer );
        return TRUE;
      }
    }
  }
  else if ( clipboard && type == DATA_SUBLAYER )
  {
    VikLayer *sel = vik_layers_panel_get_selected ( vlp );
    if ( sel && sel->type == layer_type)
    {
      if ( vik_layer_get_interface(layer_type)->paste_item )
        return vik_layer_get_interface(layer_type)->paste_item ( sel, subtype, clipboard );
    }
    else
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET(vlp)), "The clipboard contains sublayer data for a %s layers. You must select a layer of this type to paste the clipboard data.", vik_layer_get_interface(layer_type)->name );
    return FALSE;
  }
  return FALSE;
}

void a_clipboard_uninit ()
{
  if ( clipboard && type == DATA_LAYER )
    g_object_unref ( G_OBJECT(clipboard) );
  else if ( clipboard && type == DATA_SUBLAYER )
    if ( vik_layer_get_interface(layer_type)->free_copied_item )
      vik_layer_get_interface(layer_type)->free_copied_item(subtype,clipboard);
  clipboard = NULL;
  type = DATA_NONE;
}
