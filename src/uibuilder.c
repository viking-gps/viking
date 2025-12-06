/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "dialog.h"
#include "fileutils.h"
#include "globals.h"
#include "uibuilder.h"
#include "vikradiogroup.h"
#include "vikfileentry.h"
#include "vikfilelist.h"
#include "vikviewport.h"
#include "ui_util.h"

VikLayerParamData vik_lpd_true_default ( void ) { return VIK_LPD_BOOLEAN ( TRUE ); }
VikLayerParamData vik_lpd_false_default ( void ) { return VIK_LPD_BOOLEAN ( FALSE ); }
static gboolean file_save_cb ( VikFileEntry *vfe, gpointer user_data );


static void refresh_widget ( GtkWidget *widget, VikLayerParam *param, VikLayerParamData data )
{
  // Perform pre conversion if necessary
  VikLayerParamData vlpd = data;
  if ( param->convert_to_display )
    vlpd = param->convert_to_display ( data );

  switch ( param->widget_type )
  {
    case VIK_LAYER_WIDGET_COLOR:
      gtk_color_button_set_color ( GTK_COLOR_BUTTON(widget), &(vlpd.c) );
      break;
    case VIK_LAYER_WIDGET_CHECKBUTTON:
      gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(widget), vlpd.b );
      break;
    case VIK_LAYER_WIDGET_COMBOBOX:
      // NB for double usage the convert_to_display() should have returned an integer to index into the combobox
      if ( param->type == VIK_LAYER_PARAM_UINT || param->type == VIK_LAYER_PARAM_DOUBLE ) {
        gtk_combo_box_set_active ( GTK_COMBO_BOX(widget), 0 ); // In case value not found
        // map of alternate uint values for options
        if ( param->extra_widget_data ) {
          // Set the effective default value
          for ( int i = 0; ((const char **)param->widget_data)[i]; i++ )
            if ( ((guint *)param->extra_widget_data)[i] == vlpd.u ) {
              // Match default value
              gtk_combo_box_set_active ( GTK_COMBO_BOX(widget), i );
              break;
            }
        }
	else {
	  gtk_combo_box_set_active ( GTK_COMBO_BOX(widget), vlpd.u );
	}
      }
      else if ( param->type == VIK_LAYER_PARAM_STRING && param->widget_data && !param->extra_widget_data )
      {
	gtk_combo_box_set_active ( GTK_COMBO_BOX(widget), 0 );
        gchar **pstr = param->widget_data;
        guint ii = 0;
        while ( *pstr ) {
          if ( g_strcmp0(vlpd.s, *pstr) == 0 )
            gtk_combo_box_set_active ( GTK_COMBO_BOX(widget), ii );
          ii++;
          pstr++;
	}
      }
      else if ( param->type == VIK_LAYER_PARAM_STRING && param->widget_data && param->extra_widget_data) {
	gtk_combo_box_set_active ( GTK_COMBO_BOX(widget), 0 );
        if ( vlpd.s ) {
          // Set the effective default value
          // In case of value does not exist, set the first value
          for ( int i = 0; ((const char **)param->widget_data)[i]; i++ )
            if ( strcmp(((const char **)param->extra_widget_data)[i], vlpd.s) == 0 ) {
              gtk_combo_box_set_active ( GTK_COMBO_BOX(widget), i );
              break;
            }
        }
      }
      break;
    case VIK_LAYER_WIDGET_RADIOGROUP:
      // widget_data and extra_widget_data are GList
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data ) {
        if ( param->extra_widget_data ) {
          int nb_elem = g_list_length(param->widget_data);
          for ( int i = 0; i < nb_elem; i++ )
            if ( GPOINTER_TO_UINT ( g_list_nth_data(param->extra_widget_data, i) ) == vlpd.u ) {
              vik_radio_group_set_selected ( VIK_RADIO_GROUP(widget), i );
              break;
            }
        }
        else
          vik_radio_group_set_selected ( VIK_RADIO_GROUP(widget), vlpd.u );
      }
      break;
    case VIK_LAYER_WIDGET_RADIOGROUP_STATIC:
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data ) {
	// map of alternate uint values for options
        if ( param->extra_widget_data ) {
          for ( int i = 0; ((const char **)param->widget_data)[i]; i++ )
            if ( ((guint *)param->extra_widget_data)[i] == vlpd.u ) {
              vik_radio_group_set_selected ( VIK_RADIO_GROUP(widget), i );
              break;
            }
        }
        else
          vik_radio_group_set_selected ( VIK_RADIO_GROUP(widget), vlpd.u );
      }
      break;
    case VIK_LAYER_WIDGET_SPINBUTTON:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE
            || param->type == VIK_LAYER_PARAM_UINT
            || param->type == VIK_LAYER_PARAM_INT) ) {
        gdouble init_val = 0.0;
        if (param->type == VIK_LAYER_PARAM_DOUBLE)
          init_val = vlpd.d;
        else if (param->type == VIK_LAYER_PARAM_UINT)
          init_val = vlpd.u;
        else
          init_val = vlpd.i;
        gtk_spin_button_set_value ( GTK_SPIN_BUTTON(widget), init_val );
      }
    break;
    case VIK_LAYER_WIDGET_ENTRY:
    case VIK_LAYER_WIDGET_ENTRY_URL:
    case VIK_LAYER_WIDGET_PASSWORD:
      ui_entry_set_text ( widget, vlpd.s );
      break;
    case VIK_LAYER_WIDGET_FILEENTRY:
    case VIK_LAYER_WIDGET_FILESAVE:
    case VIK_LAYER_WIDGET_FOLDERENTRY:
      vik_file_entry_set_filename ( VIK_FILE_ENTRY(widget), vlpd.s );
      break;
    case VIK_LAYER_WIDGET_FILELIST:
      if ( param->type == VIK_LAYER_PARAM_STRING_LIST ) {
        vik_file_list_set_files ( VIK_FILE_LIST(widget), vlpd.sl );
      }
      break;

    case VIK_LAYER_WIDGET_HSCALE:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE || param->type == VIK_LAYER_PARAM_UINT || param->type == VIK_LAYER_PARAM_INT) ) {
        gdouble init_val = (param->type == VIK_LAYER_PARAM_DOUBLE) ? vlpd.d : (param->type == VIK_LAYER_PARAM_UINT ? vlpd.u : vlpd.i);
        gtk_range_set_value ( GTK_RANGE(widget), init_val );
      }
      break;

      // Nothing to change
    case VIK_LAYER_WIDGET_BUTTON: break;

    default: break;
  }
}

