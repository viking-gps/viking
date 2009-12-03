#ifndef __VIK_GOOGLESEARCH_H
#define __VIK_GOOGLESEARCH_H

#include <glib.h>

#include "vikwindow.h"

#include "viksearchtool.h"

#define GOOGLE_SEARCH_TOOL_TYPE            (google_search_tool_get_type ())
#define GOOGLE_SEARCH_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOOGLE_SEARCH_TOOL_TYPE, GoogleSearchTool))
#define GOOGLE_SEARCH_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GOOGLE_SEARCH_TOOL_TYPE, GoogleSearchToolClass))
#define IS_GOOGLE_SEARCH_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOOGLE_SEARCH_TOOL_TYPE))
#define IS_GOOGLE_SEARCH_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GOOGLE_SEARCH_TOOL_TYPE))
#define GOOGLE_SEARCH_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GOOGLE_SEARCH_TOOL_TYPE, GoogleSearchToolClass))


typedef struct _GoogleSearchTool GoogleSearchTool;
typedef struct _GoogleSearchToolClass GoogleSearchToolClass;

struct _GoogleSearchToolClass
{
  VikSearchToolClass object_class;
};

GType google_search_tool_get_type ();

struct _GoogleSearchTool {
  VikSearchTool obj;
};

GoogleSearchTool *google_search_tool_new ();

#endif
