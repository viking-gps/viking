/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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
#ifndef _VIKING_UIBUILDER_H
#define _VIKING_UIBUILDER_H

#include <gtk/gtk.h>
#include "vik_compat.h"
#include "config.h"

G_BEGIN_DECLS

/* Parameters (for I/O and Properties) */

typedef union {
  gdouble d;
  guint32 u;
  gint32 i;
  gboolean b;
  const gchar *s;
  GdkColor c;
  GList *sl;
  gpointer ptr; // For internal usage - don't save this value in a file!
} VikLayerParamData;

typedef enum {
  VIK_LAYER_WIDGET_CHECKBUTTON=0,
  VIK_LAYER_WIDGET_RADIOGROUP,
  VIK_LAYER_WIDGET_RADIOGROUP_STATIC,
  VIK_LAYER_WIDGET_SPINBUTTON,
  VIK_LAYER_WIDGET_ENTRY,
  VIK_LAYER_WIDGET_PASSWORD,
  VIK_LAYER_WIDGET_FILEENTRY,
  VIK_LAYER_WIDGET_FOLDERENTRY,
  VIK_LAYER_WIDGET_HSCALE,
  VIK_LAYER_WIDGET_COLOR,
  VIK_LAYER_WIDGET_COMBOBOX,
  VIK_LAYER_WIDGET_FILELIST,
  VIK_LAYER_WIDGET_BUTTON,
} VikLayerWidgetType;

/* id is index */
typedef enum {
VIK_LAYER_PARAM_DOUBLE=1,
VIK_LAYER_PARAM_UINT,
VIK_LAYER_PARAM_INT,

/* in my_layer_set_param, if you want to use the string, you should dup it
 * in my_layer_get_param, the string returned will NOT be free'd, you are responsible for managing it (I think) */
VIK_LAYER_PARAM_STRING,
VIK_LAYER_PARAM_BOOLEAN,
VIK_LAYER_PARAM_COLOR,

/* NOTE: string list works uniquely: data.sl should NOT be free'd when
 * the internals call get_param -- i.e. it should be managed w/in the layer.
 * The value passed by the internals into set_param should also be managed
 * by the layer -- i.e. free'd by the layer.
 */

VIK_LAYER_PARAM_STRING_LIST,
VIK_LAYER_PARAM_PTR, // Not really a 'parameter' but useful to route to extended configuration (e.g. toolbar order)
} VikLayerParamType;

typedef enum {
  VIK_LAYER_AGGREGATE = 0,
  VIK_LAYER_TRW,
  VIK_LAYER_COORD,
  VIK_LAYER_GEOREF,
  VIK_LAYER_GPS,
  VIK_LAYER_MAPS,
  VIK_LAYER_DEM,
#ifdef HAVE_LIBMAPNIK
  VIK_LAYER_MAPNIK,
#endif
  VIK_LAYER_NUM_TYPES // Also use this value to indicate no layer association
} VikLayerTypeEnum;

// Default value has to be returned via a function
//  because certain types value are can not be statically allocated
//  (i.e. a string value that is dependent on other functions)
// Also easier for colours to be set via a function call rather than a static assignment
typedef VikLayerParamData (*VikLayerDefaultFunc) ( void );

// Convert between the value held internally and the value used for display
//  e.g. keep the internal value in seconds yet use days in the display
typedef VikLayerParamData (*VikLayerConvertFunc) ( VikLayerParamData );

typedef struct {
  VikLayerTypeEnum layer;
  const gchar *name;
  VikLayerParamType type;
  gint16 group;
  const gchar *title;
  VikLayerWidgetType widget_type;
  gpointer widget_data;
  gpointer extra_widget_data;
  const gchar *tooltip;
  VikLayerDefaultFunc default_value;
  VikLayerConvertFunc convert_to_display;
  VikLayerConvertFunc convert_to_internal;
} VikLayerParam;

enum {
VIK_LAYER_NOT_IN_PROPERTIES=-2,
VIK_LAYER_GROUP_NONE=-1
};

typedef struct {
  gdouble min;
  gdouble max;
  gdouble step;
  guint8 digits;
} VikLayerParamScale;


  /* Annoyingly 'C' cannot initialize unions properly */
  /* It's dependent on the standard used or the compiler support... */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L || __GNUC__
#define VIK_LPD_BOOLEAN(X)     (VikLayerParamData) { .b = (X) }
#define VIK_LPD_INT(X)         (VikLayerParamData) { .u = (X) }
#define VIK_LPD_UINT(X)        (VikLayerParamData) { .i = (X) }
#define VIK_LPD_COLOR(X,Y,Z,A) (VikLayerParamData) { .c = (GdkColor){ (X), (Y), (Z), (A) } }
#define VIK_LPD_DOUBLE(X)      (VikLayerParamData) { .d = (X) }
#else
#define VIK_LPD_BOOLEAN(X)     (VikLayerParamData) { (X) }
#define VIK_LPD_INT(X)         (VikLayerParamData) { (X) }
#define VIK_LPD_UINT(X)        (VikLayerParamData) { (X) }
#define VIK_LPD_COLOR(X,Y,Z,A) (VikLayerParamData) { (X), (Y), (Z), (A) }
#define VIK_LPD_DOUBLE(X)      (VikLayerParamData) { (X) }
#endif

VikLayerParamData vik_lpd_true_default ( void );
VikLayerParamData vik_lpd_false_default ( void );

typedef enum {
  UI_CHG_LAYER = 0,
  UI_CHG_PARAM,
  UI_CHG_PARAM_ID,
  UI_CHG_WIDGETS,
  UI_CHG_LABELS,
  UI_CHG_LAST
} ui_change_index;

typedef gpointer ui_change_values[UI_CHG_LAST];

GtkWidget *a_uibuilder_new_widget ( VikLayerParam *param, VikLayerParamData data );
VikLayerParamData a_uibuilder_widget_get_value ( GtkWidget *widget, VikLayerParam *param );
gint a_uibuilder_properties_factory ( const gchar *dialog_name,
                                      GtkWindow *parent,
                                      VikLayerParam *params,
                                      guint16 params_count,
                                      gchar **groups,
                                      guint8 groups_count,
                                      gboolean (*setparam) (gpointer,guint16,VikLayerParamData,gpointer,gboolean), // AKA VikLayerFuncSetParam in viklayer.h
                                      gpointer pass_along1,
                                      gpointer pass_along2,
                                      VikLayerParamData (*getparam) (gpointer,guint16,gboolean),  // AKA VikLayerFuncGetParam in viklayer.h
                                      gpointer pass_along_getparam,
                                      void (*changeparam) (GtkWidget*, ui_change_values) ); // AKA VikLayerFuncChangeParam in viklayer.h
                                      /* pass_along1 and pass_along2 are for set_param first and last params */

VikLayerParamData *a_uibuilder_run_dialog ( const gchar *dialog_name, GtkWindow *parent, VikLayerParam *params,
                        guint16 params_count, gchar **groups, guint8 groups_count,
			VikLayerParamData *params_defaults );

/* frees data from last (if ness) */
void a_uibuilder_free_paramdatas ( VikLayerParamData *paramdatas, VikLayerParam *params, guint16 params_count );

typedef enum {
  VL_SO_NONE = 0,
  VL_SO_ALPHABETICAL_ASCENDING,
  VL_SO_ALPHABETICAL_DESCENDING,
  VL_SO_DATE_ASCENDING,
  VL_SO_DATE_DESCENDING,
  VL_SO_LAST
} vik_layer_sort_order_t;

G_END_DECLS

#endif
