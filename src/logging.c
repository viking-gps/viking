/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2020 Rob Norris <rw_norris@hotmail.com>
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
 ***********************************************************
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "logging.h"
#include "globals.h"
#include "ui_util.h"
#include "vikutils.h"
#include "clipboard.h"

#ifdef HAVE_X11_XLIB_H
#include "X11/Xlib.h"
#endif

typedef struct {
	gchar *level;
	gchar *msg;
} msg_t;

static void msg_t_free ( msg_t *mt )
{
	g_free ( mt->level );
	g_free ( mt->msg );
}

static GList *msgs = NULL; // List of msg_t

G_LOCK_DEFINE_STATIC(msg_lock);
// Keep own count rather than using glist functions
//  the idea is to prevent a (potential?) mutual deadlock between threads
//  and num_msgs can be accessed at all times
static guint num_msgs = 0;

// Maintain a list of windows as per background.c
G_LOCK_DEFINE_STATIC(window_list);
// Still only actually updating the statusbar though
static GSList *windows_to_update = NULL;

static void update_status ( VikWindow *vw, gpointer data )
{
	static gchar buf[20];
	if ( num_msgs )
		g_snprintf ( buf, sizeof(buf), "%d", num_msgs );
	else
		g_snprintf ( buf, sizeof(buf), " " );
	vik_window_statusbar_update ( vw, buf, VIK_STATUSBAR_LOG );
}

// NB can be called from any thread
static void log_update ()
{
  G_LOCK(window_list);
  g_slist_foreach ( windows_to_update, (GFunc)update_status, NULL );
  G_UNLOCK(window_list);
}

static gchar *levels[] =
	{ ("G_LOG_FLAG_RECURSION"), // NB Not a log level
	  ("G_LOG_FLAG_FATAL"),     // NB Not a log level
	  N_("Error"),
	  N_("Critical"),
	  N_("Warning"),
	  N_("Message"),
	  N_("Info"),
	  N_("Debug"),
	  NULL
	};

static void log_it ( const gchar *log_domain,
                     GLogLevelFlags log_level,
                     const gchar *message,
                     gpointer user_data )
{
	// Skip debug messages when not in debug mode
	if ( log_level == G_LOG_LEVEL_DEBUG && !vik_debug )
		return;

	// Convert log level into array index for string output
	guint level = 0;
	for ( level = 0; level < 7; level++ ) {
		if ( log_level & (1<<level) )
			break;
	}
	if ( level > G_N_ELEMENTS(levels) )
		level = 0;
	
	// Could consider adding timestamp e.g. HH:MM:SS
	// Always output to the console
	//  especially as at startup/shutdown there are not any display windows available
	g_print ( "** %s [%s]): %s\n", log_domain ? log_domain : "viking", levels[level], message );

	G_LOCK(msg_lock);
	msg_t *mt = g_malloc0 ( sizeof(msg_t) );
	mt->level = g_strdup ( levels[level] );
	mt->msg = g_strdup ( message );
	msgs = g_list_prepend ( msgs, mt );
	num_msgs++;
	G_UNLOCK(msg_lock);

	log_update();
}

#if HAVE_X11_XLIB_H
static int myXErrorHandler(Display *display, XErrorEvent *theEvent)
{
	// No exit on X errors!
	//  mainly to handle out of memory error when requesting large pixbuf from user request
	//  see vikwindow.c::save_image_file ()
	gchar *msg = g_strdup_printf ( _("Ignoring Xlib error: error code %d request code %d\n"),
	                               theEvent->error_code,
	                               theEvent->request_code );
	log_it ( "Xlib", G_LOG_LEVEL_ERROR, msg, NULL );
	g_free ( msg );

	return 0;
}
#endif

/**
 * a_logging_init:
 *
 * Intialization of the log handlers
 */
