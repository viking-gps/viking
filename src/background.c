/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2015, Rob Norris <rw_norris@hotmail.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <math.h>

#include "background.h"
#include "settings.h"
#include "util.h"
#include "uibuilder.h"
#include "globals.h"
#include "preferences.h"

static GThreadPool *thread_pool_remote = NULL;
static GThreadPool *thread_pool_local = NULL;
#ifdef HAVE_LIBMAPNIK
static GThreadPool *thread_pool_local_mapnik = NULL;
#endif
static gboolean stop_all_threads = FALSE;

// A single store of background items for all Windows
// Must always be accessed in the main thread
static GtkListStore *bgstore = NULL;

G_LOCK_DEFINE_STATIC(window_list);
// Still only actually updating the statusbar though
static GSList *windows_to_update = NULL;

static gint bgitemcount = 0;

#define VIK_BG_NUM_ARGS 8

enum
{
  TITLE_COLUMN = 0,
  PROGRESS_COLUMN,
  DATA_COLUMN,
  N_COLUMNS,
};

void a_background_update_status ( VikWindow *vw, gpointer data )
{
  static gchar buf[20];
  g_snprintf(buf, sizeof(buf), _("%d items"), bgitemcount);
  vik_window_statusbar_update ( vw, buf, VIK_STATUSBAR_ITEMS );
}

// NB can be called from any thread
static void background_thread_update ()
{
  G_LOCK(window_list);
  g_slist_foreach ( windows_to_update, (GFunc) a_background_update_status, NULL );
  G_UNLOCK(window_list);
}

typedef struct {
  GtkTreeIter *iter;
  gdouble percent;
} progress_t;

// In main thread
static gboolean idle_progress_update ( gpointer user_data )
{
  progress_t *progress = user_data;
  if ( bgstore && progress->iter )
    gtk_list_store_set ( GTK_LIST_STORE(bgstore), progress->iter, PROGRESS_COLUMN, progress->percent, -1 );
  g_free ( progress );
  return FALSE;
}

/**
 * a_background_thread_progress:
 * @callbackdata: Thread data
 * @fraction:     The value should be between 0.0 and 1.0 indicating percentage of the task complete
 *
 * Called from other threads
 *
 * Returns a non zero number if the thread should be terminated
 */
int a_background_thread_progress ( gpointer callbackdata, gdouble fraction )
{
  gpointer *args = (gpointer *) callbackdata;
  int res = a_background_testcancel ( callbackdata );
  if (args[5] != NULL) {
    gdouble myfraction = fabs(fraction);
    if ( myfraction > 1.0 )
      myfraction = 1.0;
    progress_t *progress = g_malloc0 ( sizeof(progress_t) );
    progress->percent = myfraction * 100;
    progress->iter = (GtkTreeIter*)args[5];
    args[7] = GUINT_TO_POINTER(gdk_threads_add_idle ( idle_progress_update, progress ));
  }

  args[6] = GINT_TO_POINTER(GPOINTER_TO_INT(args[6])-1);
  bgitemcount--;
  background_thread_update();
  return res;
}

static void thread_die ( gpointer args[VIK_BG_NUM_ARGS] )
{
  vik_thr_free_func userdata_free_func = args[3];

  if ( userdata_free_func != NULL )
    userdata_free_func ( args[2] );

  if ( GPOINTER_TO_INT(args[6]) )
  {
    bgitemcount -= GPOINTER_TO_INT(args[6]);
    background_thread_update ();
  }

  g_free ( args );
}

// Called from other threads
// Returns a non zero number if the thread should be terminated
int a_background_testcancel ( gpointer callbackdata )
{
  gpointer *args = (gpointer *) callbackdata;
  if ( stop_all_threads )
    return -1;
  if ( args && args[0] )
  {
    vik_thr_free_func cleanup = args[4];
    if ( cleanup )
      cleanup ( args[2] );
    return -1;
  }
  return 0;
}

// Called from the main thread
static gboolean idle_remove ( gpointer user_data )
{
  if ( bgstore && user_data ) {
    gtk_list_store_remove ( bgstore, (GtkTreeIter*)user_data );
    g_free ( user_data );
  }
  return FALSE;
}

// Called from other threads
static void thread_helper ( gpointer args[VIK_BG_NUM_ARGS], gpointer user_data )
{
  /* unpack args */
  vik_thr_func func = args[1];
  gpointer userdata = args[2];

  g_debug(__FUNCTION__);

  func ( userdata, args );

  if ( ! args[0] ) {
    gdk_threads_add_idle ( idle_remove, args[5] );
  }
  thread_die ( args );
}

