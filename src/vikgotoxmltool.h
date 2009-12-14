#ifndef __VIK_GOTO_XML_TOOL_H
#define __VIK_GOTO_XML_TOOL_H

#include <glib.h>

#include "vikwindow.h"

#include "vikgototool.h"

#define VIK_GOTO_XML_TOOL_TYPE            (vik_goto_xml_tool_get_type ())
#define VIK_GOTO_XML_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_GOTO_XML_TOOL_TYPE, VikGotoXmlTool))
#define VIK_GOTO_XML_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_GOTO_XML_TOOL_TYPE, VikGotoXmlToolClass))
#define IS_VIK_GOTO_XML_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_GOTO_XML_TOOL_TYPE))
#define IS_VIK_GOTO_XML_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_GOTO_XML_TOOL_TYPE))
#define VIK_GOTO_XML_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_GOTO_XML_TOOL_TYPE, VikGotoXmlToolClass))


typedef struct _VikGotoXmlTool VikGotoXmlTool;
typedef struct _VikGotoXmlToolClass VikGotoXmlToolClass;

struct _VikGotoXmlToolClass
{
  VikGotoToolClass object_class;
};

GType vik_goto_xml_tool_get_type ();

struct _VikGotoXmlTool {
  VikGotoTool obj;
};

VikGotoXmlTool *vik_goto_xml_tool_new ();

#endif
