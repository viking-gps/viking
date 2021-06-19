/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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
#include "background.h"
#include "logging.h"
#include "acquire.h"
#include "datasources.h"
#include "geojson.h"
#include "vikgoto.h"
#include "dems.h"
#include "mapcache.h"
#include "print.h"
#include "toolbar.h"
#include "viklayer_defaults.h"
#include "icons/icons.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "garminsymbols.h"
#include "vikmapslayer.h"
#include "vikrouting.h"
#include "geonamessearch.h"
#include "dir.h"
#include "kmz.h"
#ifdef HAVE_LIBGEOCLUE_2
#include "libgeoclue.h"
#endif
#include "viktrwlayer.h"
#include "viktrwlayer_propwin.h"

#include <ctype.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

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

// The last used directories
static gchar *last_folder_images_uri = NULL;

static void window_finalize ( GObject *gob );
static GObjectClass *parent_class;

static void window_set_filename ( VikWindow *vw, const gchar *filename );
static const gchar *window_get_filename ( VikWindow *vw );

static VikWindow *window_new ();

static void draw_update ( VikWindow *vw );

static void newwindow_cb ( GtkAction *a, VikWindow *vw );

// Signals
static void destroy_window ( GtkWidget *widget,
                             gpointer   data );

/* Drawing & stuff */

static gboolean delete_event( VikWindow *vw );

static gboolean key_press_event( VikWindow *vw, GdkEventKey *event, gpointer data );
static gboolean key_release_event( VikWindow *vw, GdkEventKey *event, gpointer data );
static gboolean key_press_event_vlp ( VikWindow *vw, GdkEventKey *event, gpointer data );
static gboolean key_release_event_vlp ( VikWindow *vw, GdkEventKey *event, gpointer data );

static void center_changed_cb ( VikWindow *vw );
static gboolean window_configure_event ( VikWindow *vw, GdkEventConfigure *event, gpointer user_data );
static gboolean draw_sync ( VikWindow *vw );
static void draw_redraw ( VikWindow *vw );
static gboolean draw_scroll  ( VikWindow *vw, GdkEventScroll *event );
static gboolean draw_click  ( VikWindow *vw, GdkEventButton *event );
static gboolean draw_release ( VikWindow *vw, GdkEventButton *event );
static gboolean draw_mouse_motion ( VikWindow *vw, GdkEventMotion *event );
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
  gpointer state; // Data used by the tool - often tool_ed_t*
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

void tool_edit_destroy ( tool_ed_t *te )
{
#if !GTK_CHECK_VERSION (3,0,0)
  if ( te->pixmap )
    g_object_unref ( G_OBJECT ( te->pixmap ) );
#endif
  g_free ( te );
}

tool_ed_t* tool_edit_create ( VikWindow *vw, VikViewport *vvp )
{
  tool_ed_t *te = g_new0(tool_ed_t, 1);
  te->vw = vw;
  te->vvp = vvp;
  return te;
}

void tool_edit_remove_image ( tool_ed_t *te )
{
  // Have to manually remove in GTK3
  vik_viewport_surface_tool_destroy ( te->vvp );
#if GTK_CHECK_VERSION (3,0,0)
  te->gc = NULL;
#endif
}

/* ui creation */
static void window_create_ui( VikWindow *window );
static void register_vik_icons (GtkIconFactory *icon_factory);

/* i/o */
static void load_file ( GtkAction *a, VikWindow *vw );
static gboolean save_file_as ( GtkAction *a, VikWindow *vw );
static gboolean save_file ( GtkAction *a, VikWindow *vw );
static gboolean save_file_and_exit ( GtkAction *a, VikWindow *vw );
static gboolean window_save ( VikWindow *vw, VikAggregateLayer *agg, gchar *filename );

struct _VikWindow {
  GtkWindow gtkwindow;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  gdouble vpaned_pc; // A percentage: 0.0..1.0 (from the top of the screen)
  gboolean vpaned_shown;
  VikViewport *viking_vvp;
  VikLayersPanel *viking_vlp;
  VikStatusbar *viking_vs;
  VikToolbar *viking_vtb;
  GtkWidget *graphs;
  gpointer graphs_widgets; // viktrwlayer_propwin.c : _propwidgets

  guint sbiu_id; // StatusBar Idle Update Id

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
  gboolean show_track_graphs;
  gboolean show_statusbar;
  gboolean show_toolbar;
  gboolean show_main_menu;
  gboolean show_side_panel_buttons;
  gboolean show_side_panel_tabs;
  gboolean show_side_panel_calendar;
  gboolean show_side_panel_goto;
  gboolean show_side_panel_stats;
  gboolean show_side_panel_splits;

  gboolean select_move;
  gboolean select_double_click;
  guint select_double_click_button;
  gboolean select_pan;
  gboolean deselect_on_release;
  guint deselect_id;
  guint show_menu_id;
  GdkEventButton select_event;
  gboolean pan_move_middle;
  gboolean pan_move;
  gint pan_x, pan_y;
  gint delayed_pan_x, delayed_pan_y; // Temporary storage
  gboolean single_click_pending;
  guint pending_draw_id;
  guint move_scroll_timeout;
  guint zoom_scroll_timeout;
  gdouble pinch_gesture_factor;

  guint draw_image_width, draw_image_height;
  gboolean draw_image_save_as_png;

  gchar *filename;
  gboolean modified;
  VikLoadType_t loaded_type;

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
  VikTrack *selected_track;
  GHashTable *selected_waypoints;
  gpointer selected_waypoint; /* notionally VikWaypoint */
  /* only use for individual track or waypoint */
  /* For track(s) & waypoint(s) it is the layer they are in - this helps refering to the individual item easier */
  gpointer containing_vtl; /* notionally VikTrwLayer */

  gdouble pinch_zoom_begin;
  gdouble pinch_zoom_last;
};

enum {
 TOOL_PAN = 0,
 TOOL_ZOOM,
 TOOL_RULER,
 TOOL_SELECT,
 TOOL_LAYER,
 NUMBER_OF_TOOLS
};

// Unclear what the point of using signals are,
// Since we don't use any features of signalling
enum {
  VW_NEWWINDOW_SIGNAL,
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
  VikWindow *vw;
} statusbar_idle_data;

/**
 * For the actual statusbar update!
 */
static gboolean statusbar_idle_update ( statusbar_idle_data *sid )
{
  sid->vw->sbiu_id = 0;
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
  sid->vw = vw;

  if ( g_thread_self() == thread ) {
    vw->sbiu_id = g_idle_add ( (GSourceFunc)statusbar_idle_update, sid );
  }
  else {
    // From a background thread
    vw->sbiu_id = gdk_threads_add_idle ( (GSourceFunc)statusbar_idle_update, sid );
  }
}

// Actual signal handlers
static void destroy_window ( GtkWidget *widget,
                             gpointer   data )
{
    g_debug ( "%s", __FUNCTION__ );
    if ( ! --window_count ) {
      vu_finish ();
      g_free ( last_folder_images_uri );
      gtk_main_quit ();
    }
}

#define VIK_SETTINGS_WIN_DEFAULT_TOOL "window_default_tool"

#define VIK_SETTINGS_WIN_SIDEPANEL "window_sidepanel"
#define VIK_SETTINGS_WIN_GRAPHS "window_track_graphs"
#define VIK_SETTINGS_WIN_STATUSBAR "window_statusbar"
#define VIK_SETTINGS_WIN_TOOLBAR "window_toolbar"
// Menubar setting to off is never auto saved in case it's accidentally turned off
// It's not so obvious so to recover the menu visibility.
// Thus this value is for setting manually via editting the settings file directly
#define VIK_SETTINGS_WIN_MENUBAR "window_menubar"
#define VIK_SETTINGS_WIN_SIDEPANEL_BUTTONS "window_sidepanel_buttons"
#define VIK_SETTINGS_WIN_SIDEPANEL_TABS "window_sidepanel_tabs"
#define VIK_SETTINGS_WIN_SIDEPANEL_CALENDAR "window_sidepanel_calendar"
#define VIK_SETTINGS_WIN_SIDEPANEL_GOTO "window_sidepanel_goto"
#define VIK_SETTINGS_WIN_SIDEPANEL_STATS "window_sidepanel_stats"
#define VIK_SETTINGS_WIN_SIDEPANEL_SPLITS "window_sidepanel_splits"

VikWindow *vik_window_new_window ()
{
  if ( window_count < MAX_WINDOWS )
  {
    VikWindow *vw = window_new ();

    if ( window_count == 0 ) {
       vik_window_statusbar_update ( vw, _("This is Viking "VIKING_VERSION), VIK_STATUSBAR_INFO );
    }

    g_signal_connect (G_OBJECT (vw), "destroy",
		      G_CALLBACK (destroy_window), NULL);
    g_signal_connect (G_OBJECT (vw), "newwindow",
		      G_CALLBACK (vik_window_new_window), NULL);

    gtk_widget_show_all ( GTK_WIDGET(vw) );

    // NB Default setting of OFF
    gboolean sidepanel_splits = FALSE;

    if ( a_vik_get_restore_window_state() ) {
      // These settings are applied after the show all as these options hide widgets
      gboolean sidepanel;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL, &sidepanel ) )
        if ( ! sidepanel ) {
          gtk_widget_hide ( GTK_WIDGET(vw->viking_vlp) );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanel" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean track_graphs;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_GRAPHS, &track_graphs ) )
        if ( ! track_graphs ) {
          gtk_widget_hide ( GTK_WIDGET(vw->graphs) );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewTrackGraphs" );
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

      gboolean sidepanel_buttons;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_BUTTONS, &sidepanel_buttons ) )
        if ( ! sidepanel_buttons ) {
          vik_layers_panel_show_buttons ( vw->viking_vlp, FALSE );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanelButtons" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean sidepanel_tabs;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_TABS, &sidepanel_tabs ) )
        if ( ! sidepanel_tabs ) {
          vik_layers_panel_show_tabs ( vw->viking_vlp, FALSE );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanelTabs" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean sidepanel_calendar;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_CALENDAR, &sidepanel_calendar ) )
        if ( ! sidepanel_calendar ) {
          vik_layers_panel_show_calendar ( vw->viking_vlp, FALSE );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanelCalendar" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean sidepanel_goto;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_GOTO, &sidepanel_goto ) )
        if ( ! sidepanel_goto ) {
          vik_layers_panel_show_goto ( vw->viking_vlp, FALSE );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanelGoto" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      gboolean sidepanel_stats;
      if ( a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_STATS, &sidepanel_stats ) )
        if ( ! sidepanel_stats ) {
          vik_layers_panel_show_stats ( vw->viking_vlp, FALSE );
          GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanelStats" );
          gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
        }

      (void)a_settings_get_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_SPLITS, &sidepanel_splits );
    }

    if ( !sidepanel_splits ) {
      vik_layers_panel_show_splits ( vw->viking_vlp, FALSE );
      GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewSidePanelSplits" );
      gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), FALSE );
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

void determine_location_fallback ( VikWindow *vw )
{
  a_background_thread ( BACKGROUND_POOL_REMOTE,
                        GTK_WINDOW(vw),
                        _("Determining location"),
                        (vik_thr_func) determine_location_thread,
                        vw,
                        NULL,
                        NULL,
                        1 );
}

#ifdef HAVE_LIBGEOCLUE_2
void update_from_geoclue ( VikWindow *vw, struct LatLon ll, gdouble accuracy )
{
  // See if we received sensible answers
  gboolean fallback = FALSE;
  if ( isnan(ll.lat) ) fallback = TRUE;
  if ( isnan(accuracy) || accuracy > 10000000 ) fallback = TRUE;

  if ( fallback ) {
    determine_location_fallback ( vw );
    return;
   }

  // Guestimate zoom level relative to accuracy
  gdouble zoom_vals[] = {2, 4, 8, 16, 32, 64, 128, 256, 512};
  guint zlevel = (guint)log10(accuracy);
  if ( zlevel > 8 )
    zlevel = 8;

  vik_viewport_set_zoom ( vw->viking_vvp, zoom_vals[zlevel] );
  vik_viewport_set_center_latlon ( vw->viking_vvp, &ll, FALSE );

  gchar *message = g_strdup_printf ( _("Location found via geoclue") );
  vik_window_statusbar_update ( vw, message, VIK_STATUSBAR_INFO );
  g_free ( message );

  // Signal to redraw from the background
  vik_layers_panel_emit_update ( vw->viking_vlp );
}
#endif

/**
 * Steps to be taken once initial loading has completed
 */
void vik_window_new_window_finish ( VikWindow *vw )
{
  // Don't add a map if we've loaded a Viking file already
  if ( vw->filename )
    return;

  // Maybe add a default map layer
  if ( a_vik_get_add_default_map_layer () ) {
    VikMapsLayer *vml = VIK_MAPS_LAYER ( vik_layer_create(VIK_LAYER_MAPS, vw->viking_vvp, FALSE) );
    vik_layer_rename ( VIK_LAYER(vml), _("Default Map") );
    vik_aggregate_layer_add_layer ( vik_layers_panel_get_top_layer(vw->viking_vlp), VIK_LAYER(vml), TRUE );
    vik_layer_post_read ( VIK_LAYER(vml), vw->viking_vvp, TRUE );
    draw_update ( vw );
  }

  // If not loaded any file, maybe try the location lookup
  if ( vw->loaded_type == LOAD_TYPE_READ_FAILURE ) {
    if ( a_vik_get_startup_method ( ) == VIK_STARTUP_METHOD_AUTO_LOCATION ) {

      vik_window_statusbar_update ( vw, _("Trying to determine location..."), VIK_STATUSBAR_INFO );
#ifdef HAVE_LIBGEOCLUE_2
      libgeoclue_where_am_i ( vw, update_from_geoclue );
#else
      determine_location_fallback ( vw );
#endif
    }
  }
}

static void open_window ( VikWindow *vw, GSList *files, gboolean external )
{
  if ( !vw  )
    return;
  guint file_num = 0;
  guint num_files = g_slist_length(files);
  gboolean change_fn = (num_files == 1); // only change fn if one file
  GSList *cur_file = files;
  while ( cur_file ) {
    // Only open a new window if a viking file
    gchar *file_name = cur_file->data;
    file_num++;
    if (vw->filename && check_file_magic_vik ( file_name ) ) {
      VikWindow *newvw = vik_window_new_window ();
      if (newvw)
        vik_window_open_file ( newvw, file_name, TRUE, TRUE, TRUE, TRUE, FALSE );
    }
    else {
      vik_window_open_file ( vw, file_name, change_fn, (file_num==1), (file_num==num_files), TRUE, external );
    }
    g_free (file_name);
    cur_file = g_slist_next (cur_file);
  }
  g_slist_free (files);
}
// End signals

/**
 * vik_window_selected_layer:
 *
 * Use to align menu & toolbar layer tool radio buttons to the selected layer
 * vl maybe NULL - then all layer tool buttons are desensitized
 */
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
      g_object_set(action, "sensitive", vl ? i == vl->type : FALSE, NULL);
      toolbar_action_set_sensitive ( vw->viking_vtb, vik_layer_get_interface(i)->tools[j].radioActionEntry.name, vl ? i == vl->type : FALSE );
    }
  }
}

static void window_finalize ( GObject *gob )
{
  VikWindow *vw = VIK_WINDOW(gob);
  g_return_if_fail ( vw != NULL );

  if ( vw->sbiu_id )
    (void)g_source_remove ( vw->sbiu_id );

  a_background_remove_window ( vw );
  a_logging_remove_window ( vw );

  window_list = g_slist_remove ( window_list, vw );

  gdk_cursor_unref ( vw->busy_cursor );
  int tt;
  for (tt = 0; tt < vw->vt->n_tools; tt++ ) {
    if ( vw->vt->tools[tt].ti.cursor )
      gdk_cursor_unref ( (GdkCursor*)vw->vt->tools[tt].ti.cursor );
    if ( vw->vt->tools[tt].ti.destroy )
      vw->vt->tools[tt].ti.destroy ( vw->vt->tools[tt].state );
  }
  g_free ( vw->vt->tools );
  g_free ( vw->vt );
  g_free ( vw->filename );

  vik_toolbar_finalize ( vw->viking_vtb );

  G_OBJECT_CLASS(parent_class)->finalize(gob);
}


static void vik_window_class_init ( VikWindowClass *klass )
{
  /* destructor */
  GObjectClass *object_class;

  window_signals[VW_NEWWINDOW_SIGNAL] = g_signal_new ( "newwindow", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikWindowClass, newwindow), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

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

static void zoom_popup_handler (GtkWidget *widget)
{
  GtkMenu *menu;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));

  /* The "widget" is the menu that was supplied when
   * g_signal_connect_swapped() was called.
   */
  menu = GTK_MENU (widget);

  gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
                  1, gtk_get_current_event_time());
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
      g_debug ("drag received string:%s", str);

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
        open_window ( VIK_WINDOW_FROM_WIDGET(widget), filenames, FALSE );
        // NB: GSList & contents are freed by open_window()

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

// Force the previously hidden widgets inside the container to get drawn
static void map_signal_cb ( VikWindow *vw )
{
  gtk_widget_show_all ( vw->graphs );
}

#if GTK_CHECK_VERSION (3,0,0)
static gboolean draw_signal ( GtkWidget *widget, cairo_t *cr, VikWindow *vw )
{
  vik_viewport_sync(vw->viking_vvp, cr);
  draw_status ( vw );
  return TRUE;
}

static void zoom_gesture_begin_cb ( GtkGesture       *gesture,
                                    GdkEventSequence *sequence,
                                    VikWindow        *vw )
{
  vw->pinch_zoom_begin = vik_viewport_get_zoom ( vw->viking_vvp );
  vw->pinch_zoom_last = vw->pinch_zoom_begin;
  g_debug ( "%s: %.4f", __FUNCTION__, vw->pinch_zoom_begin );
}

static void zoom_gesture_scale_changed_cb ( GtkGesture *gesture,
                                            gdouble    scale,
                                            VikWindow  *vw )
{
  gdouble factor = (scale < 1.0) ? scale / vw->pinch_gesture_factor : scale * vw->pinch_gesture_factor;
  gdouble target_zoom = vw->pinch_zoom_begin * 1.0/factor;
  gdouble new_zoom = 0;

  if ( target_zoom <= vw->pinch_zoom_last / 2 )
    new_zoom = vw->pinch_zoom_last / 2;
  else if ( target_zoom >= vw->pinch_zoom_last * 2 )
    new_zoom = vw->pinch_zoom_last * 2;

  if ( new_zoom >= VIK_VIEWPORT_MIN_ZOOM && new_zoom <= VIK_VIEWPORT_MAX_ZOOM ) {
    VikCoord coord;
    gint x, y;
    gdouble point_x, point_y;

    gtk_gesture_get_point ( gesture, NULL, &point_x, &point_y );
    gint center_x = vik_viewport_get_width ( vw->viking_vvp ) / 2;
    gint center_y = vik_viewport_get_height ( vw->viking_vvp ) / 2;

    vik_viewport_screen_to_coord ( vw->viking_vvp, (int)point_x, (int)point_y, &coord );

    vik_viewport_set_zoom ( vw->viking_vvp, new_zoom );

    vik_viewport_coord_to_screen ( vw->viking_vvp, &coord, &x, &y );
    vik_viewport_set_center_screen ( vw->viking_vvp, center_x + (x - point_x), center_y + (y - point_y) );

    draw_update ( vw );
    vw->pinch_zoom_last = new_zoom;
  }
  g_debug ( "%s: %.4f %.3f", __FUNCTION__, new_zoom, scale );
}
#endif

static void action_activate_current_tool ( VikWindow *vw )
{
  switch ( vw->current_tool ) {
  case TOOL_ZOOM:
    gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, "Zoom" ) );
    break;
  case TOOL_RULER:
    gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, "Ruler" ) );
    break;
  case TOOL_SELECT:
    gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, "Select" ) );
    break;
  default:
    gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, "Pan" ) );
    break;
  }
}

static void default_tool_enable ( VikWindow *vw )
{
  gchar *tool_str = NULL;
  if ( a_settings_get_string ( VIK_SETTINGS_WIN_DEFAULT_TOOL, &tool_str ) ) {
    if ( !g_ascii_strcasecmp(tool_str,"Pan") )
      vw->current_tool = TOOL_PAN;
    else if ( !g_ascii_strcasecmp(tool_str,"Zoom") )
      vw->current_tool = TOOL_ZOOM;
    else if ( !g_ascii_strcasecmp(tool_str,"Ruler") )
      vw->current_tool = TOOL_RULER;
    else if ( !g_ascii_strcasecmp(tool_str,"Select") )
      vw->current_tool = TOOL_SELECT;
    else {
      g_warning ("%s: Couldn't understand '%s' for the default tool", __FUNCTION__, tool_str);
      vw->current_tool = TOOL_SELECT;
    }
  }
  else
    vw->current_tool = TOOL_SELECT;

  g_free ( tool_str );

  action_activate_current_tool ( vw );
}

