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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vikwebtool.h"

#include <string.h>

#include <glib/gi18n.h>

#include "util.h"

static GObjectClass *parent_class;

static void webtool_finalize ( GObject *gob );

static void webtool_open ( VikExtTool *self, VikWindow *vwindow );

G_DEFINE_ABSTRACT_TYPE (VikWebtool, vik_webtool, VIK_EXT_TOOL_TYPE)

static void vik_webtool_class_init ( VikWebtoolClass *klass )
{
  GObjectClass *object_class;
  VikExtToolClass *ext_tool_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = webtool_finalize;

  parent_class = g_type_class_peek_parent (klass);

  ext_tool_class = VIK_EXT_TOOL_CLASS ( klass );
  ext_tool_class->open = webtool_open;
}

VikWebtool *vik_webtool_new ()
{
  return VIK_WEBTOOL ( g_object_new ( VIK_WEBTOOL_TYPE, NULL ) );
}

static void vik_webtool_init ( VikWebtool *vlp )
{
  // NOTHING
}

static void webtool_finalize ( GObject *gob )
{
  // VikWebtool *w = VIK_WEBTOOL ( gob );
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static void webtool_open ( VikExtTool *self, VikWindow *vwindow )
{
  VikWebtool *vwd = VIK_WEBTOOL ( self );
  gchar *url = vik_webtool_get_url ( vwd, vwindow );
  open_url ( NULL, url );
  g_free ( url );
}

gchar *vik_webtool_get_url ( VikWebtool *self, VikWindow *vwindow )
{
  return VIK_WEBTOOL_GET_CLASS( self )->get_url( self, vwindow );
}
