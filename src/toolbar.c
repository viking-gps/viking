/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *      toolbar.c - this file was part of Geany (v1.24.1), a fast and lightweight IDE
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

/**
 * @file toolbar.h
 * Toolbar (prefs).
 */
/* Utility functions to create the toolbar */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "toolbar.h"
#include "dir.h"
#include "ui_util.h"
#include "util.h"
#include <string.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include "preferences.h"


struct _VikToolbarClass
{
  GObjectClass object_class;
};

struct _VikToolbar {
  GObject obj;
  GtkWidget *widget;
  GtkUIManager *uim;
  guint merge_id;
  GtkActionGroup *group_actions;
  GtkActionGroup *group_toggles;
  GtkActionGroup *group_tools;
  GtkActionGroup *group_modes;
  GSList *list_of_actions;
  GSList *list_of_toggles;
  GSList *list_of_tools;
  GSList *list_of_modes;
};

G_DEFINE_TYPE (VikToolbar, vik_toolbar, G_TYPE_OBJECT)

static void vik_toolbar_class_init (VikToolbarClass *klass)
{
}

static void vik_toolbar_init (VikToolbar *vtb)
{
	vtb->widget = NULL;
	vtb->merge_id = 0;
	vtb->list_of_actions = NULL;
	vtb->list_of_toggles = NULL;
	vtb->list_of_tools = NULL;
	vtb->list_of_modes = NULL;
}

VikToolbar *vik_toolbar_new ()
{
	VikToolbar *vtb = (VikToolbar *)g_object_new(vik_toolbar_get_type(), NULL);
	return vtb;
}

#define TOOLBAR_PARAMS_GROUP_KEY "toolbar"
#define TOOLBAR_PARAMS_NAMESPACE "toolbar."

static gchar *params_icon_size[] = { N_("System Default"), N_("Small"), N_("Medium"), N_("Large"), NULL };
static gchar *params_icon_style[] = { N_("System Default"), N_("Icons Only"), N_("Text Only"), N_("Icons and Text"), NULL };

typedef struct {
	VikToolbar *vtb;
	GtkWindow *parent;
	GtkWidget *vbox;
	GtkWidget *hbox;
	ReloadCB *reload_cb;
	gpointer user_data;
} config_t;

static config_t extra_widget_data;

static VikLayerParam prefs[] = {
	{ VIK_LAYER_NUM_TYPES, TOOLBAR_PARAMS_NAMESPACE "append_to_menu", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Append to Menu:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
	  N_("Pack the toolbar to the main menu to save vertical space"), NULL, NULL, NULL },
	{ VIK_LAYER_NUM_TYPES, TOOLBAR_PARAMS_NAMESPACE "icon_size", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Icon Size:"), VIK_LAYER_WIDGET_COMBOBOX, params_icon_size, NULL,
	  NULL, NULL, NULL, NULL },
	{ VIK_LAYER_NUM_TYPES, TOOLBAR_PARAMS_NAMESPACE "icon_style", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Icon Style:"), VIK_LAYER_WIDGET_COMBOBOX, params_icon_style, NULL,
	  NULL, NULL, NULL, NULL },
	{ VIK_LAYER_NUM_TYPES, TOOLBAR_PARAMS_NAMESPACE "NOTSAVED1", VIK_LAYER_PARAM_PTR, VIK_LAYER_GROUP_NONE, N_("Customize:"), VIK_LAYER_WIDGET_BUTTON, N_("Customize Buttons"), NULL,
	  NULL, NULL, NULL, NULL },
};

// Global storage to enable freeing upon closure
static GHashTable *signal_data;
static GSList *toggle_overrides = NULL;

// Forward declaration
void toolbar_configure (VikToolbar *vtb, GtkWidget *toolbar, GtkWindow *parent, GtkWidget *vbox, GtkWidget *hbox, ReloadCB reload_cb, gpointer user_data);

void toolbar_configure_cb(void)
{
	// Values not known at prefs initialization.
	// So trying to pass these values via the UI builder is not possible currently.
	// ATM cheat via internal values - although this doesn't work properly for multiple Windows...
	toolbar_configure ( extra_widget_data.vtb,
	                    extra_widget_data.vtb->widget,
	                    extra_widget_data.parent,
	                    extra_widget_data.vbox,
	                    extra_widget_data.hbox,
	                    extra_widget_data.reload_cb,
	                    extra_widget_data.user_data );
}

/**
 * a_toolbar_init:
 *
 * Initialize stuff for the toolbar.
 */
void a_toolbar_init (void)
{
	// Preferences
	a_preferences_register_group ( TOOLBAR_PARAMS_GROUP_KEY, _("Toolbar") );

	guint i = 0;
	VikLayerParamData tmp;
	tmp.b = FALSE;
	a_preferences_register (&prefs[i++], tmp, TOOLBAR_PARAMS_GROUP_KEY);
	tmp.u = 0;
	a_preferences_register (&prefs[i++], tmp, TOOLBAR_PARAMS_GROUP_KEY);
#ifdef WINDOWS
	tmp.u = 1; // Small Icons for Windows by default as 'System Defaults' is more GNOME Theme driven.
#else
	tmp.u = 0;
#endif
	a_preferences_register (&prefs[i++], tmp, TOOLBAR_PARAMS_GROUP_KEY);
	tmp.ptr = toolbar_configure_cb;
	a_preferences_register (&prefs[i++], tmp, TOOLBAR_PARAMS_GROUP_KEY);

	// Signal data hash
	signal_data = g_hash_table_new_full ( g_direct_hash, g_direct_equal, NULL, g_free );
}

