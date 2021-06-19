/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2012-2013, Rob Norris <rw_norris@hotmail.com>
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

#include "jpg.h"
#include "gpx.h"
#include "kml.h"
#include "tcx.h"
#include "geojson.h"
#include "babel.h"
#include "gpsmapper.h"
#include "compression.h"
#include "file_magic.h"
#include "vikgpslayer.h"
#include "vikgeocluelayer.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "file.h"
#include "misc/strtod.h"

#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F') )
#define VIK_MAGIC "#VIK"
#define GPX_MAGIC "<?xm"
#define VIK_MAGIC_LEN 4
#define GPX_MAGIC_LEN 4

#define VIKING_FILE_VERSION 1

typedef struct _Stack Stack;

struct _Stack {
  Stack *under;
  gpointer data;
};

static void pop(Stack **stack) {
  Stack *tmp = (*stack)->under;
  g_free ( *stack );
  *stack = tmp;
}

static void push(Stack **stack)
{
  Stack *tmp = g_malloc ( sizeof ( Stack ) );
  tmp->under = *stack;
  *stack = tmp;
}

static gboolean check_magic ( FILE *f, const gchar *magic_number, guint magic_length )
{
  gchar magic[magic_length];
  gboolean rv = FALSE;
  gint8 i;
  if ( fread(magic, 1, sizeof(magic), f) == sizeof(magic) &&
      strncmp(magic, magic_number, sizeof(magic)) == 0 )
    rv = TRUE;
  for ( i = sizeof(magic)-1; i >= 0; i-- ) /* the ol' pushback */
    ungetc(magic[i],f);
  return rv;
}


static gboolean str_starts_with ( const gchar *haystack, const gchar *needle, guint16 len_needle, gboolean must_be_longer )
{
  if ( strlen(haystack) > len_needle - (!must_be_longer) && strncasecmp ( haystack, needle, len_needle ) == 0 )
    return TRUE;
  return FALSE;
}

void file_write_layer_param ( FILE *f, const gchar *name, VikLayerParamType type, VikLayerParamData data ) {
      /* string lists are handled differently. We get a GList (that shouldn't
       * be freed) back for get_param and if it is null we shouldn't write
       * anything at all (otherwise we'd read in a list with an empty string,
       * not an empty string list.
       */
      if ( type == VIK_LAYER_PARAM_STRING_LIST ) {
        if ( data.sl ) {
          GList *iter = (GList *)data.sl;
          while ( iter ) {
            fprintf ( f, "%s=", name );
            fprintf ( f, "%s\n", (gchar *)(iter->data) );
            iter = iter->next;
          }
        }
      } else if ( type != VIK_LAYER_PARAM_PTR && type != VIK_LAYER_PARAM_PTR_DEFAULT ) {
        fprintf ( f, "%s=", name );
        switch ( type )
        {
          case VIK_LAYER_PARAM_DOUBLE: {
  //          char buf[15]; /* locale independent */
  //          fprintf ( f, "%s\n", (char *) g_dtostr (data.d, buf, sizeof (buf)) ); break;
              fprintf ( f, "%f\n", data.d );
              break;
         }
          case VIK_LAYER_PARAM_UINT: fprintf ( f, "%d\n", data.u ); break;
          case VIK_LAYER_PARAM_INT: fprintf ( f, "%d\n", data.i ); break;
          case VIK_LAYER_PARAM_BOOLEAN: fprintf ( f, "%c\n", data.b ? 't' : 'f' ); break;
          case VIK_LAYER_PARAM_STRING: fprintf ( f, "%s\n", data.s ? data.s : "" ); break;
          case VIK_LAYER_PARAM_COLOR: fprintf ( f, "#%.2x%.2x%.2x\n", (int)(data.c.red/256),(int)(data.c.green/256),(int)(data.c.blue/256)); break;
          default: break;
        }
      }
}

static void write_layer_params_and_data ( VikLayer *l, FILE *f, const gchar *dirpath )
{
  VikLayerParam *params = vik_layer_get_interface(l->type)->params;
  VikLayerFuncGetParam get_param = vik_layer_get_interface(l->type)->get_param;

  fprintf ( f, "name=%s\n", l->name ? l->name : "" );
  if ( !l->visible )
    fprintf ( f, "visible=f\n" );

  if ( params && get_param )
  {
    VikLayerParamData data;
    guint16 i, params_count = vik_layer_get_interface(l->type)->params_count;
    for ( i = 0; i < params_count; i++ )
    {
      data = get_param(l, i, TRUE);
      file_write_layer_param(f, params[i].name, params[i].type, data);
    }
  }
  if ( vik_layer_get_interface(l->type)->write_file_data )
  {
    fprintf ( f, "\n\n~LayerData\n" );
    vik_layer_get_interface(l->type)->write_file_data ( l, f, dirpath );
    fprintf ( f, "~EndLayerData\n" );
  }
  /* foreach param:
     write param, and get_value, etc.
     then run layer data, and that's it.
  */
}

