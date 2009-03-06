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

#ifndef _SLIPPY_MAP_SOURCE_ABSTRACT_H
#define _SLIPPY_MAP_SOURCE_ABSTRACT_H

#include "vikcoord.h"
#include "mapcoord.h"
#include "vikmapsourcedefault.h"
#include "download.h"

G_BEGIN_DECLS

#define SLIPPY_TYPE_MAP_SOURCE_ABSTRACT             (slippy_map_source_abstract_get_type ())
#define SLIPPY_MAP_SOURCE_ABSTRACT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SLIPPY_TYPE_MAP_SOURCE_ABSTRACT, SlippyMapSourceAbstract))
#define SLIPPY_MAP_SOURCE_ABSTRACT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SLIPPY_TYPE_MAP_SOURCE_ABSTRACT, SlippyMapSourceAbstractClass))
#define SLIPPY_IS_MAP_SOURCE_ABSTRACT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SLIPPY_TYPE_MAP_SOURCE_ABSTRACT))
#define SLIPPY_IS_MAP_SOURCE_ABSTRACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SLIPPY_TYPE_MAP_SOURCE_ABSTRACT))
#define SLIPPY_MAP_SOURCE_ABSTRACT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SLIPPY_TYPE_MAP_SOURCE_ABSTRACT, SlippyMapSourceAbstractClass))

typedef struct _SlippyMapSourceAbstractClass SlippyMapSourceAbstractClass;
typedef struct _SlippyMapSourceAbstract SlippyMapSourceAbstract;

struct _SlippyMapSourceAbstractClass
{
	VikMapSourceDefaultClass parent_class;

	gchar * (*get_uri) ( SlippyMapSourceAbstract *self, MapCoord *src );
	gchar * (*get_hostname) ( SlippyMapSourceAbstract *self );
	DownloadOptions * (*get_download_options) ( SlippyMapSourceAbstract *self );
};

struct _SlippyMapSourceAbstract
{
	VikMapSourceDefault parent_instance;
};

GType slippy_map_source_abstract_get_type (void) G_GNUC_CONST;

gchar * slippy_map_source_abstract_get_uri( SlippyMapSourceAbstract *self, MapCoord *src );
gchar * slippy_map_source_abstract_get_hostname( SlippyMapSourceAbstract *self );
DownloadOptions * slippy_map_source_abstract_get_download_options( SlippyMapSourceAbstract *self );

G_END_DECLS

#endif /* _SLIPPY_MAP_SOURCE_ABSTRACT_H_ */
