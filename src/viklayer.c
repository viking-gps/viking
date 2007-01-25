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

#include "viking.h"
#include "vikradiogroup.h"
#include <string.h>

/* functions common to all layers. */
/* TODO longone: rename interface free -> finalize */

extern VikLayerInterface vik_aggregate_layer_interface;
extern VikLayerInterface vik_trw_layer_interface;
extern VikLayerInterface vik_maps_layer_interface;
extern VikLayerInterface vik_coord_layer_interface;
extern VikLayerInterface vik_georef_layer_interface;
extern VikLayerInterface vik_gps_layer_interface;

enum {
  VL_UPDATE_SIGNAL,
  VL_LAST_SIGNAL
};
static guint layer_signals[VL_LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;

static void layer_class_init ( VikLayerClass *klass );
static void layer_init ( VikLayer *vl );
static void layer_finalize ( VikLayer *vl );
static gboolean layer_properties_factory ( VikLayer *vl, gpointer vp );


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
  g_assert ( l != NULL);
  if ( l->name )
    g_free ( l->name );
  l->name = g_strdup ( new_name );
}

void vik_layer_rename_no_copy ( VikLayer *l, gchar *new_name )
{
  g_assert ( l != NULL);
  if ( l->name )
    g_free ( l->name );
  l->name = new_name;
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
      vik_layer_rename ( VIK_LAYER(new_layer), vik_layer_interfaces[type]->name );
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

VikLayer *vik_layer_copy ( VikLayer *vl, gpointer vp )
{
  if ( vik_layer_interfaces[vl->type]->copy )
  {
    VikLayer *rv = vik_layer_interfaces[vl->type]->copy ( vl, vp );
    if ( rv )
    {
      vik_layer_rename ( rv, vl->name );
      rv->visible = vl->visible;
    }
    return rv;
  }
  else
    return NULL;
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

void vik_layer_post_read ( VikLayer *layer, gpointer vp )
{
  if ( vik_layer_interfaces[layer->type]->post_read )
    vik_layer_interfaces[layer->type]->post_read ( layer, vp );
}

static GtkWidget *properties_widget_new_widget ( VikLayerParam *param, VikLayerParamData data )
{
  GtkWidget *rv = NULL;
  switch ( param->widget_type )
  {
    case VIK_LAYER_WIDGET_COLOR:
      if ( param->type == VIK_LAYER_PARAM_COLOR )
        rv = gtk_color_button_new_with_color ( &(data.c) );
      break;
    case VIK_LAYER_WIDGET_CHECKBUTTON:
      if ( param->type == VIK_LAYER_PARAM_BOOLEAN )
      {
        //rv = gtk_check_button_new_with_label ( //param->title );
        rv = gtk_check_button_new ();
        if ( data.b )
          gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(rv), TRUE );
      }
      break;
    case VIK_LAYER_WIDGET_COMBOBOX:
#ifndef GTK_2_2
      if ( param->type == VIK_LAYER_PARAM_UINT && param->widget_data )
      {
        gchar **pstr = param->widget_data;
        rv = gtk_combo_box_new_text ();
        while ( *pstr )
          gtk_combo_box_append_text ( GTK_COMBO_BOX ( rv ), *(pstr++) );
        if ( param->extra_widget_data ) /* map of alternate uint values for options */
        {
          int i;
          for ( i = 0; ((const char **)param->widget_data)[i]; i++ )
            if ( ((guint *)param->extra_widget_data)[i] == data.u )
            {
              gtk_combo_box_set_active ( GTK_COMBO_BOX(rv), i );
              break;
            }
        }
        gtk_combo_box_set_active ( GTK_COMBO_BOX ( rv ), data.u );
      }
      break;
#endif
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
            if ( g_list_nth_data(param->extra_widget_data, i) == data.u )
            {
              vik_radio_group_set_selected ( VIK_RADIO_GROUP(rv), i );
              break;
            }
        }
        else if ( data.u ) /* zero is already default */
          vik_radio_group_set_selected ( VIK_RADIO_GROUP(rv), data.u );
      }
      break;
    case VIK_LAYER_WIDGET_SPINBUTTON:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE || param->type == VIK_LAYER_PARAM_UINT
           || param->type == VIK_LAYER_PARAM_INT)  && param->widget_data )
      {
        gdouble init_val = (param->type == VIK_LAYER_PARAM_DOUBLE) ? data.d : (param->type == VIK_LAYER_PARAM_UINT ? data.u : data.i);
        VikLayerParamScale *scale = (VikLayerParamScale *) param->widget_data;
        rv = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new( init_val, scale->min, scale->max, scale->step, scale->step, scale->step )), scale->step, scale->digits );
      }
    break;
    case VIK_LAYER_WIDGET_ENTRY:
      if ( param->type == VIK_LAYER_PARAM_STRING )
      {
        rv = gtk_entry_new ();
        gtk_entry_set_text ( GTK_ENTRY(rv), data.s );
      }
      break;
    case VIK_LAYER_WIDGET_FILEENTRY:
      if ( param->type == VIK_LAYER_PARAM_STRING )
      {
        rv = vik_file_entry_new ();
        vik_file_entry_set_filename ( VIK_FILE_ENTRY(rv), data.s );
      }
      break;
    case VIK_LAYER_WIDGET_HSCALE:
      if ( (param->type == VIK_LAYER_PARAM_DOUBLE || param->type == VIK_LAYER_PARAM_UINT
           || param->type == VIK_LAYER_PARAM_INT)  && param->widget_data )
      {
        gdouble init_val = (param->type == VIK_LAYER_PARAM_DOUBLE) ? data.d : (param->type == VIK_LAYER_PARAM_UINT ? data.u : data.i);
        VikLayerParamScale *scale = (VikLayerParamScale *) param->widget_data;
        rv = gtk_hscale_new_with_range ( scale->min, scale->max, scale->step );
        gtk_scale_set_digits ( GTK_SCALE(rv), scale->digits );
        gtk_range_set_value ( GTK_RANGE(rv), init_val );
      }
  }
  return rv;
}