static void file_write ( VikAggregateLayer *top, FILE *f, gpointer vp, const gchar *dirpath )
{
  Stack *stack = NULL;
  VikLayer *current_layer;
  struct LatLon ll;
  VikViewportDrawMode mode;
  gchar *modestring = NULL;

  push(&stack);
  stack->data = (gpointer) vik_aggregate_layer_get_children(VIK_AGGREGATE_LAYER(top));
  stack->under = NULL;

  /* crazhy CRAZHY */
  vik_coord_to_latlon ( vik_viewport_get_center ( VIK_VIEWPORT(vp) ), &ll );

  mode = vik_viewport_get_drawmode ( VIK_VIEWPORT(vp) );
  switch ( mode ) {
    case VIK_VIEWPORT_DRAWMODE_UTM: modestring = "utm"; break;
    case VIK_VIEWPORT_DRAWMODE_EXPEDIA: modestring = "expedia"; break;
    case VIK_VIEWPORT_DRAWMODE_MERCATOR: modestring = "mercator"; break;
    case VIK_VIEWPORT_DRAWMODE_LATLON: modestring = "latlon"; break;
    default:
      g_critical("Houston, we've had a problem. mode=%d", mode);
  }

  fprintf ( f, "#VIKING GPS Data file " VIKING_URL "\n" );
  fprintf ( f, "FILE_VERSION=%d\n", VIKING_FILE_VERSION );
  fprintf ( f, "\nxmpp=%f\nympp=%f\nlat=%f\nlon=%f\nmode=%s\ncolor=%s\nhighlightcolor=%s\ndrawscale=%s\ndrawcentermark=%s\ndrawhighlight=%s\n",
      vik_viewport_get_xmpp ( VIK_VIEWPORT(vp) ), vik_viewport_get_ympp ( VIK_VIEWPORT(vp) ), ll.lat, ll.lon,
      modestring, vik_viewport_get_background_color(VIK_VIEWPORT(vp)),
      vik_viewport_get_highlight_color(VIK_VIEWPORT(vp)),
      vik_viewport_get_draw_scale(VIK_VIEWPORT(vp)) ? "t" : "f",
      vik_viewport_get_draw_centermark(VIK_VIEWPORT(vp)) ? "t" : "f",
      vik_viewport_get_draw_highlight(VIK_VIEWPORT(vp)) ? "t" : "f" );

  fprintf ( f, "\n~TopLayer %s\n", vik_layer_get_interface(VIK_LAYER(top)->type)->fixed_layer_name );
  write_layer_params_and_data ( VIK_LAYER(top), f, dirpath );

  while (stack && stack->data)
  {
    current_layer = VIK_LAYER(((GList *)stack->data)->data);
    fprintf ( f, "\n~Layer %s\n", vik_layer_get_interface(current_layer->type)->fixed_layer_name );
    write_layer_params_and_data ( current_layer, f, dirpath );
    if ( current_layer->type == VIK_LAYER_AGGREGATE && !vik_aggregate_layer_is_empty(VIK_AGGREGATE_LAYER(current_layer)) )
    {
      push(&stack);
      stack->data = (gpointer) vik_aggregate_layer_get_children(VIK_AGGREGATE_LAYER(current_layer));
    }
    else if ( current_layer->type == VIK_LAYER_GPS && !vik_gps_layer_is_empty(VIK_GPS_LAYER(current_layer)) )
    {
      push(&stack);
      stack->data = (gpointer) vik_gps_layer_get_children(VIK_GPS_LAYER(current_layer));
    }
    else
    {
      stack->data = (gpointer) ((GList *)stack->data)->next;
      fprintf ( f, "~EndLayer\n\n" );
      while ( stack && (!stack->data) )
      {
        pop(&stack);
        if ( stack )
        {
          stack->data = (gpointer) ((GList *)stack->data)->next;
          fprintf ( f, "~EndLayer\n\n" );
        }
      }
    }
  }

  fprintf ( f, "~EndTopLayer\n\n" );
/*
  get vikaggregatelayer's children (?)
  foreach write ALL params,
  then layer data (IF function exists)
  then endlayer

  impl:
  stack of layers (LIST) we are working on
  when layer->next == NULL ...
  we move on.
*/
}

static void string_list_delete ( gpointer key, gpointer l, gpointer user_data )
{
  /* 20071021 bugfix */
  GList *iter = (GList *) l;
  while ( iter ) {
    g_free ( iter->data );
    iter = iter->next;
  }
  g_list_free ( (GList *) l );
}

static void string_list_set_param (gint i, GList *list, gpointer *layer_and_vp)
{
  VikLayerSetParam vlsp;
  vlsp.id                  = i;
  vlsp.data.sl             = list;
  vlsp.vp                  = layer_and_vp[1];
  vlsp.is_file_operation   = TRUE;
  vlsp.dirpath             = (const gchar*)layer_and_vp[2];

  (void)vik_layer_set_param ( VIK_LAYER(layer_and_vp[0]), &vlsp );
}