#define VIK_SETTINGS_WIN_MAX "window_maximized"
#define VIK_SETTINGS_WIN_FULLSCREEN "window_fullscreen"
#define VIK_SETTINGS_WIN_WIDTH "window_width"
#define VIK_SETTINGS_WIN_HEIGHT "window_height"
#define VIK_SETTINGS_WIN_PANE_POSITION "window_horizontal_pane_position"
#define VIK_SETTINGS_WIN_VPANE_POSITION "window_vertical_main_pane_position_decimal_percent"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH "window_save_image_width"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT "window_save_image_height"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_PNG "window_save_image_as_png"
#define VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT "window_copy_centre_full_format"
#define VIK_SETTINGS_WIN_ZOOM_SCROLL_TIMEOUT "window_zoom_scroll_timeout"
#define VIK_SETTINGS_WIN_MOVE_SCROLL_TIMEOUT "window_move_scroll_timeout"
#define VIK_SETTINGS_WIN_PINCH_GESTURE_FACTOR "window_pinch_gesture_factor"

#define VIKING_ACCELERATOR_KEY_FILE "keys.rc"

static void vik_window_init ( VikWindow *vw )
{
  vw->action_group = NULL;

  vw->viking_vvp = vik_viewport_new();
  vw->viking_vlp = vik_layers_panel_new();
  vik_layers_panel_set_viewport ( vw->viking_vlp, vw->viking_vvp );
  vw->viking_vs = vik_statusbar_new ( vik_viewport_get_scale(vw->viking_vvp) );

  vw->vt = toolbox_create(vw);
  vw->viking_vtb = vik_toolbar_new ();
  window_create_ui(vw);
  window_set_filename (vw, NULL);

  vw->busy_cursor = gdk_cursor_new_for_display ( gtk_widget_get_display(GTK_WIDGET(vw)), GDK_WATCH );

  vw->filename = NULL;
  vw->loaded_type = LOAD_TYPE_READ_FAILURE; //AKA none
  vw->modified = FALSE;
  vw->only_updating_coord_mode_ui = FALSE;

  vw->select_double_click = FALSE;
  vw->select_move = FALSE;
  vw->select_pan = FALSE;
  vw->pan_move_middle = FALSE;
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

  gint zoom_scroll_timeout;
  if ( a_settings_get_integer ( VIK_SETTINGS_WIN_ZOOM_SCROLL_TIMEOUT, &zoom_scroll_timeout ) )
    vw->zoom_scroll_timeout = (guint)zoom_scroll_timeout;
  else
    vw->zoom_scroll_timeout = 150;
  gint move_scroll_timeout;
  if ( a_settings_get_integer ( VIK_SETTINGS_WIN_MOVE_SCROLL_TIMEOUT, &move_scroll_timeout ) )
    vw->move_scroll_timeout = (guint)move_scroll_timeout;
  else
    vw->move_scroll_timeout = 5;

  gdouble pinch_gesture_factor;
  if ( a_settings_get_double ( VIK_SETTINGS_WIN_PINCH_GESTURE_FACTOR, &pinch_gesture_factor ) )
    vw->pinch_gesture_factor = fabs(pinch_gesture_factor);
  else
    vw->pinch_gesture_factor = 1.5;

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
  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "update", G_CALLBACK(draw_update), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "delete_layer", G_CALLBACK(vik_window_clear_selected), vw);

  // Signals from GTK
#if GTK_CHECK_VERSION (3,0,0)
  g_signal_connect (G_OBJECT(vw->viking_vvp), "draw", G_CALLBACK(draw_signal), vw);
#else
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "expose_event", G_CALLBACK(draw_sync), vw);
#endif
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "configure_event", G_CALLBACK(window_configure_event), vw);
#if GTK_CHECK_VERSION (3,0,0)
  // Further discussion on GTK3/Wayland/X11 scrolling - https://sourceforge.net/p/scintilla/bugs/1901/
  guint smoothMask = 0;
#ifdef GDK_WINDOWING_WAYLAND
  GdkDisplay *pdisplay = gdk_display_get_default();
  // On Wayland, touch pads only produce smooth scroll events
  if ( GDK_IS_WAYLAND_DISPLAY(pdisplay) )
    smoothMask = GDK_SMOOTH_SCROLL_MASK;
#endif
  g_debug ( "%s smoothMask %d", __FUNCTION__, smoothMask );
  gtk_widget_add_events ( GTK_WIDGET(vw->viking_vvp), GDK_SCROLL_MASK | smoothMask | GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_TOUCHPAD_GESTURE_MASK );
  gtk_widget_add_events ( GTK_WIDGET(vw->viking_vvp), GDK_TOUCHPAD_GESTURE_MASK );
  GtkGesture *zoom_gesture = gtk_gesture_zoom_new ( GTK_WIDGET(vw->viking_vvp) );
  g_signal_connect ( zoom_gesture, "begin", G_CALLBACK(zoom_gesture_begin_cb), vw );
  g_signal_connect ( zoom_gesture, "scale-changed", G_CALLBACK(zoom_gesture_scale_changed_cb), vw );
#else
  gtk_widget_add_events ( GTK_WIDGET(vw->viking_vvp), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK );
#endif
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "scroll_event", G_CALLBACK(draw_scroll), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_press_event", G_CALLBACK(draw_click), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_release_event", G_CALLBACK(draw_release), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "motion_notify_event", G_CALLBACK(draw_mouse_motion), vw);

  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "key_press_event", G_CALLBACK(key_press_event), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "key_release_event", G_CALLBACK(key_release_event), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "key_press_event", G_CALLBACK(key_press_event_vlp), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "key_release_event", G_CALLBACK(key_release_event_vlp), vw);

  // Set initial button sensitivity
  center_changed_cb ( vw );

  vw->vpaned = gtk_vpaned_new ();
  vw->graphs = gtk_frame_new ( NULL );

  g_signal_connect_swapped (G_OBJECT(vw->graphs), "map", G_CALLBACK(map_signal_cb), vw);

  gtk_paned_pack1 ( GTK_PANED(vw->vpaned), GTK_WIDGET(vw->viking_vvp), TRUE, TRUE );
  gtk_paned_pack2 ( GTK_PANED(vw->vpaned), vw->graphs, FALSE, TRUE );

  vw->hpaned = gtk_hpaned_new ();
  gtk_paned_pack1 ( GTK_PANED(vw->hpaned), GTK_WIDGET(vw->viking_vlp), FALSE, TRUE );
  gtk_paned_pack2 ( GTK_PANED(vw->hpaned), vw->vpaned, TRUE, TRUE );

  /* This packs the button into the window (a gtk container). */
  gtk_box_pack_start (GTK_BOX(vw->main_vbox), vw->hpaned, TRUE, TRUE, 0);

  gtk_box_pack_end (GTK_BOX(vw->main_vbox), GTK_WIDGET(vw->viking_vs), FALSE, TRUE, 0);

  a_background_add_window ( vw );
  a_logging_add_window ( vw );

  window_list = g_slist_prepend ( window_list, vw);

  gint height = VIKING_WINDOW_HEIGHT;
  gint width = VIKING_WINDOW_WIDTH;

  vw->vpaned_pc = -1.0; // Auto positioning

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

    gdouble pos;
    if ( a_settings_get_double(VIK_SETTINGS_WIN_VPANE_POSITION, &pos) ) {
      if ( pos > 0.0 && pos < 1.0 )
        vw->vpaned_pc = pos;
    }
  }

  gtk_window_set_default_size ( GTK_WINDOW(vw), width, height );

  vw->show_side_panel = TRUE;
  vw->show_track_graphs = TRUE;
  vw->show_statusbar = TRUE;
  vw->show_toolbar = TRUE;
  vw->show_main_menu = TRUE;
  vw->show_side_panel_buttons = TRUE;
  vw->show_side_panel_tabs = TRUE;
  vw->show_side_panel_calendar = TRUE;
  vw->show_side_panel_goto = TRUE;
  vw->show_side_panel_stats = TRUE;
  vw->show_side_panel_splits = TRUE; // Actually defaults to off - see vik_window_new_window()

  // Only accept Drag and Drop of files onto the viewport
  gtk_drag_dest_set ( GTK_WIDGET(vw->viking_vvp), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
  gtk_drag_dest_add_uri_targets ( GTK_WIDGET(vw->viking_vvp) );
  g_signal_connect ( GTK_WIDGET(vw->viking_vvp), "drag-data-received", G_CALLBACK(drag_data_received_cb), NULL );

  // Store the thread value so comparisons can be made to determine the gdk update method
  // Hopefully we are storing the main thread value here :)
  //  [ATM any window initialization is always be performed by the main thread]
  vw->thread = g_thread_self();

  // Set the default tool + mode
  default_tool_enable ( vw );
  gtk_action_activate ( gtk_action_group_get_action ( vw->action_group, "ModeMercator" ) );

  gchar *accel_file_name = g_build_filename ( a_get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL );
  gtk_accel_map_load ( accel_file_name );
  g_free ( accel_file_name );

  // Set initial focus to the viewport,
  //  so if you have map you can start to move around with the arrow keys immediately
  gtk_widget_grab_focus ( GTK_WIDGET(vw->viking_vvp) );
}

static VikWindow *window_new ()
{
  return VIK_WINDOW ( g_object_new ( VIK_WINDOW_TYPE, NULL ) );
}

/**
 * Percentage is from the top / left depending on the pane type
 */
gdouble get_pane_position_as_percent ( GtkWidget *pane )
{
  GtkAllocation allocation;
  gtk_widget_get_allocation ( pane, &allocation );
  gint position = gtk_paned_get_position ( GTK_PANED(pane) );
  gdouble pc = (gdouble)position / (gdouble)allocation.height;
  return pc;
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

static gboolean key_press_event_common ( VikWindow *vw, GdkEventKey *event, gpointer data )
{
  /* Restore Main Menu via Escape key if the user has hidden it */
  /* This key is more likely to be used as they may not remember the function key */
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/View/SetShow/ViewMainMenu" );
  if ( check_box ) {
    gboolean state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
    if ( !state ) {
      if ( event->keyval == GDK_KEY_Escape ) {
	gtk_widget_show ( gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu" ) );
	gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), TRUE );
	return TRUE; // handled keypress
      }
    }
  }
  return FALSE; // don't handle the keypress
}

static gboolean key_press_event_vlp ( VikWindow *vw, GdkEventKey *event, gpointer data )
{
  if ( key_press_event_common(vw, event, data) )
    return TRUE; // handled keypress

  return FALSE; // don't handle the keypress
}

/**
 * This is the viewport key press handler
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

  GdkModifierType modifiers = event->state & gtk_accelerator_get_default_mod_mask();

  // Standard 'Refresh' keys: F5 or Ctrl+r
  // Note 'F5' is actually handled via draw_refresh_cb() later on
  //  (not 'R' it's 'r' notice the case difference!!)
  if ( event->keyval == GDK_KEY_r && modifiers == GDK_CONTROL_MASK ) {
	map_download = TRUE;
	map_download_only_new = TRUE;
  }
  // Full cache reload with Ctrl+F5 or Ctrl+Shift+r [This is not in the menu system]
  // Note the use of uppercase R here since shift key has been pressed
  else if ( (event->keyval == GDK_KEY_F5 && modifiers == GDK_CONTROL_MASK ) ||
           ( event->keyval == GDK_KEY_R && modifiers == (GDK_CONTROL_MASK + GDK_SHIFT_MASK) ) ) {
	map_download = TRUE;
	map_download_only_new = FALSE;
  }
  // Standard Ctrl+KP+ / Ctrl+KP- to zoom in/out respectively
  else if ( event->keyval == GDK_KEY_KP_Add && modifiers == GDK_CONTROL_MASK ) {
    vik_viewport_zoom_in ( vw->viking_vvp );
    draw_update(vw);
    return TRUE; // handled keypress
  }
  else if ( event->keyval == GDK_KEY_KP_Subtract && modifiers == GDK_CONTROL_MASK ) {
    vik_viewport_zoom_out ( vw->viking_vvp );
    draw_update(vw);
    return TRUE; // handled keypress
  }
  else if ( event->keyval == GDK_KEY_p && !modifiers ) {
    vw->current_tool = TOOL_PAN;
    action_activate_current_tool ( vw );
    return TRUE; // handled keypress
  }
  else if ( event->keyval == GDK_KEY_z && !modifiers ) {
    vw->current_tool = TOOL_ZOOM;
    action_activate_current_tool ( vw );
    return TRUE; // handled keypress
  }
  else if ( event->keyval == GDK_KEY_r && !modifiers ) {
    vw->current_tool = TOOL_RULER;
    action_activate_current_tool ( vw );
    return TRUE; // handled keypress
  }
  else if ( event->keyval == GDK_KEY_s && !modifiers ) {
    vw->current_tool = TOOL_SELECT;
    action_activate_current_tool ( vw );
    return TRUE; // handled keypress
  }

  if ( map_download ) {
    simple_map_update ( vw, map_download_only_new );
    return TRUE; // handled keypress
  }

  gboolean handled = FALSE;
  VikLayer *vl = vik_layers_panel_get_selected ( vw->viking_vlp );
  if (vl && vw->vt->active_tool != -1 && vw->vt->tools[vw->vt->active_tool].ti.key_press ) {
    gint ltype = vw->vt->tools[vw->vt->active_tool].layer_type;
    if ( vl && ltype == vl->type )
      handled = vw->vt->tools[vw->vt->active_tool].ti.key_press(vl, event, vw->vt->tools[vw->vt->active_tool].state);
    if ( handled )
      return TRUE;
  }

  if ( key_press_event_common(vw, event, data) )
    return TRUE; // handled keypress

  // Ensure called only on window tools (i.e. not on any of the Layer tools since the layer is NULL)
  if ( vw->current_tool < TOOL_LAYER ) {
    // No layer - but enable window tool keypress processing - these should be able to handle a NULL layer
    if ( vw->vt->tools[vw->vt->active_tool].ti.key_press ) {
      handled = vw->vt->tools[vw->vt->active_tool].ti.key_press ( vl, event, vw->vt->tools[vw->vt->active_tool].state );
    if ( handled )
      return TRUE;
    }
  }

  // Default Tool
  if ( event->keyval == GDK_KEY_Escape ) {
    default_tool_enable ( vw );
    return TRUE;
  }

  return FALSE; /* don't handle the keypress */
}

static gboolean key_release_event_vlp ( VikWindow *vw, GdkEventKey *event, gpointer data )
{
  // Nothing ATM
  return FALSE;
}

static gboolean key_release_event( VikWindow *vw, GdkEventKey *event, gpointer data )
{
  gboolean handled = FALSE;
  VikLayer *vl = vik_layers_panel_get_selected ( vw->viking_vlp );
  if (vl && vw->vt->active_tool != -1 && vw->vt->tools[vw->vt->active_tool].ti.key_release ) {
    gint ltype = vw->vt->tools[vw->vt->active_tool].layer_type;
    if ( vl && ltype == vl->type )
      handled = vw->vt->tools[vw->vt->active_tool].ti.key_release(vl, event, vw->vt->tools[vw->vt->active_tool].state);
    if ( handled )
      return TRUE;
  }
  // Ensure called on window tools
  if ( vw->current_tool < TOOL_LAYER ) {
    if ( vw->vt->tools[vw->vt->active_tool].ti.key_release ) {
      handled = vw->vt->tools[vw->vt->active_tool].ti.key_release ( vl, event, vw->vt->tools[vw->vt->active_tool].state );
    if ( handled )
      return TRUE;
    }
  }
  return FALSE;
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
      gint state = gdk_window_get_state ( gtk_widget_get_window(GTK_WIDGET(vw)) );
      gboolean state_max = state & GDK_WINDOW_STATE_MAXIMIZED;
      a_settings_set_boolean ( VIK_SETTINGS_WIN_MAX, state_max );

      gboolean state_fullscreen = state & GDK_WINDOW_STATE_FULLSCREEN;
      a_settings_set_boolean ( VIK_SETTINGS_WIN_FULLSCREEN, state_fullscreen );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL, gtk_widget_get_visible(GTK_WIDGET(vw->viking_vlp)) );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_GRAPHS, gtk_widget_get_visible(vw->graphs) );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_STATUSBAR, gtk_widget_get_visible(GTK_WIDGET(vw->viking_vs)) );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_TOOLBAR, gtk_widget_get_visible(toolbar_get_widget(vw->viking_vtb)) );

      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_BUTTONS, vw->show_side_panel_buttons );
      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_TABS, vw->show_side_panel_tabs );
      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_CALENDAR, vw->show_side_panel_calendar );
      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_GOTO, vw->show_side_panel_goto );
      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_STATS, vw->show_side_panel_stats );
      a_settings_set_boolean ( VIK_SETTINGS_WIN_SIDEPANEL_SPLITS, vw->show_side_panel_splits );

      // If supersized - no need to save the enlarged width+height values
      if ( ! (state_fullscreen || state_max) ) {
        gint width, height;
        gtk_window_get_size ( GTK_WINDOW (vw), &width, &height );
        a_settings_set_integer ( VIK_SETTINGS_WIN_WIDTH, width );
        a_settings_set_integer ( VIK_SETTINGS_WIN_HEIGHT, height );
      }

      a_settings_set_integer ( VIK_SETTINGS_WIN_PANE_POSITION, gtk_paned_get_position (GTK_PANED(vw->hpaned)) );

      // Only save the value if the vpane is being used
      if ( vw->show_track_graphs ) {
        // Get latest value
        if ( vw->vpaned_shown )
          vw->vpaned_pc = get_pane_position_as_percent ( vw->vpaned );
        a_settings_set_double ( VIK_SETTINGS_WIN_VPANE_POSITION, vw->vpaned_pc );
      }
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
  (void)draw_sync (vw);
}

static gboolean draw_sync ( VikWindow *vw )
{
  vik_viewport_sync(vw->viking_vvp, NULL);
  draw_status ( vw );
  return FALSE;
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

  // In GTK3 version if you hover the mouse over the statusbar zoom level
  //  This next statement for unknown reason causes a new 'draw' event
  //  which comes back here and so on continually!
  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_ZOOM, zoom_level );

  draw_status_tool ( vw );
}

void vik_window_set_redraw_trigger(VikLayer *vl)
{
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vl));
  if (NULL != vw)
    vw->trigger = vl;
}

/**
 * If graphs shown, then scale pane according to saved value
 */
static void scale_graphs_pane ( VikWindow *vw )
{
  if ( vw->graphs_widgets ) {
    if ( vw->show_track_graphs ) {
      GtkAllocation allocation;
      gtk_widget_get_allocation ( vw->vpaned, &allocation );
      guint position;
      if ( vw->vpaned_pc <= 0.0 || vw->vpaned_pc > 1.0 ) {
        position = allocation.height * 0.8;
      } else {
        position = allocation.height * vw->vpaned_pc;
      }
      gtk_paned_set_position ( GTK_PANED(vw->vpaned), position );
      vw->vpaned_shown = TRUE;
    }
  }
}

static gboolean window_configure_event ( VikWindow *vw, GdkEventConfigure *event, gpointer user_data )
{
  static gboolean first = TRUE;
  draw_redraw ( vw );
  if ( first ) {
    // This is a hack to initialize the cursor to the corresponding tool
    first = FALSE;
    switch ( vw->current_tool ) {
    case TOOL_ZOOM:
      vw->viewport_cursor = (GdkCursor*)toolbox_get_cursor(vw->vt, "Zoom");
      break;
    case TOOL_RULER:
      vw->viewport_cursor = (GdkCursor*)toolbox_get_cursor(vw->vt, "Ruler");
      break;
    case TOOL_SELECT:
      vw->viewport_cursor = (GdkCursor*)toolbox_get_cursor(vw->vt, "Select");
      break;
    default:
      vw->viewport_cursor = (GdkCursor*)toolbox_get_cursor(vw->vt, "Pan");
      break;
    }
    /* We set cursor, even if it is NULL: it resets to default */
    gdk_window_set_cursor ( gtk_widget_get_window(GTK_WIDGET(vw->viking_vvp)), vw->viewport_cursor );
  }

  // NB Would be nice to resize the graphs pane here when the overall window size has changed
  //  particularly when jumping from small window <-> maximised
  // But ATM this function also gets called when moving the vpane & it's not easy to separate out the individual drawing parts

  return FALSE;
}