static GtkWidget *dialog = NULL;
static GtkWidget **RefreshWidgets = NULL;

/** i18n note
 * Since UI builder often uses static structures, the text is marked with N_()
 *  however to actually get it to apply the widget (e.g. to a label) then
 *  an additional call to _() needs to occur on that string
 **/
GtkWidget *new_widget ( VikLayerParam *param, VikLayerParamData data, gboolean show_reset_buttons, gpointer pass_along_getparam )
{
  // Perform pre conversion if necessary
  VikLayerParamData vlpd = data;
  if ( param->convert_to_display )
    vlpd = param->convert_to_display ( data );

  // 1st pass: just create the widget
  GtkWidget *rv = NULL;
  switch ( param->widget_type )
  {
    case VIK_LAYER_WIDGET_COLOR:
      g_return_val_if_fail ( param->type==VIK_LAYER_PARAM_COLOR, NULL );
      rv = gtk_color_button_new_with_color ( &(vlpd.c) );
      break;
    case VIK_LAYER_WIDGET_CHECKBUTTON:
      g_return_val_if_fail ( param->type==VIK_LAYER_PARAM_BOOLEAN, NULL );
      rv = gtk_check_button_new ();
      break;
    case VIK_LAYER_WIDGET_COMBOBOX:
      if ( (param->type == VIK_LAYER_PARAM_UINT && param->widget_data) ||
           (param->type == VIK_LAYER_PARAM_DOUBLE && param->widget_data) )
      {
        /* Build a simple combobox */
        gchar **pstr = param->widget_data;
        rv = vik_combo_box_text_new ();
        while ( *pstr )
          vik_combo_box_text_append ( rv, _(*(pstr++)) );
      }
      else if ( param->type == VIK_LAYER_PARAM_STRING && param->widget_data && !param->extra_widget_data )
      {
        /* Build a combobox with editable text */
        gchar **pstr = param->widget_data;
#if GTK_CHECK_VERSION (2, 24, 0)
        rv = gtk_combo_box_text_new_with_entry ();
#else
        rv = gtk_combo_box_entry_new_text ();
#endif
        while ( *pstr ) {
          vik_combo_box_text_append ( rv, _(*(pstr++)) );
	}
      }
      else if ( param->type == VIK_LAYER_PARAM_STRING && param->widget_data && param->extra_widget_data)
      {
        /* Build a combobox with fixed selections without editable text */
        gchar **pstr = param->widget_data;
        rv = GTK_WIDGET ( vik_combo_box_text_new () );
        while ( *pstr )
          vik_combo_box_text_append ( rv, _(*(pstr++)) );
      }
      break;
    case VIK_LAYER_WIDGET_RADIOGROUP:
      /* widget_data and extra_widget_data are GList */
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data )
        rv = vik_radio_group_new ( param->widget_data );
      break;
    case VIK_LAYER_WIDGET_RADIOGROUP_STATIC:
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data )
      {
        rv = vik_radio_group_new_static ( (const gchar **) param->widget_data );
      }
      break;
    case VIK_LAYER_WIDGET_SPINBUTTON:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE || param->type == VIK_LAYER_PARAM_UINT
           || param->type == VIK_LAYER_PARAM_INT)  && param->widget_data )
      {
        gdouble init_val = 0.0;
        if (param->type == VIK_LAYER_PARAM_DOUBLE)
          init_val = vlpd.d;
        else if (param->type == VIK_LAYER_PARAM_UINT)
          init_val = vlpd.u;
        else
          init_val = vlpd.i;
        VikLayerParamScale *scale = (VikLayerParamScale *) param->widget_data;
        rv = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new( init_val, scale->min, scale->max, scale->step, scale->step, 0 )), scale->step, scale->digits );
      }
    break;
    case VIK_LAYER_WIDGET_ENTRY:
    case VIK_LAYER_WIDGET_ENTRY_URL:
      if ( param->type == VIK_LAYER_PARAM_STRING )
        rv = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
      break;
    case VIK_LAYER_WIDGET_PASSWORD:
      if ( param->type == VIK_LAYER_PARAM_STRING )
      {
        rv = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
        gtk_entry_set_visibility ( GTK_ENTRY(rv), FALSE );
        gtk_widget_set_tooltip_text ( GTK_WIDGET(rv),
                                     _("Take care that this password will be stored clearly in a plain file.") );
      }
      break;
    case VIK_LAYER_WIDGET_FILEENTRY:
      g_return_val_if_fail ( param->type==VIK_LAYER_PARAM_STRING, NULL );
      rv = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, GPOINTER_TO_INT(param->widget_data), NULL, NULL);
      break;
    case VIK_LAYER_WIDGET_FILESAVE:
      g_return_val_if_fail ( param->type==VIK_LAYER_PARAM_STRING, NULL );
      rv = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_SAVE, GPOINTER_TO_INT(param->widget_data), file_save_cb, NULL );
      break;
    case VIK_LAYER_WIDGET_FOLDERENTRY:
      g_return_val_if_fail ( param->type==VIK_LAYER_PARAM_STRING, NULL );
      rv = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, VF_FILTER_NONE, NULL, NULL);
      break;

    case VIK_LAYER_WIDGET_FILELIST:
      g_return_val_if_fail ( param->type==VIK_LAYER_PARAM_STRING_LIST, NULL );
      rv = vik_file_list_new ( _(param->title), NULL, NULL );
      break;
    case VIK_LAYER_WIDGET_HSCALE:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE || param->type == VIK_LAYER_PARAM_UINT
           || param->type == VIK_LAYER_PARAM_INT)  && param->widget_data )
      {
        VikLayerParamScale *scale = (VikLayerParamScale *) param->widget_data;
        rv = gtk_hscale_new_with_range ( scale->min, scale->max, scale->step );
        gtk_scale_set_digits ( GTK_SCALE(rv), scale->digits );
      }
      break;

    case VIK_LAYER_WIDGET_BUTTON:
      if ( (param->type == VIK_LAYER_PARAM_PTR && param->widget_data) ) {
        rv = gtk_button_new_with_label ( _(param->widget_data) );
        g_signal_connect ( G_OBJECT(rv), "clicked", G_CALLBACK (vlpd.ptr), param->extra_widget_data );
      } else if	(param->type == VIK_LAYER_PARAM_PTR_DEFAULT && param->widget_data && show_reset_buttons) {
        if ( param->extra_widget_data ) {
          // For preferences use a button on each tab - using static extra_widget_data
          rv = gtk_button_new_with_label ( _(param->widget_data) );
          g_signal_connect ( G_OBJECT(rv), "clicked", G_CALLBACK (vlpd.ptr), param->extra_widget_data );
        } else {
          // Otherwise put in a runtime value (normally a VikLayer*) for Layer properties defaults
          //  and make this button part of the outer dialog as it will reset all tabs
          GtkWidget *wgt = gtk_dialog_add_button ( GTK_DIALOG(dialog), _(param->widget_data), GTK_RESPONSE_NONE );
          g_signal_connect ( G_OBJECT(wgt), "clicked", G_CALLBACK (vlpd.ptr), pass_along_getparam );
        }
      }
      break;
    case VIK_LAYER_WIDGET_SEPARATOR:
      if ( param->type != VIK_LAYER_PARAM_SPACER )
        g_critical ( "%s: param->type should be VIK_LAYER_PARAM_SPACER but is %d ", __FUNCTION__, param->type );