/**
 * Read in a Viking file and return how successful the parsing was
 * ATM this will always work, in that even if there are parsing problems
 *  then there will be no new values to override the defaults
 *
 * TODO flow up line number(s) / error messages of problems encountered...
 *
 */
static gboolean file_read ( VikAggregateLayer *top, FILE *f, const gchar *dirpath, VikViewport *vp )
{
  Stack *stack = NULL;
  struct LatLon ll = { 0.0, 0.0 };
  gchar buffer[4096];
  gchar *line;
  guint16 len;
  long line_num = 0;

  VikLayerParam *params = NULL; /* for current layer, so we don't have to keep on looking up interface */
  guint8 params_count = 0;

  GHashTable *string_lists = g_hash_table_new(g_direct_hash,g_direct_equal);

  gboolean successful_read = TRUE;
  gint toplayer_number = 0;

  push(&stack);
  stack->under = NULL;
  stack->data = (gpointer) top;

  while ( fgets ( buffer, 4096, f ) )
  {
    line_num++;

    line = buffer;
    while ( *line == ' ' || *line =='\t' )
      line++;

    if ( line[0] == '#' )
      continue;
    

    len = strlen(line);
    if ( len > 0 && line[len-1] == '\n' )
      line[--len] = '\0';
    if ( len > 0 && line[len-1] == '\r' )
      line[--len] = '\0';

    if ( len == 0 )
      continue;


    if ( line[0] == '~' )
    {
      line++; len--;
      if ( *line == '\0' )
        continue;
      else if ( str_starts_with ( line, "TopLayer ", 9, TRUE ) )
      {
        toplayer_number++;
        // No need to create the Top Layer, values replace the current
        push(&stack);
        stack->data = (gpointer)top;
        params = vik_layer_get_interface(VIK_LAYER(top)->type)->params;
        params_count = vik_layer_get_interface(VIK_LAYER(top)->type)->params_count;
        continue;
      }
      else if ( str_starts_with ( line, "Layer ", 6, TRUE ) )
      {
        if ( ! stack->data )
          continue;
        int parent_type = VIK_LAYER(stack->data)->type;
        if ( (parent_type != VIK_LAYER_AGGREGATE) && (parent_type != VIK_LAYER_GPS) )
        {
          successful_read = FALSE;
          g_warning ( "Line %ld: Layer command inside non-Aggregate Layer (type %d)", line_num, parent_type );
          push(&stack); /* inside INVALID layer */
          stack->data = NULL;
          continue;
        }
        else
        {
          VikLayerTypeEnum type = vik_layer_type_from_string ( line+6 );
          push(&stack);
          if ( type == VIK_LAYER_NUM_TYPES )
          {
            successful_read = FALSE;
            g_warning ( "Line %ld: Unknown type %s", line_num, line+6 );
            stack->data = NULL;
          }
          else if (parent_type == VIK_LAYER_GPS)
          {
            stack->data = (gpointer) vik_gps_layer_get_a_child(VIK_GPS_LAYER(stack->under->data));
            params = vik_layer_get_interface(type)->params;
            params_count = vik_layer_get_interface(type)->params_count;
          }
#ifdef HAVE_LIBGEOCLUE_2
          else if (parent_type == VIK_LAYER_GEOCLUE)
          {
            stack->data = (gpointer) vik_geoclue_layer_get_trw(VIK_GEOCLUE_LAYER(stack->under->data));
            params = vik_layer_get_interface(type)->params;
            params_count = vik_layer_get_interface(type)->params_count;
          }
#endif
          else
          {
            stack->data = (gpointer) vik_layer_create ( type, vp, FALSE );
            params = vik_layer_get_interface(type)->params;
            params_count = vik_layer_get_interface(type)->params_count;
          }
        }
      }
      else if ( str_starts_with ( line, "EndLayer", 8, FALSE ) )
      {
        if ( stack->under == NULL ) {
          successful_read = FALSE;
          g_warning ( "Line %ld: Mismatched ~EndLayer command", line_num );
        }
        else
        {
          /* add any string lists we've accumulated */
          gpointer layer_and_vp[3];
          layer_and_vp[0] = stack->data;
          layer_and_vp[1] = vp;
          layer_and_vp[2] = (gpointer)dirpath;
          g_hash_table_foreach ( string_lists, (GHFunc) string_list_set_param, layer_and_vp );
          g_hash_table_remove_all ( string_lists );

          if ( stack->data && stack->under->data )
          {
            if (VIK_LAYER(stack->under->data)->type == VIK_LAYER_AGGREGATE) {
              vik_aggregate_layer_add_layer ( VIK_AGGREGATE_LAYER(stack->under->data), VIK_LAYER(stack->data), FALSE );
              vik_layer_post_read ( VIK_LAYER(stack->data), vp, TRUE );
            }
            else if (VIK_LAYER(stack->under->data)->type == VIK_LAYER_GPS) {
              /* TODO: anything else needs to be done here ? */
            }
            else {
              successful_read = FALSE;
              g_warning ( "Line %ld: EndLayer command inside non-Aggregate Layer (type %d)", line_num, VIK_LAYER(stack->data)->type );
            }
          }
          pop(&stack);
        }
      }
      else if ( str_starts_with ( line, "LayerData", 9, FALSE ) )
      {
        if ( stack->data && vik_layer_get_interface(VIK_LAYER(stack->data)->type)->read_file_data )
        {
          /* must read until hits ~EndLayerData */
          if ( ! vik_layer_get_interface(VIK_LAYER(stack->data)->type)->read_file_data ( VIK_LAYER(stack->data), f, dirpath ) )
            successful_read = FALSE;
        }
        else
        { /* simply skip layer data over */
          while ( fgets ( buffer, 4096, f ) )
          {
            line_num++;

            line = buffer;

            len = strlen(line);
            if ( len > 0 && line[len-1] == '\n' )
              line[--len] = '\0';
            if ( len > 0 && line[len-1] == '\r' )
              line[--len] = '\0';
            if ( strcasecmp ( line, "~EndLayerData" ) == 0 )
              break;
          }
          continue;
        }
      }
      else if ( str_starts_with ( line, "EndTopLayer", 11, FALSE ) )
      {
        toplayer_number--;
        if ( stack )
          pop(&stack);
      }
      else
      {
        successful_read = FALSE;
        g_warning ( "Line %ld: Unknown tilde command", line_num );
      }
    }
    else
    {
      gint32 eq_pos = -1;
      guint16 i;
      if ( ! stack->data )
        continue;

      for ( i = 0; i < len; i++ )
        if ( line[i] == '=' )
          eq_pos = i;

      if ( stack->under == NULL && eq_pos == 12 && strncasecmp ( line, "FILE_VERSION", eq_pos ) == 0) {
        gint version = strtol(line+13, NULL, 10);
        g_debug ( "%s: reading file version %d", __FUNCTION__, version );
        if ( version > VIKING_FILE_VERSION )
          successful_read = FALSE;
        // However we'll still carry and attempt to read whatever we can
      }
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "xmpp", eq_pos ) == 0) /* "hard coded" params: global & for all layer-types */
        vik_viewport_set_xmpp ( VIK_VIEWPORT(vp), strtod_i8n ( line+5, NULL ) );
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "ympp", eq_pos ) == 0)
        vik_viewport_set_ympp ( VIK_VIEWPORT(vp), strtod_i8n ( line+5, NULL ) );
      else if ( stack->under == NULL && eq_pos == 3 && strncasecmp ( line, "lat", eq_pos ) == 0 )
        ll.lat = strtod_i8n ( line+4, NULL );
      else if ( stack->under == NULL && eq_pos == 3 && strncasecmp ( line, "lon", eq_pos ) == 0 )
        ll.lon = strtod_i8n ( line+4, NULL );
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "mode", eq_pos ) == 0 && strcasecmp ( line+5, "utm" ) == 0)
        vik_viewport_set_drawmode ( VIK_VIEWPORT(vp), VIK_VIEWPORT_DRAWMODE_UTM);
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "mode", eq_pos ) == 0 && strcasecmp ( line+5, "expedia" ) == 0)
        vik_viewport_set_drawmode ( VIK_VIEWPORT(vp), VIK_VIEWPORT_DRAWMODE_EXPEDIA );
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "mode", eq_pos ) == 0 && strcasecmp ( line+5, "google" ) == 0)
      {
        successful_read = FALSE;
        g_warning ( _("Draw mode '%s' no more supported"), "google" );
      }
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "mode", eq_pos ) == 0 && strcasecmp ( line+5, "kh" ) == 0)
      {
        successful_read = FALSE;
        g_warning ( _("Draw mode '%s' no more supported"), "kh" );
      }
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "mode", eq_pos ) == 0 && strcasecmp ( line+5, "mercator" ) == 0)
        vik_viewport_set_drawmode ( VIK_VIEWPORT(vp), VIK_VIEWPORT_DRAWMODE_MERCATOR );
      else if ( stack->under == NULL && eq_pos == 4 && strncasecmp ( line, "mode", eq_pos ) == 0 && strcasecmp ( line+5, "latlon" ) == 0)
        vik_viewport_set_drawmode ( VIK_VIEWPORT(vp), VIK_VIEWPORT_DRAWMODE_LATLON );
      else if ( stack->under == NULL && eq_pos == 5 && strncasecmp ( line, "color", eq_pos ) == 0 )
        vik_viewport_set_background_color ( VIK_VIEWPORT(vp), line+6 );
      else if ( stack->under == NULL && eq_pos == 14 && strncasecmp ( line, "highlightcolor", eq_pos ) == 0 )
        vik_viewport_set_highlight_color ( VIK_VIEWPORT(vp), line+15 );
      else if ( stack->under == NULL && eq_pos == 9 && strncasecmp ( line, "drawscale", eq_pos ) == 0 )
        vik_viewport_set_draw_scale ( VIK_VIEWPORT(vp), TEST_BOOLEAN(line+10) );
      else if ( stack->under == NULL && eq_pos == 14 && strncasecmp ( line, "drawcentermark", eq_pos ) == 0 )
        vik_viewport_set_draw_centermark ( VIK_VIEWPORT(vp), TEST_BOOLEAN(line+15) );
      else if ( stack->under == NULL && eq_pos == 13 && strncasecmp ( line, "drawhighlight", eq_pos ) == 0 )
        vik_viewport_set_draw_highlight ( VIK_VIEWPORT(vp), TEST_BOOLEAN(line+14) );
      else if ( stack->under && eq_pos == 4 && strncasecmp ( line, "name", eq_pos ) == 0 )
        vik_layer_rename ( VIK_LAYER(stack->data), line+5 );
      else if ( eq_pos == 7 && strncasecmp ( line, "visible", eq_pos ) == 0 )
        VIK_LAYER(stack->data)->visible = TEST_BOOLEAN(line+8);
      else if ( eq_pos != -1 && stack->under )
      {
        gboolean found_match = FALSE;

        /* go thru layer params. if len == eq_pos && starts_with jazz, set it. */
        /* also got to check for name and visible. */

        if ( ! params )
        {
          successful_read = FALSE;
          g_warning ( "Line %ld: No options for this kind of layer", line_num );
          continue;
        }

        for ( i = 0; i < params_count; i++ )
          if ( strlen(params[i].name) == eq_pos && strncasecmp ( line, params[i].name, eq_pos ) == 0 )
          {
            VikLayerParamData x;
            line += eq_pos+1;
            if ( params[i].type == VIK_LAYER_PARAM_STRING_LIST ) {
              GList *l = g_list_append ( g_hash_table_lookup ( string_lists, GINT_TO_POINTER ((gint) i) ), 
					 g_strdup(line) );
              g_hash_table_replace ( string_lists, GINT_TO_POINTER ((gint)i), l );
              /* add the value to a list, possibly making a new list.
               * this will be passed to the layer when we read an ~EndLayer */
            } else {
              switch ( params[i].type )
              {
                case VIK_LAYER_PARAM_DOUBLE: x.d = strtod_i8n(line, NULL); break;
                case VIK_LAYER_PARAM_UINT: x.u = strtoul(line, NULL, 10); break;
                case VIK_LAYER_PARAM_INT: x.i = strtol(line, NULL, 10); break;
	        case VIK_LAYER_PARAM_BOOLEAN: x.b = TEST_BOOLEAN(line); break;
                case VIK_LAYER_PARAM_COLOR: memset(&(x.c), 0, sizeof(x.c)); /* default: black */
                                          gdk_color_parse ( line, &(x.c) ); break;
                /* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING */
                default: x.s = line;
              }

              VikLayerSetParam vlsp;
              vlsp.id                  = i;
              vlsp.data                = x;
              vlsp.vp                  = vp;
              vlsp.is_file_operation   = TRUE;
              vlsp.dirpath             = dirpath;
              (void)vik_layer_set_param ( VIK_LAYER(stack->data), &vlsp );
            }
            found_match = TRUE;
            break;
          }
        if ( ! found_match ) {
          // ATM don't flow up this issue because at least one internal parameter has changed from version 1.3
          //   and don't what to worry users about raising such issues
          // TODO Maybe hold old values here - compare the line value against them and if a match
          //       generate a different style of message in the GUI...
          // successful_read = FALSE;
          g_warning ( "Line %ld: Unknown parameter. Line:\n%s", line_num, line );
	}
      }
      else {
        successful_read = FALSE;
        g_warning ( "Line %ld: Invalid parameter or parameter outside of layer.", line_num );
      }
    }