/**
 * a_background_thread:
 * @bp:      Which pool this thread should run in
 * @parent:
 * @message:
 * @func: worker function
 * @userdata:
 * @userdata_free_func: free function for userdata
 * @userdata_cancel_cleanup_func:
 * @number_items:
 *
 * Function to enlist new background function.
 */
void a_background_thread ( Background_Pool_Type bp, GtkWindow *parent, const gchar *message, vik_thr_func func, gpointer userdata, vik_thr_free_func userdata_free_func, vik_thr_free_func userdata_cancel_cleanup_func, gint number_items )
{
  GtkTreeIter *piter = g_malloc ( sizeof ( GtkTreeIter ) );
  gpointer *args = g_malloc ( sizeof(gpointer) * VIK_BG_NUM_ARGS );

  g_debug(__FUNCTION__);

  args[0] = GINT_TO_POINTER(0);
  args[1] = func;
  args[2] = userdata;
  args[3] = userdata_free_func;
  args[4] = userdata_cancel_cleanup_func;
  args[5] = piter;
  args[6] = GINT_TO_POINTER(number_items);
  args[7] = GUINT_TO_POINTER(0); // Will be id of progress update func

  bgitemcount += number_items;

  gtk_list_store_append ( bgstore, piter );
  gtk_list_store_set ( bgstore, piter,
		       TITLE_COLUMN, message,
		       PROGRESS_COLUMN, 0.0,
		       DATA_COLUMN, args,
		       -1 );

  /* run the thread in the background */
  if ( bp == BACKGROUND_POOL_REMOTE )
    g_thread_pool_push( thread_pool_remote, args, NULL );
#ifdef HAVE_LIBMAPNIK
  else if ( bp == BACKGROUND_POOL_LOCAL_MAPNIK )
    g_thread_pool_push( thread_pool_local_mapnik, args, NULL );
#endif
  else
    g_thread_pool_push( thread_pool_local, args, NULL );
}

// In main thread
static void cancel_job_with_iter ( GtkTreeIter *piter )
{
    gpointer *args;

    g_debug(__FUNCTION__);

    gtk_tree_model_get( GTK_TREE_MODEL(bgstore), piter, DATA_COLUMN, &args, -1 );

    /* we know args still exists because it is free _after_ the list item is destroyed */
    /* need MUTEX ? */
    args[0] = GINT_TO_POINTER(1); /* set killswitch */
    (void)g_source_remove ( GPOINTER_TO_UINT(args[7]) ); // Stop next idle_progress_update() from running
    g_free ( args[5] );
    args[5] = NULL;
}

static GtkWidget *bgwindow = NULL;

// In main thread
static void bgwindow_response (GtkDialog *dialog, gint response_id, GtkTreeView *bgtreeview )
{
  switch ( response_id ) {
  case GTK_RESPONSE_DELETE_EVENT:
    // Delibrate fall through
  case GTK_RESPONSE_CLOSE:
    bgwindow = NULL;
    gtk_widget_destroy ( GTK_WIDGET(dialog) );
    break;
  case 1: // Delete / Cancel selected item
    {
      GtkTreeIter iter;
      if ( gtk_tree_selection_get_selected ( gtk_tree_view_get_selection(bgtreeview), NULL, &iter ) ) {
        cancel_job_with_iter ( &iter );
        gtk_list_store_remove ( bgstore, &iter );
      }
      background_thread_update();
    }
    break;
  case 2: // Clear All jobs
    {
      GtkTreeIter iter;
      if ( gtk_tree_model_get_iter_first(GTK_TREE_MODEL(bgstore), &iter) )
        do {
          cancel_job_with_iter ( &iter );
        } while ( gtk_tree_model_iter_next(GTK_TREE_MODEL(bgstore), &iter) );
      gtk_list_store_clear ( bgstore );
      background_thread_update();
    }
    default: break;
  }
}

#define VIK_SETTINGS_BACKGROUND_MAX_THREADS "background_max_threads"
#define VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL "background_max_threads_local"

#ifdef HAVE_LIBMAPNIK
// Default to 1 thread due to potential crashing issues
static VikLayerParamData mpk_thrds_default ( void ) { return VIK_LPD_UINT(1); }

VikLayerParamScale params_threads[] = { {1, 64, 1, 0} }; // 64 threads should be enough for anyone...
// implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues
static VikLayerParam prefs_mapnik[] = {
  { VIK_LAYER_NUM_TYPES, "mapnik.background_max_threads_local_mapnik", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Threads:"), VIK_LAYER_WIDGET_SPINBUTTON, params_threads, NULL,
    N_("Number of threads to use for Mapnik tasks. You need to restart Viking for a change to this value to be used"), mpk_thrds_default, NULL, NULL },
};
#endif

