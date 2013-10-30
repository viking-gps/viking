/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
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

#include <glib/gi18n.h>

#include "viking.h"
#include <string.h>
#include <stdlib.h>
#include "viklayer_defaults.h"

/* functions common to all layers. */
/* TODO longone: rename interface free -> finalize */

extern VikLayerInterface vik_aggregate_layer_interface;
extern VikLayerInterface vik_trw_layer_interface;
extern VikLayerInterface vik_maps_layer_interface;
extern VikLayerInterface vik_coord_layer_interface;
extern VikLayerInterface vik_georef_layer_interface;
extern VikLayerInterface vik_gps_layer_interface;
extern VikLayerInterface vik_dem_layer_interface;

enum {
  VL_UPDATE_SIGNAL,
  VL_LAST_SIGNAL
};
static guint layer_signals[VL_LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;

static void vik_layer_finalize ( VikLayer *vl );
static gboolean vik_layer_properties_factory ( VikLayer *vl, VikViewport *vp );
static gboolean layer_defaults_register ( VikLayerTypeEnum type );

G_DEFINE_TYPE (VikLayer, vik_layer, G_TYPE_OBJECT)

static void vik_layer_class_init (VikLayerClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = (GObjectFinalizeFunc) vik_layer_finalize;

  parent_class = g_type_class_peek_parent (klass);

  layer_signals[VL_UPDATE_SIGNAL] = g_signal_new ( "update", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikLayerClass, update), NULL, NULL, 
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  // Register all parameter defaults, early in the start up sequence
  VikLayerTypeEnum layer;
  for ( layer = 0; layer < VIK_LAYER_NUM_TYPES; layer++ )
    // ATM ignore the returned value
    layer_defaults_register ( layer );
}

/**
 * Invoke the actual drawing via signal method
 */
static gboolean idle_draw ( VikLayer *vl )
{
  g_signal_emit ( G_OBJECT(vl), layer_signals[VL_UPDATE_SIGNAL], 0 );
  return FALSE; // Nothing else to do
}

/**
 * Draw specified layer
 */
void vik_layer_emit_update ( VikLayer *vl )
{
  if ( vl->visible && vl->realized ) {
    vik_window_set_redraw_trigger(vl);

    // Only ever draw when there is time to do so
    if ( g_thread_self() != vik_window_get_thread (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vl))) )
      // Drawing requested from another (background) thread, so handle via the gdk thread method
      gdk_threads_add_idle ( (GSourceFunc) idle_draw, vl );
    else
      g_idle_add ( (GSourceFunc) idle_draw, vl );
  }
}

/**
 * should only be done by VikLayersPanel (hence never used from the background)
 * need to redraw and record trigger when we make a layer invisible.
 */
void vik_layer_emit_update_although_invisible ( VikLayer *vl )
{
  vik_window_set_redraw_trigger(vl);
  g_idle_add ( (GSourceFunc) idle_draw, vl );
}

/* doesn't set the trigger. should be done by aggregate layer when child emits update. */
void vik_layer_emit_update_secondary ( VikLayer *vl )
{
  if ( vl->visible )
    // TODO: this can used from the background - eg in acquire
    //       so will need to flow background update status through too
    g_idle_add ( (GSourceFunc) idle_draw, vl );
}

static VikLayerInterface *vik_layer_interfaces[VIK_LAYER_NUM_TYPES] = {
  &vik_aggregate_layer_interface,
  &vik_trw_layer_interface,
  &vik_coord_layer_interface,
  &vik_georef_layer_interface,
  &vik_gps_layer_interface,
  &vik_maps_layer_interface,
  &vik_dem_layer_interface,
};

VikLayerInterface *vik_layer_get_interface ( VikLayerTypeEnum type )
{
  g_assert ( type < VIK_LAYER_NUM_TYPES );
  return vik_layer_interfaces[type];
}

/**
 * Store default values for this layer
 *
 * Returns whether any parameters where registered
 */
