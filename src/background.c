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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "vikstatus.h"
#include "background.h"

static GThreadPool *thread_pool = NULL;
static gboolean stop_all_threads = FALSE;

static GtkWidget *bgwindow = NULL;
static GtkWidget *bgtreeview = NULL;
static GtkListStore *bgstore = NULL;

static GSList *statusbars_to_update = NULL;

static gint bgitemcount = 0;

enum
{
  TITLE_COLUMN = 0,
  PROGRESS_COLUMN,
  DATA_COLUMN,
  N_COLUMNS,
};

void a_background_update_status ( VikStatusbar *vs, gchar *str )
{
  gdk_threads_enter ();
  vik_statusbar_set_message ( vs, 1, str );
  gdk_threads_leave ();
}

static void background_thread_update ()
{
  static gchar buf[20];
  g_snprintf(buf, sizeof(buf), _("%d items"), bgitemcount);
  g_slist_foreach ( statusbars_to_update, (GFunc) a_background_update_status, buf );
}

int a_background_thread_progress ( gpointer callbackdata, gdouble fraction )
{
  gpointer *args = (gpointer *) callbackdata;
  int res = a_background_testcancel ( callbackdata );
  if (args[5] != NULL) {
    gdk_threads_enter();
    gtk_list_store_set( GTK_LIST_STORE(bgstore), (GtkTreeIter *) args[5], PROGRESS_COLUMN, fraction*100, -1 );
    gdk_threads_leave();
  }

  args[6] = GINT_TO_POINTER(GPOINTER_TO_INT(args[6])-1);
  bgitemcount--;
  background_thread_update();
  return res;
}

static void thread_die ( gpointer args[6] )
{
  vik_thr_free_func userdata_free_func = args[3];

  if ( userdata_free_func != NULL )
    userdata_free_func ( args[2] );

  if ( GPOINTER_TO_INT(args[6]) )
  {
    bgitemcount -= GPOINTER_TO_INT(args[6]);
    background_thread_update ();
  }

  g_free ( args[5] ); /* free iter */
  g_free ( args );
}

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

static void thread_helper ( gpointer args[6], gpointer user_data )
{
  /* unpack args */
  vik_thr_func func = args[1];
  gpointer userdata = args[2];

  g_debug(__FUNCTION__);

  func ( userdata, args );

  gdk_threads_enter();
  if ( ! args[0] )
    gtk_list_store_remove ( bgstore, (GtkTreeIter *) args[5] );
  gdk_threads_leave();

  thread_die ( args );
}

/**
 * a_background_thread:
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
void a_background_thread ( GtkWindow *parent, const gchar *message, vik_thr_func func, gpointer userdata, vik_thr_free_func userdata_free_func, vik_thr_free_func userdata_cancel_cleanup_func, gint number_items )
{
  GtkTreeIter *piter = g_malloc ( sizeof ( GtkTreeIter ) );
  gpointer *args = g_malloc ( sizeof(gpointer) * 7 );

  g_debug(__FUNCTION__);

  args[0] = GINT_TO_POINTER(0);
  args[1] = func;
  args[2] = userdata;
  args[3] = userdata_free_func;
  args[4] = userdata_cancel_cleanup_func;
  args[5] = piter;
  args[6] = GINT_TO_POINTER(number_items);

  bgitemcount += number_items;

  gtk_list_store_append ( bgstore, piter );
  gtk_list_store_set ( bgstore, piter,
		       TITLE_COLUMN, message,
		       PROGRESS_COLUMN, 0.0,
		       DATA_COLUMN, args,
		       -1 );

  /* run the thread in the background */
  g_thread_pool_push( thread_pool, args, NULL );
}

/**
 * a_background_show_window:
 *
 * Display the background window.
 */
void a_background_show_window ()
{
  gtk_widget_show_all ( bgwindow );
}

static void cancel_job_with_iter ( GtkTreeIter *piter )
{
    gpointer *args;

    g_debug(__FUNCTION__);

    gtk_tree_model_get( GTK_TREE_MODEL(bgstore), piter, DATA_COLUMN, &args, -1 );

    /* we know args still exists because it is free _after_ the list item is destroyed */
    /* need MUTEX ? */
    args[0] = GINT_TO_POINTER(1); /* set killswitch */

    gtk_list_store_remove ( bgstore, piter );
    args[5] = NULL;
}

static void bgwindow_response (GtkDialog *dialog, gint arg1 )
{
  /* note this function is a signal handler called back from the GTK main loop, 
   * so GDK is already locked.  We need to release the lock before calling 
   * thread-safe routines
   */
  if ( arg1 == 1 ) /* cancel */
    {
      GtkTreeIter iter;
      if ( gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( GTK_TREE_VIEW(bgtreeview) ), NULL, &iter ) )
	cancel_job_with_iter ( &iter );
      gdk_threads_leave();
      background_thread_update();
      gdk_threads_enter();
    }
  else if ( arg1 == 2 ) /* clear */
    {
      GtkTreeIter iter;
      while ( gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(bgstore), &iter ) )
	cancel_job_with_iter ( &iter );
      gdk_threads_leave();
      background_thread_update();
      gdk_threads_enter();
    }
  else /* OK */
    gtk_widget_hide ( bgwindow );
}

/**
 * a_background_init:
 *
 * Initialize background feature.
 */
void a_background_init()
{
  /* initialize thread pool */
  /* TODO parametrize this via preference and/or command line arg */
  gint max_threads = 10;  /* limit maximum number of threads running at one time */
  thread_pool = g_thread_pool_new ( (GFunc) thread_helper, NULL, max_threads, FALSE, NULL );

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *scrolled_window;

  g_debug(__FUNCTION__);

  /* store & treeview */
  bgstore = gtk_list_store_new ( N_COLUMNS, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_POINTER );
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

  bgwindow = gtk_dialog_new_with_buttons ( "", NULL, 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_DELETE, 1, GTK_STOCK_CLEAR, 2, NULL );
  gtk_dialog_set_default_response ( GTK_DIALOG(bgwindow), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(bgwindow), GTK_RESPONSE_ACCEPT );
#endif
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(bgwindow)->vbox), scrolled_window, TRUE, TRUE, 0 );
  gtk_window_set_default_size ( GTK_WINDOW(bgwindow), 400, 400 );
  gtk_window_set_title ( GTK_WINDOW(bgwindow), _("Viking Background Jobs") );
  if ( response_w )
    gtk_widget_grab_focus ( response_w );
  /* don't destroy win */
  g_signal_connect ( G_OBJECT(bgwindow), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL );

  g_signal_connect ( G_OBJECT(bgwindow), "response", G_CALLBACK(bgwindow_response), 0 );

}

/**
 * a_background_uninit:
 *
 * Uninitialize background feature.
 */
void a_background_uninit()
{
  /* wait until all running threads stop */
  stop_all_threads = TRUE;
  g_thread_pool_free ( thread_pool, TRUE, TRUE );
}

void a_background_add_status(VikStatusbar *vs)
{
  statusbars_to_update = g_slist_prepend(statusbars_to_update,vs);
}

void a_background_remove_status(VikStatusbar *vs)
{
  statusbars_to_update = g_slist_remove(statusbars_to_update,vs);
}

