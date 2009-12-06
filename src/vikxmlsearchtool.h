#ifndef __VIK_XML_SEARCH_TOOL_H
#define __VIK_XML_SEARCH_TOOL_H

#include <glib.h>

#include "vikwindow.h"

#include "viksearchtool.h"

#define VIK_XML_SEARCH_TOOL_TYPE            (vik_xml_search_tool_get_type ())
#define VIK_XML_SEARCH_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_XML_SEARCH_TOOL_TYPE, VikXmlSearchTool))
#define VIK_XML_SEARCH_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_XML_SEARCH_TOOL_TYPE, VikXmlSearchToolClass))
#define IS_VIK_XML_SEARCH_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_XML_SEARCH_TOOL_TYPE))
#define IS_VIK_XML_SEARCH_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_XML_SEARCH_TOOL_TYPE))
#define VIK_XML_SEARCH_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_XML_SEARCH_TOOL_TYPE, VikXmlSearchToolClass))


typedef struct _VikXmlSearchTool VikXmlSearchTool;
typedef struct _VikXmlSearchToolClass VikXmlSearchToolClass;

struct _VikXmlSearchToolClass
{
  VikSearchToolClass object_class;
};

GType vik_xml_search_tool_get_type ();

struct _VikXmlSearchTool {
  VikSearchTool obj;
};

VikXmlSearchTool *vik_xml_search_tool_new ();

#endif