#if GTK_CHECK_VERSION (3,0,0)
      rv = gtk_separator_new ( GTK_ORIENTATION_HORIZONTAL );
#endif
      break;

    default: break;
  }
  if ( rv ) {
    gchar* tt = gtk_widget_get_tooltip_text ( rv );
    if ( !tt ) {
      if ( param->tooltip )
        gtk_widget_set_tooltip_text ( rv, _(param->tooltip) );
    } else
      g_free ( tt );
  }

  // 2nd pass: set the widget value
  if ( rv )
    refresh_widget ( rv, param, data );

  return rv;
}

VikLayerParamData a_uibuilder_widget_get_value ( GtkWidget *widget, VikLayerParam *param )
{
  VikLayerParamData rv;
  switch ( param->widget_type )
  {
    case VIK_LAYER_WIDGET_COLOR:
      gtk_color_button_get_color ( GTK_COLOR_BUTTON(widget), &(rv.c) );
      break;
    case VIK_LAYER_WIDGET_CHECKBUTTON:
      rv.b = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
      break;
    case VIK_LAYER_WIDGET_COMBOBOX:
      // NB for a double the subsequent convert_to_internal() should convert
      //  from the passed in integer index back to the double value
      if ( param->type == VIK_LAYER_PARAM_UINT ||
           param->type == VIK_LAYER_PARAM_DOUBLE )
      {
        rv.i = gtk_combo_box_get_active ( GTK_COMBO_BOX(widget) );
        if ( rv.i == -1 ) rv.i = 0;
        rv.u = rv.i;
        if ( param->extra_widget_data )
          rv.u = ((guint *)param->extra_widget_data)[rv.u];
      }
      if ( param->type == VIK_LAYER_PARAM_STRING)
      {
        if ( param->extra_widget_data )
        {
          /* Combobox displays labels and we want values from extra */
          int pos = gtk_combo_box_get_active ( GTK_COMBO_BOX(widget) );
          rv.s = ((const char **)param->extra_widget_data)[pos];
        }
        else if ( widget )
        {
          /* Return raw value */
#if GTK_CHECK_VERSION (2, 24, 0)
          rv.s = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))));
