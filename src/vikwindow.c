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
#include "background.h"

#define VIKING_TITLE " - Viking " VIKING_VERSION " " VIKING_VERSION_NAME " " VIKING_URL

#include <math.h>
#include <string.h>
#include <ctype.h>

#ifdef WINDOWS
/* TODO IMPORTANT: mkdir for windows header? is it called 'mkdir' */
#define make_dir(dir) mkdir(dir)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define make_dir(dir) mkdir(dir,0777)
#endif

#define DRAW_IMAGE_DEFAULT_WIDTH 1280
#define DRAW_IMAGE_DEFAULT_HEIGHT 1024
#define DRAW_IMAGE_DEFAULT_SAVE_AS_PNG TRUE

static void window_finalize ( GObject *gob );
static GObjectClass *parent_class;

static void window_init ( VikWindow *vw );
static void window_class_init ( VikWindowClass *klass );

static void draw_update ( VikWindow *vw );

static void newwindow_cb ( VikWindow *vw );

/* Drawing & stuff */

static gboolean delete_event( VikWindow *vw );

static void draw_sync ( VikWindow *vw );
static void draw_redraw ( VikWindow *vw );
static void draw_scroll  ( VikWindow *vw, GdkEvent *event );
static void draw_click  ( VikWindow *vw, GdkEventButton *event );
static void draw_release ( VikWindow *vw, GdkEventButton *event );
static void draw_mouse_motion ( VikWindow *vw, GdkEventMotion *event );
static void draw_set_current_tool ( VikWindow *vw, guint mode );
static void draw_zoom ( VikWindow *vw, gint what );
static void draw_goto ( VikWindow *vw, gint mode );

static void draw_status ();

/* End Drawing Functions */

static void menu_addlayer_cb ( VikWindow *vw, gint type );
static void menu_properties_cb ( VikWindow *vw );
static void menu_delete_layer_cb ( VikWindow *vw );
static void menu_dynamic_tool_cb ( VikWindow *vw, guint id );

static GtkWidget *window_create_menubar( VikWindow *window );

static void load_file ( VikWindow *vw, gboolean newwindow );
static gboolean save_file_as ( VikWindow *vw );
static gboolean save_file ( VikWindow *vw );
static gboolean window_save ( VikWindow *vw );

struct _VikWindow {
  GtkWindow gtkwindow;
  VikViewport *viking_vvp;
  VikLayersPanel *viking_vlp;
  VikStatusbar *viking_vs;

  GtkItemFactory *item_factory;

  VikCoord oldcoord;
  gboolean has_oldcoord;
  guint current_tool;

  guint16 tool_layer_id;
  guint16 tool_tool_id;

  gint pan_x, pan_y;

  guint draw_image_width, draw_image_height;
  gboolean draw_image_save_as_png;

  gchar *filename;
  gboolean modified;

  GtkWidget *open_dia, *save_dia;

  gboolean only_updating_coord_mode_ui; /* hack for a bug in GTK */
};

enum {
 TOOL_ZOOM = 0,
 TOOL_RULER,
 TOOL_LAYER,
 NUMBER_OF_TOOLS
};

enum {
  VW_NEWWINDOW_SIGNAL,
  VW_OPENWINDOW_SIGNAL,
  VW_LAST_SIGNAL
};

static guint window_signals[VW_LAST_SIGNAL] = { 0 };

