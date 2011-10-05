/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viking.h"
#include "background.h"
#include "acquire.h"
#include "datasources.h"
#include "vikgoto.h"
#include "dems.h"
#include "mapcache.h"
#include "print.h"
#include "preferences.h"
#include "icons/icons.h"
#include "vikexttools.h"
#include "garminsymbols.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ctype.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>

#define VIKING_WINDOW_WIDTH      1000
#define VIKING_WINDOW_HEIGHT     800
#define DRAW_IMAGE_DEFAULT_WIDTH 1280
#define DRAW_IMAGE_DEFAULT_HEIGHT 1024
#define DRAW_IMAGE_DEFAULT_SAVE_AS_PNG TRUE

static void window_finalize ( GObject *gob );
static GObjectClass *parent_class;

static void window_init ( VikWindow *vw );
static void window_class_init ( VikWindowClass *klass );
static void window_set_filename ( VikWindow *vw, const gchar *filename );

static void draw_update ( VikWindow *vw );

static void newwindow_cb ( GtkAction *a, VikWindow *vw );

/* Drawing & stuff */

static gboolean delete_event( VikWindow *vw );

static gboolean key_press_event( VikWindow *vw, GdkEventKey *event, gpointer data );

static void window_configure_event ( VikWindow *vw );
static void draw_sync ( VikWindow *vw );
static void draw_redraw ( VikWindow *vw );
static void draw_scroll  ( VikWindow *vw, GdkEventScroll *event );
static void draw_click  ( VikWindow *vw, GdkEventButton *event );
static void draw_release ( VikWindow *vw, GdkEventButton *event );
static void draw_mouse_motion ( VikWindow *vw, GdkEventMotion *event );
static void draw_zoom_cb ( GtkAction *a, VikWindow *vw );
static void draw_goto_cb ( GtkAction *a, VikWindow *vw );

static void draw_status ( VikWindow *vw );

/* End Drawing Functions */

static void menu_addlayer_cb ( GtkAction *a, VikWindow *vw );
static void menu_properties_cb ( GtkAction *a, VikWindow *vw );
static void menu_delete_layer_cb ( GtkAction *a, VikWindow *vw );

/* tool management */
typedef struct {
  VikToolInterface ti;
  gpointer state;
  gint layer_type;
} toolbox_tool_t;
#define TOOL_LAYER_TYPE_NONE -1

typedef struct {
  int 			active_tool;
  int			n_tools;
  toolbox_tool_t 	*tools;
  VikWindow *vw;
} toolbox_tools_t;

static void menu_tool_cb ( GtkAction *old, GtkAction *a, VikWindow *vw );
static toolbox_tools_t* toolbox_create(VikWindow *vw);
static void toolbox_add_tool(toolbox_tools_t *vt, VikToolInterface *vti, gint layer_type );
static int toolbox_get_tool(toolbox_tools_t *vt, const gchar *tool_name);
static void toolbox_activate(toolbox_tools_t *vt, const gchar *tool_name);
static const GdkCursor *toolbox_get_cursor(toolbox_tools_t *vt, const gchar *tool_name);
static void toolbox_click (toolbox_tools_t *vt, GdkEventButton *event);
static void toolbox_move (toolbox_tools_t *vt, GdkEventMotion *event);
static void toolbox_release (toolbox_tools_t *vt, GdkEventButton *event);


/* ui creation */
static void window_create_ui( VikWindow *window );
static void register_vik_icons (GtkIconFactory *icon_factory);

/* i/o */
static void load_file ( GtkAction *a, VikWindow *vw );
static gboolean save_file_as ( GtkAction *a, VikWindow *vw );
static gboolean save_file ( GtkAction *a, VikWindow *vw );
static gboolean save_file_and_exit ( GtkAction *a, VikWindow *vw );
static gboolean window_save ( VikWindow *vw );

struct _VikWindow {
  GtkWindow gtkwindow;
  VikViewport *viking_vvp;
  VikLayersPanel *viking_vlp;
  VikStatusbar *viking_vs;

  GtkToolbar *toolbar;

  GtkItemFactory *item_factory;

  /* tool management state */
  guint current_tool;
  toolbox_tools_t *vt;
  guint16 tool_layer_id;
  guint16 tool_tool_id;

  GtkActionGroup *action_group;

  gboolean pan_move;
  gint pan_x, pan_y;

  guint draw_image_width, draw_image_height;
  gboolean draw_image_save_as_png;

  gchar *filename;
  gboolean modified;

  GtkWidget *open_dia, *save_dia;
  GtkWidget *save_img_dia, *save_img_dir_dia;

  gboolean only_updating_coord_mode_ui; /* hack for a bug in GTK */
  GtkUIManager *uim;

  /* half-drawn update */
  VikLayer *trigger;
  VikCoord trigger_center;

  /* Store at this level for highlighted selection drawing since it applies to the viewport and the layers panel */
  /* Only one of these items can be selected at the same time */
  gpointer selected_vtl; /* notionally VikTrwLayer */
  gpointer selected_tracks; /* notionally GList */
  gpointer selected_track; /* notionally VikTrack */
  gpointer selected_waypoints; /* notionally GList */
  gpointer selected_waypoint; /* notionally VikWaypoint */
  /* only use for individual track or waypoint */
  gpointer selected_name; /* notionally gchar */
  ////// NEED TO THINK ABOUT VALIDITY OF THESE             //////
  ////// i.e. what happens when stuff is deleted elsewhere //////
  ////// Generally seems alright as can not access them    //////
  ////// containing_vtl now seems unecessary               //////
  /* For track(s) & waypoint(s) it is the layer they are in - this helps refering to the individual item easier */
  gpointer containing_vtl; /* notionally VikTrwLayer */
};

enum {
 TOOL_PAN = 0,
 TOOL_ZOOM,
 TOOL_RULER,
 TOOL_SELECT,
 TOOL_LAYER,
 NUMBER_OF_TOOLS
};

enum {
  VW_NEWWINDOW_SIGNAL,
  VW_OPENWINDOW_SIGNAL,
  VW_LAST_SIGNAL
};

static guint window_signals[VW_LAST_SIGNAL] = { 0 };

static gchar *tool_names[NUMBER_OF_TOOLS] = { N_("Pan"), N_("Zoom"), N_("Ruler"), N_("Select") };

GType vik_window_get_type (void)
{
  static GType vw_type = 0;

  if (!vw_type)
  {
    static const GTypeInfo vw_info = 
    {
      sizeof (VikWindowClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) window_class_init, /* class_init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikWindow),
      0,
      (GInstanceInitFunc) window_init,
    };
    vw_type = g_type_register_static ( GTK_TYPE_WINDOW, "VikWindow", &vw_info, 0 );
  }

  return vw_type;
}

VikViewport * vik_window_viewport(VikWindow *vw)
{
  return(vw->viking_vvp);
}

VikLayersPanel * vik_window_layers_panel(VikWindow *vw)
{
  return(vw->viking_vlp);
}

/**
 *  Returns the statusbar for the window
 */
VikStatusbar * vik_window_get_statusbar ( VikWindow *vw )
{
  return vw->viking_vs;
}

void vik_window_selected_layer(VikWindow *vw, VikLayer *vl)
{
  int i, j, tool_count;
  VikLayerInterface *layer_interface;

  if (!vw->action_group) return;

  for (i=0; i<VIK_LAYER_NUM_TYPES; i++) {
    GtkAction *action;
    layer_interface = vik_layer_get_interface(i);
    tool_count = layer_interface->tools_count;

    for (j = 0; j < tool_count; j++) {
      action = gtk_action_group_get_action(vw->action_group,
	    layer_interface->tools[j].name);
      g_object_set(action, "sensitive", i == vl->type, NULL);
    }
  }
}

static void window_finalize ( GObject *gob )
{
  VikWindow *vw = VIK_WINDOW(gob);
  g_return_if_fail ( vw != NULL );

  a_background_remove_status ( vw->viking_vs );

  G_OBJECT_CLASS(parent_class)->finalize(gob);
}


static void window_class_init ( VikWindowClass *klass )
{
  /* destructor */
  GObjectClass *object_class;

  window_signals[VW_NEWWINDOW_SIGNAL] = g_signal_new ( "newwindow", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikWindowClass, newwindow), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  window_signals[VW_OPENWINDOW_SIGNAL] = g_signal_new ( "openwindow", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikWindowClass, openwindow), NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = window_finalize;

  parent_class = g_type_class_peek_parent (klass);

}

static void window_init ( VikWindow *vw )
{
  GtkWidget *main_vbox;
  GtkWidget *hpaned;

  vw->action_group = NULL;

  vw->viking_vvp = vik_viewport_new();
  vw->viking_vlp = vik_layers_panel_new();
  vik_layers_panel_set_viewport ( vw->viking_vlp, vw->viking_vvp );
  vw->viking_vs = vik_statusbar_new();

  vw->vt = toolbox_create(vw);
  window_create_ui(vw);
  window_set_filename (vw, NULL);
  vw->toolbar = GTK_TOOLBAR(gtk_ui_manager_get_widget (vw->uim, "/MainToolbar"));

  toolbox_activate(vw->vt, "Pan");

  vw->filename = NULL;
  vw->item_factory = NULL;

  vw->modified = FALSE;
  vw->only_updating_coord_mode_ui = FALSE;
 
  vw->pan_move = FALSE; 
  vw->pan_x = vw->pan_y = -1;
  vw->draw_image_width = DRAW_IMAGE_DEFAULT_WIDTH;
  vw->draw_image_height = DRAW_IMAGE_DEFAULT_HEIGHT;
  vw->draw_image_save_as_png = DRAW_IMAGE_DEFAULT_SAVE_AS_PNG;

  main_vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add (GTK_CONTAINER (vw), main_vbox);

  gtk_box_pack_start (GTK_BOX(main_vbox), gtk_ui_manager_get_widget (vw->uim, "/MainMenu"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX(main_vbox), GTK_WIDGET(vw->toolbar), FALSE, TRUE, 0);
  gtk_toolbar_set_icon_size (vw->toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_toolbar_set_style (vw->toolbar, GTK_TOOLBAR_ICONS);

  vik_ext_tools_add_menu_items ( vw, vw->uim );

  g_signal_connect (G_OBJECT (vw), "delete_event", G_CALLBACK (delete_event), NULL);

  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "expose_event", G_CALLBACK(draw_sync), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "configure_event", G_CALLBACK(window_configure_event), vw);
  gtk_widget_add_events ( GTK_WIDGET(vw->viking_vvp), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK );
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "scroll_event", G_CALLBACK(draw_scroll), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_press_event", G_CALLBACK(draw_click), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_release_event", G_CALLBACK(draw_release), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "motion_notify_event", G_CALLBACK(draw_mouse_motion), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "update", G_CALLBACK(draw_update), vw);

  g_signal_connect_swapped (G_OBJECT (vw->viking_vvp), "key_press_event", G_CALLBACK (key_press_event), vw);

  gtk_window_set_default_size ( GTK_WINDOW(vw), VIKING_WINDOW_WIDTH, VIKING_WINDOW_HEIGHT);

  hpaned = gtk_hpaned_new ();
  gtk_paned_pack1 ( GTK_PANED(hpaned), GTK_WIDGET (vw->viking_vlp), FALSE, FALSE );
  gtk_paned_pack2 ( GTK_PANED(hpaned), GTK_WIDGET (vw->viking_vvp), TRUE, TRUE );

  /* This packs the button into the window (a gtk container). */
  gtk_box_pack_start (GTK_BOX(main_vbox), hpaned, TRUE, TRUE, 0);

  gtk_box_pack_end (GTK_BOX(main_vbox), GTK_WIDGET(vw->viking_vs), FALSE, TRUE, 0);

  a_background_add_status(vw->viking_vs);

  vw->open_dia = NULL;
  vw->save_dia = NULL;
  vw->save_img_dia = NULL;
  vw->save_img_dir_dia = NULL;
}

VikWindow *vik_window_new ()
{
  return VIK_WINDOW ( g_object_new ( VIK_WINDOW_TYPE, NULL ) );
}

static gboolean key_press_event( VikWindow *vw, GdkEventKey *event, gpointer data )
{
  VikLayer *vl = vik_layers_panel_get_selected ( vw->viking_vlp );
  if (vl && vw->vt->active_tool != -1 && vw->vt->tools[vw->vt->active_tool].ti.key_press ) {
    gint ltype = vw->vt->tools[vw->vt->active_tool].layer_type;
    if ( vl && ltype == vl->type )
      return vw->vt->tools[vw->vt->active_tool].ti.key_press(vl, event, vw->vt->tools[vw->vt->active_tool].state);
  }

  // No layer - but enable window tool keypress processing - these should be able to handle a NULL layer
  if ( vw->vt->tools[vw->vt->active_tool].ti.key_press ) {
    return vw->vt->tools[vw->vt->active_tool].ti.key_press ( vl, event, vw->vt->tools[vw->vt->active_tool].state );
  }

  /* Restore Main Menu via Escape key if the user has hidden it */
  /* This key is more likely to be used as they may not remember the function key */
  if ( event->keyval == GDK_Escape ) {
    GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewMainMenu" );
    if ( check_box ) {
      gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
      if ( !state ) {
	gtk_widget_show ( gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu" ) );
	gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), TRUE );
	return TRUE; /* handled keypress */
      }
    }
  }

  return FALSE; /* don't handle the keypress */
}

static gboolean delete_event( VikWindow *vw )
{
#ifdef VIKING_PROMPT_IF_MODIFIED
  if ( vw->modified )
#else
  if (0)
#endif
  {
    GtkDialog *dia;
    dia = GTK_DIALOG ( gtk_message_dialog_new ( GTK_WINDOW(vw), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
      _("Do you want to save the changes you made to the document \"%s\"?\n"
	"\n"
	"Your changes will be lost if you don't save them."),
      vw->filename ? a_file_basename ( vw->filename ) : _("Untitled") ) );
    gtk_dialog_add_buttons ( dia, _("Don't Save"), GTK_RESPONSE_NO, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL );
    switch ( gtk_dialog_run ( dia ) )
    {
      case GTK_RESPONSE_NO: gtk_widget_destroy ( GTK_WIDGET(dia) ); return FALSE;
      case GTK_RESPONSE_CANCEL: gtk_widget_destroy ( GTK_WIDGET(dia) ); return TRUE;
      default: gtk_widget_destroy ( GTK_WIDGET(dia) ); return ! save_file(NULL, vw);
    }
  }
  return FALSE;
}

/* Drawing stuff */
static void newwindow_cb ( GtkAction *a, VikWindow *vw )
{
  g_signal_emit ( G_OBJECT(vw), window_signals[VW_NEWWINDOW_SIGNAL], 0 );
}

static void draw_update ( VikWindow *vw )
{
  draw_redraw (vw);
  draw_sync (vw);
}

static void draw_sync ( VikWindow *vw )
{
  vik_viewport_sync(vw->viking_vvp);
  draw_status ( vw );
  /* other things may be necc here later. */
}

static void draw_status ( VikWindow *vw )
{
  static gchar zoom_level[22];
  gdouble xmpp = vik_viewport_get_xmpp (vw->viking_vvp);
  gdouble ympp = vik_viewport_get_ympp(vw->viking_vvp);
  gchar *unit = vik_viewport_get_coord_mode(vw->viking_vvp) == VIK_COORD_UTM ? _("mpp") : _("pixelfact");
  if (xmpp != ympp)
    g_snprintf ( zoom_level, 22, "%.3f/%.3f %s", xmpp, ympp, unit );
  else
    if ( (int)xmpp - xmpp < 0.0 )
      g_snprintf ( zoom_level, 22, "%.3f %s", xmpp, unit );
    else
      /* xmpp should be a whole number so don't show useless .000 bit */
      g_snprintf ( zoom_level, 22, "%d %s", (int)xmpp, unit );
  if ( vw->current_tool == TOOL_LAYER )
    vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_TOOL, vik_layer_get_interface(vw->tool_layer_id)->tools[vw->tool_tool_id].name );
  else
    vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_TOOL, _(tool_names[vw->current_tool]) );

  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_ZOOM, zoom_level );
}