/* could be:
[Layer Type=Bla]
[EndLayer]
[LayerData]
name=this
#comment
*/
  }

  // Missing "~EndTopLayer" at end of file? - e.g. you've been manually messing around with .vik files
  // Really don't think should have more than one toplayer in a file anyway
  //  check at least open and close markers evenly matched
  if ( toplayer_number != 0 ) {
    g_warning ( "%s: Mismatched number of toplayer markers %d", __FUNCTION__, toplayer_number );
    if ( toplayer_number > 0 )
      // Force a pop() otherwise stack->data pointing to wrong place
      if ( stack )
        pop(&stack);
  }

  while ( stack )
  {
    if ( stack->under && stack->under->data && stack->data )
    {
      vik_aggregate_layer_add_layer ( VIK_AGGREGATE_LAYER(stack->under->data), VIK_LAYER(stack->data), FALSE );
      vik_layer_post_read ( VIK_LAYER(stack->data), vp, TRUE );
    }
    pop(&stack);
  }

  if ( ll.lat != 0.0 || ll.lon != 0.0 )
    vik_viewport_set_center_latlon ( VIK_VIEWPORT(vp), &ll, TRUE );

  if ( ( ! VIK_LAYER(top)->visible ) && VIK_LAYER(top)->realized )
    vik_treeview_item_set_visible ( VIK_LAYER(top)->vt, &(VIK_LAYER(top)->iter), FALSE );
  if ( VIK_LAYER(top)->realized )
    vik_treeview_item_set_name ( VIK_LAYER(top)->vt, &(VIK_LAYER(top)->iter), VIK_LAYER(top)->name );

  /* delete anything we've forgotten about -- should only happen when file ends before an EndLayer */
  g_hash_table_foreach ( string_lists, string_list_delete, NULL );
  g_hash_table_destroy ( string_lists );

  return successful_read;
}

