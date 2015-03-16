/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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
#include "geojson.h"
#include "vikgoto.h"
#include "dems.h"
#include "mapcache.h"
#include "print.h"
#include "preferences.h"
#include "toolbar.h"
#include "viklayer_defaults.h"
#include "icons/icons.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "garminsymbols.h"
#include "vikmapslayer.h"
#include "geonamessearch.h"
#include "vikutils.h"
#include "dir.h"

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

// This seems rather arbitary, quite large and pointless
//  I mean, if you have a thousand windows open;
//   why not be allowed to open a thousand more...
#define MAX_WINDOWS 1024
static guint window_count = 0;
static GSList *window_list = NULL;

#define VIKING_WINDOW_WIDTH      1000
#define VIKING_WINDOW_HEIGHT     800
#define DRAW_IMAGE_DEFAULT_WIDTH 1280
#define DRAW_IMAGE_DEFAULT_HEIGHT 1024
#define DRAW_IMAGE_DEFAULT_SAVE_AS_PNG TRUE

static void window_finalize ( GObject *gob );
static GObjectClass *parent_class;

static void window_set_filename ( VikWindow *vw, const gchar *filename );
static const gchar *window_get_filename ( VikWindow *vw );

static VikWindow *window_new ();

static void draw_update ( VikWindow *vw );

static void newwindow_cb ( GtkAction *a, VikWindow *vw );

// Signals
static void open_window ( VikWindow *vw, GSList *files );
static void destroy_window ( GtkWidget *widget,
                             gpointer   data );

/* Drawing & stuff */

static gboolean delete_event( VikWindow *vw );

static gboolean key_press_event( VikWindow *vw, GdkEventKey *event, gpointer data );

static void center_changed_cb ( VikWindow *vw );
static void window_configure_event ( VikWindow *vw );
static void draw_sync ( VikWindow *vw );
static void draw_redraw ( VikWindow *vw );
static void draw_scroll  ( VikWindow *vw, GdkEventScroll *event );
static void draw_click  ( VikWindow *vw, GdkEventButton *event );
static void draw_release ( VikWindow *vw, GdkEventButton *event );
static void draw_mouse_motion ( VikWindow *vw, GdkEventMotion *event );
static void draw_zoom_cb ( GtkAction *a, VikWindow *vw );
static void draw_goto_cb ( GtkAction *a, VikWindow *vw );
static void draw_refresh_cb ( GtkAction *a, VikWindow *vw );

static void draw_status ( VikWindow *vw );

/* End Drawing Functions */

static void toggle_draw_scale ( GtkAction *a, VikWindow *vw );
static void toggle_draw_centermark ( GtkAction *a, VikWindow *vw );
static void toggle_draw_highlight ( GtkAction *a, VikWindow *vw );

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

static void menu_cb ( GtkAction *old, GtkAction *a, VikWindow *vw );
static void window_change_coord_mode_cb ( GtkAction *old, GtkAction *a, VikWindow *vw );
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
  GtkWidget *hpaned;
  VikViewport *viking_vvp;
  VikLayersPanel *viking_vlp;
  VikStatusbar *viking_vs;
  VikToolbar *viking_vtb;

  GtkWidget *main_vbox;
  GtkWidget *menu_hbox;

  GdkCursor *busy_cursor;
  GdkCursor *viewport_cursor; // only a reference

  /* tool management state */
  guint current_tool;
  toolbox_tools_t *vt;
  guint16 tool_layer_id;
  guint16 tool_tool_id;

  GtkActionGroup *action_group;

  // Display controls
  // NB scale, centermark and highlight are in viewport.
  gboolean show_full_screen;
  gboolean show_side_panel;
  gboolean show_statusbar;
  gboolean show_toolbar;
  gboolean show_main_menu;

  gboolean select_move;
  gboolean pan_move;
  gint pan_x, pan_y;
  gint delayed_pan_x, delayed_pan_y; // Temporary storage
  gboolean single_click_pending;

  guint draw_image_width, draw_image_height;
  gboolean draw_image_save_as_png;

  gchar *filename;
  gboolean modified;
  VikLoadType_t loaded_type;

  GtkWidget *open_dia, *save_dia;
  GtkWidget *save_img_dia, *save_img_dir_dia;

  gboolean only_updating_coord_mode_ui; /* hack for a bug in GTK */
  GtkUIManager *uim;

  GThread  *thread;
  /* half-drawn update */
  VikLayer *trigger;
  VikCoord trigger_center;

  /* Store at this level for highlighted selection drawing since it applies to the viewport and the layers panel */
  /* Only one of these items can be selected at the same time */
  gpointer selected_vtl; /* notionally VikTrwLayer */
  GHashTable *selected_tracks;
  gpointer selected_track; /* notionally VikTrack */
  GHashTable *selected_waypoints;
  gpointer selected_waypoint; /* notionally VikWaypoint */
  /* only use for individual track or waypoint */
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

// TODO get rid of this as this is unnecessary duplication...
static gchar *tool_names[NUMBER_OF_TOOLS] = { N_("Pan"), N_("Zoom"), N_("Ruler"), N_("Select") };

G_DEFINE_TYPE (VikWindow, vik_window, GTK_TYPE_WINDOW)

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

/**
 *  Returns the 'project' filename
 */
const gchar *vik_window_get_filename (VikWindow *vw)
{
  return vw->filename;
}

typedef struct {
  VikStatusbar *vs;
  vik_statusbar_type_t vs_type;
  gchar* message; // Always make a copy of this data
} statusbar_idle_data;

/**
 * For the actual statusbar update!
 */
static gboolean statusbar_idle_update ( statusbar_idle_data *sid )
{
  vik_statusbar_set_message ( sid->vs, sid->vs_type, sid->message );
  g_free ( sid->message );
  g_free ( sid );
  return FALSE;
}

/**
 * vik_window_statusbar_update:
 * @vw:      The main window in which the statusbar will be updated.
 * @message: The string to be displayed. This is copied.
 * @vs_type: The part of the statusbar to be updated.
 *
 * This updates any part of the statusbar with the new string.
 * It handles calling from the main thread or any background thread
 * ATM this mostly used from background threads - as from the main thread
 *  one may use the vik_statusbar_set_message() directly.
 */
void vik_window_statusbar_update ( VikWindow *vw, const gchar* message, vik_statusbar_type_t vs_type )
{
  GThread *thread = vik_window_get_thread ( vw );
  if ( !thread )
    // Do nothing
    return;

  statusbar_idle_data *sid = g_malloc ( sizeof (statusbar_idle_data) );
  sid->vs = vw->viking_vs;
  sid->vs_type = vs_type;
  sid->message = g_strdup ( message );

  if ( g_thread_self() == thread ) {
    g_idle_add ( (GSourceFunc) statusbar_idle_update, sid );
  }
  else {
    // From a background thread
    gdk_threads_add_idle ( (GSourceFunc) statusbar_idle_update, sid );
  }
}

// Actual signal handlers
static void destroy_window ( GtkWidget *widget,
                             gpointer   data )
{
    if ( ! --window_count )
      gtk_main_quit ();
}

#define VIK_SETTINGS_WIN_SIDEPANEL "window_sidepanel"
#define VIK_SETTINGS_WIN_STATUSBAR "window_statusbar"
#define VIK_SETTINGS_WIN_TOOLBAR "window_toolbar"
// Menubar setting to off is never auto saved in case it's accidentally turned off
// It's not so obvious so to recover the menu visibility.
// Thus this value is for setting manually via editting the settings file directly
#define VIK_SETTINGS_WIN_MENUBAR "window_menubar"

VikWindow *vik_window_new_window ()
{
  if ( window_count < MAX_WINDOWS )
  {
    VikWindow *vw = window_new ();

    g_signal_connect (G_OBJECT (vw), "destroy",
		      G_CALLBACK (destroy_window), NULL);
    g_signal_connect (G_OBJECT (vw), "newwindow",
		      G_CALLBACK (vik_window_new_window), NULL);
    g_signal_connect (G_OBJECT (vw), "openwindow",
		      G_CALLBACK (open_window), NULL);

    gtk_widget_show_all ( GTK_WIDGET(vw) );

    if ( a_vik_get_restore_window_state() ) {
      // These settings are applied after the show all as these options hide widgets
      gboolean sidepanel;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL, &sidepanel ) )
        if ( ! sidepanel ) {
          gtk_widget_hide ( GTK_WIDGET(vw->viking_vlp) );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanel" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean statusbar;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_STATUSBAR, &statusbar ) )
        if ( ! statusbar ) {
          gtk_widget_hide ( GTK_WIDGET(vw->viking_vs) );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewStatusBar" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean toolbar;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_TOOLBAR, &toolbar ) )
        if ( ! toolbar ) {
          gtk_widget_hide ( toolbar_get_widget (vw->viking_vtb) );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewToolBar" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean menubar;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_MENUBAR, &menubar ) )
        if ( ! menubar ) {
          gtk_widget_hide ( gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu" ) );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewMainMenu" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }
    }
    window_count++;

    return vw;
  }
  return NULL;
}

/**
 * determine_location_thread:
 * @vw:         The window that will get updated
 * @threaddata: Data used by our background thread mechanism
 *
 * Use the features in vikgoto to determine where we are
 * Then set up the viewport:
 *  1. To goto the location
 *  2. Set an appropriate level zoom for the location type
 *  3. Some statusbar message feedback
 */
static int determine_location_thread ( VikWindow *vw, gpointer threaddata )
{
  struct LatLon ll;
  gchar *name = NULL;
  gint ans = a_vik_goto_where_am_i ( vw->viking_vvp, &ll, &name );

  int result = a_background_thread_progress ( threaddata, 1.0 );
  if ( result != 0 ) {
    vik_window_statusbar_update ( vw, _("Location lookup aborted"), VIK_STATUSBAR_INFO );
    return -1; /* Abort thread */
  }

  if ( ans ) {
    // Zoom out a little
    gdouble zoom = 16.0;

    if ( ans == 2 ) {
      // Position found with city precision - so zoom out more
      zoom = 128.0;
    }
    else if ( ans == 3 ) {
      // Position found via country name search - so zoom wayyyy out
      zoom = 2048.0;
    }

    vik_viewport_set_zoom ( vw->viking_vvp, zoom );
    vik_viewport_set_center_latlon ( vw->viking_vvp, &ll, FALSE );

    gchar *message = g_strdup_printf ( _("Location found: %s"), name );
    vik_window_statusbar_update ( vw, message, VIK_STATUSBAR_INFO );
    g_free ( name );
    g_free ( message );

    // Signal to redraw from the background
    vik_layers_panel_emit_update ( vw->viking_vlp );
  }
  else
    vik_window_statusbar_update ( vw, _("Unable to determine location"), VIK_STATUSBAR_INFO );

  return 0;
}

/**
 * Steps to be taken once initial loading has completed
 */
void vik_window_new_window_finish ( VikWindow *vw )
{
  // Don't add a map if we've loaded a Viking file already
  if ( vw->filename )
    return;

  if ( a_vik_get_startup_method ( ) == VIK_STARTUP_METHOD_SPECIFIED_FILE ) {
    vik_window_open_file ( vw, a_vik_get_startup_file(), TRUE );
    if ( vw->filename )
      return;
  }

  // Maybe add a default map layer
  if ( a_vik_get_add_default_map_layer () ) {
    VikMapsLayer *vml = VIK_MAPS_LAYER ( vik_layer_create(VIK_LAYER_MAPS, vw->viking_vvp, FALSE) );
    vik_layer_rename ( VIK_LAYER(vml), _("Default Map") );
    vik_aggregate_layer_add_layer ( vik_layers_panel_get_top_layer(vw->viking_vlp), VIK_LAYER(vml), TRUE );

    draw_update ( vw );
  }

  // If not loaded any file, maybe try the location lookup
  if ( vw->loaded_type == LOAD_TYPE_READ_FAILURE ) {
    if ( a_vik_get_startup_method ( ) == VIK_STARTUP_METHOD_AUTO_LOCATION ) {

      vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, _("Trying to determine location...") );

      a_background_thread ( BACKGROUND_POOL_REMOTE,
                            GTK_WINDOW(vw),
                            _("Determining location"),
                            (vik_thr_func) determine_location_thread,
                            vw,
                            NULL,
                            NULL,
                            1 );
    }
  }
}

static void open_window ( VikWindow *vw, GSList *files )
{
  gboolean change_fn = (g_slist_length(files) == 1); /* only change fn if one file */
  GSList *cur_file = files;
  while ( cur_file ) {
    // Only open a new window if a viking file
    gchar *file_name = cur_file->data;
    if (vw != NULL && vw->filename && check_file_magic_vik ( file_name ) ) {
      VikWindow *newvw = vik_window_new_window ();
      if (newvw)
        vik_window_open_file ( newvw, file_name, TRUE );
    }
    else {
      vik_window_open_file ( vw, file_name, change_fn );
    }
    g_free (file_name);
    cur_file = g_slist_next (cur_file);
  }
  g_slist_free (files);
}
// End signals

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
                                           layer_interface->tools[j].radioActionEntry.name);
      g_object_set(action, "sensitive", i == vl->type, NULL);
      toolbar_action_set_sensitive ( vw->viking_vtb, vik_layer_get_interface(i)->tools[j].radioActionEntry.name, i == vl->type );
    }
  }
}

static void window_finalize ( GObject *gob )
{
  VikWindow *vw = VIK_WINDOW(gob);
  g_return_if_fail ( vw != NULL );

  a_background_remove_window ( vw );

  window_list = g_slist_remove ( window_list, vw );

  gdk_cursor_unref ( vw->busy_cursor );
  int tt;
  for (tt = 0; tt < vw->vt->n_tools; tt++ )
    if ( vw->vt->tools[tt].ti.destroy )
      vw->vt->tools[tt].ti.destroy ( vw->vt->tools[tt].state );
  g_free ( vw->vt->tools );
  g_free ( vw->vt );

  vik_toolbar_finalize ( vw->viking_vtb );

  G_OBJECT_CLASS(parent_class)->finalize(gob);
}


static void vik_window_class_init ( VikWindowClass *klass )
{
  /* destructor */
  GObjectClass *object_class;

  window_signals[VW_NEWWINDOW_SIGNAL] = g_signal_new ( "newwindow", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikWindowClass, newwindow), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  window_signals[VW_OPENWINDOW_SIGNAL] = g_signal_new ( "openwindow", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikWindowClass, openwindow), NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = window_finalize;

  parent_class = g_type_class_peek_parent (klass);

}

static void zoom_changed (GtkMenuShell *menushell,
              gpointer      user_data)
{
  VikWindow *vw = VIK_WINDOW (user_data);

  GtkWidget *aw = gtk_menu_get_active ( GTK_MENU (menushell) );
  gint active = GPOINTER_TO_INT(g_object_get_data ( G_OBJECT (aw), "position" ));

  gdouble zoom_request = pow (2, active-5 );

  // But has it really changed?
  gdouble current_zoom = vik_viewport_get_zoom ( vw->viking_vvp );
  if ( current_zoom != 0.0 && zoom_request != current_zoom ) {
    vik_viewport_set_zoom ( vw->viking_vvp, zoom_request );
    // Force drawing update
    draw_update ( vw );
  }
}

/**
 * @mpp: The initial zoom level
 */
static GtkWidget *create_zoom_menu_all_levels ( gdouble mpp )
{
  GtkWidget *menu = gtk_menu_new ();
  char *itemLabels[] = { "0.031", "0.063", "0.125", "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384", "32768" };

  int i;
  for (i = 0 ; i < G_N_ELEMENTS(itemLabels) ; i++)
    {
      GtkWidget *item = gtk_menu_item_new_with_label (itemLabels[i]);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
      g_object_set_data (G_OBJECT (item), "position", GINT_TO_POINTER(i));
    }

  gint active = 5 + round ( log (mpp) / log (2) );
  // Ensure value derived from mpp is in bounds of the menu
  if ( active >= G_N_ELEMENTS(itemLabels) )
    active = G_N_ELEMENTS(itemLabels) - 1;
  if ( active < 0 )
    active = 0;
  gtk_menu_set_active ( GTK_MENU(menu), active );

  return menu;
}

