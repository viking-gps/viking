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
#ifndef _VIKING_LAYER_H
#define _VIKING_LAYER_H

#include <stdio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "uibuilder.h"
#include "vikwindow.h"
#include "viktreeview.h"
#include "vikviewport.h"

#define VIK_LAYER_TYPE            (vik_layer_get_type ())
#define VIK_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_LAYER_TYPE, VikLayer))
#define VIK_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_LAYER_TYPE, VikLayerClass))
#define IS_VIK_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_LAYER_TYPE))
#define IS_VIK_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_LAYER_TYPE))

typedef struct _VikLayer VikLayer;
typedef struct _VikLayerClass VikLayerClass;

struct _VikLayerClass
{
  GObjectClass object_class;
  void (* update) (VikLayer *vl);
};

GType vik_layer_get_type ();

struct _VikLayer {
  GObject obj;
  gchar *name;
  gboolean visible;

  gboolean realized;
  VikTreeview *vt; /* simply a reference */
  GtkTreeIter iter;

  /* for explicit "polymorphism" (function type switching) */
  guint16 type;
};



enum {
  VIK_LAYER_AGGREGATE = 0,
  VIK_LAYER_TRW,
  VIK_LAYER_COORD,
  VIK_LAYER_GEOREF,
  VIK_LAYER_GPS,
  VIK_LAYER_MAPS,
  VIK_LAYER_DEM,
  VIK_LAYER_NUM_TYPES
};

/* I think most of these are ignored,
 * returning GRAB_FOCUS grabs the focus for mouse move,
 * mouse click, release always grabs focus. Focus allows key presses
 * to be handled.
 * It used to be that, if ignored, Viking could look for other layers.
 * this was useful for clicking a way/trackpoint in any layer,
 * if no layer was selected (find way/trackpoint)
 */
typedef enum { 
  VIK_LAYER_TOOL_IGNORED=0,
  VIK_LAYER_TOOL_ACK,
  VIK_LAYER_TOOL_ACK_REDRAW_ABOVE,
  VIK_LAYER_TOOL_ACK_REDRAW_ALL,
  VIK_LAYER_TOOL_ACK_REDRAW_IF_VISIBLE,
  VIK_LAYER_TOOL_ACK_GRAB_FOCUS, /* only for move */
} VikLayerToolFuncStatus;

/* gpointer is tool-specific state created in the constructor */
typedef gpointer (*VikToolConstructorFunc) (VikWindow *, VikViewport *);
typedef void (*VikToolDestructorFunc) (gpointer);
typedef VikLayerToolFuncStatus (*VikToolMouseFunc) (VikLayer *, GdkEventButton *, gpointer);
typedef VikLayerToolFuncStatus (*VikToolMouseMoveFunc) (VikLayer *, GdkEventMotion *, gpointer);
typedef void (*VikToolActivationFunc) (VikLayer *, gpointer);
typedef gboolean (*VikToolKeyFunc) (VikLayer *, GdkEventKey *, gpointer);

typedef struct _VikToolInterface VikToolInterface;
struct _VikToolInterface {
  gchar *name;
  VikToolConstructorFunc create;
  VikToolDestructorFunc destroy;
  VikToolActivationFunc activate;
  VikToolActivationFunc deactivate;
  VikToolMouseFunc click;
  VikToolMouseMoveFunc move;
  VikToolMouseFunc release;
  VikToolKeyFunc key_press; /* return FALSE if we don't use the key press -- should return AFLSE most of the time if we want any shortcuts / UI keybindings to work! use sparingly. */
  GdkCursorType cursor_type;
  const GdkPixdata *cursor_data;
  const GdkCursor *cursor;
};

/* Parameters (for I/O and Properties) */
/* --> moved to uibuilder.h */


/* layer interface functions */

/* Create a new layer of a certain type. Should be filled with defaults */
typedef VikLayer *    (*VikLayerFuncCreate)                (VikViewport *);

/* normally only needed for layers with sublayers. This is called when they
 * are added to the treeview so they can add sublayers to the treeview. */
typedef void          (*VikLayerFuncRealize)               (VikLayer *,VikTreeview *,GtkTreeIter *);

/* rarely used, this is called after a read operation or properties box is run.
 * usually used to create GC's that depend on params,
 * but GC's can also be created from create() or set_param() */
typedef void          (*VikLayerFuncPostRead)              (VikLayer *,VikViewport *vp,gboolean from_file);

typedef void          (*VikLayerFuncFree)                  (VikLayer *);