void vik_window_set_redraw_trigger(VikLayer *vl)
{
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vl));
  if (NULL != vw)
    vw->trigger = vl;
}

static void window_configure_event ( VikWindow *vw )
{
  static int first = 1;
  draw_redraw ( vw );
  if (first) {
    // This is a hack to set the cursor corresponding to the first tool
    // FIXME find the correct way to initialize both tool and its cursor
    const GdkCursor *cursor = NULL;
    first = 0;
    cursor = toolbox_get_cursor(vw->vt, "Pan");
    /* We set cursor, even if it is NULL: it resets to default */
    gdk_window_set_cursor ( GTK_WIDGET(vw->viking_vvp)->window, (GdkCursor *)cursor );
  }
}

static void draw_redraw ( VikWindow *vw )
{
  VikCoord old_center = vw->trigger_center;
  vw->trigger_center = *(vik_viewport_get_center(vw->viking_vvp));
  VikLayer *new_trigger = vw->trigger;
  vw->trigger = NULL;
  VikLayer *old_trigger = VIK_LAYER(vik_viewport_get_trigger(vw->viking_vvp));

  if ( ! new_trigger )
    ; /* do nothing -- have to redraw everything. */
  else if ( (old_trigger != new_trigger) || !vik_coord_equals(&old_center, &vw->trigger_center) || (new_trigger->type == VIK_LAYER_AGGREGATE) )
    vik_viewport_set_trigger ( vw->viking_vvp, new_trigger ); /* todo: set to half_drawn mode if new trigger is above old */
  else
    vik_viewport_set_half_drawn ( vw->viking_vvp, TRUE );

  /* actually draw */
  vik_viewport_clear ( vw->viking_vvp);
  vik_layers_panel_draw_all ( vw->viking_vlp );
  vik_viewport_draw_scale ( vw->viking_vvp );
  vik_viewport_draw_copyright ( vw->viking_vvp );
  vik_viewport_draw_centermark ( vw->viking_vvp );
  vik_viewport_draw_logo ( vw->viking_vvp );

  vik_viewport_set_half_drawn ( vw->viking_vvp, FALSE ); /* just in case. */
}

gboolean draw_buf_done = TRUE;

static gboolean draw_buf(gpointer data)
{
  gpointer *pass_along = data;
  gdk_threads_enter();
  gdk_draw_drawable (pass_along[0], pass_along[1],
		     pass_along[2], 0, 0, 0, 0, -1, -1);
  draw_buf_done = TRUE;
  gdk_threads_leave();
  return FALSE;
}


/* Mouse event handlers ************************************************************************/

static void vik_window_pan_click (VikWindow *vw, GdkEventButton *event)
{
  /* set panning origin */
  vw->pan_move = FALSE;
  vw->pan_x = (gint) event->x;
  vw->pan_y = (gint) event->y;
}

static void draw_click (VikWindow *vw, GdkEventButton *event)
{
  gtk_widget_grab_focus ( GTK_WIDGET(vw->viking_vvp) );

  /* middle button pressed.  we reserve all middle button and scroll events
   * for panning and zooming; tools only get left/right/movement 
   */
  if ( event->button == 2) {
    vik_window_pan_click ( vw, event );
  } 
  else {
    toolbox_click(vw->vt, event);
  }
}

static void vik_window_pan_move (VikWindow *vw, GdkEventMotion *event)
{
  if ( vw->pan_x != -1 ) {
    vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2 - event->x + vw->pan_x,
                                     vik_viewport_get_height(vw->viking_vvp)/2 - event->y + vw->pan_y );
    vw->pan_move = TRUE;
    vw->pan_x = event->x;
    vw->pan_y = event->y;
    draw_update ( vw );
  }
}

static void draw_mouse_motion (VikWindow *vw, GdkEventMotion *event)
{
  static VikCoord coord;
  static struct UTM utm;
  static struct LatLon ll;
  #define BUFFER_SIZE 50
  static char pointer_buf[BUFFER_SIZE];
  gchar *lat = NULL, *lon = NULL;
  gint16 alt;
  gdouble zoom;
  VikDemInterpol interpol_method;

  /* This is a hack, but work far the best, at least for single pointer systems.
   * See http://bugzilla.gnome.org/show_bug.cgi?id=587714 for more. */
  gint x, y;
  gdk_window_get_pointer (event->window, &x, &y, NULL);
  event->x = x;
  event->y = y;

  toolbox_move(vw->vt, event);

  vik_viewport_screen_to_coord ( vw->viking_vvp, event->x, event->y, &coord );
  vik_coord_to_utm ( &coord, &utm );
  a_coords_utm_to_latlon ( &utm, &ll );
  a_coords_latlon_to_string ( &ll, &lat, &lon );
  /* Change interpolate method according to scale */
  zoom = vik_viewport_get_zoom(vw->viking_vvp);
  if (zoom > 2.0)
    interpol_method = VIK_DEM_INTERPOL_NONE;
  else if (zoom >= 1.0)
    interpol_method = VIK_DEM_INTERPOL_SIMPLE;
  else
    interpol_method = VIK_DEM_INTERPOL_BEST;
  if ((alt = a_dems_get_elev_by_coord(&coord, interpol_method)) != VIK_DEM_INVALID_ELEVATION) {
    if ( a_vik_get_units_height () == VIK_UNITS_HEIGHT_METRES )
      g_snprintf ( pointer_buf, BUFFER_SIZE, _("%s %s %dm"), lat, lon, alt );
    else
      g_snprintf ( pointer_buf, BUFFER_SIZE, _("%s %s %dft"), lat, lon, (int)VIK_METERS_TO_FEET(alt) );
  }
  else
    g_snprintf ( pointer_buf, BUFFER_SIZE, _("%s %s"), lat, lon );
  g_free (lat);
  lat = NULL;
  g_free (lon);
  lon = NULL;
  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_POSITION, pointer_buf );

  vik_window_pan_move ( vw, event );

  /* This is recommended by the GTK+ documentation, but does not work properly.
   * Use deprecated way until GTK+ gets a solution for correct motion hint handling:
   * http://bugzilla.gnome.org/show_bug.cgi?id=587714
  */
  /* gdk_event_request_motions ( event ); */
}

static void vik_window_pan_release ( VikWindow *vw, GdkEventButton *event )
{
  if ( vw->pan_move == FALSE )
    vik_viewport_set_center_screen ( vw->viking_vvp, vw->pan_x, vw->pan_y );
  else
     vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2 - event->x + vw->pan_x,
                                      vik_viewport_get_height(vw->viking_vvp)/2 - event->y + vw->pan_y );
  vw->pan_move = FALSE;
  vw->pan_x = vw->pan_y = -1;
  draw_update ( vw );
}

static void draw_release ( VikWindow *vw, GdkEventButton *event )
{
  gtk_widget_grab_focus ( GTK_WIDGET(vw->viking_vvp) );

  if ( event->button == 2 ) {  /* move / pan */
    vik_window_pan_release(vw, event);
  }
  else {
    toolbox_release(vw->vt, event);
  }
}

static void draw_scroll (VikWindow *vw, GdkEventScroll *event)
{
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);
  if ( modifiers == GDK_CONTROL_MASK ) {
    /* control == pan up & down */
    if ( event->direction == GDK_SCROLL_UP )
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, vik_viewport_get_height(vw->viking_vvp)/3 );
    else
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, vik_viewport_get_height(vw->viking_vvp)*2/3 );
  } else if ( modifiers == GDK_SHIFT_MASK ) {
    /* shift == pan left & right */
    if ( event->direction == GDK_SCROLL_UP )
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/3, vik_viewport_get_height(vw->viking_vvp)/2 );
    else
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)*2/3, vik_viewport_get_height(vw->viking_vvp)/2 );
  } else if ( modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) ) {
    /* control+shift == make sure mouse is still over the same point on the map when we zoom */
    VikCoord coord;
    gint x, y;
    gint center_x = vik_viewport_get_width ( vw->viking_vvp ) / 2;
    gint center_y = vik_viewport_get_height ( vw->viking_vvp ) / 2;
    vik_viewport_screen_to_coord ( vw->viking_vvp, event->x, event->y, &coord );
    if ( event->direction == GDK_SCROLL_UP )
      vik_viewport_zoom_in (vw->viking_vvp);
    else
      vik_viewport_zoom_out(vw->viking_vvp);
    vik_viewport_coord_to_screen ( vw->viking_vvp, &coord, &x, &y );
    vik_viewport_set_center_screen ( vw->viking_vvp, center_x + (x - event->x),
                                     center_y + (y - event->y) );
  } else {
    if ( event->direction == GDK_SCROLL_UP )
      vik_viewport_zoom_in (vw->viking_vvp);
    else
      vik_viewport_zoom_out (vw->viking_vvp);
  }

  draw_update(vw);
}



/********************************************************************************
 ** Ruler tool code
 ********************************************************************************/
static void draw_ruler(VikViewport *vvp, GdkDrawable *d, GdkGC *gc, gint x1, gint y1, gint x2, gint y2, gdouble distance)
{
  PangoFontDescription *pfd;
  PangoLayout *pl;
  gchar str[128];
  GdkGC *labgc = vik_viewport_new_gc ( vvp, "#cccccc", 1);
  GdkGC *thickgc = gdk_gc_new(d);
  
  gdouble len = sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
  gdouble dx = (x2-x1)/len*10; 
  gdouble dy = (y2-y1)/len*10;
  gdouble c = cos(15.0 * M_PI/180.0);
  gdouble s = sin(15.0 * M_PI/180.0);
  gdouble angle;
  gdouble baseangle = 0;
  gint i;

  /* draw line with arrow ends */
  {
    gint tmp_x1=x1, tmp_y1=y1, tmp_x2=x2, tmp_y2=y2;
    a_viewport_clip_line(&tmp_x1, &tmp_y1, &tmp_x2, &tmp_y2);
    gdk_draw_line(d, gc, tmp_x1, tmp_y1, tmp_x2, tmp_y2);
  }

  a_viewport_clip_line(&x1, &y1, &x2, &y2);
  gdk_draw_line(d, gc, x1, y1, x2, y2);

  gdk_draw_line(d, gc, x1 - dy, y1 + dx, x1 + dy, y1 - dx);
  gdk_draw_line(d, gc, x2 - dy, y2 + dx, x2 + dy, y2 - dx);
  gdk_draw_line(d, gc, x2, y2, x2 - (dx * c + dy * s), y2 - (dy * c - dx * s));
  gdk_draw_line(d, gc, x2, y2, x2 - (dx * c - dy * s), y2 - (dy * c + dx * s));
  gdk_draw_line(d, gc, x1, y1, x1 + (dx * c + dy * s), y1 + (dy * c - dx * s));
  gdk_draw_line(d, gc, x1, y1, x1 + (dx * c - dy * s), y1 + (dy * c + dx * s));

  /* draw compass */
#define CR 80
#define CW 4
  angle = atan2(dy, dx) + M_PI_2;

  if ( vik_viewport_get_drawmode ( vvp ) == VIK_VIEWPORT_DRAWMODE_UTM) {
    VikCoord test;
    struct LatLon ll;
    struct UTM u;
    gint tx, ty;

    vik_viewport_screen_to_coord ( vvp, x1, y1, &test );
    vik_coord_to_latlon ( &test, &ll );
    ll.lat += vik_viewport_get_ympp ( vvp ) * vik_viewport_get_height ( vvp ) / 11000.0; // about 11km per degree latitude
    a_coords_latlon_to_utm ( &ll, &u );
    vik_coord_load_from_utm ( &test, VIK_VIEWPORT_DRAWMODE_UTM, &u );
    vik_viewport_coord_to_screen ( vvp, &test, &tx, &ty );

    baseangle = M_PI - atan2(tx-x1, ty-y1);
    angle -= baseangle;
  }

  if (angle<0) 
    angle+=2*M_PI;
  if (angle>2*M_PI)
    angle-=2*M_PI;

  {
    GdkColor color;
    gdk_gc_copy(thickgc, gc);
    gdk_gc_set_line_attributes(thickgc, CW, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
    gdk_color_parse("#2255cc", &color);
    gdk_gc_set_rgb_fg_color(thickgc, &color);
  }
  gdk_draw_arc (d, thickgc, FALSE, x1-CR+CW/2, y1-CR+CW/2, 2*CR-CW, 2*CR-CW, (90 - baseangle*180/M_PI)*64, -angle*180/M_PI*64);


  gdk_gc_copy(thickgc, gc);
  gdk_gc_set_line_attributes(thickgc, 2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
  for (i=0; i<180; i++) {
    c = cos(i*M_PI/90.0 + baseangle);
    s = sin(i*M_PI/90.0 + baseangle);

    if (i%5) {
      gdk_draw_line (d, gc, x1 + CR*c, y1 + CR*s, x1 + (CR+CW)*c, y1 + (CR+CW)*s);
    } else {
      gdouble ticksize = 2*CW;
      gdk_draw_line (d, thickgc, x1 + (CR-CW)*c, y1 + (CR-CW)*s, x1 + (CR+ticksize)*c, y1 + (CR+ticksize)*s);
    }
  }

  gdk_draw_arc (d, gc, FALSE, x1-CR, y1-CR, 2*CR, 2*CR, 0, 64*360);
  gdk_draw_arc (d, gc, FALSE, x1-CR-CW, y1-CR-CW, 2*(CR+CW), 2*(CR+CW), 0, 64*360);
  gdk_draw_arc (d, gc, FALSE, x1-CR+CW, y1-CR+CW, 2*(CR-CW), 2*(CR-CW), 0, 64*360);
  c = (CR+CW*2)*cos(baseangle);
  s = (CR+CW*2)*sin(baseangle);
  gdk_draw_line (d, gc, x1-c, y1-s, x1+c, y1+s);
  gdk_draw_line (d, gc, x1+s, y1-c, x1-s, y1+c);

  /* draw labels */
#define LABEL(x, y, w, h) { \
    gdk_draw_rectangle(d, labgc, TRUE, (x)-2, (y)-1, (w)+4, (h)+1); \
    gdk_draw_rectangle(d, gc, FALSE, (x)-2, (y)-1, (w)+4, (h)+1); \
    gdk_draw_layout(d, gc, (x), (y), pl); } 
  {
    gint wd, hd, xd, yd;
    gint wb, hb, xb, yb;

    pl = gtk_widget_create_pango_layout (GTK_WIDGET(vvp), NULL);

    pfd = pango_font_description_from_string ("Sans 8"); // FIXME: settable option? global variable?
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);

    pango_layout_set_text(pl, "N", -1);
    gdk_draw_layout(d, gc, x1-5, y1-CR-3*CW-8, pl);

    /* draw label with distance */
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      if (distance >= 1000 && distance < 100000) {
	g_sprintf(str, "%3.2f km", distance/1000.0);
      } else if (distance < 1000) {
	g_sprintf(str, "%d m", (int)distance);
      } else {
	g_sprintf(str, "%d km", (int)distance/1000);
      }
      break;
    case VIK_UNITS_DISTANCE_MILES:
      if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
	g_sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
      } else if (distance < VIK_MILES_TO_METERS(1)) {
	g_sprintf(str, "%d yards", (int)(distance*1.0936133));
      } else {
	g_sprintf(str, "%d miles", (int)VIK_METERS_TO_MILES(distance));
      }
      break;
    default:
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }

    pango_layout_set_text(pl, str, -1);

    pango_layout_get_pixel_size ( pl, &wd, &hd );
    if (dy>0) {
      xd = (x1+x2)/2 + dy;
      yd = (y1+y2)/2 - hd/2 - dx;
    } else {
      xd = (x1+x2)/2 - dy;
      yd = (y1+y2)/2 - hd/2 + dx;
    }

    if ( xd < -5 || yd < -5 || xd > vik_viewport_get_width(vvp)+5 || yd > vik_viewport_get_height(vvp)+5 ) {
      xd = x2 + 10;
      yd = y2 - 5;
    }

    LABEL(xd, yd, wd, hd);

    /* draw label with bearing */
    g_sprintf(str, "%3.1f°", angle*180.0/M_PI);
    pango_layout_set_text(pl, str, -1);
    pango_layout_get_pixel_size ( pl, &wb, &hb );
    xb = x1 + CR*cos(angle-M_PI_2);
    yb = y1 + CR*sin(angle-M_PI_2);

    if ( xb < -5 || yb < -5 || xb > vik_viewport_get_width(vvp)+5 || yb > vik_viewport_get_height(vvp)+5 ) {
      xb = x2 + 10;
      yb = y2 + 10;
    }

    {
      GdkRectangle r1 = {xd-2, yd-1, wd+4, hd+1}, r2 = {xb-2, yb-1, wb+4, hb+1};
      if (gdk_rectangle_intersect(&r1, &r2, &r2)) {
	xb = xd + wd + 5;
      }
    }
    LABEL(xb, yb, wb, hb);
  }