static GtkWidget *create_zoom_combo_all_levels ()
{
  GtkWidget *combo = vik_combo_box_text_new();
  vik_combo_box_text_append ( combo, "0.25");
  vik_combo_box_text_append ( combo, "0.5");
  vik_combo_box_text_append ( combo, "1");
  vik_combo_box_text_append ( combo, "2");
  vik_combo_box_text_append ( combo, "4");
  vik_combo_box_text_append ( combo, "8");
  vik_combo_box_text_append ( combo, "16");
  vik_combo_box_text_append ( combo, "32");
  vik_combo_box_text_append ( combo, "64");
  vik_combo_box_text_append ( combo, "128");
  vik_combo_box_text_append ( combo, "256");
  vik_combo_box_text_append ( combo, "512");
  vik_combo_box_text_append ( combo, "1024");
  vik_combo_box_text_append ( combo, "2048");
  vik_combo_box_text_append ( combo, "4096");
  vik_combo_box_text_append ( combo, "8192");
  vik_combo_box_text_append ( combo, "16384");
  vik_combo_box_text_append ( combo, "32768");
  /* Create tooltip */
  gtk_widget_set_tooltip_text (combo, _("Select zoom level"));
  return combo;
}

static gint zoom_popup_handler (GtkWidget *widget)
{
  GtkMenu *menu;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);

  /* The "widget" is the menu that was supplied when
   * g_signal_connect_swapped() was called.
   */
  menu = GTK_MENU (widget);

  gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
                  1, gtk_get_current_event_time());
  return TRUE;
}

enum {
  TARGET_URIS,
};

static void drag_data_received_cb ( GtkWidget *widget,
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *selection_data,
                                    guint target_type,
                                    guint time,
                                    gpointer data )
{
  gboolean success = FALSE;

  if ( (selection_data != NULL) && (gtk_selection_data_get_length(selection_data) > 0) ) {
    switch (target_type) {
    case TARGET_URIS: {
      gchar *str = (gchar*)gtk_selection_data_get_data(selection_data);
      g_debug ("drag received string:%s \n", str);

      // Convert string into GSList of individual entries for use with our open signal
      gchar **entries = g_strsplit(str, "\r\n", 0);
      GSList *filenames = NULL;
      gint entry_runner = 0;
      gchar *entry = entries[entry_runner];
      while (entry) {
        if ( g_strcmp0 ( entry, "" ) ) {
          // Drag+Drop gives URIs. And so in particular, %20 in place of spaces in filenames
          //  thus need to convert the text into a plain string
          gchar *filename = g_filename_from_uri ( entry, NULL, NULL );
          if ( filename )
            filenames = g_slist_append ( filenames, filename );
        }
        entry_runner++;
        entry = entries[entry_runner];
      }

      if ( filenames )
        g_signal_emit ( G_OBJECT(VIK_WINDOW_FROM_WIDGET(widget)), window_signals[VW_OPENWINDOW_SIGNAL], 0, filenames );
        // NB: GSList & contents are freed by main.open_window

      success = TRUE;
      break;
    }
    default: break;
    }
  }

  gtk_drag_finish ( context, success, FALSE, time );
}

static void toolbar_tool_cb ( GtkAction *old, GtkAction *current, gpointer gp )
{
  VikWindow *vw = (VikWindow*)gp;
  GtkAction *action = gtk_action_group_get_action ( vw->action_group, gtk_action_get_name(current) );
  if ( action )
    gtk_action_activate ( action );
}

static void toolbar_reload_cb ( GtkActionGroup *grp, gpointer gp )
{
  VikWindow *vw = (VikWindow*)gp;
  center_changed_cb ( vw );
}

#define VIK_SETTINGS_WIN_MAX "window_maximized"
#define VIK_SETTINGS_WIN_FULLSCREEN "window_fullscreen"
#define VIK_SETTINGS_WIN_WIDTH "window_width"
#define VIK_SETTINGS_WIN_HEIGHT "window_height"
#define VIK_SETTINGS_WIN_PANE_POSITION "window_horizontal_pane_position"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH "window_save_image_width"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT "window_save_image_height"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_PNG "window_save_image_as_png"
#define VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT "window_copy_centre_full_format"

#define VIKING_ACCELERATOR_KEY_FILE "keys.rc"

static void vik_window_init ( VikWindow *vw )
{
  vw->action_group = NULL;

  vw->viking_vvp = vik_viewport_new();
  vw->viking_vlp = vik_layers_panel_new();
  vik_layers_panel_set_viewport ( vw->viking_vlp, vw->viking_vvp );
  vw->viking_vs = vik_statusbar_new();

  vw->vt = toolbox_create(vw);
  vw->viking_vtb = vik_toolbar_new ();
  window_create_ui(vw);
  window_set_filename (vw, NULL);

  vw->busy_cursor = gdk_cursor_new ( GDK_WATCH );

  vw->filename = NULL;
  vw->loaded_type = LOAD_TYPE_READ_FAILURE; //AKA none
  vw->modified = FALSE;
  vw->only_updating_coord_mode_ui = FALSE;

  vw->select_move = FALSE;
  vw->pan_move = FALSE; 
  vw->pan_x = vw->pan_y = -1;
  vw->single_click_pending = FALSE;

  gint draw_image_width;
  if ( a_settings_get_integer ( VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH, &draw_image_width ) )
    vw->draw_image_width = draw_image_width;
  else
    vw->draw_image_width = DRAW_IMAGE_DEFAULT_WIDTH;
  gint draw_image_height;
  if ( a_settings_get_integer ( VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT, &draw_image_height ) )
    vw->draw_image_height = draw_image_height;
  else
    vw->draw_image_height = DRAW_IMAGE_DEFAULT_HEIGHT;
  gboolean draw_image_save_as_png;
  if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, &draw_image_save_as_png ) )
    vw->draw_image_save_as_png = draw_image_save_as_png;
  else
    vw->draw_image_save_as_png = DRAW_IMAGE_DEFAULT_SAVE_AS_PNG;

  vw->main_vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add (GTK_CONTAINER (vw), vw->main_vbox);
  vw->menu_hbox = gtk_hbox_new(FALSE, 1);
  GtkWidget *menu_bar = gtk_ui_manager_get_widget (vw->uim, "/MainMenu");
  gtk_box_pack_start (GTK_BOX(vw->menu_hbox), menu_bar, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX(vw->main_vbox), vw->menu_hbox, FALSE, TRUE, 0);

  toolbar_init(vw->viking_vtb,
               &vw->gtkwindow,
               vw->main_vbox,
               vw->menu_hbox,
               toolbar_tool_cb,
               toolbar_reload_cb,
               (gpointer)vw); // This auto packs toolbar into the vbox
  // Must be performed post toolbar init
  gint i,j;
  for (i=0; i<VIK_LAYER_NUM_TYPES; i++) {
    for ( j = 0; j < vik_layer_get_interface(i)->tools_count; j++ ) {
      toolbar_action_set_sensitive ( vw->viking_vtb, vik_layer_get_interface(i)->tools[j].radioActionEntry.name, FALSE );
    }
  }

  vik_ext_tool_datasources_add_menu_items ( vw, vw->uim );

  GtkWidget * zoom_levels = gtk_ui_manager_get_widget (vw->uim, "/MainMenu/View/SetZoom");
  GtkWidget * zoom_levels_menu = create_zoom_menu_all_levels ( vik_viewport_get_zoom(vw->viking_vvp) );
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (zoom_levels), zoom_levels_menu);
  g_signal_connect ( G_OBJECT(zoom_levels_menu), "selection-done", G_CALLBACK(zoom_changed), vw);
  g_signal_connect_swapped ( G_OBJECT(vw->viking_vs), "clicked", G_CALLBACK(zoom_popup_handler), zoom_levels_menu );

  g_signal_connect (G_OBJECT (vw), "delete_event", G_CALLBACK (delete_event), NULL);

  // Own signals
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "updated_center", G_CALLBACK(center_changed_cb), vw);
  // Signals from GTK
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "expose_event", G_CALLBACK(draw_sync), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "configure_event", G_CALLBACK(window_configure_event), vw);
  gtk_widget_add_events ( GTK_WIDGET(vw->viking_vvp), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK );
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "scroll_event", G_CALLBACK(draw_scroll), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_press_event", G_CALLBACK(draw_click), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_release_event", G_CALLBACK(draw_release), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "motion_notify_event", G_CALLBACK(draw_mouse_motion), vw);

  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "update", G_CALLBACK(draw_update), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "delete_layer", G_CALLBACK(vik_window_clear_highlight), vw);

  // Allow key presses to be processed anywhere
  g_signal_connect_swapped (G_OBJECT (vw), "key_press_event", G_CALLBACK (key_press_event), vw);

  // Set initial button sensitivity
  center_changed_cb ( vw );

  vw->hpaned = gtk_hpaned_new ();
  gtk_paned_pack1 ( GTK_PANED(vw->hpaned), GTK_WIDGET (vw->viking_vlp), FALSE, TRUE );
  gtk_paned_pack2 ( GTK_PANED(vw->hpaned), GTK_WIDGET (vw->viking_vvp), TRUE, TRUE );

  /* This packs the button into the window (a gtk container). */
  gtk_box_pack_start (GTK_BOX(vw->main_vbox), vw->hpaned, TRUE, TRUE, 0);

  gtk_box_pack_end (GTK_BOX(vw->main_vbox), GTK_WIDGET(vw->viking_vs), FALSE, TRUE, 0);

  a_background_add_window ( vw );

  window_list = g_slist_prepend ( window_list, vw);

  gint height = VIKING_WINDOW_HEIGHT;
  gint width = VIKING_WINDOW_WIDTH;

  if ( a_vik_get_restore_window_state() ) {
    if ( a_settings_get_integer ( VIK_SETTINGS_WIN_HEIGHT, &height ) ) {
      // Enforce a basic minimum size
      if ( height < 160 )
        height = 160;
    }
    else
      // No setting - so use default
      height = VIKING_WINDOW_HEIGHT;

    if ( a_settings_get_integer ( VIK_SETTINGS_WIN_WIDTH, &width ) ) {
      // Enforce a basic minimum size
      if ( width < 320 )
        width = 320;
    }
    else
      // No setting - so use default
      width = VIKING_WINDOW_WIDTH;

    gboolean maxed;
    if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_MAX, &maxed ) )
      if ( maxed )
	gtk_window_maximize ( GTK_WINDOW(vw) );

    gboolean full;
    if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_FULLSCREEN, &full ) ) {
      if ( full ) {
        vw->show_full_screen = TRUE;
        gtk_window_fullscreen ( GTK_WINDOW(vw) );
        GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/FullScreen" );
        if ( check_box )
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), TRUE );
      }
    }

    gint position = -1; // Let GTK determine default positioning
    if ( !a_settings_get_integer ( VIK_SETTINGS_WIN_PANE_POSITION, &position ) ) {
      position = -1;
    }
    gtk_paned_set_position ( GTK_PANED(vw->hpaned), position );
  }

  gtk_window_set_default_size ( GTK_WINDOW(vw), width, height );

  vw->open_dia = NULL;
  vw->save_dia = NULL;
  vw->save_img_dia = NULL;
  vw->save_img_dir_dia = NULL;

  vw->show_side_panel = TRUE;
  vw->show_statusbar = TRUE;
  vw->show_toolbar = TRUE;
  vw->show_main_menu = TRUE;

  // Only accept Drag and Drop of files onto the viewport
  gtk_drag_dest_set ( GTK_WIDGET(vw->viking_vvp), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
  gtk_drag_dest_add_uri_targets ( GTK_WIDGET(vw->viking_vvp) );
  g_signal_connect ( GTK_WIDGET(vw->viking_vvp), "drag-data-received", G_CALLBACK(drag_data_received_cb), NULL );

  // Store the thread value so comparisons can be made to determine the gdk update method
  // Hopefully we are storing the main thread value here :)
  //  [ATM any window initialization is always be performed by the main thread]
  vw->thread = g_thread_self();

  // Set the default tool + mode
  gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, "Pan" ) );
  gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, "ModeMercator" ) );

  gchar *accel_file_name = g_build_filename ( a_get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL );
  gtk_accel_map_load ( accel_file_name );
  g_free ( accel_file_name );
}

static VikWindow *window_new ()
{
  return VIK_WINDOW ( g_object_new ( VIK_WINDOW_TYPE, NULL ) );
}

/**
 * Update the displayed map
 *  Only update the top most visible map layer
 *  ATM this assumes (as per defaults) the top most map has full alpha setting
 *   such that other other maps even though they may be active will not be seen
 *  It's more complicated to work out which maps are actually visible due to alpha settings
 *   and overkill for this simple refresh method.
 */
static void simple_map_update ( VikWindow *vw, gboolean only_new )
{
  // Find the most relevent single map layer to operate on
  VikLayer *vl = vik_aggregate_layer_get_top_visible_layer_of_type (vik_layers_panel_get_top_layer(vw->viking_vlp), VIK_LAYER_MAPS);
  if ( vl )
	vik_maps_layer_download ( VIK_MAPS_LAYER(vl), vw->viking_vvp, only_new );
}

/**
 * This is the global key press handler
 *  Global shortcuts are available at any time and hence are not restricted to when a certain tool is enabled
 */
static gboolean key_press_event( VikWindow *vw, GdkEventKey *event, gpointer data )
{
  // The keys handled here are not in the menuing system for a couple of reasons:
  //  . Keeps the menu size compact (alebit at expense of discoverably)
  //  . Allows differing key bindings to perform the same actions

  // First decide if key events are related to the maps layer
  gboolean map_download = FALSE;
  gboolean map_download_only_new = TRUE; // Only new or reload

  GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();

  // Standard 'Refresh' keys: F5 or Ctrl+r
  // Note 'F5' is actually handled via draw_refresh_cb() later on
  //  (not 'R' it's 'r' notice the case difference!!)
  if ( event->keyval == GDK_r && (event->state & modifiers) == GDK_CONTROL_MASK ) {
	map_download = TRUE;
	map_download_only_new = TRUE;
  }
  // Full cache reload with Ctrl+F5 or Ctrl+Shift+r [This is not in the menu system]
  // Note the use of uppercase R here since shift key has been pressed
  else if ( (event->keyval == GDK_F5 && (event->state & modifiers) == GDK_CONTROL_MASK ) ||
           ( event->keyval == GDK_R && (event->state & modifiers) == (GDK_CONTROL_MASK + GDK_SHIFT_MASK) ) ) {
	map_download = TRUE;
	map_download_only_new = FALSE;
  }
  // Standard Ctrl+KP+ / Ctrl+KP- to zoom in/out respectively
  else if ( event->keyval == GDK_KEY_KP_Add && (event->state & modifiers) == GDK_CONTROL_MASK ) {
    vik_viewport_zoom_in ( vw->viking_vvp );
    draw_update(vw);
    return TRUE; // handled keypress
  }
  else if ( event->keyval == GDK_KEY_KP_Subtract && (event->state & modifiers) == GDK_CONTROL_MASK ) {
    vik_viewport_zoom_out ( vw->viking_vvp );
    draw_update(vw);
    return TRUE; // handled keypress
  }

  if ( map_download ) {
    simple_map_update ( vw, map_download_only_new );
    return TRUE; // handled keypress
  }

  VikLayer *vl = vik_layers_panel_get_selected ( vw->viking_vlp );
  if (vl && vw->vt->active_tool != -1 && vw->vt->tools[vw->vt->active_tool].ti.key_press ) {
    gint ltype = vw->vt->tools[vw->vt->active_tool].layer_type;
    if ( vl && ltype == vl->type )
      return vw->vt->tools[vw->vt->active_tool].ti.key_press(vl, event, vw->vt->tools[vw->vt->active_tool].state);
  }

  // Ensure called only on window tools (i.e. not on any of the Layer tools since the layer is NULL)
  if ( vw->current_tool < TOOL_LAYER ) {
    // No layer - but enable window tool keypress processing - these should be able to handle a NULL layer
    if ( vw->vt->tools[vw->vt->active_tool].ti.key_press ) {
      return vw->vt->tools[vw->vt->active_tool].ti.key_press ( vl, event, vw->vt->tools[vw->vt->active_tool].state );
    }
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
      window_get_filename ( vw ) ) );
    gtk_dialog_add_buttons ( dia, _("Don't Save"), GTK_RESPONSE_NO, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL );
    switch ( gtk_dialog_run ( dia ) )
    {
      case GTK_RESPONSE_NO: gtk_widget_destroy ( GTK_WIDGET(dia) ); return FALSE;
      case GTK_RESPONSE_CANCEL: gtk_widget_destroy ( GTK_WIDGET(dia) ); return TRUE;
      default: gtk_widget_destroy ( GTK_WIDGET(dia) ); return ! save_file(NULL, vw);
    }
  }

  if ( window_count == 1 ) {
    // On the final window close - save latest state - if it's wanted...
    if ( a_vik_get_restore_window_state() ) {
      gint state = gdk_window_get_state ( GTK_WIDGET(vw)->window );
      gboolean state_max = state & GDK_WINDOW_STATE_MAXIMIZED;
      a_settings_set_boolean ( VIK_SETTINGS_WIN_MAX, state_max );

      gboolean state_fullscreen = state & GDK_WINDOW_STATE_FULLSCREEN;
      a_settings_set_boolean ( VIK_SETTINGS_WIN_FULLSCREEN, state_fullscreen );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL, GTK_WIDGET_VISIBLE (GTK_WIDGET(vw->viking_vlp)) );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_STATUSBAR, GTK_WIDGET_VISIBLE (GTK_WIDGET(vw->viking_vs)) );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_TOOLBAR, GTK_WIDGET_VISIBLE (toolbar_get_widget(vw->viking_vtb)) );

      // If supersized - no need to save the enlarged width+height values
      if ( ! (state_fullscreen || state_max) ) {
        gint width, height;
        gtk_window_get_size ( GTK_WINDOW (vw), &width, &height );
        a_settings_set_integer ( VIK_SETTINGS_WIN_WIDTH, width );
        a_settings_set_integer ( VIK_SETTINGS_WIN_HEIGHT, height );
      }

      a_settings_set_integer ( VIK_SETTINGS_WIN_PANE_POSITION, gtk_paned_get_position (GTK_PANED(vw->hpaned)) );
    }

    a_settings_set_integer ( VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH, vw->draw_image_width );
    a_settings_set_integer ( VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT, vw->draw_image_height );
    a_settings_set_boolean ( VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, vw->draw_image_save_as_png );

    gchar *accel_file_name = g_build_filename ( a_get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL );
    gtk_accel_map_save ( accel_file_name );
    g_free ( accel_file_name );
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
}

