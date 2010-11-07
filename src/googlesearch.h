/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Quy Tonthat <qtonthat@gmail.com>
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
 */
#ifndef __VIK_GOOGLEGOTO_H
#define __VIK_GOOGLEGOTO_H

#include <glib.h>

#include "vikwindow.h"

#include "vikgototool.h"

#define GOOGLE_GOTO_TOOL_TYPE            (google_goto_tool_get_type ())
#define GOOGLE_GOTO_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOOGLE_GOTO_TOOL_TYPE, GoogleGotoTool))
#define GOOGLE_GOTO_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GOOGLE_GOTO_TOOL_TYPE, GoogleGotoToolClass))
#define IS_GOOGLE_GOTO_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOOGLE_GOTO_TOOL_TYPE))
#define IS_GOOGLE_GOTO_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GOOGLE_GOTO_TOOL_TYPE))
#define GOOGLE_GOTO_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GOOGLE_GOTO_TOOL_TYPE, GoogleGotoToolClass))


typedef struct _GoogleGotoTool GoogleGotoTool;
typedef struct _GoogleGotoToolClass GoogleGotoToolClass;

struct _GoogleGotoToolClass
{
  VikGotoToolClass object_class;
};

GType google_goto_tool_get_type ();

struct _GoogleGotoTool {
  VikGotoTool obj;
};

GoogleGotoTool *google_goto_tool_new ();

#endif