#undef LABEL

  g_object_unref ( G_OBJECT ( pl ) );
  g_object_unref ( G_OBJECT ( labgc ) );
  g_object_unref ( G_OBJECT ( thickgc ) );
}

typedef struct {
  VikWindow *vw;
  VikViewport *vvp;
  gboolean has_oldcoord;
  VikCoord oldcoord;
} ruler_tool_state_t;

static gpointer ruler_create (VikWindow *vw, VikViewport *vvp) 
{
  ruler_tool_state_t *s = g_new(ruler_tool_state_t, 1);
  s->vw = vw;
  s->vvp = vvp;
  s->has_oldcoord = FALSE;
  return s;
}

static void ruler_destroy (ruler_tool_state_t *s)
{
  g_free(s);
}

static VikLayerToolFuncStatus ruler_click (VikLayer *vl, GdkEventButton *event, ruler_tool_state_t *s)
{
  struct LatLon ll;
  VikCoord coord;
  gchar *temp;
  if ( event->button == 1 ) {
    gchar *lat=NULL, *lon=NULL;
    vik_viewport_screen_to_coord ( s->vvp, (gint) event->x, (gint) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );
    a_coords_latlon_to_string ( &ll, &lat, &lon );
    if ( s->has_oldcoord ) {
      vik_units_distance_t dist_units = a_vik_get_units_distance ();
      switch (dist_units) {
      case VIK_UNITS_DISTANCE_KILOMETRES:
	temp = g_strdup_printf ( "%s %s DIFF %f meters", lat, lon, vik_coord_diff( &coord, &(s->oldcoord) ) );
	break;
      case VIK_UNITS_DISTANCE_MILES:
	temp = g_strdup_printf ( "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES(vik_coord_diff( &coord, &(s->oldcoord) )) );
	break;
      default:
	temp = g_strdup_printf ("Just to keep the compiler happy");
	g_critical("Houston, we've had a problem. distance=%d", dist_units);
      }

      s->has_oldcoord = FALSE;
    }
    else {
      temp = g_strdup_printf ( "%s %s", lat, lon );
      s->has_oldcoord = TRUE;
    }

    vik_statusbar_set_message ( s->vw->viking_vs, VIK_STATUSBAR_INFO, temp );
    g_free ( temp );

    s->oldcoord = coord;
  }
  else {
    vik_viewport_set_center_screen ( s->vvp, (gint) event->x, (gint) event->y );
    draw_update ( s->vw );
  }
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus ruler_move (VikLayer *vl, GdkEventMotion *event, ruler_tool_state_t *s)
{
  VikViewport *vvp = s->vvp;
  VikWindow *vw = s->vw;

  struct LatLon ll;
  VikCoord coord;
  gchar *temp;

  if ( s->has_oldcoord ) {
    int oldx, oldy, w1, h1, w2, h2;
    static GdkPixmap *buf = NULL;
    gchar *lat=NULL, *lon=NULL;
    w1 = vik_viewport_get_width(vvp); 
    h1 = vik_viewport_get_height(vvp);
    if (!buf) {
      buf = gdk_pixmap_new ( GTK_WIDGET(vvp)->window, w1, h1, -1 );
    }
    gdk_drawable_get_size(buf, &w2, &h2);
    if (w1 != w2 || h1 != h2) {
      g_object_unref ( G_OBJECT ( buf ) );
      buf = gdk_pixmap_new ( GTK_WIDGET(vvp)->window, w1, h1, -1 );
    }

    vik_viewport_screen_to_coord ( vvp, (gint) event->x, (gint) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );
    vik_viewport_coord_to_screen ( vvp, &s->oldcoord, &oldx, &oldy );

    gdk_draw_drawable (buf, GTK_WIDGET(vvp)->style->black_gc, 
		       vik_viewport_get_pixmap(vvp), 0, 0, 0, 0, -1, -1);
    draw_ruler(vvp, buf, GTK_WIDGET(vvp)->style->black_gc, oldx, oldy, event->x, event->y, vik_coord_diff( &coord, &(s->oldcoord)) );
    if (draw_buf_done) {
      static gpointer pass_along[3];
      pass_along[0] = GTK_WIDGET(vvp)->window;
      pass_along[1] = GTK_WIDGET(vvp)->style->black_gc;
      pass_along[2] = buf;
      g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, draw_buf, pass_along, NULL);
      draw_buf_done = FALSE;
    }
    a_coords_latlon_to_string(&ll, &lat, &lon);
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      temp = g_strdup_printf ( "%s %s DIFF %f meters", lat, lon, vik_coord_diff( &coord, &(s->oldcoord) ) );
      break;
    case VIK_UNITS_DISTANCE_MILES:
      temp = g_strdup_printf ( "%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES (vik_coord_diff( &coord, &(s->oldcoord) )) );
      break;
    default:
      temp = g_strdup_printf ("Just to keep the compiler happy");
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }
    vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, temp );
    g_free ( temp );
  }
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus ruler_release (VikLayer *vl, GdkEventButton *event, ruler_tool_state_t *s)
{
  return VIK_LAYER_TOOL_ACK;
}

static void ruler_deactivate (VikLayer *vl, ruler_tool_state_t *s)
{
  draw_update ( s->vw );
}

static gboolean ruler_key_press (VikLayer *vl, GdkEventKey *event, ruler_tool_state_t *s)
{
  if (event->keyval == GDK_Escape) {
    s->has_oldcoord = FALSE;
    ruler_deactivate ( vl, s );
    return TRUE;
  }
  return FALSE;
}

static VikToolInterface ruler_tool = 
  { "Ruler", 
    (VikToolConstructorFunc) ruler_create,
    (VikToolDestructorFunc) ruler_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) ruler_deactivate, 
    (VikToolMouseFunc) ruler_click, 
    (VikToolMouseMoveFunc) ruler_move, 
    (VikToolMouseFunc) ruler_release,
    (VikToolKeyFunc) ruler_key_press,
    GDK_CURSOR_IS_PIXMAP,
    &cursor_ruler_pixbuf };
/*** end ruler code ********************************************************/



/********************************************************************************
 ** Zoom tool code
 ********************************************************************************/
static gpointer zoomtool_create (VikWindow *vw, VikViewport *vvp)
{
  return vw;
}

