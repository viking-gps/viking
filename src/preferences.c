#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "preferences.h"
#include "file.h"

// TODO: register_group
// TODO: STRING_LIST
// TODO: share code in file reading
// TODO: remove hackaround in show_window
// TODO: move typeddata to uibuilder, make it more used & general, it's a "prettier" solution methinks
// maybe this wasn't such a good idea...

#define VIKING_PREFS_FILE "viking.prefs"

#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F') )

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
  g_ptr_array_free ( groups_names, TRUE );
  g_hash_table_destroy ( groups_keys_to_indices );
}

void a_preferences_register_group ( const gchar *key, const gchar *name )
{
  if ( g_hash_table_lookup ( groups_keys_to_indices, key ) )
    g_error("Duplicate preferences group keys");
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

/************/

typedef struct {
  VikLayerParamData data;
  guint8 type;
  gpointer freeme; // because data.s is const and the compiler complains
} VikLayerTypedParamData;

void layer_typed_param_data_free(gpointer p)
{
  VikLayerTypedParamData *val = (VikLayerTypedParamData *)p;
  switch ( val->type ) {
    case VIK_LAYER_PARAM_STRING:
      if ( val->freeme )
        g_free ( val->freeme );
      break;
    /* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
     * the internals call get_param -- i.e. it should be managed w/in the layer.
     * The value passed by the internals into set_param should also be managed
     * by the layer -- i.e. free'd by the layer.
     */
    case VIK_LAYER_PARAM_STRING_LIST:
      g_error ( "Param strings not implemented in preferences"); //fake it
      break;
  }
  g_free ( val );
}

VikLayerTypedParamData *layer_typed_param_data_copy_from_data(guint8 type, VikLayerParamData val) {
  VikLayerTypedParamData *newval = g_new(VikLayerTypedParamData,1);
  newval->data = val;
  newval->type = type;
  switch ( newval->type ) {
    case VIK_LAYER_PARAM_STRING: {
      gchar *s = g_strdup(newval->data.s);
      newval->data.s = s;
      newval->freeme = s;
      break;
    }
    /* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
     * the internals call get_param -- i.e. it should be managed w/in the layer.
     * The value passed by the internals into set_param should also be managed
     * by the layer -- i.e. free'd by the layer.
     */
    case VIK_LAYER_PARAM_STRING_LIST:
      g_error ( "Param strings not implemented in preferences"); //fake it
      break;
  }
  return newval;
}

/* TODO: share this code with file.c */
VikLayerTypedParamData *layer_data_typed_param_copy_from_string ( guint8 type, const gchar *str )
{
  g_assert ( type != VIK_LAYER_PARAM_STRING_LIST );
  VikLayerTypedParamData *rv = g_new(VikLayerTypedParamData,1);
  rv->type = type;
  switch ( type )
  {
    case VIK_LAYER_PARAM_DOUBLE: rv->data.d = strtod(str, NULL); break;
    case VIK_LAYER_PARAM_UINT: rv->data.u = strtoul(str, NULL, 10); break;
    case VIK_LAYER_PARAM_INT: rv->data.i = strtol(str, NULL, 10); break;
    case VIK_LAYER_PARAM_BOOLEAN: rv->data.b = TEST_BOOLEAN(str); break;
    case VIK_LAYER_PARAM_COLOR: memset(&(rv->data.c), 0, sizeof(rv->data.c)); /* default: black */
      gdk_color_parse ( str, &(rv->data.c) ); break;
    /* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING */
    default: {
      gchar *s = g_strdup(str);
      rv->data.s = s;
      rv->freeme = s;
    }
  }
  return rv;
}

/************/

/* MAKES A COPY OF THE KEY!!! */
static gboolean preferences_load_parse_param(gchar *buf, gchar **key, gchar **val )
{
  gchar *eq_pos;
  gint len;

  // comments, special characters in viking file format
  if ( buf == NULL || buf[0] == '\0' || buf[0] == '~' || buf[0] == '=' || buf[0] == '#' )
    return FALSE;
  eq_pos = strchr ( buf, '=' );
  if ( ! eq_pos )
    return FALSE;
  *key = g_strndup ( buf, eq_pos - buf );
  *val = eq_pos + 1;
  len = strlen(*val);
  if ( len > 0 )
    if ( (*val)[len - 1] == '\n' )
      (*val) [ len - 1 ] = '\0'; /* cut off newline */
  return TRUE;
}

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
      fgets(buf,sizeof(buf),f);
      if ( preferences_load_parse_param(buf, &key, &val ) ) {
        // if it's not in there, ignore it
        oldval = g_hash_table_lookup ( values, key );
        if ( ! oldval ) {
          g_free(key);
          continue;
        }

        // otherwise change it (you know the type!)
        // if it's a string list do some funky stuff ... yuck... not yet.
        if ( oldval->type == VIK_LAYER_PARAM_STRING_LIST )
          g_error ( "Param strings not implemented in preferences"); // fake it

        newval = layer_data_typed_param_copy_from_string ( oldval->type, val );
        g_hash_table_insert ( values, key, newval );

        g_free(key);

        // change value
      }
    }
    fclose(f);
    f = NULL;
    return TRUE;
  }
  return FALSE;
}

static void preferences_run_setparam ( gpointer notused, guint16 i, VikLayerParamData data, VikLayerParam *params )
{
  if ( params[i].type == VIK_LAYER_PARAM_STRING_LIST )
    g_error ( "Param strings not implemented in preferences"); //fake it
  g_hash_table_insert ( values, (gchar *)(params[i].name), layer_typed_param_data_copy_from_data(params[i].type, data) );
}

/* Allow preferences to be manipulated externally */
void a_preferences_run_setparam ( VikLayerParamData data, VikLayerParam *params )
{
  preferences_run_setparam (NULL, 0, data, params);
}

static VikLayerParamData preferences_run_getparam ( gpointer notused, guint16 i )
{
  VikLayerTypedParamData *val = (VikLayerTypedParamData *) g_hash_table_lookup ( values, ((VikLayerParam *)g_ptr_array_index(params,i))->name );
  g_assert ( val != NULL );
  if ( val->type == VIK_LAYER_PARAM_STRING_LIST )
    g_error ( "Param strings not implemented in preferences"); //fake it
  return val->data;
}

/* TRUE on success */
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
      g_assert ( val != NULL );
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
                                (gboolean (*) (gpointer,guint16,VikLayerParamData,gpointer)) preferences_run_setparam,
				NULL /* not used */, contiguous_params,
                                preferences_run_getparam, NULL /* not used */ ) ) {
      a_preferences_save_to_file();
    }
    g_free ( contiguous_params );
}

void a_preferences_register(VikLayerParam *pref, VikLayerParamData defaultval, const gchar *group_key )
{
  /* copy value */
  VikLayerParam *newpref = g_new(VikLayerParam,1);
  *newpref = *pref;
  VikLayerTypedParamData *newval = layer_typed_param_data_copy_from_data(pref->type, defaultval);
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
  values = g_hash_table_new_full ( g_str_hash, g_str_equal, NULL, layer_typed_param_data_free);

  loaded = FALSE;
}

void a_preferences_uninit()
{
  preferences_groups_uninit();

  g_ptr_array_free ( params, TRUE );
  g_hash_table_destroy ( values );
}



VikLayerParamData *a_preferences_get(const gchar *key)
{
  if ( ! loaded ) {
    /* since we can't load the file in a_preferences_init (no params registered yet),
     * do it once before we get the first key. */
    preferences_load_from_file();
    loaded = TRUE;
  }
  return g_hash_table_lookup ( values, key );
}
