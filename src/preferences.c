#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>
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

/** application wide parameters. */
static VikLayerParam prefs[] = {
  { "geocaching.username", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("geocaching.com username:"), VIK_LAYER_WIDGET_ENTRY },
  { "geocaching.password", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("geocaching.com password:"), VIK_LAYER_WIDGET_ENTRY },
};

static GPtrArray *params;
static GHashTable *values;

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
  // comments, special characters in viking file format
  if ( buf == NULL || buf[0] == '\0' || buf[0] == '~' || buf[0] == '=' || buf[0] == '#' )
    return FALSE;
  eq_pos = strchr ( buf, '=' );
  if ( ! eq_pos )
    return FALSE;
  *key = g_strndup ( buf, eq_pos - buf );
  *val = eq_pos + 1;
  return TRUE;
}

static gboolean preferences_load_from_file()
{
  gchar *fn = g_build_filename(a_get_viking_dir(), VIKING_PREFS_FILE, NULL);
  FILE *f = fopen(fn, "r");
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

        g_free(key);

        newval = layer_data_typed_param_copy_from_string ( oldval->type, val );
        g_hash_table_insert ( values, key, newval );

        // change value
      }
    }
    fclose(f);
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

static VikLayerParamData preferences_run_getparam ( gpointer notused, guint16 i )
{
  VikLayerTypedParamData *val = (VikLayerTypedParamData *) g_hash_table_lookup ( values, ((VikLayerParam *)g_ptr_array_index(params,i))->name );
  g_assert ( val != NULL );
  if ( val->type == VIK_LAYER_PARAM_STRING_LIST )
    g_error ( "Param strings not implemented in preferences"); //fake it
  return val->data;
}

/* TRUE on success */
static gboolean preferences_save_to_file()
{
  gchar *fn = g_build_filename(a_get_viking_dir(), VIKING_PREFS_FILE, NULL);

  // TODO: error checking
  FILE *f = fopen(fn, "w");
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

    preferences_load_from_file();
    if ( a_uibuilder_properties_factory ( parent, contiguous_params, params_count,
				NULL, 0, // groups, groups_count, // groups? what groups?!
                                (gboolean (*) (gpointer,guint16,VikLayerParamData,gpointer)) preferences_run_setparam,
				NULL /* not used */, contiguous_params,
                                preferences_run_getparam, NULL /* not used */ ) ) {
      preferences_save_to_file();
    }
    g_free ( contiguous_params );
}

void a_preferences_register(VikLayerParam *pref, VikLayerParamData defaultval)
{
  /* copy value */
  VikLayerTypedParamData *newval = layer_typed_param_data_copy_from_data(pref->type, defaultval);

  g_ptr_array_add ( params, pref );
  g_hash_table_insert ( values, (gchar *)pref->name, newval );
}

void a_preferences_init()
{
  /* not copied */
  params = g_ptr_array_new ();

  /* key not copied (same ptr as in pref), actual param data yes */
  values = g_hash_table_new_full ( g_str_hash, g_str_equal, NULL, layer_typed_param_data_free);

  VikLayerParamData tmp;
  tmp.s = "hello world";
  a_preferences_register(prefs, tmp);
  a_preferences_register(prefs+1, tmp);

  /* load from file? */
}

void a_preferences_uninit()
{
  g_ptr_array_free ( params, FALSE );
  g_hash_table_destroy ( values );
}



VikLayerParamData *a_preferences_get(const gchar *key)
{
  return g_hash_table_lookup ( values, key );
}
