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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "preferences.h"
#include "dir.h"
#include "file.h"
#include "util.h"

// TODO: STRING_LIST
// TODO: share code in file reading
// TODO: remove hackaround in show_window

#define VIKING_PREFS_FILE "viking.prefs"

static GPtrArray *params;
static GHashTable *values;
gboolean loaded;

/************ groups *********/

static GPtrArray *groups_names;
static GHashTable *groups_keys_to_indices; // contains gint, NULL (0) is not found, instead 1 is used for 0, 2 for 1, etc.

static void preferences_groups_init()
{
  groups_names = g_ptr_array_new();
  groups_keys_to_indices = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );
}

static void preferences_groups_uninit()
{
  g_ptr_array_foreach ( groups_names, (GFunc)g_free, NULL );
  g_ptr_array_free ( groups_names, TRUE );
  g_hash_table_destroy ( groups_keys_to_indices );
}

void a_preferences_register_group ( const gchar *key, const gchar *name )
{
  if ( g_hash_table_lookup ( groups_keys_to_indices, key ) )
    g_critical("Duplicate preferences group keys");
  else {
    g_ptr_array_add ( groups_names, g_strdup(name) );
    g_hash_table_insert ( groups_keys_to_indices, g_strdup(key), GINT_TO_POINTER ( (gint) groups_names->len ) ); /* index + 1 */
  }
}

/* returns -1 if not found. */
static gint16 preferences_groups_key_to_index( const gchar *key )
{
  gint index = GPOINTER_TO_INT ( g_hash_table_lookup ( groups_keys_to_indices, key ) );
  if ( ! index )
    return VIK_LAYER_GROUP_NONE; /* which should be -1 anyway */
  return (gint16) (index - 1);
}

/*****************************/

static gboolean preferences_load_from_file()
{
  gchar *fn = g_build_filename(a_get_viking_dir(), VIKING_PREFS_FILE, NULL);
  FILE *f = g_fopen(fn, "r");
  g_free ( fn );

  if ( f ) {
    gchar buf[4096];
    gchar *key, *val;
    VikLayerTypedParamData *oldval, *newval;
    while ( ! feof (f) ) {
      if (fgets(buf,sizeof(buf),f) == NULL)
        break;
      if ( split_string_from_file_on_equals ( buf, &key, &val ) ) {
        // if it's not in there, ignore it
        oldval = g_hash_table_lookup ( values, key );
        if ( ! oldval ) {
          g_free(key);
          g_free(val);
          continue;
        }

        // otherwise change it (you know the type!)
        // if it's a string list do some funky stuff ... yuck... not yet.
        if ( oldval->type == VIK_LAYER_PARAM_STRING_LIST )
          g_critical ( "Param strings not implemented in preferences"); // fake it

        newval = vik_layer_data_typed_param_copy_from_string ( oldval->type, val );
        g_hash_table_insert ( values, key, newval );

        g_free(key);
        g_free(val);
        // change value
      }
    }
    fclose(f);
    f = NULL;
    return TRUE;
  }
  return FALSE;
}

static void preferences_run_setparam ( gpointer notused, guint16 i, VikLayerParamData data, VikLayerParam *vlparams )
{
  // Don't change stored pointer values
  if ( vlparams[i].type == VIK_LAYER_PARAM_PTR )
    return;
  if ( vlparams[i].type == VIK_LAYER_PARAM_STRING_LIST )
    g_critical ( "Param strings not implemented in preferences"); //fake it
  g_hash_table_insert ( values, (gchar *)(vlparams[i].name), vik_layer_typed_param_data_copy_from_data(vlparams[i].type, data) );
}

/* Allow preferences to be manipulated externally */
void a_preferences_run_setparam ( VikLayerParamData data, VikLayerParam *vlparams )
{
  preferences_run_setparam (NULL, 0, data, vlparams);
}

static VikLayerParamData preferences_run_getparam ( gpointer notused, guint16 i, gboolean notused2 )
{
  VikLayerTypedParamData *val = (VikLayerTypedParamData *) g_hash_table_lookup ( values, ((VikLayerParam *)g_ptr_array_index(params,i))->name );
  g_assert ( val != NULL );
  if ( val->type == VIK_LAYER_PARAM_STRING_LIST )
    g_critical ( "Param strings not implemented in preferences"); //fake it
  return val->data;
}