/*
 * Split the status update, as sometimes only need to update the tool part
 *  also on initialization the zoom related stuff is not ready to be used
 */
static void draw_status_tool ( VikWindow *vw )
{
  if ( vw->current_tool == TOOL_LAYER )
    // Use tooltip rather than the internal name as the tooltip is i8n
    vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_TOOL, vik_layer_get_interface(vw->tool_layer_id)->tools[vw->tool_tool_id].radioActionEntry.tooltip );
  else
    vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_TOOL, _(tool_names[vw->current_tool]) );
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

  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_ZOOM, zoom_level );

  draw_status_tool ( vw );  
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
    first = 0;
    vw->viewport_cursor = (GdkCursor *)toolbox_get_cursor(vw->vt, "Pan");
    /* We set cursor, even if it is NULL: it resets to default */
    gdk_window_set_cursor ( gtk_widget_get_window(GTK_WIDGET(vw->viking_vvp)), vw->viewport_cursor );
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
  // Main layer drawing
  vik_layers_panel_draw_all ( vw->viking_vlp );
  // Draw highlight (possibly again but ensures it is on top - especially for when tracks overlap)
  if ( vik_viewport_get_draw_highlight (vw->viking_vvp) ) {
    if ( vw->containing_vtl && (vw->selected_tracks || vw->selected_waypoints ) ) {
      vik_trw_layer_draw_highlight_items ( vw->containing_vtl, vw->selected_tracks, vw->selected_waypoints, vw->viking_vvp );
    }
    else if ( vw->containing_vtl && (vw->selected_track || vw->selected_waypoint) ) {
      vik_trw_layer_draw_highlight_item ( vw->containing_vtl, vw->selected_track, vw->selected_waypoint, vw->viking_vvp );
    }
    else if ( vw->selected_vtl ) {
      vik_trw_layer_draw_highlight ( vw->selected_vtl, vw->viking_vvp );
    }
  }
  // Other viewport decoration items on top if they are enabled/in use
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
    if ( vw->vt->tools[vw->vt->active_tool].ti.pan_handler )
      // Tool still may need to do something (such as disable something)
      toolbox_click(vw->vt, event);
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

/**
 * get_location_strings:
 *
 * Utility function to get positional strings for the given location
 * lat and lon strings will get allocated and so need to be freed after use
 */
static void get_location_strings ( VikWindow *vw, struct UTM utm, gchar **lat, gchar **lon )
{
  if ( vik_viewport_get_drawmode ( vw->viking_vvp ) == VIK_VIEWPORT_DRAWMODE_UTM ) {
    // Reuse lat for the first part (Zone + N or S, and lon for the second part (easting and northing) of a UTM format:
    //  ZONE[N|S] EASTING NORTHING
    *lat = g_malloc(4*sizeof(gchar));
    // NB zone is stored in a char but is an actual number
    g_snprintf (*lat, 4, "%d%c", utm.zone, utm.letter);
    *lon = g_malloc(16*sizeof(gchar));
    g_snprintf (*lon, 16, "%d %d", (gint)utm.easting, (gint)utm.northing);
  }
  else {
    struct LatLon ll;
    a_coords_utm_to_latlon ( &utm, &ll );
    a_coords_latlon_to_string ( &ll, lat, lon );
  }
}

static void draw_mouse_motion (VikWindow *vw, GdkEventMotion *event)
{
  static VikCoord coord;
  static struct UTM utm;
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

  get_location_strings ( vw, utm, &lat, &lon );

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

/**
 * Action the single click after a small timeout
 * If a double click has occurred then this will do nothing
 */
static gboolean vik_window_pan_timeout (VikWindow *vw)
{
  if ( ! vw->single_click_pending ) {
    // Double click happened, so don't do anything
    return FALSE;
  }

  /* set panning origin */
  vw->pan_move = FALSE;
  vw->single_click_pending = FALSE;
  vik_viewport_set_center_screen ( vw->viking_vvp, vw->delayed_pan_x, vw->delayed_pan_y );
  draw_update ( vw );

  // Really turn off the pan moving!!
  vw->pan_x = vw->pan_y = -1;
  return FALSE;
}

static void vik_window_pan_release ( VikWindow *vw, GdkEventButton *event )
{
  gboolean do_draw = TRUE;

  if ( vw->pan_move == FALSE ) {
    vw->single_click_pending = !vw->single_click_pending;

    if ( vw->single_click_pending ) {
      // Store offset to use
      vw->delayed_pan_x = vw->pan_x;
      vw->delayed_pan_y = vw->pan_y;
      // Get double click time
      GtkSettings *gs = gtk_widget_get_settings ( GTK_WIDGET(vw) );
      GValue dct = { 0 }; // = G_VALUE_INIT; // GLIB 2.30+ only
      g_value_init ( &dct, G_TYPE_INT );
      g_object_get_property ( G_OBJECT(gs), "gtk-double-click-time", &dct );
      // Give chance for a double click to occur
      gint timer = g_value_get_int ( &dct ) + 50;
      g_timeout_add ( timer, (GSourceFunc)vik_window_pan_timeout, vw );
      do_draw = FALSE;
    }
    else {
      vik_viewport_set_center_screen ( vw->viking_vvp, vw->pan_x, vw->pan_y );
    }
  }
  else {
     vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2 - event->x + vw->pan_x,
                                      vik_viewport_get_height(vw->viking_vvp)/2 - event->y + vw->pan_y );
  }

  vw->pan_move = FALSE;
  vw->pan_x = vw->pan_y = -1;
  if ( do_draw )
    draw_update ( vw );
}

static void draw_release ( VikWindow *vw, GdkEventButton *event )
{
  gtk_widget_grab_focus ( GTK_WIDGET(vw->viking_vvp) );

  if ( event->button == 2 ) {  /* move / pan */
    if ( vw->vt->tools[vw->vt->active_tool].ti.pan_handler )
      // Tool still may need to do something (such as reenable something)
      toolbox_release(vw->vt, event);
    vik_window_pan_release ( vw, event );
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
    // This zoom is on the center position
    if ( event->direction == GDK_SCROLL_UP )
      vik_viewport_zoom_in (vw->viking_vvp);
    else
      vik_viewport_zoom_out (vw->viking_vvp);
  } else {
    /* make sure mouse is still over the same point on the map when we zoom */
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
  }

  draw_update(vw);
}



/********************************************************************************
 ** Ruler tool code
 ********************************************************************************/
static void draw_ruler(VikViewport *vvp, GdkDrawable *d, GdkGC *gc, gint x1, gint y1, gint x2, gint y2, gdouble distance)
{
  PangoLayout *pl;
  gchar str[128];
  GdkGC *labgc = vik_viewport_new_gc ( vvp, "#cccccc", 1);
  GdkGC *thickgc = gdk_gc_new(d);
  
  gdouble len = sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
  gdouble dx = (x2-x1)/len*10; 
  gdouble dy = (y2-y1)/len*10;
  gdouble c = cos(DEG2RAD(15.0));
  gdouble s = sin(DEG2RAD(15.0));
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

  vik_viewport_compute_bearing ( vvp, x1, y1, x2, y2, &angle, &baseangle );

  {
    GdkColor color;
    gdk_gc_copy(thickgc, gc);
    gdk_gc_set_line_attributes(thickgc, CW, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
    gdk_color_parse("#2255cc", &color);
    gdk_gc_set_rgb_fg_color(thickgc, &color);
  }
  gdk_draw_arc (d, thickgc, FALSE, x1-CR+CW/2, y1-CR+CW/2, 2*CR-CW, 2*CR-CW, (90 - RAD2DEG(baseangle))*64, -RAD2DEG(angle)*64);


  gdk_gc_copy(thickgc, gc);
  gdk_gc_set_line_attributes(thickgc, 2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
  for (i=0; i<180; i++) {
    c = cos(DEG2RAD(i)*2 + baseangle);
    s = sin(DEG2RAD(i)*2 + baseangle);

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
    pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(vvp))->font_desc);
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
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
        g_sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
      } else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
        g_sprintf(str, "%d yards", (int)(distance*1.0936133));
      } else {
        g_sprintf(str, "%d NM", (int)VIK_METERS_TO_NAUTICAL_MILES(distance));
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
    g_sprintf(str, "%3.1f°", RAD2DEG(angle));
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
      case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
        temp = g_strdup_printf ( "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff( &coord, &(s->oldcoord) )) );
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
      buf = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
    }
    gdk_drawable_get_size(buf, &w2, &h2);
    if (w1 != w2 || h1 != h2) {
      g_object_unref ( G_OBJECT ( buf ) );
      buf = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), w1, h1, -1 );
    }

    vik_viewport_screen_to_coord ( vvp, (gint) event->x, (gint) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );
    vik_viewport_coord_to_screen ( vvp, &s->oldcoord, &oldx, &oldy );

    gdk_draw_drawable (buf, gtk_widget_get_style(GTK_WIDGET(vvp))->black_gc,
		       vik_viewport_get_pixmap(vvp), 0, 0, 0, 0, -1, -1);
    draw_ruler(vvp, buf, gtk_widget_get_style(GTK_WIDGET(vvp))->black_gc, oldx, oldy, event->x, event->y, vik_coord_diff( &coord, &(s->oldcoord)) );
    if (draw_buf_done) {
      static gpointer pass_along[3];
      pass_along[0] = gtk_widget_get_window(GTK_WIDGET(vvp));
      pass_along[1] = gtk_widget_get_style(GTK_WIDGET(vvp))->black_gc;
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
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      temp = g_strdup_printf ( "%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES (vik_coord_diff( &coord, &(s->oldcoord) )) );
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
  // Regardless of whether we used it, return false so other GTK things may use it
  return FALSE;
}

static VikToolInterface ruler_tool =
  // NB Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead
  { { "Ruler", "vik-icon-ruler", N_("_Ruler"), "<control><shift>U", N_("Ruler Tool"), 2 },
    (VikToolConstructorFunc) ruler_create,
    (VikToolDestructorFunc) ruler_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) ruler_deactivate, 
    (VikToolMouseFunc) ruler_click, 
    (VikToolMouseMoveFunc) ruler_move, 
    (VikToolMouseFunc) ruler_release,
    (VikToolKeyFunc) ruler_key_press,
    FALSE,
    GDK_CURSOR_IS_PIXMAP,
    &cursor_ruler_pixbuf,
    NULL };
/*** end ruler code ********************************************************/



/********************************************************************************
 ** Zoom tool code
 ********************************************************************************/

typedef struct {
  VikWindow *vw;
  GdkPixmap *pixmap;
  // Track zoom bounds for zoom tool with shift modifier:
  gboolean bounds_active;
  gint start_x;
  gint start_y;
} zoom_tool_state_t;

/*
 * In case the screen size has changed
 */
static void zoomtool_resize_pixmap (zoom_tool_state_t *zts)
{
    int w1, h1, w2, h2;

    // Allocate a drawing area the size of the viewport
    w1 = vik_viewport_get_width ( zts->vw->viking_vvp );
    h1 = vik_viewport_get_height ( zts->vw->viking_vvp );

    if ( !zts->pixmap ) {
      // Totally new
      zts->pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(zts->vw->viking_vvp)), w1, h1, -1 );
    }

    gdk_drawable_get_size ( zts->pixmap, &w2, &h2 );

    if ( w1 != w2 || h1 != h2 ) {
      // Has changed - delete and recreate with new values
      g_object_unref ( G_OBJECT ( zts->pixmap ) );
      zts->pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(zts->vw->viking_vvp)), w1, h1, -1 );
    }
}

static gpointer zoomtool_create (VikWindow *vw, VikViewport *vvp)
{
  zoom_tool_state_t *zts = g_new(zoom_tool_state_t, 1);
  zts->vw = vw;
  zts->pixmap = NULL;
  zts->start_x = 0;
  zts->start_y = 0;
  zts->bounds_active = FALSE;
  return zts;
}

static void zoomtool_destroy ( zoom_tool_state_t *zts)
{
  if ( zts->pixmap )
    g_object_unref ( G_OBJECT ( zts->pixmap ) );
  g_free(zts);
}

