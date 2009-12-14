#ifndef __VIK_SEARCH_H
#define __VIK_SEARCH_H

#include "vikwindow.h"
#include "vikviewport.h"
#include "viklayerspanel.h"
#include "vikgototool.h"

void vik_goto_register (VikGotoTool *tool);
void vik_goto_unregister_all (void);

extern void a_vik_goto(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp);
gchar * a_vik_goto_get_search_string_for_this_place(VikWindow *vw);

#endif