/*
read thru file
if "[Layer Type="
  push(&stack)
  new default layer of type (str_to_type) (check interface->name)
if "[EndLayer]"
  VikLayer *vl = stack->data;
  pop(&stack);
  vik_aggregate_layer_add_layer(stack->data, vl);
if "[LayerData]"
  vik_layer_data ( VIK_LAYER_DATA(stack->data), f, vp );

*/

/* ---------------------------------------------------- */

static FILE *xfopen ( const char *fn )
{
  if ( strcmp(fn,"-") == 0 )
    return stdin;
  else
    // NB File names passed into Viking should already be converted into UFT8
    //  by g_win32_get_command_line() in main.c for Windows systems
    return g_fopen(fn, "r");
}

static void xfclose ( FILE *f )
{
  if ( f != stdin && f != stdout ) {
    fclose ( f );
    f = NULL;
  }
}

/*
 * Function to determine if a filename is a 'viking' type file
 */
gboolean check_file_magic_vik ( const gchar *filename )
{
  gboolean result = FALSE;
  FILE *ff = xfopen ( filename );
  if ( ff ) {
    result = check_magic ( ff, VIK_MAGIC, VIK_MAGIC_LEN );
    xfclose ( ff );
  }
  return result;
}

/**
 * append_file_ext:
 *
 * Append a file extension, if not already present.
 *
 * Returns: a newly allocated string
 */