static gboolean layer_defaults_register ( VikLayerTypeEnum type )
{
  // See if any parameters
  VikLayerParam *params = vik_layer_interfaces[type]->params;
  if ( ! params )
    return FALSE;

  gboolean answer = FALSE; // Incase all parameters are 'not in properties'
  guint16 params_count = vik_layer_interfaces[type]->params_count;
  guint16 i;
  // Process each parameter
  for ( i = 0; i < params_count; i++ ) {
    if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES ) {
      if ( params[i].default_value ) {
        VikLayerParamData paramd = params[i].default_value();
        a_layer_defaults_register ( &params[i], paramd, vik_layer_interfaces[type]->fixed_layer_name );
        answer = TRUE;
      }
    }
  }

  return answer;
}

static void vik_layer_init ( VikLayer *vl )
{
  vl->visible = TRUE;
  vl->name = NULL;
  vl->realized = FALSE;
}

void vik_layer_set_type ( VikLayer *vl, VikLayerTypeEnum type )
{
  vl->type = type;
}

/* frees old name */
void vik_layer_rename ( VikLayer *l, const gchar *new_name )
{
  g_assert ( l != NULL );
  g_assert ( new_name != NULL );
  g_free ( l->name );
  l->name = g_strdup ( new_name );
}

void vik_layer_rename_no_copy ( VikLayer *l, gchar *new_name )
{
  g_assert ( l != NULL );
  g_assert ( new_name != NULL );
  g_free ( l->name );
  l->name = new_name;
}

const gchar *vik_layer_get_name ( VikLayer *l )
{
  g_assert ( l != NULL);
  return l->name;
}

VikLayer *vik_layer_create ( VikLayerTypeEnum type, gpointer vp, GtkWindow *w, gboolean interactive )
{
  VikLayer *new_layer = NULL;
  g_assert ( type < VIK_LAYER_NUM_TYPES );

  new_layer = vik_layer_interfaces[type]->create ( vp );

  g_assert ( new_layer != NULL );

  if ( interactive )
  {
    if ( vik_layer_properties ( new_layer, vp ) )
      /* We translate the name here */
      /* in order to avoid translating name set by user */
      vik_layer_rename ( VIK_LAYER(new_layer), _(vik_layer_interfaces[type]->name) );
    else
    {
      g_object_unref ( G_OBJECT(new_layer) ); /* cancel that */
      new_layer = NULL;
    }
  }
  return new_layer;
}

/* returns TRUE if OK was pressed */
gboolean vik_layer_properties ( VikLayer *layer, gpointer vp )
{
  if ( vik_layer_interfaces[layer->type]->properties )
    return vik_layer_interfaces[layer->type]->properties ( layer, vp );
  return vik_layer_properties_factory ( layer, vp );
}

void vik_layer_draw ( VikLayer *l, VikViewport *vp )
{
  if ( l->visible )
    if ( vik_layer_interfaces[l->type]->draw )
      vik_layer_interfaces[l->type]->draw ( l, vp );
}

void vik_layer_change_coord_mode ( VikLayer *l, VikCoordMode mode )
{
  if ( vik_layer_interfaces[l->type]->change_coord_mode )
    vik_layer_interfaces[l->type]->change_coord_mode ( l, mode );
}

typedef struct {
  VikLayerTypeEnum layer_type;
  gint len;
  guint8 data[0];
} header_t;

void vik_layer_marshall ( VikLayer *vl, guint8 **data, gint *len )
{
  header_t *header;
  if ( vl && vik_layer_interfaces[vl->type]->marshall ) {
    vik_layer_interfaces[vl->type]->marshall ( vl, data, len );
    if (*data) {
      header = g_malloc(*len + sizeof(*header));
      header->layer_type = vl->type;
      header->len = *len;
      memcpy(header->data, *data, *len);
      g_free(*data);
      *data = (guint8 *)header;
      *len = *len + sizeof(*header);
    }
  } else {
    *data = NULL;
  }
}