/**
 * a_preferences_save_to_file:
 * 
 * Returns: TRUE on success
 */
gboolean a_preferences_save_to_file()
{
  gchar *fn = g_build_filename(a_get_viking_dir(), VIKING_PREFS_FILE, NULL);

  // TODO: error checking
  FILE *f = g_fopen(fn, "w");
  /* Since preferences files saves OSM login credentials,
   * it'll be better to store it in secret.
   */
  g_chmod(fn, 0600);
  g_free ( fn );

  if ( f ) {
    VikLayerParam *param;
    VikLayerTypedParamData *val;
    int i;
    for ( i = 0; i < params->len; i++ ) {
      param = (VikLayerParam *) g_ptr_array_index(params,i);
      val = (VikLayerTypedParamData *) g_hash_table_lookup ( values, param->name );
      if ( val )
        if ( val->type != VIK_LAYER_PARAM_PTR )
          file_write_layer_param ( f, param->name, val->type, val->data );
    }
    fclose(f);
    f = NULL;
    return TRUE;
  }

  return FALSE;
}


void a_preferences_show_window(GtkWindow *parent) {
    //VikLayerParamData *a_uibuilder_run_dialog ( GtkWindow *parent, VikLayerParam \*params, // guint16 params_count, gchar **groups, guint8 groups_count, // VikLayerParamData *params_defaults )
    // TODO: THIS IS A MAJOR HACKAROUND, but ok when we have only a couple preferences.
    gint params_count = params->len;
    VikLayerParam *contiguous_params = g_new(VikLayerParam,params_count);
    int i;
    for ( i = 0; i < params->len; i++ ) {
      contiguous_params[i] = *((VikLayerParam*)(g_ptr_array_index(params,i)));
    }
    loaded = TRUE;
    preferences_load_from_file();
    if ( a_uibuilder_properties_factory ( _("Preferences"), parent, contiguous_params, params_count,
				(gchar **) groups_names->pdata, groups_names->len, // groups, groups_count, // groups? what groups?!
				(gboolean (*) (gpointer,guint16,VikLayerParamData,gpointer,gboolean)) preferences_run_setparam,
				NULL /* not used */, contiguous_params,
                                preferences_run_getparam, NULL, NULL /* not used */ ) ) {
      a_preferences_save_to_file();
    }
    g_free ( contiguous_params );
}

void a_preferences_register(VikLayerParam *pref, VikLayerParamData defaultval, const gchar *group_key )
{
  // All preferences should be registered before loading
  if ( loaded )
    g_critical ( "REGISTERING preference %s after LOADING from " VIKING_PREFS_FILE, pref->name );
  /* copy value */
  VikLayerParam *newpref = g_new(VikLayerParam,1);
  *newpref = *pref;
  VikLayerTypedParamData *newval = vik_layer_typed_param_data_copy_from_data(pref->type, defaultval);
  if ( group_key )
    newpref->group = preferences_groups_key_to_index ( group_key );

  g_ptr_array_add ( params, newpref );
  g_hash_table_insert ( values, (gchar *)pref->name, newval );
}

void a_preferences_init()
{
  preferences_groups_init();

  /* not copied */
  params = g_ptr_array_new ();

  /* key not copied (same ptr as in pref), actual param data yes */
  values = g_hash_table_new_full ( g_str_hash, g_str_equal, NULL, vik_layer_typed_param_data_free);

  loaded = FALSE;
}

void a_preferences_uninit()
{
  preferences_groups_uninit();

  g_ptr_array_foreach ( params, (GFunc)g_free, NULL );
  g_ptr_array_free ( params, TRUE );
  g_hash_table_destroy ( values );
}



VikLayerParamData *a_preferences_get(const gchar *key)
{
  if ( ! loaded ) {
    g_debug ( "%s: First time: %s\n", __FUNCTION__, key );
    /* since we can't load the file in a_preferences_init (no params registered yet),
     * do it once before we get the first key. */
    preferences_load_from_file();
    loaded = TRUE;
  }
  return g_hash_table_lookup ( values, key );
}