static void draw_redraw ( VikWindow *vw )
{
  VikCoord old_center = vw->trigger_center;
  vw->trigger_center = *(vik_viewport_get_center(vw->viking_vvp));
  VikLayer *new_trigger = vw->trigger;
  vw->trigger = NULL;
  gpointer gp = vik_viewport_get_trigger ( vw->viking_vvp );
  VikLayer *old_trigger = NULL;
  if ( !gp )
    vik_viewport_set_trigger ( vw->viking_vvp, new_trigger );
  else
    old_trigger = VIK_LAYER(gp);

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

#if !GTK_CHECK_VERSION (3,0,0)
static gboolean draw_buf(gpointer data)
{
  gpointer *pass_along = data;
  gdk_draw_drawable (pass_along[0], pass_along[1],
		     pass_along[2], 0, 0, 0, 0, -1, -1);
  draw_buf_done = TRUE;
  return FALSE;
}
#endif

/* Mouse event handlers ************************************************************************/

static void vik_window_pan_click (VikWindow *vw, GdkEventButton *event)
{
  /* set panning origin */
  vw->pan_move = FALSE;
  vw->pan_x = (gint) event->x;
  vw->pan_y = (gint) event->y;
}

static gboolean draw_click (VikWindow *vw, GdkEventButton *event)
{
  gtk_widget_grab_focus ( GTK_WIDGET(vw->viking_vvp) );

  /* middle button pressed.  we reserve all middle button and scroll events
   * for panning and zooming; tools only get left/right/movement 
   */
  if ( event->button == 2) {
    if ( vw->vt->tools[vw->vt->active_tool].ti.pan_handler )
      // Tool still may need to do something (such as disable something)
      toolbox_click(vw->vt, event);
    vw->pan_move_middle = TRUE;
    vik_window_pan_click ( vw, event );
  } 
  else {
    toolbox_click(vw->vt, event);
  }
  return FALSE;
}

/**
 * Perform screen redraw after a little delay
 * (particularly from scroll events)
 */
static gboolean pending_draw_timeout ( VikWindow *vw )
{
  vw->pending_draw_id = 0;
  draw_update ( vw );
  return FALSE;
}

static void vik_window_pan_move (VikWindow *vw, GdkEventMotion *event)
{
  if ( vw->pan_x != -1 ) {
    gint new_pan_x = (gint)round(event->x);
    gint new_pan_y = (gint)round(event->y);
    vik_viewport_set_center_screen ( vw->viking_vvp,
                                     vik_viewport_get_width(vw->viking_vvp)/2 - new_pan_x + vw->pan_x,
                                     vik_viewport_get_height(vw->viking_vvp)/2 - new_pan_y + vw->pan_y );
    vw->pan_move = TRUE;
    vw->pan_x = new_pan_x;
    vw->pan_y = new_pan_y;
    if ( vw->pending_draw_id )
      g_source_remove ( vw->pending_draw_id );
    vw->pending_draw_id = g_timeout_add ( vw->move_scroll_timeout, (GSourceFunc)pending_draw_timeout, vw );
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

static gboolean draw_mouse_motion (VikWindow *vw, GdkEventMotion *event)
{
  static VikCoord coord;
  static struct UTM utm;
  #define BUFFER_SIZE 50
  static char pointer_buf[BUFFER_SIZE];
  gchar *lat = NULL, *lon = NULL;
  gint16 alt;
  gdouble zoom;
  VikDemInterpol interpol_method;

  gint x, y;
#if GTK_CHECK_VERSION (3,0,0)
  x = (int)round(event->x);
  y = (int)round(event->y);
#else
  // Maintain for GTK2 even if possibly not necessary any more;
  // For Wayland displays should not be used
  /* This is a hack, but work far the best, at least for single pointer systems.
   * See http://bugzilla.gnome.org/show_bug.cgi?id=587714 for more. */
  gdk_window_get_pointer (event->window, &x, &y, NULL);
#endif

  toolbox_move(vw->vt, event);

  vik_viewport_screen_to_coord ( vw->viking_vvp, x, y, &coord );
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

  // Middle button moving only
  if ( vw->pan_move_middle )
    vik_window_pan_move ( vw, event );

  /* This is recommended by the GTK+ documentation, but does not work properly.
   * Use deprecated way until GTK+ gets a solution for correct motion hint handling:
   * http://bugzilla.gnome.org/show_bug.cgi?id=587714
  */
  /* gdk_event_request_motions ( event ); */

  return FALSE;
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
      GValue dct = G_VALUE_INIT;
      g_value_init ( &dct, G_TYPE_INT );
      g_object_get_property ( G_OBJECT(gs), "gtk-double-click-time", &dct );
      // Give chance for a double click to occur
      gint timer = g_value_get_int ( &dct ) + 50;
      (void)g_timeout_add ( timer, (GSourceFunc)vik_window_pan_timeout, vw );
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

static gboolean draw_release ( VikWindow *vw, GdkEventButton *event )
{
  gtk_widget_grab_focus ( GTK_WIDGET(vw->viking_vvp) );

  if ( event->button == 2 ) {  /* move / pan */
    if ( vw->vt->tools[vw->vt->active_tool].ti.pan_handler )
      // Tool still may need to do something (such as reenable something)
      toolbox_release(vw->vt, event);
    vw->pan_move_middle = FALSE;
    vik_window_pan_release ( vw, event );
  }
  else {
    toolbox_release(vw->vt, event);
  }
  return FALSE;
}

static void scroll_zoom_direction ( VikWindow *vw, GdkScrollDirection direction )
{
  // In our GTK3 version, 'invert_scroll_direction' is only for moving the viewport
  //  and thus doesn't effect zoom direction anymore
#if !GTK_CHECK_VERSION (3,0,0)
  if ( a_vik_get_invert_scroll_direction() ) {
    if ( direction == GDK_SCROLL_DOWN )
     vik_viewport_zoom_in (vw->viking_vvp);
    else if ( direction == GDK_SCROLL_UP ) {
      vik_viewport_zoom_out (vw->viking_vvp);
    }
  } else
#endif
  {
    if ( direction == GDK_SCROLL_UP )
      vik_viewport_zoom_in (vw->viking_vvp);
    else if ( direction == GDK_SCROLL_DOWN ) {
      vik_viewport_zoom_out (vw->viking_vvp);
    }
  }
}

static void scroll_move_viewport ( VikWindow *vw, GdkEventScroll *event )
{
  static const gdouble DELTA_STEP = 0.0333;
  int width = vik_viewport_get_width(vw->viking_vvp);
  int height = vik_viewport_get_height(vw->viking_vvp);
  if ( a_vik_get_invert_scroll_direction() ) {
    switch ( event->direction ) {
    case GDK_SCROLL_RIGHT:
      vik_viewport_set_center_screen ( vw->viking_vvp, width*0.666, height/2 ); break;
    case GDK_SCROLL_LEFT:
      vik_viewport_set_center_screen ( vw->viking_vvp, width*0.333, height/2 ); break;
    case GDK_SCROLL_DOWN:
      vik_viewport_set_center_screen ( vw->viking_vvp, width/2, height*0.666 ); break;
    case GDK_SCROLL_UP:
      vik_viewport_set_center_screen ( vw->viking_vvp, width/2, height*0.333 ); break;
#if GTK_CHECK_VERSION (3,0,0)
    case GDK_SCROLL_SMOOTH:
      vik_viewport_set_center_screen ( vw->viking_vvp, width/2+(DELTA_STEP*width*event->delta_x), height/2+(DELTA_STEP*height*event->delta_y ) ); break;
#endif
    default: g_critical ( "%s: unhandled scroll direction %d", __FUNCTION__, event->direction ); break;
    }
  } else {
    switch ( event->direction ) {
    case GDK_SCROLL_RIGHT:
      vik_viewport_set_center_screen ( vw->viking_vvp, width*0.333, height/2 ); break;
    case GDK_SCROLL_LEFT:
      vik_viewport_set_center_screen ( vw->viking_vvp, width*0.666, height/2 ); break;
    case GDK_SCROLL_DOWN:
      vik_viewport_set_center_screen ( vw->viking_vvp, width/2, height*0.333 ); break;
    case GDK_SCROLL_UP:
      vik_viewport_set_center_screen ( vw->viking_vvp, width/2, height*0.666 ); break;
#if GTK_CHECK_VERSION (3,0,0)
    case GDK_SCROLL_SMOOTH:
      vik_viewport_set_center_screen ( vw->viking_vvp, width/2-(DELTA_STEP*width*event->delta_x), height/2-(DELTA_STEP*height*event->delta_y ) ); break;
#endif
    default: g_critical ( "%s: unhandled scroll direction %d", __FUNCTION__, event->direction ); break;
    }
  }
}

/**
 * Zoom on specified point in screen x,y pixels
 *  Whether to zoom in or out is dependent on the either the mouse button pressed or scroll type
 */
static void zoom_at_xy ( VikWindow *vw, guint point_x, guint point_y, gboolean scroll_event, GdkScrollDirection direction, gint event_button )
{
  VikCoord coord;
  gint x, y;
  gint center_x = vik_viewport_get_width ( vw->viking_vvp ) / 2;
  gint center_y = vik_viewport_get_height ( vw->viking_vvp ) / 2;
  vik_viewport_screen_to_coord ( vw->viking_vvp, point_x, point_y, &coord );
  if ( scroll_event )
    scroll_zoom_direction ( vw, direction );
  else {
    if ( event_button == 1 )
      vik_viewport_zoom_in ( vw->viking_vvp );
    else if ( event_button == 3 )
      vik_viewport_zoom_out ( vw->viking_vvp );
  }
  vik_viewport_coord_to_screen ( vw->viking_vvp, &coord, &x, &y );
  vik_viewport_set_center_screen ( vw->viking_vvp, center_x + (x - point_x), center_y + (y - point_y) );
}

static gboolean draw_scroll (VikWindow *vw, GdkEventScroll *event)
{
  // Typically wheel mouse scrolls should zoom;
  //  but one could use a mouse with dual scroll wheels.
  // Whereas in GTK3 we can detect touch device scrolls and so it will then always move the viewport
  gboolean do_move = !a_vik_get_scroll_to_zoom();
#if GTK_CHECK_VERSION (3,0,0)
  GdkDevice *device = gdk_event_get_source_device ( (GdkEvent*)event );
  GdkInputSource isrc = gdk_device_get_source ( device );
  if ( isrc == GDK_SOURCE_TOUCHPAD || isrc == GDK_SOURCE_TOUCHSCREEN )
    do_move = TRUE;
#endif
  if ( do_move ) {
    scroll_move_viewport ( vw, event );

    // Paint the screen image
    draw_sync ( vw );

    // Note using a shorter timeout compared to the other instance at the end of this function
    //  since one path to get here is via touch-pad scrolls which would be generating many events
    if ( vw->pending_draw_id )
      g_source_remove ( vw->pending_draw_id );
    vw->pending_draw_id = g_timeout_add ( vw->move_scroll_timeout, (GSourceFunc)pending_draw_timeout, vw );

    return TRUE;
  }
    GdkScrollDirection direction = event->direction;
    // Possibly in Wayland even mouse wheel scrolls are 'smooth' events!
#if GTK_CHECK_VERSION(3,0,0)
    if ( direction == GDK_SCROLL_SMOOTH ) {
      double x_scroll, y_scroll;
      if ( gdk_event_get_scroll_deltas((GdkEvent*)event, &x_scroll, &y_scroll) ) {
        if ( y_scroll < 0 )
          direction = GDK_SCROLL_UP;
        else
          direction = GDK_SCROLL_DOWN;
      }
    }
#endif
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);
  if ( modifiers == GDK_CONTROL_MASK ) {
    /* control == pan up & down */
    if ( direction == GDK_SCROLL_UP )
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, vik_viewport_get_height(vw->viking_vvp)/3 );
    else if ( direction == GDK_SCROLL_DOWN )
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, vik_viewport_get_height(vw->viking_vvp)*2/3 );
  } else if ( modifiers == GDK_SHIFT_MASK ) {
    /* shift == pan left & right */
    if ( direction == GDK_SCROLL_UP )
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/3, vik_viewport_get_height(vw->viking_vvp)/2 );
    else if ( direction == GDK_SCROLL_DOWN )
      vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)*2/3, vik_viewport_get_height(vw->viking_vvp)/2 );
  } else if ( modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) ) {
    // This zoom is on the center position
    scroll_zoom_direction ( vw, direction );
  } else {
    /* make sure mouse is still over the same point on the map when we zoom */
    zoom_at_xy ( vw, event->x, event->y, TRUE, direction, 0 );
  }

  // If a pending draw, remove it and create a new one
  //  thus avoiding intermediary screen redraws when transiting through several
  //  zoom levels in quick succession, as typical when scroll zooming.
  if ( vw->pending_draw_id )
    g_source_remove ( vw->pending_draw_id );
  vw->pending_draw_id = g_timeout_add ( vw->zoom_scroll_timeout, (GSourceFunc)pending_draw_timeout, vw );

  return TRUE;
}

static void set_distance_text ( VikViewport *vvp, gdouble distance, PangoLayout *pl )
{
  gchar str[128];
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      if (distance >= 1000 && distance < 100000) {
        g_sprintf(str, "%3.2f km", distance/1000.0);
      } else if (distance < 1000) {
        g_sprintf(str, "%d m", (int)round(distance));
      } else {
        g_sprintf(str, "%d km", (int)round(distance/1000));
      }
      break;
    case VIK_UNITS_DISTANCE_MILES:
      if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
        g_sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
      } else if (distance < VIK_MILES_TO_METERS(1)) {
        g_sprintf(str, "%d yards", (int)round((distance*1.0936133)));
      } else {
        g_sprintf(str, "%d miles", (int)round(VIK_METERS_TO_MILES(distance)));
      }
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
        g_sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
      } else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
        g_sprintf(str, "%d yards", (int)round(distance*1.0936133));
      } else {
        g_sprintf(str, "%d NM", (int)round(VIK_METERS_TO_NAUTICAL_MILES(distance)));
      }
      break;
    default:
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }

    pango_layout_set_text(pl, str, -1);
    pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(vvp))->font_desc);
}

#if GTK_CHECK_VERSION (3,0,0)
// Hack for a now unused type, yet retain internal function parameter definitions
//  in a compatible manner for GTK2+3
typedef gpointer GdkDrawable;
#endif

// Clang compiler (or strict interpretation of C standard?) doesn't allow functions in static initializer,
//  so manually put in the values - even if GCC accepts it
static const double C15 = 0.999989561; // cos(DEG2RAD(15.0));
static const double S15 = 0.004569245; // sin(DEG2RAD(15.0));

/********************************************************************************
 ** Ruler tool code
 ********************************************************************************/
static void draw_ruler(VikViewport *vvp, GdkDrawable *d, GdkGC *gc, const VikCoord *start, VikCoord *end)
{
#define CR 80
#define CW 4
  PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(vvp), NULL);
  gchar str[128];
  // 'N' for North
  pango_layout_set_text(pl, _("N"), -1);

  gint x1, y1, x2, y2;
  vik_viewport_coord_to_screen ( vvp, start, &x1, &y1 );

  /* normalize end position in case the mouse pointer was moved out of the map */
  if (end->mode == VIK_COORD_LATLON) {
    end->east_west = fmod(end->east_west + 360, 360);
    if (end->east_west > 180) {
      end->east_west -= 360;
    }
  }
  vik_viewport_coord_to_screen ( vvp, end, &x2, &y2 );

  gdouble len, dx, dy, c, s, angle, angle_end, display_angle, baseangle;
  gint i;

  vik_viewport_compute_bearing ( vvp, x1, y1, x2, y2, &display_angle, &baseangle );
  angle = fmod(baseangle  + DEG2RAD(vik_coord_angle ( start, end )), 2*M_PI);
  angle_end = fmod(baseangle + DEG2RAD(vik_coord_angle_end ( start, end )), 2*M_PI);

  gdouble distance = vik_coord_diff (start, end);

  len = sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
  if ( len > 0.0001 ) {
    dx = (x2-x1)/len*10;
    dy = (y2-y1)/len*10;
  } else {
    dx = 0.0;
    dy = 0.0;
  }

#if GTK_CHECK_VERSION (3,0,0)
  ui_cr_clear ( gc );
  if ( gc ) {
    /* if the distance is less than 10km, the curvature definitely won't be visible */
    if (distance < 10e3) {
      /* draw line with arrow ends */
      a_viewport_clip_line(&x1, &y1, &x2, &y2);
      ui_cr_draw_line(gc, x1, y1, x2, y2);

      /* orthogonal bars */
      ui_cr_draw_line(gc, x1 - dy, y1 + dx, x1 + dy, y1 - dx);
      ui_cr_draw_line(gc, x2 - dy, y2 + dx, x2 + dy, y2 - dx);
      /* arrow components */
      ui_cr_draw_line(gc, x2, y2, x2 - (dx * C15 + dy * S15), y2 - (dy * C15 - dx * S15));
      ui_cr_draw_line(gc, x2, y2, x2 - (dx * C15 - dy * S15), y2 - (dy * C15 + dx * S15));
      ui_cr_draw_line(gc, x1, y1, x1 + (dx * C15 + dy * S15), y1 + (dy * C15 - dx * S15));
      ui_cr_draw_line(gc, x1, y1, x1 + (dx * C15 - dy * S15), y1 + (dy * C15 + dx * S15));
    } else {
      gint last_x = x1;
      gint last_y = y1;
      gint x, y;

      /* draw geodesic */
      for (gint step=0;step<=100;step++) {
        gdouble n = (gdouble) step / 100;
        VikCoord coord;
        vik_coord_geodesic_coord ( start, end, n, &coord );
        vik_viewport_coord_to_screen ( vvp, &coord, &x, &y );

        struct LatLon ll;
        vik_coord_to_latlon ( &coord, &ll) ;

        if (sqrt(pow(last_x-x, 2) + pow(last_y-y, 2)) < 100) {
          ui_cr_draw_line(gc, last_x, last_y, x, y);
        }
        last_x = x;
        last_y = y;
      }

      gdouble dx1 = 10 * cos(angle);
      gdouble dy1 = 10 * sin(angle);
      gdouble dx2 = 10 * cos(angle_end);
      gdouble dy2 = 10 * sin(angle_end);

      /* orthogonal bars */
      ui_cr_draw_line(gc, x1 - dx1, y1 - dy1, x1 + dx1, y1 + dy1);
      ui_cr_draw_line(gc, x2 - dx2, y2 - dy2, x2 + dx2, y2 + dy2);
      /* arrow components */
      ui_cr_draw_line(gc, x2, y2, x2 + (-dy2 * C15 + dx2 * S15), y2 + (+dx2 * C15 + dy2 * S15));
      ui_cr_draw_line(gc, x2, y2, x2 + (-dy2 * C15 - dx2 * S15), y2 + (+dx2 * C15 - dy2 * S15));
      ui_cr_draw_line(gc, x1, y1, x1 + (+dy1 * C15 + dx1 * S15), y1 + (-dx1 * C15 + dy1 * S15));
      ui_cr_draw_line(gc, x1, y1, x1 + (+dy1 * C15 - dx1 * S15), y1 + (-dx1 * C15 - dy1 * S15));
    }

    /* draw compass */
    for (i=0; i<180; i++) {
      c = cos(DEG2RAD(i)*2 + baseangle);
      s = sin(DEG2RAD(i)*2 + baseangle);

      if (i%5) {
        ui_cr_draw_line (gc, x1 + CR*c, y1 + CR*s, x1 + (CR+CW)*c, y1 + (CR+CW)*s);
      } else {
        gdouble ticksize = 2*CW;
        ui_cr_draw_line (gc, x1 + (CR-CW)*c, y1 + (CR-CW)*s, x1 + (CR+ticksize)*c, y1 + (CR+ticksize)*s);
      }
    }

    vik_viewport_draw_arc (NULL, gc, FALSE, x1-CR, y1-CR, 2*CR, 2*CR, 0, 64*360, NULL);
    vik_viewport_draw_arc (NULL, gc, FALSE, x1-CR-CW, y1-CR-CW, 2*(CR+CW), 2*(CR+CW), 0, 64*360, NULL);
    vik_viewport_draw_arc (NULL, gc, FALSE, x1-CR+CW, y1-CR+CW, 2*(CR-CW), 2*(CR-CW), 0, 64*360, NULL);
    c = (CR+CW*2)*cos(baseangle);
    s = (CR+CW*2)*sin(baseangle);
    ui_cr_draw_line (gc, x1-c, y1-s, x1+c, y1+s);
    ui_cr_draw_line (gc, x1+s, y1-c, x1-s, y1+c);

    // Output compass rose and bearing lines, before switching to draw the inner ring
    cairo_stroke(gc);

    // Switch colour for the inner ring
    GdkColor color;
    gdk_color_parse("#2255cc", &color);
    cairo_set_line_width ( gc, CW );
    vik_viewport_draw_arc (NULL, gc, FALSE, x1-CR+CW/2, y1-CR+CW/2, 2*CR-CW, 2*CR-CW, (RAD2DEG(baseangle)-90)*64, (RAD2DEG(angle)-90)*64, &color );
    cairo_stroke(gc);

    /* draw labels */
    gint wd, hd, xd, yd;
    gint wb, hb, xb, yb;
    gdk_color_parse("#000000", &color);
    gdk_cairo_set_source_color ( gc, &color );
    cairo_set_line_width ( gc, 1 );
    ui_cr_draw_layout (gc, x1-5, y1-CR-3*CW-8, pl);

    set_distance_text ( vvp, distance, pl );

    /* draw label with distance */
    pango_layout_get_pixel_size ( pl, &wd, &hd );

    gint mx, my;
    VikCoord midpoint;
    vik_coord_geodesic_coord ( start, end, 0.5, &midpoint );
    vik_viewport_coord_to_screen ( vvp, &midpoint, &mx, &my );

    if (dy>0) {
      xd = mx + dy;
      yd = my - hd/2 - dx;
    } else {
      xd = mx - dy;
      yd = my - hd/2 + dx;
    }

    if ( xd < -5 || yd < -5 || xd > vik_viewport_get_width(vvp)+5 || yd > vik_viewport_get_height(vvp)+5 ) {
      xd = x2 + 10;
      yd = y2 - 5;
    }

    ui_cr_label_with_bg (gc, xd, yd, wd, hd, pl);

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

    GdkRectangle r1 = {xd-2, yd-1, wd+4, hd+1}, r2 = {xb-2, yb-1, wb+4, hb+1};
    if (gdk_rectangle_intersect(&r1, &r2, &r2)) {
      xb = xd + wd + 5;
    }
    ui_cr_label_with_bg (gc, xb, yb, wb, hb, pl);
  }
