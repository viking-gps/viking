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

#include "vikexttools.h"

#include <string.h>

#include <glib/gi18n.h>

#define VIK_TOOL_DATA_KEY "vik-tool-data"

static GList *ext_tools_list = NULL;

void vik_ext_tools_register ( VikExtTool *tool )
{
  if ( IS_VIK_EXT_TOOL( tool ) )
    ext_tools_list = g_list_append ( ext_tools_list, g_object_ref ( tool ) );
}

void vik_ext_tools_unregister_all ()
{
  g_list_foreach ( ext_tools_list, (GFunc) g_object_unref, NULL );
}

static void ext_tools_open_cb ( GtkWidget *widget, VikWindow *vwindow )
{
  gpointer ptr = g_object_get_data ( G_OBJECT(widget), VIK_TOOL_DATA_KEY );
  VikExtTool *ext_tool = VIK_EXT_TOOL ( ptr );
  vik_ext_tool_open ( ext_tool, vwindow );
}

void vik_ext_tools_add_action_items ( VikWindow *vwindow, GtkUIManager *uim, GtkActionGroup *action_group, guint mid )
{
  GList *iter;
  for (iter = ext_tools_list; iter; iter = iter->next)
  {
    VikExtTool *ext_tool = NULL;
    gchar *label = NULL;
    ext_tool = VIK_EXT_TOOL ( iter->data );
    label = vik_ext_tool_get_label ( ext_tool );
    if ( label )
    {
      gtk_ui_manager_add_ui ( uim, mid, "/ui/MainMenu/Tools/Exttools",
                              _(label),
                              label,
                              GTK_UI_MANAGER_MENUITEM, FALSE );

      GtkAction *action = gtk_action_new ( label, label, NULL, NULL );
      g_object_set_data ( G_OBJECT(action), VIK_TOOL_DATA_KEY, ext_tool );
      g_signal_connect ( G_OBJECT(action), "activate", G_CALLBACK(ext_tools_open_cb), vwindow );

      gtk_action_group_add_action ( action_group, action );

      g_object_unref ( action );

      g_free ( label ); label = NULL;
    }
  }
}
