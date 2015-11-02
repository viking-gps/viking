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
#include "uibuilder.h"
#include "vikradiogroup.h"
#include "vikfileentry.h"
#include "vikfilelist.h"

VikLayerParamData vik_lpd_true_default ( void ) { return VIK_LPD_BOOLEAN ( TRUE ); }
VikLayerParamData vik_lpd_false_default ( void ) { return VIK_LPD_BOOLEAN ( FALSE ); }

GtkWidget *a_uibuilder_new_widget ( VikLayerParam *param, VikLayerParamData data )
{
  // Perform pre conversion if necessary
  VikLayerParamData vlpd = data;
  if ( param->convert_to_display )
   vlpd = param->convert_to_display ( data );

  GtkWidget *rv = NULL;
  switch ( param->widget_type )
  {
    case VIK_LAYER_WIDGET_COLOR:
      if ( param->type == VIK_LAYER_PARAM_COLOR )
        rv = gtk_color_button_new_with_color ( &(vlpd.c) );
      break;
    case VIK_LAYER_WIDGET_CHECKBUTTON:
      if ( param->type == VIK_LAYER_PARAM_BOOLEAN )
      {
        //rv = gtk_check_button_new_with_label ( //param->title );
        rv = gtk_check_button_new ();
        if ( vlpd.b )
          gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(rv), TRUE );
      }
      break;
    case VIK_LAYER_WIDGET_COMBOBOX:
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data )
      {
        /* Build a simple combobox */
        gchar **pstr = param->widget_data;
        rv = vik_combo_box_text_new ();
        while ( *pstr )
          vik_combo_box_text_append ( rv, *(pstr++) );
        if ( param->extra_widget_data ) /* map of alternate uint values for options */
        {
          /* Set the effective default value */
          int i;
          for ( i = 0; ((const char **)param->widget_data)[i]; i++ )
            if ( ((guint *)param->extra_widget_data)[i] == vlpd.u )
            {
              /* Match default value */
              gtk_combo_box_set_active ( GTK_COMBO_BOX(rv), i );
              break;
            }
        }
        else
          gtk_combo_box_set_active ( GTK_COMBO_BOX ( rv ), vlpd.u );
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
        if ( vlpd.s )
          vik_combo_box_text_append ( rv, vlpd.s );
        while ( *pstr )
          vik_combo_box_text_append ( rv, *(pstr++) );
        if ( vlpd.s )
          gtk_combo_box_set_active ( GTK_COMBO_BOX ( rv ), 0 );
      }
      else if ( param->type == VIK_LAYER_PARAM_STRING && param->widget_data && param->extra_widget_data)
      {
        /* Build a combobox with fixed selections without editable text */
        gchar **pstr = param->widget_data;
        rv = GTK_WIDGET ( vik_combo_box_text_new () );
        while ( *pstr )
          vik_combo_box_text_append ( rv, *(pstr++) );
        if ( vlpd.s )
        {
          /* Set the effective default value */
          /* In case of value does not exist, set the first value */
          gtk_combo_box_set_active ( GTK_COMBO_BOX ( rv ), 0 );
          int i;
          for ( i = 0; ((const char **)param->widget_data)[i]; i++ )
            if ( strcmp(((const char **)param->extra_widget_data)[i], vlpd.s) == 0 )
            {
              /* Match default value */
              gtk_combo_box_set_active ( GTK_COMBO_BOX ( rv ), i );
              break; 
            }
        }
        else
          gtk_combo_box_set_active ( GTK_COMBO_BOX ( rv ), 0 );
      }
      break;
    case VIK_LAYER_WIDGET_RADIOGROUP:
      /* widget_data and extra_widget_data are GList */
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data )
      {
        rv = vik_radio_group_new ( param->widget_data );
        if ( param->extra_widget_data ) /* map of alternate uint values for options */
        {
          int i;
          int nb_elem = g_list_length(param->widget_data);
          for ( i = 0; i < nb_elem; i++ )
            if ( GPOINTER_TO_UINT ( g_list_nth_data(param->extra_widget_data, i) ) == vlpd.u )
            {
              vik_radio_group_set_selected ( VIK_RADIO_GROUP(rv), i );
              break;
            }
        }
        else if ( vlpd.u ) /* zero is already default */
          vik_radio_group_set_selected ( VIK_RADIO_GROUP(rv), vlpd.u );
      }
      break;
    case VIK_LAYER_WIDGET_RADIOGROUP_STATIC:
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data )
      {
        rv = vik_radio_group_new_static ( (const gchar **) param->widget_data );
        if ( param->extra_widget_data ) /* map of alternate uint values for options */
        {
          int i;
          for ( i = 0; ((const char **)param->widget_data)[i]; i++ )
            if ( ((guint *)param->extra_widget_data)[i] == vlpd.u )
            {
              vik_radio_group_set_selected ( VIK_RADIO_GROUP(rv), i );
              break;
            }
        }
        else if ( vlpd.u ) /* zero is already default */
          vik_radio_group_set_selected ( VIK_RADIO_GROUP(rv), vlpd.u );
      }
      break;
    case VIK_LAYER_WIDGET_SPINBUTTON:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE || param->type == VIK_LAYER_PARAM_UINT
           || param->type == VIK_LAYER_PARAM_INT)  && param->widget_data )
      {
        gdouble init_val = (param->type == VIK_LAYER_PARAM_DOUBLE) ? vlpd.d : (param->type == VIK_LAYER_PARAM_UINT ? vlpd.u : vlpd.i);
        VikLayerParamScale *scale = (VikLayerParamScale *) param->widget_data;
        rv = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new( init_val, scale->min, scale->max, scale->step, scale->step, 0 )), scale->step, scale->digits );
      }
    break;
    case VIK_LAYER_WIDGET_ENTRY:
      if ( param->type == VIK_LAYER_PARAM_STRING )
      {
        rv = gtk_entry_new ();
        if ( vlpd.s )
          gtk_entry_set_text ( GTK_ENTRY(rv), vlpd.s );
      }
      break;
    case VIK_LAYER_WIDGET_PASSWORD:
      if ( param->type == VIK_LAYER_PARAM_STRING )
      {
        rv = gtk_entry_new ();
        gtk_entry_set_visibility ( GTK_ENTRY(rv), FALSE );
        if ( vlpd.s )
          gtk_entry_set_text ( GTK_ENTRY(rv), vlpd.s );
        gtk_widget_set_tooltip_text ( GTK_WIDGET(rv),
                                     _("Take care that this password will be stored clearly in a plain file.") );
      }
      break;
    case VIK_LAYER_WIDGET_FILEENTRY:
      if ( param->type == VIK_LAYER_PARAM_STRING )
      {
        rv = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, GPOINTER_TO_INT(param->widget_data), NULL, NULL);
        if ( vlpd.s )
          vik_file_entry_set_filename ( VIK_FILE_ENTRY(rv), vlpd.s );
      }
      break;
    case VIK_LAYER_WIDGET_FOLDERENTRY:
      if ( param->type == VIK_LAYER_PARAM_STRING )
      {
        rv = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, VF_FILTER_NONE, NULL, NULL);
        if ( vlpd.s )
          vik_file_entry_set_filename ( VIK_FILE_ENTRY(rv), vlpd.s );
      }
      break;

    case VIK_LAYER_WIDGET_FILELIST:
      if ( param->type == VIK_LAYER_PARAM_STRING_LIST )
      {
        rv = vik_file_list_new ( _(param->title), NULL );
        vik_file_list_set_files ( VIK_FILE_LIST(rv), vlpd.sl );
      }
      break;
    case VIK_LAYER_WIDGET_HSCALE:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE || param->type == VIK_LAYER_PARAM_UINT
           || param->type == VIK_LAYER_PARAM_INT)  && param->widget_data )
      {
        gdouble init_val = (param->type == VIK_LAYER_PARAM_DOUBLE) ? vlpd.d : (param->type == VIK_LAYER_PARAM_UINT ? vlpd.u : vlpd.i);
        VikLayerParamScale *scale = (VikLayerParamScale *) param->widget_data;
        rv = gtk_hscale_new_with_range ( scale->min, scale->max, scale->step );
        gtk_scale_set_digits ( GTK_SCALE(rv), scale->digits );
        gtk_range_set_value ( GTK_RANGE(rv), init_val );
      }
      break;

    case VIK_LAYER_WIDGET_BUTTON:
      if ( param->type == VIK_LAYER_PARAM_PTR && param->widget_data ) {
        rv = gtk_button_new_with_label ( param->widget_data );
        g_signal_connect ( G_OBJECT(rv), "clicked", G_CALLBACK (vlpd.ptr), param->extra_widget_data );
      }
      break;

    default: break;
  }
  if ( rv && !gtk_widget_get_tooltip_text ( rv ) ) {
    if ( param->tooltip )
      gtk_widget_set_tooltip_text ( rv, _(param->tooltip) );
  }
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
      if ( param->type == VIK_LAYER_PARAM_UINT )
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
        else
        {
          /* Return raw value */
#if GTK_CHECK_VERSION (2, 24, 0)
          rv.s = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))));
