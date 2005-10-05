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

#include <stdio.h>
#include <string.h>
#include "viking.h"

#define DATA_NONE 0
#define DATA_LAYER 1
#define DATA_SUBLAYER 2

typedef struct {
  gpointer clipboard;
  guint8 type;
  gint subtype;
  guint16 layer_type;
} vik_clipboard_t;

static GtkTargetEntry target_table[] = {
  { "application/viking", 0, 0 },
  { "STRING", 0, 1 },
};

/**************************************
 ** paste from clipboard functions   **
 **************************************/

static void clip_get ( GtkClipboard *c, GtkSelectionData *selection_data, guint info, gpointer p ) 
{
  vik_clipboard_t *vc = p;
  g_print("clip_get\n");
  if (info==0) {
    gtk_selection_data_set ( selection_data, selection_data->target, 8, (void *)vc, sizeof(*vc) );
  }
  if (info==1) {
    if (vc->type == DATA_LAYER) {
      gtk_selection_data_set_text ( selection_data, VIK_LAYER(vc->clipboard)->name, -1 );
    } 
  }
}

static void clip_clear ( GtkClipboard *c, gpointer p )
{
  vik_clipboard_t *vc = p;
  g_print("clip_clear\n");

  if ( vc->clipboard && vc->type == DATA_LAYER )
    g_object_unref ( G_OBJECT(vc->clipboard) );
  else if ( vc->clipboard && vc->type == DATA_SUBLAYER )
    if ( vik_layer_get_interface(vc->layer_type)->free_copied_item )
      vik_layer_get_interface(vc->layer_type)->free_copied_item(vc->subtype,vc->clipboard);
  vc->clipboard = NULL;
  vc->type = DATA_NONE;
}


/******************************************************
 ** functions which receive from the clipboard owner **
 ******************************************************/

/* our own data type */
static void clip_receive_viking ( GtkClipboard *c, GtkSelectionData *sd, gpointer p ) 
{
  VikLayersPanel *vlp = p;
  vik_clipboard_t *vc;
  if (sd->length == -1) {
    g_print("receive failed\n");
    return;
  } 
  g_print("clip receive: target = %s, type = %s\n", gdk_atom_name(sd->target), gdk_atom_name(sd->type));
  g_assert(!strcmp(gdk_atom_name(sd->target), target_table[0].target));
  g_assert(sd->length == sizeof(*vc));

  vc = (vik_clipboard_t *)sd->data;

  if ( vc->clipboard && vc->type == DATA_LAYER )
  {
    /* oooh, _private_ ... so sue me, there's no other way to do this. */
    if ( G_OBJECT(vc->clipboard)->ref_count == 1 )
    { /* optimization -- if layer has been deleted, don't copy the layer. */
      g_object_ref ( G_OBJECT(vc->clipboard) );
      vik_layers_panel_add_layer ( vlp, VIK_LAYER(vc->clipboard) );
    }
    else
    {
      VikLayer *new_layer = vik_layer_copy ( VIK_LAYER(vc->clipboard), vik_layers_panel_get_viewport(vlp) );
      if ( new_layer )
      {
        vik_layers_panel_add_layer ( vlp, new_layer );
      }
    }
  }
  else if ( vc->clipboard && vc->type == DATA_SUBLAYER )
  {
    VikLayer *sel = vik_layers_panel_get_selected ( vlp );
    if ( sel && sel->type == vc->layer_type)
    {
      if ( vik_layer_get_interface(vc->layer_type)->paste_item )
        vik_layer_get_interface(vc->layer_type)->paste_item ( sel, vc->subtype, vc->clipboard );
    }
    else
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET(vlp)), "The clipboard contains sublayer data for a %s layers. You must select a layer of this type to paste the clipboard data.", vik_layer_get_interface(vc->layer_type)->name );
  }
}


/*
 * - search for N dd.dddddd W dd.dddddd, N dd° dd.dddd W dd° dd.ddddd and so forth
 */
static void clip_receive_text (GtkClipboard *c, const gchar *text, gpointer p)
{
  VikLayersPanel *vlp = p;
  g_print("Pasted text is ""%s""\n", text);
}

/*
 * - copying from Mozilla seems to give html in UTF-16.
 * - search for <span id="LatLon">...</span> like on geocaching.com and make a new waypoint
 * - search for N dd.dddddd W dd.dddddd, N dd° dd.dddd W dd° dd.ddddd and so forth
 */
static void clip_receive_html ( GtkClipboard *c, GtkSelectionData *sd, gpointer p ) 
{
  VikLayersPanel *vlp = p;
  gint r, w;
  GError *err = NULL;
  gchar *s;

  if (sd->length == -1) {
    return;
  } 

  if (!(s =  g_convert ( sd->data, sd->length, "UTF-8", "UTF-16", &r, &w, &err))) {
    return;
  }
  g_print("html is %d bytes long: %s\n", sd->length, s);
  g_free(s);
}

/*
 * deal with various data types a clipboard may hold 
 */
void clip_receive_targets ( GtkClipboard *c, GdkAtom *a, gint n, gpointer p )
{
  VikLayersPanel *vlp = p;
  gint i;

  g_print("got targets\n");
  for (i=0; i<n; i++) {
    g_print("  ""%s""\n", gdk_atom_name(a[i]));
    if (!strcmp(gdk_atom_name(a[i]), "text/html")) {
      gtk_clipboard_request_contents ( c, gdk_atom_intern("text/html", TRUE), clip_receive_html, vlp );
      break;
    }
    if (a[i] == GDK_TARGET_STRING) {
      gtk_clipboard_request_text ( c, clip_receive_text, vlp );
      break;
    }
    if (!strcmp(gdk_atom_name(a[i]), "application/viking")) {
      gtk_clipboard_request_contents ( c, gdk_atom_intern("application/viking", TRUE), clip_receive_viking, vlp );
      break;
    }
  }
}

/*********************************************************************************
 ** public functions                                                            **
 *********************************************************************************/

/* 
 * make a copy of selected object and associate ourselves with the clipboard
 */
void a_clipboard_copy ( VikLayersPanel *vlp )
{
  VikLayer *sel = vik_layers_panel_get_selected ( vlp );
  GtkTreeIter iter;
  vik_clipboard_t *vc = g_malloc(sizeof(*vc));
  GtkClipboard *c = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );

  if ( ! sel )
    return;

  vik_treeview_get_selected_iter ( sel->vt, &iter );

  if ( vik_treeview_item_get_type ( sel->vt, &iter ) == VIK_TREEVIEW_TYPE_SUBLAYER )
  {
    vc->layer_type = sel->type;
    if ( vik_layer_get_interface(vc->layer_type)->copy_item && (vc->clipboard = vik_layer_get_interface(vc->layer_type)->
        copy_item(sel,vc->subtype=vik_treeview_item_get_data(sel->vt,&iter),vik_treeview_item_get_pointer(sel->vt,&iter)) ))
      vc->type = DATA_SUBLAYER;
  }
  else
  {
    vc->clipboard = sel;
    g_object_ref ( G_OBJECT(sel) );
    vc->type = DATA_LAYER;
  }
  gtk_clipboard_set_with_data ( c, target_table, G_N_ELEMENTS(target_table), clip_get, clip_clear, vc );
}

/*
 * to deal with multiple data types, we first request the type of data on the clipboard,
 * and handle them in the callback
 */
gboolean a_clipboard_paste ( VikLayersPanel *vlp )
{
  GtkClipboard *c = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );
  gtk_clipboard_request_targets ( c, clip_receive_targets, vlp );
  return TRUE;
}

void a_clipboard_uninit ()
{
  /* not needed any longer */
}