static gchar *tool_names[NUMBER_OF_TOOLS] = { "Zoom", "Ruler", "Pan" };

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

  gtk_window_set_title ( GTK_WINDOW(vw), "Untitled" VIKING_TITLE );

  vw->viking_vvp = vik_viewport_new();
  vw->viking_vlp = vik_layers_panel_new();
  vik_layers_panel_set_viewport ( vw->viking_vlp, vw->viking_vvp );
  vw->viking_vs = vik_statusbar_new();

  vw->filename = NULL;

  vw->has_oldcoord = FALSE;
  vw->modified = FALSE;
  vw->only_updating_coord_mode_ui = FALSE;

  vw->pan_x = vw->pan_y = -1;
  vw->draw_image_width = DRAW_IMAGE_DEFAULT_WIDTH;
  vw->draw_image_height = DRAW_IMAGE_DEFAULT_HEIGHT;
  vw->draw_image_save_as_png = DRAW_IMAGE_DEFAULT_SAVE_AS_PNG;

  main_vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add (GTK_CONTAINER (vw), main_vbox);

  gtk_box_pack_start (GTK_BOX(main_vbox), window_create_menubar(vw), FALSE, TRUE, 0);

  g_signal_connect (G_OBJECT (vw), "delete_event", G_CALLBACK (delete_event), NULL);

  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "expose_event", G_CALLBACK(draw_sync), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "configure_event", G_CALLBACK(draw_redraw), vw);
  gtk_widget_add_events ( GTK_WIDGET(vw->viking_vvp), GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "scroll_event", G_CALLBACK(draw_scroll), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_press_event", G_CALLBACK(draw_click), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "button_release_event", G_CALLBACK(draw_release), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vvp), "motion_notify_event", G_CALLBACK(draw_mouse_motion), vw);
  g_signal_connect_swapped (G_OBJECT(vw->viking_vlp), "update", G_CALLBACK(draw_update), vw);

  gtk_window_set_default_size ( GTK_WINDOW(vw), 1000, 800);

  hpaned = gtk_hpaned_new ();
  gtk_paned_add1 ( GTK_PANED(hpaned), GTK_WIDGET (vw->viking_vlp) );
  gtk_paned_add2 ( GTK_PANED(hpaned), GTK_WIDGET (vw->viking_vvp) );

  /* This packs the button into the window (a gtk container). */
  gtk_box_pack_start (GTK_BOX(main_vbox), hpaned, TRUE, TRUE, 0);

  gtk_box_pack_end (GTK_BOX(main_vbox), GTK_WIDGET(vw->viking_vs), FALSE, TRUE, 0);

  a_background_add_status(vw->viking_vs);

  vw->open_dia = NULL;
  vw->save_dia = NULL;
}

VikWindow *vik_window_new ()
{
  return VIK_WINDOW ( g_object_new ( VIK_WINDOW_TYPE, NULL ) );
}

static gboolean delete_event( VikWindow *vw )
{
  if ( vw->modified )
  {
    GtkDialog *dia;
    dia = GTK_DIALOG ( gtk_message_dialog_new ( GTK_WINDOW(vw), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
      "Do you want to save the changes you made to the document \"%s\"?\n\nYour changes will be lost if you don't save them.",
      vw->filename ? a_file_basename ( vw->filename ) : "Untitled" ) );
    gtk_dialog_add_buttons ( dia, "Don't Save", GTK_RESPONSE_NO, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL );
    switch ( gtk_dialog_run ( dia ) )
    {
      case GTK_RESPONSE_NO: gtk_widget_destroy ( GTK_WIDGET(dia) ); return FALSE;
      case GTK_RESPONSE_CANCEL: gtk_widget_destroy ( GTK_WIDGET(dia) ); return TRUE;
      default: gtk_widget_destroy ( GTK_WIDGET(dia) ); return ! save_file(vw);
    }
  }
  return FALSE;
}

/* Drawing stuff */
static void newwindow_cb ( VikWindow *vw )
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
  g_snprintf ( zoom_level, 22, "%.3f/%.3f %s", vik_viewport_get_xmpp (vw->viking_vvp), vik_viewport_get_ympp(vw->viking_vvp), vik_viewport_get_coord_mode(vw->viking_vvp) == VIK_COORD_UTM ? "mpp" : "pixelfact" );
  if ( vw->current_tool == TOOL_LAYER )
    vik_statusbar_set_message ( vw->viking_vs, 0, vik_layer_get_interface(vw->tool_layer_id)->tools[vw->tool_tool_id].name );
  else
    vik_statusbar_set_message ( vw->viking_vs, 0, tool_names[vw->current_tool] );

  vik_statusbar_set_message ( vw->viking_vs, 2, zoom_level );
}

static void draw_redraw ( VikWindow *vw )
{
  vik_viewport_clear ( vw->viking_vvp);

  vik_layers_panel_draw_all ( vw->viking_vlp );
}

static void draw_mouse_motion (VikWindow *vw, GdkEventMotion *event)
{
  static VikCoord coord;
  static struct UTM utm;
  static struct LatLon ll;
  static char pointer_buf[36];

  vik_viewport_screen_to_coord ( vw->viking_vvp, event->x, event->y, &coord );
  vik_coord_to_utm ( &coord, &utm );
  a_coords_utm_to_latlon ( &utm, &ll );

  g_snprintf ( pointer_buf, 36, "Cursor: %f %f", ll.lat, ll.lon );

  if ( vw->pan_x != -1 )
    vik_viewport_pan_sync ( vw->viking_vvp, event->x - vw->pan_x, event->y - vw->pan_y );

  vik_statusbar_set_message ( vw->viking_vs, 4, pointer_buf );
}