#else
  /* if the distance is less than 10km, the curvature definitely won't be visible */
  if (distance < 10e3) {
    /* draw line with arrow ends */
    a_viewport_clip_line(&x1, &y1, &x2, &y2);
    gdk_draw_line(d, gc, x1, y1, x2, y2);

    /* orthogonal bars */
    gdk_draw_line(d, gc, x1 - dy, y1 + dx, x1 + dy, y1 - dx);
    gdk_draw_line(d, gc, x2 - dy, y2 + dx, x2 + dy, y2 - dx);
    /* arrow components */
    gdk_draw_line(d, gc, x2, y2, x2 - (dx * C15 + dy * S15), y2 - (dy * C15 - dx * S15));
    gdk_draw_line(d, gc, x2, y2, x2 - (dx * C15 - dy * S15), y2 - (dy * C15 + dx * S15));
    gdk_draw_line(d, gc, x1, y1, x1 + (dx * C15 + dy * S15), y1 + (dy * C15 - dx * S15));
    gdk_draw_line(d, gc, x1, y1, x1 + (dx * C15 - dy * S15), y1 + (dy * C15 + dx * S15));
  }

  else {
    gint last_x = x1;
    gint last_y = y1;
    gint x, y;

    /* draw geodesic */
    for (gint step=0;step<=100;step++) {
      gdouble n = (gdouble) step / 100;
      VikCoord coord;
      vik_coord_geodesic_coord ( start, end, n, &coord );
      vik_viewport_coord_to_screen ( vvp, &coord, &x, &y );

      struct LatLon ll;
      vik_coord_to_latlon ( &coord, &ll) ;

      if (sqrt(pow(last_x-x, 2) + pow(last_y-y, 2)) < 100) {
        gdk_draw_line(d, gc, last_x, last_y, x, y);
      }
      last_x = x;
      last_y = y;
    }

    gdouble dx1 = 10 * cos(angle);
    gdouble dy1 = 10 * sin(angle);
    gdouble dx2 = 10 * cos(angle_end);
    gdouble dy2 = 10 * sin(angle_end);

    /* orthogonal bars */
    gdk_draw_line(d, gc, x1 - dx1, y1 - dy1, x1 + dx1, y1 + dy1);
    gdk_draw_line(d, gc, x2 - dx2, y2 - dy2, x2 + dx2, y2 + dy2);
    /* arrow components */
    gdk_draw_line(d, gc, x2, y2, x2 + (-dy2 * C15 + dx2 * S15), y2 + (+dx2 * C15 + dy2 * S15));
    gdk_draw_line(d, gc, x2, y2, x2 + (-dy2 * C15 - dx2 * S15), y2 + (+dx2 * C15 - dy2 * S15));
    gdk_draw_line(d, gc, x1, y1, x1 + (+dy1 * C15 + dx1 * S15), y1 + (-dx1 * C15 + dy1 * S15));
    gdk_draw_line(d, gc, x1, y1, x1 + (+dy1 * C15 - dx1 * S15), y1 + (-dx1 * C15 - dy1 * S15));
  }

  GdkGC *labgc = vik_viewport_new_gc ( vvp, "#cccccc", 1);
  GdkGC *thickgc = gdk_gc_new(d);

  /* draw compass */
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
    gdk_draw_layout(d, gc, x1-5, y1-CR-3*CW-8, pl);
    set_distance_text ( vvp, distance, pl );

    /* draw label with distance */
    pango_layout_get_pixel_size ( pl, &wd, &hd );

    gint mx, my;
    VikCoord midpoint;
    vik_coord_geodesic_coord ( start, end, 0.5, &midpoint );
    vik_viewport_coord_to_screen ( vvp, &midpoint, &mx, &my );

    if (dy>0) {
      xd = mx + dy;
      yd = my - hd/2 - dx;
    } else {
      xd = mx - dy;
      yd = my - hd/2 + dx;
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

  g_object_unref ( G_OBJECT ( labgc ) );
  g_object_unref ( G_OBJECT ( thickgc ) );
#endif
  g_object_unref ( G_OBJECT ( pl ) );
}

static void ruler_click_normal (VikLayer *vl, GdkEventButton *event, tool_ed_t *s)
{
  struct LatLon ll;
  VikCoord coord;
  gchar *temp;
  if ( event->button == 1 ) {
    gchar *lat=NULL, *lon=NULL;
    s->displayed = TRUE;
    vik_viewport_screen_to_coord ( s->vw->viking_vvp, (gint) event->x, (gint) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );
    a_coords_latlon_to_string ( &ll, &lat, &lon );
    if ( s->has_oldcoord ) {
      gchar tmp_buf[64];
      vik_units_distance_t dist_units = a_vik_get_units_distance ();
      gdouble diff_dist = vik_coord_diff ( &coord, &(s->oldcoord) );
      vu_distance_text ( tmp_buf, sizeof(tmp_buf), dist_units, diff_dist, TRUE, "%.3f", FALSE );
      temp = g_strdup_printf ( _("%s %s DIFF %s"), lat, lon, tmp_buf );
      s->has_oldcoord = FALSE;
    }
    else {
      temp = g_strdup_printf ( "%s %s", lat, lon );
      s->has_oldcoord = TRUE;
    }

    vik_statusbar_set_message ( s->vw->viking_vs, VIK_STATUSBAR_INFO, temp );
    g_free ( temp );
    g_free ( lat );
    g_free ( lon );

    s->oldcoord = coord;
  }
  else {
    vik_viewport_set_center_screen ( s->vw->viking_vvp, (gint) event->x, (gint) event->y );
    draw_update ( s->vw );
  }
}

static VikLayerToolFuncStatus ruler_click (VikLayer *vl, GdkEventButton *event, tool_ed_t *te)
{
  te->bounds_active = FALSE;
  if ( event->button == 1 && event->state & GDK_SHIFT_MASK ) {
    // (re)start bounds
    te->bounds_active = TRUE;
    te->start_x = (gint)event->x;
    te->start_y = (gint)event->y;
    te->displayed = TRUE;
  } else {
    ruler_click_normal ( vl, event, te );
  }
  return VIK_LAYER_TOOL_ACK;
}

static void tool_resize_drawing_area (tool_ed_t *te, guint thickness, const gchar *color );
static void tool_redraw_drawing_area_box (tool_ed_t *te, GdkEventMotion *event);

static void ruler_move_normal (VikLayer *vl, GdkEventMotion *event, tool_ed_t *s)
{
  VikWindow *vw = s->vw;
  VikViewport *vvp = s->vw->viking_vvp;
  struct LatLon ll;
  VikCoord coord;
  gchar *temp;

  if ( s->has_oldcoord ) {
    gchar *lat=NULL, *lon=NULL;
    vik_viewport_screen_to_coord ( vvp, (gint) event->x, (gint) event->y, &coord );
    vik_coord_to_latlon ( &coord, &ll );

#if GTK_CHECK_VERSION (3,0,0)
    tool_resize_drawing_area ( s, vik_viewport_get_scale(vvp), "#000000" );
    if ( s->gc ) {
      draw_ruler ( vvp, NULL, s->gc, &s->oldcoord, &coord );
      gtk_widget_queue_draw ( GTK_WIDGET(vvp) );
    }
#else
    int w1, h1, w2, h2;
    static GdkPixmap *buf = NULL;
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

    gdk_draw_drawable (buf, vik_viewport_get_black_gc(vvp), vik_viewport_get_pixmap(vvp), 0, 0, 0, 0, -1, -1);
    draw_ruler(vvp, buf, vik_viewport_get_black_gc(vvp), &s->oldcoord, &coord );
    if (draw_buf_done) {
      static gpointer pass_along[3];
      pass_along[0] = gtk_widget_get_window(GTK_WIDGET(vvp));
      pass_along[1] = vik_viewport_get_black_gc ( vvp );
      pass_along[2] = buf;
      (void)g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, draw_buf, pass_along, NULL);
      draw_buf_done = FALSE;
    }
#endif
    a_coords_latlon_to_string(&ll, &lat, &lon);
    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    static gchar tmp_buf[64];
    gdouble diff_dist = vik_coord_diff ( &coord, &(s->oldcoord) );
    vu_distance_text ( tmp_buf, sizeof(tmp_buf), dist_units, diff_dist, TRUE, "%.3f", FALSE );
    temp = g_strdup_printf ( _("%s %s DIFF %s"), lat, lon, tmp_buf );
    vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, temp );
    g_free ( temp );
    g_free ( lat );
    g_free ( lon );
  }
}

/**
 * Draw a boxed label on the viewport somewhere in the xy coords
 *  as specified by the positional parameter
 */
static void draw_boxed_label (VikViewport *vvp, GdkDrawable *d, GdkGC *gc, gint x1, gint y1, gint x2, gint y2, vik_positional_t pos, gchar *str)
{
  gint wd, hd, xd, yd;
  PangoLayout *pl = gtk_widget_create_pango_layout (GTK_WIDGET(vvp), NULL);
  pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(vvp))->font_desc);
  pango_layout_set_text(pl, str, -1);
  pango_layout_get_pixel_size ( pl, &wd, &hd );

  xd = (x1+x2)/2 - wd/2;
  switch (pos) {
  case VIK_POSITIONAL_MIDDLE:
    yd = (y1+y2)/2 - hd/2;
    break;
  case VIK_POSITIONAL_TOP:
    yd = y1 + 5;
    break;
  default:
    // VIK_POSITIONAL_BOTTOM | NONE (obvs this shouldn't be called with NONE)
    yd = y2 - hd - 5;
  break;
  }

#if GTK_CHECK_VERSION (3,0,0)
  ui_cr_label_with_bg ( gc, xd, yd, wd, hd, pl );
#else
  GdkGC *labgc = vik_viewport_new_gc ( vvp, "#cccccc", 1);
#define LABEL(x, y, w, h) { \
    gdk_draw_rectangle(d, labgc, TRUE, (x)-2, (y)-1, (w)+4, (h)+1); \
    gdk_draw_rectangle(d, gc, FALSE, (x)-2, (y)-1, (w)+4, (h)+1); \
    gdk_draw_layout(d, gc, (x), (y), pl); }
  LABEL(xd, yd, wd, hd);
#undef LABEL
  g_object_unref ( G_OBJECT ( labgc ) );
#endif
  g_object_unref ( G_OBJECT ( pl ) );
}

static void ruler_move_shift (VikLayer *vl, GdkEventMotion *event, tool_ed_t *te)
{
  VikViewport *vvp = te->vw->viking_vvp;
  tool_resize_drawing_area ( te, 2*vik_viewport_get_scale(vvp), "#000000" );
  tool_redraw_drawing_area_box ( te, event);
  gchar *str;
  gdouble zoom = vik_viewport_get_zoom ( vvp );
  if ( zoom < 64.0 ) {
    VikCoord tl,tr,bl;
    vik_viewport_screen_to_coord ( vvp, te->start_x, te->start_y, &tl );
    vik_viewport_screen_to_coord ( vvp, (gint)event->x, te->start_y, &bl );
    vik_viewport_screen_to_coord ( vvp, te->start_y, (gint)event->y, &tr );

    gdouble ydiff = vik_coord_diff ( &tl, &bl );
    gdouble xdiff = vik_coord_diff ( &tl, &tr );
    /*
     * Very basic area estimation for coords that are assumed to form a rectangle as a flat projection
     * i.e. only approximation only works for high zoom levels and also increasingly worse at high latitudes
     */
    gdouble area = ydiff * xdiff;

    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_MILES:
      str = g_strdup_printf ( _("Area: %.3f miles * %.3f miles = %.3f sq. miles"), VIK_METERS_TO_MILES(xdiff),  VIK_METERS_TO_MILES(ydiff), area/2589988.11);
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      str = g_strdup_printf ( _("Area: %.3f NM * %.3f NM = %.3f sq. NM"), VIK_METERS_TO_NAUTICAL_MILES(xdiff), VIK_METERS_TO_NAUTICAL_MILES(ydiff), area/(1852.0*1852.0));
      break;
    default:
      //case VIK_UNITS_DISTANCE_KILOMETRES:
      str = g_strdup_printf ( _("Area: %.3f km * %.3f km = %.3f sq. km"), (ydiff/1000.0), (xdiff/1000.0), area/1000000.0);
      break;
    }

    vik_positional_t label_pos = a_vik_get_ruler_area_label_pos();
    if ( label_pos != VIK_POSITIONAL_NONE ) {
#if GTK_CHECK_VERSION (3,0,0)
      draw_boxed_label ( vvp, NULL, te->gc, te->start_x, te->start_y, (gint)event->x, (gint)event->y, label_pos, str );
#else
      draw_boxed_label ( vvp, te->pixmap, vik_viewport_get_black_gc(te->vw->viking_vvp), te->start_x, te->start_y, (gint)event->x, (gint)event->y, label_pos, str );
#endif
    }
  }
  else {
    str = g_strdup ( _("Area approximation not valid at this zoom level") );
  }
  vik_statusbar_set_message ( te->vw->viking_vs, VIK_STATUSBAR_INFO, str );
  g_free ( str );
}

static VikLayerToolFuncStatus ruler_move (VikLayer *vl, GdkEventMotion *event, tool_ed_t *te)
{
  if ( te->bounds_active && event->state & GDK_SHIFT_MASK ) {
    ruler_move_shift ( vl, event, te );
  } else {
    ruler_move_normal ( vl, event, te );
  }
  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus ruler_release (VikLayer *vl, GdkEventButton *event, tool_ed_t *te)
{
  if ( event->button == 1 && te->bounds_active ) {
    te->bounds_active = FALSE;
  }
  return VIK_LAYER_TOOL_ACK;
}

// Common handler for all tools
static gboolean tool_key_press_common ( VikLayer *vl, GdkEventKey *event, gpointer unused_data )
{
  // Pretend to handle arrow keys to avoid GTK widget focus change away from the viewport
  if ( event->keyval == GDK_KEY_Right || event->keyval == GDK_KEY_Left ||
       event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down )
    return TRUE;
  return FALSE;
}

/**
 * Generic move viewport on an arrow key event (i.e. could be either press or release)
 */
static gboolean move_arrow_key_event ( VikWindow *vw, guint keyval )
{
  gboolean ans = FALSE;
  if ( keyval == GDK_KEY_Up ) {
    vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, 0 );
    ans = TRUE;
  }
  else if ( keyval == GDK_KEY_Right ) {
    vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp), vik_viewport_get_height(vw->viking_vvp)/2 );
    ans = TRUE;
  }
  else if ( keyval == GDK_KEY_Down ) {
    vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2, vik_viewport_get_height(vw->viking_vvp) );
    ans = TRUE;
  }
  else if ( keyval == GDK_KEY_Left ) {
    vik_viewport_set_center_screen ( vw->viking_vvp, 0, vik_viewport_get_height(vw->viking_vvp)/2 );
    ans = TRUE;
  }

  if ( ans )
    draw_update ( vw );

  return ans;
}

static gboolean tool_key_release_common ( VikLayer *vl, GdkEventKey *event, VikWindow *vw )
{
  // NB not only do we get standard single key events,
  //  it also still gets called after the action callback draw_pan_cb() from ctrl+arrow key
  gboolean cmask = event->state & GDK_CONTROL_MASK;
  // So don't do anything if ctrl+arrow (as already viewport has already been moved)
  if ( !cmask )
    if ( move_arrow_key_event(vw, event->keyval) )
      return TRUE;
  return FALSE;
}

static gboolean tool_key_release_common_tool_edit ( VikLayer *vl, GdkEventKey *event, tool_ed_t *te )
{
  return tool_key_release_common ( vl, event, te->vw );
}

static void ruler_deactivate (VikLayer *ignore, tool_ed_t *s)
{
  tool_edit_remove_image ( s );
  (void)draw_sync ( s->vw );
}

static gboolean ruler_key_press (VikLayer *vl, GdkEventKey *event, tool_ed_t *s)
{
  if (event->keyval == GDK_KEY_Escape) {
    if ( s->displayed ) {
      s->has_oldcoord = FALSE;
      ruler_deactivate ( NULL, s );
      s->displayed = FALSE;
      return TRUE;
    }
  }
  if ( tool_key_press_common(vl, event, s) )
    return TRUE;
  // Regardless of whether we used it, return false so other GTK things may use it
  return FALSE;
}

static VikToolInterface ruler_tool =
  // NB Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead
  { "ruler_18",
    { "Ruler", "ruler_18", N_("_Ruler"), "<control><shift>U", N_("Ruler Tool"), TOOL_RULER },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) ruler_deactivate, 
    (VikToolMouseFunc) ruler_click,
    (VikToolMouseMoveFunc) ruler_move,
    (VikToolMouseFunc) ruler_release,
    (VikToolKeyFunc) ruler_key_press,
    (VikToolKeyFunc) tool_key_release_common_tool_edit,
    FALSE,
    GDK_CURSOR_IS_PIXMAP,
    "cursor_ruler",
    NULL };
/*** end ruler code ********************************************************/



/********************************************************************************
 ** Zoom tool code
 ********************************************************************************/

/*
 * In case the screen size has changed
 */
static void tool_resize_drawing_area (tool_ed_t *te, guint thickness, const gchar *color )
{
    int w1, h1, w2, h2;
    // Allocate a drawing area the size of the viewport
    w1 = vik_viewport_get_width ( te->vw->viking_vvp );
    h1 = vik_viewport_get_height ( te->vw->viking_vvp );

#if GTK_CHECK_VERSION (3,0,0)
    if ( te->gc ) {
      cairo_surface_t *surface = vik_viewport_surface_tool_get(te->vvp);
      if ( surface ) {
        w2 = cairo_image_surface_get_width ( surface );
        h2 = cairo_image_surface_get_height ( surface );
        if ( w1 != w2 || h1 != h2 )
          tool_edit_remove_image ( te );
      }
    }
    if ( !te->gc ) {
      te->gc = vik_viewport_surface_tool_create ( te->vvp );
      if ( te->gc ) {
        ui_cr_set_color ( te->gc, color );
        cairo_set_line_width ( te->gc, thickness*vik_viewport_get_scale(te->vvp) );
      }
    }
#else
    if ( !te->pixmap ) {
      // Totally new
      te->pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(te->vw->viking_vvp)), w1, h1, -1 );
    }

    gdk_drawable_get_size ( te->pixmap, &w2, &h2 );

    if ( w1 != w2 || h1 != h2 ) {
      // Has changed - delete and recreate with new values
      g_object_unref ( G_OBJECT ( te->pixmap ) );
      te->pixmap = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(te->vw->viking_vvp)), w1, h1, -1 );
    }