static VikLayerToolFuncStatus zoomtool_click (VikLayer *vl, GdkEventButton *event, VikWindow *vw)
{
  vw->modified = TRUE;
  vik_viewport_set_center_screen ( vw->viking_vvp, (gint) event->x, (gint) event->y );
  if ( event->button == 1 )
    vik_viewport_zoom_in (vw->viking_vvp);
  else if ( event->button == 3 )
    vik_viewport_zoom_out (vw->viking_vvp);
  draw_update ( vw );
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_move (VikLayer *vl, GdkEventMotion *event, VikViewport *vvp)
{
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_release (VikLayer *vl, GdkEventButton *event, VikViewport *vvp)
{
  return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface zoom_tool = 
  { "Zoom", 
    (VikToolConstructorFunc) zoomtool_create,
    (VikToolDestructorFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) zoomtool_click, 
    (VikToolMouseMoveFunc) zoomtool_move,
    (VikToolMouseFunc) zoomtool_release,
    NULL,
    GDK_CURSOR_IS_PIXMAP,
    &cursor_zoom_pixbuf };
/*** end zoom code ********************************************************/

/********************************************************************************
 ** Pan tool code
 ********************************************************************************/
static gpointer pantool_create (VikWindow *vw, VikViewport *vvp)
{
  return vw;
}

static VikLayerToolFuncStatus pantool_click (VikLayer *vl, GdkEventButton *event, VikWindow *vw)
{
  vw->modified = TRUE;
  if ( event->button == 1 )
    vik_window_pan_click ( vw, event );
  draw_update ( vw );
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus pantool_move (VikLayer *vl, GdkEventMotion *event, VikWindow *vw)
{
  vik_window_pan_move ( vw, event );
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus pantool_release (VikLayer *vl, GdkEventButton *event, VikWindow *vw)
{
  if ( event->button == 1 )
    vik_window_pan_release ( vw, event );
  return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface pan_tool = 
  { "Pan", 
    (VikToolConstructorFunc) pantool_create,
    (VikToolDestructorFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) pantool_click, 
    (VikToolMouseMoveFunc) pantool_move,
    (VikToolMouseFunc) pantool_release,
    NULL,
    GDK_FLEUR };
/*** end pan code ********************************************************/

/********************************************************************************
 ** Select tool code
 ********************************************************************************/
static gpointer selecttool_create (VikWindow *vw, VikViewport *vvp)
{
  tool_ed_t *t = g_new(tool_ed_t, 1);
  t->vw = vw;
  t->vvp = vvp;
  t->vtl = NULL;
  t->is_waypoint = FALSE;
  return t;
}

static void selecttool_destroy (tool_ed_t *t)
{
  g_free(t);
}

typedef struct {
  gboolean cont;
  VikViewport *vvp;
  GdkEventButton *event;
  tool_ed_t *tool_edit;
} clicker;

static void click_layer_selected (VikLayer *vl, clicker *ck)
{
  /* Do nothing when function call returns true; */
  /* i.e. stop on first found item */
  if ( ck->cont )
    if ( vl->visible )
      if ( vik_layer_get_interface(vl->type)->select_click )
	ck->cont = !vik_layer_get_interface(vl->type)->select_click ( vl, ck->event, ck->vvp, ck->tool_edit );
}

static VikLayerToolFuncStatus selecttool_click (VikLayer *vl, GdkEventButton *event, tool_ed_t *t)
{
  /* Only allow selection on primary button */
  if ( event->button == 1 ) {
    /* Enable click to apply callback to potentially all track/waypoint layers */
    /* Useful as we can find things that aren't necessarily in the currently selected layer */
    GList* gl = vik_layers_panel_get_all_layers_of_type ( t->vw->viking_vlp, VIK_LAYER_TRW, FALSE ); // Don't get invisible layers
    clicker ck;
    ck.cont = TRUE;
    ck.vvp = t->vw->viking_vvp;
    ck.event = event;
    ck.tool_edit = t;
    g_list_foreach ( gl, (GFunc) click_layer_selected, &ck );
    g_list_free ( gl );

    // If nothing found then deselect & redraw screen if necessary to remove the highlight
    if ( ck.cont ) {
      GtkTreeIter iter;
      VikTreeview *vtv = vik_layers_panel_get_treeview ( t->vw->viking_vlp );

      if ( vik_treeview_get_selected_iter ( vtv, &iter ) ) {
	// Only clear if selected thing is a TrackWaypoint layer or a sublayer
	gint type = vik_treeview_item_get_type ( vtv, &iter );
	if ( type == VIK_TREEVIEW_TYPE_SUBLAYER ||
	     VIK_LAYER(vik_treeview_item_get_pointer ( vtv, &iter ))->type == VIK_LAYER_TRW ) {
   
	  vik_treeview_item_unselect ( vtv, &iter );
	  if ( vik_window_clear_highlight ( t->vw ) )
	    draw_update ( t->vw );
	}
      }
    }
  }
  else if ( ( event->button == 3 ) && ( vl && ( vl->type == VIK_LAYER_TRW ) ) ) {
    if ( vl->visible )
      /* Act on currently selected item to show menu */
      if ( ( t->vw->selected_track || t->vw->selected_waypoint ) && t->vw->selected_name )
	if ( vik_layer_get_interface(vl->type)->show_viewport_menu )
	  vik_layer_get_interface(vl->type)->show_viewport_menu ( vl, event, t->vw->viking_vvp );
  }

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_move (VikLayer *vl, GdkEventButton *event, tool_ed_t *t)
{
  /* Only allow selection on primary button */
  if ( event->button == 1 ) {
    // Don't care about vl here
    if ( t->vtl )
      if ( vik_layer_get_interface(VIK_LAYER_TRW)->select_move )
	vik_layer_get_interface(VIK_LAYER_TRW)->select_move ( vl, event, t->vvp, t );
  }
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_release (VikLayer *vl, GdkEventButton *event, tool_ed_t *t)
{
  /* Only allow selection on primary button */
  if ( event->button == 1 ) {
    // Don't care about vl here
    if ( t->vtl )
      if ( vik_layer_get_interface(VIK_LAYER_TRW)->select_release )
	vik_layer_get_interface(VIK_LAYER_TRW)->select_release ( (VikLayer*)t->vtl, event, t->vvp, t );
  }
  return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface select_tool =
  { "Select",
    (VikToolConstructorFunc) selecttool_create,
    (VikToolDestructorFunc) selecttool_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) selecttool_click,
    (VikToolMouseMoveFunc) selecttool_move,
    (VikToolMouseFunc) selecttool_release,
    (VikToolKeyFunc) NULL,
    GDK_LEFT_PTR,
    NULL,
    NULL };
/*** end select tool code ********************************************************/

static void draw_pan_cb ( GtkAction *a, VikWindow *vw )
{
  if (!strcmp(gtk_action_get_name(a), "PanNorth")) {
    vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, 0 );
  } else if (!strcmp(gtk_action_get_name(a), "PanEast")) {
    vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp), vik_viewport_get_height(vw->viking_vvp)/2 );
  } else if (!strcmp(gtk_action_get_name(a), "PanSouth")) {
    vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, vik_viewport_get_height(vw->viking_vvp) );
  } else if (!strcmp(gtk_action_get_name(a), "PanWest")) {
    vik_viewport_set_center_screen ( vw->viking_vvp, 0, vik_viewport_get_height(vw->viking_vvp)/2 );
  }
  draw_update ( vw );
}

static void full_screen_cb ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/FullScreen" );
  g_assert(check_box);
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box));
  if ( state )
    gtk_window_fullscreen ( GTK_WINDOW(vw) );
  else
    gtk_window_unfullscreen ( GTK_WINDOW(vw) );
}

static void draw_zoom_cb ( GtkAction *a, VikWindow *vw )
{
  guint what = 128;

  if (!strcmp(gtk_action_get_name(a), "ZoomIn")) {
    what = -3;
  } 
  else if (!strcmp(gtk_action_get_name(a), "ZoomOut")) {
    what = -4;
  }
  else if (!strcmp(gtk_action_get_name(a), "Zoom0.25")) {
    what = -2;
  }
  else if (!strcmp(gtk_action_get_name(a), "Zoom0.5")) {
    what = -1;
  }
  else {
    gchar *s = (gchar *)gtk_action_get_name(a);
    what = atoi(s+4);
  }

  switch (what)
  {
    case -3: vik_viewport_zoom_in ( vw->viking_vvp ); break;
    case -4: vik_viewport_zoom_out ( vw->viking_vvp ); break;
    case -1: vik_viewport_set_zoom ( vw->viking_vvp, 0.5 ); break;
    case -2: vik_viewport_set_zoom ( vw->viking_vvp, 0.25 ); break;
    default: vik_viewport_set_zoom ( vw->viking_vvp, what );
  }
  draw_update ( vw );
}

void draw_goto_cb ( GtkAction *a, VikWindow *vw )
{
  VikCoord new_center;

  if (!strcmp(gtk_action_get_name(a), "GotoLL")) {
    struct LatLon ll, llold;
    vik_coord_to_latlon ( vik_viewport_get_center ( vw->viking_vvp ), &llold );
    if ( a_dialog_goto_latlon ( GTK_WINDOW(vw), &ll, &llold ) )
      vik_coord_load_from_latlon ( &new_center, vik_viewport_get_coord_mode(vw->viking_vvp), &ll );
    else
      return;
  }
  else if (!strcmp(gtk_action_get_name(a), "GotoUTM")) {
    struct UTM utm, utmold;
    vik_coord_to_utm ( vik_viewport_get_center ( vw->viking_vvp ), &utmold );
    if ( a_dialog_goto_utm ( GTK_WINDOW(vw), &utm, &utmold ) )
      vik_coord_load_from_utm ( &new_center, vik_viewport_get_coord_mode(vw->viking_vvp), &utm );
    else
     return;
  }
  else {
    g_critical("Houston, we've had a problem.");
    return;
  }

  vik_viewport_set_center_coord ( vw->viking_vvp, &new_center );
  draw_update ( vw );
}

static void menu_addlayer_cb ( GtkAction *a, VikWindow *vw )
{
  gint type;
  for ( type = 0; type < VIK_LAYER_NUM_TYPES; type++ ) {
    if (!strcmp(vik_layer_get_interface(type)->name, gtk_action_get_name(a))) {
      if ( vik_layers_panel_new_layer ( vw->viking_vlp, type ) ) {
	draw_update ( vw );
	vw->modified = TRUE;
      }
    }
  }
}

static void menu_copy_layer_cb ( GtkAction *a, VikWindow *vw )
{
  a_clipboard_copy_selected ( vw->viking_vlp );
}

static void menu_cut_layer_cb ( GtkAction *a, VikWindow *vw )
{
  vik_layers_panel_cut_selected ( vw->viking_vlp );
  vw->modified = TRUE;
}

static void menu_paste_layer_cb ( GtkAction *a, VikWindow *vw )
{
  if ( a_clipboard_paste ( vw->viking_vlp ) )
  {
    vw->modified = TRUE;
  }
}

static void menu_properties_cb ( GtkAction *a, VikWindow *vw )
{
  if ( ! vik_layers_panel_properties ( vw->viking_vlp ) )
    a_dialog_info_msg ( GTK_WINDOW(vw), _("You must select a layer to show its properties.") );
}

static void help_help_cb ( GtkAction *a, VikWindow *vw )
{
#ifdef WINDOWS
  ShellExecute(NULL, "open", ""PACKAGE".pdf", NULL, NULL, SW_SHOWNORMAL);
#else /* WINDOWS */
#if GTK_CHECK_VERSION (2, 14, 0)
  gchar *uri;
  uri = g_strdup_printf("ghelp:%s", PACKAGE);
  gtk_show_uri(NULL, uri, GDK_CURRENT_TIME, NULL);
  g_free(uri);
#endif
#endif /* WINDOWS */
}

static void help_about_cb ( GtkAction *a, VikWindow *vw )
{
  a_dialog_about(GTK_WINDOW(vw));
}

static void menu_delete_layer_cb ( GtkAction *a, VikWindow *vw )
{
  if ( vik_layers_panel_get_selected ( vw->viking_vlp ) )
  {
    vik_layers_panel_delete_selected ( vw->viking_vlp );
    vw->modified = TRUE;
  }
  else
    a_dialog_info_msg ( GTK_WINDOW(vw), _("You must select a layer to delete.") );
}

static void view_side_panel_cb ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanel" );
  g_assert(check_box);
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box));
  if ( state )
    gtk_widget_show(GTK_WIDGET(vw->viking_vlp));
  else
    gtk_widget_hide(GTK_WIDGET(vw->viking_vlp));
}

static void view_statusbar_cb ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewStatusBar" );
  if ( !check_box )
    return;
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( state )
    gtk_widget_show ( GTK_WIDGET(vw->viking_vs) );
  else
    gtk_widget_hide ( GTK_WIDGET(vw->viking_vs) );
}

static void view_toolbar_cb ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewToolbar" );
  if ( !check_box )
    return;
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( state )
    gtk_widget_show ( GTK_WIDGET(vw->toolbar) );
  else
    gtk_widget_hide ( GTK_WIDGET(vw->toolbar) );
}

static void view_main_menu_cb ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewMainMenu" );
  if ( !check_box )
    return;
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( !state )
    gtk_widget_hide ( gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu" ) );
  else
    gtk_widget_show ( gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu" ) );
}

/***************************************
 ** tool management routines
 **
 ***************************************/

static toolbox_tools_t* toolbox_create(VikWindow *vw)
{
  toolbox_tools_t *vt = g_new(toolbox_tools_t, 1);
  vt->tools = NULL;
  vt->n_tools = 0;
  vt->active_tool = -1;
  vt->vw = vw;
  if (!vw->viking_vvp) {
    g_critical("no viewport found.");
    exit(1);
  }
  return vt;
}

static void toolbox_add_tool(toolbox_tools_t *vt, VikToolInterface *vti, gint layer_type )
{
  vt->tools = g_renew(toolbox_tool_t, vt->tools, vt->n_tools+1);
  vt->tools[vt->n_tools].ti = *vti;
  vt->tools[vt->n_tools].layer_type = layer_type;
  if (vti->create) {
    vt->tools[vt->n_tools].state = vti->create(vt->vw, vt->vw->viking_vvp);
  } 
  else {
    vt->tools[vt->n_tools].state = NULL;
  }
  vt->n_tools++;
}

static int toolbox_get_tool(toolbox_tools_t *vt, const gchar *tool_name)
{
  int i;
  for (i=0; i<vt->n_tools; i++) {
    if (!strcmp(tool_name, vt->tools[i].ti.name)) {
      break;
    }
  }
  return i;
}

static void toolbox_activate(toolbox_tools_t *vt, const gchar *tool_name)
{
  int tool = toolbox_get_tool(vt, tool_name);
  toolbox_tool_t *t = &vt->tools[tool];
  VikLayer *vl = vik_layers_panel_get_selected ( vt->vw->viking_vlp );

  if (tool == vt->n_tools) {
    g_critical("trying to activate a non-existent tool...");
    exit(1);
  }
  /* is the tool already active? */
  if (vt->active_tool == tool) {
    return;
  }

  if (vt->active_tool != -1) {
    if (vt->tools[vt->active_tool].ti.deactivate) {
      vt->tools[vt->active_tool].ti.deactivate(NULL, vt->tools[vt->active_tool].state);
    }
  }
  if (t->ti.activate) {
    t->ti.activate(vl, t->state);
  }
  vt->active_tool = tool;
}

static const GdkCursor *toolbox_get_cursor(toolbox_tools_t *vt, const gchar *tool_name)
{
  int tool = toolbox_get_tool(vt, tool_name);
  toolbox_tool_t *t = &vt->tools[tool];
  if (t->ti.cursor == NULL) {
    if (t->ti.cursor_type == GDK_CURSOR_IS_PIXMAP && t->ti.cursor_data != NULL) {
      GError *cursor_load_err = NULL;
      GdkPixbuf *cursor_pixbuf = gdk_pixbuf_from_pixdata (t->ti.cursor_data, FALSE, &cursor_load_err);
      /* TODO: settable offeset */
      t->ti.cursor = gdk_cursor_new_from_pixbuf ( gdk_display_get_default(), cursor_pixbuf, 3, 3 );
      g_object_unref ( G_OBJECT(cursor_pixbuf) );
    } else {
      t->ti.cursor = gdk_cursor_new ( t->ti.cursor_type );
    }
  }
  return t->ti.cursor;
}

static void toolbox_click (toolbox_tools_t *vt, GdkEventButton *event)
{
  VikLayer *vl = vik_layers_panel_get_selected ( vt->vw->viking_vlp );
  if (vt->active_tool != -1 && vt->tools[vt->active_tool].ti.click) {
    gint ltype = vt->tools[vt->active_tool].layer_type;
    if ( ltype == TOOL_LAYER_TYPE_NONE || (vl && ltype == vl->type) )
      vt->tools[vt->active_tool].ti.click(vl, event, vt->tools[vt->active_tool].state);
  }
}

static void toolbox_move (toolbox_tools_t *vt, GdkEventMotion *event)
{
  VikLayer *vl = vik_layers_panel_get_selected ( vt->vw->viking_vlp );
  if (vt->active_tool != -1 && vt->tools[vt->active_tool].ti.move) {
    gint ltype = vt->tools[vt->active_tool].layer_type;
    if ( ltype == TOOL_LAYER_TYPE_NONE || (vl && ltype == vl->type) )
      if ( VIK_LAYER_TOOL_ACK_GRAB_FOCUS == vt->tools[vt->active_tool].ti.move(vl, event, vt->tools[vt->active_tool].state) )
        gtk_widget_grab_focus ( GTK_WIDGET(vt->vw->viking_vvp) );
  }
}

static void toolbox_release (toolbox_tools_t *vt, GdkEventButton *event)
{
  VikLayer *vl = vik_layers_panel_get_selected ( vt->vw->viking_vlp );
  if (vt->active_tool != -1 && vt->tools[vt->active_tool].ti.release ) {
    gint ltype = vt->tools[vt->active_tool].layer_type;
    if ( ltype == TOOL_LAYER_TYPE_NONE || (vl && ltype == vl->type) )
      vt->tools[vt->active_tool].ti.release(vl, event, vt->tools[vt->active_tool].state);
  }
}
/** End tool management ************************************/

void vik_window_enable_layer_tool ( VikWindow *vw, gint layer_id, gint tool_id )
{
  gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, vik_layer_get_interface(layer_id)->tools[tool_id].name ) );
}

/* this function gets called whenever a toolbar tool is clicked */
static void menu_tool_cb ( GtkAction *old, GtkAction *a, VikWindow *vw )
{
  /* White Magic, my friends ... White Magic... */
  int layer_id, tool_id;
  const GdkCursor *cursor = NULL;

  toolbox_activate(vw->vt, gtk_action_get_name(a));

  cursor = toolbox_get_cursor(vw->vt, gtk_action_get_name(a));
  /* We set cursor, even if it is NULL: it resets to default */
  gdk_window_set_cursor ( GTK_WIDGET(vw->viking_vvp)->window, (GdkCursor *)cursor );

  if (!strcmp(gtk_action_get_name(a), "Pan")) {
    vw->current_tool = TOOL_PAN;
  } 
  else if (!strcmp(gtk_action_get_name(a), "Zoom")) {
    vw->current_tool = TOOL_ZOOM;
  } 
  else if (!strcmp(gtk_action_get_name(a), "Ruler")) {
    vw->current_tool = TOOL_RULER;
  }
  else if (!strcmp(gtk_action_get_name(a), "Select")) {
    vw->current_tool = TOOL_SELECT;
  }
  else {
    /* TODO: only enable tools from active layer */
    for (layer_id=0; layer_id<VIK_LAYER_NUM_TYPES; layer_id++) {
      for ( tool_id = 0; tool_id < vik_layer_get_interface(layer_id)->tools_count; tool_id++ ) {
	if (!strcmp(vik_layer_get_interface(layer_id)->tools[tool_id].name, gtk_action_get_name(a))) {
           vw->current_tool = TOOL_LAYER;
           vw->tool_layer_id = layer_id;
           vw->tool_tool_id = tool_id;
	}
      }
    }
  }
  draw_status ( vw );
}

