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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "vikradiogroup.h"

static GObjectClass *parent_class;

static void radio_group_finalize ( GObject *gob );
static void radio_group_class_init ( VikRadioGroupClass *klass );

struct _VikRadioGroup {
  GtkVBox parent;
  GSList *radios;
  guint options_count;
};

GType vik_radio_group_get_type (void)
{
  static GType vrg_type = 0;

  if (!vrg_type)
  {
    static const GTypeInfo vrg_info = 
    {
      sizeof (VikRadioGroupClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) radio_group_class_init, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikRadioGroup),
      0,
      NULL /* instance init */
    };
    vrg_type = g_type_register_static ( GTK_TYPE_VBOX, "VikRadioGroup", &vrg_info, 0 );
  }

  return vrg_type;
}

static void radio_group_class_init ( VikRadioGroupClass *klass )
{
  /* Destructor */
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = radio_group_finalize;

  parent_class = g_type_class_peek_parent (klass);
}

GtkWidget *vik_radio_group_new ( GList *options )
{
  VikRadioGroup *vrg;
  GtkWidget *t;
  gchar *label;
  GList *option = options;

  if ( ! options )
    return NULL;

  vrg = VIK_RADIO_GROUP ( g_object_new ( VIK_RADIO_GROUP_TYPE, NULL ) );

  label = g_list_nth_data(options, 0);
  t = gtk_radio_button_new_with_label ( NULL, gettext(label) );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(t), TRUE );
  gtk_box_pack_start ( GTK_BOX(vrg), t, FALSE, FALSE, 0 );

  vrg->radios = g_slist_append ( NULL, t );
  vrg->options_count = 1;

  while ( ( option = g_list_next(option) ) != NULL )
  {
    label = option->data;
    t = gtk_radio_button_new_with_label_from_widget (
            GTK_RADIO_BUTTON(vrg->radios->data), gettext(label));
    vrg->radios = g_slist_append( vrg->radios, t );
    gtk_box_pack_start ( GTK_BOX(vrg), GTK_WIDGET(t), FALSE, FALSE, 0 );
    vrg->options_count++;
  }

  return GTK_WIDGET(vrg);
}

GtkWidget *vik_radio_group_new_static ( const gchar **options )
{
  VikRadioGroup *vrg;
  GtkWidget *t;

  if ( ! *options )
    return NULL;

  vrg = VIK_RADIO_GROUP ( g_object_new ( VIK_RADIO_GROUP_TYPE, NULL ) );

  t = gtk_radio_button_new_with_label ( NULL, options[0] );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(t), TRUE );
  gtk_box_pack_start ( GTK_BOX(vrg), t, FALSE, FALSE, 0 );

  vrg->radios = g_slist_append ( NULL, t );
  vrg->options_count = 1;

  for ( options++ ; *options ; options++ )
  {
    t = gtk_radio_button_new_with_label_from_widget ( GTK_RADIO_BUTTON(vrg->radios->data), *options );
    vrg->radios = g_slist_append( vrg->radios, t );
    gtk_box_pack_start ( GTK_BOX(vrg), GTK_WIDGET(t), FALSE, FALSE, 0 );
    vrg->options_count++;
  }

  return GTK_WIDGET(vrg);
}


void vik_radio_group_set_selected ( VikRadioGroup *vrg, guint8 i )
{
  if ( i < vrg->options_count )
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(g_slist_nth_data(vrg->radios,i)), TRUE );
}

guint8 vik_radio_group_get_selected ( VikRadioGroup *vrg )
{
  guint8 i = 0;
  GSList *iter = vrg->radios;
  while ( iter )
  {
    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(iter->data) ) )
      return i;
    iter = iter->next;
    i++;
  }
  return 0;
}

static void radio_group_finalize ( GObject *gob )
{
  VikRadioGroup *vrg = VIK_RADIO_GROUP ( gob );
  if ( vrg->radios )
    g_slist_free ( vrg->radios );
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