#endif
}

/**
 * Draw outline box in the tool
 */
static void tool_redraw_drawing_area_box (tool_ed_t *te, GdkEventMotion *event)
{
    // Calculate new box starting point & size in pixels
    int xx, yy, width, height;
    if ( event->y > te->start_y ) {
      yy = te->start_y;
      height = event->y-te->start_y;
    }
    else {
      yy = event->y;
      height = te->start_y-event->y;
    }
    if ( event->x > te->start_x ) {
      xx = te->start_x;
      width = event->x-te->start_x;
    }
    else {
      xx = event->x;
      width = te->start_x-event->x;
    }

#if GTK_CHECK_VERSION (3,0,0)
    if ( te->gc ) {
      ui_cr_clear ( te->gc );
      ui_cr_draw_rectangle ( te->gc, FALSE, xx, yy, width, height );
      cairo_stroke ( te->gc );
      gtk_widget_queue_draw ( GTK_WIDGET(te->vw->viking_vvp) );
    }
#else
    // Blank out currently drawn area
    gdk_draw_drawable ( te->pixmap,
                        vik_viewport_get_black_gc(te->vw->viking_vvp),
                        vik_viewport_get_pixmap(te->vw->viking_vvp),
                        0, 0, 0, 0, -1, -1 );

    // Draw the box
    gdk_draw_rectangle (te->pixmap, vik_viewport_get_black_gc(te->vw->viking_vvp), FALSE, xx, yy, width, height);

    // Only actually draw when there's time to do so
    if (draw_buf_done) {
      static gpointer pass_along[3];
      pass_along[0] = gtk_widget_get_window(GTK_WIDGET(te->vw->viking_vvp));
      pass_along[1] = vik_viewport_get_black_gc ( te->vw->viking_vvp );
      pass_along[2] = te->pixmap;
      (void)g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, draw_buf, pass_along, NULL);
      draw_buf_done = FALSE;
    }
#endif
}

static VikLayerToolFuncStatus zoomtool_click (VikLayer *vl, GdkEventButton *event, tool_ed_t *te)
{
  te->vw->modified = TRUE;
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

  gint center_x = vik_viewport_get_width ( te->vw->viking_vvp ) / 2;
  gint center_y = vik_viewport_get_height ( te->vw->viking_vvp ) / 2;

  gboolean skip_update = FALSE;

  te->bounds_active = FALSE;

  if ( modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) ) {
    // This zoom is on the center position
    vik_viewport_set_center_screen ( te->vw->viking_vvp, center_x, center_y );
    if ( event->button == 1 )
      vik_viewport_zoom_in (te->vw->viking_vvp);
    else if ( event->button == 3 )
      vik_viewport_zoom_out (te->vw->viking_vvp);
  }
  else if ( modifiers == GDK_CONTROL_MASK ) {
    // This zoom is to recenter on the mouse position
    vik_viewport_set_center_screen ( te->vw->viking_vvp, (gint) event->x, (gint) event->y );
    if ( event->button == 1 )
      vik_viewport_zoom_in (te->vw->viking_vvp);
    else if ( event->button == 3 )
      vik_viewport_zoom_out (te->vw->viking_vvp);
  }
  else if ( modifiers == GDK_SHIFT_MASK ) {
    // Get start of new zoom bounds
    if ( event->button == 1 ) {
      te->bounds_active = TRUE;
      te->start_x = (gint) event->x;
      te->start_y = (gint) event->y;
      skip_update = TRUE;
    }
  }
  else {
    /* make sure mouse is still over the same point on the map when we zoom */
    zoom_at_xy ( te->vw, event->x, event->y, FALSE, GDK_SCROLL_UP, event->button );
  }

  if ( !skip_update )
    draw_update ( te->vw );

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_move (VikLayer *vl, GdkEventMotion *event, tool_ed_t *te)
{
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

  if ( te->bounds_active && modifiers == GDK_SHIFT_MASK ) {
    tool_resize_drawing_area ( te, 2, "#000000" );
    tool_redraw_drawing_area_box ( te, event );
  }
  else
    te->bounds_active = FALSE;

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_release (VikLayer *vl, GdkEventButton *event, tool_ed_t *te)
{
  guint modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

  // Ensure haven't just released on the exact same position
  //  i.e. probably haven't moved the mouse at all
  if ( te->bounds_active && modifiers == GDK_SHIFT_MASK &&
     ( event->x < te->start_x-5 || event->x > te->start_x+5 ) &&
     ( event->y < te->start_y-5 || event->y > te->start_y+5 ) ) {

    VikCoord coord1, coord2;
    vik_viewport_screen_to_coord ( te->vw->viking_vvp, te->start_x, te->start_y, &coord1);
    vik_viewport_screen_to_coord ( te->vw->viking_vvp, event->x, event->y, &coord2);

    // From the extend of the bounds pick the best zoom level
    // c.f. trw_layer_zoom_to_show_latlons()
    // Maybe refactor...
    struct LatLon ll1, ll2;
    vik_coord_to_latlon(&coord1, &ll1);
    vik_coord_to_latlon(&coord2, &ll2);
    struct LatLon average = { (ll1.lat+ll2.lat)/2,
			      (ll1.lon+ll2.lon)/2 };

    VikCoord new_center;
    vik_coord_load_from_latlon ( &new_center, vik_viewport_get_coord_mode ( te->vw->viking_vvp ), &average );
    vik_viewport_set_center_coord ( te->vw->viking_vvp, &new_center, FALSE );

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
    vik_viewport_set_zoom ( te->vw->viking_vvp, zoom );

    gdouble min_lat, max_lat, min_lon, max_lon;
    /* Should only be a maximum of about 18 iterations from min to max zoom levels */
    while ( zoom <= VIK_VIEWPORT_MAX_ZOOM ) {
      vik_viewport_get_min_max_lat_lon ( te->vw->viking_vvp, &min_lat, &max_lat, &min_lon, &max_lon );
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
      vik_viewport_set_zoom ( te->vw->viking_vvp, zoom );
    }
  }
  else {
     // When pressing shift and clicking for zoom, then jump three levels
     if ( modifiers == GDK_SHIFT_MASK ) {
       // Zoom in/out by three if possible
       vik_viewport_set_center_screen ( te->vw->viking_vvp, event->x, event->y );
       if ( event->button == 1 ) {
          vik_viewport_zoom_in ( te->vw->viking_vvp );
          vik_viewport_zoom_in ( te->vw->viking_vvp );
          vik_viewport_zoom_in ( te->vw->viking_vvp );
       }
       else if ( event->button == 3 ) {
          vik_viewport_zoom_out ( te->vw->viking_vvp );
          vik_viewport_zoom_out ( te->vw->viking_vvp );
          vik_viewport_zoom_out ( te->vw->viking_vvp );
       }
     }
  }

  tool_edit_remove_image ( te );

  draw_update ( te->vw );

  // Reset
  te->bounds_active = FALSE;

  return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface zoom_tool = 
  { "zoom_18",
    { "Zoom", "zoom_18", N_("_Zoom"), "<control><shift>Z", N_("Zoom Tool"), TOOL_ZOOM },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) zoomtool_click, 
    (VikToolMouseMoveFunc) zoomtool_move,
    (VikToolMouseFunc) zoomtool_release,
    (VikToolKeyFunc) tool_key_press_common,
    (VikToolKeyFunc) tool_key_release_common_tool_edit,
    FALSE,
    GDK_CURSOR_IS_PIXMAP,
    "cursor_zoom",
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
  { "mover_22",
    { "Pan", "mover_22", N_("_Pan"), "<control><shift>P", N_("Pan Tool"), TOOL_PAN },
    (VikToolConstructorFunc) pantool_create,
    (VikToolDestructorFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) pantool_click, 
    (VikToolMouseMoveFunc) pantool_move,
    (VikToolMouseFunc) pantool_release,
    (VikToolKeyFunc) tool_key_press_common,
    (VikToolKeyFunc) tool_key_release_common,
    FALSE,
    GDK_FLEUR,
    NULL,
    NULL };
/*** end pan code ********************************************************/

/********************************************************************************
 ** Select tool code
 ********************************************************************************/

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

static gboolean window_deselect (VikWindow *vw)
{
  GtkTreeIter iter;
  VikTreeview *vtv = vik_layers_panel_get_treeview ( vw->viking_vlp );
  if ( vik_treeview_get_selected_iter ( vtv, &iter ) ) {
    // Only clear if selected thing is a TrackWaypoint layer or a sublayer
    gint type = vik_treeview_item_get_type ( vtv, &iter );
    if ( type == VIK_TREEVIEW_TYPE_SUBLAYER ||
         VIK_LAYER(vik_treeview_item_get_pointer ( vtv, &iter ))->type == VIK_LAYER_TRW ) {

      vik_treeview_item_unselect ( vtv, &iter );
      if ( vik_window_clear_selected(vw) )
        draw_update ( vw );
    }
  }
  vik_window_selected_layer ( vw, NULL );
  vw->deselect_id = 0;
  return FALSE;
}

static gboolean window_show_menu (VikWindow *vw)
{
  VikLayer *vl = vik_layers_panel_get_selected ( vw->viking_vlp );
  if ( vl )
    if ( vl->visible )
      // Act on currently selected item to show menu
      if ( vw->selected_track || vw->selected_waypoint || vl->type == VIK_LAYER_AGGREGATE )
        if ( vik_layer_get_interface(vl->type)->show_viewport_menu ) {
          (void)vik_layer_get_interface(vl->type)->show_viewport_menu ( vl, &vw->select_event, vw->viking_vvp );
        }
  vw->show_menu_id = 0;
  return FALSE;
}

static VikLayerToolFuncStatus selecttool_click (VikLayer *vl, GdkEventButton *event, tool_ed_t *t)
{
  t->vw->select_double_click = (event->type == GDK_2BUTTON_PRESS);
  t->vw->select_double_click_button = event->button;
  t->vw->deselect_on_release = FALSE;

  // Don't process these any further
  if ( event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS ) {
    vik_window_pan_click ( t->vw, event );
    return VIK_LAYER_TOOL_ACK;
  }

  t->vw->select_move = FALSE;
  t->vw->select_pan = FALSE;
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
          // Deselecton now performed on release (if mouse not moved in between)
          t->vw->deselect_on_release = TRUE;
        }

        // Go into pan mode as nothing found
        t->vw->select_pan = TRUE;
        vik_window_pan_click ( t->vw, event );
      }
      else {
        // Something found - so enable movement
        t->vw->select_move = TRUE;
      }
    }
  }
  else if ( ( event->button == 3 ) && ( vl && (vl->type == VIK_LAYER_TRW || vl->type == VIK_LAYER_AGGREGATE) ) ) {
    t->vw->select_event = *event;
    if ( a_vik_get_select_double_click_to_zoom() && !t->vw->show_menu_id ) {
      // Best if slightly longer than the double click time,
      //  otherwise timeout would get removed, only to be recreated again by the second GTK_BUTTON_PRESS
      GtkSettings *gs = gtk_widget_get_settings ( GTK_WIDGET(t->vw) );
      GValue gto = G_VALUE_INIT;
      g_value_init ( &gto, G_TYPE_INT );
      g_object_get_property ( G_OBJECT(gs), "gtk-double-click-time", &gto );
      gint timer = g_value_get_int ( &gto ) + 50;
      t->vw->show_menu_id = g_timeout_add ( timer, (GSourceFunc)window_show_menu, t->vw );
    } else
      // Not using double clicks - so no need to wait and thus apply now
      (void)window_show_menu ( t->vw );
  }

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_move (VikLayer *vl, GdkEventMotion *event, tool_ed_t *t)
{
  if ( t->vw->select_move ) {
    // Don't care about vl here
    if ( t->vtl )
      if ( vik_layer_get_interface(VIK_LAYER_TRW)->select_move )
        (void)vik_layer_get_interface(VIK_LAYER_TRW)->select_move ( vl, event, t->vvp, t );
  }
  else
    // Optional Panning
    if ( t->vw->select_pan || event->state & VIK_MOVE_MODIFIER ) {
      // Abort deselection
      t->vw->deselect_on_release = FALSE;
      if ( t->vw->deselect_id ) {
        g_source_remove ( t->vw->deselect_id );
        t->vw->deselect_id = 0;
      }
      vik_window_pan_move ( t->vw, event );
    }

  return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_release (VikLayer *vl, GdkEventButton *event, tool_ed_t *t)
{
  gboolean do_draw = FALSE;
  if ( t->vw->select_move ) {
    // Don't care about vl here
    if ( t->vtl )
      if ( vik_layer_get_interface(VIK_LAYER_TRW)->select_release )
        (void)vik_layer_get_interface(VIK_LAYER_TRW)->select_release ( (VikLayer*)t->vtl, event, t->vvp, t );
  } else {
    if ( t->vw->deselect_on_release ) {
      // Only have one pending deselection timeout
      if ( a_vik_get_select_double_click_to_zoom() && !t->vw->deselect_id ) {
        // Best if slightly longer than the double click time,
        //  otherwise timeout would get removed, only to be recreated again by the second GTK_BUTTON_PRESS
        GtkSettings *gs = gtk_widget_get_settings ( GTK_WIDGET(t->vw) );
        GValue gto = G_VALUE_INIT;
        g_value_init ( &gto, G_TYPE_INT );
        g_object_get_property ( G_OBJECT(gs), "gtk-double-click-time", &gto );
        gint timer = g_value_get_int ( &gto ) + 50;
        t->vw->deselect_id = g_timeout_add ( timer, (GSourceFunc)window_deselect, t->vw );
      } else
        // Not using double clicks - so no need to wait and thus apply now
        (void)window_deselect ( t->vw );
    }
    else if ( event->button == 1 && event->state & VIK_MOVE_MODIFIER )
      vik_window_pan_release ( t->vw, event );
    else {
      if ( a_vik_get_select_double_click_to_zoom() && t->vw->select_double_click ) {
        // Turn off otherwise pending deselection - as now overridden by the double click
        if ( t->vw->deselect_id ) {
          g_source_remove ( t->vw->deselect_id );
          t->vw->deselect_id = 0;
        }
        // Turn off otherwise pending show menu - as now overridden by the double click
        if ( t->vw->show_menu_id ) {
          g_source_remove ( t->vw->show_menu_id );
          t->vw->show_menu_id = 0;
        }

        // Invert the zoom direction if necessary
        guint modifier = event->state & GDK_SHIFT_MASK;
        if ( modifier && (t->vw->select_double_click_button == 1) ) {
          t->vw->select_double_click_button = 3;
        }
        zoom_at_xy ( t->vw, t->vw->pan_x, t->vw->pan_y, FALSE, GDK_SCROLL_UP, t->vw->select_double_click_button );

        draw_update ( t->vw );
        t->vw->select_double_click = FALSE;
      }
      else {
        do_draw = t->vw->select_pan;
      }
    }
  }

  // Force pan off incase it was on
  t->vw->select_pan = FALSE;
  t->vw->pan_move = FALSE;
  t->vw->pan_x = t->vw->pan_y = -1;

  // End of this select movement
  t->vw->select_move = FALSE;
  t->vw->deselect_on_release = FALSE;

  // Final end of the select movement
  //  (redraw to trigger potential map downloads as now 'pan_move' is off)
  if ( do_draw )
    draw_update ( t->vw );
  t->vw->select_pan = FALSE;

  return VIK_LAYER_TOOL_ACK;
}

// Fast response / multiple rapid reptitions useful
static gboolean selecttool_key_press ( VikLayer *vl, GdkEventKey *event, tool_ed_t *te )
{
  if (vl && (vl->type == VIK_LAYER_TRW) && (vl->visible) ) {
    if ( te->vw->containing_vtl ) {
      if ( event->keyval == GDK_KEY_Left ) {
        vik_trw_layer_goto_track_prev_point ( te->vw->containing_vtl );
        return TRUE;
      }
      else if ( event->keyval == GDK_KEY_Right ) {
        vik_trw_layer_goto_track_next_point ( te->vw->containing_vtl );
        return TRUE;
      }
    }
  }

  if ( tool_key_press_common(vl, event, te) )
    return TRUE;
  return FALSE;
}

static gboolean selecttool_key_release (VikLayer *vl, GdkEventKey *event, tool_ed_t *t)
{
  if (vl && (vl->type == VIK_LAYER_TRW)) {
    if ( vl->visible ) {
      if ( event->keyval == GDK_KEY_Menu ) {
        /* Act on currently selected item to show menu */
        if ( t->vw->selected_track || t->vw->selected_waypoint )
          if ( vik_layer_get_interface(vl->type)->show_viewport_menu )
            return vik_layer_get_interface(vl->type)->show_viewport_menu ( vl, NULL, t->vw->viking_vvp );
      }
      else if ( t->vw->containing_vtl ) {
        if ( (event->keyval == GDK_KEY_bracketleft) || (event->keyval == GDK_KEY_KP_Subtract) ) {
          vik_trw_layer_insert_tp_beside_current_tp ( t->vw->containing_vtl, TRUE );
          return TRUE;
        }
        else if ( (event->keyval == GDK_KEY_bracketright) || (event->keyval == GDK_KEY_KP_Add) ) {
          vik_trw_layer_insert_tp_beside_current_tp ( t->vw->containing_vtl, FALSE );
          return TRUE;
        }
        // Ctrl-D to delete (rather than delete key or BackSpace)
        else if ( (event->keyval == GDK_KEY_d) && (event->state & GDK_CONTROL_MASK) ) {
          if ( t->vw->selected_waypoint )
            vik_trw_layer_delete_waypoint ( t->vw->containing_vtl, t->vw->selected_waypoint );
          else
            vik_trw_layer_delete_trackpoint_selected ( t->vw->containing_vtl );
          return TRUE;
        }
      }
    }
  } else {
    if ( tool_key_release_common(vl, event, t->vw) )
      return TRUE;
  }
  return FALSE;
}

static VikToolInterface select_tool =
  { "select_18",
    { "Select", "select_18", N_("_Select"), "<control><shift>C", N_("Select Tool"), TOOL_SELECT },
    (VikToolConstructorFunc) tool_edit_create,
    (VikToolDestructorFunc) tool_edit_destroy,
    (VikToolActivationFunc) NULL,
    (VikToolActivationFunc) NULL,
    (VikToolMouseFunc) selecttool_click,
    (VikToolMouseMoveFunc) selecttool_move,
    (VikToolMouseFunc) selecttool_release,
    (VikToolKeyFunc) selecttool_key_press,
    (VikToolKeyFunc) selecttool_key_release,
    FALSE,
    GDK_LEFT_PTR,
    NULL,
    NULL };
/*** end select tool code ********************************************************/


gpointer vik_window_get_active_tool_data ( VikWindow *vw )
{
  gpointer ans = NULL;
  if ( vw->vt->active_tool >= 0 && vw->vt->active_tool < vw->vt->n_tools )
    ans = vw->vt->tools[vw->vt->active_tool].state;
  return ans;
}

gpointer vik_window_get_active_tool_interface ( VikWindow *vw )
{
  gpointer ans = NULL;
  if ( vw->vt->active_tool >= 0 && vw->vt->active_tool < vw->vt->n_tools )
    ans = &vw->vt->tools[vw->vt->active_tool].ti;
  return ans;
}

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

static void calendar_goto_today ( GtkAction *a, VikWindow *vw )
{
  vik_layers_panel_calendar_today ( vw->viking_vlp );
}

static void menu_addlayer_cb ( GtkAction *a, VikWindow *vw )
{
 VikLayerTypeEnum type;
  for ( type = 0; type < VIK_LAYER_NUM_TYPES; type++ ) {
    if (!strcmp(vik_layer_get_interface(type)->fixed_layer_name, gtk_action_get_name(a))) {
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
  uri = g_strdup_printf("help:%s", PACKAGE);
  GError *error = NULL;
#if GTK_CHECK_VERSION (3,22,0)
  gboolean show = gtk_show_uri_on_window ( GTK_WINDOW(vw), uri, GDK_CURRENT_TIME, &error );
#else
  gboolean show = gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, &error);
#endif
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

static void toggle_debug_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean state = !vik_debug;
  GtkWidget *check_box = gtk_ui_manager_get_widget ( vw->uim, "/ui/MainMenu/Help/HelpDebug" );
  if ( !check_box )
    return;
  gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), state );
  vik_debug = state;
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

static void toggle_track_graphs ( VikWindow *vw )
{
  vw->show_track_graphs = !vw->show_track_graphs;
  if ( vw->show_track_graphs )
    gtk_widget_show ( GTK_WIDGET(vw->graphs) );
  else
    gtk_widget_hide ( GTK_WIDGET(vw->graphs) );
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

static void toggle_side_panel_buttons ( VikWindow *vw )
{
  vw->show_side_panel_buttons = !vw->show_side_panel_buttons;
  vik_layers_panel_show_buttons ( vw->viking_vlp, vw->show_side_panel_buttons );
}

static void toggle_side_panel_tabs ( VikWindow *vw )
{
  vw->show_side_panel_tabs = !vw->show_side_panel_tabs;
  vik_layers_panel_show_tabs ( vw->viking_vlp, vw->show_side_panel_tabs );
}

static void toggle_side_panel_calendar ( VikWindow *vw )
{
  vw->show_side_panel_calendar = !vw->show_side_panel_calendar;
  vik_layers_panel_show_calendar ( vw->viking_vlp, vw->show_side_panel_calendar );
}

static void toggle_side_panel_goto ( VikWindow *vw )
{
  vw->show_side_panel_goto = !vw->show_side_panel_goto;
  vik_layers_panel_show_goto ( vw->viking_vlp, vw->show_side_panel_goto );
}

static void toggle_side_panel_stats ( VikWindow *vw )
{
  vw->show_side_panel_stats = !vw->show_side_panel_stats;
  vik_layers_panel_show_stats ( vw->viking_vlp, vw->show_side_panel_stats );
}

static void toggle_side_panel_splits ( VikWindow *vw )
{
  vw->show_side_panel_splits = !vw->show_side_panel_splits;
  vik_layers_panel_show_splits ( vw->viking_vlp, vw->show_side_panel_splits );
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

static void tb_view_track_graphs_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_track_graphs;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_track_graphs ( vw );
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

static void tb_view_side_panel_buttons_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_buttons;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_side_panel_buttons ( vw );
}

static void tb_view_side_panel_tabs_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_tabs;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_side_panel_tabs ( vw );
}

static void tb_view_side_panel_calendar_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_calendar;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_side_panel_calendar ( vw );
}