#else
          rv.s = gtk_combo_box_get_active_text ( GTK_COMBO_BOX(widget) );
#endif
        }
        else
          rv.s = NULL;
        g_debug("%s: %s", __FUNCTION__, rv.s);
      }
      break;
    case VIK_LAYER_WIDGET_RADIOGROUP:
    case VIK_LAYER_WIDGET_RADIOGROUP_STATIC:
      rv.u = vik_radio_group_get_selected(VIK_RADIO_GROUP(widget));
      if ( param->extra_widget_data )
        rv.u = GPOINTER_TO_UINT ( g_list_nth_data(param->extra_widget_data, rv.u) );
      break;
    case VIK_LAYER_WIDGET_SPINBUTTON:
      if ( param->type == VIK_LAYER_PARAM_UINT )
        rv.u = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(widget) );
      else if ( param->type == VIK_LAYER_PARAM_INT )
        rv.i = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(widget) );
      else
        rv.d = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(widget) );
      break;
    case VIK_LAYER_WIDGET_ENTRY:
    case VIK_LAYER_WIDGET_ENTRY_URL:
    case VIK_LAYER_WIDGET_PASSWORD:
      rv.s = gtk_entry_get_text ( GTK_ENTRY(widget) );
      break;
    case VIK_LAYER_WIDGET_FILEENTRY:
    case VIK_LAYER_WIDGET_FILESAVE:
    case VIK_LAYER_WIDGET_FOLDERENTRY:
      rv.s = vik_file_entry_get_filename ( VIK_FILE_ENTRY(widget) );
      break;
    case VIK_LAYER_WIDGET_FILELIST:
      rv.sl = vik_file_list_get_files ( VIK_FILE_LIST(widget) );
      break;
    case VIK_LAYER_WIDGET_HSCALE:
      if ( param->type == VIK_LAYER_PARAM_UINT )
        rv.u = (guint32) gtk_range_get_value ( GTK_RANGE(widget) );
      else if ( param->type == VIK_LAYER_PARAM_INT )
        rv.i = (gint32) gtk_range_get_value ( GTK_RANGE(widget) );
      else
        rv.d = gtk_range_get_value ( GTK_RANGE(widget) );
      break;
    default: break;
  }

  // Perform conversion if necessary
  if ( param->convert_to_internal )
    rv = param->convert_to_internal ( rv );

  return rv;
}

