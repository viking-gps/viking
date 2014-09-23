/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "viklayer_defaults.h"
#include "dir.h"
#include "file.h"

#define VIKING_LAYER_DEFAULTS_INI_FILE "viking_layer_defaults.ini"

// A list of the parameter types in use
static GPtrArray *paramsVD;

static GKeyFile *keyfile;

static gboolean loaded;

static VikLayerParamData get_default_data_answer ( const gchar *group, const gchar *name, VikLayerParamType ptype, gpointer *success )
{
	VikLayerParamData data = VIK_LPD_BOOLEAN ( FALSE );

	GError *error = NULL;

	switch ( ptype ) {
	case VIK_LAYER_PARAM_DOUBLE: {
		gdouble dd = g_key_file_get_double ( keyfile, group, name, &error );
		if ( !error ) data.d = dd;
		break;
	}
	case VIK_LAYER_PARAM_UINT: {
		guint32 uu = g_key_file_get_integer ( keyfile, group, name, &error );
		if ( !error ) data.u = uu;
		break;
	}
	case VIK_LAYER_PARAM_INT: {
		gint32 ii = g_key_file_get_integer ( keyfile, group, name, &error );
		if ( !error ) data.i = ii;
		break;
	}
	case VIK_LAYER_PARAM_BOOLEAN: {
		gboolean bb = g_key_file_get_boolean ( keyfile, group, name, &error );
		if ( !error ) data.b = bb;
		break;
	}
	case VIK_LAYER_PARAM_STRING: {
		gchar *str = g_key_file_get_string ( keyfile, group, name, &error );
		if ( !error ) data.s = str;
		break;
	}
	//case VIK_LAYER_PARAM_STRING_LIST: {
	//	gchar **str = g_key_file_get_string_list ( keyfile, group, name, &error );
	//	data.sl = str_to_glist (str); //TODO convert
	//	break;
	//}
	case VIK_LAYER_PARAM_COLOR: {
		gchar *str = g_key_file_get_string ( keyfile, group, name, &error );
		if ( !error ) {
			memset(&(data.c), 0, sizeof(data.c));
			gdk_color_parse ( str, &(data.c) );
		}
		g_free ( str );
		break;
	}
	default: break;
	}
	*success = GINT_TO_POINTER (TRUE);
	if ( error ) {
		g_warning ( "%s", error->message );
		g_error_free ( error );
		*success = GINT_TO_POINTER (FALSE);
	}

	return data;
}

static VikLayerParamData get_default_data ( const gchar *group, const gchar *name, VikLayerParamType ptype )
{
	gpointer success = GINT_TO_POINTER (TRUE);
	// NB This should always succeed - don't worry about 'success'
	return get_default_data_answer ( group, name, ptype, &success );
}

static void set_default_data ( VikLayerParamData data, const gchar *group, const gchar *name, VikLayerParamType ptype )
{
	switch ( ptype ) {
	case VIK_LAYER_PARAM_DOUBLE:
		g_key_file_set_double ( keyfile, group, name, data.d );
		break;
	case VIK_LAYER_PARAM_UINT:
		g_key_file_set_integer ( keyfile, group, name, data.u );
		break;
	case VIK_LAYER_PARAM_INT:
		g_key_file_set_integer ( keyfile, group, name, data.i );
		break;
	case VIK_LAYER_PARAM_BOOLEAN:
		g_key_file_set_boolean ( keyfile, group, name, data.b );
		break;
	case VIK_LAYER_PARAM_STRING:
		g_key_file_set_string ( keyfile, group, name, data.s );
		break;
	case VIK_LAYER_PARAM_COLOR: {
		gchar *str = g_strdup_printf ( "#%.2x%.2x%.2x", (int)(data.c.red/256),(int)(data.c.green/256),(int)(data.c.blue/256));
		g_key_file_set_string ( keyfile, group, name, str );
		g_free ( str );
		break;
	}
	default: break;
	}
}

static void defaults_run_setparam ( gpointer index_ptr, guint16 i, VikLayerParamData data, VikLayerParam *params )
{
	// Index is only an index into values from this layer
	gint index = GPOINTER_TO_INT ( index_ptr );
	VikLayerParam *vlp = (VikLayerParam *)g_ptr_array_index(paramsVD,index+i);

	set_default_data ( data, vik_layer_get_interface(vlp->layer)->fixed_layer_name, vlp->name, vlp->type );
}

