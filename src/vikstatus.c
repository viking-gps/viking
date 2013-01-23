/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <glib/gi18n.h>

#include <math.h>

#include "vikstatus.h"

enum
{
  ZOOM_CHANGED,
  LAST_SIGNAL
};

struct _VikStatusbar {
  GtkHBox hbox;
  GtkWidget *status[VIK_STATUSBAR_NUM_TYPES];
  gboolean empty[VIK_STATUSBAR_NUM_TYPES];
};

G_DEFINE_TYPE (VikStatusbar, vik_statusbar, GTK_TYPE_HBOX)

static guint vik_statusbar_signals[LAST_SIGNAL] = { 0 };

static void
selection_done (GtkMenuShell *menushell,
                gpointer      user_data)
{
  VikStatusbar *vs = VIK_STATUSBAR (user_data);

  GtkWidget *aw = gtk_menu_get_active ( GTK_MENU (menushell) );
  gint active = GPOINTER_TO_INT(gtk_object_get_data ( GTK_OBJECT (aw), "position" ));

  gdouble zoom_request = pow (2, active-2 );

  g_signal_emit (G_OBJECT (vs),
                         vik_statusbar_signals[ZOOM_CHANGED], 0,
                         zoom_request);
}

static gint
zoom_popup_handler (GtkWidget *widget)
{
  GtkMenu *menu;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);

  /* The "widget" is the menu that was supplied when 
   * g_signal_connect_swapped() was called.
   */
  menu = GTK_MENU (widget);

  gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 
                  1, gtk_get_current_event_time());
  return TRUE;
}

static GtkWidget *
create_zoom_menu_all_levels ()
{
  GtkWidget *menu = gtk_menu_new ();
  char *itemLabels[] = { "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384", "32768", NULL };

  int i;
  for (i = 0 ; itemLabels[i] != NULL ; i++)
    {
      GtkWidget *item = gtk_menu_item_new_with_label (itemLabels[i]);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
      gtk_object_set_data (GTK_OBJECT (item), "position", GINT_TO_POINTER(i));
    }
  return menu;
}

static void
vik_statusbar_class_init (VikStatusbarClass *klass)
{
  vik_statusbar_signals[ZOOM_CHANGED] =
    g_signal_new ("zoom-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (VikStatusbarClass, zoom_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE,
                  G_TYPE_NONE, 1,
                  G_TYPE_DOUBLE);

  klass->zoom_changed = NULL;
}

static void
vik_statusbar_init (VikStatusbar *vs)
{
  gint i;

  for ( i = 0; i < VIK_STATUSBAR_NUM_TYPES; i++ ) {
    vs->empty[i] = TRUE;
    
    if (i == VIK_STATUSBAR_ZOOM)
      vs->status[i] = gtk_button_new();
    else
    {
      vs->status[i] = gtk_statusbar_new();
      gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs->status[i]), FALSE );
    }
  }

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_TOOL], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_TOOL], 150, -1 );

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_ITEMS], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_ITEMS], 100, -1 );

  GtkWidget *menu = create_zoom_menu_all_levels ();
  g_signal_connect ( G_OBJECT(menu), "selection-done", G_CALLBACK(selection_done), vs);
  g_signal_connect_swapped ( G_OBJECT(vs->status[VIK_STATUSBAR_ZOOM]), "clicked", G_CALLBACK (zoom_popup_handler), menu);
  gtk_button_set_relief ( GTK_BUTTON(vs->status[VIK_STATUSBAR_ZOOM]), GTK_RELIEF_NONE );
  gtk_widget_set_tooltip_text (GTK_WIDGET (vs->status[VIK_STATUSBAR_ZOOM]), _("Current zoom level. Click to select a new one."));
  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_ZOOM], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_ZOOM], 100, -1 );

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_POSITION], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_POSITION], 250, -1 );

  gtk_box_pack_end ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_INFO], TRUE, TRUE, 1);

  // Set minimum overall size
  //  otherwise the individual size_requests above create an implicit overall size,
  //  and so one can't downsize horizontally as much as may be desired when the statusbar is on
  gtk_widget_set_size_request ( GTK_WIDGET(vs), 50, -1 );
}

/**
 * vik_statusbar_new:
 *
 * Creates a new #VikStatusbar widget.
 *
 * Return value: the new #VikStatusbar widget.
 **/
VikStatusbar *
vik_statusbar_new ()
{
  VikStatusbar *vs = VIK_STATUSBAR ( g_object_new ( VIK_STATUSBAR_TYPE, NULL ) );

  return vs;
}

/**
 * vik_statusbar_set_message:
 * @vs: the #VikStatusbar itself
 * @field: the field to update
 * @message: the message to use
 *
 * Update the message of the given field.
 **/
void
vik_statusbar_set_message ( VikStatusbar *vs, vik_statusbar_type_t field, const gchar *message )
{
  if ( field >= 0 && field < VIK_STATUSBAR_NUM_TYPES )
  {
    if ( field == VIK_STATUSBAR_ZOOM )
    {
      gtk_button_set_label ( GTK_BUTTON(vs->status[field]), message);
    }
    else
    {
    GtkStatusbar *gsb = GTK_STATUSBAR(vs->status[field]);

    if ( !vs->empty[field] )
      gtk_statusbar_pop ( gsb, 0 );
    else
      vs->empty[field] = FALSE;

    gtk_statusbar_push ( gsb, 0, message );
    }
  }
}