/**
 * a_toolbar_uninit:
 *
 * Uninitialize toolbar related stuff.
 */
void a_toolbar_uninit ( void )
{
	g_hash_table_destroy ( signal_data );
	g_slist_foreach ( toggle_overrides, (GFunc)g_free, NULL );
	g_slist_free ( toggle_overrides );
}

static gboolean prefs_get_append_to_menu (void)
{
  return a_preferences_get(TOOLBAR_PARAMS_NAMESPACE "append_to_menu")->b;
}

static guint prefs_get_icon_size (void)
{
  return a_preferences_get(TOOLBAR_PARAMS_NAMESPACE "icon_size")->u;
}

static guint prefs_get_icon_style (void)
{
  return a_preferences_get(TOOLBAR_PARAMS_NAMESPACE "icon_style")->u;
}

/* Note: The returned widget pointer is only valid until the toolbar is reloaded. So, either
 * update the widget pointer in this case (i.e. request it again) or better use
 * toolbar_get_action_by_name() instead. The action objects will remain the same even when the
 * toolbar is reloaded. */
GtkWidget *toolbar_get_widget_by_name(VikToolbar *vtb, const gchar *name)
{
	GtkWidget *widget;
	gchar *path;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(VIK_IS_TOOLBAR(vtb), NULL);

	path = g_strconcat("/ui/MainToolbar/", name, NULL);
	widget = gtk_ui_manager_get_widget(vtb->uim, path);

	g_free(path);
	return widget;
}

static GtkAction *get_action ( VikToolbar *vtb, const gchar *name )
{
	// Try all groups
	GtkAction *action = gtk_action_group_get_action (vtb->group_actions, name);
	if ( !action )
		action = gtk_action_group_get_action (vtb->group_tools, name);
	if ( !action )
		action = gtk_action_group_get_action (vtb->group_toggles, name);
	if ( !action )
		action = gtk_action_group_get_action (vtb->group_modes, name);
	return action;
}

/**
 * toolbar_get_action_by_name:
 *
 * Find an action in the specified toolbar via the action name
 */
GtkAction *toolbar_get_action_by_name(VikToolbar *vtb, const gchar *name)
{
	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(VIK_IS_TOOLBAR(vtb), NULL);

	return get_action(vtb,name);
}

/**
 * toolbar_action_tool_entry_register:
 *
 * Register a tool button in the specified toolbar
 *  Only one of these tools can be active at a time (hence it is a GtkRadioActionEntry)
 */
void toolbar_action_tool_entry_register(VikToolbar *vtb, GtkRadioActionEntry *action)
{
	g_return_if_fail(VIK_IS_TOOLBAR(vtb));
	g_return_if_fail(action != NULL);
	vtb->list_of_tools = g_slist_append(vtb->list_of_tools, action);
}

/**
 * toolbar_action_mode_entry_register:
 *
 * Register a drawing projection mode button in the specified toolbar
 *  Only one of these modes can be active at a time (hence it is a GtkRadioActionEntry)
 */
void toolbar_action_mode_entry_register(VikToolbar *vtb, GtkRadioActionEntry *action)
{
	g_return_if_fail(VIK_IS_TOOLBAR(vtb));
	g_return_if_fail(action != NULL);
	vtb->list_of_modes = g_slist_append(vtb->list_of_modes, action);
}

/**
 * toolbar_action_toggle_entry_register:
 *
 * Register a toggle button in the specified toolbar with the specified callback
 * Used in preventing circluar callbacks of a toolbar toggle event calling the menu toggle event
 *  (that then calls toolbar callback and so on and so on...)
 * The toggle action must be given a pointer to a function that is used on the callback for toolbar only
 *  (that must offer a way to have a finite call chain!)
 */
void toolbar_action_toggle_entry_register(VikToolbar *vtb, GtkToggleActionEntry *action, gpointer callback)
{
	g_return_if_fail(VIK_IS_TOOLBAR(vtb));
	g_return_if_fail(action != NULL);

	GtkToggleActionEntry *myaction = g_malloc (sizeof (GtkToggleActionEntry) );
	memcpy ( myaction, action, sizeof (GtkToggleActionEntry) );
	// Overwrite with specific callback
	myaction->callback = callback;
	vtb->list_of_toggles = g_slist_append(vtb->list_of_toggles, myaction);

	// Store override so it can be freed upon toolbar destruction
	toggle_overrides = g_slist_append ( toggle_overrides, myaction );
}

/**
 * toolbar_action_entry_register:
 *
 *  Register a standard action button in the specified toolbar
 */
void toolbar_action_entry_register(VikToolbar *vtb, GtkActionEntry *action)
{
	g_return_if_fail(VIK_IS_TOOLBAR(vtb));
	g_return_if_fail(action != NULL);
	vtb->list_of_actions = g_slist_append(vtb->list_of_actions, action);
}

static void configure_cb (GtkWidget *widget, gpointer user_data)
{
	config_t *data = (config_t*)user_data;
	toolbar_configure ( data->vtb, data->vtb->widget, data->parent, data->vbox, data->hbox, data->reload_cb, data->user_data);
}