/**
 * Hacky method to enable closing the dialog within preference code
 */
void a_uibuilder_factory_close ( gint response_id )
{
  if ( dialog ) {
    RefreshWidgets = NULL;
    gtk_dialog_response ( GTK_DIALOG(dialog), response_id );
  }
}

/**
 * Another hacky method to update the widgets of a specified group
 * Must only be used whilst the properties_factory is open
 */
void a_uibuilder_factory_refresh ( VikLayerParam *params,
                                   guint16 params_count,
                                   gint16 group,
                                   VikLayerParamData (*getparam) (gpointer,guint16,gboolean),
                                   gpointer getparam1 )
{
  if ( !RefreshWidgets ) {
    g_critical ( __FUNCTION__ );
    return;
  }

  guint i, j;
  for ( i = 0, j = 0; i < params_count; i++ ) {
    if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES ) {
      if ( params[i].group == group ) {
        // Don't attempt to refresh pointers
        if ( params[i].type <= VIK_LAYER_PARAM_STRING_LIST ) {
          VikLayerParamData data = getparam ( getparam1, i, FALSE );
          refresh_widget ( RefreshWidgets[j], &(params[i]), data );
        }
      }
      j++;
    }
  }
}

/**
 * @have_apply_button: Whether the dialog should have an apply button
 * @redraw:            Function to be invoked to redraw when apply button is pressed
 *                     Typically expected to be #vik_layer_emit_update()
 * @redraw_param:      Parameter to be passed to the redraw(), so typically a VikLayer*
 *
 * Returns:
 *  0 = Dialog cancelled
 *  1 = No parameters to be displayed
 *  2 = Parameter changed that needs redraw
 *  3 = Parameter changed but redraw not necessary
 */
