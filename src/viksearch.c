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
 * Created by Quy Tonthat <qtonthat@gmail.com>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "util.h"
#include "curl_download.h"

#include "viksearchtool.h"

static gchar *last_search_str = NULL;
static VikCoord *last_coord = NULL;
static gchar *last_successful_search_str = NULL;

static GList *search_tools_list = NULL;

int last_search_tool = -1;

void vik_search_register ( VikSearchTool *tool )
{
  IS_VIK_SEARCH_TOOL( tool );

  search_tools_list = g_list_append ( search_tools_list, g_object_ref ( tool ) );
}

void vik_search_unregister_all ()
{
  g_list_foreach ( search_tools_list, (GFunc) g_object_unref, NULL );
}

gchar * a_vik_search_get_search_string_for_this_place(VikWindow *vw)
{
  if (!last_coord)
    return NULL;

  VikViewport *vvp = vik_window_viewport(vw);
  const VikCoord *cur_center = vik_viewport_get_center(vvp);
  if (vik_coord_equals(cur_center, last_coord)) {
    return(last_successful_search_str);
  }
  else
    return NULL;
}

static void display_no_tool(VikWindow *vw)
{
  GtkWidget *dialog = NULL;

  dialog = gtk_message_dialog_new ( GTK_WINDOW(vw), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, _("No search tool available.") );

  gtk_dialog_run ( GTK_DIALOG(dialog) );

  gtk_widget_destroy(dialog);
}

static gboolean prompt_try_again(VikWindow *vw)
{
  GtkWidget *dialog = NULL;
  gboolean ret = TRUE;

  dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_window_set_title(GTK_WINDOW(dialog), _("Search"));

  GtkWidget *search_label = gtk_label_new(_("I don't know that place. Do you want another search?"));
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_label, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT )
    ret = FALSE;

  gtk_widget_destroy(dialog);
  return ret;
}

static gchar *  a_prompt_for_search_string(VikWindow *vw)
{
  GtkWidget *dialog = NULL;

  dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_window_set_title(GTK_WINDOW(dialog), _("Search"));

  GtkWidget *tool_label = gtk_label_new(_("Search provider:"));
  GtkWidget *tool_list = gtk_combo_box_new_text ();

  GList *current = g_list_first (search_tools_list);
  while (current != NULL)
  {
    char *label = NULL;
    VikSearchTool *tool = current->data;
    label = vik_search_tool_get_label (tool);
    gtk_combo_box_append_text ( GTK_COMBO_BOX( tool_list ), label);
    current = g_list_next (current);
  }
  /* Set the first provider as default */
  gtk_combo_box_set_active ( GTK_COMBO_BOX( tool_list ), 0);

  GtkWidget *search_label = gtk_label_new(_("Enter address or place name:"));
  GtkWidget *search_entry = gtk_entry_new();
  if (last_search_str)
    gtk_entry_set_text(GTK_ENTRY(search_entry), last_search_str);

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), tool_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), tool_list, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_entry, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
    gtk_widget_destroy(dialog);
    return NULL;
  }
  
  last_search_tool = gtk_combo_box_get_active ( GTK_COMBO_BOX (tool_list) );

  gchar *search_str = g_strdup ( gtk_entry_get_text ( GTK_ENTRY(search_entry) ) );

  gtk_widget_destroy(dialog);

  if (search_str[0] != '\0') {
    if (last_search_str)
      g_free(last_search_str);
    last_search_str = g_strdup(search_str);
  }

  return(search_str);   /* search_str needs to be freed by caller */
}

void a_vik_search(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp)
{
  VikCoord new_center;
  gchar *s_str;
  gboolean more = TRUE;

  if (search_tools_list == NULL)
  {
    /* Empty list */
    display_no_tool(vw);
    return;
  }

  do {
    s_str = a_prompt_for_search_string(vw);
    if ((!s_str) || (s_str[0] == 0)) {
      more = FALSE;
    }

    else if (!vik_search_tool_get_coord(g_list_nth_data (search_tools_list, last_search_tool), vw, vvp, s_str, &new_center)) {
      vik_viewport_set_center_coord(vvp, &new_center);
      vik_layers_panel_emit_update(vlp);
      more = FALSE;
    }
    else if (!prompt_try_again(vw))
        more = FALSE;
    g_free(s_str);
  } while (more);
}
