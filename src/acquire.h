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

#ifndef _VIKING_ACQUIRE_H
#define _VIKING_ACQUIRE_H

#include <gtk/gtk.h>

#include "vikwindow.h"
#include "viklayerspanel.h"
#include "vikviewport.h"
#include "babel.h"

typedef struct _VikDataSourceInterface VikDataSourceInterface;

/* global data structure used to expose the progress dialog to the worker thread */
typedef struct {
  GtkWidget *status;
  VikWindow *vw;
  VikLayersPanel *vlp;
  VikViewport *vvp;
  GtkWidget *dialog;
  gboolean ok; /* if OK is false when we exit, we MUST free w */
  VikDataSourceInterface *source_interface;
  gpointer user_data;
} acq_dialog_widgets_t;

/* Direct, URL & Shell types process the results with GPSBabel to create tracks/waypoint */
typedef enum {
  VIK_DATASOURCE_GPSBABEL_DIRECT,
  VIK_DATASOURCE_URL,
  VIK_DATASOURCE_SHELL_CMD,
  VIK_DATASOURCE_INTERNAL
} vik_datasource_type_t;

typedef enum {
  VIK_DATASOURCE_CREATENEWLAYER,
  VIK_DATASOURCE_ADDTOLAYER
} vik_datasource_mode_t;
/* TODO: replace track/layer? */

typedef enum {
  VIK_DATASOURCE_INPUTTYPE_NONE = 0,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
  VIK_DATASOURCE_INPUTTYPE_TRACK,
  VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK
} vik_datasource_inputtype_t;

/* returns pointer to state if OK, otherwise NULL */
typedef gpointer (*VikDataSourceInitFunc) ();

/* returns NULL if OK, otherwise returns an error message. */
typedef gchar *(*VikDataSourceCheckExistenceFunc) ();

/* Create widgets to show in a setup dialog, set up state via user_data */
typedef void (*VikDataSourceAddSetupWidgetsFunc) ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );

/* if VIK_DATASOURCE_GPSBABEL_DIRECT, babelargs and inputfile.
   if VIK_DATASOURCE_SHELL_CMD, shellcmd and inputtype.
   if VIK_DATASOURCE_URL, url and inputtype.
   set both to NULL to signal refusal (ie already downloading) */
typedef void (*VikDataSourceGetCmdStringFunc) ( gpointer user_data, gchar **babelargs_or_shellcmd, gchar **inputfile_or_inputtype );

typedef void (*VikDataSourceGetCmdStringFuncWithInput) ( gpointer user_data, gchar **babelargs_or_shellcmd, gchar **inputfile_or_inputtype, const gchar *input_file_name );
typedef void (*VikDataSourceGetCmdStringFuncWithInputInput) ( gpointer user_data, gchar **babelargs_or_shellcmd, gchar **inputfile_or_inputtype, const gchar *input_file_name, const gchar *input_track_file_name );

/* The actual function to do stuff - must report success/failure */
typedef gboolean (*VikDataSourceProcessFunc)  ( gpointer vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw );

/* */
typedef void  (*VikDataSourceProgressFunc)  (gpointer c, gpointer data, acq_dialog_widgets_t *w);

/* Creates widgets to show in a progress dialog, may set up state via user_data */
typedef void  (*VikDataSourceAddProgressWidgetsFunc) ( GtkWidget *dialog, gpointer user_data );

/* Frees any widgets created for the setup or progress dialogs, any allocated state, etc. */
typedef void (*VikDataSourceCleanupFunc) ( gpointer user_data );

typedef void (*VikDataSourceOffFunc) ( gpointer user_data, gchar **babelargs_or_shellcmd, gchar **inputfile_or_inputtype );;

struct _VikDataSourceInterface {
  const gchar *window_title;
  const gchar *layer_title;
  vik_datasource_type_t type;
  vik_datasource_mode_t mode;
  vik_datasource_inputtype_t inputtype;
  gboolean autoview;
  gboolean keep_dialog_open; /* when done */


  /*** Manual UI Building ***/
  VikDataSourceInitFunc init_func;
  VikDataSourceCheckExistenceFunc check_existence_func;
  VikDataSourceAddSetupWidgetsFunc add_setup_widgets_func;      
  /***                    ***/

  /* or VikDataSourceGetCmdStringFuncWithInput, if inputtype is not NONE */
  VikDataSourceGetCmdStringFunc get_cmd_string_func; 

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
/**********************************/
/**********************************/

/* for sources with no input data */
void a_acquire ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikDataSourceInterface *source_interface );

/* Create a sub menu intended for rightclicking on a TRWLayer. menu called "Filter"
 * returns NULL if no filters */
GtkWidget *a_acquire_trwlayer_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl);

/* Create a sub menu intended for rightclicking on a TRWLayer. menu called "Filter with Track "TRACKNAME"..."
 * returns NULL if no filters or no filter track has been set
 */
GtkWidget *a_acquire_trwlayer_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrwLayer *vtl);

/* Create a sub menu intended for rightclicking on a track. menu called "Filter"
 * returns NULL if no applicable filters */
GtkWidget *a_acquire_track_menu (VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikTrack *tr);

/* sets aplication-wide track to use with filter. references the track. */
void a_acquire_set_filter_track ( VikTrack *tr, const gchar *name );


#endif