static VikLayerToolFuncStatus zoomtool_click (VikLayer *vl, GdkEventButton *event, zoom_tool_state_t *zts)
{
  zts->vw->modified = TRUE;
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

  VikCoord coord;
  gint x, y;
  gint center_x = vik_viewport_get_width ( zts->vw->viking_vvp ) / 2;
  gint center_y = vik_viewport_get_height ( zts->vw->viking_vvp ) / 2;

  gboolean skip_update = FALSE;

  zts->bounds_active = FALSE;

  if ( modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) ) {
    // This zoom is on the center position
    vik_viewport_set_center_screen ( zts->vw->viking_vvp, center_x, center_y );
    if ( event->button == 1 )
      vik_viewport_zoom_in (zts->vw->viking_vvp);
    else if ( event->button == 3 )
      vik_viewport_zoom_out (zts->vw->viking_vvp);
  }
  else if ( modifiers == GDK_CONTROL_MASK ) {
    // This zoom is to recenter on the mouse position
    vik_viewport_set_center_screen ( zts->vw->viking_vvp, (gint) event->x, (gint) event->y );
    if ( event->button == 1 )
      vik_viewport_zoom_in (zts->vw->viking_vvp);
    else if ( event->button == 3 )
      vik_viewport_zoom_out (zts->vw->viking_vvp);
  }
  else if ( modifiers == GDK_SHIFT_MASK ) {
    // Get start of new zoom bounds
    if ( event->button == 1 ) {
      zts->bounds_active = TRUE;
      zts->start_x = (gint) event->x;
      zts->start_y = (gint) event->y;
      skip_update = TRUE;
    }
  }
  else {
    /* make sure mouse is still over the same point on the map when we zoom */
    vik_viewport_screen_to_coord ( zts->vw->viking_vvp, event->x, event->y, &coord );
    if ( event->button == 1 )
      vik_viewport_zoom_in (zts->vw->viking_vvp);
    else if ( event->button == 3 )
      vik_viewport_zoom_out(zts->vw->viking_vvp);
    vik_viewport_coord_to_screen ( zts->vw->viking_vvp, &coord, &x, &y );
    vik_viewport_set_center_screen ( zts->vw->viking_vvp,
                                     center_x + (x - event->x),
                                     center_y + (y - event->y) );
  }

  if ( !skip_update )
    draw_update ( zts->vw );

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_move (VikLayer *vl, GdkEventMotion *event, zoom_tool_state_t *zts)
{
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

  if ( zts->bounds_active && modifiers == GDK_SHIFT_MASK ) {
    zoomtool_resize_pixmap ( zts );

    // Blank out currently drawn area
    gdk_draw_drawable ( zts->pixmap,
                        gtk_widget_get_style(GTK_WIDGET(zts->vw->viking_vvp))->black_gc,
                        vik_viewport_get_pixmap(zts->vw->viking_vvp),
                        0, 0, 0, 0, -1, -1);

    // Calculate new box starting point & size in pixels
    int xx, yy, width, height;
    if ( event->y > zts->start_y ) {
      yy = zts->start_y;
      height = event->y-zts->start_y;
    }
    else {
      yy = event->y;
      height = zts->start_y-event->y;
    }
    if ( event->x > zts->start_x ) {
      xx = zts->start_x;
      width = event->x-zts->start_x;
    }
    else {
      xx = event->x;
      width = zts->start_x-event->x;
    }

    // Draw the box
    gdk_draw_rectangle (zts->pixmap, gtk_widget_get_style(GTK_WIDGET(zts->vw->viking_vvp))->black_gc, FALSE, xx, yy, width, height);

    // Only actually draw when there's time to do so
    if (draw_buf_done) {
      static gpointer pass_along[3];
      pass_along[0] = gtk_widget_get_window(GTK_WIDGET(zts->vw->viking_vvp));
      pass_along[1] = gtk_widget_get_style(GTK_WIDGET(zts->vw->viking_vvp))->black_gc;
      pass_along[2] = zts->pixmap;
      g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, draw_buf, pass_along, NULL);
      draw_buf_done = FALSE;
    }
  }
  else
    zts->bounds_active = FALSE;

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_release (VikLayer *vl, GdkEventButton *event, zoom_tool_state_t *zts)
{
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

  // Ensure haven't just released on the exact same position
  //  i.e. probably haven't moved the mouse at all
  if ( zts->bounds_active && modifiers == GDK_SHIFT_MASK &&
     ( event->x < zts->start_x-5 || event->x > zts->start_x+5 ) &&
     ( event->y < zts->start_y-5 || event->y > zts->start_y+5 ) ) {

    VikCoord coord1, coord2;
    vik_viewport_screen_to_coord ( zts->vw->viking_vvp, zts->start_x, zts->start_y, &coord1);
    vik_viewport_screen_to_coord ( zts->vw->viking_vvp, event->x, event->y, &coord2);

    // From the extend of the bounds pick the best zoom level
    // c.f. trw_layer_zoom_to_show_latlons()
    // Maybe refactor...
    struct LatLon ll1, ll2;
    vik_coord_to_latlon(&coord1, &ll1);
    vik_coord_to_latlon(&coord2, &ll2);
    struct LatLon average = { (ll1.lat+ll2.lat)/2,
			      (ll1.lon+ll2.lon)/2 };

    VikCoord new_center;
    vik_coord_load_from_latlon ( &new_center, vik_viewport_get_coord_mode ( zts->vw->viking_vvp ), &average );
    vik_viewport_set_center_coord ( zts->vw->viking_vvp, &new_center, FALSE );

    /* Convert into definite 'smallest' and 'largest' positions */
    struct LatLon minmin;
    if ( ll1.lat < ll2.lat )
      minmin.lat = ll1.lat;
    else
      minmin.lat = ll2.lat;

    struct LatLon maxmax;
    if ( ll1.lon > ll2.lon )
      maxmax.lon = ll1.lon;
    else
      maxmax.lon = ll2.lon;

    /* Always recalculate the 'best' zoom level */
    gdouble zoom = VIK_VIEWPORT_MIN_ZOOM;
    vik_viewport_set_zoom ( zts->vw->viking_vvp, zoom );

    gdouble min_lat, max_lat, min_lon, max_lon;
    /* Should only be a maximum of about 18 iterations from min to max zoom levels */
    while ( zoom <= VIK_VIEWPORT_MAX_ZOOM ) {
      vik_viewport_get_min_max_lat_lon ( zts->vw->viking_vvp, &min_lat, &max_lat, &min_lon, &max_lon );
      /* NB I think the logic used in this test to determine if the bounds is within view
	 fails if track goes across 180 degrees longitude.
	 Hopefully that situation is not too common...
	 Mind you viking doesn't really do edge locations to well anyway */
      if ( min_lat < minmin.lat &&
           max_lat > minmin.lat &&
           min_lon < maxmax.lon &&
           max_lon > maxmax.lon )
	/* Found within zoom level */
	break;

      /* Try next */
      zoom = zoom * 2;
      vik_viewport_set_zoom ( zts->vw->viking_vvp, zoom );
    }
  }
  else {
     // When pressing shift and clicking for zoom, then jump three levels
     if ( modifiers == GDK_SHIFT_MASK ) {
       // Zoom in/out by three if possible
       vik_viewport_set_center_screen ( zts->vw->viking_vvp, event->x, event->y );
       if ( event->button == 1 ) {
          vik_viewport_zoom_in ( zts->vw->viking_vvp );
          vik_viewport_zoom_in ( zts->vw->viking_vvp );
          vik_viewport_zoom_in ( zts->vw->viking_vvp );
       }
       else if ( event->button == 3 ) {
          vik_viewport_zoom_out ( zts->vw->viking_vvp );
          vik_viewport_zoom_out ( zts->vw->viking_vvp );
          vik_viewport_zoom_out ( zts->vw->viking_vvp );
       }
     }
  }

  draw_update ( zts->vw );

  // Reset
  zts->bounds_active = FALSE;

  return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface zoom_tool = 
  { { "Zoom", "vik-icon-zoom", N_("_Zoom"), "<control><shift>Z", N_("Zoom Tool"), 1 },
    (VikToolConstructorFunc) zoomtool_create,
    (VikToolDestructorFunc) zoomtool_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) zoomtool_click, 
    (VikToolMouseMoveFunc) zoomtool_move,
    (VikToolMouseFunc) zoomtool_release,
    NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP,
    &cursor_zoom_pixbuf,
    NULL };
/*** end zoom code ********************************************************/

/********************************************************************************
 ** Pan tool code
 ********************************************************************************/
static gpointer pantool_create (VikWindow *vw, VikViewport *vvp)
{
  return vw;
}

// NB Double clicking means this gets called THREE times!!!
static VikLayerToolFuncStatus pantool_click (VikLayer *vl, GdkEventButton *event, VikWindow *vw)
{
  vw->modified = TRUE;

  if ( event->type == GDK_2BUTTON_PRESS ) {
    // Zoom in / out on double click
    // No need to change the center as that has already occurred in the first click of a double click occurrence
    if ( event->button == 1 ) {
      guint modifier = event->state & GDK_SHIFT_MASK;
      if ( modifier )
        vik_viewport_zoom_out ( vw->viking_vvp );
      else
        vik_viewport_zoom_in ( vw->viking_vvp );
    }
    else if ( event->button == 3 )
      vik_viewport_zoom_out ( vw->viking_vvp );

    draw_update ( vw );
  }
  else
    // Standard pan click
    if ( event->button == 1 )
      vik_window_pan_click ( vw, event );

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
  { { "Pan", "vik-icon-pan", N_("_Pan"), "<control><shift>P", N_("Pan Tool"), 0 },
    (VikToolConstructorFunc) pantool_create,
    (VikToolDestructorFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) pantool_click, 
    (VikToolMouseMoveFunc) pantool_move,
    (VikToolMouseFunc) pantool_release,
    NULL,
    FALSE,
    GDK_FLEUR,
    NULL,
    NULL };
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

#ifdef WINDOWS
// Hopefully Alt keys by default
#define VIK_MOVE_MODIFIER GDK_MOD1_MASK
#else
// Alt+mouse on Linux desktops tend to be used by the desktop manager
// Thus use an alternate modifier - you may need to set something into this group
#define VIK_MOVE_MODIFIER GDK_MOD5_MASK
#endif

static VikLayerToolFuncStatus selecttool_click (VikLayer *vl, GdkEventButton *event, tool_ed_t *t)
{
  t->vw->select_move = FALSE;
  /* Only allow selection on primary button */
  if ( event->button == 1 ) {

    if ( event->state & VIK_MOVE_MODIFIER )
      vik_window_pan_click ( t->vw, event );
    else {
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
      else {
        // Something found - so enable movement
        t->vw->select_move = TRUE;
      }
    }
  }
  else if ( ( event->button == 3 ) && ( vl && ( vl->type == VIK_LAYER_TRW ) ) ) {
    if ( vl->visible )
      /* Act on currently selected item to show menu */
      if ( t->vw->selected_track || t->vw->selected_waypoint )
	if ( vik_layer_get_interface(vl->type)->show_viewport_menu )
	  vik_layer_get_interface(vl->type)->show_viewport_menu ( vl, event, t->vw->viking_vvp );
  }

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_move (VikLayer *vl, GdkEventMotion *event, tool_ed_t *t)
{
  if ( t->vw->select_move ) {
    // Don't care about vl here
    if ( t->vtl )
      if ( vik_layer_get_interface(VIK_LAYER_TRW)->select_move )
        vik_layer_get_interface(VIK_LAYER_TRW)->select_move ( vl, event, t->vvp, t );
  }
  else
    // Optional Panning
    if ( event->state & VIK_MOVE_MODIFIER )
      vik_window_pan_move ( t->vw, event );

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_release (VikLayer *vl, GdkEventButton *event, tool_ed_t *t)
{
  if ( t->vw->select_move ) {
    // Don't care about vl here
    if ( t->vtl )
      if ( vik_layer_get_interface(VIK_LAYER_TRW)->select_release )
        vik_layer_get_interface(VIK_LAYER_TRW)->select_release ( (VikLayer*)t->vtl, event, t->vvp, t );
  }

  if ( event->button == 1 && (event->state & VIK_MOVE_MODIFIER) )
      vik_window_pan_release ( t->vw, event );

  // Force pan off incase it was on
  t->vw->pan_move = FALSE;
  t->vw->pan_x = t->vw->pan_y = -1;

  // End of this select movement
  t->vw->select_move = FALSE;

  return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface select_tool =
  { { "Select", "vik-icon-select", N_("_Select"), "<control><shift>S", N_("Select Tool"), 3 },
    (VikToolConstructorFunc) selecttool_create,
    (VikToolDestructorFunc) selecttool_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) selecttool_click,
    (VikToolMouseMoveFunc) selecttool_move,
    (VikToolMouseFunc) selecttool_release,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_LEFT_PTR,
    NULL,
    NULL };
/*** end select tool code ********************************************************/

static void draw_pan_cb ( GtkAction *a, VikWindow *vw )
{
  // Since the treeview cell editting intercepts standard keyboard handlers, it means we can receive events here
  // Thus if currently editting, ensure we don't move the viewport when Ctrl+<arrow> is received
  VikLayer *sel = vik_layers_panel_get_selected ( vw->viking_vlp );
  if ( sel && vik_treeview_get_editing ( sel->vt ) )
    return;

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

static void draw_goto_cb ( GtkAction *a, VikWindow *vw )
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

  vik_viewport_set_center_coord ( vw->viking_vvp, &new_center, TRUE );
  draw_update ( vw );
}

/**
 * center_changed_cb:
 */
static void center_changed_cb ( VikWindow *vw )
{
// ATM Keep back always available, so when we pan - we can jump to the last requested position
/*
  GtkAction* action_back = gtk_action_group_get_action ( vw->action_group, "GoBack" );
  if ( action_back ) {
    gtk_action_set_sensitive ( action_back, vik_viewport_back_available(vw->viking_vvp) );
  }
*/
  GtkAction* action_forward = gtk_action_group_get_action ( vw->action_group, "GoForward" );
  if ( action_forward ) {
    gtk_action_set_sensitive ( action_forward, vik_viewport_forward_available(vw->viking_vvp) );
  }

  toolbar_action_set_sensitive ( vw->viking_vtb, "GoForward", vik_viewport_forward_available(vw->viking_vvp) );
}

/**
 * draw_goto_back_and_forth:
 */
static void draw_goto_back_and_forth ( GtkAction *a, VikWindow *vw )
{
  gboolean changed = FALSE;
  if (!strcmp(gtk_action_get_name(a), "GoBack")) {
    changed = vik_viewport_go_back ( vw->viking_vvp );
  }
  else if (!strcmp(gtk_action_get_name(a), "GoForward")) {
    changed = vik_viewport_go_forward ( vw->viking_vvp );
  }
  else {
    return;
  }

  // Recheck buttons sensitivities, as the center changed signal is not sent on back/forward changes
  //  (otherwise we would get stuck in an infinite loop!)
  center_changed_cb ( vw );

  if ( changed )
    draw_update ( vw );
}

/**
 * Refresh maps displayed
 */
static void draw_refresh_cb ( GtkAction *a, VikWindow *vw )
{
  // Only get 'new' maps
  simple_map_update ( vw, TRUE );
}

static void menu_addlayer_cb ( GtkAction *a, VikWindow *vw )
{
 VikLayerTypeEnum type;
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
  if ( vik_layers_panel_paste_selected ( vw->viking_vlp ) )
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
  gchar *uri;
  uri = g_strdup_printf("ghelp:%s", PACKAGE);
  GError *error = NULL;
  gboolean show = gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, &error);
  if ( !show && !error )
    // No error to show, so unlikely this will get called
    a_dialog_error_msg ( GTK_WINDOW(vw), _("The help system is not available.") );
  else if ( error ) {
    // Main error path
    a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Help is not available because: %s.\nEnsure a Mime Type ghelp handler program is installed (e.g. yelp)."), error->message );
    g_error_free ( error );
  }
  g_free(uri);
#endif /* WINDOWS */
}

static void toggle_side_panel ( VikWindow *vw )
{
  vw->show_side_panel = !vw->show_side_panel;
  if ( vw->show_side_panel )
    gtk_widget_show(GTK_WIDGET(vw->viking_vlp));
  else
    gtk_widget_hide(GTK_WIDGET(vw->viking_vlp));
}

static void toggle_full_screen ( VikWindow *vw )
{
  vw->show_full_screen = !vw->show_full_screen;
  if ( vw->show_full_screen )
    gtk_window_fullscreen ( GTK_WINDOW(vw) );
  else
    gtk_window_unfullscreen ( GTK_WINDOW(vw) );
}

static void toggle_statusbar ( VikWindow *vw )
{
  vw->show_statusbar = !vw->show_statusbar;
  if ( vw->show_statusbar )
    gtk_widget_show ( GTK_WIDGET(vw->viking_vs) );
  else
    gtk_widget_hide ( GTK_WIDGET(vw->viking_vs) );
}

static void toggle_toolbar ( VikWindow *vw )
{
  vw->show_toolbar = !vw->show_toolbar;
  if ( vw->show_toolbar )
    gtk_widget_show ( toolbar_get_widget (vw->viking_vtb) );
  else
    gtk_widget_hide ( toolbar_get_widget (vw->viking_vtb) );
}

static void toggle_main_menu ( VikWindow *vw )
{
  vw->show_main_menu = !vw->show_main_menu;
  if ( vw->show_main_menu )
    gtk_widget_show ( gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu" ) );
  else
    gtk_widget_hide ( gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu" ) );
}

// Only for 'view' toggle menu widgets ATM.
GtkWidget *get_show_widget_by_name(VikWindow *vw, const gchar *name)
{
  g_return_val_if_fail(name != NULL, NULL);

  // ATM only FullScreen is *not* in SetShow path
  gchar *path;
  if ( g_strcmp0 ("FullScreen", name ) )
    path = g_strconcat("/ui/MainMenu/View/SetShow/", name, NULL);
  else
    path = g_strconcat("/ui/MainMenu/View/", name, NULL);

  GtkWidget *widget = gtk_ui_manager_get_widget(vw->uim, path);
  g_free(path);

  return widget;
}

static void tb_view_side_panel_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_side_panel ( vw );
}

static void tb_full_screen_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_full_screen;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_full_screen ( vw );
}

static void tb_view_statusbar_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_statusbar;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_statusbar ( vw );
}

static void tb_view_toolbar_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_toolbar;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_toolbar ( vw );
}