gchar *append_file_ext ( const gchar *filename, VikFileType_t type )
{
  gchar *new_name = NULL;
  const gchar *ext = NULL;

  /* Select an extension */
  switch (type)
  {
  case FILE_TYPE_GPX:
    ext = ".gpx";
    break;
  case FILE_TYPE_KML:
    ext = ".kml";
    break;
  case FILE_TYPE_GEOJSON:
    ext = ".geojson";
    break;
  case FILE_TYPE_GPSMAPPER:
  case FILE_TYPE_GPSPOINT:
  default:
    /* Do nothing, ext already set to NULL */
    break;
  }

  /* Do */
  if ( ext != NULL && ! a_file_check_ext ( filename, ext ) )
    new_name = g_strconcat ( filename, ext, NULL );
  else
    /* Simply duplicate */
    new_name = g_strdup ( filename );

  return new_name;
}

/**
 * a_file_load_stream:
 *
 */
VikLoadType_t a_file_load_stream ( FILE *f,
                                   const gchar *filename,
                                   VikAggregateLayer *top,
                                   VikViewport *vp,
                                   VikTrwLayer *vtl,
                                   gboolean new_layer,
                                   gboolean external,
                                   const gchar *dirpath,
                                   const gchar *name )
{
  VikLoadType_t load_answer = LOAD_TYPE_OTHER_SUCCESS;

  // Attempt loading the primary file type first - our internal .vik file:
  if ( check_magic ( f, VIK_MAGIC, VIK_MAGIC_LEN ) )
  {
    if ( file_read ( top, f, dirpath, vp ) )
      load_answer = LOAD_TYPE_VIK_SUCCESS;
    else
      load_answer = LOAD_TYPE_VIK_FAILURE_NON_FATAL;
  }
  else if ( file_magic_check ( filename, "application/zip", ".zip" ) ) {
    (void)fclose ( f );
    load_answer = uncompress_load_zip_file ( filename, top, vp, vtl, new_layer, external, dirpath );
  }
  else if ( file_magic_check ( filename, "application/x-bzip2", ".bz2" ) ) {
    load_answer = uncompress_load_bzip_file ( filename, top, vp, vtl, new_layer, external );
  }
  else if ( a_jpg_magic_check ( filename ) ) {
    if ( ! a_jpg_load_file ( top, filename, vp ) )
      load_answer = LOAD_TYPE_UNSUPPORTED_FAILURE;
  }
  // NB TCX files are XML
  else if ( a_file_check_ext ( filename, ".tcx" ) && check_magic ( f, GPX_MAGIC, GPX_MAGIC_LEN ) ) {
    if ( !a_tcx_read_file ( top, vp, f, filename ) ) {
      load_answer = LOAD_TYPE_TCX_FAILURE;
    }
  }
  else
  {
	// For all other file types which consist of tracks, routes and/or waypoints,
	//  must be loaded into a new TrackWaypoint layer (hence it be created)
    gboolean success = TRUE; // Detect load failures - mainly to remove the layer created as it's not required

    // Load the file as a new layer as instructed
    gboolean add_new = new_layer;

    // If a layer is specified, then potentially load into that one
    if ( a_vik_get_open_files_in_selected_layer() || !add_new ) {
      add_new = TRUE;
      if ( vtl ) {
        if ( IS_VIK_TRW_LAYER(vtl) ) {
          // Provided that the layer will be visible
          //  (otherwise possibly confusing to load but not display it)
          if ( vik_treeview_item_get_visible_tree (VIK_LAYER(vtl)->vt, &(VIK_LAYER(vtl)->iter)) ) {
            add_new = FALSE;
          }
        }
      }
    }

    if (add_new) {
      vtl = VIK_TRW_LAYER (vik_layer_create ( VIK_LAYER_TRW, vp, FALSE ));
      vik_layer_rename ( VIK_LAYER(vtl), name ? name : a_file_basename ( filename ) );
    }

    // In fact both kml & gpx files start the same as they are in xml
    if ( a_file_check_ext ( filename, ".kml" ) && check_magic ( f, GPX_MAGIC, GPX_MAGIC_LEN ) ) {
      if ( ! ( success = a_kml_read_file ( vtl, f ) ) ) {
        load_answer = LOAD_TYPE_KML_FAILURE;
      }
    }
    // NB use a extension check first, as a GPX file header may have a Byte Order Mark (BOM) in it
    //    - which currently confuses our check_magic function
    else if ( a_file_check_ext ( filename, ".gpx" ) || check_magic ( f, GPX_MAGIC, GPX_MAGIC_LEN ) ) {
      if ( ! ( success = a_gpx_read_file ( vtl, f, dirpath, !add_new ) ) ) {
        load_answer = LOAD_TYPE_GPX_FAILURE;
      }
      if ( load_answer == LOAD_TYPE_OTHER_SUCCESS ) {
	if ( external )
	  // TODO may have to make absolute??
	  trw_layer_replace_external ( vtl, filename );
      }
    }
    else {
      // Try final supported file type
      if ( ! ( success = a_gpspoint_read_file ( vtl, f, dirpath ) ) ) {
        // Failure here means we don't know how to handle the file
        load_answer = LOAD_TYPE_UNSUPPORTED_FAILURE;
      }
    }
    // Clean up when we can't handle the file
    if ( ! success ) {
      // free up layer
      g_object_unref ( vtl );
    }
    else {
      // Complete the setup from the successful load
      vik_layer_post_read ( VIK_LAYER(vtl), vp, TRUE );
      if (add_new) {
        vik_aggregate_layer_add_layer ( top, VIK_LAYER(vtl), FALSE );
      }
      else {
	// Make it more accessible in layers panel
	vik_layer_expand_tree ( VIK_LAYER(vtl) );
      }
      vik_trw_layer_auto_set_view ( vtl, vp );
    }
  }
  return load_answer;
}

