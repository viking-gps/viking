/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _VIKING_WEBTOOL_DATASOURCE_H
#define _VIKING_WEBTOOL_DATASOURCE_H

#include <glib.h>

#include "vikwebtool.h"

G_BEGIN_DECLS

#define VIK_WEBTOOL_DATASOURCE_TYPE            (vik_webtool_datasource_get_type ())
#define VIK_WEBTOOL_DATASOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_WEBTOOL_DATASOURCE_TYPE, VikWebtoolDatasource))
#define VIK_WEBTOOL_DATASOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_WEBTOOL_DATASOURCE_TYPE, VikWebtoolDatasourceClass))
#define IS_VIK_WEBTOOL_DATASOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_WEBTOOL_DATASOURCE_TYPE))
#define IS_VIK_WEBTOOL_DATASOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_WEBTOOL_DATASOURCE_TYPE))
#define VIK_WEBTOOL_DATASOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_WEBTOOL_DATASOURCE_TYPE, VikWebtoolDatasourceClass))


typedef struct _VikWebtoolDatasource VikWebtoolDatasource;
typedef struct _VikWebtoolDatasourceClass VikWebtoolDatasourceClass;

struct _VikWebtoolDatasourceClass
{
  VikWebtoolClass object_class;
};

GType vik_webtool_datasource_get_type ();

struct _VikWebtoolDatasource {
  VikWebtool obj;
};

VikWebtoolDatasource *vik_webtool_datasource_new ( );
VikWebtoolDatasource *vik_webtool_datasource_new_with_members ( const gchar *label,
                                                                const gchar *url,
                                                                const gchar *url_format_code,
                                                                const gchar *file_type,
                                                                const gchar *babel_filter_args,
                                                                const gchar *input_label);

G_END_DECLS

#endif
