#ifndef __VIK_SEARCH_H
#define __VIK_SEARCH_H

#include "vikwindow.h"
#include "vikviewport.h"
#include "viklayerspanel.h"
#include "viksearchtool.h"

void vik_search_register (VikSearchTool *tool);
void vik_search_unregister_all (void);

extern void a_vik_search(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp);
gchar * a_vik_search_get_search_string_for_this_place(VikWindow *vw);

#endif