static void window_set_filename ( VikWindow *vw, const gchar *filename )
{
  gchar *title;
  const gchar *file;
  if ( vw->filename )
    g_free ( vw->filename );
  if ( filename == NULL )
  {
    vw->filename = NULL;
    file = _("Untitled");
  }
  else
  {
    vw->filename = g_strdup(filename);
    file = a_file_basename ( filename );
  }
  title = g_strdup_printf( "%s - Viking", file );
  gtk_window_set_title ( GTK_WINDOW(vw), title );
  g_free ( title );
}

GtkWidget *vik_window_get_drawmode_button ( VikWindow *vw, VikViewportDrawMode mode )
{
  GtkWidget *mode_button;
  gchar *buttonname;
  switch ( mode ) {
#ifdef VIK_CONFIG_EXPEDIA
    case VIK_VIEWPORT_DRAWMODE_EXPEDIA: buttonname = "/ui/MainMenu/View/ModeExpedia"; break;
#endif
    case VIK_VIEWPORT_DRAWMODE_MERCATOR: buttonname = "/ui/MainMenu/View/ModeMercator"; break;
    case VIK_VIEWPORT_DRAWMODE_LATLON: buttonname = "/ui/MainMenu/View/ModeLatLon"; break;
    default: buttonname = "/ui/MainMenu/View/ModeUTM";
  }
  mode_button = gtk_ui_manager_get_widget ( vw->uim, buttonname );
  g_assert ( mode_button );
  return mode_button;
}

/**
 * vik_window_get_pan_move:
 * @vw: some VikWindow
 *
 * Retrieves @vw's pan_move.
 *
 * Should be removed as soon as possible.
 *
 * Returns: @vw's pan_move
 *
 * Since: 0.9.96
 **/
gboolean vik_window_get_pan_move ( VikWindow *vw )
{
  return vw->pan_move;
}

static void on_activate_recent_item (GtkRecentChooser *chooser,
                                     VikWindow *self)
{
  gchar *filename;

  filename = gtk_recent_chooser_get_current_uri (chooser);
  if (filename != NULL)
  {
    GFile *file = g_file_new_for_uri ( filename );
    gchar *path = g_file_get_path ( file );
    g_object_unref ( file );
    if ( self->filename )
    {
      GSList *filenames = NULL;
      filenames = g_slist_append ( filenames, path );
      g_signal_emit ( G_OBJECT(self), window_signals[VW_OPENWINDOW_SIGNAL], 0, filenames );
      // NB: GSList & contents are freed by main.open_window
    }
    else {
      vik_window_open_file ( self, path, TRUE );
      g_free ( path );
    }
  }

  g_free (filename);
}

static void setup_recent_files (VikWindow *self)
{
  GtkRecentManager *manager;
  GtkRecentFilter *filter;
  GtkWidget *menu, *menu_item;

  filter = gtk_recent_filter_new ();
  /* gtk_recent_filter_add_application (filter, g_get_application_name()); */
  gtk_recent_filter_add_group(filter, "viking");

  manager = gtk_recent_manager_get_default ();
  menu = gtk_recent_chooser_menu_new_for_manager (manager);
  gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (menu), GTK_RECENT_SORT_MRU);
  gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (menu), filter);

  menu_item = gtk_ui_manager_get_widget (self->uim, "/ui/MainMenu/File/OpenRecentFile");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);

  g_signal_connect (G_OBJECT (menu), "item-activated",
                    G_CALLBACK (on_activate_recent_item), (gpointer) self);
}

static void update_recently_used_document(const gchar *filename)
{
  /* Update Recently Used Document framework */
  GtkRecentManager *manager = gtk_recent_manager_get_default();
  GtkRecentData *recent_data = g_slice_new (GtkRecentData);
  gchar *groups[] = {"viking", NULL};
  GFile *file = g_file_new_for_commandline_arg(filename);
  gchar *uri = g_file_get_uri(file);
  gchar *basename = g_path_get_basename(filename);
  g_object_unref(file);
  file = NULL;

  recent_data->display_name   = basename;
  recent_data->description    = NULL;
  recent_data->mime_type      = "text/x-gps-data";
  recent_data->app_name       = (gchar *) g_get_application_name ();
  recent_data->app_exec       = g_strjoin (" ", g_get_prgname (), "%f", NULL);
  recent_data->groups         = groups;
  recent_data->is_private     = FALSE;
  if (!gtk_recent_manager_add_full (manager, uri, recent_data))
  {
    g_warning (_("Unable to add '%s' to the list of recently used documents"), uri);
  }

  g_free (uri);
  g_free (basename);
  g_free (recent_data->app_exec);
  g_slice_free (GtkRecentData, recent_data);
}

void vik_window_open_file ( VikWindow *vw, const gchar *filename, gboolean change_filename )
{
  switch ( a_file_load ( vik_layers_panel_get_top_layer(vw->viking_vlp), vw->viking_vvp, filename ) )
  {
    case LOAD_TYPE_READ_FAILURE:
      a_dialog_error_msg ( GTK_WINDOW(vw), _("The file you requested could not be opened.") );
      break;
    case LOAD_TYPE_GPSBABEL_FAILURE:
      a_dialog_error_msg ( GTK_WINDOW(vw), _("GPSBabel is required to load files of this type or GPSBabel encountered problems.") );
      break;
    case LOAD_TYPE_UNSUPPORTED_FAILURE:
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unsupported file type for %s"), filename );
      break;
    case LOAD_TYPE_VIK_SUCCESS:
    {
      GtkWidget *mode_button;
      /* Update UI */
      if ( change_filename )
        window_set_filename ( vw, filename );
      mode_button = vik_window_get_drawmode_button ( vw, vik_viewport_get_drawmode ( vw->viking_vvp ) );
      vw->only_updating_coord_mode_ui = TRUE; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
      gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(mode_button), TRUE );
      vw->only_updating_coord_mode_ui = FALSE;

      vik_layers_panel_change_coord_mode ( vw->viking_vlp, vik_viewport_get_coord_mode ( vw->viking_vvp ) );
      
      mode_button = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowScale" );
      g_assert ( mode_button );
      gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(mode_button),vik_viewport_get_draw_scale(vw->viking_vvp) );

      mode_button = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowCenterMark" );
      g_assert ( mode_button );
      gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(mode_button),vik_viewport_get_draw_centermark(vw->viking_vvp) );
      
      mode_button = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowHighlight" );
      g_assert ( mode_button );
      gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(mode_button),vik_viewport_get_draw_highlight (vw->viking_vvp) );
    }
    //case LOAD_TYPE_OTHER_SUCCESS:
    default:
      update_recently_used_document(filename);
      draw_update ( vw );
      break;
  }
}
static void load_file ( GtkAction *a, VikWindow *vw )
{
  GSList *files = NULL;
  GSList *cur_file = NULL;
  gboolean newwindow;
  if (!strcmp(gtk_action_get_name(a), "Open")) {
    newwindow = TRUE;
  } 
  else if (!strcmp(gtk_action_get_name(a), "Append")) {
    newwindow = FALSE;
  } 
  else {
    g_critical("Houston, we've had a problem.");
    return;
  }
    
  if ( ! vw->open_dia )
  {
    vw->open_dia = gtk_file_chooser_dialog_new (_("Please select a GPS data file to open. "),
				      GTK_WINDOW(vw),
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
    GtkFileFilter *filter;
    // NB file filters are listed this way for alphabetical ordering
#ifdef VIK_CONFIG_GEOCACHES
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name( filter, _("Geocaching") );
    gtk_file_filter_add_pattern ( filter, "*.loc" ); // No MIME type available
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(vw->open_dia), filter);
#endif

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name( filter, _("Google Earth") );
    gtk_file_filter_add_mime_type ( filter, "application/vnd.google-earth.kml+xml");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(vw->open_dia), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name( filter, _("GPX") );
    gtk_file_filter_add_pattern ( filter, "*.gpx" ); // No MIME type available
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(vw->open_dia), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name( filter, _("Viking") );
    gtk_file_filter_add_pattern ( filter, "*.vik" );
    gtk_file_filter_add_pattern ( filter, "*.viking" );
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(vw->open_dia), filter);

    // NB could have filters for gpspoint (*.gps,*.gpsoint?) + gpsmapper (*.gsm,*.gpsmapper?)
    // However assume this are barely used and thus not worthy of inclusion
    //   as they'll just make the options too many and have no clear file pattern
    //   one can always use the all option
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name( filter, _("All") );
    gtk_file_filter_add_pattern ( filter, "*" );
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(vw->open_dia), filter);
    // Default to any file - same as before open filters were added
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER(vw->open_dia), filter);

    gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER(vw->open_dia), TRUE );
    gtk_window_set_transient_for ( GTK_WINDOW(vw->open_dia), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->open_dia), TRUE );
  }
  if ( gtk_dialog_run ( GTK_DIALOG(vw->open_dia) ) == GTK_RESPONSE_ACCEPT )
  {
    gtk_widget_hide ( vw->open_dia );
#ifdef VIKING_PROMPT_IF_MODIFIED
    if ( (vw->modified || vw->filename) && newwindow )
#else
    if ( vw->filename && newwindow )
#endif
      g_signal_emit ( G_OBJECT(vw), window_signals[VW_OPENWINDOW_SIGNAL], 0, gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(vw->open_dia) ) );
    else {
      files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(vw->open_dia) );
      gboolean change_fn = newwindow && (g_slist_length(files)==1); /* only change fn if one file */
      
      cur_file = files;
      while ( cur_file ) {
        gchar *file_name = cur_file->data;
        vik_window_open_file ( vw, file_name, change_fn );
        g_free (file_name);
        cur_file = g_slist_next (cur_file);
      }
      g_slist_free (files);
    }
  }
  else
    gtk_widget_hide ( vw->open_dia );
}

static gboolean save_file_as ( GtkAction *a, VikWindow *vw )
{
  gboolean rv = FALSE;
  const gchar *fn;
  if ( ! vw->save_dia )
  {
    vw->save_dia = gtk_file_chooser_dialog_new (_("Save as Viking File."),
				      GTK_WINDOW(vw),
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);
    gtk_window_set_transient_for ( GTK_WINDOW(vw->save_dia), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->save_dia), TRUE );
  }

  while ( gtk_dialog_run ( GTK_DIALOG(vw->save_dia) ) == GTK_RESPONSE_ACCEPT )
  {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(vw->save_dia) );
    if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) == FALSE || a_dialog_yes_or_no ( GTK_WINDOW(vw->save_dia), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
    {
      window_set_filename ( vw, fn );
      rv = window_save ( vw );
      vw->modified = FALSE;
      break;
    }
  }
  gtk_widget_hide ( vw->save_dia );
  return rv;
}

static gboolean window_save ( VikWindow *vw )
{
  if ( a_file_save ( vik_layers_panel_get_top_layer ( vw->viking_vlp ), vw->viking_vvp, vw->filename ) )
  {
    update_recently_used_document ( vw->filename );
    return TRUE;
  }
  else
  {
    a_dialog_error_msg ( GTK_WINDOW(vw), _("The filename you requested could not be opened for writing.") );
    return FALSE;
  }
}

static gboolean save_file ( GtkAction *a, VikWindow *vw )
{
  if ( ! vw->filename )
    return save_file_as ( NULL, vw );
  else
  {
    vw->modified = FALSE;
    return window_save ( vw );
  }
}

static void acquire_from_gps ( GtkAction *a, VikWindow *vw )
{
  // Via the file menu, acquiring from a GPS makes a new layer
  //  this has always been the way (not entirely sure if this was the real intention!)
  //  thus maintain the behaviour ATM.
  // Hence explicit setting here (as the value may be changed elsewhere)
  vik_datasource_gps_interface.mode = VIK_DATASOURCE_CREATENEWLAYER;
  a_acquire(vw, vw->viking_vlp, vw->viking_vvp, &vik_datasource_gps_interface );
}

static void acquire_from_file ( GtkAction *a, VikWindow *vw )
{
  a_acquire(vw, vw->viking_vlp, vw->viking_vvp, &vik_datasource_file_interface );
}

static void acquire_from_google ( GtkAction *a, VikWindow *vw )
{
  a_acquire(vw, vw->viking_vlp, vw->viking_vvp, &vik_datasource_google_interface );
}

#ifdef VIK_CONFIG_OPENSTREETMAP
static void acquire_from_osm ( GtkAction *a, VikWindow *vw )
{
  a_acquire(vw, vw->viking_vlp, vw->viking_vvp, &vik_datasource_osm_interface );
}
#endif

#ifdef VIK_CONFIG_GEOCACHES
static void acquire_from_gc ( GtkAction *a, VikWindow *vw )
{
  a_acquire(vw, vw->viking_vlp, vw->viking_vvp, &vik_datasource_gc_interface );
}
#endif

#ifdef VIK_CONFIG_GEOTAG
static void acquire_from_geotag ( GtkAction *a, VikWindow *vw )
{
  vik_datasource_geotag_interface.mode = VIK_DATASOURCE_CREATENEWLAYER;
  a_acquire(vw, vw->viking_vlp, vw->viking_vvp, &vik_datasource_geotag_interface );
}
#endif

static void goto_default_location( GtkAction *a, VikWindow *vw)
{
  struct LatLon ll;
  ll.lat = a_vik_get_default_lat();
  ll.lon = a_vik_get_default_long();
  vik_viewport_set_center_latlon(vw->viking_vvp, &ll);
  vik_layers_panel_emit_update(vw->viking_vlp);
}


static void goto_address( GtkAction *a, VikWindow *vw)
{
  a_vik_goto(vw, vw->viking_vlp, vw->viking_vvp);
}

static void mapcache_flush_cb ( GtkAction *a, VikWindow *vw )
{
  a_mapcache_flush();
}

static void preferences_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean wp_icon_size = a_vik_get_use_large_waypoint_icons();

  a_preferences_show_window ( GTK_WINDOW(vw) );

  // Delete icon indexing 'cache' and so automatically regenerates with the new setting when changed
  if (wp_icon_size != a_vik_get_use_large_waypoint_icons())
    clear_garmin_icon_syms ();

  draw_update ( vw );
}

