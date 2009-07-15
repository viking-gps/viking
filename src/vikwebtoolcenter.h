/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#ifndef _VIKING_WEBTOOL_CENTER_H
#define _VIKING_WEBTOOL_CENTER_H

#include <glib.h>

#include "vikwebtool.h"

#define VIK_WEBTOOL_CENTER_TYPE            (vik_webtool_center_get_type ())
#define VIK_WEBTOOL_CENTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_WEBTOOL_CENTER_TYPE, VikWebtoolCenter))
#define VIK_WEBTOOL_CENTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_WEBTOOL_CENTER_TYPE, VikWebtoolCenterClass))
#define IS_VIK_WEBTOOL_CENTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_WEBTOOL_CENTER_TYPE))
#define IS_VIK_WEBTOOL_CENTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_WEBTOOL_CENTER_TYPE))
#define VIK_WEBTOOL_CENTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_WEBTOOL_CENTER_TYPE, VikWebtoolCenterClass))


typedef struct _VikWebtoolCenter VikWebtoolCenter;
typedef struct _VikWebtoolCenterClass VikWebtoolCenterClass;

struct _VikWebtoolCenterClass
{
  VikWebtoolClass object_class;
  guint8 (* mpp_to_zoom) (VikWebtool *self, gdouble mpp);
};

GType vik_webtool_center_get_type ();

struct _VikWebtoolCenter {
  VikWebtool obj;
};

guint8 vik_webtool_center_mpp_to_zoom (VikWebtool *self, gdouble mpp);

VikWebtoolCenter* vik_webtool_center_new ( );
VikWebtoolCenter* vik_webtool_center_new_with_members ( const gchar *label, const gchar *url );

#endif