static VikLayerParamData defaults_run_getparam ( gpointer index_ptr, guint16 i, gboolean notused2 )
{
	// Index is only an index into values from this layer
	gint index = GPOINTER_TO_INT ( index_ptr );
	VikLayerParam *vlp = (VikLayerParam *)g_ptr_array_index(paramsVD,index+i);

	return get_default_data ( vik_layer_get_interface(vlp->layer)->fixed_layer_name, vlp->name, vlp->type );
}

static void use_internal_defaults_if_missing_default ( VikLayerTypeEnum type )
{
	VikLayerParam *params = vik_layer_get_interface(type)->params;
	if ( ! params )
		return;

	guint16 params_count = vik_layer_get_interface(type)->params_count;
	guint16 i;
	// Process each parameter
	for ( i = 0; i < params_count; i++ ) {
		if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES ) {
			gpointer success = GINT_TO_POINTER (FALSE);
			// Check current default is available
			get_default_data_answer ( vik_layer_get_interface(type)->fixed_layer_name, params[i].name, params[i].type, &success );
			// If no longer have a viable default
			if ( ! GPOINTER_TO_INT (success) ) {
				// Reset value
				if ( params[i].default_value ) {
					VikLayerParamData paramd = params[i].default_value();
					set_default_data ( paramd, vik_layer_get_interface(type)->fixed_layer_name, params[i].name, params[i].type );
				}
			}
		}
	}
}

static gboolean defaults_load_from_file()
{
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

	GError *error = NULL;

	gchar *fn = g_build_filename ( a_get_viking_dir(), VIKING_LAYER_DEFAULTS_INI_FILE, NULL );

	if ( !g_key_file_load_from_file ( keyfile, fn, flags, &error ) ) {
		g_warning ( "%s: %s", error->message, fn );
		g_free ( fn );
		g_error_free ( error );
		return FALSE;
	}

	g_free ( fn );

	// Ensure if have a key file, then any missing values are set from the internal defaults
	VikLayerTypeEnum layer;
	for ( layer = 0; layer < VIK_LAYER_NUM_TYPES; layer++ ) {
		use_internal_defaults_if_missing_default ( layer );
	}

	return TRUE;
}

/* TRUE on success */
static gboolean layer_defaults_save_to_file()
{
	gboolean answer = TRUE;
	GError *error = NULL;
	gchar *fn = g_build_filename ( a_get_viking_dir(), VIKING_LAYER_DEFAULTS_INI_FILE, NULL );
	gsize size;

    gchar *keyfilestr = g_key_file_to_data ( keyfile, &size, &error );

    if ( error ) {
		g_warning ( "%s", error->message );
		g_error_free ( error );
		answer = FALSE;
		goto tidy;
	}

	// optionally could do:
	// g_file_set_contents ( fn, keyfilestr, size, &error );
    // if ( error ) {
	//	g_warning ( "%s: %s", error->message, fn );
	//	 g_error_free ( error );
	//  answer = FALSE; 
	//	goto tidy;
	// } 

	FILE *ff;
	if ( !(ff = g_fopen ( fn, "w")) ) {
		g_warning ( _("Could not open file: %s"), fn );
		answer = FALSE;
		goto tidy;
	}
    // Layer defaults not that secret, but just incase...
	g_chmod ( fn, 0600 );
	
	fputs ( keyfilestr, ff );
	fclose ( ff );

tidy:
	g_free ( keyfilestr );
	g_free ( fn );

	return answer;
}

/**
 * a_layer_defaults_show_window:
 * @parent:     The Window
 * @layername:  The layer
 *
 * This displays a Window showing the default parameter values for the selected layer
 * It allows the parameters to be changed.
 *
 * Returns: %TRUE if the window is displayed (because there are parameters to view)
 */