static void default_location_cb ( GtkAction *a, VikWindow *vw )
{
  /* Simplistic repeat of preference setting
     Only the name & type are important for setting the preference via this 'external' way */
  VikLayerParam pref_lat[] = {
    { VIKING_PREFERENCES_NAMESPACE "default_latitude",
      VIK_LAYER_PARAM_DOUBLE,
      VIK_LOCATION_LAT,
      NULL,
      VIK_LAYER_WIDGET_SPINBUTTON,
      NULL,
      NULL },
  };
  VikLayerParam pref_lon[] = {
    { VIKING_PREFERENCES_NAMESPACE "default_longitude",
      VIK_LAYER_PARAM_DOUBLE,
      VIK_LOCATION_LONG,
      NULL,
      VIK_LAYER_WIDGET_SPINBUTTON,
      NULL,
      NULL },
  };

  /* Get current center */
  struct LatLon ll;
  vik_coord_to_latlon ( vik_viewport_get_center ( vw->viking_vvp ), &ll );

  /* Apply to preferences */
  VikLayerParamData vlp_data;
  vlp_data.d = ll.lat;
  a_preferences_run_setparam (vlp_data, pref_lat);
  vlp_data.d = ll.lon;
  a_preferences_run_setparam (vlp_data, pref_lon);
  /* Remember to save */
  a_preferences_save_to_file();
}

static void clear_cb ( GtkAction *a, VikWindow *vw )
{
  vik_layers_panel_clear ( vw->viking_vlp );
  window_set_filename ( vw, NULL );
  draw_update ( vw );
}

static void window_close ( GtkAction *a, VikWindow *vw )
{
  if ( ! delete_event ( vw ) )
    gtk_widget_destroy ( GTK_WIDGET(vw) );
}

static gboolean save_file_and_exit ( GtkAction *a, VikWindow *vw )
{
  if (save_file( NULL, vw)) {
    window_close( NULL, vw);
    return(TRUE);
  }
  else
    return(FALSE);
}

static void zoom_to_cb ( GtkAction *a, VikWindow *vw )
{
  gdouble xmpp = vik_viewport_get_xmpp ( vw->viking_vvp ), ympp = vik_viewport_get_ympp ( vw->viking_vvp );
  if ( a_dialog_custom_zoom ( GTK_WINDOW(vw), &xmpp, &ympp ) )
  {
    vik_viewport_set_xmpp ( vw->viking_vvp, xmpp );
    vik_viewport_set_ympp ( vw->viking_vvp, ympp );
    draw_update ( vw );
  }
}

static void save_image_file ( VikWindow *vw, const gchar *fn, guint w, guint h, gdouble zoom, gboolean save_as_png )
{
  /* more efficient way: stuff draws directly to pixbuf (fork viewport) */
  GdkPixbuf *pixbuf_to_save;
  gdouble old_xmpp, old_ympp;
  GError *error = NULL;

  /* backup old zoom & set new */
  old_xmpp = vik_viewport_get_xmpp ( vw->viking_vvp );
  old_ympp = vik_viewport_get_ympp ( vw->viking_vvp );
  vik_viewport_set_zoom ( vw->viking_vvp, zoom );

  /* reset width and height: */
  vik_viewport_configure_manually ( vw->viking_vvp, w, h );

  /* draw all layers */
  draw_redraw ( vw );

  /* save buffer as file. */
  pixbuf_to_save = gdk_pixbuf_get_from_drawable ( NULL, GDK_DRAWABLE(vik_viewport_get_pixmap ( vw->viking_vvp )), NULL, 0, 0, 0, 0, w, h);
  gdk_pixbuf_save ( pixbuf_to_save, fn, save_as_png ? "png" : "jpeg", &error, NULL );
  if (error)
  {
    g_warning("Unable to write to file %s: %s", fn, error->message );
    g_error_free (error);
  }
  g_object_unref ( G_OBJECT(pixbuf_to_save) );

  /* pretend like nothing happened ;) */
  vik_viewport_set_xmpp ( vw->viking_vvp, old_xmpp );
  vik_viewport_set_ympp ( vw->viking_vvp, old_ympp );
  vik_viewport_configure ( vw->viking_vvp );
  draw_update ( vw );
}

static void save_image_dir ( VikWindow *vw, const gchar *fn, guint w, guint h, gdouble zoom, gboolean save_as_png, guint tiles_w, guint tiles_h )
{
  gulong size = sizeof(gchar) * (strlen(fn) + 15);
  gchar *name_of_file = g_malloc ( size );
  guint x = 1, y = 1;
  struct UTM utm_orig, utm;

  /* *** copied from above *** */
  GdkPixbuf *pixbuf_to_save;
  gdouble old_xmpp, old_ympp;
  GError *error = NULL;

  /* backup old zoom & set new */
  old_xmpp = vik_viewport_get_xmpp ( vw->viking_vvp );
  old_ympp = vik_viewport_get_ympp ( vw->viking_vvp );
  vik_viewport_set_zoom ( vw->viking_vvp, zoom );

  /* reset width and height: do this only once for all images (same size) */
  vik_viewport_configure_manually ( vw->viking_vvp, w, h );
  /* *** end copy from above *** */

  g_assert ( vik_viewport_get_coord_mode ( vw->viking_vvp ) == VIK_COORD_UTM );

  g_mkdir(fn,0777);

  utm_orig = *((const struct UTM *)vik_viewport_get_center ( vw->viking_vvp ));

  for ( y = 1; y <= tiles_h; y++ )
  {
    for ( x = 1; x <= tiles_w; x++ )
    {
      g_snprintf ( name_of_file, size, "%s%cy%d-x%d.%s", fn, G_DIR_SEPARATOR, y, x, save_as_png ? "png" : "jpg" );
      utm = utm_orig;
      if ( tiles_w & 0x1 )
        utm.easting += ((gdouble)x - ceil(((gdouble)tiles_w)/2)) * (w*zoom);
      else
        utm.easting += ((gdouble)x - (((gdouble)tiles_w)+1)/2) * (w*zoom);
      if ( tiles_h & 0x1 ) /* odd */
        utm.northing -= ((gdouble)y - ceil(((gdouble)tiles_h)/2)) * (h*zoom);
      else /* even */
        utm.northing -= ((gdouble)y - (((gdouble)tiles_h)+1)/2) * (h*zoom);

      /* move to correct place. */
      vik_viewport_set_center_utm ( vw->viking_vvp, &utm );

      draw_redraw ( vw );

      /* save buffer as file. */
      pixbuf_to_save = gdk_pixbuf_get_from_drawable ( NULL, GDK_DRAWABLE(vik_viewport_get_pixmap ( vw->viking_vvp )), NULL, 0, 0, 0, 0, w, h);
      gdk_pixbuf_save ( pixbuf_to_save, name_of_file, save_as_png ? "png" : "jpeg", &error, NULL );
      if (error)
      {
        g_warning("Unable to write to file %s: %s", name_of_file, error->message );
        g_error_free (error);
      }

      g_object_unref ( G_OBJECT(pixbuf_to_save) );
    }
  }

  vik_viewport_set_center_utm ( vw->viking_vvp, &utm_orig );
  vik_viewport_set_xmpp ( vw->viking_vvp, old_xmpp );
  vik_viewport_set_ympp ( vw->viking_vvp, old_ympp );
  vik_viewport_configure ( vw->viking_vvp );
  draw_update ( vw );

  g_free ( name_of_file );
}

static void draw_to_image_file_current_window_cb(GtkWidget* widget,GdkEventButton *event,gpointer *pass_along)
{
  VikWindow *vw = VIK_WINDOW(pass_along[0]);
  GtkSpinButton *width_spin = GTK_SPIN_BUTTON(pass_along[1]), *height_spin = GTK_SPIN_BUTTON(pass_along[2]);
  GtkSpinButton *zoom_spin = GTK_SPIN_BUTTON(pass_along[3]);
  gdouble width_min, width_max, height_min, height_max;
  gint width, height;

  gtk_spin_button_get_range ( width_spin, &width_min, &width_max );
  gtk_spin_button_get_range ( height_spin, &height_min, &height_max );

  /* TODO: support for xzoom and yzoom values */
  width = vik_viewport_get_width ( vw->viking_vvp ) * vik_viewport_get_xmpp ( vw->viking_vvp ) / gtk_spin_button_get_value ( zoom_spin );
  height = vik_viewport_get_height ( vw->viking_vvp ) * vik_viewport_get_xmpp ( vw->viking_vvp ) / gtk_spin_button_get_value ( zoom_spin );

  if ( width > width_max || width < width_min || height > height_max || height < height_min )
    a_dialog_info_msg ( GTK_WINDOW(vw), _("Viewable region outside allowable pixel size bounds for image. Clipping width/height values.") );

  gtk_spin_button_set_value ( width_spin, width );
  gtk_spin_button_set_value ( height_spin, height );
}

static void draw_to_image_file_total_area_cb (GtkSpinButton *spinbutton, gpointer *pass_along)
{
  GtkSpinButton *width_spin = GTK_SPIN_BUTTON(pass_along[1]), *height_spin = GTK_SPIN_BUTTON(pass_along[2]);
  GtkSpinButton *zoom_spin = GTK_SPIN_BUTTON(pass_along[3]);
  gchar *label_text;
  gdouble w, h;
  w = gtk_spin_button_get_value(width_spin) * gtk_spin_button_get_value(zoom_spin);
  h = gtk_spin_button_get_value(height_spin) * gtk_spin_button_get_value(zoom_spin);
  if (pass_along[4]) /* save many images; find TOTAL area covered */
  {
    w *= gtk_spin_button_get_value(GTK_SPIN_BUTTON(pass_along[4]));
    h *= gtk_spin_button_get_value(GTK_SPIN_BUTTON(pass_along[5]));
  }
  vik_units_distance_t dist_units = a_vik_get_units_distance ();
  switch (dist_units) {
  case VIK_UNITS_DISTANCE_KILOMETRES:
    label_text = g_strdup_printf ( _("Total area: %ldm x %ldm (%.3f sq. km)"), (glong)w, (glong)h, (w*h/1000000));
    break;
  case VIK_UNITS_DISTANCE_MILES:
    label_text = g_strdup_printf ( _("Total area: %ldm x %ldm (%.3f sq. miles)"), (glong)w, (glong)h, (w*h/2589988.11));
    break;
  default:
    label_text = g_strdup_printf ("Just to keep the compiler happy");
    g_critical("Houston, we've had a problem. distance=%d", dist_units);
  }

  gtk_label_set_text(GTK_LABEL(pass_along[6]), label_text);
  g_free ( label_text );
}

static void draw_to_image_file ( VikWindow *vw, const gchar *fn, gboolean one_image_only )
{
  /* todo: default for answers inside VikWindow or static (thruout instance) */
  GtkWidget *dialog = gtk_dialog_new_with_buttons ( _("Save to Image File"), GTK_WINDOW(vw),
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL );
  GtkWidget *width_label, *width_spin, *height_label, *height_spin;
  GtkWidget *png_radio, *jpeg_radio;
  GtkWidget *current_window_button;
  gpointer current_window_pass_along[7];
  GtkWidget *zoom_label, *zoom_spin;
  GtkWidget *total_size_label;

  /* only used if (!one_image_only) */
  GtkWidget *tiles_width_spin = NULL, *tiles_height_spin = NULL;


  width_label = gtk_label_new ( _("Width (pixels):") );
  width_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vw->draw_image_width, 10, 5000, 10, 100, 0 )), 10, 0 );
  height_label = gtk_label_new ( _("Height (pixels):") );
  height_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vw->draw_image_height, 10, 5000, 10, 100, 0 )), 10, 0 );

  zoom_label = gtk_label_new ( _("Zoom (meters per pixel):") );
  /* TODO: separate xzoom and yzoom factors */
  zoom_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vik_viewport_get_xmpp(vw->viking_vvp), VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM/2.0, 1, 100, 0 )), 16, 0);

  total_size_label = gtk_label_new ( NULL );

  current_window_button = gtk_button_new_with_label ( _("Area in current viewable window") );
  current_window_pass_along [0] = vw;
  current_window_pass_along [1] = width_spin;
  current_window_pass_along [2] = height_spin;
  current_window_pass_along [3] = zoom_spin;
  current_window_pass_along [4] = NULL; /* used for one_image_only != 1 */
  current_window_pass_along [5] = NULL;
  current_window_pass_along [6] = total_size_label;
  g_signal_connect ( G_OBJECT(current_window_button), "button_press_event", G_CALLBACK(draw_to_image_file_current_window_cb), current_window_pass_along );

  png_radio = gtk_radio_button_new_with_label ( NULL, _("Save as PNG") );
  jpeg_radio = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(png_radio), _("Save as JPEG") );

  if ( ! vw->draw_image_save_as_png )
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(jpeg_radio), TRUE );

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), width_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), width_spin, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), height_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), height_spin, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), current_window_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), png_radio, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), jpeg_radio, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), zoom_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), zoom_spin, FALSE, FALSE, 0);

  if ( ! one_image_only )
  {
    GtkWidget *tiles_width_label, *tiles_height_label;


    tiles_width_label = gtk_label_new ( _("East-west image tiles:") );
    tiles_width_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( 5, 1, 10, 1, 100, 0 )), 1, 0 );
    tiles_height_label = gtk_label_new ( _("North-south image tiles:") );
    tiles_height_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( 5, 1, 10, 1, 100, 0 )), 1, 0 );
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), tiles_width_label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), tiles_width_spin, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), tiles_height_label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), tiles_height_spin, FALSE, FALSE, 0);

    current_window_pass_along [4] = tiles_width_spin;
    current_window_pass_along [5] = tiles_height_spin;
    g_signal_connect ( G_OBJECT(tiles_width_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
    g_signal_connect ( G_OBJECT(tiles_height_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
  }
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), total_size_label, FALSE, FALSE, 0);
  g_signal_connect ( G_OBJECT(width_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
  g_signal_connect ( G_OBJECT(height_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
  g_signal_connect ( G_OBJECT(zoom_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );

  draw_to_image_file_total_area_cb ( NULL, current_window_pass_along ); /* set correct size info now */

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  gtk_widget_show_all ( GTK_DIALOG(dialog)->vbox );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    gtk_widget_hide ( GTK_WIDGET(dialog) );
    if ( one_image_only )
      save_image_file ( vw, fn, 
                      vw->draw_image_width = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(width_spin) ),
                      vw->draw_image_height = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(height_spin) ),
                      gtk_spin_button_get_value ( GTK_SPIN_BUTTON(zoom_spin) ), /* do not save this value, default is current zoom */
                      vw->draw_image_save_as_png = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(png_radio) ) );
    else {
      // NB is in UTM mode ATM
      save_image_dir ( vw, fn,
                       vw->draw_image_width = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(width_spin) ),
                       vw->draw_image_height = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(height_spin) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(zoom_spin) ), /* do not save this value, default is current zoom */
                       vw->draw_image_save_as_png = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(png_radio) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(tiles_width_spin) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(tiles_height_spin) ) );
    }
  }
  gtk_widget_destroy ( GTK_WIDGET(dialog) );
}