static void draw_scroll (VikWindow *vw, GdkEvent *event)
{
  if ( event->scroll.direction == GDK_SCROLL_UP )
    vik_viewport_zoom_in (vw->viking_vvp);
  else
    vik_viewport_zoom_out(vw->viking_vvp);
  draw_update(vw);
}

static void draw_release ( VikWindow *vw, GdkEventButton *event )
{
  if ( event->button == 2 ) { /* move / pan */
    if ( ABS(vw->pan_x - event->x) <= 1 && ABS(vw->pan_y - event->y) <= 1 )
        vik_viewport_set_center_screen ( vw->viking_vvp, vw->pan_x, vw->pan_y );
      else
         vik_viewport_set_center_screen ( vw->viking_vvp, vik_viewport_get_width(vw->viking_vvp)/2 - event->x + vw->pan_x,
                                         vik_viewport_get_height(vw->viking_vvp)/2 - event->y + vw->pan_y );
      draw_update ( vw );
      vw->pan_x = vw->pan_y = -1;
  }
  if ( vw->current_tool == TOOL_LAYER && ( event->button == 1 || event->button == 3 ) &&
      vik_layer_get_interface(vw->tool_layer_id)->tools[vw->tool_tool_id].callback_release )
  {
    vik_layers_panel_tool ( vw->viking_vlp, vw->tool_layer_id,
        vik_layer_get_interface(vw->tool_layer_id)->tools[vw->tool_tool_id].callback_release,
        event, vw->viking_vvp );
  }
}

static void draw_click (VikWindow *vw, GdkEventButton *event)
{
  if ( event->button == 2) {
    vw->pan_x = (gint) event->x;
    vw->pan_y = (gint) event->y;
  } else if ( vw->current_tool == TOOL_ZOOM )
  {
    vw->modified = TRUE;
    vik_viewport_set_center_screen ( vw->viking_vvp, (gint) event->x, (gint) event->y );
    if ( event->button == 1 )
      vik_viewport_zoom_in (vw->viking_vvp);
    else if ( event->button == 3 )
      vik_viewport_zoom_out (vw->viking_vvp);
    draw_update ( vw );
  }
  else if ( vw->current_tool == TOOL_RULER )
  {
    struct LatLon ll;
    VikCoord coord;
    gchar *temp;
    if ( event->button == 1 || event->button == 3 )
    {
      vik_viewport_screen_to_coord ( vw->viking_vvp, (gint) event->x, (gint) event->y, &coord );
      vik_coord_to_latlon ( &coord, &ll );
      if ( vw->has_oldcoord )
         temp = g_strdup_printf ( "%f %f DIFF %f meters", ll.lat, ll.lon, vik_coord_diff( &coord, &(vw->oldcoord) ) );
      else
        temp = g_strdup_printf ( "%f %f", ll.lat, ll.lon );

      vik_statusbar_set_message ( vw->viking_vs, 3, temp );
      g_free ( temp );

      vw->oldcoord = coord;
      /* we don't use anything else so far */
      vw->has_oldcoord = TRUE;
    }
    else
    {
      vik_viewport_set_center_screen ( vw->viking_vvp, (gint) event->x, (gint) event->y );
      draw_update ( vw );
    }
  }
  else if ( vw->current_tool == TOOL_LAYER )
  {
    vw->modified = TRUE;
    if ( ! vik_layers_panel_tool ( vw->viking_vlp, vw->tool_layer_id,
        vik_layer_get_interface(vw->tool_layer_id)->tools[vw->tool_tool_id].callback,
        event, vw->viking_vvp ) )
      a_dialog_info_msg_extra ( GTK_WINDOW(vw), "A %s Layer must exist and be either selected or visible before you can use this function.", vik_layer_get_interface ( vw->tool_layer_id )->name );
  }
}

static void draw_set_current_tool ( VikWindow *vw, guint mode )
{
  vw->current_tool = mode;
  draw_status ( vw );
}