static void tb_view_main_menu_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_main_menu;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_main_menu ( vw );
}

static void tb_set_draw_scale ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vik_viewport_get_draw_scale ( vw->viking_vvp );
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else {
    vik_viewport_set_draw_scale ( vw->viking_vvp, next_state );
    draw_update ( vw );
  }
}

static void tb_set_draw_centermark ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vik_viewport_get_draw_centermark ( vw->viking_vvp );
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else {
    vik_viewport_set_draw_centermark ( vw->viking_vvp, next_state );
    draw_update ( vw );
  }
}

static void tb_set_draw_highlight ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vik_viewport_get_draw_highlight ( vw->viking_vvp );
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else {
    vik_viewport_set_draw_highlight ( vw->viking_vvp, next_state );
    draw_update ( vw );
  }
}

static void help_about_cb ( GtkAction *a, VikWindow *vw )
{
  a_dialog_about(GTK_WINDOW(vw));
}

static void help_cache_info_cb ( GtkAction *a, VikWindow *vw )
{
  // NB: No i18n as this is just for debug
  gint byte_size = a_mapcache_get_size();
  gchar *msg_sz = NULL;
  gchar *msg = NULL;
#if GLIB_CHECK_VERSION(2,30,0)
  msg_sz = g_format_size_full ( byte_size, G_FORMAT_SIZE_LONG_FORMAT );
#else
  msg_sz = g_format_size_for_display ( byte_size );
#endif
  msg = g_strdup_printf ( "Map Cache size is %s with %d items", msg_sz, a_mapcache_get_count());
  a_dialog_info_msg_extra ( GTK_WINDOW(vw), "%s", msg );
  g_free ( msg_sz );
  g_free ( msg );
}

static void back_forward_info_cb ( GtkAction *a, VikWindow *vw )
{
  vik_viewport_show_centers ( vw->viking_vvp, GTK_WINDOW(vw) );
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

static void full_screen_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_full_screen;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_full_screen ( vw );
  }
  else
    toggle_full_screen ( vw );
}

static void view_side_panel_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_side_panel ( vw );
  }
  else
    toggle_side_panel ( vw );
}

static void view_statusbar_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_statusbar;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_statusbar ( vw );
  }
  else
    toggle_statusbar ( vw );
}

static void view_toolbar_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_toolbar;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_toolbar ( vw );
  }
  else
    toggle_toolbar ( vw );
}

static void view_main_menu_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_main_menu;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_main_menu ( vw );
  }
  else
    toggle_toolbar ( vw );
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
    if (!strcmp(tool_name, vt->tools[i].ti.radioActionEntry.name)) {
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
    return;
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
  gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, vik_layer_get_interface(layer_id)->tools[tool_id].radioActionEntry.name ) );
}

// Be careful with usage - as it may trigger actions being continually alternately by the menu and toolbar items
// DON'T Use this from menu callback with toggle toolbar items!!
static void toolbar_sync ( VikWindow *vw, const gchar *name, gboolean state )
{
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, name );
  if ( tbutton ) {
    // Causes toggle signal action to be raised.
    gtk_toggle_tool_button_set_active ( tbutton, state );
  }
}

/* this function gets called whenever a menu is clicked */
// Note old is not used
static void menu_cb ( GtkAction *old, GtkAction *a, VikWindow *vw )
{
  // Ensure Toolbar kept in sync
  const gchar *name = gtk_action_get_name(a);
  toolbar_sync ( vw, name, TRUE );

  /* White Magic, my friends ... White Magic... */
  gint tool_id;
  toolbox_activate(vw->vt, name);

  vw->viewport_cursor = (GdkCursor *)toolbox_get_cursor(vw->vt, name);

  if ( gtk_widget_get_window(GTK_WIDGET(vw->viking_vvp)) )
    /* We set cursor, even if it is NULL: it resets to default */
    gdk_window_set_cursor ( gtk_widget_get_window(GTK_WIDGET(vw->viking_vvp)), vw->viewport_cursor );

  if (!g_strcmp0(name, "Pan")) {
    vw->current_tool = TOOL_PAN;
  } 
  else if (!g_strcmp0(name, "Zoom")) {
    vw->current_tool = TOOL_ZOOM;
  } 
  else if (!g_strcmp0(name, "Ruler")) {
    vw->current_tool = TOOL_RULER;
  }
  else if (!g_strcmp0(name, "Select")) {
    vw->current_tool = TOOL_SELECT;
  }
  else {
    VikLayerTypeEnum layer_id;
    for (layer_id=0; layer_id<VIK_LAYER_NUM_TYPES; layer_id++) {
      for ( tool_id = 0; tool_id < vik_layer_get_interface(layer_id)->tools_count; tool_id++ ) {
        if (!g_strcmp0(vik_layer_get_interface(layer_id)->tools[tool_id].radioActionEntry.name, name)) {
           vw->current_tool = TOOL_LAYER;
           vw->tool_layer_id = layer_id;
           vw->tool_tool_id = tool_id;
        }
      }
    }
  }
  draw_status_tool ( vw );
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
  }
  else
  {
    vw->filename = g_strdup(filename);
  }

  /* Refresh window's title */
  file = window_get_filename ( vw );
  title = g_strdup_printf( "%s - Viking", file );
  gtk_window_set_title ( GTK_WINDOW(vw), title );
  g_free ( title );
}

static const gchar *window_get_filename ( VikWindow *vw )
{
  return vw->filename ? a_file_basename ( vw->filename ) : _("Untitled");
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
  gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (menu), a_vik_get_recent_number_files() );

  menu_item = gtk_ui_manager_get_widget (self->uim, "/ui/MainMenu/File/OpenRecentFile");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);

  g_signal_connect (G_OBJECT (menu), "item-activated",
                    G_CALLBACK (on_activate_recent_item), (gpointer) self);
}

/*
 *
 */
static void update_recently_used_document (VikWindow *vw, const gchar *filename)
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
    gchar *msg = g_strdup_printf (_("Unable to add '%s' to the list of recently used documents"), uri);
    vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, msg );
    g_free ( msg );
  }

  g_free (uri);
  g_free (basename);
  g_free (recent_data->app_exec);
  g_slice_free (GtkRecentData, recent_data);
}

/**
 * Call this before doing things that may take a long time and otherwise not show any other feedback
 *  such as loading and saving files
 */
void vik_window_set_busy_cursor ( VikWindow *vw )
{
  gdk_window_set_cursor ( gtk_widget_get_window(GTK_WIDGET(vw)), vw->busy_cursor );
  // Viewport has a separate cursor
  gdk_window_set_cursor ( gtk_widget_get_window(GTK_WIDGET(vw->viking_vvp)), vw->busy_cursor );
  // Ensure cursor updated before doing stuff
  while( gtk_events_pending() )
    gtk_main_iteration();
}

void vik_window_clear_busy_cursor ( VikWindow *vw )
{
  gdk_window_set_cursor ( gtk_widget_get_window(GTK_WIDGET(vw)), NULL );
  // Restore viewport cursor
  gdk_window_set_cursor ( gtk_widget_get_window(GTK_WIDGET(vw->viking_vvp)), vw->viewport_cursor );
}

void vik_window_open_file ( VikWindow *vw, const gchar *filename, gboolean change_filename )
{
  vik_window_set_busy_cursor ( vw );

  // Enable the *new* filename to be accessible by the Layers codez
  gchar *original_filename = g_strdup ( vw->filename );
  g_free ( vw->filename );
  vw->filename = g_strdup ( filename );
  gboolean success = FALSE;
  gboolean restore_original_filename = FALSE;

  vw->loaded_type = a_file_load ( vik_layers_panel_get_top_layer(vw->viking_vlp), vw->viking_vvp, filename );
  switch ( vw->loaded_type )
  {
    case LOAD_TYPE_READ_FAILURE:
      a_dialog_error_msg ( GTK_WINDOW(vw), _("The file you requested could not be opened.") );
      break;
    case LOAD_TYPE_GPSBABEL_FAILURE:
      a_dialog_error_msg ( GTK_WINDOW(vw), _("GPSBabel is required to load files of this type or GPSBabel encountered problems.") );
      break;
    case LOAD_TYPE_GPX_FAILURE:
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unable to load malformed GPX file %s"), filename );
      break;
    case LOAD_TYPE_UNSUPPORTED_FAILURE:
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unsupported file type for %s"), filename );
      break;
    case LOAD_TYPE_VIK_FAILURE_NON_FATAL:
    {
      // Since we can process .vik files with issues just show a warning in the status bar
      // Not that a user can do much about it... or tells them what this issue is yet...
      gchar *msg = g_strdup_printf (_("WARNING: issues encountered loading %s"), a_file_basename (filename) );
      vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, msg );
      g_free ( msg );
    }
      // No break, carry on to show any data
    case LOAD_TYPE_VIK_SUCCESS:
    {
      restore_original_filename = TRUE; // NB Will actually get inverted by the 'success' component below
      GtkWidget *mode_button;
      /* Update UI */
      if ( change_filename )
        window_set_filename ( vw, filename );
      mode_button = vik_window_get_drawmode_button ( vw, vik_viewport_get_drawmode ( vw->viking_vvp ) );
      vw->only_updating_coord_mode_ui = TRUE; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
      gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(mode_button), TRUE );
      vw->only_updating_coord_mode_ui = FALSE;

      vik_layers_panel_change_coord_mode ( vw->viking_vlp, vik_viewport_get_coord_mode ( vw->viking_vvp ) );

      // Slightly long winded methods to align loaded viewport settings with the UI
      //  Since the rewrite for toolbar + menu actions
      //  there no longer exists a simple way to directly change the UI to a value for toggle settings
      //  it only supports toggling the existing setting (otherwise get infinite loops in trying to align tb+menu elements)
      // Thus get state, compare them, if different then invert viewport setting and (re)sync the setting (via toggling)
      gboolean vp_state_scale = vik_viewport_get_draw_scale ( vw->viking_vvp );
      gboolean ui_state_scale = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(get_show_widget_by_name(vw, "ShowScale")) );
      if ( vp_state_scale != ui_state_scale ) {
        vik_viewport_set_draw_scale ( vw->viking_vvp, !vp_state_scale );
        toggle_draw_scale ( NULL, vw );
      }
      gboolean vp_state_centermark = vik_viewport_get_draw_centermark ( vw->viking_vvp );
      gboolean ui_state_centermark = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(get_show_widget_by_name(vw, "ShowCenterMark")) );
      if ( vp_state_centermark != ui_state_centermark ) {
        vik_viewport_set_draw_centermark ( vw->viking_vvp, !vp_state_centermark );
        toggle_draw_centermark ( NULL, vw );
      }
      gboolean vp_state_highlight = vik_viewport_get_draw_highlight ( vw->viking_vvp );
      gboolean ui_state_highlight = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(get_show_widget_by_name(vw, "ShowHighlight")) );
      if ( vp_state_highlight != ui_state_highlight ) {
        vik_viewport_set_draw_highlight ( vw->viking_vvp, !vp_state_highlight );
        toggle_draw_highlight ( NULL, vw );
      }
    }
      // NB No break, carry on to redraw
    //case LOAD_TYPE_OTHER_SUCCESS:
    default:
      success = TRUE;
      // When LOAD_TYPE_OTHER_SUCCESS *only*, this will maintain the existing Viking project
      restore_original_filename = ! restore_original_filename;
      update_recently_used_document (vw, filename);
      draw_update ( vw );
      break;
  }

  if ( ! success || restore_original_filename )
    // Load didn't work or want to keep as the existing Viking project, keep using the original name
    window_set_filename ( vw, original_filename );
  g_free ( original_filename );

  vik_window_clear_busy_cursor ( vw );
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
    gchar *cwd = g_get_current_dir();
    if ( cwd ) {
      gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER(vw->open_dia), cwd );
      g_free ( cwd );
    }

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
    gtk_file_filter_set_name ( filter, _("JPG") );
    gtk_file_filter_add_mime_type ( filter, "image/jpeg");
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
      gboolean first_vik_file = TRUE;
      cur_file = files;
      while ( cur_file ) {

        gchar *file_name = cur_file->data;
        if ( newwindow && check_file_magic_vik ( file_name ) ) {
          // Load first of many .vik files in current window
          if ( first_vik_file ) {
            vik_window_open_file ( vw, file_name, TRUE );
            first_vik_file = FALSE;
          }
          else {
            // Load each subsequent .vik file in a separate window
            VikWindow *newvw = vik_window_new_window ();
            if (newvw)
              vik_window_open_file ( newvw, file_name, TRUE );
          }
        }
        else
          // Other file types
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
    gchar *cwd = g_get_current_dir();
    if ( cwd ) {
      gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER(vw->save_dia), cwd );
      g_free ( cwd );
    }

    GtkFileFilter *filter;
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name( filter, _("All") );
    gtk_file_filter_add_pattern ( filter, "*" );
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(vw->save_dia), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name( filter, _("Viking") );
    gtk_file_filter_add_pattern ( filter, "*.vik" );
    gtk_file_filter_add_pattern ( filter, "*.viking" );
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(vw->save_dia), filter);
    // Default to a Viking file
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER(vw->save_dia), filter);

    gtk_window_set_transient_for ( GTK_WINDOW(vw->save_dia), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->save_dia), TRUE );
  }
  // Auto append / replace extension with '.vik' to the suggested file name as it's going to be a Viking File
  gchar* auto_save_name = g_strdup ( window_get_filename ( vw ) );
  if ( ! a_file_check_ext ( auto_save_name, ".vik" ) )
    auto_save_name = g_strconcat ( auto_save_name, ".vik", NULL );

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(vw->save_dia), auto_save_name);

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
  g_free ( auto_save_name );
  gtk_widget_hide ( vw->save_dia );
  return rv;
}

static gboolean window_save ( VikWindow *vw )
{
  vik_window_set_busy_cursor ( vw );
  gboolean success = TRUE;

  if ( a_file_save ( vik_layers_panel_get_top_layer ( vw->viking_vlp ), vw->viking_vvp, vw->filename ) )
  {
    update_recently_used_document ( vw, vw->filename );
  }
  else
  {
    a_dialog_error_msg ( GTK_WINDOW(vw), _("The filename you requested could not be opened for writing.") );
    success = FALSE;
  }
  vik_window_clear_busy_cursor ( vw );
  return success;
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

/**
 * export_to:
 *
 * Export all TRW Layers in the list to individual files in the specified directory
 *
 * Returns: %TRUE on success
 */
static gboolean export_to ( VikWindow *vw, GList *gl, VikFileType_t vft, const gchar *dir, const gchar *extension )
{
  gboolean success = TRUE;

  gint export_count = 0;

  vik_window_set_busy_cursor ( vw );

  while ( gl ) {

    gchar *fn = g_strconcat ( dir, G_DIR_SEPARATOR_S, VIK_LAYER(gl->data)->name, extension, NULL );

    // Some protection in attempting to write too many same named files
    // As this will get horribly slow...
    gboolean safe = FALSE;
    gint ii = 2;
    while ( ii < 5000 ) {
      if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) ) {
        // Try rename
        g_free ( fn );
        fn = g_strdup_printf ( "%s%s%s#%03d%s", dir, G_DIR_SEPARATOR_S, VIK_LAYER(gl->data)->name, ii, extension );
	  }
	  else {
		  safe = TRUE;
		  break;
	  }
	  ii++;
    }
    if ( ii == 5000 )
      success = FALSE;

    // NB: We allow exporting empty layers
    if ( safe ) {
      gboolean this_success = a_file_export ( VIK_TRW_LAYER(gl->data), fn, vft, NULL, TRUE );

      // Show some progress
      if ( this_success ) {
        export_count++;
        gchar *message = g_strdup_printf ( _("Exporting to file: %s"), fn );
        vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, message );
        while ( gtk_events_pending() )
          gtk_main_iteration ();
        g_free ( message );
      }
      
      success = success && this_success;
    }

    g_free ( fn );
    gl = g_list_next ( gl );
  }

  vik_window_clear_busy_cursor ( vw );

  // Confirm what happened.
  gchar *message = g_strdup_printf ( _("Exported files: %d"), export_count );
  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, message );
  g_free ( message );

  return success;
}

