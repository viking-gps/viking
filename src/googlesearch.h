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
