/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_MAPNIKLAYER_H
#define _VIKING_MAPNIKLAYER_H

#include "viklayer.h"

G_BEGIN_DECLS

#define VIK_MAPNIK_LAYER_TYPE            (vik_mapnik_layer_get_type ())
#define VIK_MAPNIK_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_MAPNIK_LAYER_TYPE, VikMapnikLayer))
#define VIK_MAPNIK_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_MAPNIK_LAYER_TYPE, VikMapnikLayerClass))
#define IS_VIK_MAPNIK_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_MAPNIK_LAYER_TYPE))
#define IS_VIK_MAPNIK_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_MAPNIK_LAYER_TYPE))

typedef struct _VikMapnikLayerClass VikMapnikLayerClass;

GType vik_mapnik_layer_get_type ();

typedef struct _VikMapnikLayer VikMapnikLayer;

void vik_mapnik_layer_init (void);
void vik_mapnik_layer_post_init (void);
void vik_mapnik_layer_uninit (void);

G_END_DECLS

#endif
