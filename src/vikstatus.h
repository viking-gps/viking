/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _VIKING_STATUS_H
#define _VIKING_STATUS_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "vikviewport.h"

G_BEGIN_DECLS

#define VIK_STATUSBAR_TYPE            (vik_statusbar_get_type ())
#define VIK_STATUSBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_STATUSBAR_TYPE, VikStatusbar))
#define VIK_STATUSBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_STATUSBAR_TYPE, VikStatusbarClass))
#define IS_VIK_STATUSBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_STATUSBAR_TYPE))
#define IS_VIK_STATUSBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_STATUSBAR_TYPE))

typedef struct _VikStatusbar VikStatusbar;
typedef struct _VikStatusbarClass VikStatusbarClass;

struct _VikStatusbarClass
{
  GtkStatusbarClass statusbar_class;

  void (* clicked)  (VikStatusbar *vs,
                     gint          item);
};

GType vik_statusbar_get_type ();


typedef enum {
  VIK_STATUSBAR_TOOL,
  VIK_STATUSBAR_ITEMS,
  VIK_STATUSBAR_ZOOM,
  VIK_STATUSBAR_INFO,
  VIK_STATUSBAR_POSITION,
  VIK_STATUSBAR_LOG,
  VIK_STATUSBAR_NUM_TYPES
} vik_statusbar_type_t;

VikStatusbar *vik_statusbar_new ( guint scale );
void vik_statusbar_set_message ( VikStatusbar *vs, vik_statusbar_type_t field, const gchar *message );
void vik_statusbar_set_drawmode ( VikStatusbar *vs, VikViewportDrawMode dmode );

G_END_DECLS

#endif