static gboolean toolbar_popup_menu (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	// Only display menu on right button clicks
	if (event->button == 3) {
		GtkWidget *tmenu;
		tmenu = gtk_menu_new();
		GtkWidget *item = gtk_menu_item_new_with_mnemonic ( _("_Customize") );
		g_signal_connect ( G_OBJECT(item), "activate", G_CALLBACK(configure_cb), user_data );
		gtk_menu_shell_append ( GTK_MENU_SHELL(tmenu), item );
		gtk_menu_popup ( GTK_MENU(tmenu), NULL, NULL, NULL, NULL, event->button, event->time );
		gtk_widget_show_all ( GTK_WIDGET(tmenu) );
		g_object_ref_sink (tmenu);
		return TRUE;
	}
	return FALSE;
}

/* sets the icon style of the toolbar */
static void toolbar_set_icon_style (GtkWidget *toolbar)
{
	gint icon_style = prefs_get_icon_style();

	if (icon_style == 0)
		icon_style = ui_get_gtk_settings_integer("gtk-toolbar-style", GTK_TOOLBAR_ICONS);
	else
		// Adjust to enum GtkToolbarStyle
		icon_style--;

	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), icon_style);
}


/* sets the icon size of the toolbar */
static void toolbar_set_icon_size (GtkWidget *toolbar)
{
	gint icon_size = prefs_get_icon_size();

	if ( icon_size == 0 )
		icon_size = ui_get_gtk_settings_integer("gtk-toolbar-icon-size", GTK_ICON_SIZE_SMALL_TOOLBAR);
	else {
		// Adjust to enum GtkIconSize
		if ( icon_size == 1 )
			icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;
		else if ( icon_size == 2 )
			icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
		else if ( icon_size == 3 )
			icon_size = GTK_ICON_SIZE_DND;
	}

	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), icon_size);
}

/**
 * toolbar_apply_settings:
 * @vbox:   Potential vertical container for the specified toolbar
 * @hbox:   Potential horizontal container for the specified toolbar
 * @Reset:  Specify if the toolbar should be reparented
 *           (when called externally this should always be TRUE)
 *
 * Updates the specified toolbar with current setting values
 */
void toolbar_apply_settings(VikToolbar *vtb,
                            GtkWidget *vbox,
                            GtkWidget *hbox,
                            gboolean reset)
{
	g_return_if_fail(VIK_IS_TOOLBAR(vtb));

	if ( reset ) {
		g_object_ref (vtb->widget); // ensure not deleted when removed
		// Try both places it could be
		if ( gtk_widget_get_parent (vtb->widget) == hbox )
			gtk_container_remove(GTK_CONTAINER(hbox), vtb->widget );
		if ( gtk_widget_get_parent (vtb->widget) == vbox )
			gtk_container_remove(GTK_CONTAINER(vbox), vtb->widget );
	}

	toolbar_set_icon_style(vtb->widget);
	toolbar_set_icon_size(vtb->widget);

	/* add the toolbar again to the main window */
	// Use reorder to ensure toolbar always comes after the menu
	if (prefs_get_append_to_menu())
	{
		if ( hbox ) {
			gtk_box_pack_start(GTK_BOX(hbox), vtb->widget, TRUE, TRUE, 0);
			gtk_box_reorder_child(GTK_BOX(hbox), vtb->widget, 1);
		}
	}
	else
	{
		if ( vbox ) {
			gtk_box_pack_start(GTK_BOX(vbox), vtb->widget, FALSE, TRUE, 0);
			gtk_box_reorder_child(GTK_BOX(vbox), vtb->widget, 1);
		}
	}
}

/**
 * toolbar_get_widget:
 *
 */
GtkWidget* toolbar_get_widget(VikToolbar *vtb)
{
	g_return_val_if_fail(VIK_IS_TOOLBAR(vtb), NULL);
	return vtb->widget;
}

#include "toolbar.xml.h"
static void toolbar_reload ( VikToolbar *vtb,
                             const gchar *markup,
                             GtkWindow *parent,
                             GtkWidget *vbox,
                             GtkWidget *hbox,
                             ReloadCB reload_cb,
                             gpointer user_data )
{
	GError *error = NULL;
	g_debug ( "%s: %d", __FUNCTION__, g_hash_table_size(signal_data) );

	/* Cleanup old toolbar */
	if (vtb->merge_id > 0)
	{
		/* Get rid of it! */
		gtk_widget_destroy(vtb->widget);

		gtk_ui_manager_remove_ui(vtb->uim, vtb->merge_id);
		gtk_ui_manager_ensure_update(vtb->uim);

		g_hash_table_remove ( signal_data, vtb );
	}

	if (markup != NULL)
	{
		vtb->merge_id = gtk_ui_manager_add_ui_from_string(vtb->uim, markup, -1, &error);
	}
	else
	{
		gchar *filename = NULL;
		/* Load the toolbar UI XML file from disk */
		// Consider using a_get_viking_data_path() first
		filename = g_build_filename (a_get_viking_dir(), "ui_toolbar.xml", NULL);
		vtb->merge_id = gtk_ui_manager_add_ui_from_file(vtb->uim, filename, &error);
		g_free(filename);
	}
	if (error != NULL)
	{
		g_debug("UI creation failed, using internal fallback definition. Error message: %s", error->message);
		g_error_free(error);
		error = NULL;

		/* finally load the internally defined markup as fallback */
		vtb->merge_id = gtk_ui_manager_add_ui_from_string(vtb->uim, toolbar_xml, -1, &error);
		if (error) {
			// Abort - this should only happen if you're missing around with the code
			g_error("Internal UI creation failed. Error message: %s", error->message);
		}

	}
	vtb->widget = gtk_ui_manager_get_widget(vtb->uim, "/ui/MainToolbar");

	/* update button states */
	reload_cb ( vtb->group_actions, user_data );

	toolbar_apply_settings(vtb, vbox, hbox, FALSE);

	gtk_widget_show(vtb->widget);

	/* Signals */
	config_t *data = g_malloc(sizeof(config_t));
	data->vtb = vtb;
	data->parent = parent;
	data->vbox = vbox;
	data->hbox = hbox;
	data->reload_cb = reload_cb;
	data->user_data = user_data;

	// Store data in a hash so it can be freed when the toolbar is reconfigured
	g_hash_table_insert (signal_data, vtb, data);

	g_signal_connect(vtb->widget, "button-press-event", G_CALLBACK(toolbar_popup_menu), data);

	/* We don't need to disconnect those signals as this is done automatically when the entry
	 * widgets are destroyed, happens when the toolbar itself is destroyed. */
}