static void draw_to_image_file_cb ( GtkAction *a, VikWindow *vw )
{
  gchar *fn;
  if (!vw->save_img_dia) {
    vw->save_img_dia = gtk_file_chooser_dialog_new (_("Save Image"),
				      GTK_WINDOW(vw),
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);
    gtk_window_set_transient_for ( GTK_WINDOW(vw->save_img_dia), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->save_img_dia), TRUE );
  }

  while ( gtk_dialog_run ( GTK_DIALOG(vw->save_img_dia) ) == GTK_RESPONSE_ACCEPT )
  {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(vw->save_img_dia) );
    if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) == FALSE || a_dialog_yes_or_no ( GTK_WINDOW(vw->save_img_dia), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
    {
      draw_to_image_file ( vw, fn, TRUE );
      break;
    }
    g_free(fn);
    fn = NULL;
  }
  gtk_widget_hide ( vw->save_img_dia );
}

static void draw_to_image_dir_cb ( GtkAction *a, VikWindow *vw )
{
  gchar *fn = NULL;
  
  if ( vik_viewport_get_coord_mode(vw->viking_vvp) != VIK_COORD_UTM ) {
    a_dialog_error_msg ( GTK_WINDOW(vw), _("You must be in UTM mode to use this feature") );
    return;
  }

  if (!vw->save_img_dir_dia) {
    vw->save_img_dir_dia = gtk_file_chooser_dialog_new (_("Choose a directory to hold images"),
				      GTK_WINDOW(vw),
				      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				      NULL);
    gtk_window_set_transient_for ( GTK_WINDOW(vw->save_img_dir_dia), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->save_img_dir_dia), TRUE );
  }
  
  while ( gtk_dialog_run ( GTK_DIALOG(vw->save_img_dir_dia) ) == GTK_RESPONSE_ACCEPT )
  {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(vw->save_img_dir_dia) );
    if ( fn )
    {
      draw_to_image_file ( vw, fn, FALSE );
      g_free(fn);
      fn = NULL;
      break;
    }
  }
  gtk_widget_hide ( vw->save_img_dir_dia );
}

#if GTK_CHECK_VERSION(2,10,0)
static void print_cb ( GtkAction *a, VikWindow *vw )
{
  a_print(vw, vw->viking_vvp);
}
#endif

/* really a misnomer: changes coord mode (actual coordinates) AND/OR draw mode (viewport only) */
static void window_change_coord_mode_cb ( GtkAction *old_a, GtkAction *a, VikWindow *vw )
{
  VikViewportDrawMode drawmode;
  if (!strcmp(gtk_action_get_name(a), "ModeUTM")) {
    drawmode = VIK_VIEWPORT_DRAWMODE_UTM;
  }
  else if (!strcmp(gtk_action_get_name(a), "ModeLatLon")) {
    drawmode = VIK_VIEWPORT_DRAWMODE_LATLON;
  }
  else if (!strcmp(gtk_action_get_name(a), "ModeExpedia")) {
    drawmode = VIK_VIEWPORT_DRAWMODE_EXPEDIA;
  }
  else if (!strcmp(gtk_action_get_name(a), "ModeMercator")) {
    drawmode = VIK_VIEWPORT_DRAWMODE_MERCATOR;
  }
  else {
    g_critical("Houston, we've had a problem.");
    return;
  }

  if ( !vw->only_updating_coord_mode_ui )
  {
    VikViewportDrawMode olddrawmode = vik_viewport_get_drawmode ( vw->viking_vvp );
    if ( olddrawmode != drawmode )
    {
      /* this takes care of coord mode too */
      vik_viewport_set_drawmode ( vw->viking_vvp, drawmode );
      if ( drawmode == VIK_VIEWPORT_DRAWMODE_UTM ) {
        vik_layers_panel_change_coord_mode ( vw->viking_vlp, VIK_COORD_UTM );
      } else if ( olddrawmode == VIK_VIEWPORT_DRAWMODE_UTM ) {
        vik_layers_panel_change_coord_mode ( vw->viking_vlp, VIK_COORD_LATLON );
      }
      draw_update ( vw );
    }
  }
}

static void set_draw_scale ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowScale" );
  g_assert(check_box);
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box));
  vik_viewport_set_draw_scale ( vw->viking_vvp, state );
  draw_update ( vw );
}

static void set_draw_centermark ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowCenterMark" );
  g_assert(check_box);
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box));
  vik_viewport_set_draw_centermark ( vw->viking_vvp, state );
  draw_update ( vw );
}

static void set_draw_highlight ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowHighlight" );
  g_assert(check_box);
  gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box));
  vik_viewport_set_draw_highlight (  vw->viking_vvp, state );
  draw_update ( vw );
}

static void set_bg_color ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *colorsd = gtk_color_selection_dialog_new ( _("Choose a background color") );
  GdkColor *color = vik_viewport_get_background_gdkcolor ( vw->viking_vvp );
  gtk_color_selection_set_previous_color ( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorsd)->colorsel), color );
  gtk_color_selection_set_current_color ( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorsd)->colorsel), color );
  if ( gtk_dialog_run ( GTK_DIALOG(colorsd) ) == GTK_RESPONSE_OK )
  {
    gtk_color_selection_get_current_color ( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorsd)->colorsel), color );
    vik_viewport_set_background_gdkcolor ( vw->viking_vvp, color );
    draw_update ( vw );
  }
  g_free ( color );
  gtk_widget_destroy ( colorsd );
}

static void set_highlight_color ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *colorsd = gtk_color_selection_dialog_new ( _("Choose a track highlight color") );
  GdkColor *color = vik_viewport_get_highlight_gdkcolor ( vw->viking_vvp );
  gtk_color_selection_set_previous_color ( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorsd)->colorsel), color );
  gtk_color_selection_set_current_color ( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorsd)->colorsel), color );
  if ( gtk_dialog_run ( GTK_DIALOG(colorsd) ) == GTK_RESPONSE_OK )
  {
    gtk_color_selection_get_current_color ( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorsd)->colorsel), color );
    vik_viewport_set_highlight_gdkcolor ( vw->viking_vvp, color );
    draw_update ( vw );
  }
  g_free ( color );
  gtk_widget_destroy ( colorsd );
}



/***********************************************************************************************
 ** GUI Creation
 ***********************************************************************************************/

static GtkActionEntry entries[] = {
  { "File", NULL, N_("_File"), 0, 0, 0 },
  { "Edit", NULL, N_("_Edit"), 0, 0, 0 },
  { "View", NULL, N_("_View"), 0, 0, 0 },
  { "SetShow", NULL, N_("_Show"), 0, 0, 0 },
  { "SetZoom", NULL, N_("_Zoom"), 0, 0, 0 },
  { "SetPan", NULL, N_("_Pan"), 0, 0, 0 },
  { "Layers", NULL, N_("_Layers"), 0, 0, 0 },
  { "Tools", NULL, N_("_Tools"), 0, 0, 0 },
  { "Exttools", NULL, N_("_Webtools"), 0, 0, 0 },
  { "Help", NULL, N_("_Help"), 0, 0, 0 },

  { "New",       GTK_STOCK_NEW,          N_("_New"),                          "<control>N", N_("New file"),                                     (GCallback)newwindow_cb          },
  { "Open",      GTK_STOCK_OPEN,         N_("_Open..."),                         "<control>O", N_("Open a file"),                                  (GCallback)load_file             },
  { "OpenRecentFile", NULL,              N_("Open _Recent File"),         NULL,         NULL,                                               (GCallback)NULL },
  { "Append",    GTK_STOCK_ADD,          N_("Append _File..."),           NULL,         N_("Append data from a different file"),            (GCallback)load_file             },
  { "Acquire",   GTK_STOCK_GO_DOWN,      N_("A_cquire"),                  NULL,         NULL,                                               (GCallback)NULL },
  { "AcquireGPS",   NULL,                N_("From _GPS..."),           	  NULL,         N_("Transfer data from a GPS device"),              (GCallback)acquire_from_gps      },
  { "AcquireGPSBabel",   NULL,                N_("Import File With GPS_Babel..."),           	  NULL,         N_("Import file via GPSBabel converter"),              (GCallback)acquire_from_file      },
  { "AcquireGoogle",   NULL,             N_("Google _Directions..."),     NULL,         N_("Get driving directions from Google"),           (GCallback)acquire_from_google   },
#ifdef VIK_CONFIG_OPENSTREETMAP
  { "AcquireOSM",   NULL,                 N_("_OSM Traces..."),    	  NULL,         N_("Get traces from OpenStreetMap"),            (GCallback)acquire_from_osm       },
#endif
#ifdef VIK_CONFIG_GEOCACHES
  { "AcquireGC",   NULL,                 N_("Geo_caches..."),    	  NULL,         N_("Get Geocaches from geocaching.com"),            (GCallback)acquire_from_gc       },
#endif
#ifdef VIK_CONFIG_GEOTAG
  { "AcquireGeotag", NULL,               N_("From Geotagged _Images..."), NULL,         N_("Create waypoints from geotagged images"),       (GCallback)acquire_from_geotag   },
#endif
  { "Save",      GTK_STOCK_SAVE,         N_("_Save"),                         "<control>S", N_("Save the file"),                                (GCallback)save_file             },
  { "SaveAs",    GTK_STOCK_SAVE_AS,      N_("Save _As..."),                      NULL,  N_("Save the file under different name"),           (GCallback)save_file_as          },
  { "GenImg",    GTK_STOCK_CLEAR,        N_("_Generate Image File..."),          NULL,  N_("Save a snapshot of the workspace into a file"), (GCallback)draw_to_image_file_cb },
  { "GenImgDir", GTK_STOCK_DND_MULTIPLE, N_("Generate _Directory of Images..."), NULL,  N_("FIXME:IMGDIR"),                                 (GCallback)draw_to_image_dir_cb  },

#if GTK_CHECK_VERSION(2,10,0)
  { "Print",    GTK_STOCK_PRINT,        N_("_Print..."),          NULL,         N_("Print maps"), (GCallback)print_cb },
#endif

  { "Exit",      GTK_STOCK_QUIT,         N_("E_xit"),                         "<control>W", N_("Exit the program"),                             (GCallback)window_close          },
  { "SaveExit",  GTK_STOCK_QUIT,         N_("Save and Exit"),                 NULL, N_("Save and Exit the program"),                             (GCallback)save_file_and_exit          },

  { "GotoDefaultLocation", GTK_STOCK_HOME, N_("Go to the _Default Location"),  NULL,         N_("Go to the default location"),                     (GCallback)goto_default_location },
  { "GotoSearch", GTK_STOCK_JUMP_TO,     N_("Go to _Location..."),    	      NULL,         N_("Go to address/place using text search"),        (GCallback)goto_address       },
  { "GotoLL",    GTK_STOCK_JUMP_TO,      N_("_Go to Lat/Lon..."),           NULL,         N_("Go to arbitrary lat/lon coordinate"),         (GCallback)draw_goto_cb          },
  { "GotoUTM",   GTK_STOCK_JUMP_TO,      N_("Go to UTM..."),                  NULL,         N_("Go to arbitrary UTM coordinate"),               (GCallback)draw_goto_cb          },
  { "SetHLColor",GTK_STOCK_SELECT_COLOR, N_("Set _Highlight Color..."),       NULL,         NULL,                                           (GCallback)set_highlight_color   },
  { "SetBGColor",GTK_STOCK_SELECT_COLOR, N_("Set Bac_kground Color..."),      NULL,         NULL,                                           (GCallback)set_bg_color          },
  { "ZoomIn",    GTK_STOCK_ZOOM_IN,      N_("Zoom _In"),                   "<control>plus", NULL,                                           (GCallback)draw_zoom_cb          },
  { "ZoomOut",   GTK_STOCK_ZOOM_OUT,     N_("Zoom _Out"),                 "<control>minus", NULL,                                           (GCallback)draw_zoom_cb          },
  { "ZoomTo",    GTK_STOCK_ZOOM_FIT,     N_("Zoom _To..."),               "<control>Z", NULL,                                           (GCallback)zoom_to_cb            },
  { "Zoom0.25",  NULL,                   N_("0.25"),                          NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom0.5",   NULL,                   N_("0.5"),                           NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom1",     NULL,                   N_("1"),                             NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom2",     NULL,                   N_("2"),                             NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom4",     NULL,                   N_("4"),                             NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom8",     NULL,                   N_("8"),                             NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom16",    NULL,                   N_("16"),                            NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom32",    NULL,                   N_("32"),                            NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom64",    NULL,                   N_("64"),                            NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom128",   NULL,                   N_("128"),                           NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom256",   NULL,                   N_("256"),                           NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom512",   NULL,                   N_("512"),                           NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom1024",  NULL,                   N_("1024"),                          NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom2048",  NULL,                   N_("2048"),                          NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom4096",  NULL,                   N_("4096"),                          NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom8192",  NULL,                   N_("8192"),                          NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom16384", NULL,                   N_("16384"),                         NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "Zoom32768", NULL,                   N_("32768"),                         NULL,         NULL,                                           (GCallback)draw_zoom_cb          },
  { "PanNorth",  NULL,                   N_("Pan _North"),                "<control>Up",    NULL,                                           (GCallback)draw_pan_cb },
  { "PanEast",   NULL,                   N_("Pan _East"),                 "<control>Right", NULL,                                           (GCallback)draw_pan_cb },
  { "PanSouth",  NULL,                   N_("Pan _South"),                "<control>Down",  NULL,                                           (GCallback)draw_pan_cb },
  { "PanWest",   NULL,                   N_("Pan _West"),                 "<control>Left",  NULL,                                           (GCallback)draw_pan_cb },
  { "BGJobs",    GTK_STOCK_EXECUTE,      N_("Background _Jobs"),              NULL,         NULL,                                           (GCallback)a_background_show_window },

  { "Cut",       GTK_STOCK_CUT,          N_("Cu_t"),                          NULL,         NULL,                                           (GCallback)menu_cut_layer_cb     },
  { "Copy",      GTK_STOCK_COPY,         N_("_Copy"),                         NULL,         NULL,                                           (GCallback)menu_copy_layer_cb    },
  { "Paste",     GTK_STOCK_PASTE,        N_("_Paste"),                        NULL,         NULL,                                           (GCallback)menu_paste_layer_cb   },
  { "Delete",    GTK_STOCK_DELETE,       N_("_Delete"),                       NULL,         NULL,                                           (GCallback)menu_delete_layer_cb  },
  { "DeleteAll", NULL,                   N_("Delete All"),                    NULL,         NULL,                                           (GCallback)clear_cb              },
  { "MapCacheFlush",NULL,                N_("_Flush Map Cache"),              NULL,         NULL,                                           (GCallback)mapcache_flush_cb     },
  { "SetDefaultLocation", GTK_STOCK_GO_FORWARD, N_("_Set the Default Location"), NULL, N_("Set the Default Location to the current position"),(GCallback)default_location_cb },
  { "Preferences",GTK_STOCK_PREFERENCES, N_("_Preferences"),                  NULL,         NULL,                                           (GCallback)preferences_cb              },
  { "Properties",GTK_STOCK_PROPERTIES,   N_("_Properties"),                   NULL,         NULL,                                           (GCallback)menu_properties_cb    },

  { "HelpEntry", GTK_STOCK_HELP,         N_("_Help"),                         "F1",         NULL,                                           (GCallback)help_help_cb     },
  { "About",     GTK_STOCK_ABOUT,        N_("_About"),                        NULL,         NULL,                                           (GCallback)help_about_cb    },
};