/**
 * a_background_init:
 *
 * Just setup any preferences.
 */
void a_background_init ()
{
#ifdef HAVE_LIBMAPNIK
  // implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues
  a_preferences_register(&prefs_mapnik[0], (VikLayerParamData){0}, "mapnik");
#endif
}

/**
 * a_background_post_init:
 *
 * Initialize background feature.
 */
void a_background_post_init()
{
  // initialize thread pools
  gint max_threads = 10;  /* limit maximum number of threads running at one time */
  gint maxt;
  if ( a_settings_get_integer ( VIK_SETTINGS_BACKGROUND_MAX_THREADS, &maxt ) )
    max_threads = maxt;

  thread_pool_remote = g_thread_pool_new ( (GFunc) thread_helper, NULL, max_threads, FALSE, NULL );

  if ( a_settings_get_integer ( VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL, &maxt ) )
    max_threads = maxt;
  else {
    guint cpus = util_get_number_of_cpus ();
    max_threads = cpus > 1 ? cpus-1 : 1; // Don't use all available CPUs!
  }

  thread_pool_local = g_thread_pool_new ( (GFunc) thread_helper, NULL, max_threads, FALSE, NULL );

#ifdef HAVE_LIBMAPNIK
  // implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues
  guint mapnik_threads = a_preferences_get("mapnik.background_max_threads_local_mapnik")->u;
  thread_pool_local_mapnik = g_thread_pool_new ( (GFunc) thread_helper, NULL, mapnik_threads, FALSE, NULL );
#endif

  bgstore = gtk_list_store_new ( N_COLUMNS, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_POINTER );
}

/**
 * a_background_show_window:
 *
 * Display the background window.
 */
void a_background_show_window ()
{
  // Only allow one dialog showing background processes
  if ( bgwindow )
    return;

  GtkWidget *bgtreeview = NULL;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *scrolled_window;

  /* store & treeview */
  bgtreeview = gtk_tree_view_new_with_model ( GTK_TREE_MODEL(bgstore) );
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (bgtreeview), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (bgtreeview)),
                               GTK_SELECTION_SINGLE);

  /* add columns */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ( _("Job"), renderer, "text", TITLE_COLUMN, NULL );
  gtk_tree_view_append_column ( GTK_TREE_VIEW(bgtreeview), column );

  renderer = gtk_cell_renderer_progress_new ();
  column = gtk_tree_view_column_new_with_attributes ( _("Progress"), renderer, "value", PROGRESS_COLUMN, NULL );
  gtk_tree_view_append_column ( GTK_TREE_VIEW(bgtreeview), column );

  /* setup window */
  scrolled_window = gtk_scrolled_window_new ( NULL, NULL );
  gtk_container_add ( GTK_CONTAINER(scrolled_window), bgtreeview );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

  bgwindow = gtk_dialog_new_with_buttons ( _("Viking Background Jobs"), NULL, 0, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, GTK_STOCK_DELETE, 1, GTK_STOCK_CLEAR, 2, NULL );
  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(bgwindow))), scrolled_window, TRUE, TRUE, 0 );
  gtk_window_set_default_size ( GTK_WINDOW(bgwindow), 400, 400 );

  g_signal_connect ( G_OBJECT(bgwindow), "response", G_CALLBACK(bgwindow_response), GTK_TREE_VIEW(bgtreeview) );

  gtk_widget_show_all ( bgwindow );

  gtk_dialog_set_default_response ( GTK_DIALOG(bgwindow), GTK_RESPONSE_CLOSE );
}

/**
 * a_background_uninit:
 *
 * Uninitialize background feature.
 */
void a_background_uninit()
{
  stop_all_threads = TRUE;
  // Don't wait for these threads to complete - i.e. end now.
  g_thread_pool_free ( thread_pool_remote, TRUE, FALSE );
  g_thread_pool_free ( thread_pool_local, TRUE, FALSE );
#ifdef HAVE_LIBMAPNIK
  g_thread_pool_free ( thread_pool_local_mapnik, TRUE, FALSE );
#endif
  gtk_list_store_clear ( bgstore );
  g_object_unref ( bgstore );
  bgstore = NULL;
}

void a_background_add_window (VikWindow *vw)
{
  G_LOCK(window_list);
  windows_to_update = g_slist_prepend(windows_to_update,vw);
  G_UNLOCK(window_list);
}

void a_background_remove_window (VikWindow *vw)
{
  G_LOCK(window_list);
  windows_to_update = g_slist_remove(windows_to_update,vw);
  G_UNLOCK(window_list);
}
