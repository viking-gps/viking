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

#ifndef _VIKING_VIKWINDOW_H
#define _VIKING_VIKWINDOW_H
/* Requires <gtk/gtk.h> or glib, and coords.h*/

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "vikviewport.h"
#include "vikstatus.h"
#include "viktrack.h"

G_BEGIN_DECLS

#define VIK_WINDOW_TYPE            (vik_window_get_type ())
#define VIK_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_WINDOW_TYPE, VikWindow))
#define VIK_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_WINDOW_TYPE, VikWindowClass))
#define IS_VIK_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_WINDOW_TYPE))
#define IS_VIK_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_WINDOW_TYPE))

typedef struct _VikWindow VikWindow;
typedef struct _VikWindowClass VikWindowClass;

struct _VikWindowClass
{
  GtkWindowClass window_class;
  void (* newwindow) (VikWindow *vw);
  void (* openwindow) (VikWindow *vw, GSList *filenames);
};

GType vik_window_get_type ();

// To call from main to start things off:
VikWindow *vik_window_new_window ();

void vik_window_new_window_finish ( VikWindow *vw, gboolean maybe_add_map, gboolean maybe_add_location );

void vik_window_draw_update ( VikWindow *vw );

GtkWidget *vik_window_get_graphs_widget ( VikWindow *vw );
gpointer vik_window_get_graphs_widgets ( VikWindow *vw );
void vik_window_set_graphs_widgets ( VikWindow *vw, gpointer gp );
gboolean vik_window_get_graphs_widgets_shown ( VikWindow *vw );
void vik_window_close_graphs ( VikWindow *vw );

GtkWidget *vik_window_get_drawmode_button ( VikWindow *vw, VikViewportDrawMode mode );
gboolean vik_window_get_pan_move ( VikWindow *vw );
void vik_window_open_file ( VikWindow *vw, const gchar *filename, gboolean change_filename, gboolean first, gboolean last, gboolean new_layer, gboolean external );
struct _VikLayer;
void vik_window_selected_layer(VikWindow *vw, struct _VikLayer *vl);
struct _VikViewport * vik_window_viewport(VikWindow *vw);
struct _VikLayersPanel * vik_window_layers_panel(VikWindow *vw);
struct _VikStatusbar * vik_window_get_statusbar(VikWindow *vw);
const gchar *vik_window_get_filename(VikWindow *vw);

gboolean vik_window_save_file_as ( VikWindow *vw, gpointer val ); // gpointer is a VikAggregateLayer

void vik_window_statusbar_update (VikWindow *vw, const gchar* message, vik_statusbar_type_t vs_type);

void vik_window_enable_layer_tool ( VikWindow *vw, gint layer_id, gint tool_id );

gpointer vik_window_get_selected_trw_layer ( VikWindow *vw ); /* return type VikTrwLayer */
void vik_window_set_selected_trw_layer ( VikWindow *vw, gpointer vtl ); /* input VikTrwLayer */
GHashTable *vik_window_get_selected_tracks ( VikWindow *vw );
void vik_window_set_selected_tracks ( VikWindow *vw, GHashTable *ght, gpointer vtl ); /* gpointer is a VikTrwLayer */
VikTrack *vik_window_get_selected_track ( VikWindow *vw );
void vik_window_set_selected_track ( VikWindow *vw, VikTrack *vt, gpointer vtl ); /* gpointer is a VikTrwLayer */
GHashTable *vik_window_get_selected_waypoints ( VikWindow *vw );
void vik_window_set_selected_waypoints ( VikWindow *vw, GHashTable *ght, gpointer vtl ); /* gpointer is a VikTrwLayer */
gpointer vik_window_get_selected_waypoint ( VikWindow *vw ); /* return type VikWaypoint */
void vik_window_set_selected_waypoint ( VikWindow *vw, gpointer *vwp, gpointer vtl ); /* input VikWaypoint, VikTrwLayer */
/* Return the VikTrwLayer of the selected track(s) or waypoint(s) are in (maybe NULL) */
gpointer vik_window_get_containing_trw_layer ( VikWindow *vw );
/* return indicates if a redraw is necessary */
gboolean vik_window_clear_selected ( VikWindow *vw );

GThread *vik_window_get_thread ( VikWindow *vw );

void vik_window_set_busy_cursor ( VikWindow *vw );
void vik_window_clear_busy_cursor ( VikWindow *vw );
void vik_window_set_busy_cursor_widget ( GtkWidget *widget, VikWindow *vw );
void vik_window_clear_busy_cursor_widget ( GtkWidget *widget, VikWindow *vw );

void vik_window_set_modified ( VikWindow *vw );

typedef struct {
  VikWindow *vw;
  VikViewport *vvp;
  gpointer vtl; // VikTrwLayer
  gboolean holding;
  gboolean moving;
  gboolean is_waypoint; // otherwise a track
  GdkGC *gc;
  GdkColor color; // For GTK3+ use as no longer in the gc
  int oldx, oldy;
  // Monitor the bounds for the tool with shift modifier
  gboolean bounds_active;
  gint start_x;
  gint start_y;
#if !GTK_CHECK_VERSION (3,0,0)
  GdkPixmap *pixmap;
#endif
  // The following are mostly for ruler tool
  gboolean has_oldcoord;
  VikCoord oldcoord;
  gboolean displayed;
} tool_ed_t;

tool_ed_t* tool_edit_create ( VikWindow *vw, VikViewport *vvp );
void tool_edit_destroy ( tool_ed_t *te );
void tool_edit_remove_image ( tool_ed_t *te );

gboolean vik_window_copy_event_location ( VikWindow *vw, int x, int y );

gpointer vik_window_get_active_tool_interface ( VikWindow *vw ); // returns VikToolInterface*
gpointer vik_window_get_active_tool_data ( VikWindow *vw );

VikWindow *a_vik_window_get_a_window ();

G_END_DECLS

#define VIK_WINDOW_FROM_WIDGET(x) VIK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(x)))

#endif
