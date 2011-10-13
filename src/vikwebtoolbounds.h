/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _VIKING_WEBTOOL_BOUNDS_H
#define _VIKING_WEBTOOL_BOUNDS_H

#include <glib.h>

#include "vikwebtool.h"

#define VIK_WEBTOOL_BOUNDS_TYPE            (vik_webtool_bounds_get_type ())
#define VIK_WEBTOOL_BOUNDS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_WEBTOOL_BOUNDS_TYPE, VikWebtoolBounds))
#define VIK_WEBTOOL_BOUNDS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_WEBTOOL_BOUNDS_TYPE, VikWebtoolBoundsClass))
#define IS_VIK_WEBTOOL_BOUNDS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_WEBTOOL_BOUNDS_TYPE))
#define IS_VIK_WEBTOOL_BOUNDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_WEBTOOL_BOUNDS_TYPE))
#define VIK_WEBTOOL_BOUNDS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_WEBTOOL_BOUNDS_TYPE, VikWebtoolBoundsClass))


typedef struct _VikWebtoolBounds VikWebtoolBounds;
typedef struct _VikWebtoolBoundsClass VikWebtoolBoundsClass;

struct _VikWebtoolBoundsClass
{
  VikWebtoolClass object_class;
};

GType vik_webtool_bounds_get_type ();

struct _VikWebtoolBounds {
  VikWebtool obj;
};

VikWebtoolBounds* vik_webtool_bounds_new ( );
VikWebtoolBounds* vik_webtool_bounds_new_with_members ( const gchar *label, const gchar *url );

#endif