static void toolbar_notify_style_cb(GObject *object, GParamSpec *arg1, gpointer data)
{
	const gchar *arg_name = g_param_spec_get_name(arg1);
	gint value;

	if (prefs_get_icon_style() == 0 && !g_strcmp0(arg_name, "gtk-toolbar-style"))
	{
		value = ui_get_gtk_settings_integer(arg_name, GTK_TOOLBAR_ICONS);
		if ( GTK_IS_TOOLBAR (data) )
			gtk_toolbar_set_style(GTK_TOOLBAR(data), value);
	}
	else if (prefs_get_icon_size() == 0 && !g_strcmp0(arg_name, "gtk-toolbar-size"))
	{
		value = ui_get_gtk_settings_integer(arg_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
		if ( GTK_IS_TOOLBAR (data) )
			gtk_toolbar_set_icon_size(GTK_TOOLBAR(data), value);
	}
}

/**
 * toolbar_init:
 *
 * Initialize the specified toolbar using the given values
 */
void toolbar_init (VikToolbar *vtb,
                   GtkWindow *parent,
                   GtkWidget *vbox,
                   GtkWidget *hbox,
                   ToolCB tool_cb,
                   ReloadCB reload_cb,
                   gpointer user_data)
{
	vtb->uim = gtk_ui_manager_new();

	vtb->group_actions = gtk_action_group_new("MainToolbar");
	gtk_action_group_set_translation_domain(vtb->group_actions, GETTEXT_PACKAGE);
	GtkActionEntry *actions = NULL;
	GSList *gl;
	gint nn = 0;
	foreach_slist(gl, vtb->list_of_actions) {
		GtkActionEntry *action = gl->data;
		actions = g_renew(GtkActionEntry, actions, nn+1);
		actions[nn] = *action;
		nn++;
	}
	gtk_action_group_add_actions(vtb->group_actions, actions, nn, user_data);
	gtk_ui_manager_insert_action_group(vtb->uim, vtb->group_actions, 0);

	vtb->group_toggles = gtk_action_group_new("UIItems");
	gtk_action_group_set_translation_domain(vtb->group_toggles, GETTEXT_PACKAGE);
	GtkToggleActionEntry *toggle_actions = NULL;
	nn = 0;
	foreach_slist(gl, vtb->list_of_toggles) {
		GtkToggleActionEntry *action = gl->data;
		toggle_actions = g_renew(GtkToggleActionEntry, toggle_actions, nn+1);
		toggle_actions[nn] = *action;
		nn++;
	}
	gtk_action_group_add_toggle_actions(vtb->group_toggles, toggle_actions, nn, user_data);
	gtk_ui_manager_insert_action_group(vtb->uim, vtb->group_toggles, 0);

	vtb->group_tools = gtk_action_group_new("ToolItems");
	gtk_action_group_set_translation_domain(vtb->group_tools, GETTEXT_PACKAGE);

	GtkRadioActionEntry *tool_actions = NULL;
	nn = 0;
	foreach_slist(gl, vtb->list_of_tools) {
		GtkRadioActionEntry *action = gl->data;
		tool_actions = g_renew(GtkRadioActionEntry, tool_actions, nn+1);
		tool_actions[nn] = *action;
		tool_actions[nn].value = nn;
		nn++;
	}
	gtk_action_group_add_radio_actions(vtb->group_tools, tool_actions, nn, 0, G_CALLBACK(tool_cb), user_data);
	gtk_ui_manager_insert_action_group(vtb->uim, vtb->group_tools, 0);

	vtb->group_modes = gtk_action_group_new("ModeItems");
	gtk_action_group_set_translation_domain(vtb->group_modes, GETTEXT_PACKAGE);

	GtkRadioActionEntry *mode_actions = NULL;
	nn = 0;
	foreach_slist(gl, vtb->list_of_modes) {
		GtkRadioActionEntry *action = gl->data;
		mode_actions = g_renew(GtkRadioActionEntry, mode_actions, nn+1);
		mode_actions[nn] = *action;
		mode_actions[nn].value = nn;
		nn++;
	}
	gtk_action_group_add_radio_actions(vtb->group_modes, mode_actions, nn, 0, G_CALLBACK(tool_cb), user_data);
	gtk_ui_manager_insert_action_group(vtb->uim, vtb->group_modes, 0);

	toolbar_reload(vtb, NULL, parent, vbox, hbox, reload_cb, user_data);

#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_style_context_add_class(gtk_widget_get_style_context(vtb->widget), "primary-toolbar");
#endif

	GtkSettings *gtk_settings = gtk_widget_get_settings(vtb->widget);
	if (gtk_settings != NULL)
	{
		g_signal_connect(gtk_settings, "notify::gtk-toolbar-style",
			G_CALLBACK(toolbar_notify_style_cb), vtb->widget);
	}

	extra_widget_data.vtb = vtb;
	extra_widget_data.parent = parent;
	extra_widget_data.vbox = vbox;
	extra_widget_data.reload_cb = reload_cb;
	extra_widget_data.user_data = user_data;
}

/**
 * toolbar_action_set_sensitive:
 *
 * Set sensitivity of a particular action
 */
void toolbar_action_set_sensitive (VikToolbar *vtb, const gchar *name, gboolean sensitive)
{
	g_return_if_fail(VIK_IS_TOOLBAR(vtb));
	g_return_if_fail(name != NULL);
	// Try all groups
	GtkAction *action = get_action ( vtb, name );
	if ( action )
		g_object_set ( action, "sensitive", sensitive, NULL);
}

/**
 * vik_toolbar_finalize:
 *
 * Memory cleanups upon toolbar destruction
 */
void vik_toolbar_finalize ( VikToolbar *vtb )
{
	g_hash_table_remove ( signal_data, vtb );

	/* unref'ing the GtkUIManager object will destroy all its widgets unless they were ref'ed */
	g_object_unref(vtb->uim);
	g_object_unref(vtb->group_actions);
	g_object_unref(vtb->group_tools);
	g_object_unref(vtb->group_toggles);
	g_object_unref(vtb->group_modes);

	g_slist_free(vtb->list_of_actions);
	g_slist_free(vtb->list_of_tools);
	g_slist_free(vtb->list_of_toggles);
	g_slist_free(vtb->list_of_modes);
}


#define TB_EDITOR_SEPARATOR _("Separator")
#define TB_EDITOR_SEPARATOR_LABEL _("--- Separator ---")
typedef struct
{
	GtkWidget *dialog;

	GtkTreeView *tree_available;
	GtkTreeView *tree_used;

	GtkListStore *store_available;
	GtkListStore *store_used;

	GtkTreePath *last_drag_path;
	GtkTreeViewDropPosition last_drag_pos;

	GtkWidget *drag_source;

	config_t config;
} TBEditorWidget;

static const GtkTargetEntry tb_editor_dnd_targets[] =
{
	{ "VIKING_TB_EDITOR_ROW", 0, 0 }
};
static const gint tb_editor_dnd_targets_len = G_N_ELEMENTS(tb_editor_dnd_targets);

enum
{
	TB_EDITOR_COL_ACTION,
	TB_EDITOR_COL_LABEL,
	TB_EDITOR_COL_ICON,
	TB_EDITOR_COLS_MAX
};

static void tb_editor_handler_start_element(GMarkupParseContext *context, const gchar *element_name,
											const gchar **attribute_names,
											const gchar **attribute_values, gpointer data,
											GError **error)
{
	gint i;
	GSList **actions = data;

	/* This is very basic parsing, stripped down any error checking, requires a valid UI markup. */
	if (!g_strcmp0(element_name, "separator"))
		*actions = g_slist_append(*actions, g_strdup(TB_EDITOR_SEPARATOR));

	for (i = 0; attribute_names[i] != NULL; i++)
	{
		if (!g_strcmp0(attribute_names[i], "action"))
		{
			*actions = g_slist_append(*actions, g_strdup(attribute_values[i]));
		}
	}
}


static const GMarkupParser tb_editor_xml_parser =
{
	tb_editor_handler_start_element, NULL, NULL, NULL, NULL
};


static GSList *tb_editor_parse_ui(const gchar *buffer, gssize length, GError **error)
{
	GMarkupParseContext *context;
	GSList *list = NULL;

	context = g_markup_parse_context_new(&tb_editor_xml_parser, 0, &list, NULL);
	g_markup_parse_context_parse(context, buffer, length, error);
	g_markup_parse_context_free(context);

	return list;
}


static void tb_editor_set_item_values(VikToolbar *vtb, const gchar *name, GtkListStore *store, GtkTreeIter *iter)
{
	gchar *icon = NULL;
	gchar *label = NULL;
	gchar *label_clean = NULL;

	// Tries all action groups
	GtkAction *action = get_action ( vtb, name );

	if (action == NULL) {
		if (!g_strcmp0(name, TB_EDITOR_SEPARATOR))
			label_clean = g_strdup(TB_EDITOR_SEPARATOR_LABEL);
		else
			return;
	}
	if (action != NULL) {
		g_object_get(action, "icon-name", &icon, NULL);
		if (icon == NULL)
			g_object_get(action, "stock-id", &icon, NULL);

		g_object_get(action, "label", &label, NULL);
		if (label != NULL)
			label_clean = util_str_remove_chars(g_strdup(label), "_");
	}

	gtk_list_store_set(store, iter,
		TB_EDITOR_COL_ACTION, name,
		TB_EDITOR_COL_LABEL, label_clean,
		TB_EDITOR_COL_ICON, icon,
		-1);

	g_free(icon);
	g_free(label);
	g_free(label_clean);
}


static void tb_editor_scroll_to_iter(GtkTreeView *treeview, GtkTreeIter *iter)
{
	GtkTreePath *path = gtk_tree_model_get_path(gtk_tree_view_get_model(treeview), iter);
	gtk_tree_view_scroll_to_cell(treeview, path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(path);
}


static void tb_editor_free_path(TBEditorWidget *tbw)
{
	if (tbw->last_drag_path != NULL)
	{
		gtk_tree_path_free(tbw->last_drag_path);
		tbw->last_drag_path = NULL;
	}
}


static void tb_editor_btn_remove_clicked_cb(GtkWidget *button, TBEditorWidget *tbw)
{
	GtkTreeModel *model_used;
	GtkTreeSelection *selection_used;
	GtkTreeIter iter_used, iter_new;
	gchar *action_name;

	selection_used = gtk_tree_view_get_selection(tbw->tree_used);
	if (gtk_tree_selection_get_selected(selection_used, &model_used, &iter_used))
	{
		gtk_tree_model_get(model_used, &iter_used, TB_EDITOR_COL_ACTION, &action_name, -1);
		if (gtk_list_store_remove(tbw->store_used, &iter_used))
			gtk_tree_selection_select_iter(selection_used, &iter_used);

		if (g_strcmp0(action_name, TB_EDITOR_SEPARATOR))
		{
			gtk_list_store_append(tbw->store_available, &iter_new);
			tb_editor_set_item_values(tbw->config.vtb, action_name, tbw->store_available, &iter_new);
			tb_editor_scroll_to_iter(tbw->tree_available, &iter_new);
		}

		g_free(action_name);
	}
}


static void tb_editor_btn_add_clicked_cb(GtkWidget *button, TBEditorWidget *tbw)
{
	GtkTreeModel *model_available;
	GtkTreeSelection *selection_available, *selection_used;
	GtkTreeIter iter_available, iter_new, iter_selected;
	gchar *action_name;

	selection_available = gtk_tree_view_get_selection(tbw->tree_available);
	if (gtk_tree_selection_get_selected(selection_available, &model_available, &iter_available))
	{
		gtk_tree_model_get(model_available, &iter_available,
			TB_EDITOR_COL_ACTION, &action_name, -1);
		if (g_strcmp0(action_name, TB_EDITOR_SEPARATOR))
		{
			if (gtk_list_store_remove(tbw->store_available, &iter_available))
				gtk_tree_selection_select_iter(selection_available, &iter_available);
		}

		selection_used = gtk_tree_view_get_selection(tbw->tree_used);
		if (gtk_tree_selection_get_selected(selection_used, NULL, &iter_selected))
			gtk_list_store_insert_before(tbw->store_used, &iter_new, &iter_selected);
		else
			gtk_list_store_append(tbw->store_used, &iter_new);

		tb_editor_set_item_values(tbw->config.vtb, action_name, tbw->store_used, &iter_new);
		tb_editor_scroll_to_iter(tbw->tree_used, &iter_new);

		g_free(action_name);
	}
}


static gboolean tb_editor_drag_motion_cb(GtkWidget *widget, GdkDragContext *drag_context,
                                         gint x, gint y, guint ltime, TBEditorWidget *tbw)
{
	if (tbw->last_drag_path != NULL)
		gtk_tree_path_free(tbw->last_drag_path);
	gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(widget),
		&(tbw->last_drag_path), &(tbw->last_drag_pos));

	return FALSE;
}


static void tb_editor_drag_data_get_cb(GtkWidget *widget, GdkDragContext *context,
                                       GtkSelectionData *data, guint info, guint ltime,
                                       TBEditorWidget *tbw)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GdkAtom atom;
	gchar *name;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, TB_EDITOR_COL_ACTION, &name, -1);
	if (G_UNLIKELY(EMPTY(name)))
	{
		g_free(name);
		return;
	}

	atom = gdk_atom_intern(tb_editor_dnd_targets[0].target, FALSE);
	gtk_selection_data_set(data, atom, 8, (guchar*) name, strlen(name));

	g_free(name);

	tbw->drag_source = widget;
}


