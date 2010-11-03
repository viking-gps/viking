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

#ifndef _VIKING_FILELIST_H
#define _VIKING_FILELIST_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VIK_FILE_LIST_TYPE            (vik_file_list_get_type ())
#define VIK_FILE_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_FILE_LIST_TYPE, VikFileList))
#define VIK_FILE_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_FILE_LIST_TYPE, VikFileListClass))
#define IS_VIK_FILE_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_FILE_LIST_TYPE))
#define IS_VIK_FILE_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_FILE_LIST_TYPE))

typedef struct _VikFileList VikFileList;
typedef struct _VikFileListClass VikFileListClass;

struct _VikFileListClass
{
  GtkVBoxClass vbox_class;
};

GType vik_file_list_get_type ();

GtkWidget *vik_file_list_new ( const gchar *title );
/* result must be freed */
GList *vik_file_list_get_files ( VikFileList *vfl );
void vik_file_list_set_files ( VikFileList *vfl, GList * );

#endif