static void draw_zoom ( VikWindow *vw, gint what )
{
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

void draw_goto ( VikWindow *vw, gint mode )
{
  VikCoord new_center;
  if ( mode == 1)
  {
    struct LatLon ll, llold;
    vik_coord_to_latlon ( vik_viewport_get_center ( vw->viking_vvp ), &llold );
    if ( a_dialog_goto_latlon ( GTK_WINDOW(vw), &ll, &llold ) )
      vik_coord_load_from_latlon ( &new_center, vik_viewport_get_coord_mode(vw->viking_vvp), &ll );
    else
      return;
  }
  else
  {
    struct UTM utm, utmold;
    vik_coord_to_utm ( vik_viewport_get_center ( vw->viking_vvp ), &utmold );
    if ( a_dialog_goto_utm ( GTK_WINDOW(vw), &utm, &utmold ) )
      vik_coord_load_from_utm ( &new_center, vik_viewport_get_coord_mode(vw->viking_vvp), &utm );
    else
     return;
  }

  vik_viewport_set_center_coord ( vw->viking_vvp, &new_center );
  draw_update ( vw );
}

static void menu_addlayer_cb ( VikWindow *vw, gint type )
{
  if ( vik_layers_panel_new_layer ( vw->viking_vlp, type ) )
  {
    draw_update ( vw );
    vw->modified = TRUE;
  }
}

static void menu_copy_layer_cb ( VikWindow *vw )
{
  a_clipboard_copy ( vw->viking_vlp );
}

static void menu_cut_layer_cb ( VikWindow *vw )
{
  a_clipboard_copy ( vw->viking_vlp );
  menu_delete_layer_cb ( vw );
}

static void menu_paste_layer_cb ( VikWindow *vw )
{
  if ( a_clipboard_paste ( vw->viking_vlp ) )
  {
    draw_update ( vw );
    vw->modified = TRUE;
  }
}

static void menu_properties_cb ( VikWindow *vw )
{
  if ( ! vik_layers_panel_properties ( vw->viking_vlp ) )
    a_dialog_info_msg ( GTK_WINDOW(vw), "You must select a layer to show its properties." );
}

static void menu_delete_layer_cb ( VikWindow *vw )
{
  if ( vik_layers_panel_get_selected ( vw->viking_vlp ) )
  {
    vik_layers_panel_delete_selected ( vw->viking_vlp );
    vw->modified = TRUE;
  }
  else
    a_dialog_info_msg ( GTK_WINDOW(vw), "You must select a layer to delete." );
}

static void menu_dynamic_tool_cb ( VikWindow *vw, guint id )
{
  /* White Magic, my friends ... White Magic... */
  vw->current_tool = TOOL_LAYER;
  vw->tool_layer_id = id >> 16;
  vw->tool_tool_id = id & 0xff;
  draw_status ( vw );
}

static void window_set_filename ( VikWindow *vw, const gchar *filename )
{
  gchar *title;
  if ( vw->filename )
    g_free ( vw->filename );
  if ( filename == NULL )
  {
    vw->filename = NULL;
    gtk_window_set_title ( GTK_WINDOW(vw), "Untitled" VIKING_TITLE );
  }
  else
  {
    vw->filename = g_strdup(filename);
    title = g_strconcat ( a_file_basename ( filename ), VIKING_TITLE, NULL );
    gtk_window_set_title ( GTK_WINDOW(vw), title );
    g_free ( title );
  }
}

void vik_window_open_file ( VikWindow *vw, const gchar *filename, gboolean change_filename )
{
  switch ( a_file_load ( vik_layers_panel_get_top_layer(vw->viking_vlp), vw->viking_vvp, filename ) )
  {
    case 0:
      a_dialog_error_msg ( GTK_WINDOW(vw), "The file you requested could not be opened." );
      break;
    case 1:
    {
      GtkWidget *mode_button;
      gchar *buttonname;
      if ( change_filename )
        window_set_filename ( vw, filename );
      switch ( vik_viewport_get_drawmode ( vw->viking_vvp ) ) {
        case VIK_VIEWPORT_DRAWMODE_UTM: buttonname = "/View/UTM Mode"; break;
        case VIK_VIEWPORT_DRAWMODE_EXPEDIA: buttonname = "/View/Expedia Mode"; break;
        case VIK_VIEWPORT_DRAWMODE_GOOGLE: buttonname = "/View/Google Mode"; break;
        default: buttonname = "/View/KH\\/Flat LatLon Mode";
      }
      mode_button = gtk_item_factory_get_item ( vw->item_factory, buttonname );
      g_assert ( mode_button );
      vw->only_updating_coord_mode_ui = TRUE; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
      gtk_check_menu_item_set_active ( GTK_CHECK_MENU_ITEM(mode_button), TRUE );
      vw->only_updating_coord_mode_ui = FALSE;

      vik_layers_panel_change_coord_mode ( vw->viking_vlp, vik_viewport_get_coord_mode ( vw->viking_vvp ) );
    }
    default: draw_update ( vw );
  }
}

static void load_file ( VikWindow *vw, gboolean newwindow )
{
  if ( ! vw->open_dia )
  {
    vw->open_dia = gtk_file_selection_new ("Please select a GPS data file to open. " );
    gtk_file_selection_set_select_multiple ( GTK_FILE_SELECTION(vw->open_dia), TRUE );
    gtk_window_set_transient_for ( GTK_WINDOW(vw->open_dia), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->open_dia), TRUE );
  }
  if ( gtk_dialog_run ( GTK_DIALOG(vw->open_dia) ) == GTK_RESPONSE_OK )
  {
    gtk_widget_hide ( vw->open_dia );
    if ( (vw->modified || vw->filename) && newwindow )
      g_signal_emit ( G_OBJECT(vw), window_signals[VW_OPENWINDOW_SIGNAL], 0, gtk_file_selection_get_selections (GTK_FILE_SELECTION(vw->open_dia) ) );
    else {
      gchar **files = gtk_file_selection_get_selections (GTK_FILE_SELECTION(vw->open_dia) );
      gboolean change_fn = newwindow && (!files[1]); /* only change fn if one file */
      while ( *files ) {
        vik_window_open_file ( vw, *files, change_fn );
        files++;
      }
    }
  }
  else
    gtk_widget_hide ( vw->open_dia );
}