gint a_uibuilder_properties_factory ( const gchar *dialog_name,
                                      GtkWindow *parent,
                                      VikLayerParam *params,
                                      guint16 params_count,
                                      guint16 params_offset,
                                      gchar **groups,
                                      guint8 groups_count,
                                      gboolean (*setparam) (gpointer,gpointer),
                                      gboolean (*setparam4) (gpointer,guint16,VikLayerParamData,gpointer),
                                      gpointer pass_along1,
                                      gpointer pass_along2,
                                      VikLayerParamData (*getparam) (gpointer,guint16,gboolean),
                                      gpointer pass_along_getparam,
                                      void (*changeparam) (GtkWidget*, ui_change_values),
                                      gboolean have_apply_button,
                                      void (*redraw) (gpointer),
                                      gpointer redraw_param,
                                      gboolean show_reset_buttons )
{
  guint16 i, j, widget_count = 0;
  gboolean must_redraw = FALSE;

  if ( ! params )
    return 1; /* no params == no options, so all is good */

  for ( i = 0; i < params_count; i++ )
    if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES )
      widget_count++;

  if ( widget_count == 0)
    return 0; /* TODO -- should be one? */
  else
  {
    /* create widgets and titles; place in table */
    dialog = gtk_dialog_new_with_buttons ( dialog_name, parent,
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL, NULL );
    gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
    GtkWidget *response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
    GtkWidget *table = NULL;
    GtkWidget **tables = NULL; /* for more than one group */

    GtkWidget *notebook = NULL;
    GtkWidget **labels = g_malloc ( sizeof(GtkWidget *) * widget_count );
    GtkWidget **widgets = g_malloc ( sizeof(GtkWidget *) * widget_count );
    ui_change_values *change_values = g_malloc ( sizeof(ui_change_values) * widget_count );
    RefreshWidgets = widgets; // Update value to enable refresh mechanism to work

    if ( groups && groups_count > 1 )
    {
      guint8 current_group;
      guint16 tab_widget_count;
      notebook = gtk_notebook_new ();
      // Switch to vertical notebook mode when many groups
      if ( groups_count > 4 )
        gtk_notebook_set_tab_pos ( GTK_NOTEBOOK(notebook), GTK_POS_LEFT );
      gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook, TRUE, TRUE, 0);
      tables = g_malloc ( sizeof(GtkWidget *) * groups_count );
      for ( current_group = 0; current_group < groups_count; current_group++ )
      {
        tab_widget_count = 0;
        for ( j = 0; j < params_count; j ++ )
          if ( params[j].group == current_group )
            tab_widget_count++;

        if ( tab_widget_count ) {
          // Obviously lots of options so enable scrolling vertically
          //  especially for Preferences & TrackWaypoint Layer Properties;
          //  also makes it easier to add more options without making the dialog massive
          tables[current_group] = gtk_table_new ( tab_widget_count, 1, FALSE );
          GtkWidget *sw = gtk_scrolled_window_new ( NULL, NULL );
          gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

          // Force scrollbar to *not* overlay the window
          // (otherwise gets in the way of the checkboxes, +/- spinboxes, etc...)
#if GTK_CHECK_VERSION (3,0,0)
          gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW(sw), FALSE );
#endif
          gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW(sw), tables[current_group] );
          // Ensure it doesn't start off too small
          //  as this is a communal code for different dialogs,
          //  can't try to save/restore previous size as it may not be appropriate.
          gint width, height;
          gtk_window_get_size ( parent, &width, &height );
          height /= 2;
          // Limit the height from being excessively long
          if ( height > 325*vik_viewport_get_scale(NULL) )
            height = 325*vik_viewport_get_scale(NULL);
          gtk_widget_set_size_request ( sw, width/2.5, height );
          gtk_notebook_append_page ( GTK_NOTEBOOK(notebook), sw, gtk_label_new(groups[current_group]) );
        }
      }
    }
    else
    {
      table = gtk_table_new( widget_count, 1, FALSE );
      gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, TRUE, TRUE, 0);
    }

    for ( i = 0, j = 0; i < params_count; i++ )
    {
      if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES )
      {
        if ( tables )
          table = tables[MAX(0, params[i].group)]; /* round up NOT_IN_GROUP, that's not reasonable here */

        VikLayerParamData data = getparam ( pass_along_getparam, i, FALSE );
        widgets[j] = new_widget ( &(params[i]), data, show_reset_buttons, pass_along_getparam );

        if ( widgets[j] ) {
          if ( params[i].type == VIK_LAYER_PARAM_PTR_DEFAULT ) {
            // For the restore defaults button placed it centrally
            //   (i.e. don't use a label, thus making it appear different to the normal settings values)
            gtk_table_attach ( GTK_TABLE(table), widgets[j], 0, 2, j, j+1, GTK_SHRINK, 0, 2, 2 );
	  } else {
            if ( params[i].widget_type == VIK_LAYER_WIDGET_ENTRY_URL && data.s )
              labels[j] = gtk_link_button_new_with_label ( data.s, _(params[i].title));
#if GTK_CHECK_VERSION (3,0,0)
            else if ( params[i].widget_type == VIK_LAYER_WIDGET_SEPARATOR )
              labels[j] = gtk_separator_new ( GTK_ORIENTATION_VERTICAL );
#endif
            else
              labels[j] = gtk_label_new(_(params[i].title));
            gtk_table_attach ( GTK_TABLE(table), labels[j], 0, 1, j, j+1,
                               params[i].type == VIK_LAYER_PARAM_SPACER ? GTK_FILL : 0, 0, 0, 0 );
            gtk_table_attach ( GTK_TABLE(table), widgets[j], 1, 2, j, j+1, GTK_EXPAND | GTK_FILL,
                               params[i].type == VIK_LAYER_PARAM_STRING_LIST ? GTK_EXPAND | GTK_FILL : 0, 2, 2 );
	  }

          if ( changeparam )
          {
            change_values[j][UI_CHG_LAYER] = pass_along1;
            change_values[j][UI_CHG_PARAM] = &params[i];
            change_values[j][UI_CHG_WIDGETS] = widgets;
            change_values[j][UI_CHG_LABELS] = labels;
            // Determine whether being called from Layer Properties or Default Layer
            // ATM redraw is only used if coming from Layer Properties
            if ( redraw ) {
              change_values[j][UI_CHG_PARAM_ID] = GINT_TO_POINTER((gint)i);
            }
            else {
              // For Default Layer dialog, need to adjust IDs
              change_values[j][UI_CHG_PARAM_ID] = GINT_TO_POINTER((gint)(i+params_offset));
            }

            switch ( params[i].widget_type )
            {
              // Change conditions for other widget types can be added when needed
              case VIK_LAYER_WIDGET_COMBOBOX:
                g_signal_connect ( G_OBJECT(widgets[j]), "changed", G_CALLBACK(changeparam), change_values[j] );
                break;
              case VIK_LAYER_WIDGET_CHECKBUTTON:
                g_signal_connect ( G_OBJECT(widgets[j]), "toggled", G_CALLBACK(changeparam), change_values[j] );
                break;
              default: break;
            }
          }
        }
        j++;
      }
    }

    // Repeat run through to force changeparam callbacks now that the widgets have been created
    // This primarily so the widget sensitivities get set up
    if ( changeparam ) {
      for ( i = 0, j = 0; i < params_count; i++ ) {
        if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES ) {
          if ( widgets[j] ) {
            changeparam ( widgets[j], change_values[j] );
          }
          j++;
        }
      }
    }

    // Adding buttons performed after new_widget(),
    //  since that now has the option to insert a button in the dialog controls
    (void)gtk_dialog_add_button ( GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT );
    if ( have_apply_button )
      (void)gtk_dialog_add_button ( GTK_DIALOG(dialog), GTK_STOCK_APPLY, GTK_RESPONSE_APPLY );
    (void)gtk_dialog_add_button ( GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT );

    if ( response_w )
      gtk_widget_grab_focus ( response_w );

    gtk_widget_show_all ( dialog );

    gint resp = GTK_RESPONSE_APPLY;
    gint answer = 0;
    while ( resp == GTK_RESPONSE_APPLY || resp == GTK_RESPONSE_NONE ) {
      resp = gtk_dialog_run (GTK_DIALOG (dialog));
      if ( resp == GTK_RESPONSE_ACCEPT || resp == GTK_RESPONSE_APPLY ) {
	VikLayerSetParam vlsp;
	vlsp.is_file_operation = FALSE;
	vlsp.dirpath = NULL;
	for ( i = 0, j = 0; i < params_count; i++ ) {
	  if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES ) {
            // Don't attempt to change pointers
            if ( params[i].type <= VIK_LAYER_PARAM_STRING_LIST ) {
              vlsp.id = i;
              vlsp.vp = pass_along2;
              vlsp.data = a_uibuilder_widget_get_value ( widgets[j], &(params[i]) );
              // Main callback into each layer's setparam
              if ( setparam && setparam ( pass_along1, &vlsp ) )
                must_redraw = TRUE;
              // Or a basic callback for each parameter
              else if ( setparam4 && setparam4 ( pass_along1, i, vlsp.data, pass_along2 ) )
                must_redraw = TRUE;
            }
            j++;
          }
        }

        if ( resp == GTK_RESPONSE_APPLY ) {
          if ( redraw && redraw_param )
            redraw ( redraw_param );
          answer = 0;
        }
        else
          answer = must_redraw ? 2 : 3; // user clicked OK
      }
    }

    g_free ( widgets );
    g_free ( labels );
    g_free ( change_values );
    if ( tables )
      g_free ( tables );
    gtk_widget_destroy ( dialog );
    dialog = NULL;

    RefreshWidgets = NULL;
    return answer;
  }
}

