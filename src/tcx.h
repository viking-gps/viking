/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2020, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _VIKING_TCX_H
#define _VIKING_TCX_H

#include "vikaggregatelayer.h"
#include "vikviewport.h"
#include "viktrwlayer.h"

G_BEGIN_DECLS

gboolean a_tcx_read_file ( VikAggregateLayer *val, VikViewport *vvp, FILE *ff, const gchar* filename );

gboolean a_tcx_read_file_into_layer ( VikTrwLayer *vtl, FILE *ff, const gchar* filename );

G_END_DECLS

#endif