static gboolean save_file_as ( VikWindow *vw )
{
  gboolean rv = FALSE;
  const gchar *fn;
  if ( ! vw->save_dia )
  {
    vw->save_dia = gtk_file_selection_new ("Save as Viking File. " );
    gtk_window_set_transient_for ( GTK_WINDOW(vw->save_dia), GTK_WINDOW(vw) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vw->save_dia), TRUE );
  }

  while ( gtk_dialog_run ( GTK_DIALOG(vw->save_dia) ) == GTK_RESPONSE_OK )
  {
    fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(vw->save_dia) );
    if ( access ( fn, F_OK ) != 0 || a_dialog_overwrite ( GTK_WINDOW(vw->save_dia), "The file \"%s\" exists, do you wish to overwrite it?", a_file_basename ( fn ) ) )
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
    return TRUE;
  else
  {
    a_dialog_error_msg ( GTK_WINDOW(vw), "The filename you requested could not be opened for writing." );
    return FALSE;
  }
}

static gboolean save_file ( VikWindow *vw )
{
  if ( ! vw->filename )
    return save_file_as ( vw );
  else
  {
    vw->modified = FALSE;
    return window_save ( vw );
  }
}

static void clear_cb ( VikWindow *vw )
{
  vik_layers_panel_clear ( vw->viking_vlp );
  window_set_filename ( vw, NULL );
  draw_update ( vw );
}

static void window_close ( VikWindow *vw )
{
  if ( ! delete_event ( vw ) )
    gtk_widget_destroy ( GTK_WIDGET(vw) );
}