void a_logging_init ()
{
	// Note that typically most messages we output to g_log() are English only
	//  since they are mainly for diagnostic purposes,
	//  however these are now exposed into the GUI for all users.
	// There is no intention that all messages need to be marked for i18n
	//  (nor then a need for anyone to attempt to translate loads more messages)

	// Simply get all levels (including debug ones)
	//  we will filter the logging ourselves
	(void)g_log_set_handler ( NULL, G_LOG_LEVEL_MASK, log_it, NULL );
	
	if ( vik_debug ) {
		// Also capture various other messages
		(void)g_log_set_handler ( "Gtk", G_LOG_LEVEL_MASK, log_it, NULL );
		(void)g_log_set_handler ( "GLib", G_LOG_LEVEL_MASK, log_it, NULL );
	}

#if HAVE_X11_XLIB_H
	XSetErrorHandler(myXErrorHandler);
#endif
}

/**
 * a_logging_update:
 *
 * An external manual update 
 * Primarily intended for when the main window is created,
 *  thus showing the count of messages that were created during startup.
 */
void a_logging_update ()
{
	log_update ();
}

static gboolean filter_cb ( GtkTreeModel *model, GtkTreeIter *iter, gpointer data )
{
	const gchar *filter = gtk_entry_get_text ( GTK_ENTRY(data) );

	if ( filter == NULL || strlen ( filter ) == 0 )
		return TRUE;

	gchar *filter_case = g_utf8_casefold ( filter, -1 );

	gboolean visible = FALSE;

	gchar *msg;
	gtk_tree_model_get ( model, iter, 1, &msg, -1 );
	if ( msg != NULL ) {
		gchar *msg_case = g_utf8_casefold ( msg, -1 );
		visible |= ( strstr ( msg_case, filter_case ) != NULL );
		g_free ( msg_case );
	}
	g_free ( msg );

	g_free ( filter_case );

	return visible;
}

static void filter_changed_cb ( GtkEntry   *entry,
								GParamSpec *pspec,
								gpointer   data )
{
    gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER(data) );
}

// Dialog is always closed
static void response_cb ( GtkDialog *dialog, gint response_id, gpointer ignored )
{
	switch ( response_id ) {
	case 1:
		// Clear list
		G_LOCK(msg_lock);
		g_list_free_full ( msgs, (GDestroyNotify)msg_t_free ); msgs = NULL;
		num_msgs = 0;
		G_UNLOCK(msg_lock);

		log_update();

		// Delibrate fall through
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CLOSE:
	default:	
		gtk_widget_destroy ( GTK_WIDGET(dialog) );
		break;
	}
}

static void copy_selection ( GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer data )
{
	GString *gstr = (GString*)data;
	gchar* level; gtk_tree_model_get ( model, iter, 0, &level, -1 );
	gchar* msg; gtk_tree_model_get ( model, iter, 1, &msg, -1 );
	g_string_append_printf ( gstr, "%s%c%s\n", level, '\t', msg );
	g_free ( msg );
}

static void copy_selected_cb ( GtkWidget *tree_view )
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(tree_view) );
	GString *gstr = g_string_new ( NULL );
	gtk_tree_selection_selected_foreach ( selection, copy_selection, gstr );
	a_clipboard_copy ( VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, gstr->str, NULL );
	g_string_free ( gstr, TRUE );
}

static gboolean menu_popup_cb ( GtkWidget *tree_view,
                                GdkEventButton *event,
                                gpointer data )
{
	GtkWidget *menu = gtk_menu_new();
	(void)vu_menu_add_item ( GTK_MENU(menu), _("_Copy Data"), GTK_STOCK_COPY, G_CALLBACK(copy_selected_cb), tree_view );
	gtk_widget_show_all ( menu );
	gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
	return TRUE;
}

static gboolean button_pressed_cb ( GtkWidget *tree_view,
                                    GdkEventButton *event,
                                    gpointer data )
{
	// Only on right clicks...
	if ( ! (event->type == GDK_BUTTON_PRESS && event->button == 3) )
		return FALSE;

	// ATM Force a selection...
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(tree_view) );
	if ( gtk_tree_selection_count_selected_rows (selection) <= 1 ) {
		GtkTreePath *path;
		/* Get tree path for row that was clicked */
		if ( gtk_tree_view_get_path_at_pos ( GTK_TREE_VIEW(tree_view),
		                                     (gint)event->x,
		                                     (gint)event->y,
		                                     &path, NULL, NULL, NULL)) {
			gtk_tree_selection_unselect_all ( selection );
			gtk_tree_selection_select_path ( selection, path );
			gtk_tree_path_free ( path );
		}
	}
	return menu_popup_cb ( tree_view, event, data );
}