static void tb_view_side_panel_goto_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_goto;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_side_panel_goto ( vw );
}

static void tb_view_side_panel_stats_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_stats;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_side_panel_stats ( vw );
}

static void tb_view_side_panel_splits_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_splits;
  GtkWidget *check_box = get_show_widget_by_name ( vw, gtk_action_get_name(a) );
  gboolean menu_state = gtk_check_menu_item_get_active ( GTK_CHECK_MENU_ITEM(check_box) );
  if ( next_state != menu_state )
    gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(check_box), next_state );
  else
    toggle_side_panel_splits ( vw );
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
  gchar *msg_sz = g_format_size_full ( byte_size, G_FORMAT_SIZE_LONG_FORMAT );
  gchar *msg = g_strdup_printf ( "Map Cache size is %s with %d items", msg_sz, a_mapcache_get_count());
  a_dialog_info_msg_extra ( GTK_WINDOW(vw), "%s", msg );
  g_free ( msg_sz );
  g_free ( msg );
}

static void back_forward_info_cb ( GtkAction *a, VikWindow *vw )
{
  vik_viewport_show_centers ( vw->viking_vvp, GTK_WINDOW(vw) );
}

static void build_info_cb ( GtkAction *a, VikWindow *vw )
{
  a_dialog_build_info ( GTK_WINDOW(vw) );
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


static void view_track_graphs_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_track_graphs;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_track_graphs ( vw );
  }
  else
    toggle_track_graphs ( vw );
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
    toggle_main_menu ( vw );
}

static void view_side_panel_buttons_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_buttons;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_side_panel_buttons ( vw );
  }
  else
    toggle_side_panel_buttons ( vw );
}

static void view_side_panel_tabs_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_tabs;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_side_panel_tabs ( vw );
  }
  else
    toggle_side_panel_tabs ( vw );
}

static void view_side_panel_calendar_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_calendar;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_side_panel_calendar ( vw );
  }
  else
    toggle_side_panel_calendar ( vw );
}

static void view_side_panel_goto_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_goto;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_side_panel_goto ( vw );
  }
  else
    toggle_side_panel_goto ( vw );
}

static void view_side_panel_stats_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_stats;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_side_panel_stats ( vw );
  }
  else
    toggle_side_panel_stats ( vw );
}

static void view_side_panel_splits_cb ( GtkAction *a, VikWindow *vw )
{
  gboolean next_state = !vw->show_side_panel_splits;
  GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name ( vw->viking_vtb, gtk_action_get_name(a) );
  if ( tbutton ) {
    gboolean tb_state = gtk_toggle_tool_button_get_active ( tbutton );
    if ( next_state != tb_state )
      gtk_toggle_tool_button_set_active ( tbutton, next_state );
    else
      toggle_side_panel_splits ( vw );
  }
  else
    toggle_side_panel_splits ( vw );
}

/***************************************
 ** tool management routines
 **
 ***************************************/

static toolbox_tools_t* toolbox_create(VikWindow *vw)
{
  toolbox_tools_t *vt = g_new0(toolbox_tools_t, 1);
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
    if (t->ti.cursor_type == GDK_CURSOR_IS_PIXMAP && t->ti.cursor_name != NULL) {
      GdkPixbuf *cursor_pixbuf = ui_get_icon ( t->ti.cursor_name, 32 );
      /* TODO: settable offeset */
      t->ti.cursor = gdk_cursor_new_from_pixbuf ( gdk_display_get_default(), cursor_pixbuf, 3, 3 );
      g_object_unref ( G_OBJECT(cursor_pixbuf) );
    } else {
      t->ti.cursor = gdk_cursor_new_for_display ( gtk_widget_get_display(GTK_WIDGET(vt->vw)), t->ti.cursor_type );
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
      (void)vt->tools[vt->active_tool].ti.click(vl, event, vt->tools[vt->active_tool].state);
    else
      g_debug ( "%s: No layer selected", __FUNCTION__ ); // This shouldn't happen often
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
      (void)vt->tools[vt->active_tool].ti.release(vl, event, vt->tools[vt->active_tool].state);
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

GtkWidget *vik_window_get_graphs_widget ( VikWindow *vw )
{
  return vw->graphs;
}

gpointer vik_window_get_graphs_widgets ( VikWindow *vw )
{
  return vw->graphs_widgets;
}

void vik_window_set_graphs_widgets ( VikWindow *vw, gpointer gp )
{
  vw->graphs_widgets = gp;
  // Manual restore of previous pane position
  scale_graphs_pane ( vw );
}

gboolean vik_window_get_graphs_widgets_shown ( VikWindow *vw )
{
  return vw->show_track_graphs;
}

/**
 * Close the graphs portion of the main display
 *
 */
void vik_window_close_graphs ( VikWindow *vw )
{
  if ( vw->graphs_widgets ) {
    // Store current position of the separator so we can restore it
    vw->vpaned_pc = get_pane_position_as_percent ( vw->vpaned );

    vik_trw_layer_propwin_main_close ( vw->graphs_widgets );
    vw->graphs_widgets = NULL;

    // The 'hide' - move the handle all the way to the end
    GtkAllocation allocation;
    gtk_widget_get_allocation ( vw->vpaned, &allocation );
    gtk_paned_set_position ( GTK_PANED(vw->vpaned), allocation.height );
    vw->vpaned_shown = FALSE;
  }
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

/**
 * Remove the potentially auto added map
 */
static void remove_default_map_layer ( VikWindow *vw )
{
  if ( a_vik_get_add_default_map_layer () ) {
    VikAggregateLayer *top = vik_layers_panel_get_top_layer(vw->viking_vlp);
    if ( vik_aggregate_layer_count(top) == 1 ) {
      VikLayer *vl = vik_aggregate_layer_get_top_visible_layer_of_type (top, VIK_LAYER_MAPS);
      // Could try to compare name vs _("Default Map") but this might have i8n issues if not translated
      if ( vl )
        vik_aggregate_layer_delete_layer ( top, vl );
    }
  }
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
      open_window ( self, filenames, FALSE );
      // NB: GSList & contents are freed by open_window()
    }
    else {
      if ( check_file_magic_vik ( path ) )
        remove_default_map_layer ( self );
      vik_window_open_file ( self, path, TRUE, TRUE, TRUE, TRUE, FALSE );
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
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)menu_item, gtk_image_new_from_stock(GTK_STOCK_FILE, GTK_ICON_SIZE_MENU) );

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

void vik_window_set_busy_cursor_widget ( GtkWidget *widget, VikWindow *vw )
{
  gdk_window_set_cursor ( gtk_widget_get_window ( widget ), vw->busy_cursor );
  vik_window_set_busy_cursor ( vw );
}

void vik_window_clear_busy_cursor_widget ( GtkWidget *widget, VikWindow *vw )
{
  gdk_window_set_cursor ( gtk_widget_get_window( widget ), NULL );
  vik_window_clear_busy_cursor ( vw );
}

// ATM Call upon file load
// TODO maybe also hook into (new) signals on new/deleted viklayers (of TRW)
// THen only need for adds to mark additional entries.
// For delete may need to recreate
#if 0
void calendar_update ( VikWindow *vw )
{
  // Maybe skip if not shown...
  // But then on visibility shown, would need to generate it
  // Above method seems better.
  
  GtkCalendar *calendar = (GtkCalendar*)vik_layers_panel_get_calendar ( vw->viking_vlp );
  gtk_calendar_clear_marks ( calendar );

  GList *layers = vik_layers_panel_get_all_layers_of_type ( vw->viking_vlp, VIK_LAYER_TRW, TRUE );
  if ( !layers )
    return;
  
  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    VikTrwLayer *vtl = VIK_TRW_LAYER(layer->data);
    calendar_consider_layer ( calendar, vtl );
  }
  g_list_free ( layers );
}
#endif

/**
 * @first: Indicates the first file in a possible list of files to be loaded
 * @last:  Indicates the last file in a possible list of files to be loaded
 *        Hence a draw operation can be performed
 */
void vik_window_open_file ( VikWindow *vw, const gchar *filename, gboolean change_filename, gboolean first, gboolean last, gboolean new_layer, gboolean external )
{
  if ( first )
    vik_window_set_busy_cursor ( vw );

  // Enable the *new* filename to be accessible by the Layers codez
  gchar *original_filename = g_strdup ( vw->filename );
  g_free ( vw->filename );
  vw->filename = g_strdup ( filename );
  gboolean success = FALSE;
  gboolean restore_original_filename = FALSE;

  VikAggregateLayer *agg = vik_layers_panel_get_top_layer(vw->viking_vlp);
  // Make Append load into the selected Aggregate layer (if there is one)
  if ( !new_layer ) {
    GtkTreeIter iter;
    VikTreeview *vtv = vik_layers_panel_get_treeview ( vw->viking_vlp );
    if ( vtv ) {
      if ( vik_treeview_get_selected_iter ( vtv, &iter ) ) {
        gint type = vik_treeview_item_get_type ( vtv, &iter );
        if ( type == VIK_TREEVIEW_TYPE_LAYER ) {
          VikLayer *vl = VIK_LAYER(vik_treeview_item_get_pointer ( vtv, &iter ));
          if ( vl->type == VIK_LAYER_AGGREGATE ) {
            agg = VIK_AGGREGATE_LAYER(vl);
          }
        }
      }
    }
  }

  vw->loaded_type = a_file_load ( agg, vw->viking_vvp, vw->containing_vtl, filename, new_layer, external, NULL );
  switch ( vw->loaded_type )
  {
    case LOAD_TYPE_READ_FAILURE:
      a_dialog_error_msg ( GTK_WINDOW(vw), _("The file you requested could not be opened.") );
      g_warning ( "%s: could not open %s", __FUNCTION__, filename );
      break;
    case LOAD_TYPE_GPSBABEL_FAILURE:
      a_dialog_error_msg ( GTK_WINDOW(vw), _("GPSBabel is required to load files of this type or GPSBabel encountered problems.") );
      break;
    case LOAD_TYPE_GPX_FAILURE:
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unable to load malformed GPX file %s"), filename );
      break;
    case LOAD_TYPE_TCX_FAILURE:
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unable to load malformed TCX file %s"), filename );
      break;
    case LOAD_TYPE_KML_FAILURE:
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unable to load malformed KML file %s"), filename );
      break;
    case LOAD_TYPE_UNSUPPORTED_FAILURE:
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unsupported file type for %s"), filename );
      break;
    case LOAD_TYPE_OTHER_FAILURE_NON_FATAL:
    {
      // Since we can process certain other files with issues (e.g. zip files) just show a warning in the status bar
      // Not that a user can do much about it... or tells them what this issue is yet...
      gchar *msg = g_strdup_printf (_("WARNING: issues encountered loading %s"), a_file_basename (filename) );
      vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, msg );
      g_free ( msg );
      break;
    }
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
      break;
  }

  if ( ! success || restore_original_filename )
    // Load didn't work or want to keep as the existing Viking project, keep using the original name
    window_set_filename ( vw, original_filename );
  g_free ( original_filename );

  if ( last ) {
    vik_aggregate_layer_file_load_complete ( agg );
    // Draw even if the last load unsuccessful, as may have successful loads in a list of files
    draw_update ( vw );
    vik_layers_panel_calendar_update ( vw->viking_vlp );
  }
  // Always clear cursor (e.g. incase first & last loads are on different VikWindows)
  vik_window_clear_busy_cursor ( vw );
}

static void load_file ( GtkAction *a, VikWindow *vw )
{
  GSList *files = NULL;
  GSList *cur_file = NULL;
  gboolean append = FALSE;
  gboolean external = FALSE;
  if (!strcmp(gtk_action_get_name(a), "Open")) {
    append = FALSE;
  } 
  else if (!strcmp(gtk_action_get_name(a), "Append")) {
    append = TRUE;
  }
  else if (!strcmp(gtk_action_get_name(a), "OpenExtLayer")) {
    if ( !vu_check_confirm_external_use(GTK_WINDOW(vw)) )
      return;
    external = TRUE;
  }
  else {
    g_critical("Houston, we've had a problem.");
    return;
  }

  files = vu_get_ui_selected_gps_files ( vw, external );

  if ( files ) {
#ifdef VIKING_PROMPT_IF_MODIFIED
    if ( (vw->modified || vw->filename) && !append ) {
#else
    if ( vw->filename && !append ) {
#endif
      open_window ( vw, files, external );
      // NB: GSList & contents of 'files' are freed by open_window()
    }
    else {
      guint file_num = 0;
      guint num_files = g_slist_length(files);
      gboolean change_fn = !append && (num_files==1); // only change fn if one file
      gboolean first_vik_file = TRUE;
      cur_file = files;
      while ( cur_file ) {
        gchar *file_name = cur_file->data;
        file_num++;
        if ( !append && check_file_magic_vik ( file_name ) ) {
          // Load first of many .vik files in current window
          if ( first_vik_file ) {
            remove_default_map_layer ( vw );
            vik_window_open_file ( vw, file_name, TRUE, TRUE, TRUE, TRUE, FALSE );
            first_vik_file = FALSE;
          }
          else {
            // Load each subsequent .vik file in a separate window
            VikWindow *newvw = vik_window_new_window ();
            if (newvw)
              vik_window_open_file ( newvw, file_name, TRUE, TRUE, TRUE, TRUE, FALSE );
          }
        }
        else
          // Other file types or appending a .vik file
          vik_window_open_file ( vw, file_name, change_fn, (file_num==1), (file_num==num_files), !append, external );

        g_free (file_name);
        cur_file = g_slist_next (cur_file);
      }
      g_slist_free (files);
    }
  }
}

static gboolean window_save_file_as ( VikWindow *vw, VikAggregateLayer *agg, gboolean set_name )
{
  gboolean rv = FALSE;
  gchar *fn = NULL;
  GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Save as Viking File."),
                                                   GTK_WINDOW(vw),
                                                   GTK_FILE_CHOOSER_ACTION_SAVE,
                                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                   GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                   NULL);

  if ( vu_get_last_folder_files_uri() )
    gtk_file_chooser_set_current_folder_uri ( GTK_FILE_CHOOSER(dialog), vu_get_last_folder_files_uri() );

  GtkFileFilter *filter;
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name( filter, _("All") );
  gtk_file_filter_add_pattern ( filter, "*" );
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name( filter, _("Viking") );
  gtk_file_filter_add_pattern ( filter, "*.vik" );
  gtk_file_filter_add_pattern ( filter, "*.viking" );
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
  // Default to a Viking file
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER(dialog), filter);

  gtk_window_set_transient_for ( GTK_WINDOW(dialog), GTK_WINDOW(vw) );
  gtk_window_set_destroy_with_parent ( GTK_WINDOW(dialog), TRUE );

  // Auto append / replace extension with '.vik' to the suggested file name as it's going to be a Viking File
  gchar* auto_save_name = NULL;
  if ( set_name )
    auto_save_name = g_strdup ( window_get_filename ( vw ) );
  else
    auto_save_name = VIK_LAYER(agg)->name;

  if ( ! a_file_check_ext ( auto_save_name, ".vik" ) )
    auto_save_name = g_strconcat ( auto_save_name, ".vik", NULL );

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(dialog), auto_save_name);

  while ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog) );
    if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) == FALSE || a_dialog_yes_or_no ( GTK_WINDOW(dialog), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
    {
      if ( set_name )
        window_set_filename ( vw, fn );
      rv = window_save ( vw, agg, fn );
      if ( rv ) {
        vw->modified = FALSE;
        vu_set_last_folder_files_uri ( gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(dialog)) );
      }
      g_free ( fn );
      break;
    }
    g_free ( fn );
  }
  g_free ( auto_save_name );
  gtk_widget_destroy ( dialog );
  return rv;
}

gboolean vik_window_save_file_as ( VikWindow *vw, gpointer val )
{
  return window_save_file_as ( vw, (VikAggregateLayer*)val, FALSE );
}