static void zoom_to ( VikWindow *vw )
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
    fprintf(stderr, "Unable to write to file %s: %s", fn, error->message );
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

  make_dir(fn);

  utm_orig = *((const struct UTM *)vik_viewport_get_center ( vw->viking_vvp ));

  for ( y = 1; y <= tiles_h; y++ )
  {
    for ( x = 1; x <= tiles_w; x++ )
    {
      g_snprintf ( name_of_file, size, "%s%cy%d-x%d.%s", fn, VIKING_FILE_SEP, y, x, save_as_png ? "png" : "jpg" );
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
        fprintf(stderr, "Unable to write to file %s: %s", name_of_file, error->message );
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
    a_dialog_info_msg ( GTK_WINDOW(vw), "Viewable region outside allowable pixel size bounds for image. Clipping width/height values." );

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
  label_text = g_strdup_printf ( "Total area: %ldm x %ldm (%.3f sq. km)", (glong)w, (glong)h, (w*h/1000000));
  gtk_label_set_text(GTK_LABEL(pass_along[6]), label_text);
  g_free ( label_text );
}

static void draw_to_image_file ( VikWindow *vw, const gchar *fn, gboolean one_image_only )
{
  /* todo: default for answers inside VikWindow or static (thruout instance) */
  GtkWidget *dialog = gtk_dialog_new_with_buttons ( "Save to Image File", GTK_WINDOW(vw),
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  0 );
  GtkWidget *width_label, *width_spin, *height_label, *height_spin;
  GtkWidget *png_radio, *jpeg_radio;
  GtkWidget *current_window_button;
  gpointer current_window_pass_along[7];
  GtkWidget *zoom_label, *zoom_spin;
  GtkWidget *total_size_label;

  /* only used if (!one_image_only) */
  GtkWidget *tiles_width_spin, *tiles_height_spin;


  width_label = gtk_label_new ( "Width (pixels):" );
  width_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vw->draw_image_width, 10, 5000, 10, 100, 0 )), 10, 0 );
  height_label = gtk_label_new ( "Height (pixels):" );
  height_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vw->draw_image_height, 10, 5000, 10, 100, 0 )), 10, 0 );

  zoom_label = gtk_label_new ( "Zoom (meters per pixel):" );
  /* TODO: separate xzoom and yzoom factors */
  zoom_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( vik_viewport_get_xmpp(vw->viking_vvp), VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM/2.0, 1, 100, 3 )), 16, 3);

  total_size_label = gtk_label_new ( NULL );

  current_window_button = gtk_button_new_with_label ( "Area in current viewable window" );
  current_window_pass_along [0] = vw;
  current_window_pass_along [1] = width_spin;
  current_window_pass_along [2] = height_spin;
  current_window_pass_along [3] = zoom_spin;
  current_window_pass_along [4] = NULL; /* used for one_image_only != 1 */
  current_window_pass_along [5] = NULL;
  current_window_pass_along [6] = total_size_label;
  g_signal_connect ( G_OBJECT(current_window_button), "button_press_event", G_CALLBACK(draw_to_image_file_current_window_cb), current_window_pass_along );

  png_radio = gtk_radio_button_new_with_label ( NULL, "Save as PNG" );
  jpeg_radio = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(png_radio), "Save as JPEG" );

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


    tiles_width_label = gtk_label_new ( "East-west image tiles:" );
    tiles_width_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new ( 5, 1, 10, 1, 100, 0 )), 1, 0 );
    tiles_height_label = gtk_label_new ( "North-south image tiles:" );
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
      if ( vik_viewport_get_coord_mode(vw->viking_vvp) == VIK_COORD_UTM )
        save_image_dir ( vw, fn, 
                       vw->draw_image_width = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(width_spin) ),
                       vw->draw_image_height = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(height_spin) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(zoom_spin) ), /* do not save this value, default is current zoom */
                       vw->draw_image_save_as_png = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(png_radio) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(tiles_width_spin) ),
                       gtk_spin_button_get_value ( GTK_SPIN_BUTTON(tiles_height_spin) ) );
      else
        a_dialog_error_msg ( GTK_WINDOW(vw), "You must be in UTM mode to use this feature" );
    }
  }
  gtk_widget_destroy ( GTK_WIDGET(dialog) );
}


static void draw_to_image_file_cb ( VikWindow *vw )
{
  GtkWidget *file_selector;
  const gchar *fn;
  file_selector = gtk_file_selection_new ("Save Image");

  while ( gtk_dialog_run ( GTK_DIALOG(file_selector) ) == GTK_RESPONSE_OK )
  {
    fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(file_selector) );
    if ( access ( fn, F_OK ) != 0 || a_dialog_overwrite ( GTK_WINDOW(file_selector), "The file \"%s\" exists, do you wish to overwrite it?", a_file_basename ( fn ) ) )
    {
      gtk_widget_hide ( file_selector );
      draw_to_image_file ( vw, fn, TRUE );
      break;
    }
  }
  gtk_widget_destroy ( file_selector );
}

static void draw_to_image_dir_cb ( VikWindow *vw )
{
  GtkWidget *file_selector;
  const gchar *fn;
  file_selector = gtk_file_selection_new ("Choose a name for a new directory to hold images");

  while ( gtk_dialog_run ( GTK_DIALOG(file_selector) ) == GTK_RESPONSE_OK )
  {
    fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(file_selector) );
    if ( access ( fn, F_OK ) == 0 )
      a_dialog_info_msg_extra ( GTK_WINDOW(file_selector), "The file %s exists. Please choose a name for a new directory to hold images in that does not exist.", a_file_basename(fn) );
    else
    {
      gtk_widget_hide ( file_selector );
      draw_to_image_file ( vw, fn, FALSE );
      break;
    }
  }
  gtk_widget_destroy ( file_selector );
}