static void tb_editor_drag_data_rcvd_cb(GtkWidget *widget, GdkDragContext *context,
										gint x, gint y, GtkSelectionData *data, guint info,
										guint ltime, TBEditorWidget *tbw)
{
	GtkTreeView *tree = GTK_TREE_VIEW(widget);
	gboolean del = FALSE;

	if (gtk_selection_data_get_length(data) >= 0 && gtk_selection_data_get_format(data) == 8)
	{
		gboolean is_sep;
		gchar *text = NULL;

		text = (gchar*) gtk_selection_data_get_data(data);
		is_sep = !g_strcmp0(text, TB_EDITOR_SEPARATOR);
		/* If the source of the action is equal to the target, we do just re-order and so need
		 * to delete the separator to get it moved, not just copied. */
		if (is_sep && widget == tbw->drag_source)
			is_sep = FALSE;

		if (tree != tbw->tree_available || ! is_sep)
		{
			GtkTreeIter iter, iter_before, *iter_before_ptr;
			GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(tree));

			if (tbw->last_drag_path != NULL)
			{
				gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter_before, tbw->last_drag_path);

				if (gtk_list_store_iter_is_valid(store, &iter_before))
					iter_before_ptr = &iter_before;
				else
					iter_before_ptr = NULL;

				if (tbw->last_drag_pos == GTK_TREE_VIEW_DROP_BEFORE ||
					tbw->last_drag_pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)
					gtk_list_store_insert_before(store, &iter, iter_before_ptr);
				else
					gtk_list_store_insert_after(store, &iter, iter_before_ptr);
			}
			else
				gtk_list_store_append(store, &iter);

			tb_editor_set_item_values(tbw->config.vtb, text, store, &iter);
			tb_editor_scroll_to_iter(tree, &iter);
		}
		if (tree != tbw->tree_used || ! is_sep)
			del = TRUE;
	}

	tbw->drag_source = NULL; /* reset the value just to be sure */
	tb_editor_free_path(tbw);
	gtk_drag_finish(context, TRUE, del, ltime);
}