#else
          rv.s = gtk_combo_box_get_active_text ( GTK_COMBO_BOX(widget) );
#endif
        }
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
    case VIK_LAYER_WIDGET_PASSWORD:
      rv.s = gtk_entry_get_text ( GTK_ENTRY(widget) );
      break;
    case VIK_LAYER_WIDGET_FILEENTRY:
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

//static void draw_to_image_file_total_area_cb (GtkSpinButton *spinbutton, gpointer *pass_along)
gint a_uibuilder_properties_factory ( const gchar *dialog_name,
                                      GtkWindow *parent,
                                      VikLayerParam *params,
                                      guint16 params_count,
                                      gchar **groups,
                                      guint8 groups_count,
                                      gboolean (*setparam) (gpointer,guint16,VikLayerParamData,gpointer,gboolean),
                                      gpointer pass_along1,
                                      gpointer pass_along2,
                                      VikLayerParamData (*getparam) (gpointer,guint16,gboolean),
                                      gpointer pass_along_getparam,
                                      void (*changeparam) (GtkWidget*, ui_change_values) )
                                      /* pass_along1 and pass_along2 are for set_param first and last params */
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
    GtkWidget *dialog = gtk_dialog_new_with_buttons ( dialog_name,
						      parent,
						      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
						      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL );
    gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
    GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
    response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
