/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * 
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VIK_MAP_TYPE_H_
#define _VIK_MAP_TYPE_H_

#include <glib-object.h>

#include "vikmapsource.h"
#include "vikmapslayer_compat.h"

G_BEGIN_DECLS

#define VIK_TYPE_MAP_TYPE             (vik_map_type_get_type ())
#define VIK_MAP_TYPE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_MAP_TYPE, VikMapType))
#define VIK_MAP_TYPE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_MAP_TYPE, VikMapTypeClass))
#define VIK_IS_MAP_TYPE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_MAP_TYPE))
#define VIK_IS_MAP_TYPE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_MAP_TYPE))
#define VIK_MAP_TYPE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_MAP_TYPE, VikMapTypeClass))

typedef struct _VikMapTypeClass VikMapTypeClass;
typedef struct _VikMapType VikMapType;

struct _VikMapTypeClass
{
	VikMapSourceClass parent_class;
};

struct _VikMapType
{
	VikMapSource parent_instance;
};

GType vik_map_type_get_type (void) G_GNUC_CONST;
VikMapType *vik_map_type_new_with_id (VikMapsLayer_MapType map_type, const char *label);

G_END_DECLS

#endif /* _VIK_MAP_TYPE_H_ */