/* Radio items */
/* FIXME use VIEWPORT_DRAWMODE values */
static GtkRadioActionEntry mode_entries[] = {
  { "ModeUTM",         NULL,         N_("_UTM Mode"),               "<control>u", NULL, 0 },
  { "ModeExpedia",     NULL,         N_("_Expedia Mode"),           "<control>e", NULL, 1 },
  { "ModeMercator",    NULL,         N_("_Mercator Mode"),            "<control>m", NULL, 4 },
  { "ModeLatLon",      NULL,         N_("Lat_/Lon Mode"),           "<control>l", NULL, 5 },
};

static GtkRadioActionEntry tool_entries[] = {
  { "Pan",      "vik-icon-pan",        N_("_Pan"),                         "<control><shift>P", N_("Pan Tool"),  0 },
  { "Zoom",      "vik-icon-zoom",        N_("_Zoom"),                         "<control><shift>Z", N_("Zoom Tool"),  1 },
  { "Ruler",     "vik-icon-ruler",       N_("_Ruler"),                        "<control><shift>R", N_("Ruler Tool"), 2 },
  { "Select",    "vik-icon-select",      N_("_Select"),                       "<control><shift>S", N_("Select Tool"), 3 }
};

static GtkToggleActionEntry toggle_entries[] = {
  { "ShowScale",      NULL,                 N_("Show _Scale"),               "F5",         N_("Show Scale"),                              (GCallback)set_draw_scale, TRUE },
  { "ShowCenterMark", NULL,                 N_("Show _Center Mark"),         "F6",         N_("Show Center Mark"),                        (GCallback)set_draw_centermark, TRUE },
  { "ShowHighlight",  GTK_STOCK_UNDERLINE,  N_("Show _Highlight"),           "F7",         N_("Show Highlight"),                          (GCallback)set_draw_highlight, TRUE },
  { "FullScreen",     GTK_STOCK_FULLSCREEN, N_("_Full Screen"),              "F11",        N_("Activate full screen mode"),               (GCallback)full_screen_cb, FALSE },
  { "ViewSidePanel",  GTK_STOCK_INDEX,      N_("Show Side _Panel"),          "F9",         N_("Show Side Panel"),                         (GCallback)view_side_panel_cb, TRUE },
  { "ViewStatusBar",  NULL,                 N_("Show Status_bar"),           "F12",        N_("Show Statusbar"),                          (GCallback)view_statusbar_cb, TRUE },
  { "ViewToolbar",    NULL,                 N_("Show _Toolbar"),             "F3",         N_("Show Toolbar"),                            (GCallback)view_toolbar_cb, TRUE },
  { "ViewMainMenu",   NULL,                 N_("Show _Menu"),                "F4",         N_("Show Menu"),                               (GCallback)view_main_menu_cb, TRUE },
};

#include "menu.xml.h"
static void window_create_ui( VikWindow *window )
{
  GtkUIManager *uim;
  GtkActionGroup *action_group;
  GtkAccelGroup *accel_group;
  GError *error;
  guint i, j, mid;
  GtkIconFactory *icon_factory;
  GtkIconSet *icon_set; 
  GtkRadioActionEntry *tools = NULL, *radio;
  guint ntools;
  
  uim = gtk_ui_manager_new ();
  window->uim = uim;

  toolbox_add_tool(window->vt, &ruler_tool, TOOL_LAYER_TYPE_NONE);
  toolbox_add_tool(window->vt, &zoom_tool, TOOL_LAYER_TYPE_NONE);
  toolbox_add_tool(window->vt, &pan_tool, TOOL_LAYER_TYPE_NONE);
  toolbox_add_tool(window->vt, &select_tool, TOOL_LAYER_TYPE_NONE);

  error = NULL;
  if (!(mid = gtk_ui_manager_add_ui_from_string (uim, menu_xml, -1, &error))) {
    g_error_free (error);
    exit (1);
  }

  action_group = gtk_action_group_new ("MenuActions");
  gtk_action_group_set_translation_domain(action_group, PACKAGE_NAME);
  gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), window);
  gtk_action_group_add_toggle_actions (action_group, toggle_entries, G_N_ELEMENTS (toggle_entries), window);
  gtk_action_group_add_radio_actions (action_group, mode_entries, G_N_ELEMENTS (mode_entries), 4, (GCallback)window_change_coord_mode_cb, window);

  icon_factory = gtk_icon_factory_new ();
  gtk_icon_factory_add_default (icon_factory); 

  register_vik_icons(icon_factory);

  ntools = 0;
  for (i=0; i<G_N_ELEMENTS(tool_entries); i++) {
      tools = g_renew(GtkRadioActionEntry, tools, ntools+1);
      radio = &tools[ntools];
      ntools++;
      *radio = tool_entries[i];
      radio->value = ntools;
  }  

  for (i=0; i<VIK_LAYER_NUM_TYPES; i++) {
    GtkActionEntry action;
    gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Layers/", 
			  vik_layer_get_interface(i)->name,
			  vik_layer_get_interface(i)->name,
			  GTK_UI_MANAGER_MENUITEM, FALSE);

    icon_set = gtk_icon_set_new_from_pixbuf (gdk_pixbuf_from_pixdata (vik_layer_get_interface(i)->icon, FALSE, NULL ));
    gtk_icon_factory_add (icon_factory, vik_layer_get_interface(i)->name, icon_set);
    gtk_icon_set_unref (icon_set);

    action.name = vik_layer_get_interface(i)->name;
    action.stock_id = vik_layer_get_interface(i)->name;
    action.label = g_strdup_printf( _("New %s Layer"), vik_layer_get_interface(i)->name);
    action.accelerator = NULL;
    action.tooltip = NULL;
    action.callback = (GCallback)menu_addlayer_cb;
    gtk_action_group_add_actions(action_group, &action, 1, window);

    if ( vik_layer_get_interface(i)->tools_count ) {
      gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools/", vik_layer_get_interface(i)->name, NULL, GTK_UI_MANAGER_SEPARATOR, FALSE);
      gtk_ui_manager_add_ui(uim, mid,  "/ui/MainToolbar/ToolItems/", vik_layer_get_interface(i)->name, NULL, GTK_UI_MANAGER_SEPARATOR, FALSE);
    }

    for ( j = 0; j < vik_layer_get_interface(i)->tools_count; j++ ) {
      tools = g_renew(GtkRadioActionEntry, tools, ntools+1);
      radio = &tools[ntools];
      ntools++;
      
      gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools", 
			    _(vik_layer_get_interface(i)->tools[j].name),
			    vik_layer_get_interface(i)->tools[j].name,
			    GTK_UI_MANAGER_MENUITEM, FALSE);
      gtk_ui_manager_add_ui(uim, mid,  "/ui/MainToolbar/ToolItems", 
			    _(vik_layer_get_interface(i)->tools[j].name),
			    vik_layer_get_interface(i)->tools[j].name,
			    GTK_UI_MANAGER_TOOLITEM, FALSE);

      toolbox_add_tool(window->vt, &(vik_layer_get_interface(i)->tools[j]), i);

      radio->name = vik_layer_get_interface(i)->tools[j].name;
      radio->stock_id = vik_layer_get_interface(i)->tools[j].name,
      radio->label = _(vik_layer_get_interface(i)->tools[j].name);
      radio->accelerator = NULL;
      radio->tooltip = _(vik_layer_get_interface(i)->tools[j].name);
      radio->value = ntools;
    }
  }
  g_object_unref (icon_factory);

  gtk_action_group_add_radio_actions(action_group, tools, ntools, 0, (GCallback)menu_tool_cb, window);
  g_free(tools);

  gtk_ui_manager_insert_action_group (uim, action_group, 0);

  for (i=0; i<VIK_LAYER_NUM_TYPES; i++) {
    for ( j = 0; j < vik_layer_get_interface(i)->tools_count; j++ ) {
      GtkAction *action = gtk_action_group_get_action(action_group,
			    vik_layer_get_interface(i)->tools[j].name);
      g_object_set(action, "sensitive", FALSE, NULL);
    }
  }
  window->action_group = action_group;

  accel_group = gtk_ui_manager_get_accel_group (uim);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  gtk_ui_manager_ensure_update (uim);
  
  setup_recent_files(window);
}



static struct { 
  const GdkPixdata *data;
  gchar *stock_id;
} stock_icons[] = {
  { &begintr_18_pixbuf,		"Begin Track"      },
  { &route_finder_18_pixbuf,	"Route Finder"     },
  { &mover_22_pixbuf,		"vik-icon-pan"     },
  { &demdl_18_pixbuf,		"DEM Download/Import"     },
  { &showpic_18_pixbuf,		"Show Picture"      },
  { &addtr_18_pixbuf,		"Create Track"      },
  { &edtr_18_pixbuf,		"Edit Trackpoint"   },
  { &addwp_18_pixbuf,		"Create Waypoint"   },
  { &edwp_18_pixbuf,		"Edit Waypoint"     },
  { &zoom_18_pixbuf,		"vik-icon-zoom"     },
  { &ruler_18_pixbuf,		"vik-icon-ruler"    },
  { &select_18_pixbuf,		"vik-icon-select"   },
  { &geozoom_18_pixbuf,		"Georef Zoom Tool"  },
  { &geomove_18_pixbuf,		"Georef Move Map"   },
  { &mapdl_18_pixbuf,		"Maps Download"     },
};
 
static gint n_stock_icons = G_N_ELEMENTS (stock_icons);

static void
register_vik_icons (GtkIconFactory *icon_factory)
{
  GtkIconSet *icon_set; 
  gint i;

  for (i = 0; i < n_stock_icons; i++) {
    icon_set = gtk_icon_set_new_from_pixbuf (gdk_pixbuf_from_pixdata (
                   stock_icons[i].data, FALSE, NULL ));
    gtk_icon_factory_add (icon_factory, stock_icons[i].stock_id, icon_set);
    gtk_icon_set_unref (icon_set);
  }
}

gpointer vik_window_get_selected_trw_layer ( VikWindow *vw )
{
  return vw->selected_vtl;
}

void vik_window_set_selected_trw_layer ( VikWindow *vw, gpointer vtl )
{
  vw->selected_vtl   = vtl;
  vw->containing_vtl = vtl;
  /* Clear others */
  vw->selected_track     = NULL;
  vw->selected_tracks    = NULL;
  vw->selected_waypoint  = NULL;
  vw->selected_waypoints = NULL;
  vw->selected_name      = NULL;
  // Set highlight thickness
  vik_viewport_set_highlight_thickness ( vw->viking_vvp, vik_trw_layer_get_property_tracks_line_thickness (vw->containing_vtl) );
}

gpointer vik_window_get_selected_tracks ( VikWindow *vw )
{
  return vw->selected_tracks;
}

void vik_window_set_selected_tracks ( VikWindow *vw, gpointer gl, gpointer vtl )
{
  vw->selected_tracks = gl;
  vw->containing_vtl  = vtl;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_track     = NULL;
  vw->selected_waypoint  = NULL;
  vw->selected_waypoints = NULL;
  vw->selected_name      = NULL;
  // Set highlight thickness
  vik_viewport_set_highlight_thickness ( vw->viking_vvp, vik_trw_layer_get_property_tracks_line_thickness (vw->containing_vtl) );
}

gpointer vik_window_get_selected_track ( VikWindow *vw )
{
  return vw->selected_track;
}

void vik_window_set_selected_track ( VikWindow *vw, gpointer *vt, gpointer vtl, gpointer name )
{
  vw->selected_track = vt;
  vw->containing_vtl = vtl;
  vw->selected_name  = name;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_tracks    = NULL;
  vw->selected_waypoint  = NULL;
  vw->selected_waypoints = NULL;
  // Set highlight thickness
  vik_viewport_set_highlight_thickness ( vw->viking_vvp, vik_trw_layer_get_property_tracks_line_thickness (vw->containing_vtl) );
}

gpointer vik_window_get_selected_waypoints ( VikWindow *vw )
{
  return vw->selected_waypoints;
}

void vik_window_set_selected_waypoints ( VikWindow *vw, gpointer gl, gpointer vtl )
{
  vw->selected_waypoints = gl;
  vw->containing_vtl     = vtl;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_track     = NULL;
  vw->selected_tracks    = NULL;
  vw->selected_waypoint  = NULL;
  vw->selected_name      = NULL;
}

gpointer vik_window_get_selected_waypoint ( VikWindow *vw )
{
  return vw->selected_waypoint;
}

void vik_window_set_selected_waypoint ( VikWindow *vw, gpointer *vwp, gpointer vtl, gpointer name )
{
  vw->selected_waypoint = vwp;
  vw->containing_vtl    = vtl;
  vw->selected_name     = name;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_track     = NULL;
  vw->selected_tracks    = NULL;
  vw->selected_waypoints = NULL;
}

gpointer vik_window_get_selected_name ( VikWindow *vw )
{
  return vw->selected_name;
}

gboolean vik_window_clear_highlight ( VikWindow *vw )
{
  gboolean need_redraw = FALSE;
  if ( vw->selected_vtl != NULL ) {
    vw->selected_vtl = NULL;
    need_redraw = TRUE;
  }
  if ( vw->selected_track != NULL ) {
    vw->selected_track = NULL;
    need_redraw = TRUE;
  }
  if ( vw->selected_tracks != NULL ) {
    vw->selected_tracks = NULL;
    need_redraw = TRUE;
  }
  if ( vw->selected_waypoint != NULL ) {
    vw->selected_waypoint = NULL;
    need_redraw = TRUE;
  }
  if ( vw->selected_waypoints != NULL ) {
    vw->selected_waypoints = NULL;
    need_redraw = TRUE;
  }
  return need_redraw;
}
