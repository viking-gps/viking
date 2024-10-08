/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_ACQUIRE_H
#define _VIKING_ACQUIRE_H

#include <gtk/gtk.h>

#include "vikwindow.h"
#include "viklayerspanel.h"
#include "vikviewport.h"
#include "babel.h"

G_BEGIN_DECLS

typedef struct _VikDataSourceInterface VikDataSourceInterface;

typedef struct {
  VikWindow *vw;
  VikLayersPanel *vlp;
  VikViewport *vvp;
  gpointer userdata;
} acq_vik_t;

/**
 * acq_dialog_widgets_t:
 *
 * global data structure used to expose the progress dialog to the worker thread.
 */
typedef struct {
  GtkWidget *status;
  VikWindow *vw;
  VikLayersPanel *vlp;
  VikViewport *vvp;
  GtkWidget *dialog;
  gboolean running;
  VikDataSourceInterface *source_interface;
  gpointer user_data;
} acq_dialog_widgets_t;

typedef enum {
  VIK_DATASOURCE_CREATENEWLAYER, // Generally Datasources shouldn't use these and let the HCI decide
  VIK_DATASOURCE_ADDTOLAYER,     //    between the create or add to layer options
  VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
  VIK_DATASOURCE_MANUAL_LAYER_MANAGEMENT,
} vik_datasource_mode_t;
/* TODO: replace track/layer? */

typedef enum {
  VIK_DATASOURCE_INPUTTYPE_NONE = 0,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
  VIK_DATASOURCE_INPUTTYPE_TRACK,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK
} vik_datasource_inputtype_t;

/**
 * VikDataSourceInitFunc:
 *
 * Returns: pointer to state if OK, otherwise %NULL
 */
typedef gpointer (*VikDataSourceInitFunc) ( acq_vik_t *avt );

/**
 * VikDataSourceCheckExistenceFunc:
 *
 * Returns: %NULL if OK, otherwise returns an error message.
 */
typedef gchar *(*VikDataSourceCheckExistenceFunc) ();

/**
 * VikDataSourceAddSetupWidgetsFunc:
 *
 * Create widgets to show in a setup dialog, set up state via user_data.
 */
typedef void (*VikDataSourceAddSetupWidgetsFunc) ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );

/**
 * VikDataSourceGetProcessOptionsFunc:
 * @user_data: provided by #VikDataSourceInterface.init_func or dialog with params
 * @process_options: main options controlling the behaviour of #VikDataSourceInterface.process_func
 * @download_options: optional options for downloads from URLs for #VikDataSourceInterface.process_func
 * @input_file_name:
 * @input_track_file_name:
 *
 * set both to %NULL to signal refusal (ie already downloading).
 */
typedef void (*VikDataSourceGetProcessOptionsFunc) ( gpointer user_data, ProcessOptions *process_options, gpointer download_options, const gchar *input_file_name, const gchar *input_track_file_name );

/**
 * VikDataSourceProcessFunc:
 * @vtl:
 * @process_options: options to control the behaviour of this function (see #ProcessOptions)
 * @status_cb: the #VikDataSourceInterface.progress_func
 * @adw: the widgets and data used by #VikDataSourceInterface.progress_func
 * @download_options: Optional options used if downloads from URLs is used.
 *
 * The actual function to do stuff - must report success/failure.
 */
typedef gboolean (*VikDataSourceProcessFunc) ( gpointer vtl, ProcessOptions *process_options, BabelStatusFunc, acq_dialog_widgets_t *adw, gpointer download_options );

/* NB Same as BabelStatusFunc */
typedef void  (*VikDataSourceProgressFunc)  ( BabelProgressCode c, gpointer data, acq_dialog_widgets_t *w );

/**
 * VikDataSourceAddProgressWidgetsFunc:
 *
 * Creates widgets to show in a progress dialog, may set up state via user_data.
 */
typedef void  (*VikDataSourceAddProgressWidgetsFunc) ( GtkWidget *dialog, gpointer user_data );

/**
 * VikDataSourceCleanupFunc:
 *
 * Frees any widgets created for the setup or progress dialogs, any allocated state, etc.
 */
typedef void (*VikDataSourceCleanupFunc) ( gpointer user_data );

typedef void (*VikDataSourceOffFunc) ( gpointer user_data, gchar **babelargs, gchar **file_descriptor );

/**
 * VikDataSourceInterface:
 *
 * Main interface.
 */
struct _VikDataSourceInterface {
  const gchar *window_title;
  const gchar *layer_title;
  vik_datasource_mode_t mode;
  vik_datasource_inputtype_t inputtype;
  gboolean autoview;
  gboolean keep_dialog_open; /* when done */

  gboolean is_thread;

  /*** Manual UI Building ***/
  VikDataSourceInitFunc init_func;
  VikDataSourceCheckExistenceFunc check_existence_func;
  VikDataSourceAddSetupWidgetsFunc add_setup_widgets_func;
  /***                    ***/

  VikDataSourceGetProcessOptionsFunc get_process_options_func;

  VikDataSourceProcessFunc process_func;

  VikDataSourceProgressFunc progress_func;
  VikDataSourceAddProgressWidgetsFunc add_progress_widgets_func;
  VikDataSourceCleanupFunc cleanup_func;
  VikDataSourceOffFunc off_func;

  /*** UI Building        ***/
  VikLayerParam *                   params;
  guint16                           params_count;
  VikLayerParamData *               params_defaults;
  gchar **                          params_groups;
  guint8                            params_groups_count;

};

/**********************************/

void a_acquire ( VikWindow *vw,
                 VikLayersPanel *vlp,
                 VikViewport *vvp,
                 vik_datasource_mode_t mode,
                 VikDataSourceInterface *source_interface,
                 gpointer userdata,
                 VikDataSourceCleanupFunc cleanup_function );

GtkWidget *a_acquire_trwlayer_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl);

GtkWidget *a_acquire_trwlayer_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl);

GtkWidget *a_acquire_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrack *tr);

void a_acquire_set_filter_track ( VikTrack *tr );

G_END_DECLS

#endif
