/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

static void layer_class_init ( VikLayerClass *klass );
static void layer_init ( VikLayer *vl );
static void layer_finalize ( VikLayer *vl );
static gboolean layer_properties_factory ( VikLayer *vl, VikViewport *vp );


/* TODO longone: rename vik_layer_init -> set_type */

GType vik_layer_get_type ()
{
  static GType vl_type = 0;

  if (!vl_type)
  {
    static const GTypeInfo vl_info =
    {
      sizeof (VikLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) layer_class_init, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikLayer),
      0,
      (GInstanceInitFunc) layer_init /* instance init */
    };
    vl_type = g_type_register_static ( G_TYPE_OBJECT, "VikLayer", &vl_info, 0 );
  }

  return vl_type;
}

static void layer_class_init (VikLayerClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = (GObjectFinalizeFunc) layer_finalize;

  parent_class = g_type_class_peek_parent (klass);

  layer_signals[VL_UPDATE_SIGNAL] = g_signal_new ( "update", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikLayerClass, update), NULL, NULL, 
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

void vik_layer_emit_update ( VikLayer *vl )
{
  if ( vl->visible ) {
    vik_window_set_redraw_trigger(vl);
    g_signal_emit ( G_OBJECT(vl), layer_signals[VL_UPDATE_SIGNAL], 0 );
  }
}

/* should only be done by VikLayersPanel -- need to redraw and record trigger
 * when we make a layer invisible.
 */
void vik_layer_emit_update_although_invisible ( VikLayer *vl )
{
  vik_window_set_redraw_trigger(vl);
  g_signal_emit ( G_OBJECT(vl), layer_signals[VL_UPDATE_SIGNAL], 0 );
}

/* doesn't set the trigger. should be done by aggregate layer when child emits update. */
void vik_layer_emit_update_secondary ( VikLayer *vl )
{
  if ( vl->visible )
    g_signal_emit ( G_OBJECT(vl), layer_signals[VL_UPDATE_SIGNAL], 0 );
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

VikLayerInterface *vik_layer_get_interface ( gint type )
{
  g_assert ( type < VIK_LAYER_NUM_TYPES );
  return vik_layer_interfaces[type];
}

static void layer_init ( VikLayer *vl )
{
  vl->visible = TRUE;
  vl->name = NULL;
  vl->realized = FALSE;
}

void vik_layer_init ( VikLayer *vl, gint type )
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

VikLayer *vik_layer_create ( gint type, gpointer vp, GtkWindow *w, gboolean interactive )
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
  return layer_properties_factory ( layer, vp );
}

void vik_layer_draw ( VikLayer *l, gpointer data )
{
  if ( l->visible )
    if ( vik_layer_interfaces[l->type]->draw )
      vik_layer_interfaces[l->type]->draw ( l, data );
}

void vik_layer_change_coord_mode ( VikLayer *l, VikCoordMode mode )
{
  if ( vik_layer_interfaces[l->type]->change_coord_mode )
    vik_layer_interfaces[l->type]->change_coord_mode ( l, mode );
}

typedef struct {
  gint layer_type;
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
      d = get_param(vl, i);
      switch ( params[i].type )
      {
      case VIK_LAYER_PARAM_STRING: 
	vlm_append(d.s, strlen(d.s));
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
      switch ( params[i].type )
      {
      case VIK_LAYER_PARAM_STRING: 
	s = g_malloc(vlm_size + 1);
	s[vlm_size]=0;
	vlm_read(s);
	d.s = s;
	set_param(vl, i, d, vvp);
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
        set_param ( vl, i, d, vvp );
        /* don't free -- string list is responsibility of the layer */

        break;
        }
      default:
	vlm_read(&d);
	set_param(vl, i, d, vvp);
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

static void layer_finalize ( VikLayer *vl )
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

gboolean vik_layer_sublayer_add_menu_items ( VikLayer *l, GtkMenu *menu, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter )
{
  if ( vik_layer_interfaces[l->type]->sublayer_add_menu_items )
    return vik_layer_interfaces[l->type]->sublayer_add_menu_items ( l, menu, vlp, subtype, sublayer, iter );
  return FALSE;
}


const gchar *vik_layer_sublayer_rename_request ( VikLayer *l, const gchar *newname, gpointer vlp, gint subtype, gpointer sublayer, GtkTreeIter *iter )
{
  if ( vik_layer_interfaces[l->type]->sublayer_rename_request )
    return vik_layer_interfaces[l->type]->sublayer_rename_request ( l, newname, vlp, subtype, sublayer, iter );
  return NULL;
}

GdkPixbuf *vik_layer_load_icon ( gint type )
{
  g_assert ( type < VIK_LAYER_NUM_TYPES );
  if ( vik_layer_interfaces[type]->icon )
    return gdk_pixbuf_from_pixdata ( vik_layer_interfaces[type]->icon, FALSE, NULL );
  return NULL;
}

gboolean vik_layer_set_param ( VikLayer *layer, guint16 id, VikLayerParamData data, gpointer vp )
{
  if ( vik_layer_interfaces[layer->type]->set_param )
    return vik_layer_interfaces[layer->type]->set_param ( layer, id, data, vp );
  return FALSE;
}

void vik_layer_post_read ( VikLayer *layer, VikViewport *vp, gboolean from_file )
{
  if ( vik_layer_interfaces[layer->type]->post_read )
    vik_layer_interfaces[layer->type]->post_read ( layer, vp, from_file );
}

static gboolean layer_properties_factory ( VikLayer *vl, VikViewport *vp )
{
  switch ( a_uibuilder_properties_factory ( VIK_GTK_WINDOW_FROM_WIDGET(vp),
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

