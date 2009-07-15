/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#ifndef _VIKING_EXT_TOOL_H
#define _VIKING_EXT_TOOL_H

#include <glib.h>

#include "vikwindow.h"

#define VIK_EXT_TOOL_TYPE            (vik_ext_tool_get_type ())
#define VIK_EXT_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_EXT_TOOL_TYPE, VikExtTool))
#define VIK_EXT_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_EXT_TOOL_TYPE, VikExtToolClass))
#define IS_VIK_EXT_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_EXT_TOOL_TYPE))
#define IS_VIK_EXT_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_EXT_TOOL_TYPE))
#define VIK_EXT_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_EXT_TOOL_TYPE, VikExtToolClass))


typedef struct _VikExtTool VikExtTool;
typedef struct _VikExtToolClass VikExtToolClass;

struct _VikExtToolClass
{
  GObjectClass object_class;
  gchar *(* get_label) (VikExtTool *self);
  void (* open) (VikExtTool *self, VikWindow *vwindow);
};

GType vik_ext_tool_get_type ();

struct _VikExtTool {
  GObject obj;
};

gchar *vik_ext_tool_get_label ( VikExtTool *self );
void vik_ext_tool_open ( VikExtTool *self, VikWindow *vwindow );

#endif
