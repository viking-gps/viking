/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2017, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _VIKING_GEOCLUELAYER_H
#define _VIKING_GEOCLUELAYER_H

#include "viklayer.h"
#include "viktrack.h"
#include "viktrwlayer.h"

G_BEGIN_DECLS

#define VIK_GEOCLUE_LAYER_TYPE            (vik_geoclue_layer_get_type ())
#define VIK_GEOCLUE_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_GEOCLUE_LAYER_TYPE, VikGeoclueLayer))
#define VIK_GEOCLUE_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_GEOCLUE_LAYER_TYPE, VikGeoclueLayerClass))
#define IS_VIK_GEOCLUE_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_GEOCLUE_LAYER_TYPE))
#define IS_VIK_GEOCLUE_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_GEOCLUE_LAYER_TYPE))

typedef struct _VikGeoclueLayerClass VikGeoclueLayerClass;
struct _VikGeoclueLayerClass
{
  VikLayerClass vik_layer_class;
};

GType vik_geoclue_layer_get_type ();

typedef struct _VikGeoclueLayer VikGeoclueLayer;

gboolean vik_geoclue_layer_is_empty ( VikGeoclueLayer *vgl );
VikTrwLayer* vik_geoclue_layer_get_trw ( VikGeoclueLayer *vgl );

G_END_DECLS

#endif