/* do _not_ use this unless absolutely neccesary. Use the dynamic properties (see coordlayer for example)
  * returns TRUE if OK was pressed */
typedef gboolean      (*VikLayerFuncProperties)            (VikLayer *,VikViewport *);

typedef void          (*VikLayerFuncDraw)                  (VikLayer *,VikViewport *);
typedef void          (*VikLayerFuncChangeCoordMode)       (VikLayer *,VikCoordMode);

typedef void          (*VikLayerFuncSetMenuItemsSelection)          (VikLayer *,guint16);
typedef guint16          (*VikLayerFuncGetMenuItemsSelection)          (VikLayer *);

typedef void          (*VikLayerFuncAddMenuItems)          (VikLayer *,GtkMenu *,gpointer); /* gpointer is a VikLayersPanel */
typedef gboolean      (*VikLayerFuncSublayerAddMenuItems)  (VikLayer *,GtkMenu *,gpointer, /* first gpointer is a VikLayersPanel */
                                                            gint,gpointer,GtkTreeIter *);
typedef const gchar * (*VikLayerFuncSublayerRenameRequest) (VikLayer *,const gchar *,gpointer,
                                                            gint,VikViewport *,GtkTreeIter *); /* first gpointer is a VikLayersPanel */
typedef gboolean      (*VikLayerFuncSublayerToggleVisible) (VikLayer *,gint,gpointer);
typedef const gchar * (*VikLayerFuncSublayerTooltip)       (VikLayer *,gint,gpointer);
typedef const gchar * (*VikLayerFuncLayerTooltip)          (VikLayer *);

typedef void          (*VikLayerFuncMarshall)              (VikLayer *, guint8 **, gint *);
typedef VikLayer *    (*VikLayerFuncUnmarshall)            (guint8 *, gint, VikViewport *);

/* returns TRUE if needs to redraw due to changed param */
/* in parameter gboolean denotes if for file I/O, as opposed to display/cut/copy etc... operations */
typedef gboolean      (*VikLayerFuncSetParam)              (VikLayer *, guint16, VikLayerParamData, VikViewport *, gboolean);

/* in parameter gboolean denotes if for file I/O, as opposed to display/cut/copy etc... operations */
typedef VikLayerParamData
                      (*VikLayerFuncGetParam)              (VikLayer *, guint16, gboolean);

typedef void          (*VikLayerFuncReadFileData)          (VikLayer *, FILE *);
typedef void          (*VikLayerFuncWriteFileData)         (VikLayer *, FILE *);

/* item manipulation */
typedef void          (*VikLayerFuncDeleteItem)            (VikLayer *, gint, gpointer);
                                                         /*      layer, subtype, pointer to sub-item */
typedef void          (*VikLayerFuncCutItem)               (VikLayer *, gint, gpointer);
typedef void          (*VikLayerFuncCopyItem)              (VikLayer *, gint, gpointer, guint8 **, guint *);
                                                         /*      layer, subtype, pointer to sub-item, return pointer, return len */
typedef gboolean      (*VikLayerFuncPasteItem)             (VikLayer *, gint, guint8 *, guint);
typedef void          (*VikLayerFuncFreeCopiedItem)        (gint, gpointer);

/* treeview drag and drop method. called on the destination layer. it is given a source and destination layer, 
 * and the source and destination iters in the treeview. 
 */
typedef void 	      (*VikLayerFuncDragDropRequest)       (VikLayer *, VikLayer *, GtkTreeIter *, GtkTreePath *);

typedef enum {
  VIK_MENU_ITEM_PROPERTY=1,
  VIK_MENU_ITEM_CUT=2,
  VIK_MENU_ITEM_COPY=4,
  VIK_MENU_ITEM_PASTE=8,
  VIK_MENU_ITEM_DELETE=16,
  VIK_MENU_ITEM_ALL=0xff
} VikStdLayerMenuItem;

typedef struct _VikLayerInterface VikLayerInterface;

/* See vik_layer_* for function parameter names */
struct _VikLayerInterface {
  const gchar *                     name;
  const GdkPixdata *                icon;

  VikToolInterface *                tools;
  guint16                           tools_count;

  /* for I/O reading to and from .vik files -- params like coordline width, color, etc. */
  VikLayerParam *                   params;
  guint16                           params_count;
  gchar **                          params_groups;
  guint8                            params_groups_count;

  /* menu items to be created */
  VikStdLayerMenuItem               menu_items_selection;