static gboolean save_file_as ( GtkAction *a, VikWindow *vw )
{
  return window_save_file_as ( vw, vik_layers_panel_get_top_layer(vw->viking_vlp), TRUE );
}

 static gboolean window_save ( VikWindow *vw, VikAggregateLayer *agg, gchar *filename )
{
  vik_window_set_busy_cursor ( vw );
  gboolean success = TRUE;

  if ( a_file_save(agg, vw->viking_vvp, filename) )
  {
    update_recently_used_document ( vw, filename );
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
    return window_save ( vw, vik_layers_panel_get_top_layer(vw->viking_vlp), vw->filename );
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

static void export_to_single_gpx ( GtkAction *a, VikWindow *vw )
{
  VikAggregateLayer *top = vik_layers_panel_get_top_layer ( vw->viking_vlp );
  vik_aggregate_layer_export_gpx_setup ( top );
}

static void export_to_kml ( GtkAction *a, VikWindow *vw )
{
  export_to_common ( vw, FILE_TYPE_KML, ".kml" );
}

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
        gint byte_size = stat_buf.st_size;
        gchar *size = g_format_size_full ( byte_size, G_FORMAT_SIZE_DEFAULT );
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

#define GPSBABEL_URL "https://www.gpsbabel.org"
static void goto_gpsbabel_website ( GtkAction *a, VikWindow *vw )
{
  open_url ( GTK_WINDOW(vw), GPSBABEL_URL );
}

static void goto_default_location( GtkAction *a, VikWindow *vw)
{
  struct LatLon ll;
  ll.lat = a_vik_get_default_lat();
  ll.lon = a_vik_get_default_long();
  vik_viewport_set_center_latlon(vw->viking_vvp, &ll, TRUE);
  vik_layers_panel_emit_update(vw->viking_vlp);
}

static void goto_auto_location( GtkAction *a, VikWindow *vw)
{
  vik_window_statusbar_update ( vw, _("Trying to determine location..."), VIK_STATUSBAR_INFO );
#ifdef HAVE_LIBGEOCLUE_2
  libgeoclue_where_am_i ( vw, update_from_geoclue );
#else
  determine_location_fallback ( vw );
#endif
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
  (void)a_settings_get_boolean ( VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT, &full_format );

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

  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    // Reset the individual waypoints themselves due to the preferences change
    VikTrwLayer *vtl = VIK_TRW_LAYER(layer->data);
    vik_trw_layer_reset_waypoints ( vtl );
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

  vik_layers_panel_set_preferences ( vw->viking_vlp );
}

static void preferences_reset_cb ( GtkAction *a, VikWindow *vw )
{
  if ( a_dialog_yes_or_no ( GTK_WINDOW(vw), _("Are you sure you wish to reset all preferences back to the defaults?"), NULL ) ) {
    gchar *filename = a_preferences_reset_all_defaults();
    if ( filename ) {
      a_dialog_info_msg_extra ( GTK_WINDOW(vw), _("A backup of the previous preferences was saved to: %s"), filename );
      toolbar_apply_settings ( vw->viking_vtb, vw->main_vbox, vw->menu_hbox, TRUE );
      vik_layers_panel_set_preferences ( vw->viking_vlp );
    } else {
      a_dialog_error_msg ( GTK_WINDOW(vw), _("Preferences not reset as backup of current preferences failed") );
    }
    g_free ( filename );
  }
}

static void suppressions_reset_cb ( GtkAction *a, VikWindow *vw )
{
  GList *messages = a_settings_get_string_list ( VIK_SUPPRESS_MESSAGES );
  if ( messages ) {
    GList *ans = a_dialog_select_from_list ( GTK_WINDOW(vw),
                                             messages,
                                             FALSE,
                                             _("Suppressions List"),
                                             _("Reset all the suppressed messages?") );
    if ( ans ) {
      a_settings_clear_string_list ( VIK_SUPPRESS_MESSAGES );
      g_list_free_full ( ans, g_free );
    }
  }
  else
    a_dialog_info_msg ( GTK_WINDOW(vw), _("No messages are being suppressed") );
}

static void default_location_cb ( GtkAction *a, VikWindow *vw )
{
  // Get relevant preferences - these should always be available
  VikLayerParam *pref_lat = a_preferences_get_param ( VIKING_PREFERENCES_NAMESPACE "default_latitude" );
  VikLayerParam *pref_lon = a_preferences_get_param ( VIKING_PREFERENCES_NAMESPACE "default_longitude" );
  if ( !pref_lat || !pref_lon ) {
    g_critical ( "%s: preference not found", __FUNCTION__ );
    return;
  }

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

/**
 * Delete All
 */
static void clear_cb ( GtkAction *a, VikWindow *vw )
{
  // Do nothing if empty
  VikAggregateLayer *top = vik_layers_panel_get_top_layer(vw->viking_vlp);
  if ( ! vik_aggregate_layer_is_empty(top) ) {
    if ( a_dialog_yes_or_no ( GTK_WINDOW(vw), _("Are you sure you wish to delete all layers?"), NULL ) ) {
      vik_layers_panel_clear ( vw->viking_vlp );
      window_set_filename ( vw, NULL );
      draw_update ( vw );
    }
  }
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

static void save_image_file ( VikWindow *vw, const gchar *fn, guint w, guint h, gdouble zoom, gboolean save_as_png, gboolean save_kmz )
{
  /* more efficient way: stuff draws directly to pixbuf (fork viewport) */
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
  GdkPixbuf *pixbuf_to_save = vik_viewport_get_pixbuf ( vw->viking_vvp, w, h );
  if ( !pixbuf_to_save ) {
    g_warning("Failed to generate internal pixmap size: %d x %d", w, h);
    gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG(msgbox), _("Failed to generate internal image.\n\nTry creating a smaller image.") );
    goto cleanup;
  }

  int ans = 0; // Default to success

  if ( save_kmz ) {
    gdouble north, east, south, west;
    vik_viewport_get_min_max_lat_lon ( vw->viking_vvp, &south, &north, &west, &east );
    ans = kmz_save_file ( pixbuf_to_save, fn, north, east, south, west );
  }
  else {
    gdk_pixbuf_save ( pixbuf_to_save, fn, save_as_png ? "png" : "jpeg", &error, NULL );
    if (error) {
      g_warning("Unable to write to file %s: %s", fn, error->message );
      g_error_free (error);
      ans = 42;
    }
  }

  if ( ans == 0 )
    gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG(msgbox), _("Image file generated.") );
  else
    gtk_message_dialog_set_markup ( GTK_MESSAGE_DIALOG(msgbox), _("Failed to generate image file.") );

  g_object_unref ( G_OBJECT(pixbuf_to_save) );

 cleanup:
  vik_statusbar_set_message ( vw->viking_vs, VIK_STATUSBAR_INFO, "" );
  gtk_dialog_add_button ( GTK_DIALOG(msgbox), GTK_STOCK_OK, GTK_RESPONSE_OK );
  gtk_dialog_run ( GTK_DIALOG(msgbox) ); // Don't care about the result

  /* pretend like nothing happened ;) */
  vik_viewport_set_xmpp ( vw->viking_vvp, old_xmpp );
  vik_viewport_set_ympp ( vw->viking_vvp, old_ympp );
  (void)vik_viewport_configure ( vw->viking_vvp );
  draw_update ( vw );
}

static void save_image_dir ( VikWindow *vw, const gchar *fn, guint w, guint h, gdouble zoom, gboolean save_as_png, guint tiles_w, guint tiles_h )
{
  gulong size = sizeof(gchar) * (strlen(fn) + 15);
  gchar *name_of_file = g_malloc ( size );
  guint x = 1, y = 1;
  struct UTM utm_orig, utm;

  /* *** copied from above *** */
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

  if ( g_mkdir(fn,0777) != 0 )
    g_warning ( "%s: Failed to create directory %s", __FUNCTION__, fn );

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
      GdkPixbuf *pixbuf_to_save = vik_viewport_get_pixbuf ( vw->viking_vvp, w, h );
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
  (void)vik_viewport_configure ( vw->viking_vvp );
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

  // NB UTM mode should be exact MPP & a 'factor' of 1.0
  VikViewport *vvp = VIK_WINDOW(pass_along[0])->viking_vvp;
  const VikCoord *vc = vik_viewport_get_center ( vvp );
  if ( vc->mode == VIK_COORD_LATLON ) {
    // Convert from actual image MPP to Viking 'pixelfact'
    // NB the 1.193 - is at the Equator.
    // http://wiki.openstreetmap.org/wiki/Zoom_levels
    struct LatLon ll;
    vik_coord_to_latlon ( vc, &ll );
    gdouble factor = cos(DEG2RAD(ll.lat)) * 1.193;
    h = h * factor;
    w = w * factor;
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

typedef enum {
  VW_GEN_SINGLE_IMAGE,
  VW_GEN_DIRECTORY_OF_IMAGES,
  VW_GEN_KMZ_FILE,
} img_generation_t;

#define VIK_SETTINGS_KMZ_DEFAULT_MAPS_DIR "kmz_default_maps_dir"

/*
 * Get an allocated filename (or directory as specified)
 */
static gchar* draw_image_filename ( VikWindow *vw, img_generation_t img_gen, gdouble zoom )
{
  gchar *fn = NULL;
  if ( img_gen != VW_GEN_DIRECTORY_OF_IMAGES )
  {
    // Single file
    GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Save Image"),
                                                     GTK_WINDOW(vw),
                                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                     NULL);
    if ( last_folder_images_uri )
      gtk_file_chooser_set_current_folder_uri ( GTK_FILE_CHOOSER(dialog), last_folder_images_uri );

    GtkFileChooser *chooser = GTK_FILE_CHOOSER ( dialog );
    /* Add filters */
    GtkFileFilter *filter;
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, _("All") );
    gtk_file_filter_add_pattern ( filter, "*" );
    gtk_file_chooser_add_filter ( chooser, filter );

    filter = gtk_file_filter_new ();
    gchar *extension = NULL;

    if ( img_gen == VW_GEN_KMZ_FILE ) {
      gtk_file_filter_set_name ( filter, _("KMZ") );
      gtk_file_filter_add_mime_type ( filter, "vnd.google-earth.kmz");
      gtk_file_filter_add_pattern ( filter, "*.kmz" );
      gtk_file_chooser_add_filter ( chooser, filter );
      extension = "kmz";
    }
    else {
      if ( vw->draw_image_save_as_png ) {
        gtk_file_filter_set_name ( filter, _("PNG") );
        gtk_file_filter_add_mime_type ( filter, "image/png");
        gtk_file_chooser_add_filter ( chooser, filter );
        extension = "png";
      } else {
        gtk_file_filter_set_name ( filter, _("JPG") );
        gtk_file_filter_add_mime_type ( filter, "image/jpeg");
        gtk_file_chooser_add_filter ( chooser, filter );
        extension = "jpg";
      }
    }
    gtk_file_chooser_set_filter ( chooser, filter );

    // Autogenerate a name
    // Read from settings for the directory
    gchar *save_dir = NULL;
    (void)a_settings_get_string ( VIK_SETTINGS_KMZ_DEFAULT_MAPS_DIR, &save_dir );
    if ( save_dir )
       (void)gtk_file_chooser_set_current_folder ( chooser, save_dir );

    const VikCoord *vc = vik_viewport_get_center ( vw->viking_vvp );
    if ( vc->mode == VIK_COORD_LATLON ) {
      gchar *fname = g_strdup_printf ( "%.3f_%.3f_%d.%s", vc->north_south, vc->east_west, (gint)zoom, extension );
      gtk_file_chooser_set_current_name ( chooser, fname );
      g_free ( fname );
    }

    gtk_window_set_transient_for ( GTK_WINDOW(dialog), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(dialog), TRUE );

    if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {
      g_free ( last_folder_images_uri );
      last_folder_images_uri = gtk_file_chooser_get_current_folder_uri ( GTK_FILE_CHOOSER(dialog) );

      fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog) );
      if ( g_file_test ( fn, G_FILE_TEST_EXISTS ) )
        if ( ! a_dialog_yes_or_no ( GTK_WINDOW(dialog), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
          fn = NULL;
    }
    gtk_widget_destroy ( dialog );
  }
  else {
    // A directory
    // For some reason this method is only written to work in UTM...
    if ( vik_viewport_get_coord_mode(vw->viking_vvp) != VIK_COORD_UTM ) {
      a_dialog_error_msg ( GTK_WINDOW(vw), _("You must be in UTM mode to use this feature") );
      return fn;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Choose a directory to hold images"),
                                                     GTK_WINDOW(vw),
                                                     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                     NULL);
    gtk_window_set_transient_for ( GTK_WINDOW(dialog), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(dialog), TRUE );

    if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {
      fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog) );
    }
    gtk_widget_destroy ( dialog );
  }
  return fn;
}

static void draw_to_image_file ( VikWindow *vw, img_generation_t img_gen )
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
  GtkWidget *current_window_button;
  gpointer current_window_pass_along[7];
  GtkWidget *zoom_label, *zoom_combo;
  GtkWidget *total_size_label;

  // only used for VW_GEN_DIRECTORY_OF_IMAGES
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
  current_window_pass_along [4] = NULL; // Only for directory of tiles: width
  current_window_pass_along [5] = NULL; // Only for directory of tiles: height
  current_window_pass_along [6] = total_size_label;
  g_signal_connect ( G_OBJECT(current_window_button), "button_press_event", G_CALLBACK(draw_to_image_file_current_window_cb), current_window_pass_along );

  GtkWidget *png_radio = gtk_radio_button_new_with_label ( NULL, _("Save as PNG") );
  GtkWidget *jpeg_radio = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(png_radio), _("Save as JPEG") );

  if ( img_gen == VW_GEN_KMZ_FILE ) {
    // Don't show image type selection if creating a KMZ (always JPG internally)
    // Start with viewable area by default
    draw_to_image_file_current_window_cb ( current_window_button, NULL, current_window_pass_along );
  } else {
    gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), jpeg_radio, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), png_radio, FALSE, FALSE, 0);
  }

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

  if ( img_gen == VW_GEN_DIRECTORY_OF_IMAGES )
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

    gint active_z = gtk_combo_box_get_active ( GTK_COMBO_BOX(zoom_combo) );
    gdouble zoom = pow (2, active_z-2 );

    vw->draw_image_save_as_png = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(png_radio) );

    gchar *fn = draw_image_filename ( vw, img_gen, zoom );
    if ( !fn )
      return;

    if ( img_gen == VW_GEN_SINGLE_IMAGE ) {
      save_image_file ( vw, fn, 
                      vw->draw_image_width = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(width_spin) ),
                      vw->draw_image_height = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(height_spin) ),
                      zoom,
                      vw->draw_image_save_as_png,
                      FALSE );
     } else if ( img_gen == VW_GEN_KMZ_FILE ) {
      // Remove some viewport overlays as these aren't useful in KMZ file.
      gboolean restore_xhair = vik_viewport_get_draw_centermark ( vw->viking_vvp );
      if ( restore_xhair )
        vik_viewport_set_draw_centermark ( vw->viking_vvp, FALSE );
      gboolean restore_scale = vik_viewport_get_draw_scale ( vw->viking_vvp );
      if ( restore_scale )
        vik_viewport_set_draw_scale ( vw->viking_vvp, FALSE );

      save_image_file ( vw,
                        fn,
                        gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(width_spin) ),
                        gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(height_spin) ),
                        zoom,
                        FALSE, // JPG
                        TRUE );

      if ( restore_xhair )
        vik_viewport_set_draw_centermark ( vw->viking_vvp, TRUE );
      if ( restore_scale )
        vik_viewport_set_draw_scale ( vw->viking_vvp, TRUE );
      if ( restore_xhair || restore_scale )
        draw_update ( vw );
    }
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
  draw_to_image_file ( vw, VW_GEN_SINGLE_IMAGE );
}

static void draw_to_image_dir_cb ( GtkAction *a, VikWindow *vw )
{
  draw_to_image_file ( vw, VW_GEN_DIRECTORY_OF_IMAGES );
}

#ifdef HAVE_ZIP_H
static void draw_to_kmz_file_cb ( GtkAction *a, VikWindow *vw )
{
  if ( vik_viewport_get_coord_mode(vw->viking_vvp) == VIK_COORD_UTM ) {
    a_dialog_error_msg ( GTK_WINDOW(vw), _("This feature is not available in UTM mode") );
    return;
  }
  // NB ATM This only generates a KMZ file with the current viewport image - intended mostly for map images [but will include any lines/icons from track & waypoints that are drawn]
  // (it does *not* include a full KML dump of every track, waypoint etc...)
  draw_to_image_file ( vw, VW_GEN_KMZ_FILE );
}

/**
 *
 */
static void import_kmz_file_cb ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Open File"),
                                                   GTK_WINDOW(vw),
                                                   GTK_FILE_CHOOSER_ACTION_OPEN,
                                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                   NULL);

  GtkFileFilter *filter;
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name ( filter, _("KMZ") );
  gtk_file_filter_add_mime_type ( filter, "vnd.google-earth.kmz");
  gtk_file_filter_add_pattern ( filter, "*.kmz" );
  gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER(dialog), filter );
  // Default filter to KMZ type
  gtk_file_chooser_set_filter ( GTK_FILE_CHOOSER(dialog), filter );

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name( filter, _("All") );
  gtk_file_filter_add_pattern ( filter, "*" );
  gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER(dialog), filter );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )  {
    gchar *fn = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(dialog) );
    // TODO convert ans value into readable explaination of failure...
    int ans = kmz_open_file ( fn, vw->viking_vvp, vw->viking_vlp );
    if ( ans )
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unable to import %s."), fn );
    g_free ( fn );
    draw_update ( vw );
  }
  gtk_widget_destroy ( dialog );
}
#endif

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
  GdkColor color = vik_viewport_get_background_gdkcolor ( vw->viking_vvp );
  gtk_color_selection_set_previous_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), &color );
  gtk_color_selection_set_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), &color );
  if ( gtk_dialog_run ( GTK_DIALOG(colorsd) ) == GTK_RESPONSE_OK )
  {
    gtk_color_selection_get_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), &color );
    vik_viewport_set_background_gdkcolor ( vw->viking_vvp, color );
    draw_update ( vw );
  }
  gtk_widget_destroy ( colorsd );
}

static void set_highlight_color ( GtkAction *a, VikWindow *vw )
{
  GtkWidget *colorsd = gtk_color_selection_dialog_new ( _("Choose a track highlight color") );
  GdkColor color = vik_viewport_get_highlight_gdkcolor ( vw->viking_vvp );
  gtk_color_selection_set_previous_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), &color );
  gtk_color_selection_set_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), &color );
  if ( gtk_dialog_run ( GTK_DIALOG(colorsd) ) == GTK_RESPONSE_OK )
  {
    gtk_color_selection_get_current_color ( GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), &color );
    vik_viewport_set_highlight_gdkcolor ( vw->viking_vvp, color );
    draw_update ( vw );
  }
  gtk_widget_destroy ( colorsd );
}

/**
 * a_vik_window_get_a_window:
 *
 * Returns a #VikWindow for cases when the calling code otherwise has no access visibility of what #VikWindow called it.
 *  This should only be used in special cases e.g. inside a callback that no data can be passed into it.
 *  Ideally this should also be only used for new dialogs like messages, so the actual #VikWindow is less relevant
 *  (rather than trying to get a viewport or similar as it won't necessarily be the right one when there is one than one Window).
 */
VikWindow* a_vik_window_get_a_window ( void )
{
  GSList *item = g_slist_last (window_list);
  if ( item )
    return (VikWindow*)item->data;
  return NULL;
}

/***********************************************************************************************
 ** GUI Creation
 ***********************************************************************************************/

// NB Still having to use GtkIconFactory to map icons from GResource for use with GtkUIManager
static void
a_register_icon ( GtkIconFactory *icon_factory, const gchar *name )
{
  if ( !name ) return;
  GtkIconSet *icon_set = gtk_icon_set_new_from_pixbuf ( ui_get_icon ( name, 32 ));
  gtk_icon_factory_add ( icon_factory, name, icon_set );
  gtk_icon_set_unref ( icon_set );
}

// NB Also see GNOME/GTK Standard keys
// https://developer.gnome.org/hig/stable/keyboard-input.html.en
// So for example that's why F8 & F10 aren't used.
//  (some of these are handled externally, thus Viking doesn't get notified anyway)
// Unfortunately F6 was used (for the 'Show Center Mark') starting back in 2010,
//  so this is left as is in case anyone is used to it.