#endif
    gint resp;

    GtkWidget *table = NULL;
    GtkWidget **tables = NULL; /* for more than one group */

    GtkWidget *notebook = NULL;
    GtkWidget **labels = g_malloc ( sizeof(GtkWidget *) * widget_count );
    GtkWidget **widgets = g_malloc ( sizeof(GtkWidget *) * widget_count );
    ui_change_values *change_values = g_malloc ( sizeof(ui_change_values) * widget_count );

    if ( groups && groups_count > 1 )
    {
      guint8 current_group;
      guint16 tab_widget_count;
      notebook = gtk_notebook_new ();
      // Switch to vertical notebook mode when many groups
      if ( groups_count > 4 )
        gtk_notebook_set_tab_pos ( GTK_NOTEBOOK(notebook), GTK_POS_LEFT );
      gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook, FALSE, FALSE, 0);
      tables = g_malloc ( sizeof(GtkWidget *) * groups_count );
      for ( current_group = 0; current_group < groups_count; current_group++ )
      {
        tab_widget_count = 0;
        for ( j = 0; j < params_count; j ++ )
          if ( params[j].group == current_group )
            tab_widget_count++;

        if ( tab_widget_count )
        {
          tables[current_group] = gtk_table_new ( tab_widget_count, 1, FALSE );
          gtk_notebook_append_page ( GTK_NOTEBOOK(notebook), tables[current_group], gtk_label_new(groups[current_group]) );
        }
      }
    }
    else
    {
      table = gtk_table_new( widget_count, 1, FALSE );
      gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, FALSE, FALSE, 0);
    }

    for ( i = 0, j = 0; i < params_count; i++ )
    {
      if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES )
      {
        if ( tables )
          table = tables[MAX(0, params[i].group)]; /* round up NOT_IN_GROUP, that's not reasonable here */

        widgets[j] = a_uibuilder_new_widget ( &(params[i]), getparam ( pass_along_getparam, i, FALSE ) );

        if ( widgets[j] ) {
          labels[j] = gtk_label_new(_(params[i].title));
          gtk_table_attach ( GTK_TABLE(table), labels[j], 0, 1, j, j+1, 0, 0, 0, 0 );
          gtk_table_attach ( GTK_TABLE(table), widgets[j], 1, 2, j, j+1, GTK_EXPAND | GTK_FILL, 0, 2, 2 );

          if ( changeparam )
          {
            change_values[j][UI_CHG_LAYER] = pass_along1;
            change_values[j][UI_CHG_PARAM] = &params[i];
            change_values[j][UI_CHG_PARAM_ID] = GINT_TO_POINTER((gint)i);
            change_values[j][UI_CHG_WIDGETS] = widgets;
            change_values[j][UI_CHG_LABELS] = labels;

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

    if ( response_w )
      gtk_widget_grab_focus ( response_w );

    gtk_widget_show_all ( dialog );

    resp = gtk_dialog_run (GTK_DIALOG (dialog));
    if ( resp == GTK_RESPONSE_ACCEPT )
    {
      for ( i = 0, j = 0; i < params_count; i++ )
      {
        if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES )
        {
          if ( setparam ( pass_along1,
			  i,
			  a_uibuilder_widget_get_value ( widgets[j], &(params[i]) ),
			  pass_along2,
			  FALSE ) )
            must_redraw = TRUE;
          j++;
        }
      }

      g_free ( widgets );
      g_free ( labels );
      g_free ( change_values );
      if ( tables )
        g_free ( tables );
      gtk_widget_destroy ( dialog ); /* hide before redrawing. */

      return must_redraw ? 2 : 3; /* user clicked OK */
    }

    g_free ( widgets );
    g_free ( labels );
    g_free ( change_values );
    if ( tables )
      g_free ( tables );
    gtk_widget_destroy ( dialog );

    return 0;
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
					  groups, 
					  groups_count,
					  (gpointer) uibuilder_run_setparam, 
					  paramdatas, 
					  params,
					  (gpointer) uibuilder_run_getparam, 
					  params_defaults,
					  NULL ) > 0 ) {

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
