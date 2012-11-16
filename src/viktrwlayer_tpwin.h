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

#ifndef _VIKING_TRWLAYER_TPWIN_H
#define _VIKING_TRWLAYER_TPWIN_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* response codes */
#define VIK_TRW_LAYER_TPWIN_CLOSE    6
#define VIK_TRW_LAYER_TPWIN_INSERT   5
#define VIK_TRW_LAYER_TPWIN_DELETE   4
#define VIK_TRW_LAYER_TPWIN_SPLIT    3
#define VIK_TRW_LAYER_TPWIN_BACK     1
#define VIK_TRW_LAYER_TPWIN_FORWARD  0

#define VIK_TRW_LAYER_TPWIN_DATA_CHANGED 100

#define VIK_TRW_LAYER_TPWIN_TYPE            (vik_trw_layer_tpwin_get_type ())
#define VIK_TRW_LAYER_TPWIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TRW_LAYER_TPWIN_TYPE, VikTrwLayerTpwin))
#define VIK_TRW_LAYER_TPWIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TRW_LAYER_TPWIN_TYPE, VikTrwLayerTpwinClass))
#define IS_VIK_TRW_LAYER_TPWIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TRW_LAYER_TPWIN_TYPE))
#define IS_VIK_TRW_LAYER_TPWIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TRW_LAYER_TPWIN_TYPE))

typedef struct _VikTrwLayerTpwin VikTrwLayerTpwin;
typedef struct _VikTrwLayerTpwinClass VikTrwLayerTpwinClass;

struct _VikTrwLayerTpwinClass
{
  GtkDialogClass vik_trw_layer_class;
};

GType vik_trw_layer_tpwin_get_type ();

VikTrwLayerTpwin *vik_trw_layer_tpwin_new ( GtkWindow *parent );
void vik_trw_layer_tpwin_set_empty ( VikTrwLayerTpwin *tpwin );
void vik_trw_layer_tpwin_disable_join ( VikTrwLayerTpwin *tpwin );
void vik_trw_layer_tpwin_set_tp ( VikTrwLayerTpwin *tpwin, GList *tpl, gchar *track_name );
void vik_trw_layer_tpwin_set_track_name ( VikTrwLayerTpwin *tpwin, const gchar *track_name );

G_END_DECLS

#endif