static void export_to_common ( VikWindow *vw, VikFileType_t vft, const gchar *extension )
{
  GList *gl = vik_layers_panel_get_all_layers_of_type ( vw->viking_vlp, VIK_LAYER_TRW, TRUE );

  if ( !gl ) {
    a_dialog_info_msg ( GTK_WINDOW(vw), _("Nothing to Export!") );
    return;
  }

  GtkWidget *dialog = gtk_file_chooser_dialog_new ( _("Export to directory"),
                                                    GTK_WINDOW(vw),
                                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                    GTK_STOCK_CANCEL,
                                                    GTK_RESPONSE_REJECT,
                                                    GTK_STOCK_OK,
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL );
  gtk_window_set_transient_for ( GTK_WINDOW(dialog), GTK_WINDOW(vw) );
  gtk_window_set_destroy_with_parent ( GTK_WINDOW(dialog), TRUE );
  gtk_window_set_modal ( GTK_WINDOW(dialog), TRUE );

  gtk_widget_show_all ( dialog );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {
    gchar *dir = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(dialog) );
    gtk_widget_destroy ( dialog );
    if ( dir ) {
      if ( !export_to ( vw, gl, vft, dir, extension ) )
        a_dialog_error_msg ( GTK_WINDOW(vw),_("Could not convert all files") );
      g_free ( dir );
    }
  }
  else
    gtk_widget_destroy ( dialog );

  g_list_free ( gl );
}

static void export_to_gpx ( GtkAction *a, VikWindow *vw )
{
  export_to_common ( vw, FILE_TYPE_GPX, ".gpx" );
}

static void export_to_kml ( GtkAction *a, VikWindow *vw )
{
  export_to_common ( vw, FILE_TYPE_KML, ".kml" );
}

#if !GLIB_CHECK_VERSION(2,26,0)
typedef struct stat GStatBuf;
#endif

static void file_properties_cb ( GtkAction *a, VikWindow *vw )
{
  gchar *message = NULL;
  if ( vw->filename ) {
    if ( g_file_test ( vw->filename, G_FILE_TEST_EXISTS ) ) {
      // Get some timestamp information of the file
      GStatBuf stat_buf;
      if ( g_stat ( vw->filename, &stat_buf ) == 0 ) {
        gchar time_buf[64];
        strftime ( time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime) );
        gchar *size = NULL;
        gint byte_size = stat_buf.st_size;
#if GLIB_CHECK_VERSION(2,30,0)
        size = g_format_size_full ( byte_size, G_FORMAT_SIZE_DEFAULT );
#else
        size = g_format_size_for_display ( byte_size );
#endif
        message = g_strdup_printf ( "%s\n\n%s\n\n%s", vw->filename, time_buf, size );
        g_free (size);
      }
    }
    else
      message = g_strdup ( _("File not accessible") );
  }
  else
    message = g_strdup ( _("No Viking File") );

  // Show the info
  a_dialog_info_msg ( GTK_WINDOW(vw), message );
  g_free ( message );
}

static void my_acquire ( VikWindow *vw, VikDataSourceInterface *datasource )
{
  vik_datasource_mode_t mode = datasource->mode;
  if ( mode == VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT )
    mode = VIK_DATASOURCE_CREATENEWLAYER;
  a_acquire ( vw, vw->viking_vlp, vw->viking_vvp, mode, datasource, NULL, NULL );
}

static void acquire_from_gps ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_gps_interface );
}

static void acquire_from_file ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_file_interface );
}

static void acquire_from_geojson ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_geojson_interface );
}

static void acquire_from_routing ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_routing_interface );
}

#ifdef VIK_CONFIG_OPENSTREETMAP
static void acquire_from_osm ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_osm_interface );
}

static void acquire_from_my_osm ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_osm_my_traces_interface );
}
#endif

#ifdef VIK_CONFIG_GEOCACHES
static void acquire_from_gc ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_gc_interface );
}
#endif

#ifdef VIK_CONFIG_GEOTAG
static void acquire_from_geotag ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_geotag_interface );
}
#endif

#ifdef VIK_CONFIG_GEONAMES
static void acquire_from_wikipedia ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_wikipedia_interface );
}
#endif

static void acquire_from_url ( GtkAction *a, VikWindow *vw )
{
  my_acquire ( vw, &vik_datasource_url_interface );
}

static void goto_default_location( GtkAction *a, VikWindow *vw)
{
  struct LatLon ll;
  ll.lat = a_vik_get_default_lat();
  ll.lon = a_vik_get_default_long();
  vik_viewport_set_center_latlon(vw->viking_vvp, &ll, TRUE);
  vik_layers_panel_emit_update(vw->viking_vlp);
}


static void goto_address( GtkAction *a, VikWindow *vw)
{
  a_vik_goto ( vw, vw->viking_vvp );
  vik_layers_panel_emit_update ( vw->viking_vlp );
}

static void mapcache_flush_cb ( GtkAction *a, VikWindow *vw )
{
  a_mapcache_flush();
}

static void menu_copy_centre_cb ( GtkAction *a, VikWindow *vw )
{
  const VikCoord* coord;
  struct UTM utm;
  gchar *lat = NULL, *lon = NULL;

  coord = vik_viewport_get_center ( vw->viking_vvp );
  vik_coord_to_utm ( coord, &utm );

  gboolean full_format = FALSE;
  a_settings_get_boolean ( VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT, &full_format );

  if ( full_format )
    // Bells & Whistles - may include degrees, minutes and second symbols
    get_location_strings ( vw, utm, &lat, &lon );
  else {
    // Simple x.xx y.yy format
    struct LatLon ll;
    a_coords_utm_to_latlon ( &utm, &ll );
    lat = g_strdup_printf ( "%.6f", ll.lat );
    lon = g_strdup_printf ( "%.6f", ll.lon );
  }

  gchar *msg = g_strdup_printf ( "%s %s", lat, lon );
  g_free (lat);
  g_free (lon);

  a_clipboard_copy ( VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, msg, NULL );

  g_free ( msg );
}

static void layer_defaults_cb ( GtkAction *a, VikWindow *vw )
{
  gchar **texts = g_strsplit ( gtk_action_get_name(a), "Layer", 0 );

  if ( !texts[1] )
    return; // Internally broken :(

  if ( ! a_layer_defaults_show_window ( GTK_WINDOW(vw), texts[1] ) )
    a_dialog_info_msg ( GTK_WINDOW(vw), _("This layer has no configurable properties.") );
  // NB no update needed

  g_strfreev ( texts );
}

static void preferences_change_update ( VikWindow *vw, gpointer data )
{
  // Want to update all TrackWaypoint layers
  GList *layers = vik_layers_panel_get_all_layers_of_type ( vw->viking_vlp, VIK_LAYER_TRW, TRUE );

  if ( !layers )
    return;

  while ( layers ) {
    // Reset the individual waypoints themselves due to the preferences change
    VikTrwLayer *vtl = VIK_TRW_LAYER(layers->data);
    vik_trw_layer_reset_waypoints ( vtl );
    layers = g_list_next ( layers );
  }

  g_list_free ( layers );

  draw_update ( vw );
}

static void preferences_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean wp_icon_size = a_vik_get_use_large_waypoint_icons();

  a_preferences_show_window ( GTK_WINDOW(vw) );

  // Has the waypoint size setting changed?
  if (wp_icon_size != a_vik_get_use_large_waypoint_icons()) {
    // Delete icon indexing 'cache' and so automatically regenerates with the new setting when changed
    clear_garmin_icon_syms ();

    // Update all windows
    g_slist_foreach ( window_list, (GFunc) preferences_change_update, NULL );
  }

  // Ensure TZ Lookup initialized
  if ( a_vik_get_time_ref_frame() == VIK_TIME_REF_WORLD )
    vu_setup_lat_lon_tz_lookup();

  toolbar_apply_settings ( vw->viking_vtb, vw->main_vbox, vw->menu_hbox, TRUE );
}

static void default_location_cb ( GtkAction *a, VikWindow *vw )
{
  /* Simplistic repeat of preference setting
     Only the name & type are important for setting the preference via this 'external' way */
  VikLayerParam pref_lat[] = {
    { VIK_LAYER_NUM_TYPES,
      VIKING_PREFERENCES_NAMESPACE "default_latitude",
      VIK_LAYER_PARAM_DOUBLE,
      VIK_LOCATION_LAT,
      NULL,
      VIK_LAYER_WIDGET_SPINBUTTON,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
    },
  };
  VikLayerParam pref_lon[] = {
    { VIK_LAYER_NUM_TYPES,
      VIKING_PREFERENCES_NAMESPACE "default_longitude",
      VIK_LAYER_PARAM_DOUBLE,
      VIK_LOCATION_LONG,
      NULL,
      VIK_LAYER_WIDGET_SPINBUTTON,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
    },
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

  GtkWidget *msgbox = gtk_message_dialog_new ( GTK_WINDOW(vw),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_NONE,
					       _("Generating image file...") );

  g_signal_connect_swapped (msgbox, "response", G_CALLBACK (gtk_widget_destroy), msgbox);
  // Ensure dialog shown
  gtk_widget_show_all ( msgbox );
  // Try harder...
  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, _("Generating image file...") );
  while ( gtk_events_pending() )
    gtk_main_iteration ();
  // Despite many efforts & variations, GTK on my Linux system doesn't show the actual msgbox contents :(
  // At least the empty box can give a clue something's going on + the statusbar msg...
  // Windows version under Wine OK!

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
  if ( !pixbuf_to_save ) {
    g_warning("Failed to generate internal pixmap size: %d x %d", w, h);
    gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG(msgbox), _("Failed to generate internal image.\n\nTry creating a smaller image.") );
    goto cleanup;
  }

  gdk_pixbuf_save ( pixbuf_to_save, fn, save_as_png ? "png" : "jpeg", &error, NULL );
  if (error)
  {
    g_warning("Unable to write to file %s: %s", fn, error->message );
    gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG(msgbox), _("Failed to generate image file.") );
    g_error_free (error);
  }
  else {
    // Success
    gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG(msgbox), _("Image file generated.") );
  }
  g_object_unref ( G_OBJECT(pixbuf_to_save) );

 cleanup:
  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, "" );
  gtk_dialog_add_button ( GTK_DIALOG(msgbox), GTK_STOCK_OK, GTK_RESPONSE_OK );
  gtk_dialog_run ( GTK_DIALOG(msgbox) ); // Don't care about the result

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
      vik_viewport_set_center_utm ( vw->viking_vvp, &utm, FALSE );

      draw_redraw ( vw );

      /* save buffer as file. */
      pixbuf_to_save = gdk_pixbuf_get_from_drawable ( NULL, GDK_DRAWABLE(vik_viewport_get_pixmap ( vw->viking_vvp )), NULL, 0, 0, 0, 0, w, h);
      gdk_pixbuf_save ( pixbuf_to_save, name_of_file, save_as_png ? "png" : "jpeg", &error, NULL );
      if (error)
      {
        gchar *msg = g_strdup_printf (_("Unable to write to file %s: %s"), name_of_file, error->message );
        vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, msg );
        g_free ( msg );
        g_error_free (error);
      }

      g_object_unref ( G_OBJECT(pixbuf_to_save) );
    }
  }

  vik_viewport_set_center_utm ( vw->viking_vvp, &utm_orig, FALSE );
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

  gint active = gtk_combo_box_get_active ( GTK_COMBO_BOX(pass_along[3]) );
  gdouble zoom = pow (2, active-2 );

  gdouble width_min, width_max, height_min, height_max;
  gint width, height;

  gtk_spin_button_get_range ( width_spin, &width_min, &width_max );
  gtk_spin_button_get_range ( height_spin, &height_min, &height_max );

  /* TODO: support for xzoom and yzoom values */
  width = vik_viewport_get_width ( vw->viking_vvp ) * vik_viewport_get_xmpp ( vw->viking_vvp ) / zoom;
  height = vik_viewport_get_height ( vw->viking_vvp ) * vik_viewport_get_xmpp ( vw->viking_vvp ) / zoom;

  if ( width > width_max || width < width_min || height > height_max || height < height_min )
    a_dialog_info_msg ( GTK_WINDOW(vw), _("Viewable region outside allowable pixel size bounds for image. Clipping width/height values.") );

  gtk_spin_button_set_value ( width_spin, width );
  gtk_spin_button_set_value ( height_spin, height );
}

static void draw_to_image_file_total_area_cb (GtkSpinButton *spinbutton, gpointer *pass_along)
{
  GtkSpinButton *width_spin = GTK_SPIN_BUTTON(pass_along[1]), *height_spin = GTK_SPIN_BUTTON(pass_along[2]);

  gint active = gtk_combo_box_get_active ( GTK_COMBO_BOX(pass_along[3]) );
  gdouble zoom = pow (2, active-2 );

  gchar *label_text;
  gdouble w, h;
  w = gtk_spin_button_get_value(width_spin) * zoom;
  h = gtk_spin_button_get_value(height_spin) * zoom;
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
  case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
    label_text = g_strdup_printf ( _("Total area: %ldm x %ldm (%.3f sq. NM)"), (glong)w, (glong)h, (w*h/(1852.0*1852.0)));
    break;
  default:
    label_text = g_strdup_printf ("Just to keep the compiler happy");
    g_critical("Houston, we've had a problem. distance=%d", dist_units);
  }

  gtk_label_set_text(GTK_LABEL(pass_along[6]), label_text);
  g_free ( label_text );
}

/*
 * Get an allocated filename (or directory as specified)
 */
static gchar* draw_image_filename ( VikWindow *vw, gboolean one_image_only )
{
  gchar *fn = NULL;
  if ( one_image_only )
  {
    // Single file
    if (!vw->save_img_dia) {
      vw->save_img_dia = gtk_file_chooser_dialog_new (_("Save Image"),
                                                      GTK_WINDOW(vw),
                                                      GTK_FILE_CHOOSER_ACTION_SAVE,
                                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                      NULL);

      gchar *cwd = g_get_current_dir();
      if ( cwd ) {
        gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER(vw->save_img_dia), cwd );
        g_free ( cwd );
      }

      GtkFileChooser *chooser = GTK_FILE_CHOOSER ( vw->save_img_dia );
      /* Add filters */
      GtkFileFilter *filter;
      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name ( filter, _("All") );
      gtk_file_filter_add_pattern ( filter, "*" );
      gtk_file_chooser_add_filter ( chooser, filter );

      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name ( filter, _("JPG") );
      gtk_file_filter_add_mime_type ( filter, "image/jpeg");
      gtk_file_chooser_add_filter ( chooser, filter );

      if ( !vw->draw_image_save_as_png )
        gtk_file_chooser_set_filter ( chooser, filter );

      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name ( filter, _("PNG") );
      gtk_file_filter_add_mime_type ( filter, "image/png");
      gtk_file_chooser_add_filter ( chooser, filter );

      if ( vw->draw_image_save_as_png )
        gtk_file_chooser_set_filter ( chooser, filter );

      gtk_window_set_transient_for ( GTK_WINDOW(vw->save_img_dia), GTK_WINDOW(vw) );
      gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->save_img_dia), TRUE );
    }

    if ( gtk_dialog_run ( GTK_DIALOG(vw->save_img_dia) ) == GTK_RESPONSE_ACCEPT ) {
      fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(vw->save_img_dia) );
      if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) )
	if ( ! a_dialog_yes_or_no ( GTK_WINDOW(vw->save_img_dia), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
	  fn = NULL;
    }
    gtk_widget_hide ( vw->save_img_dia );
  }
  else {
    // A directory
    // For some reason this method is only written to work in UTM...
    if ( vik_viewport_get_coord_mode(vw->viking_vvp) != VIK_COORD_UTM ) {
      a_dialog_error_msg ( GTK_WINDOW(vw), _("You must be in UTM mode to use this feature") );
      return fn;
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

    if ( gtk_dialog_run ( GTK_DIALOG(vw->save_img_dir_dia) ) == GTK_RESPONSE_ACCEPT ) {
      fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(vw->save_img_dir_dia) );
    }
    gtk_widget_hide ( vw->save_img_dir_dia );
  }
  return fn;
}

