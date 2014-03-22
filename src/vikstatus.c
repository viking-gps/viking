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
#include "background.h"

enum
{
  CLICKED,
  LAST_SIGNAL
};

struct _VikStatusbar {
  GtkHBox hbox;
  GtkWidget *status[VIK_STATUSBAR_NUM_TYPES];
  gboolean empty[VIK_STATUSBAR_NUM_TYPES];
};

G_DEFINE_TYPE (VikStatusbar, vik_statusbar, GTK_TYPE_HBOX)

static guint vik_statusbar_signals[LAST_SIGNAL] = { 0 };

static gint
forward_signal (GObject *object, gpointer user_data)
{
    gint item = GPOINTER_TO_INT (g_object_get_data ( object, "type" ));
    VikStatusbar *vs = VIK_STATUSBAR (user_data);

    // Clicking on the items field will bring up the background jobs window
    if ( item == VIK_STATUSBAR_ITEMS )
      a_background_show_window();
    else if ( item == VIK_STATUSBAR_INFO )
      // Clear current info message
      vik_statusbar_set_message ( vs, VIK_STATUSBAR_INFO, "" );
    else
      g_signal_emit (G_OBJECT (vs),
                     vik_statusbar_signals[CLICKED], 0,
                     item);

    return TRUE;
}

static void
vik_statusbar_class_init (VikStatusbarClass *klass)
{
  vik_statusbar_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (VikStatusbarClass, clicked),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);

  klass->clicked = NULL;
}

static gboolean button_release_event (GtkWidget* widget, GdkEvent *event, gpointer *user_data)
{
  if ( ((GdkEventButton*)event)->button == 3 ) {
    gint type = GPOINTER_TO_INT (g_object_get_data ( G_OBJECT(widget), "type" ));
    VikStatusbar *vs = VIK_STATUSBAR (user_data);
    // Right Click: so copy the text in the INFO buffer only ATM
    if ( type == VIK_STATUSBAR_INFO ) {
      const gchar* msg = gtk_button_get_label (GTK_BUTTON(vs->status[VIK_STATUSBAR_INFO]));
      if ( msg ) {
        GtkClipboard *clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );
        gtk_clipboard_set_text  ( clipboard, msg, -1 );
      }
    }
    // We've handled the event
    return TRUE;
  }
  // Otherwise carry on with other event handlers - i.e. ensure forward_signal() is called
  return FALSE;
}

static void
vik_statusbar_init (VikStatusbar *vs)
{
  gint i;

  for ( i = 0; i < VIK_STATUSBAR_NUM_TYPES; i++ ) {
    vs->empty[i] = TRUE;
    
    if (i == VIK_STATUSBAR_ITEMS || i == VIK_STATUSBAR_ZOOM || i == VIK_STATUSBAR_INFO )
      vs->status[i] = gtk_button_new();
    else
    {
      vs->status[i] = gtk_statusbar_new();
      gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR(vs->status[i]), FALSE );
    }
    g_object_set_data (G_OBJECT (vs->status[i]), "type", GINT_TO_POINTER(i));
  }

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_TOOL], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_TOOL], 125, -1 );

  g_signal_connect ( G_OBJECT(vs->status[VIK_STATUSBAR_ITEMS]), "clicked", G_CALLBACK (forward_signal), vs);
  gtk_button_set_relief ( GTK_BUTTON(vs->status[VIK_STATUSBAR_ITEMS]), GTK_RELIEF_NONE );
  gtk_widget_set_tooltip_text (GTK_WIDGET (vs->status[VIK_STATUSBAR_ITEMS]), _("Current number of background tasks. Click to see the background jobs."));
  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_ITEMS], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_ITEMS], 100, -1 );

  g_signal_connect ( G_OBJECT(vs->status[VIK_STATUSBAR_ZOOM]), "clicked", G_CALLBACK (forward_signal), vs);
  gtk_button_set_relief ( GTK_BUTTON(vs->status[VIK_STATUSBAR_ZOOM]), GTK_RELIEF_NONE );
  gtk_widget_set_tooltip_text (GTK_WIDGET (vs->status[VIK_STATUSBAR_ZOOM]), _("Current zoom level. Click to select a new one."));
  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_ZOOM], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_ZOOM], 100, -1 );

  gtk_box_pack_start ( GTK_BOX(vs), vs->status[VIK_STATUSBAR_POSITION], FALSE, FALSE, 1);
  gtk_widget_set_size_request ( vs->status[VIK_STATUSBAR_POSITION], 275, -1 );

  g_signal_connect ( G_OBJECT(vs->status[VIK_STATUSBAR_INFO]), "button-release-event", G_CALLBACK (button_release_event), vs);
  g_signal_connect ( G_OBJECT(vs->status[VIK_STATUSBAR_INFO]), "clicked", G_CALLBACK (forward_signal), vs);
  gtk_widget_set_tooltip_text (GTK_WIDGET (vs->status[VIK_STATUSBAR_INFO]), _("Left click to clear the message. Right click to copy the message."));
  gtk_button_set_alignment ( GTK_BUTTON(vs->status[VIK_STATUSBAR_INFO]), 0.0, 0.5 ); // Left align the text
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
    if ( field == VIK_STATUSBAR_ITEMS || field == VIK_STATUSBAR_ZOOM || field == VIK_STATUSBAR_INFO )
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
