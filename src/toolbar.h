/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *      toolbar.h - this file was part of Geany (v1.24.1), a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2014 Rob Norris <rw_norris@hotmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef VIKING_TOOLBAR_H
#define VIKING_TOOLBAR_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VIK_TOOLBAR_TYPE             (vik_toolbar_get_type ())
#define VIK_TOOLBAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TOOLBAR_TYPE, VikToolbar))
#define VIK_TOOLBAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TOOLBAR_TYPE, VikToolbarClass))
#define VIK_IS_TOOLBAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TOOLBAR_TYPE))
#define VIK_IS_TOOLBAR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TOOLBAR_TYPE))
#define VIK_TOOLBAR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TOOLBAR_TYPE, VikToolbarClass))

typedef struct _VikToolbarClass VikToolbarClass;
typedef struct _VikToolbar VikToolbar;

GType vik_toolbar_get_type ();

VikToolbar *vik_toolbar_new (void);
void vik_toolbar_finalize ( VikToolbar *vtb );

GtkWidget *toolbar_get_widget_by_name(VikToolbar *vtb, const gchar *name);
GtkAction *toolbar_get_action_by_name(VikToolbar *vtb, const gchar *name);

void toolbar_action_tool_entry_register(VikToolbar *vtb, GtkRadioActionEntry *action);
void toolbar_action_mode_entry_register(VikToolbar *vtb, GtkRadioActionEntry *action);
void toolbar_action_toggle_entry_register(VikToolbar *vtb, GtkToggleActionEntry *action, gpointer callback);
void toolbar_action_entry_register(VikToolbar *vtb, GtkActionEntry *action);

void toolbar_action_set_sensitive (VikToolbar *vtb, const gchar *name, gboolean sensitive);

typedef void (ToolCB) (GtkAction *, GtkAction *, gpointer); // gpointer is actually a VikWindow
typedef void (ReloadCB) (GtkActionGroup *, gpointer); // gpointer is actually a VikWindow

void toolbar_init(VikToolbar *vtb,
                  GtkWindow *parent,
                  GtkWidget *vbox,
                  GtkWidget *hbox,
                  ToolCB tool_cb,
                  ReloadCB reload_cb,
                  gpointer user_data);

void toolbar_apply_settings(VikToolbar *vtb,
                            GtkWidget *vbox,
                            GtkWidget *hbox,
                            gboolean reset);

GtkWidget* toolbar_get_widget(VikToolbar *vtb);

void a_toolbar_init (void);

void a_toolbar_uninit (void);

G_END_DECLS

#endif