/* really a misnomer: changes coord mode (actual coordinates) AND/OR draw mode (viewport only) */
static void window_change_coord_mode ( VikWindow *vw, VikViewportDrawMode drawmode )
{
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

static void set_bg_color ( VikWindow *vw )
{
  GtkWidget *colorsd = gtk_color_selection_dialog_new ("Choose a background color");
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

static GtkItemFactoryEntry menu_items[] = {
  { "/_File", NULL, NULL, 0, "<Branch>" },
  { "/File/_New", "<control>N", newwindow_cb, 0, "<StockItem>", GTK_STOCK_NEW }, 
  { "/File/_Open", "<control>O", load_file, TRUE, "<StockItem>", GTK_STOCK_OPEN },
  { "/File/A_ppend File", NULL, load_file, FALSE, "<Item>" },
  { "/File/_Save", "<control>S", (GtkItemFactoryCallback) save_file, 0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/Save _As", NULL, (GtkItemFactoryCallback) save_file_as, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
  { "/File/sep1", NULL, NULL, 0, "<Separator>" },
  { "/File/_Generate Image File", NULL, draw_to_image_file_cb, 0, "<Item>" },
  { "/File/Generate Directory of Images", NULL, draw_to_image_dir_cb, 0, "<Item>" },
  { "/File/sep1", NULL, NULL, 0, "<Separator>" },
  { "/File/_Close", "<CTRL>W", window_close, 0, "<StockItem>", GTK_STOCK_QUIT },
  { "/_View", NULL, NULL, 0, "<Branch>" },
  { "/View/_UTM Mode", "<ctrl>u", (GtkItemFactoryCallback) window_change_coord_mode, VIK_VIEWPORT_DRAWMODE_UTM, "<RadioItem>" },
  { "/View/_Expedia Mode", "<ctrl>e", (GtkItemFactoryCallback) window_change_coord_mode, VIK_VIEWPORT_DRAWMODE_EXPEDIA, "/View/UTM Mode" },
  { "/View/_Google Mode", "<ctrl>g", (GtkItemFactoryCallback) window_change_coord_mode, VIK_VIEWPORT_DRAWMODE_GOOGLE, "/View/UTM Mode" },
  { "/View/_KH\\/Flat LatLon Mode", "<ctrl>k", (GtkItemFactoryCallback) window_change_coord_mode, VIK_VIEWPORT_DRAWMODE_KH, "/View/UTM Mode" },
  { "/View/_Mercator (New Google)", "<ctrl>m", (GtkItemFactoryCallback) window_change_coord_mode, VIK_VIEWPORT_DRAWMODE_MERCATOR, "/View/UTM Mode" },
  { "/View/sep1", NULL, NULL, 0, "<Separator>" },
  { "/View/_Go to Lat\\/Lon...", NULL, draw_goto, 1, "<Item>" },
  { "/View/Go to UTM...", NULL, draw_goto, 2, "<Item>" },
  { "/View/sep1", NULL, NULL, 0, "<Separator>" },
  { "/View/Set Background Color...", NULL, set_bg_color, 0, "<StockItem>", GTK_STOCK_SELECT_COLOR },
  { "/View/sep1", NULL, NULL, 0, "<Separator>" },
  { "/View/Zoom _In", "<ctrl>plus", draw_zoom, -3, "<StockItem>", GTK_STOCK_ZOOM_IN },
  { "/View/Zoom _Out", "<ctrl>minus", draw_zoom, -4, "<StockItem>", GTK_STOCK_ZOOM_OUT },
  { "/View/Zoom To...", "<ctrl><shift>Z", zoom_to, 0, "<Item>" },
  { "/View/_Zoom", NULL, NULL, 0, "<Branch>" },
  { "/View/Zoom/0.25", "<ctrl>1", draw_zoom, -2, "<Item>" },
  { "/View/Zoom/0.5", "<ctrl>2", draw_zoom, -1, "<Item>" },
  { "/View/Zoom/1", "<ctrl>3", draw_zoom, 1, "<Item>" },
  { "/View/Zoom/2", "<ctrl>4", draw_zoom, 2, "<Item>" },
  { "/View/Zoom/4", "<ctrl>5", draw_zoom, 4, "<Item>" },
  { "/View/Zoom/8", "<ctrl>6", draw_zoom, 8, "<Item>" },
  { "/View/Zoom/16", "<ctrl>7", draw_zoom, 16, "<Item>" },
  { "/View/Zoom/32", "<ctrl>8", draw_zoom, 32, "<Item>" },
  { "/View/Zoom/64", "<ctrl>9", draw_zoom, 64, "<Item>" },
  { "/View/Zoom/128", "<ctrl>0", draw_zoom, 64, "<Item>" },
  { "/View/sep1", NULL, NULL, 0, "<Separator>" },
  { "/View/Background _Jobs...", "<ctrl>j", (GtkItemFactoryCallback) a_background_show_window, 0, "<Item>" },

  { "/_Layers", NULL, NULL, 0, "<Branch>" },
  { "/Layers/Cu_t", NULL, menu_cut_layer_cb, -1, "<StockItem>", GTK_STOCK_CUT },
  { "/Layers/_Copy", NULL, menu_copy_layer_cb, -1, "<StockItem>", GTK_STOCK_COPY },
  { "/Layers/_Paste", NULL, menu_paste_layer_cb, -1, "<StockItem>", GTK_STOCK_PASTE },
  { "/Layers/sep1", NULL, NULL, 0, "<Separator>" },
  { "/Layers/_Properties", NULL, menu_properties_cb, -1, "<StockItem>", GTK_STOCK_PROPERTIES },
  { "/Layers/_Delete", NULL, menu_delete_layer_cb, -1, "<StockItem>", GTK_STOCK_DELETE },
  { "/Layers/Delete All", NULL, clear_cb, 0, "<StockItem>", GTK_STOCK_CLEAR },
  { "/Layers/sep1", NULL, NULL, 0, "<Separator>" },
  /* Plus Dynamic */

  { "/_Tools", NULL, NULL, 0, "<Branch>" },
  { "/Tools/sep1", NULL, NULL, 0, "<Tearoff>" },
  { "/Tools/_Zoom", "<ctrl><shift>Z", draw_set_current_tool, TOOL_ZOOM, "<Item>" },
  { "/Tools/_Ruler", "<ctrl><shift>R", draw_set_current_tool, TOOL_RULER, "<Item>" },
  /* Plus Dynamic */
};

static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
static GtkItemFactoryEntry tool_sep = { "/_Tools/sep1", NULL, NULL, 0, "<Separator>" };

static GtkWidget *window_create_menubar( VikWindow *window )
{
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  GtkItemFactoryEntry add_layer_item;
  guint i, j, tmp;

  g_assert ( sizeof(guint16)*2 <= sizeof(guint) ); /* FIXME: should be a compiler warning */

  accel_group = gtk_accel_group_new ();
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
                                       accel_group);
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, window);
  gtk_window_add_accel_group ( GTK_WINDOW(window), accel_group);

  add_layer_item.accelerator = NULL;

  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
  {
    /* TODO: FIXME: if name has a '/' in it it will get all messed up. why not have an itemfactory field with
                    name, serialized icon, shortcut, etc.? */
    add_layer_item.path = g_strdup_printf("/_Layers/New %s Layer", vik_layer_get_interface(i)->name );
    add_layer_item.callback = menu_addlayer_cb;
    add_layer_item.callback_action = i;
    if ( vik_layer_get_interface(i)->icon )
    {
      add_layer_item.item_type = "<ImageItem>";
      add_layer_item.extra_data = gdk_pixdata_serialize ( vik_layer_get_interface(i)->icon, &tmp );
    }
    else
      add_layer_item.item_type = "<Item>";
    gtk_item_factory_create_item ( item_factory, &add_layer_item, window, 1 );
    g_free ( add_layer_item.path );
    g_free ( (gpointer) add_layer_item.extra_data );

    if ( vik_layer_get_interface(i)->tools_count )
      gtk_item_factory_create_item ( item_factory, &tool_sep, window, 1 );

    for ( j = 0; j < vik_layer_get_interface(i)->tools_count; j++ )
    {
      add_layer_item.path = g_strdup_printf("/_Tools/%s", vik_layer_get_interface(i)->tools[j].name);
      add_layer_item.callback = menu_dynamic_tool_cb;
      add_layer_item.callback_action = ( i << 16 ) | j;
      add_layer_item.item_type = "<Item>";
      gtk_item_factory_create_item ( item_factory, &add_layer_item, window, 1 );
      g_free ( add_layer_item.path );
    }
  }

  window->item_factory = item_factory;
  return gtk_item_factory_get_widget (item_factory, "<main>");
}
