/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 * Copyright (C) 2008-2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * UTM multi-zone stuff by Kit Transue <notlostyet@didactek.com>
 * Dynamic map type by Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vikmapslayer.h"
#include "vikmapslayer_compat.h"
#include "vikmaptype.h"

void maps_layer_register_type ( const char *label, guint id, VikMapsLayer_MapType *map_type )
{
    g_assert(id == map_type->uniq_id);
    VikMapType *object = vik_map_type_new_with_id (*map_type, label);
    maps_layer_register_map_source ( VIK_MAP_SOURCE (object) );
}