void vik_layer_marshall_params ( VikLayer *vl, guint8 **data, gint *datalen )
{
  VikLayerParam *params = vik_layer_get_interface(vl->type)->params;
  VikLayerFuncGetParam get_param = vik_layer_get_interface(vl->type)->get_param;
  GByteArray* b = g_byte_array_new ();
  gint len;

#define vlm_append(obj, sz) 	\
  len = (sz);    		\
  g_byte_array_append ( b, (guint8 *)&len, sizeof(len) );	\
  g_byte_array_append ( b, (guint8 *)(obj), len );

  vlm_append(vl->name, strlen(vl->name));

  if ( params && get_param )
  {
    VikLayerParamData d;
    guint16 i, params_count = vik_layer_get_interface(vl->type)->params_count;
    for ( i = 0; i < params_count; i++ )
    {
      g_debug("%s: %s", __FUNCTION__, params[i].name);
      d = get_param(vl, i, FALSE);
      switch ( params[i].type )
      {
      case VIK_LAYER_PARAM_STRING:
        // Remember need braces as these are macro calls, not single statement functions!
        if ( d.s ) {
          vlm_append(d.s, strlen(d.s));
        }
        else {
          // Need to insert empty string otherwise the unmarshall will get confused
          vlm_append("", 0);
        }
        break;
      /* print out the string list in the array */
      case VIK_LAYER_PARAM_STRING_LIST: {
        GList *list = d.sl;
        
        /* write length of list (# of strings) */
        gint listlen = g_list_length ( list );
        g_byte_array_append ( b, (guint8 *)&listlen, sizeof(listlen) );

        /* write each string */
        while ( list ) {
          gchar *s = (gchar *) list->data;
          vlm_append(s, strlen(s));
          list = list->next;
        }

	break;
      }
      default:
	vlm_append(&d, sizeof(d));
	break;
      }
    }
  }
  
  *data = b->data;
  *datalen = b->len;
  g_byte_array_free ( b, FALSE );

#undef vlm_append
}

void vik_layer_unmarshall_params ( VikLayer *vl, guint8 *data, gint datalen, VikViewport *vvp )
{
  VikLayerParam *params = vik_layer_get_interface(vl->type)->params;
  VikLayerFuncSetParam set_param = vik_layer_get_interface(vl->type)->set_param;
  gchar *s;
  guint8 *b = (guint8 *)data;
  
#define vlm_size (*(gint *)b)
#define vlm_read(obj)				\
  memcpy((obj), b+sizeof(gint), vlm_size);	\
  b += sizeof(gint) + vlm_size;
  
  s = g_malloc(vlm_size + 1);
  s[vlm_size]=0;
  vlm_read(s);
  
  vik_layer_rename(vl, s);
  
  g_free(s);

  if ( params && set_param )
  {
    VikLayerParamData d;
    guint16 i, params_count = vik_layer_get_interface(vl->type)->params_count;
    for ( i = 0; i < params_count; i++ )
    {
      g_debug("%s: %s", __FUNCTION__, params[i].name);
      switch ( params[i].type )
      {
      case VIK_LAYER_PARAM_STRING: 
	s = g_malloc(vlm_size + 1);
	s[vlm_size]=0;
	vlm_read(s);
	d.s = s;
	set_param(vl, i, d, vvp, FALSE);
	g_free(s);
	break;
      case VIK_LAYER_PARAM_STRING_LIST:  {
        gint listlen = vlm_size, j;
        GList *list = NULL;
        b += sizeof(gint); /* skip listlen */;

        for ( j = 0; j < listlen; j++ ) {
          /* get a string */
          s = g_malloc(vlm_size + 1);
	  s[vlm_size]=0;
	  vlm_read(s);
          list = g_list_append ( list, s );
        }
        d.sl = list;
        set_param(vl, i, d, vvp, FALSE);
        /* don't free -- string list is responsibility of the layer */

        break;
        }
      default:
	vlm_read(&d);
	set_param(vl, i, d, vvp, FALSE);
	break;
      }
    }
  }
}

VikLayer *vik_layer_unmarshall ( guint8 *data, gint len, VikViewport *vvp )
{
  header_t *header;

  header = (header_t *)data;
  
  if ( vik_layer_interfaces[header->layer_type]->unmarshall ) {
    return vik_layer_interfaces[header->layer_type]->unmarshall ( header->data, header->len, vvp );
  } else {
    return NULL;
  }
}

static void vik_layer_finalize ( VikLayer *vl )
{
  g_assert ( vl != NULL );
  if ( vik_layer_interfaces[vl->type]->free )
    vik_layer_interfaces[vl->type]->free ( vl );
  if ( vl->name )
    g_free ( vl->name );
  G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(vl));
}