/**
 * a_file_load:
 *
 */
VikLoadType_t a_file_load ( VikAggregateLayer *top,
                            VikViewport *vp,
                            VikTrwLayer *vtl,
                            const gchar *filename_or_uri,
                            gboolean new_layer,
                            gboolean external,
                            const gchar *name )
{
  g_return_val_if_fail ( vp != NULL, LOAD_TYPE_READ_FAILURE );

  char *filename = (char *)filename_or_uri;
  if (strncmp(filename, "file://", 7) == 0) {
    // Consider replacing this with:
    // filename = g_filename_from_uri ( entry, NULL, NULL );
    // Since this doesn't support URIs properly (i.e. will failure if is it has %20 characters in it)
    filename = filename + 7;
    g_debug ( "Loading file %s from URI %s", filename, filename_or_uri );
  }
  FILE *f = xfopen ( filename );

  if ( ! f )
    return LOAD_TYPE_READ_FAILURE;

  gchar *absolute = file_realpath_dup ( filename );
  gchar *dirpath = NULL;
  if ( absolute )
    dirpath = g_path_get_dirname ( absolute );
  g_free ( absolute );

  VikLoadType_t load_answer = a_file_load_stream ( f, filename, top, vp, vtl, new_layer, external, dirpath, name );

  g_free ( dirpath );
  xfclose(f);
  return load_answer;
}

gboolean a_file_save ( VikAggregateLayer *top, gpointer vp, const gchar *filename )
{
  FILE *f;

  if (strncmp(filename, "file://", 7) == 0)
    filename = filename + 7;

  f = g_fopen(filename, "w");

  if ( ! f )
    return FALSE;

  // Enable relative paths in .vik files to work
  gchar *cwd = g_get_current_dir();
  gchar *dir = g_path_get_dirname ( filename );
  if ( dir ) {
    if ( g_chdir ( dir ) ) {
      g_warning ( "Could not change directory to %s", dir );
    }
  }

  file_write ( top, f, vp, dir );
  g_free (dir);

  // Restore previous working directory
  if ( cwd ) {
    if ( g_chdir ( cwd ) ) {
      g_warning ( "Could not return to directory %s", cwd );
    }
    g_free (cwd);
  }

  fclose(f);
  f = NULL;

  return TRUE;
}


