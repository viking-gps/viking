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

#ifndef _VIKING_FILEENTRY_H
#define _VIKING_FILEENTRY_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VIK_FILE_ENTRY_TYPE            (vik_file_entry_get_type ())
#define VIK_FILE_ENTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_FILE_ENTRY_TYPE, VikFileEntry))
#define VIK_FILE_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_FILE_ENTRY_TYPE, VikFileEntryClass))
#define IS_VIK_FILE_ENTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_FILE_ENTRY_TYPE))
#define IS_VIK_FILE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_FILE_ENTRY_TYPE))

typedef struct _VikFileEntry VikFileEntry;
typedef struct _VikFileEntryClass VikFileEntryClass;

struct _VikFileEntryClass
{
  GtkHBoxClass hbox_class;
};

GType vik_file_entry_get_type ();

GtkWidget *vik_file_entry_new (GtkFileChooserAction action);
G_CONST_RETURN gchar *vik_file_entry_get_filename ( VikFileEntry *vfe );
void vik_file_entry_set_filename ( VikFileEntry *vfe, const gchar *filename );

G_END_DECLS

#endif