static gboolean tb_editor_foreach_used(GtkTreeModel *model, GtkTreePath *path,
                                       GtkTreeIter *iter, gpointer data)
{
	gchar *action_name;

	gtk_tree_model_get(model, iter, TB_EDITOR_COL_ACTION, &action_name, -1);

	if (!g_strcmp0(action_name, TB_EDITOR_SEPARATOR))
		g_string_append_printf(data, "\t\t<separator/>\n");
	else if (G_LIKELY(!EMPTY(action_name)))
		g_string_append_printf(data, "\t\t<toolitem action='%s' />\n", action_name);

	g_free(action_name);
	return FALSE;
}


static void tb_editor_write_markup(TBEditorWidget *tbw)
{
	/* <ui> must be the first tag, otherwise gtk_ui_manager_add_ui_from_string() will fail. */
	const gchar *template = "<ui>\n<!--\n\
This is Viking's toolbar UI definition.\nThe DTD can be found at \n\
http://library.gnome.org/devel/gtk/stable/GtkUIManager.html#GtkUIManager.description.\n\n\
Generally one should use the toolbar editor in Viking rather than editing this file.\n\n\
For manual changes to this file to take effect, you need to restart Viking.\n-->\n\
\t<toolbar name='MainToolbar'>\n";
	GString *str = g_string_new(template);

	gtk_tree_model_foreach(GTK_TREE_MODEL(tbw->store_used), tb_editor_foreach_used, str);

	g_string_append(str, "\t</toolbar>\n</ui>\n");

	toolbar_reload(tbw->config.vtb,
	               str->str,
	               tbw->config.parent,
	               tbw->config.vbox,
	               tbw->config.hbox,
	               tbw->config.reload_cb,
	               tbw->config.user_data);

	// ATM always save the toolbar when changed
	gchar *filename = g_build_filename(a_get_viking_dir (), "ui_toolbar.xml", NULL);
	GError *error = NULL;
	if (! g_file_set_contents(filename, str->str, -1, &error)) {
		g_warning ("%s: could not write to file %s (%s)", __FUNCTION__, filename, error->message);
		g_error_free(error);
	}
	g_free(filename);

	g_string_free(str, TRUE);
}