/* example: 
     gboolean is_gpx = a_file_check_ext ( "a/b/c.gpx", ".gpx" );
*/
gboolean a_file_check_ext ( const gchar *filename, const gchar *fileext )
{
  g_return_val_if_fail ( filename != NULL, FALSE );
  g_return_val_if_fail ( fileext && fileext[0]=='.', FALSE );
  const gchar *basename = a_file_basename(filename);
  if (!basename)
    return FALSE;

  const char * dot = strrchr(basename, '.');
  if (dot && !strcmp(dot, fileext))
    return TRUE;

  return FALSE;
}

gboolean kml_export ( VikTrwLayer *vtl, const gchar *filename )
{
  gchar *ustr = NULL;
  switch ( a_vik_get_kml_export_units () ) {
  case VIK_KML_EXPORT_UNITS_STATUTE:  ustr = ""; break;
  case VIK_KML_EXPORT_UNITS_NAUTICAL: ustr = ",units=n"; break;
  default: ustr = ",units=m"; break;
  }
  gchar *babelargs = g_strdup_printf ("-o kml%s,track=%d,points=%d",
                                      ustr, a_vik_get_kml_export_track(), a_vik_get_kml_export_points() );
  gboolean ans = a_babel_convert_to ( vtl, NULL, babelargs, filename, NULL, NULL );
  g_free ( babelargs );
  return ans;
}

/**
 * a_file_export:
 * @vtl: The TrackWaypoint to export data from
 * @filename: The name of the file to be written
 * @file_type: Choose one of the supported file types for the export
 * @trk: If specified then only export this track rather than the whole layer
 * @write_hidden: Whether to write invisible items
 *
 * A general export command to convert from Viking TRW layer data to an external supported format.
 * The write_hidden option is provided mainly to be able to transfer selected items when uploading to a GPS
 */
gboolean a_file_export ( VikTrwLayer *vtl, const gchar *filename, VikFileType_t file_type, VikTrack *trk, gboolean write_hidden )
{
  g_return_val_if_fail ( vtl != NULL, FALSE );

  GpxWritingOptions options = { FALSE, FALSE, write_hidden, FALSE, vik_trw_layer_get_gpx_version(vtl) };
  FILE *f = g_fopen ( filename, "w" );
  if ( f )
  {
    gboolean result = TRUE;
    gchar *dirpath = g_path_get_dirname ( filename );

    if ( trk ) {
      switch ( file_type ) {
        case FILE_TYPE_GPX:
          // trk defined so can set the option
          options.is_route = trk->is_route;
          a_gpx_write_track_file ( trk, f, &options );
          break;
        default:
          g_critical("Houston, we've had a problem. file_type=%d", file_type);
      }
    } else {
      switch ( file_type ) {
        case FILE_TYPE_GPSMAPPER:
          a_gpsmapper_write_file ( vtl, f );
          break;
        case FILE_TYPE_GPX:
          a_gpx_write_file ( vtl, f, &options, dirpath );
          break;
        case FILE_TYPE_GPSPOINT:
          a_gpspoint_write_file ( vtl, f, dirpath );
          break;
        case FILE_TYPE_GEOJSON:
          result = a_geojson_write_file ( vtl, f );
          break;
        case FILE_TYPE_KML:
          fclose ( f );
          return kml_export ( vtl, filename );
          break;
        default:
          g_critical("Houston, we've had a problem. file_type=%d", file_type);
      }
    }
    g_free ( dirpath );
    fclose ( f );
    return result;
  }
  return FALSE;
}

/**
 * a_file_export_babel:
 */
gboolean a_file_export_babel ( VikTrwLayer *vtl, const gchar *filename, const gchar *format,
                               gboolean tracks, gboolean routes, gboolean waypoints, const gchar *suboptions )
{
  gchar *sopts = NULL;
  if ( suboptions && strlen(suboptions) > 0 ) {
    // Ensure there is a starting comma
    if ( suboptions[0] != ',' )
      sopts = g_strdup_printf ( ",%s", suboptions );
    else
      sopts = g_strdup ( ",%s" );
  } else
    sopts = g_strdup ( "" );

  gchar *args = g_strdup_printf("%s %s %s -o %s%s",
                                tracks ? "-t" : "",
                                routes ? "-r" : "",
                                waypoints ? "-w" : "",
                                format, sopts);
  gboolean result = a_babel_convert_to ( vtl, NULL, args, filename, NULL, NULL );
  g_free(sopts);
  g_free(args);
  return result;
}
