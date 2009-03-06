/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) Guilhem Bonnefille 2009 <guilhem.bonnefille@gmail.com>
 * 
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
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

#ifndef _VIK_SLIPPY_MAP_SOURCE_ABSTRACT_H
#define _VIK_SLIPPY_MAP_SOURCE_ABSTRACT_H

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapsourcedefault.h"
#include "download.h"

G_BEGIN_DECLS

#define VIK_TYPE_SLIPPY_MAP_SOURCE_ABSTRACT             (vik_slippy_map_source_abstract_get_type ())
#define VIK_SLIPPY_MAP_SOURCE_ABSTRACT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_SLIPPY_MAP_SOURCE_ABSTRACT, VikSlippyMapSourceAbstract))
#define VIK_SLIPPY_MAP_SOURCE_ABSTRACT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_SLIPPY_MAP_SOURCE_ABSTRACT, VikSlippyMapSourceAbstractClass))
#define VIK_IS_SLIPPY_MAP_SOURCE_ABSTRACT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_SLIPPY_MAP_SOURCE_ABSTRACT))
#define VIK_IS_SLIPPY_MAP_SOURCE_ABSTRACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_SLIPPY_MAP_SOURCE_ABSTRACT))
#define VIK_SLIPPY_MAP_SOURCE_ABSTRACT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_SLIPPY_MAP_SOURCE_ABSTRACT, VikSlippyMapSourceAbstractClass))

typedef struct _VikSlippyMapSourceAbstractClass VikSlippyMapSourceAbstractClass;
typedef struct _VikSlippyMapSourceAbstract VikSlippyMapSourceAbstract;

struct _VikSlippyMapSourceAbstractClass
{
	VikMapSourceDefaultClass parent_class;

	gchar * (*get_uri) ( VikSlippyMapSourceAbstract *self, MapCoord *src );
	gchar * (*get_hostname) ( VikSlippyMapSourceAbstract *self );
	DownloadOptions * (*get_download_options) ( VikSlippyMapSourceAbstract *self );
};

struct _VikSlippyMapSourceAbstract
{
	VikMapSourceDefault parent_instance;
};

GType vik_slippy_map_source_abstract_get_type (void) G_GNUC_CONST;

gchar * vik_slippy_map_source_abstract_get_uri( VikSlippyMapSourceAbstract *self, MapCoord *src );
gchar * vik_slippy_map_source_abstract_get_hostname( VikSlippyMapSourceAbstract *self );
DownloadOptions * vik_slippy_map_source_abstract_get_download_options( VikSlippyMapSourceAbstract *self );

G_END_DECLS

#endif /* _VIK_SLIPPY_MAP_SOURCE_ABSTRACT_H_ */