static void tb_editor_available_items_changed_cb(GtkTreeModel *model, GtkTreePath *arg1,
												 GtkTreeIter *arg2, TBEditorWidget *tbw)
{
	tb_editor_write_markup(tbw);
}


static void tb_editor_available_items_deleted_cb(GtkTreeModel *model, GtkTreePath *arg1,
												 TBEditorWidget *tbw)
{
	tb_editor_write_markup(tbw);
}


static TBEditorWidget *tb_editor_create_dialog(VikToolbar *vtb, GtkWindow *parent, GtkWidget *toolbar, GtkWidget *vbox, GtkWidget *menu_hbox, ReloadCB reload_cb, gpointer user_data)
{
	GtkWidget *dialog, *hbox, *vbox_buttons, *button_add, *button_remove;
	GtkWidget *swin_available, *swin_used, *tree_available, *tree_used, *label;
	GtkCellRenderer *text_renderer, *icon_renderer;
	GtkTreeViewColumn *column;

	if (parent == NULL) {
		g_warning ( "No parent" );
		return NULL;
	}

	TBEditorWidget *tbw = g_new(TBEditorWidget, 1);

	dialog = gtk_dialog_new_with_buttons(_("Customize Toolbar"),
	                                     GTK_WINDOW(parent),
	                                     GTK_DIALOG_DESTROY_WITH_PARENT,
	                                     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_widget_set_name(dialog, "VikingDialog");
	gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 400);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	tbw->store_available = gtk_list_store_new(TB_EDITOR_COLS_MAX,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	tbw->store_used = gtk_list_store_new(TB_EDITOR_COLS_MAX,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	tbw->config.vtb = vtb;
	tbw->config.parent = parent;
	tbw->config.vbox = vbox;
	tbw->config.hbox = menu_hbox;
	tbw->config.reload_cb = reload_cb;
	tbw->config.user_data = user_data;

	label = gtk_label_new(
		_("Select items to be displayed on the toolbar. Items can be reordered by drag and drop."));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	tree_available = gtk_tree_view_new();
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree_available), GTK_TREE_MODEL(tbw->store_available));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_available), TRUE);
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(tbw->store_available), TB_EDITOR_COL_LABEL, GTK_SORT_ASCENDING);

	icon_renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
		NULL, icon_renderer, "stock-id", TB_EDITOR_COL_ICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_available), column);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Available Items"), text_renderer, "text", TB_EDITOR_COL_LABEL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_available), column);

	swin_available = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin_available),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin_available), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(swin_available), tree_available);

	tree_used = gtk_tree_view_new();
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree_used), GTK_TREE_MODEL(tbw->store_used));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_used), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_used), TRUE);

	icon_renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
		NULL, icon_renderer, "stock-id", TB_EDITOR_COL_ICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_used), column);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Displayed Items"), text_renderer, "text", TB_EDITOR_COL_LABEL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_used), column);

	swin_used = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin_used),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin_used), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(swin_used), tree_used);

	/* drag'n'drop */
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tree_available), GDK_BUTTON1_MASK,
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(tree_available),
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	g_signal_connect(tree_available, "drag-data-get",
		G_CALLBACK(tb_editor_drag_data_get_cb), tbw);
	g_signal_connect(tree_available, "drag-data-received",
		G_CALLBACK(tb_editor_drag_data_rcvd_cb), tbw);
	g_signal_connect(tree_available, "drag-motion",
		G_CALLBACK(tb_editor_drag_motion_cb), tbw);

	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tree_used), GDK_BUTTON1_MASK,
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(tree_used),
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	g_signal_connect(tree_used, "drag-data-get",
		G_CALLBACK(tb_editor_drag_data_get_cb), tbw);
	g_signal_connect(tree_used, "drag-data-received",
		G_CALLBACK(tb_editor_drag_data_rcvd_cb), tbw);
	g_signal_connect(tree_used, "drag-motion",
		G_CALLBACK(tb_editor_drag_motion_cb), tbw);


	button_add = ui_button_new_with_image(GTK_STOCK_GO_FORWARD, NULL);
	button_remove = ui_button_new_with_image(GTK_STOCK_GO_BACK, NULL);
	g_signal_connect(button_add, "clicked", G_CALLBACK(tb_editor_btn_add_clicked_cb), tbw);
	g_signal_connect(button_remove, "clicked", G_CALLBACK(tb_editor_btn_remove_clicked_cb), tbw);

	vbox_buttons = gtk_vbox_new(FALSE, 6);
	/* FIXME this is a little hack'ish, any better ideas? */
	gtk_box_pack_start(GTK_BOX(vbox_buttons), gtk_label_new(""), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_buttons), button_add, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_buttons), button_remove, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_buttons), gtk_label_new(""), TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), swin_available, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox_buttons, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), swin_used, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

	gtk_widget_show_all(dialog);

	g_object_unref(tbw->store_available);
	g_object_unref(tbw->store_used);

	tbw->dialog = dialog;
	tbw->tree_available = GTK_TREE_VIEW(tree_available);
	tbw->tree_used = GTK_TREE_VIEW(tree_used);

	tbw->last_drag_path = NULL;

	return tbw;
}