static GtkActionEntry entries[] = {
  { "File", NULL, N_("_File"), 0, 0, 0 },
  { "Edit", NULL, N_("_Edit"), 0, 0, 0 },
  { "View", NULL, N_("_View"), 0, 0, 0 },
  { "SetShow", VIK_ICON_CHECKBOX, N_("_Show"), 0, 0, 0 },
  { "SetZoom", "zoom_18", N_("_Zoom"), 0, 0, 0 },
  { "SetPan", "mover_22", N_("_Pan"), 0, 0, 0 },
  { "Layers", NULL, N_("_Layers"), 0, 0, 0 },
  { "Tools", NULL, N_("_Tools"), 0, 0, 0 },
  { "Exttools", GTK_STOCK_NETWORK, N_("_Webtools"), 0, 0, 0 },
  { "Help", NULL, N_("_Help"), 0, 0, 0 },

  { "New",       GTK_STOCK_NEW,          N_("_New"),                          "<control>N", N_("New file"),                                     (GCallback)newwindow_cb          },
  { "Open",      GTK_STOCK_OPEN,         N_("_Open..."),                         "<control>O", N_("Open a file"),                                  (GCallback)load_file             },
  { "OpenRecentFile", NULL,              N_("Open _Recent File"),         NULL,         NULL,                                               (GCallback)NULL },
  { "Append",    GTK_STOCK_ADD,          N_("Append _File..."),           NULL,         N_("Append data from a different file"),            (GCallback)load_file             },
  { "OpenExtLayer", VIK_ICON_ATTACH,     N_("Open GPX as External _Layer..."),    NULL,         N_("Open a GPX file as an external layer"), (GCallback)load_file },
  { "Export",    GTK_STOCK_CONVERT,      N_("_Export All"),               NULL,         N_("Export All TrackWaypoint Layers"),              (GCallback)NULL                  },
  { "ExportGPX", NULL,                   N_("_GPX..."),           	      NULL,         N_("Export as GPX"),                                (GCallback)export_to_gpx         },
  { "ExportSingleGPX", NULL,             N_("_Single GPX File..."),       NULL,         N_("Export to Single GPX File"),                    (GCallback)export_to_single_gpx  },
  { "Acquire",   GTK_STOCK_GO_DOWN,      N_("A_cquire"),                  NULL,         NULL,                                               (GCallback)NULL },
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
  { "SaveAs",    GTK_STOCK_SAVE_AS,      N_("Save _As..."),                   "<control><shift>S",  N_("Save the file under different name"),           (GCallback)save_file_as          },
  { "FileProperties", GTK_STOCK_PROPERTIES, N_("Properties..."),                 NULL,  N_("File Properties"),                              (GCallback)file_properties_cb },
#ifdef HAVE_ZIP_H
  { "ImportKMZ", GTK_STOCK_CONVERT,      N_("Import KMZ _Map File..."),        NULL,  N_("Import a KMZ file"), (GCallback)import_kmz_file_cb },
  { "GenKMZ",    GTK_STOCK_DND,          N_("Generate _KMZ Map File..."),        NULL,  N_("Generate a KMZ file with an overlay of the current view"), (GCallback)draw_to_kmz_file_cb },
#endif
  { "GenImg",    GTK_STOCK_FILE,         N_("_Generate Image File..."),          NULL,  N_("Save a snapshot of the workspace into a file"), (GCallback)draw_to_image_file_cb },
  { "GenImgDir", GTK_STOCK_DND_MULTIPLE, N_("Generate _Directory of Images..."), NULL,  N_("Generate _Directory of Images"),                (GCallback)draw_to_image_dir_cb },
  { "Print",     GTK_STOCK_PRINT,        N_("_Print..."),                     "<control>P", N_("Print maps"), (GCallback)print_cb },
  { "Exit",      GTK_STOCK_QUIT,         N_("E_xit"),                         "<control>W", N_("Exit the program"),                             (GCallback)window_close          },
  { "SaveExit",  GTK_STOCK_QUIT,         N_("Save and Exit"),                 NULL, N_("Save and Exit the program"),                             (GCallback)save_file_and_exit          },

  { "GoBack",    GTK_STOCK_GO_BACK,      N_("Go to the Pre_vious Location"),  NULL,         N_("Go to the previous location"),              (GCallback)draw_goto_back_and_forth },
  { "GoForward", GTK_STOCK_GO_FORWARD,   N_("Go to the _Next Location"),      NULL,         N_("Go to the next location"),                  (GCallback)draw_goto_back_and_forth },
  { "GotoDefaultLocation", GTK_STOCK_HOME, N_("Go to the _Default Location"),  NULL,         N_("Go to the default location"),                     (GCallback)goto_default_location },
  { "GotoAutoLocation", GTK_STOCK_REFRESH, N_("Go to the _Auto Location"), "<control>A",     N_("Go to a location via automatic lookup"),   (GCallback)goto_auto_location },
  { "GotoSearch", GTK_STOCK_JUMP_TO,     N_("Go to _Location..."),     "<control>F",         N_("Go to address/place using text search"),        (GCallback)goto_address       },
  { "GotoLL",    GTK_STOCK_JUMP_TO,      N_("_Go to Lat/Lon..."),           NULL,         N_("Go to arbitrary lat/lon coordinate"),         (GCallback)draw_goto_cb          },
  { "GotoUTM",   GTK_STOCK_JUMP_TO,      N_("Go to UTM..."),                  NULL,         N_("Go to arbitrary UTM coordinate"),               (GCallback)draw_goto_cb          },
  { "GotoToday", GTK_STOCK_JUMP_TO,      N_("Go to Today"),               "<control>T",     N_("Go to today on the calendar"),              (GCallback)calendar_goto_today },
  { "Refresh",   GTK_STOCK_REFRESH,      N_("_Refresh"),                      "F5",         N_("Refresh any maps displayed"),               (GCallback)draw_refresh_cb       },
  { "SetHLColor",GTK_STOCK_SELECT_COLOR, N_("Set _Highlight Color..."),       NULL,         N_("Set Highlight Color"),                      (GCallback)set_highlight_color },
  { "SetBGColor",GTK_STOCK_SELECT_COLOR, N_("Set Bac_kground Color..."),      NULL,         N_("Set Background Color"),                     (GCallback)set_bg_color },
  { "ZoomIn",    GTK_STOCK_ZOOM_IN,      N_("Zoom _In"),                  "<control>plus",  N_("Zoom In"),                                  (GCallback)draw_zoom_cb },
  { "ZoomOut",   GTK_STOCK_ZOOM_OUT,     N_("Zoom _Out"),                 "<control>minus", N_("Zoom Out"),                                 (GCallback)draw_zoom_cb },
  { "ZoomTo",    GTK_STOCK_ZOOM_FIT,     N_("Zoom _To..."),               "<control>Z",     N_("Zoom To"),                                  (GCallback)zoom_to_cb },
  { "PanNorth",  GTK_STOCK_GO_UP,        N_("Pan _North"),                "<control>Up",    NULL,                                           (GCallback)draw_pan_cb },
  { "PanEast",   GTK_STOCK_GO_BACK,      N_("Pan _East"),                 "<control>Right", NULL,                                           (GCallback)draw_pan_cb },
  { "PanSouth",  GTK_STOCK_GO_DOWN,      N_("Pan _South"),                "<control>Down",  NULL,                                           (GCallback)draw_pan_cb },
  { "PanWest",   GTK_STOCK_GO_FORWARD,   N_("Pan _West"),                 "<control>Left",  NULL,                                           (GCallback)draw_pan_cb },
  { "BGJobs",    GTK_STOCK_EXECUTE,      N_("Background _Jobs"),              NULL,         N_("Background Jobs"),                          (GCallback)a_background_show_window },
  { "Log",       GTK_STOCK_INFO,         N_("Log"),                           NULL,         N_("Logged messages"),                          (GCallback)a_logging_show_window },

  { "Cut",       GTK_STOCK_CUT,          N_("Cu_t"),                          NULL,         N_("Cut selected layer"),                       (GCallback)menu_cut_layer_cb     },
  { "Copy",      GTK_STOCK_COPY,         N_("_Copy"),                         NULL,         N_("Copy selected layer"),                      (GCallback)menu_copy_layer_cb    },
  { "Paste",     GTK_STOCK_PASTE,        N_("_Paste"),                        NULL,         N_("Paste layer into selected container layer or otherwise above selected layer"), (GCallback)menu_paste_layer_cb },
  { "Delete",    GTK_STOCK_DELETE,       N_("_Delete"),                   "<control>Delete",N_("Remove selected layer"),                    (GCallback)menu_delete_layer_cb  },
  { "DeleteAll", GTK_STOCK_REMOVE,       N_("Delete All"),                    NULL,         NULL,                                           (GCallback)clear_cb              },
  { "CopyCentre", GTK_STOCK_COPY,        N_("Copy Centre _Location"),     "<control>h",     NULL,                                           (GCallback)menu_copy_centre_cb   },
  { "MapCacheFlush", GTK_STOCK_DISCARD,  N_("_Flush Map Cache"),              NULL,         NULL,                                           (GCallback)mapcache_flush_cb     },
  { "SetDefaultLocation", GTK_STOCK_GO_FORWARD, N_("_Set the Default Location"), NULL, N_("Set the Default Location to the current position"),(GCallback)default_location_cb },
  { "Preferences",GTK_STOCK_PREFERENCES, N_("_Preferences"),                  NULL,         N_("Program Preferences"),                      (GCallback)preferences_cb },
  { "PreferencesReset",GTK_STOCK_REFRESH, N_("Preferences Reset All"),        NULL,         N_("Reset All Program Preferences"),            (GCallback)preferences_reset_cb },
  { "SuppressionsReset",GTK_STOCK_REFRESH,N_("_Suppression Messages..."),     NULL,         N_("Reset List of Messages being Suppressed"),  (GCallback)suppressions_reset_cb },
  { "LayerDefaults",GTK_STOCK_PROPERTIES, N_("_Layer Defaults"),             NULL,         NULL,                                           NULL },
  { "Properties",GTK_STOCK_PROPERTIES,   N_("_Properties"),                   NULL,         N_("Layer Properties"),                         (GCallback)menu_properties_cb },

  { "HelpEntry", GTK_STOCK_HELP,         N_("_Help"),                         "F1",         N_("Help"),                                     (GCallback)help_help_cb },
  { "About",     GTK_STOCK_ABOUT,        N_("_About"),                        NULL,         N_("About"),                                    (GCallback)help_about_cb },
};

static GtkActionEntry debug_entries[] = {
  { "MapCacheInfo", NULL,                "_Map Cache Info",                   NULL,         NULL,                                           (GCallback)help_cache_info_cb    },
  { "BackForwardInfo", NULL,             "_Back/Forward Info",                NULL,         NULL,                                           (GCallback)back_forward_info_cb  },
  { "BuildInfo", NULL,                   "_Build Info",                       NULL,         NULL,                                           (GCallback)build_info_cb  },
};

static GtkActionEntry entries_gpsbabel[] = {
  { "ExportKML", NULL,                   N_("_KML..."),           	      NULL,         N_("Export as KML"),                                (GCallback)export_to_kml },
  { "AcquireGPS",   NULL,                N_("From _GPS..."),           	  NULL,         N_("Transfer data from a GPS device"),              (GCallback)acquire_from_gps      },
  { "AcquireGPSBabel", NULL,             N_("Import File With GPS_Babel..."), NULL,     N_("Import file via GPSBabel converter"),           (GCallback)acquire_from_file },
};

static GtkActionEntry entries_nogpsbabel[] = {
  { "GPSBabelURL", GTK_STOCK_MISSING_IMAGE, N_("Missing GPSBabel program recommended..."), NULL, GPSBABEL_URL,                              (GCallback)goto_gpsbabel_website },
};

static GtkActionEntry entries_geojson[] = {
  { "AcquireGeoJSON",   NULL,            N_("Import Geo_JSON File..."),   NULL,         N_("Import GeoJSON file"),                          (GCallback)acquire_from_geojson },
};

static GtkToggleActionEntry toggle_debug[] = {
  { "HelpDebug", NULL, N_("Debug Mode"), NULL, N_("Toggle debug mode"), (GCallback)toggle_debug_cb, FALSE },
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
  { "ShowCenterMark", GTK_STOCK_ADD,        N_("Show _Center Mark"),         "F6",         N_("Show Center Mark"),                        (GCallback)toggle_draw_centermark, TRUE },
  { "ShowHighlight",  GTK_STOCK_UNDERLINE,  N_("Show _Highlight"),           "F7",         N_("Show Highlight"),                          (GCallback)toggle_draw_highlight, TRUE },
  { "FullScreen",     GTK_STOCK_FULLSCREEN, N_("_Full Screen"),              "F11",        N_("Activate full screen mode"),               (GCallback)full_screen_cb, FALSE },
  { "ViewSidePanel",  GTK_STOCK_INDEX,      N_("Show Side _Panel"),          "F9",         N_("Show Side Panel"),                         (GCallback)view_side_panel_cb, TRUE },
  { "ViewTrackGraphs",NULL,                 N_("Show Track _Graphs"),        "<shift>F12", N_("Show Track Graphs"),                       (GCallback)view_track_graphs_cb, TRUE },
  { "ViewStatusBar",  NULL,                 N_("Show Status_bar"),           "F12",        N_("Show Statusbar"),                          (GCallback)view_statusbar_cb, TRUE },
  { "ViewToolbar",    NULL,                 N_("Show _Toolbar"),             "F3",         N_("Show Toolbar"),                            (GCallback)view_toolbar_cb, TRUE },
  { "ViewMainMenu",   NULL,                 N_("Show _Menu"),                "F4",         N_("Show Menu"),                               (GCallback)view_main_menu_cb, TRUE },
  { "ViewSidePanelButtons",    NULL,        N_("Show Side Panel B_uttons"),  "<shift>F9",  N_("Show Side Panel Buttons"),                 (GCallback)view_side_panel_buttons_cb, TRUE },
  { "ViewSidePanelCalendar",   NULL,        N_("Show Side Panel Ca_lendar"), "<shift>F8",  N_("Show Side Panel Calendar"),                (GCallback)view_side_panel_calendar_cb, TRUE },
  { "ViewSidePanelTabs",       NULL,        N_("Show Side Panel Tabs"),      "<shift>F10",  N_("Show Side Panel Tabs"),                    (GCallback)view_side_panel_tabs_cb, TRUE },
  { "ViewSidePanelGoto",       NULL,        N_("Show Side Panel Goto"),      "<shift>F7",  N_("Show Side Panel Goto"),                    (GCallback)view_side_panel_goto_cb, TRUE },
  { "ViewSidePanelStats",      NULL,        N_("Show Side Panel Statistics"),"<shift>F4",  N_("Show Side Panel Statistics"),              (GCallback)view_side_panel_stats_cb, TRUE },
  { "ViewSidePanelSplits",     NULL,        N_("Show Side Panel Splits"),    "<shift>F3",  N_("Show Side Panel Splits"),                  (GCallback)view_side_panel_splits_cb, TRUE },
};

// This must match the toggle entries order above
static gpointer toggle_entries_toolbar_cb[] = {
  (GCallback)tb_set_draw_scale,
  (GCallback)tb_set_draw_centermark,
  (GCallback)tb_set_draw_highlight,
  (GCallback)tb_full_screen_cb,
  (GCallback)tb_view_side_panel_cb,
  (GCallback)tb_view_track_graphs_cb,
  (GCallback)tb_view_statusbar_cb,
  (GCallback)tb_view_toolbar_cb,
  (GCallback)tb_view_main_menu_cb,
  (GCallback)tb_view_side_panel_buttons_cb,
  (GCallback)tb_view_side_panel_calendar_cb,
  (GCallback)tb_view_side_panel_tabs_cb,
  (GCallback)tb_view_side_panel_goto_cb,
  (GCallback)tb_view_side_panel_stats_cb,
  (GCallback)tb_view_side_panel_splits_cb,
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
           "<menuitem action='BuildInfo'/>"
         "</menu></menubar></ui>",
         -1, NULL ) ) {
      gtk_action_group_add_actions (action_group, debug_entries, G_N_ELEMENTS (debug_entries), window);
    }
  }
  if ( gtk_ui_manager_add_ui_from_string ( uim,
       "<ui><menubar name='MainMenu'><menu action='Help'>"
       "<menuitem action='HelpDebug'/>"
       "</menu></menubar></ui>",
       -1, NULL ) ) {
    toggle_debug[0].is_active = vik_debug;
    gtk_action_group_add_toggle_actions ( action_group, toggle_debug, G_N_ELEMENTS(toggle_debug), window );
  }

  for ( i=0; i < G_N_ELEMENTS (entries); i++ ) {
    if ( entries[i].callback )
      toolbar_action_entry_register ( window->viking_vtb, &entries[i] );
  }

  if ( G_N_ELEMENTS (toggle_entries) !=  G_N_ELEMENTS (toggle_entries_toolbar_cb) ) {
    g_critical ( "%s: Broken entries definitions", __FUNCTION__ );
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
         "<ui>" \
         "<menubar name='MainMenu'>" \
         "<menu action='File'><menu action='Export'><menuitem action='ExportKML'/></menu></menu>" \
         "<menu action='File'><menu action='Acquire'><menuitem action='AcquireGPS'/></menu></menu>" \
         "<menu action='File'><menu action='Acquire'><menuitem action='AcquireGPSBabel'/></menu></menu>" \
         "</menubar>" \
         "</ui>",
         -1, &error ) )
      gtk_action_group_add_actions ( action_group, entries_gpsbabel, G_N_ELEMENTS (entries_gpsbabel), window );
  } else {
    // Stick in a link to GPSBabel website
    if ( gtk_ui_manager_add_ui_from_string ( uim,
         "<ui><menubar name='MainMenu'><menu action='Help'><separator/><menuitem action='GPSBabelURL'/></menu></menubar></ui>",
         -1, &error ) )
      gtk_action_group_add_actions ( action_group, entries_nogpsbabel, G_N_ELEMENTS (entries_nogpsbabel), window );
  }

  // GeoJSON import capability
  gchar *gjp = g_find_program_in_path ( a_geojson_program_import() );
  if ( gjp ) {
    if ( gtk_ui_manager_add_ui_from_string ( uim,
         "<ui><menubar name='MainMenu'><menu action='File'><menu action='Acquire'><menuitem action='AcquireGeoJSON'/></menu></menu></menubar></ui>",
         -1, &error ) )
      gtk_action_group_add_actions ( action_group, entries_geojson, G_N_ELEMENTS (entries_geojson), window );
    g_free ( gjp );
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
      a_register_icon ( icon_factory, window->vt->tools[i].ti.icon );
  }

  for (i=0; i<VIK_LAYER_NUM_TYPES; i++) {
    GtkActionEntry action;
    gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Layers/", 
			  vik_layer_get_interface(i)->fixed_layer_name,
			  vik_layer_get_interface(i)->fixed_layer_name,
			  GTK_UI_MANAGER_MENUITEM, FALSE);

    action.name = vik_layer_get_interface(i)->fixed_layer_name;
    action.stock_id = vik_layer_get_interface(i)->icon;
    action.label = g_strdup_printf( _("New _%s Layer"), _(vik_layer_get_interface(i)->name));
    action.accelerator = vik_layer_get_interface(i)->accelerator;
    action.tooltip = NULL;
    action.callback = (GCallback)menu_addlayer_cb;
    gtk_action_group_add_actions(action_group, &action, 1, window);

    g_free ( (gchar*)action.label );

    if ( vik_layer_get_interface(i)->tools_count ) {
      gtk_ui_manager_add_ui ( uim, mid,  "/ui/MainMenu/Tools/",
                              vik_layer_get_interface(i)->fixed_layer_name,
                              vik_layer_get_interface(i)->fixed_layer_name,
                              GTK_UI_MANAGER_SEPARATOR, FALSE );
    }

    // Further tool copying for to apply to the UI, also apply menu UI setup
    for ( j = 0; j < vik_layer_get_interface(i)->tools_count; j++ ) {
      tools = g_renew(GtkRadioActionEntry, tools, ntools+1);
      radio = &tools[ntools];
      ntools++;

      a_register_icon ( icon_factory, vik_layer_get_interface(i)->tools[j].icon );

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
			  vik_layer_get_interface(i)->fixed_layer_name,
			  layername,
			  GTK_UI_MANAGER_MENUITEM, FALSE);
    g_free (layername);

    // For default layers use action names of the form 'Layer<LayerName>'
    // This is to avoid clashing with just the layer name used above for the tool actions
    action_dl.name = g_strconcat("Layer", vik_layer_get_interface(i)->fixed_layer_name, NULL);
    action_dl.stock_id = vik_layer_get_interface(i)->fixed_layer_name;
    action_dl.label = g_strconcat(_(vik_layer_get_interface(i)->name), _("..."), NULL);
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

  if ( vik_routing_number_of_engines(VIK_ROUTING_METHOD_DIRECTIONS) == 0 ) {
    GtkWidget *widget = gtk_ui_manager_get_widget ( uim, "/ui/MainMenu/File/Acquire/AcquireRouting" );
    if ( widget ) {
      g_object_set ( widget, "sensitive", FALSE, NULL );
    }
    g_debug ( "No direction routing engines available" );
  }
}

static void
register_vik_icons ( GtkIconFactory *icon_factory )
{
  a_register_icon ( icon_factory, VIK_ICON_ATTACH );
  a_register_icon ( icon_factory, VIK_ICON_CHECKBOX );
  a_register_icon ( icon_factory, VIK_ICON_FILTER );
  a_register_icon ( icon_factory, VIK_ICON_GLOBE );
  a_register_icon ( icon_factory, VIK_ICON_SHOW_PICTURE );
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
  VikTrack *one = vik_trw_layer_get_only_track ( vtl );
  if ( one )
    vik_layers_panel_track_add ( vw->viking_vlp, one, vtl );
  else
    vik_layers_panel_track_remove ( vw->viking_vlp );
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
  vik_layers_panel_track_remove ( vw->viking_vlp );
  // Set highlight thickness
  vik_viewport_set_highlight_thickness ( vw->viking_vvp, vik_trw_layer_get_property_tracks_line_thickness (vw->containing_vtl) );
}

VikTrack* vik_window_get_selected_track ( VikWindow *vw )
{
  return vw->selected_track;
}

void vik_window_set_selected_track ( VikWindow *vw, VikTrack *vt, gpointer vtl )
{
  vw->selected_track = vt;
  if ( vt )
    vik_layers_panel_track_add ( vw->viking_vlp, vt, vtl );
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
  vik_layers_panel_track_remove ( vw->viking_vlp );
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
  vik_layers_panel_track_remove ( vw->viking_vlp );
}

gboolean vik_window_clear_selected ( VikWindow *vw )
{
  if ( vw->graphs_widgets ) {
    vik_window_close_graphs ( vw );
  }

  gboolean need_redraw = FALSE;
  vw->containing_vtl = NULL;
  if ( vw->selected_vtl != NULL ) {
    vw->selected_vtl = NULL;
    vik_layers_panel_track_remove ( vw->viking_vlp );
    need_redraw = TRUE;
  }
  if ( vw->selected_track != NULL ) {
    vw->selected_track = NULL;
    vik_layers_panel_track_remove ( vw->viking_vlp );
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
