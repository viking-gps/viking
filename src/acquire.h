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

/* global data structure used to expose the progress dialog to the worker thread */
typedef struct {
  GtkWidget *status;
  VikWindow *vw;
  VikLayersPanel *vlp;
  VikViewport *vvp;
  GtkWidget *dialog;
  gboolean ok; /* if OK is false when we exit, we MUST free w */
  gpointer specific_data;
} acq_dialog_widgets_t;

typedef enum { VIK_DATASOURCE_GPSBABEL_DIRECT, VIK_DATASOURCE_SHELL_CMD } vik_datasource_type_t;
typedef enum { VIK_DATASOURCE_CREATENEWLAYER, VIK_DATASOURCE_ADDTOLAYER } vik_datasource_mode_t;

typedef gpointer (*VikDataSourceAddWidgetsFunc) ( GtkWidget *dialog );

/* if VIK_DATASOURCE_GPSBABEL_DIRECT, babelargs and inputfile.
   if VIK_DATASOURCE_SHELL_CMD, shellcmd and inputtype.
   set both to NULL to signal refusal (ie already downloading) */
typedef void (*VikDataSourceGetCmdStringFunc) ( gpointer widgets_data, gchar **babelargs_or_shellcmd, gchar **inputfile_or_inputtype );
typedef void (*VikDataSourceFirstCleanupFunc) ( gpointer widgets_data );
typedef void  (*VikDataSourceProgressFunc)  (gpointer c, gpointer data, acq_dialog_widgets_t *w);
typedef gpointer  (*VikDataSourceAddProgressWidgetsFunc) ( GtkWidget *dialog );
typedef void (*VikDataSourceCleanupFunc) ( gpointer progress_widgets_data );

typedef struct {
  const gchar *layer_title;
  vik_datasource_type_t type;
  vik_datasource_type_t mode;

  VikDataSourceAddWidgetsFunc add_widgets_func; /* NULL if no first dialog */
  VikDataSourceGetCmdStringFunc get_cmd_string_func; /* passed rv from above */
  VikDataSourceFirstCleanupFunc first_cleanup_func; /* frees rv from addwidgets */

  VikDataSourceProgressFunc progress_func;
  VikDataSourceAddProgressWidgetsFunc add_progress_widgets_func;
  VikDataSourceCleanupFunc cleanup_func;
} VikDataSourceInterface;


void a_acquire ( VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp, VikDataSourceInterface *interface );

#endif