/**
 * toolbar_configure:
 *
 */
void toolbar_configure (VikToolbar *vtb, GtkWidget *toolbar, GtkWindow *parent, GtkWidget *vbox, GtkWidget *hbox, ReloadCB reload_cb, gpointer user_data)
{
	gchar *markup;
	const gchar *name;
	GSList *sl, *used_items;
	GList *l, *all_items;
	GtkTreeIter iter;
	GtkTreePath *path;
	TBEditorWidget *tbw;

	/* read the current active toolbar items */
	markup = gtk_ui_manager_get_ui(vtb->uim);
	used_items = tb_editor_parse_ui(markup, -1, NULL);
	g_free(markup);

	/* get all available actions */
	all_items = gtk_action_group_list_actions(vtb->group_actions);
	all_items = g_list_concat ( all_items, gtk_action_group_list_actions(vtb->group_toggles) );
	all_items = g_list_concat ( all_items, gtk_action_group_list_actions(vtb->group_tools) );
	all_items = g_list_concat ( all_items, gtk_action_group_list_actions(vtb->group_modes) );

	/* create the GUI */
	tbw = tb_editor_create_dialog(vtb, parent, toolbar, vbox, hbox, reload_cb, user_data);

	/* fill the stores */
	gtk_list_store_insert_with_values(tbw->store_available, NULL, -1,
		TB_EDITOR_COL_ACTION, TB_EDITOR_SEPARATOR,
		TB_EDITOR_COL_LABEL, TB_EDITOR_SEPARATOR_LABEL,
		-1);
	foreach_list(l, all_items)
	{
		name = gtk_action_get_name(l->data);
		if (g_slist_find_custom(used_items, name, (GCompareFunc) strcmp) == NULL)
		{
			gtk_list_store_append(tbw->store_available, &iter);
			tb_editor_set_item_values(vtb, name, tbw->store_available, &iter);
		}
	}
	foreach_slist(sl, used_items)
	{
		gtk_list_store_append(tbw->store_used, &iter);
		tb_editor_set_item_values(vtb, sl->data, tbw->store_used, &iter);
	}
	/* select first item */
	path = gtk_tree_path_new_from_string("0");
	gtk_tree_selection_select_path(gtk_tree_view_get_selection(tbw->tree_used), path);
	gtk_tree_path_free(path);

	/* connect the changed signals after populating the store */
	g_signal_connect(tbw->store_used, "row-changed",
		G_CALLBACK(tb_editor_available_items_changed_cb), tbw);
	g_signal_connect(tbw->store_used, "row-deleted",
		G_CALLBACK(tb_editor_available_items_deleted_cb), tbw);

	/* run it */
	gtk_dialog_run(GTK_DIALOG(tbw->dialog));

	gtk_widget_destroy(tbw->dialog);

	g_slist_foreach(used_items, (GFunc) g_free, NULL);
	g_slist_free(used_items);
	g_list_free(all_items);
	tb_editor_free_path(tbw);
	g_free(tbw);
}
