/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#ifndef _VIKING_GOTO_TOOL_H
#define _VIKING_GOTO_TOOL_H

#include <glib.h>

#include "vikwindow.h"
#include "download.h"

#define VIK_GOTO_TOOL_TYPE            (vik_goto_tool_get_type ())
#define VIK_GOTO_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_GOTO_TOOL_TYPE, VikGotoTool))
#define VIK_GOTO_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_GOTO_TOOL_TYPE, VikGotoToolClass))
#define IS_VIK_GOTO_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_GOTO_TOOL_TYPE))
#define IS_VIK_GOTO_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_GOTO_TOOL_TYPE))
#define VIK_GOTO_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_GOTO_TOOL_TYPE, VikGotoToolClass))


typedef struct _VikGotoTool VikGotoTool;
typedef struct _VikGotoToolClass VikGotoToolClass;

struct _VikGotoToolClass
{
  GObjectClass object_class;
  gchar *(* get_label) (VikGotoTool *self);
  gchar *(* get_url_format) (VikGotoTool *self);
  DownloadMapOptions *(* get_download_options) (VikGotoTool *self);
  gboolean (* parse_file_for_latlon) (VikGotoTool *self, gchar *filename, struct LatLon *ll);
};

GType vik_goto_tool_get_type ();

struct _VikGotoTool {
  GObject obj;
};

gchar *vik_goto_tool_get_label ( VikGotoTool *self );
gchar *vik_goto_tool_get_url_format ( VikGotoTool *self );
DownloadMapOptions *vik_goto_tool_get_download_options ( VikGotoTool *self );
gboolean vik_goto_tool_parse_file_for_latlon ( VikGotoTool *self, gchar *filename, struct LatLon *ll );
int vik_goto_tool_get_coord ( VikGotoTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord );

#endif