static void uibuilder_run_setparam ( VikLayerParamData *paramdatas, guint16 i, VikLayerParamData data, VikLayerParam *params )
{
  /* could have to copy it if it's a string! */
  switch ( params[i].type ) {
    case VIK_LAYER_PARAM_STRING:
      paramdatas[i].s = g_strdup ( data.s );
      break;
    default:
     paramdatas[i] = data; /* string list will have to be freed by layer. anything else not freed */
  }
}

static VikLayerParamData uibuilder_run_getparam ( VikLayerParamData *params_defaults, guint16 i )
{
  return params_defaults[i];
}


VikLayerParamData *a_uibuilder_run_dialog (  const gchar *dialog_name, GtkWindow *parent, VikLayerParam *params,
                        guint16 params_count, gchar **groups, guint8 groups_count,
                        VikLayerParamData *params_defaults )
{
    VikLayerParamData *paramdatas = g_new(VikLayerParamData, params_count);
    if ( a_uibuilder_properties_factory ( dialog_name,
					  parent,
					  params,
					  params_count,
					  0,
					  groups,
					  groups_count,
					  NULL,
					  (gpointer) uibuilder_run_setparam,
					  paramdatas,
					  params,
					  (gpointer) uibuilder_run_getparam,
					  params_defaults,
					  NULL,
					  FALSE,
					  NULL,
					  NULL,
					  FALSE) > 0 ) {
      return paramdatas;
    }
    g_free ( paramdatas );
    return NULL;
}

/* frees data from last (if ness) */
void a_uibuilder_free_paramdatas ( VikLayerParamData *paramdatas, VikLayerParam *params, guint16 params_count )
{
  int i;
  /* may have to free strings, etc. */
  for ( i = 0; i < params_count; i++ ) {
    switch ( params[i].type ) {
      case VIK_LAYER_PARAM_STRING:
        g_free ( (gchar *) paramdatas[i].s );
        break;
      case VIK_LAYER_PARAM_STRING_LIST: {
        /* should make a util function out of this */
        GList *iter = paramdatas[i].sl;
        while ( iter ) {
          g_free ( iter->data );
          iter = iter->next;
        }
        g_list_free ( paramdatas[i].sl );
        break;
      default:
        break;
      }
    }
  }
  g_free ( paramdatas );
}

static gboolean file_save_cb ( VikFileEntry *vfe, gpointer user_data ) {
  const gchar *entry = vik_file_entry_get_filename ( vfe );
  return ( g_file_test ( entry, G_FILE_TEST_EXISTS ) == FALSE ||
           a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_WIDGET(vfe), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( entry ) ) );
}

