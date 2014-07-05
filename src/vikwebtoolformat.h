/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Format Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Format Public License for more details.
 *
 * You should have received a copy of the GNU Format Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _VIKING_WEBTOOL_FORMAT_H
#define _VIKING_WEBTOOL_FORMAT_H

#include <glib.h>

#include "vikwebtool.h"

G_BEGIN_DECLS

#define VIK_WEBTOOL_FORMAT_TYPE            (vik_webtool_format_get_type ())
#define VIK_WEBTOOL_FORMAT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_WEBTOOL_FORMAT_TYPE, VikWebtoolFormat))
#define VIK_WEBTOOL_FORMAT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_WEBTOOL_FORMAT_TYPE, VikWebtoolFormatClass))
#define IS_VIK_WEBTOOL_FORMAT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_WEBTOOL_FORMAT_TYPE))
#define IS_VIK_WEBTOOL_FORMAT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_WEBTOOL_FORMAT_TYPE))
#define VIK_WEBTOOL_FORMAT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_WEBTOOL_FORMAT_TYPE, VikWebtoolFormatClass))

typedef struct _VikWebtoolFormat VikWebtoolFormat;
typedef struct _VikWebtoolFormatClass VikWebtoolFormatClass;

struct _VikWebtoolFormatClass
{
	VikWebtoolClass object_class;
	guint8 (* mpp_to_zoom) (VikWebtool *self, gdouble mpp);
};

GType vik_webtool_format_get_type ();

struct _VikWebtoolFormat {
	VikWebtool obj;
};

guint8 vik_webtool_format_mpp_to_zoom (VikWebtool *self, gdouble mpp);

VikWebtoolFormat* vik_webtool_format_new ( );
VikWebtoolFormat* vik_webtool_format_new_with_members ( const gchar *label,
                                                        const gchar *url,
                                                        const gchar *url_format_code );

G_END_DECLS

#endif
