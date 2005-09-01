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

/* gtk status bars: just plain dumb. this file 
shouldn't have to exist. */
#include <gtk/gtk.h>

#include "vikstatus.h"

#define STATUS_COUNT 5

struct _VikStatusbar {
  GtkStatusbar parent;
  gint num_extra_bars;
  GtkWidget *status[STATUS_COUNT-1];
  gboolean empty[STATUS_COUNT];
};

GType vik_statusbar_get_type (void)
{
  static GType vs_type = 0;

  if (!vs_type)
  {
    static const GTypeInfo vs_info = 
    {
      sizeof (VikStatusbarClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikStatusbar),
      0,
      NULL /* instance init */
    };
    vs_type = g_type_register_static ( GTK_TYPE_STATUSBAR, "VikStatusbar", &vs_info, 0 );
  }

  return vs_type;
}

VikStatusbar *vik_statusbar_new ()
{
  VikStatusbar *vs = VIK_STATUSBAR ( g_object_new ( VIK_STATUSBAR_TYPE, NULL ) );
  gint i;

  gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs), FALSE );

  vs->status[0] = gtk_statusbar_new();
  gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs->status[0]), FALSE );
  gtk_box_pack_start ( GTK_BOX(vs), vs->status[0], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[0], 100, -1 );

  vs->status[1] = gtk_statusbar_new();
  gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs->status[1]), FALSE );
  gtk_box_pack_start ( GTK_BOX(vs), vs->status[1], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[1], 100, -1 );

  vs->status[2] = gtk_statusbar_new();
  gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs->status[2]), FALSE );
  gtk_box_pack_end ( GTK_BOX(vs), vs->status[2], TRUE, TRUE, 1);

  vs->status[3] = gtk_statusbar_new();
  gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs->status[3]), FALSE );
  gtk_box_pack_end ( GTK_BOX(vs), vs->status[3], TRUE, TRUE, 1);

  for ( i = 0; i < STATUS_COUNT; i++ )
    vs->empty[i] = TRUE;

  return vs;
}

void vik_statusbar_set_message ( VikStatusbar *vs, gint field, const gchar *message )
{
  if ( field >= 0 && field < STATUS_COUNT )
  {
    GtkStatusbar *gsb;
    if ( field == 0 )
      gsb = GTK_STATUSBAR(vs);
    else
      gsb = GTK_STATUSBAR(vs->status[field-1]);

    if ( !vs->empty[field] )
      gtk_statusbar_pop ( gsb, 0 );
    else
      vs->empty[field] = FALSE;

    gtk_statusbar_push ( gsb, 0, message );
  }
}