  VikLayerFuncCreate                create;
  VikLayerFuncRealize               realize;
  VikLayerFuncPostRead              post_read;
  VikLayerFuncFree                  free;

  VikLayerFuncProperties            properties;
  VikLayerFuncDraw                  draw;
  VikLayerFuncChangeCoordMode       change_coord_mode;

  VikLayerFuncSetMenuItemsSelection set_menu_selection;
  VikLayerFuncGetMenuItemsSelection get_menu_selection;

  VikLayerFuncAddMenuItems          add_menu_items;
  VikLayerFuncSublayerAddMenuItems  sublayer_add_menu_items;
  VikLayerFuncSublayerRenameRequest sublayer_rename_request;
  VikLayerFuncSublayerToggleVisible sublayer_toggle_visible;
  VikLayerFuncSublayerTooltip       sublayer_tooltip;
  VikLayerFuncLayerTooltip          layer_tooltip;

  VikLayerFuncMarshall              marshall;
  VikLayerFuncUnmarshall            unmarshall;

  /* for I/O */
  VikLayerFuncSetParam              set_param;
  VikLayerFuncGetParam              get_param;

  /* for I/O -- extra non-param data like TrwLayer data */
  VikLayerFuncReadFileData          read_file_data;
  VikLayerFuncWriteFileData         write_file_data;

  VikLayerFuncDeleteItem            delete_item;
  VikLayerFuncCutItem               cut_item;
  VikLayerFuncCopyItem              copy_item;
  VikLayerFuncPasteItem             paste_item;
  VikLayerFuncFreeCopiedItem        free_copied_item;

  VikLayerFuncDragDropRequest       drag_drop_request;
};

VikLayerInterface *vik_layer_get_interface ( gint type );


void vik_layer_init ( VikLayer *vl, gint type );
void vik_layer_draw ( VikLayer *l, gpointer data );
void vik_layer_change_coord_mode ( VikLayer *l, VikCoordMode mode );
void vik_layer_rename ( VikLayer *l, const gchar *new_name );
void vik_layer_rename_no_copy ( VikLayer *l, gchar *new_name );
const gchar *vik_layer_get_name ( VikLayer *l );

gboolean vik_layer_set_param (VikLayer *layer, guint16 id, VikLayerParamData data, gpointer vp, gboolean is_file_operation);

void vik_layer_emit_update ( VikLayer *vl );

/* GUI */
void vik_layer_set_menu_items_selection(VikLayer *l, guint16 selection);
guint16 vik_layer_get_menu_items_selection(VikLayer *l);
void vik_layer_add_menu_items ( VikLayer *l, GtkMenu *menu, gpointer vlp );
VikLayer *vik_layer_create ( gint type, gpointer vp, GtkWindow *w, gboolean interactive );
gboolean vik_layer_properties ( VikLayer *layer, gpointer vp );

void vik_layer_realize ( VikLayer *l, VikTreeview *vt, GtkTreeIter * layer_iter );
void vik_layer_post_read ( VikLayer *layer, VikViewport *vp, gboolean from_file );

gboolean vik_layer_sublayer_add_menu_items ( VikLayer *l, GtkMenu *menu, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter );

VikLayer *vik_layer_copy ( VikLayer *vl, gpointer vp );
void      vik_layer_marshall ( VikLayer *vl, guint8 **data, gint *len );
VikLayer *vik_layer_unmarshall ( guint8 *data, gint len, VikViewport *vvp );
void      vik_layer_marshall_params ( VikLayer *vl, guint8 **data, gint *len );
void      vik_layer_unmarshall_params ( VikLayer *vl, guint8 *data, gint len, VikViewport *vvp );

const gchar *vik_layer_sublayer_rename_request ( VikLayer *l, const gchar *newname, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter );

gboolean vik_layer_sublayer_toggle_visible ( VikLayer *l, gint subtype, gpointer sublayer );

const gchar* vik_layer_sublayer_tooltip ( VikLayer *l, gint subtype, gpointer sublayer );

const gchar* vik_layer_layer_tooltip ( VikLayer *l );

/* TODO: put in layerspanel */
GdkPixbuf *vik_layer_load_icon ( gint type );

VikLayer *vik_layer_get_and_reset_trigger();
void vik_layer_emit_update_secondary ( VikLayer *vl ); /* to be called by aggregate layer only. doesn't set the trigger */
void vik_layer_emit_update_although_invisible ( VikLayer *vl );

#endif
