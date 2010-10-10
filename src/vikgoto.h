/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