static void draw_to_image_file ( VikWindow *vw, gboolean one_image_only )
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
  GtkWidget *zoom_label, *zoom_combo;
  GtkWidget *total_size_label;

  /* only used if (!one_image_only) */
  GtkWidget *tiles_width_spin = NULL, *tiles_height_spin = NULL;

  width_label = gtk_label_new ( _("Width (pixels):") );
  width_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vw->draw_image_width, 10, 50000, 10, 100, 0 )), 10, 0 );
  height_label = gtk_label_new ( _("Height (pixels):") );
  height_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vw->draw_image_height, 10, 50000, 10, 100, 0 )), 10, 0 );
#ifdef WINDOWS
  GtkWidget *win_warning_label = gtk_label_new ( _("WARNING: USING LARGE IMAGES OVER 10000x10000\nMAY CRASH THE PROGRAM!") );
#endif
  zoom_label = gtk_label_new ( _("Zoom (meters per pixel):") );
  /* TODO: separate xzoom and yzoom factors */
  zoom_combo = create_zoom_combo_all_levels();

  gdouble mpp = vik_viewport_get_xmpp(vw->viking_vvp);
  gint active = 2 + round ( log (mpp) / log (2) );

  // Can we not hard code size here?
  if ( active > 17 )
    active = 17;
  if ( active < 0 )
    active = 0;
  gtk_combo_box_set_active ( GTK_COMBO_BOX(zoom_combo), active );

  total_size_label = gtk_label_new ( NULL );

  current_window_button = gtk_button_new_with_label ( _("Area in current viewable window") );
  current_window_pass_along [0] = vw;
  current_window_pass_along [1] = width_spin;
  current_window_pass_along [2] = height_spin;
  current_window_pass_along [3] = zoom_combo;
  current_window_pass_along [4] = NULL; /* used for one_image_only != 1 */
  current_window_pass_along [5] = NULL;
  current_window_pass_along [6] = total_size_label;
  g_signal_connect ( G_OBJECT(current_window_button), "button_press_event", G_CALLBACK(draw_to_image_file_current_window_cb), current_window_pass_along );

  png_radio = gtk_radio_button_new_with_label ( NULL, _("Save as PNG") );
  jpeg_radio = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(png_radio), _("Save as JPEG") );

  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), png_radio, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), jpeg_radio, FALSE, FALSE, 0);

  if ( ! vw->draw_image_save_as_png )
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(jpeg_radio), TRUE );

  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), width_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), width_spin, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), height_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), height_spin, FALSE, FALSE, 0);
#ifdef WINDOWS
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), win_warning_label, FALSE, FALSE, 0);
#endif
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), current_window_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), zoom_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), zoom_combo, FALSE, FALSE, 0);

  if ( ! one_image_only )
  {
    GtkWidget *tiles_width_label, *tiles_height_label;

    tiles_width_label = gtk_label_new ( _("East-west image tiles:") );
    tiles_width_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( 5, 1, 10, 1, 100, 0 )), 1, 0 );
    tiles_height_label = gtk_label_new ( _("North-south image tiles:") );
    tiles_height_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( 5, 1, 10, 1, 100, 0 )), 1, 0 );
    gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_width_label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_width_spin, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_height_label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_height_spin, FALSE, FALSE, 0);

    current_window_pass_along [4] = tiles_width_spin;
    current_window_pass_along [5] = tiles_height_spin;
    g_signal_connect ( G_OBJECT(tiles_width_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
    g_signal_connect ( G_OBJECT(tiles_height_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
  }
  gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), total_size_label, FALSE, FALSE, 0);
  g_signal_connect ( G_OBJECT(width_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
  g_signal_connect ( G_OBJECT(height_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );
  g_signal_connect ( G_OBJECT(zoom_combo), "changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along );

  draw_to_image_file_total_area_cb ( NULL, current_window_pass_along ); /* set correct size info now */

  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );

  gtk_widget_show_all ( gtk_dialog_get_content_area(GTK_DIALOG(dialog)) );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    gtk_widget_hide ( GTK_WIDGET(dialog) );

    gchar *fn = draw_image_filename ( vw, one_image_only );
    if ( !fn )
      return;

    gint active_z = gtk_combo_box_get_active ( GTK_COMBO_BOX(zoom_combo) );
    gdouble zoom = pow (2, active_z-2 );

    if ( one_image_only )
      save_image_file ( vw, fn, 
                      vw->draw_image_width = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(width_spin) ),
                      vw->draw_image_height = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(height_spin) ),
                      zoom,
                      vw->draw_image_save_as_png = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(png_radio) ) );
    else {
      // NB is in UTM mode ATM
      save_image_dir ( vw, fn,
                       vw->draw_image_width = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(width_spin) ),
                       vw->draw_image_height = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(height_spin) ),
                       zoom,
                       vw->draw_image_save_as_png = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(png_radio) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(tiles_width_spin) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(tiles_height_spin) ) );
    }

    g_free ( fn );
  }
  gtk_widget_destroy ( GTK_WIDGET(dialog) );
}


static void draw_to_image_file_cb ( GtkAction *a, VikWindow *vw )
{
  draw_to_image_file ( vw, TRUE );
}

static void draw_to_image_dir_cb ( GtkAction *a, VikWindow *vw )
{
  draw_to_image_file ( vw, FALSE );
}

static void print_cb ( GtkAction *a, VikWindow *vw )
{
  a_print(vw, vw->viking_vvp);
}

/* really a misnomer: changes coord mode (actual coordinates) AND/OR draw mode (viewport only) */
static void window_change_coord_mode_cb ( GtkAction *old_a, GtkAction *a, VikWindow *vw )
{
  const gchar *name = gtk_action_get_name(a);
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, name );
  if ( tbutton )
    gtk_toggle_tool_button_set_active ( tbutton, TRUE );

  VikViewportDrawMode drawmode;
  if (!g_strcmp0(name, "ModeUTM")) {
    drawmode = VIK_VIEWPORT_DRAWMODE_UTM;
  }
  else if (!g_strcmp0(name, "ModeLatLon")) {
    drawmode = VIK_VIEWPORT_DRAWMODE_LATLON;
  }
  else if (!g_strcmp0(name, "ModeExpedia")) {
    drawmode = VIK_VIEWPORT_DRAWMODE_EXPEDIA;
  }
  else if (!g_strcmp0(name, "ModeMercator")) {
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

static void toggle_draw_scale ( GtkAction *a, VikWindow *vw )
{
  gboolean state = !vik_viewport_get_draw_scale ( vw->viking_vvp );
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowScale" );
  if ( !check_box )
    return;
  gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), state );
  vik_viewport_set_draw_scale ( vw->viking_vvp, state );
  draw_update ( vw );
}

static void toggle_draw_centermark ( GtkAction *a, VikWindow *vw )
{
  gboolean state = !vik_viewport_get_draw_centermark ( vw->viking_vvp );
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowCenterMark" );
  if ( !check_box )
    return;
  gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), state );
  vik_viewport_set_draw_centermark ( vw->viking_vvp, state );
  draw_update ( vw );
}

static void toggle_draw_highlight ( GtkAction *a, VikWindow *vw )
{
  gboolean state = !vik_viewport_get_draw_highlight ( vw->viking_vvp );
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ShowHighlight" );
  if ( !check_box )
    return;
  gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), state );
  vik_viewport_set_draw_highlight ( vw->viking_vvp, state );
  draw_update ( vw );
}

static void set_bg_color ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *colorsd = gtk_color_selection_dialog_new ( _("Choose a background color") );
  GdkColor *color = vik_viewport_get_background_gdkcolor ( vw->viking_vvp );
  gtk_color_selection_set_previous_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color );
  gtk_color_selection_set_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color );
  if ( gtk_dialog_run ( GTK_DIALOG(colorsd) ) == GTK_RESPONSE_OK )
  {
    gtk_color_selection_get_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color );
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
  gtk_color_selection_set_previous_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color );
  gtk_color_selection_set_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color );
  if ( gtk_dialog_run ( GTK_DIALOG(colorsd) ) == GTK_RESPONSE_OK )
  {
    gtk_color_selection_get_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color );
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
  { "Export",    GTK_STOCK_CONVERT,      N_("_Export All"),               NULL,         N_("Export All TrackWaypoint Layers"),              (GCallback)NULL                  },
  { "ExportGPX", NULL,                   N_("_GPX..."),           	      NULL,         N_("Export as GPX"),                                (GCallback)export_to_gpx         },
  { "Acquire",   GTK_STOCK_GO_DOWN,      N_("A_cquire"),                  NULL,         NULL,                                               (GCallback)NULL },
  { "AcquireGPS",   NULL,                N_("From _GPS..."),           	  NULL,         N_("Transfer data from a GPS device"),              (GCallback)acquire_from_gps      },
  { "AcquireGPSBabel",   NULL,                N_("Import File With GPS_Babel..."),           	  NULL,         N_("Import file via GPSBabel converter"),              (GCallback)acquire_from_file      },
  { "AcquireRouting",   NULL,             N_("_Directions..."),     NULL,         N_("Get driving directions"),           (GCallback)acquire_from_routing   },
#ifdef VIK_CONFIG_OPENSTREETMAP
  { "AcquireOSM",   NULL,                 N_("_OSM Traces..."),    	  NULL,         N_("Get traces from OpenStreetMap"),            (GCallback)acquire_from_osm       },
  { "AcquireMyOSM", NULL,                 N_("_My OSM Traces..."),    	  NULL,         N_("Get Your Own Traces from OpenStreetMap"),   (GCallback)acquire_from_my_osm    },
#endif
#ifdef VIK_CONFIG_GEOCACHES
  { "AcquireGC",   NULL,                 N_("Geo_caches..."),    	  NULL,         N_("Get Geocaches from geocaching.com"),            (GCallback)acquire_from_gc       },
#endif
#ifdef VIK_CONFIG_GEOTAG
  { "AcquireGeotag", NULL,               N_("From Geotagged _Images..."), NULL,         N_("Create waypoints from geotagged images"),       (GCallback)acquire_from_geotag   },
#endif
  { "AcquireURL", NULL,                  N_("From _URL..."),              NULL,         N_("Get a file from a URL"),                        (GCallback)acquire_from_url },
#ifdef VIK_CONFIG_GEONAMES
  { "AcquireWikipedia", NULL,            N_("From _Wikipedia Waypoints"), NULL,         N_("Create waypoints from Wikipedia items in the current view"), (GCallback)acquire_from_wikipedia },
#endif
  { "Save",      GTK_STOCK_SAVE,         N_("_Save"),                         "<control>S", N_("Save the file"),                                (GCallback)save_file             },
  { "SaveAs",    GTK_STOCK_SAVE_AS,      N_("Save _As..."),                      NULL,  N_("Save the file under different name"),           (GCallback)save_file_as          },
  { "FileProperties", NULL,              N_("Properties..."),                    NULL,  N_("File Properties"),                              (GCallback)file_properties_cb },
  { "GenImg",    GTK_STOCK_CLEAR,        N_("_Generate Image File..."),          NULL,  N_("Save a snapshot of the workspace into a file"), (GCallback)draw_to_image_file_cb },
  { "GenImgDir", GTK_STOCK_DND_MULTIPLE, N_("Generate _Directory of Images..."), NULL,  N_("Generate _Directory of Images"),                (GCallback)draw_to_image_dir_cb },
  { "Print",    GTK_STOCK_PRINT,        N_("_Print..."),          NULL,         N_("Print maps"), (GCallback)print_cb },
  { "Exit",      GTK_STOCK_QUIT,         N_("E_xit"),                         "<control>W", N_("Exit the program"),                             (GCallback)window_close          },
  { "SaveExit",  GTK_STOCK_QUIT,         N_("Save and Exit"),                 NULL, N_("Save and Exit the program"),                             (GCallback)save_file_and_exit          },

  { "GoBack",    GTK_STOCK_GO_BACK,      N_("Go to the Pre_vious Location"),  NULL,         N_("Go to the previous location"),              (GCallback)draw_goto_back_and_forth },
  { "GoForward", GTK_STOCK_GO_FORWARD,   N_("Go to the _Next Location"),      NULL,         N_("Go to the next location"),                  (GCallback)draw_goto_back_and_forth },
  { "GotoDefaultLocation", GTK_STOCK_HOME, N_("Go to the _Default Location"),  NULL,         N_("Go to the default location"),                     (GCallback)goto_default_location },
  { "GotoSearch", GTK_STOCK_JUMP_TO,     N_("Go to _Location..."),    	      NULL,         N_("Go to address/place using text search"),        (GCallback)goto_address       },
  { "GotoLL",    GTK_STOCK_JUMP_TO,      N_("_Go to Lat/Lon..."),           NULL,         N_("Go to arbitrary lat/lon coordinate"),         (GCallback)draw_goto_cb          },
  { "GotoUTM",   GTK_STOCK_JUMP_TO,      N_("Go to UTM..."),                  NULL,         N_("Go to arbitrary UTM coordinate"),               (GCallback)draw_goto_cb          },
  { "Refresh",   GTK_STOCK_REFRESH,      N_("_Refresh"),                      "F5",         N_("Refresh any maps displayed"),               (GCallback)draw_refresh_cb       },
  { "SetHLColor",GTK_STOCK_SELECT_COLOR, N_("Set _Highlight Color..."),       NULL,         N_("Set Highlight Color"),                      (GCallback)set_highlight_color },
  { "SetBGColor",GTK_STOCK_SELECT_COLOR, N_("Set Bac_kground Color..."),      NULL,         N_("Set Background Color"),                     (GCallback)set_bg_color },
  { "ZoomIn",    GTK_STOCK_ZOOM_IN,      N_("Zoom _In"),                  "<control>plus",  N_("Zoom In"),                                  (GCallback)draw_zoom_cb },
  { "ZoomOut",   GTK_STOCK_ZOOM_OUT,     N_("Zoom _Out"),                 "<control>minus", N_("Zoom Out"),                                 (GCallback)draw_zoom_cb },
  { "ZoomTo",    GTK_STOCK_ZOOM_FIT,     N_("Zoom _To..."),               "<control>Z",     N_("Zoom To"),                                  (GCallback)zoom_to_cb },
  { "PanNorth",  NULL,                   N_("Pan _North"),                "<control>Up",    NULL,                                           (GCallback)draw_pan_cb },
  { "PanEast",   NULL,                   N_("Pan _East"),                 "<control>Right", NULL,                                           (GCallback)draw_pan_cb },
  { "PanSouth",  NULL,                   N_("Pan _South"),                "<control>Down",  NULL,                                           (GCallback)draw_pan_cb },
  { "PanWest",   NULL,                   N_("Pan _West"),                 "<control>Left",  NULL,                                           (GCallback)draw_pan_cb },
  { "BGJobs",    GTK_STOCK_EXECUTE,      N_("Background _Jobs"),              NULL,         N_("Background Jobs"),                          (GCallback)a_background_show_window },

  { "Cut",       GTK_STOCK_CUT,          N_("Cu_t"),                          NULL,         N_("Cut selected layer"),                       (GCallback)menu_cut_layer_cb     },
  { "Copy",      GTK_STOCK_COPY,         N_("_Copy"),                         NULL,         N_("Copy selected layer"),                      (GCallback)menu_copy_layer_cb    },
  { "Paste",     GTK_STOCK_PASTE,        N_("_Paste"),                        NULL,         N_("Paste layer into selected container layer or otherwise above selected layer"), (GCallback)menu_paste_layer_cb },
  { "Delete",    GTK_STOCK_DELETE,       N_("_Delete"),                       NULL,         N_("Remove selected layer"),                    (GCallback)menu_delete_layer_cb  },
  { "DeleteAll", NULL,                   N_("Delete All"),                    NULL,         NULL,                                           (GCallback)clear_cb              },
  { "CopyCentre",NULL,                   N_("Copy Centre _Location"),     "<control>h",     NULL,                                           (GCallback)menu_copy_centre_cb   },
  { "MapCacheFlush",NULL,                N_("_Flush Map Cache"),              NULL,         NULL,                                           (GCallback)mapcache_flush_cb     },
  { "SetDefaultLocation", GTK_STOCK_GO_FORWARD, N_("_Set the Default Location"), NULL, N_("Set the Default Location to the current position"),(GCallback)default_location_cb },
  { "Preferences",GTK_STOCK_PREFERENCES, N_("_Preferences"),                  NULL,         N_("Program Preferences"),                      (GCallback)preferences_cb },
  { "LayerDefaults",GTK_STOCK_PROPERTIES, N_("_Layer Defaults"),             NULL,         NULL,                                           NULL },
  { "Properties",GTK_STOCK_PROPERTIES,   N_("_Properties"),                   NULL,         N_("Layer Properties"),                         (GCallback)menu_properties_cb },

  { "HelpEntry", GTK_STOCK_HELP,         N_("_Help"),                         "F1",         N_("Help"),                                     (GCallback)help_help_cb },
  { "About",     GTK_STOCK_ABOUT,        N_("_About"),                        NULL,         N_("About"),                                    (GCallback)help_about_cb },
};