gboolean a_layer_defaults_show_window ( GtkWindow *parent, const gchar *layername )
{
	if ( ! loaded ) {
		// since we can't load the file in a_defaults_init (no params registered yet),
		// do it once before we display the params.
		defaults_load_from_file();
		loaded = TRUE;
	}
  
    VikLayerTypeEnum layer = vik_layer_type_from_string ( layername );
    
    // Need to know where the params start and they finish for this layer

    // 1. inspect every registered param - see if it has the layer value required to determine overall size
    //    [they are contiguous from the start index]
    // 2. copy the these params from the main list into a tmp struct
    // 
    // Then pass this tmp struct to uibuilder for display

    guint layer_params_count = 0;
    
    gboolean found_first = FALSE;
    gint index = 0;
    int i;
    for ( i = 0; i < paramsVD->len; i++ ) {
		VikLayerParam *param = (VikLayerParam*)(g_ptr_array_index(paramsVD,i));
		if ( param->layer == layer ) {
			layer_params_count++;
			if ( !found_first ) {
				index = i;
				found_first = TRUE;
			}
		}
    }

	// Have we any parameters to show!
    if ( !layer_params_count )
		return FALSE;

    VikLayerParam *params = g_new(VikLayerParam,layer_params_count);
    for ( i = 0; i < layer_params_count; i++ ) {
      params[i] = *((VikLayerParam*)(g_ptr_array_index(paramsVD,i+index)));
    }

    gchar *title = g_strconcat ( layername, ": ", _("Layer Defaults"), NULL );
    
	if ( a_uibuilder_properties_factory ( title,
	                                      parent,
	                                      params,
	                                      layer_params_count,
	                                      vik_layer_get_interface(layer)->params_groups,
	                                      vik_layer_get_interface(layer)->params_groups_count,
	                                      (gboolean (*) (gpointer,guint16,VikLayerParamData,gpointer,gboolean)) defaults_run_setparam,
	                                      GINT_TO_POINTER ( index ),
	                                      params,
	                                      defaults_run_getparam,
	                                      GINT_TO_POINTER ( index ),
	                                      NULL ) ) {
		// Save
		layer_defaults_save_to_file();
    }
    
    g_free ( title );
    g_free ( params );
    
    return TRUE;
}

/**
 * a_layer_defaults_register:
 * @vlp:        The parameter
 * @defaultval: The default value
 * @layername:  The layer in which the parameter resides
 *
 * Call this function on to set the default value for the particular parameter
 */
void a_layer_defaults_register (VikLayerParam *vlp, VikLayerParamData defaultval, const gchar *layername )
{
	/* copy value */
	VikLayerParam *newvlp = g_new(VikLayerParam,1);
	*newvlp = *vlp;

	g_ptr_array_add ( paramsVD, newvlp );

	set_default_data ( defaultval, layername, vlp->name, vlp->type );
}

/**
 * a_layer_defaults_init:
 *
 * Call this function at startup
 */
void a_layer_defaults_init()
{
	keyfile = g_key_file_new();

	/* not copied */
	paramsVD = g_ptr_array_new ();

	loaded = FALSE;
}

/**
 * a_layer_defaults_uninit:
 *
 * Call this function on program exit
 */
void a_layer_defaults_uninit()
{
	g_key_file_free ( keyfile );	
	g_ptr_array_foreach ( paramsVD, (GFunc)g_free, NULL );
	g_ptr_array_free ( paramsVD, TRUE );
}

/**
 * a_layer_defaults_get:
 * @layername:  String name of the layer
 * @param_name: String name of the parameter
 * @param_type: The parameter type
 *
 * Call this function to get the default value for the parameter requested
 */
VikLayerParamData a_layer_defaults_get ( const gchar *layername, const gchar *param_name, VikLayerParamType param_type )
{
	if ( ! loaded ) {
		// since we can't load the file in a_defaults_init (no params registered yet),
		// do it once before we get the first key.
		defaults_load_from_file();
		loaded = TRUE;
	}
  
	return get_default_data ( layername, param_name, param_type );
}

/**
 * a_layer_defaults_save:
 *
 * Call this function to save the current layer defaults
 * Normally should only be performed if any layer defaults have been changed via direct manipulation of the layer
 *  rather than the user changing the preferences via the dialog window above
 *
 * This must only be performed once all layer parameters have been initialized
 *
 * Returns: %TRUE if saving was successful
 */
gboolean a_layer_defaults_save ()
{
	// Generate defaults
	VikLayerTypeEnum layer;
	for ( layer = 0; layer < VIK_LAYER_NUM_TYPES; layer++ ) {
		use_internal_defaults_if_missing_default ( layer );
	}

	return layer_defaults_save_to_file();
}
