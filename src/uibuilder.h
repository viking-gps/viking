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

/* Parameters (for I/O and Properties) */

typedef union {
  gdouble d;
  guint32 u;
  gint32 i;
  gboolean b;
  const gchar *s;
  GdkColor c;
  GList *sl;
} VikLayerParamData;

typedef struct {
  const gchar *name;
  guint8 type;
  gint16 group;
  const gchar *title;
  guint8 widget_type;
  gpointer widget_data;
  gpointer extra_widget_data;
} VikLayerParam;

enum {
VIK_LAYER_NOT_IN_PROPERTIES=-2,
VIK_LAYER_GROUP_NONE=-1
};

enum {
VIK_LAYER_WIDGET_CHECKBUTTON=0,
VIK_LAYER_WIDGET_RADIOGROUP,
VIK_LAYER_WIDGET_RADIOGROUP_STATIC,
VIK_LAYER_WIDGET_SPINBUTTON,
VIK_LAYER_WIDGET_ENTRY,
VIK_LAYER_WIDGET_PASSWORD,
VIK_LAYER_WIDGET_FILEENTRY,
VIK_LAYER_WIDGET_HSCALE,
VIK_LAYER_WIDGET_COLOR,
VIK_LAYER_WIDGET_COMBOBOX,
VIK_LAYER_WIDGET_FILELIST,
};

typedef struct {
  gdouble min;
  gdouble max;
  gdouble step;
  guint8 digits;
} VikLayerParamScale;

/* id is index */
enum {
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
};

GtkWidget *a_uibuilder_new_widget ( VikLayerParam *param, VikLayerParamData data );
VikLayerParamData a_uibuilder_widget_get_value ( GtkWidget *widget, VikLayerParam *param );
gint a_uibuilder_properties_factory ( GtkWindow *parent, VikLayerParam *params,
                        guint16 params_count, gchar **groups, guint8 groups_count,
                        gboolean (*setparam) (gpointer,guint16,VikLayerParamData,gpointer),
                        gpointer pass_along1, gpointer pass_along2,
                        VikLayerParamData (*getparam) (gpointer,guint16),
                        gpointer pass_along_getparam );
                                /* pass_along1 and pass_along2 are for set_param first and last params */


VikLayerParamData *a_uibuilder_run_dialog ( GtkWindow *parent, VikLayerParam *params,
                        guint16 params_count, gchar **groups, guint8 groups_count,
			VikLayerParamData *params_defaults );

/* frees data from last (if ness) */
void a_uibuilder_free_paramdatas ( VikLayerParamData *paramdatas, VikLayerParam *params, guint16 params_count );

#endif