static GtkActionEntry debug_entries[] = {
  { "MapCacheInfo", NULL,                "_Map Cache Info",                   NULL,         NULL,                                           (GCallback)help_cache_info_cb    },
  { "BackForwardInfo", NULL,             "_Back/Forward Info",                NULL,         NULL,                                           (GCallback)back_forward_info_cb  },
};

static GtkActionEntry entries_gpsbabel[] = {
  { "ExportKML", NULL,                   N_("_KML..."),           	      NULL,         N_("Export as KML"),                                (GCallback)export_to_kml },
};

static GtkActionEntry entries_geojson[] = {
  { "AcquireGeoJSON",   NULL,            N_("Import Geo_JSON File..."),   NULL,         N_("Import GeoJSON file"),                          (GCallback)acquire_from_geojson },
};

/* Radio items */
static GtkRadioActionEntry mode_entries[] = {
  { "ModeUTM",         NULL,         N_("_UTM Mode"),               "<control>u", NULL, VIK_VIEWPORT_DRAWMODE_UTM },
  { "ModeExpedia",     NULL,         N_("_Expedia Mode"),           "<control>e", NULL, VIK_VIEWPORT_DRAWMODE_EXPEDIA },
  { "ModeMercator",    NULL,         N_("_Mercator Mode"),          "<control>m", NULL, VIK_VIEWPORT_DRAWMODE_MERCATOR },
  { "ModeLatLon",      NULL,         N_("Lat_/Lon Mode"),           "<control>l", NULL, VIK_VIEWPORT_DRAWMODE_LATLON },
};

static GtkToggleActionEntry toggle_entries[] = {
  { "ShowScale",      NULL,                 N_("Show _Scale"),               "<shift>F5",  N_("Show Scale"),                              (GCallback)toggle_draw_scale, TRUE },
  { "ShowCenterMark", NULL,                 N_("Show _Center Mark"),         "F6",         N_("Show Center Mark"),                        (GCallback)toggle_draw_centermark, TRUE },
  { "ShowHighlight",  GTK_STOCK_UNDERLINE,  N_("Show _Highlight"),           "F7",         N_("Show Highlight"),                          (GCallback)toggle_draw_highlight, TRUE },
  { "FullScreen",     GTK_STOCK_FULLSCREEN, N_("_Full Screen"),              "F11",        N_("Activate full screen mode"),               (GCallback)full_screen_cb, FALSE },
  { "ViewSidePanel",  GTK_STOCK_INDEX,      N_("Show Side _Panel"),          "F9",         N_("Show Side Panel"),                         (GCallback)view_side_panel_cb, TRUE },
  { "ViewStatusBar",  NULL,                 N_("Show Status_bar"),           "F12",        N_("Show Statusbar"),                          (GCallback)view_statusbar_cb, TRUE },
  { "ViewToolbar",    NULL,                 N_("Show _Toolbar"),             "F3",         N_("Show Toolbar"),                            (GCallback)view_toolbar_cb, TRUE },
  { "ViewMainMenu",   NULL,                 N_("Show _Menu"),                "F4",         N_("Show Menu"),                               (GCallback)view_main_menu_cb, TRUE },
};

// This must match the toggle entries order above
static gpointer toggle_entries_toolbar_cb[] = {
  (GCallback)tb_set_draw_scale,
  (GCallback)tb_set_draw_centermark,
  (GCallback)tb_set_draw_highlight,
  (GCallback)tb_full_screen_cb,
  (GCallback)tb_view_side_panel_cb,
  (GCallback)tb_view_statusbar_cb,
  (GCallback)tb_view_toolbar_cb,
  (GCallback)tb_view_main_menu_cb,
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

  toolbar_action_tool_entry_register ( window->viking_vtb, &pan_tool.radioActionEntry );
  toolbar_action_tool_entry_register ( window->viking_vtb, &zoom_tool.radioActionEntry );
  toolbar_action_tool_entry_register ( window->viking_vtb, &ruler_tool.radioActionEntry );
  toolbar_action_tool_entry_register ( window->viking_vtb, &select_tool.radioActionEntry );

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
  if ( vik_debug ) {
    if ( gtk_ui_manager_add_ui_from_string ( uim,
         "<ui><menubar name='MainMenu'><menu action='Help'>"
           "<menuitem action='MapCacheInfo'/>"
           "<menuitem action='BackForwardInfo'/>"
         "</menu></menubar></ui>",
         -1, NULL ) ) {
      gtk_action_group_add_actions (action_group, debug_entries, G_N_ELEMENTS (debug_entries), window);
    }
  }

  for ( i=0; i < G_N_ELEMENTS (entries); i++ ) {
    if ( entries[i].callback )
      toolbar_action_entry_register ( window->viking_vtb, &entries[i] );
  }

  if ( G_N_ELEMENTS (toggle_entries) !=  G_N_ELEMENTS (toggle_entries_toolbar_cb) ) {
    g_print ( "Broken entries definitions\n" );
    exit (1);
  }
  for ( i=0; i < G_N_ELEMENTS (toggle_entries); i++ ) {
    if ( toggle_entries_toolbar_cb[i] )
      toolbar_action_toggle_entry_register ( window->viking_vtb, &toggle_entries[i], toggle_entries_toolbar_cb[i] );
  }

  for ( i=0; i < G_N_ELEMENTS (mode_entries); i++ ) {
    toolbar_action_mode_entry_register ( window->viking_vtb, &mode_entries[i] );
  }

  // Use this to see if GPSBabel is available:
  if ( a_babel_available () ) {
	// If going to add more entries then might be worth creating a menu_gpsbabel.xml.h file
	if ( gtk_ui_manager_add_ui_from_string ( uim,
	  "<ui><menubar name='MainMenu'><menu action='File'><menu action='Export'><menuitem action='ExportKML'/></menu></menu></menubar></ui>",
	  -1, &error ) )
      gtk_action_group_add_actions ( action_group, entries_gpsbabel, G_N_ELEMENTS (entries_gpsbabel), window );
  }

  // GeoJSON import capability
  if ( g_find_program_in_path ( a_geojson_program_import() ) ) {
    if ( gtk_ui_manager_add_ui_from_string ( uim,
         "<ui><menubar name='MainMenu'><menu action='File'><menu action='Acquire'><menuitem action='AcquireGeoJSON'/></menu></menu></menubar></ui>",
         -1, &error ) )
      gtk_action_group_add_actions ( action_group, entries_geojson, G_N_ELEMENTS (entries_geojson), window );
  }

  icon_factory = gtk_icon_factory_new ();
  gtk_icon_factory_add_default (icon_factory); 

  register_vik_icons(icon_factory);

  // Copy the tool RadioActionEntries out of the main Window structure into an extending array 'tools'
  //  so that it can be applied to the UI in one action group add function call below
  ntools = 0;
  for (i=0; i<window->vt->n_tools; i++) {
      tools = g_renew(GtkRadioActionEntry, tools, ntools+1);
      radio = &tools[ntools];
      ntools++;
      *radio = window->vt->tools[i].ti.radioActionEntry;
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
    action.label = g_strdup_printf( _("New _%s Layer"), vik_layer_get_interface(i)->name);
    action.accelerator = vik_layer_get_interface(i)->accelerator;
    action.tooltip = NULL;
    action.callback = (GCallback)menu_addlayer_cb;
    gtk_action_group_add_actions(action_group, &action, 1, window);

    g_free ( (gchar*)action.label );

    if ( vik_layer_get_interface(i)->tools_count ) {
      gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools/", vik_layer_get_interface(i)->name, NULL, GTK_UI_MANAGER_SEPARATOR, FALSE);
    }

    // Further tool copying for to apply to the UI, also apply menu UI setup
    for ( j = 0; j < vik_layer_get_interface(i)->tools_count; j++ ) {
      tools = g_renew(GtkRadioActionEntry, tools, ntools+1);
      radio = &tools[ntools];
      ntools++;
      
      gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools", 
			    vik_layer_get_interface(i)->tools[j].radioActionEntry.label,
			    vik_layer_get_interface(i)->tools[j].radioActionEntry.name,
			    GTK_UI_MANAGER_MENUITEM, FALSE);

      toolbox_add_tool(window->vt, &(vik_layer_get_interface(i)->tools[j]), i);
      toolbar_action_tool_entry_register ( window->viking_vtb, &(vik_layer_get_interface(i)->tools[j].radioActionEntry) );

      *radio = vik_layer_get_interface(i)->tools[j].radioActionEntry;
      // Overwrite with actual number to use
      radio->value = ntools;
    }

    GtkActionEntry action_dl;
    gchar *layername = g_strdup_printf ( "Layer%s", vik_layer_get_interface(i)->fixed_layer_name );
    gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Edit/LayerDefaults",
			  vik_layer_get_interface(i)->name,
			  layername,
			  GTK_UI_MANAGER_MENUITEM, FALSE);
    g_free (layername);

    // For default layers use action names of the form 'Layer<LayerName>'
    // This is to avoid clashing with just the layer name used above for the tool actions
    action_dl.name = g_strconcat("Layer", vik_layer_get_interface(i)->fixed_layer_name, NULL);
    action_dl.stock_id = NULL;
    action_dl.label = g_strconcat("_", vik_layer_get_interface(i)->name, "...", NULL); // Prepend marker for keyboard accelerator
    action_dl.accelerator = NULL;
    action_dl.tooltip = NULL;
    action_dl.callback = (GCallback)layer_defaults_cb;
    gtk_action_group_add_actions(action_group, &action_dl, 1, window);
    g_free ( (gchar*)action_dl.name );
    g_free ( (gchar*)action_dl.label );
  }
  g_object_unref (icon_factory);

  gtk_action_group_add_radio_actions(action_group, tools, ntools, 0, (GCallback)menu_cb, window);
  g_free(tools);

  gtk_ui_manager_insert_action_group (uim, action_group, 0);

  for (i=0; i<VIK_LAYER_NUM_TYPES; i++) {
    for ( j = 0; j < vik_layer_get_interface(i)->tools_count; j++ ) {
      GtkAction *action = gtk_action_group_get_action(action_group,
			    vik_layer_get_interface(i)->tools[j].radioActionEntry.name);
      g_object_set(action, "sensitive", FALSE, NULL);
    }
  }

  // This is done last so we don't need to track the value of mid anymore
  vik_ext_tools_add_action_items ( window, window->uim, action_group, mid );

  window->action_group = action_group;

  accel_group = gtk_ui_manager_get_accel_group (uim);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  gtk_ui_manager_ensure_update (uim);
  
  setup_recent_files(window);
}


// TODO - add method to add tool icons defined from outside this file
//  and remove the reverse dependency on icon definition from this file
static struct { 
  const GdkPixdata *data;
  gchar *stock_id;
} stock_icons[] = {
  { &mover_22_pixbuf,		"vik-icon-pan"      },
  { &zoom_18_pixbuf,		"vik-icon-zoom"     },
  { &ruler_18_pixbuf,		"vik-icon-ruler"    },
  { &select_18_pixbuf,		"vik-icon-select"   },
  { &vik_new_route_18_pixbuf,   "vik-icon-Create Route"     },
  { &route_finder_18_pixbuf,	"vik-icon-Route Finder"     },
  { &demdl_18_pixbuf,		"vik-icon-DEM Download"     },
  { &showpic_18_pixbuf,		"vik-icon-Show Picture"     },
  { &addtr_18_pixbuf,		"vik-icon-Create Track"     },
  { &edtr_18_pixbuf,		"vik-icon-Edit Trackpoint"  },
  { &addwp_18_pixbuf,		"vik-icon-Create Waypoint"  },
  { &edwp_18_pixbuf,		"vik-icon-Edit Waypoint"    },
  { &geozoom_18_pixbuf,		"vik-icon-Georef Zoom Tool" },
  { &geomove_18_pixbuf,		"vik-icon-Georef Move Map"  },
  { &mapdl_18_pixbuf,		"vik-icon-Maps Download"    },
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
  // Set highlight thickness
  vik_viewport_set_highlight_thickness ( vw->viking_vvp, vik_trw_layer_get_property_tracks_line_thickness (vw->containing_vtl) );
}

GHashTable *vik_window_get_selected_tracks ( VikWindow *vw )
{
  return vw->selected_tracks;
}

void vik_window_set_selected_tracks ( VikWindow *vw, GHashTable *ght, gpointer vtl )
{
  vw->selected_tracks = ght;
  vw->containing_vtl  = vtl;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_track     = NULL;
  vw->selected_waypoint  = NULL;
  vw->selected_waypoints = NULL;
  // Set highlight thickness
  vik_viewport_set_highlight_thickness ( vw->viking_vvp, vik_trw_layer_get_property_tracks_line_thickness (vw->containing_vtl) );
}

gpointer vik_window_get_selected_track ( VikWindow *vw )
{
  return vw->selected_track;
}

void vik_window_set_selected_track ( VikWindow *vw, gpointer *vt, gpointer vtl )
{
  vw->selected_track = vt;
  vw->containing_vtl = vtl;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_tracks    = NULL;
  vw->selected_waypoint  = NULL;
  vw->selected_waypoints = NULL;
  // Set highlight thickness
  vik_viewport_set_highlight_thickness ( vw->viking_vvp, vik_trw_layer_get_property_tracks_line_thickness (vw->containing_vtl) );
}

GHashTable *vik_window_get_selected_waypoints ( VikWindow *vw )
{
  return vw->selected_waypoints;
}

void vik_window_set_selected_waypoints ( VikWindow *vw, GHashTable *ght, gpointer vtl )
{
  vw->selected_waypoints = ght;
  vw->containing_vtl     = vtl;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_track     = NULL;
  vw->selected_tracks    = NULL;
  vw->selected_waypoint  = NULL;
}

gpointer vik_window_get_selected_waypoint ( VikWindow *vw )
{
  return vw->selected_waypoint;
}

void vik_window_set_selected_waypoint ( VikWindow *vw, gpointer *vwp, gpointer vtl )
{
  vw->selected_waypoint = vwp;
  vw->containing_vtl    = vtl;
  /* Clear others */
  vw->selected_vtl       = NULL;
  vw->selected_track     = NULL;
  vw->selected_tracks    = NULL;
  vw->selected_waypoints = NULL;
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

/**
 * May return NULL if the window no longer exists
 */
GThread *vik_window_get_thread ( VikWindow *vw )
{
  if ( vw )
    return vw->thread;
  return NULL;
}
