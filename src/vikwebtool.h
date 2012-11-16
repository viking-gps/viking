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
#ifndef _VIKING_WEBTOOL_H
#define _VIKING_WEBTOOL_H

#include <glib.h>

#include "vikwindow.h"

#include "vikexttool.h"

G_BEGIN_DECLS

#define VIK_WEBTOOL_TYPE            (vik_webtool_get_type ())
#define VIK_WEBTOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_WEBTOOL_TYPE, VikWebtool))
#define VIK_WEBTOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_WEBTOOL_TYPE, VikWebtoolClass))
#define IS_VIK_WEBTOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_WEBTOOL_TYPE))
#define IS_VIK_WEBTOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_WEBTOOL_TYPE))
#define VIK_WEBTOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_WEBTOOL_TYPE, VikWebtoolClass))


typedef struct _VikWebtool VikWebtool;
typedef struct _VikWebtoolClass VikWebtoolClass;

struct _VikWebtoolClass
{
  VikExtToolClass object_class;
  gchar *(* get_url) (VikWebtool *self, VikWindow *vwindow);
};

GType vik_webtool_get_type ();

struct _VikWebtool {
  VikExtTool obj;
};

gchar *vik_webtool_get_url ( VikWebtool *self, VikWindow *vwindow );

G_END_DECLS

#endif