/**
 *
 * a_logging_show_window:
 *
 * Get the latest log information and display it in a new dialog
 * ATM this dialog does not update if new entries are made in the background
 * 
 */
void a_logging_show_window ()
{
	GtkTreeStore *store = gtk_tree_store_new ( 2, G_TYPE_STRING, G_TYPE_STRING );

	G_LOCK(msg_lock);
    // Could have an option to default which way around these are shown
	// Step backwards through the list
	//for ( GList *iter = g_list_last(msgs); iter != NULL; iter = g_list_previous(iter) ) {

	// As list is prepended, this shows the newest messages first
	for ( GList *iter = g_list_first(msgs); iter != NULL; iter = g_list_next(iter) ) {
		GtkTreeIter t_iter;
		gtk_tree_store_append ( store, &t_iter, NULL );
		msg_t *mt = (msg_t*)iter->data;
		gtk_tree_store_set ( store, &t_iter, 0, mt->level, -1 );
		gtk_tree_store_set ( store, &t_iter, 1, mt->msg, -1 );
	}
	G_UNLOCK(msg_lock);

	GtkWidget *view = gtk_tree_view_new();
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set ( G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	gint column_runner = 0;
	(void)ui_new_column_text ( _("Category"), renderer, view, column_runner++ );
	(void)ui_new_column_text ( _("Messages"), renderer, view, column_runner++ );

	GtkTreeModelFilter *model = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new ( GTK_TREE_MODEL(store), NULL));
	GtkTreeModelSort *sorted = GTK_TREE_MODEL_SORT(gtk_tree_model_sort_new_with_model ( GTK_TREE_MODEL(model) ));

	gtk_tree_view_set_model ( GTK_TREE_VIEW(view), GTK_TREE_MODEL(sorted) );
	gtk_tree_selection_set_mode ( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE );
	gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW(view), TRUE );

	GtkWidget *scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_container_add ( GTK_CONTAINER(scrolledwindow), view );

	g_object_unref ( store );

	g_signal_connect ( view, "popup-menu", G_CALLBACK(menu_popup_cb), NULL );
	g_signal_connect ( view, "button-press-event", G_CALLBACK(button_pressed_cb), NULL );

	GtkWidget *filter_entry = ui_entry_new( NULL, GTK_ENTRY_ICON_SECONDARY );
	g_signal_connect ( filter_entry, "notify::text", G_CALLBACK(filter_changed_cb), model );
	gtk_tree_model_filter_set_visible_func ( model, filter_cb, filter_entry, NULL );

	GtkWidget *filter_label = gtk_label_new ( _("Filter") );
	g_object_set ( filter_label, "has-tooltip", TRUE, NULL );
	g_signal_connect ( filter_label, "query-tooltip", G_CALLBACK(ui_tree_model_number_tooltip_cb), model );

	GtkWidget *filter_box = gtk_hbox_new( FALSE, 10 );
	gtk_box_pack_start (GTK_BOX(filter_box), filter_label, FALSE, TRUE, 10);
	gtk_box_pack_start (GTK_BOX(filter_box), filter_entry, TRUE, TRUE, 10);

	GtkWidget *dialog = gtk_dialog_new_with_buttons ( _("Log"), NULL, 0,
													  GTK_STOCK_CLEAR, 1,
	                                                  GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
	                                                  NULL );

	g_signal_connect ( G_OBJECT(dialog), "response", G_CALLBACK(response_cb), NULL );
	
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX(GTK_DIALOG(dialog)->vbox), filter_box, FALSE, TRUE, 10);

	gtk_window_set_default_size ( GTK_WINDOW(dialog), 600, 500 );

	gtk_widget_show_all ( dialog );

	gtk_dialog_run (GTK_DIALOG (dialog));
}

void a_logging_add_window ( VikWindow *vw )
{
	G_LOCK(window_list);
	windows_to_update = g_slist_prepend ( windows_to_update, vw );
	G_UNLOCK(window_list);
}

void a_logging_remove_window ( VikWindow *vw )
{
	G_LOCK(window_list);
	windows_to_update = g_slist_remove ( windows_to_update, vw );
	G_UNLOCK(window_list);
}
