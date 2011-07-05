/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

/* gtk status bars: just plain dumb. this file shouldn't have to exist.
   NB as of gtk 2.18 there are 'info bars' that could be useful... */
#include <gtk/gtk.h>

#include "vikstatus.h"

struct _VikStatusbar {
  GtkHBox hbox;
  GtkWidget *status[VIK_STATUSBAR_NUM_TYPES];
  gboolean empty[VIK_STATUSBAR_NUM_TYPES];
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
    vs_type = g_type_register_static ( GTK_TYPE_HBOX, "VikStatusbar", &vs_info, 0 );
  }

  return vs_type;
}

VikStatusbar *vik_statusbar_new ()
{
  VikStatusbar *vs = VIK_STATUSBAR ( g_object_new ( VIK_STATUSBAR_TYPE, NULL ) );
  gint i;

  for ( i = 0; i < VIK_STATUSBAR_NUM_TYPES; i++ ) {
    vs->empty[i] = TRUE;
    vs->status[i] = gtk_statusbar_new();
    gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs->status[i]), FALSE );
  }

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_TOOL], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_TOOL], 150, -1 );

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_ITEMS], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_ITEMS], 100, -1 );

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_ZOOM], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_ZOOM], 100, -1 );

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_POSITION], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_POSITION], 250, -1 );

  gtk_box_pack_end ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_INFO], TRUE, TRUE, 1);

  // Set minimum overall size
  //  otherwise the individual size_requests above create an implicit overall size,
  //  and so one can't downsize horizontally as much as may be desired when the statusbar is on
  gtk_widget_set_size_request ( GTK_WIDGET(vs), 50, -1 );

  return vs;
}

void vik_statusbar_set_message ( VikStatusbar *vs, vik_statusbar_type_t field, const gchar *message )
{
  if ( field >= 0 && field < VIK_STATUSBAR_NUM_TYPES )
  {
    GtkStatusbar *gsb = GTK_STATUSBAR(vs->status[field]);

    if ( !vs->empty[field] )
      gtk_statusbar_pop ( gsb, 0 );
    else
      vs->empty[field] = FALSE;

    gtk_statusbar_push ( gsb, 0, message );
  }
}