/* sublayer switching */
gboolean vik_layer_sublayer_toggle_visible ( VikLayer *l, gint subtype, gpointer sublayer )
{
  if ( vik_layer_interfaces[l->type]->sublayer_toggle_visible )
    return vik_layer_interfaces[l->type]->sublayer_toggle_visible ( l, subtype, sublayer );
  return TRUE; /* if unknown, will always be visible */
}

gboolean vik_layer_selected ( VikLayer *l, gint subtype, gpointer sublayer, gint type, gpointer vlp )
{
  if ( vik_layer_interfaces[l->type]->layer_selected )
    return vik_layer_interfaces[l->type]->layer_selected ( l, subtype, sublayer, type, vlp );
  /* Since no 'layer_selected' function explicitly turn off here */
  return vik_window_clear_highlight ( (VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(l) );
}

void vik_layer_realize ( VikLayer *l, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  l->vt = vt;
  l->iter = *layer_iter;
  l->realized = TRUE;
  if ( vik_layer_interfaces[l->type]->realize )
    vik_layer_interfaces[l->type]->realize ( l, vt, layer_iter );
}

void vik_layer_set_menu_items_selection(VikLayer *l, guint16 selection)
{
  if ( vik_layer_interfaces[l->type]->set_menu_selection )
    vik_layer_interfaces[l->type]->set_menu_selection ( l, selection );
}

guint16 vik_layer_get_menu_items_selection(VikLayer *l)
{
  if ( vik_layer_interfaces[l->type]->get_menu_selection )
    return(vik_layer_interfaces[l->type]->get_menu_selection (l));
  else
    return(vik_layer_interfaces[l->type]->menu_items_selection);
}

void vik_layer_add_menu_items ( VikLayer *l, GtkMenu *menu, gpointer vlp )
{
  if ( vik_layer_interfaces[l->type]->add_menu_items )
    vik_layer_interfaces[l->type]->add_menu_items ( l, menu, vlp );
}

gboolean vik_layer_sublayer_add_menu_items ( VikLayer *l, GtkMenu *menu, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter, VikViewport *vvp )
{
  if ( vik_layer_interfaces[l->type]->sublayer_add_menu_items )
    return vik_layer_interfaces[l->type]->sublayer_add_menu_items ( l, menu, vlp, subtype, sublayer, iter, vvp );
  return FALSE;
}


const gchar *vik_layer_sublayer_rename_request ( VikLayer *l, const gchar *newname, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter )
{
  if ( vik_layer_interfaces[l->type]->sublayer_rename_request )
    return vik_layer_interfaces[l->type]->sublayer_rename_request ( l, newname, vlp, subtype, sublayer, iter );
  return NULL;
}

const gchar* vik_layer_sublayer_tooltip ( VikLayer *l, gint subtype, gpointer sublayer )
{
  if ( vik_layer_interfaces[l->type]->sublayer_tooltip )
    return vik_layer_interfaces[l->type]->sublayer_tooltip ( l, subtype, sublayer );
  return NULL;
}

const gchar* vik_layer_layer_tooltip ( VikLayer *l )
{
  if ( vik_layer_interfaces[l->type]->layer_tooltip )
    return vik_layer_interfaces[l->type]->layer_tooltip ( l );
  return NULL;
}

GdkPixbuf *vik_layer_load_icon ( VikLayerTypeEnum type )
{
  g_assert ( type < VIK_LAYER_NUM_TYPES );
  if ( vik_layer_interfaces[type]->icon )
    return gdk_pixbuf_from_pixdata ( vik_layer_interfaces[type]->icon, FALSE, NULL );
  return NULL;
}

gboolean vik_layer_set_param ( VikLayer *layer, guint16 id, VikLayerParamData data, gpointer vp, gboolean is_file_operation )
{
  if ( vik_layer_interfaces[layer->type]->set_param )
    return vik_layer_interfaces[layer->type]->set_param ( layer, id, data, vp, is_file_operation );
  return FALSE;
}

void vik_layer_post_read ( VikLayer *layer, VikViewport *vp, gboolean from_file )
{
  if ( vik_layer_interfaces[layer->type]->post_read )
    vik_layer_interfaces[layer->type]->post_read ( layer, vp, from_file );
}

static gboolean vik_layer_properties_factory ( VikLayer *vl, VikViewport *vp )
{
  switch ( a_uibuilder_properties_factory ( _("Layer Properties"),
					    VIK_GTK_WINDOW_FROM_WIDGET(vp),
					    vik_layer_interfaces[vl->type]->params,
					    vik_layer_interfaces[vl->type]->params_count,
					    vik_layer_interfaces[vl->type]->params_groups,
					    vik_layer_interfaces[vl->type]->params_groups_count,
					    (gpointer) vik_layer_interfaces[vl->type]->set_param, 
					    vl, 
					    vp,
					    (gpointer) vik_layer_interfaces[vl->type]->get_param, 
					    vl) ) {
    case 0:
    case 3:
      return FALSE;
      /* redraw (?) */
    case 2:
      vik_layer_post_read ( vl, vp, FALSE ); /* update any gc's */
    default:
      return TRUE;
  }
}

VikLayerTypeEnum vik_layer_type_from_string ( const gchar *str )
{
  VikLayerTypeEnum i;
  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
    if ( strcasecmp ( str, vik_layer_get_interface(i)->fixed_layer_name ) == 0 )
      return i;
  return VIK_LAYER_NUM_TYPES;
}

void vik_layer_typed_param_data_free ( gpointer gp )
{
  VikLayerTypedParamData *val = (VikLayerTypedParamData *)gp;
  switch ( val->type ) {
    case VIK_LAYER_PARAM_STRING:
      if ( val->data.s )
        g_free ( (gpointer)val->data.s );
      break;
    /* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
     * the internals call get_param -- i.e. it should be managed w/in the layer.
     * The value passed by the internals into set_param should also be managed
     * by the layer -- i.e. free'd by the layer.
     */
    case VIK_LAYER_PARAM_STRING_LIST:
      g_warning ("Param strings not implemented"); //fake it
      break;
    default:
      break;
  }
  g_free ( val );
}

VikLayerTypedParamData *vik_layer_typed_param_data_copy_from_data (VikLayerParamType type, VikLayerParamData val) {
  VikLayerTypedParamData *newval = g_new(VikLayerTypedParamData,1);
  newval->data = val;
  newval->type = type;
  switch ( newval->type ) {
    case VIK_LAYER_PARAM_STRING: {
      gchar *s = g_strdup(newval->data.s);
      newval->data.s = s;
      break;
    }
    /* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
     * the internals call get_param -- i.e. it should be managed w/in the layer.
     * The value passed by the internals into set_param should also be managed
     * by the layer -- i.e. free'd by the layer.
     */
    case VIK_LAYER_PARAM_STRING_LIST:
      g_critical ( "Param strings not implemented"); //fake it
      break;
    default:
      break;
  }
  return newval;
}

#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F') )

VikLayerTypedParamData *vik_layer_data_typed_param_copy_from_string ( VikLayerParamType type, const gchar *str )
{
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
    }
  }
  return rv;
}


/**
 * vik_layer_set_defaults:
 *
 * Loop around all parameters for the specified layer to call the function to get the
 *  default value for that parameter
 */
void vik_layer_set_defaults ( VikLayer *vl, VikViewport *vvp )
{
  VikLayerInterface *vli = vik_layer_get_interface ( vl->type );
  const gchar *layer_name = vli->fixed_layer_name;
  VikLayerParamData data;

  int i;
  for ( i = 0; i < vli->params_count; i++ ) {
    // Ensure parameter is for use
    if ( vli->params[i].group > VIK_LAYER_NOT_IN_PROPERTIES ) {
      // ATM can't handle string lists
      // only DEM files uses this currently
      if ( vli->params[i].type != VIK_LAYER_PARAM_STRING_LIST ) {
        data = a_layer_defaults_get ( layer_name, vli->params[i].name, vli->params[i].type );
        vik_layer_set_param ( vl, i, data, vvp, TRUE ); // Possibly come from a file
      }
    }
  }
}