static VikLayerParamData properties_widget_get_value ( GtkWidget *widget, VikLayerParam *param )
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
#ifndef GTK_2_2
      rv.i = gtk_combo_box_get_active ( GTK_COMBO_BOX(widget) );
      if ( rv.i == -1 ) rv.i = 0;
      rv.u = rv.i;
      if ( param->extra_widget_data )
        rv.u = ((guint *)param->extra_widget_data)[rv.u];
      break;
#endif
    case VIK_LAYER_WIDGET_RADIOGROUP:
      rv.u = vik_radio_group_get_selected(VIK_RADIO_GROUP(widget));
      if ( param->extra_widget_data )
        rv.u = (guint *)g_list_nth_data(param->extra_widget_data, rv.u);
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
      rv.s = gtk_entry_get_text ( GTK_ENTRY(widget) );
      break;
    case VIK_LAYER_WIDGET_FILEENTRY:
      rv.s = vik_file_entry_get_filename ( VIK_FILE_ENTRY(widget) );
      break;
    case VIK_LAYER_WIDGET_HSCALE:
      if ( param->type == VIK_LAYER_PARAM_UINT )
        rv.u = (guint32) gtk_range_get_value ( GTK_RANGE(widget) );
      else if ( param->type == VIK_LAYER_PARAM_INT )
        rv.i = (gint32) gtk_range_get_value ( GTK_RANGE(widget) );
      else
        rv.d = gtk_range_get_value ( GTK_RANGE(widget) );
      break;
  }
  return rv;
}

/* false if cancel, true if OK */
/* some would claim this wasn't written to be human-readable. */
static gboolean layer_properties_factory ( VikLayer *vl, gpointer vp )
{
  VikLayerParam *params = vik_layer_interfaces[vl->type]->params;
  guint16 params_count = vik_layer_interfaces[vl->type]->params_count;
  guint16 i, j, widget_count = 0;
  gboolean must_redraw = FALSE;

  if ( ! params )
    return TRUE; /* no params == no options, so all is good */

  for ( i = 0; i < params_count; i++ )
    if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES )
      widget_count++;

  if ( widget_count == 0)
    return FALSE;
  else
  {
    /* create widgets and titles; place in table */
    GtkWidget *dialog = gtk_dialog_new_with_buttons ( "Layer Properties",
                            VIK_GTK_WINDOW_FROM_WIDGET(vp),
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL );
    gint resp;

    gchar **groups = vik_layer_interfaces[vl->type]->params_groups;
    guint8 groups_count = vik_layer_interfaces[vl->type]->params_groups_count;

    GtkWidget *table = NULL;
    GtkWidget **tables = NULL; /* for more than one group */

    GtkWidget *notebook = NULL;
    GtkWidget **widgets = g_malloc ( sizeof(GtkWidget *) * widget_count );

    if ( groups && groups_count )
    {
      guint8 current_group;
      guint16 tab_widget_count;
      notebook = gtk_notebook_new ();
      gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, FALSE, FALSE, 0);
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
      gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);
    }

    for ( i = 0, j = 0; i < params_count; i++ )
    {
      if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES )
      {
        if ( tables )
          table = tables[MAX(0, params[i].group)]; /* round up NOT_IN_GROUP, that's not reasonable here */

        widgets[j] = properties_widget_new_widget ( &(params[i]),
                         vik_layer_interfaces[vl->type]->get_param ( vl, i ) );

        g_assert ( widgets[j] != NULL );

        gtk_table_attach ( GTK_TABLE(table), gtk_label_new(params[i].title), 0, 1, j, j+1, 0, 0, 0, 0 );
        gtk_table_attach ( GTK_TABLE(table), widgets[j], 1, 2, j, j+1, GTK_EXPAND | GTK_FILL, 0, 2, 2 );
        j++;
      }
    }

    gtk_widget_show_all ( dialog );

    resp = gtk_dialog_run (GTK_DIALOG (dialog));
    if ( resp == GTK_RESPONSE_ACCEPT )
    {
      for ( i = 0, j = 0; i < params_count; i++ )
      {
        if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES )
        {
          if ( vik_layer_interfaces[vl->type]->set_param ( vl, i,
              properties_widget_get_value ( widgets[j], &(params[i]) ), vp ) )
            must_redraw = TRUE;
          j++;
        }
      }
      vik_layer_post_read ( vl, vp ); /* update any gc's */

      gtk_widget_destroy ( dialog ); /* hide before redrawing. */
      g_free ( widgets );

      if ( must_redraw )
        vik_layer_emit_update ( vl ); /* if this is a new layer, it won't redraw twice because no on'es listening to this signal. */
      return TRUE; /* user clicked OK */
    }

    if ( tables )
      g_free ( tables );
    gtk_widget_destroy ( dialog );
    g_free ( widgets );
    return FALSE;
  }
}
