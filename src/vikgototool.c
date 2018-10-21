/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "vikgototool.h"
#include "util.h"

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

static GObjectClass *parent_class;

static void goto_tool_finalize ( GObject *gob );
static gchar *goto_tool_get_label ( VikGotoTool *vw );
static DownloadFileOptions *goto_tool_get_download_options ( VikGotoTool *self );

typedef struct _VikGotoToolPrivate VikGotoToolPrivate;

struct _VikGotoToolPrivate
{
  gint   id;
  gchar *label;
};

#define GOTO_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                    VIK_GOTO_TOOL_TYPE,          \
                                    VikGotoToolPrivate))

G_DEFINE_ABSTRACT_TYPE (VikGotoTool, vik_goto_tool, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_ID,
  PROP_LABEL,
};

static void
goto_tool_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  VikGotoTool *self = VIK_GOTO_TOOL (object);
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      priv->id = g_value_get_uint (value);
      g_debug ("VikGotoTool.id: %d", priv->id);
      break;

    case PROP_LABEL:
      g_free (priv->label);
      priv->label = g_value_dup_string (value);
      g_debug ("VikGotoTool.label: %s", priv->label);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
goto_tool_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  VikGotoTool *self = VIK_GOTO_TOOL (object);
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_uint (value, priv->id);
      break;

    case PROP_LABEL:
      g_value_set_string (value, priv->label);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void vik_goto_tool_class_init ( VikGotoToolClass *klass )
{
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = goto_tool_finalize;
  gobject_class->set_property = goto_tool_set_property;
  gobject_class->get_property = goto_tool_get_property;

  pspec = g_param_spec_string ("label",
                               "Label",
                               "Set the label",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   pspec);

  pspec = g_param_spec_uint ("id",
                             "Id of the tool",
                             "Set the id",
                             0  /* minimum value */,
                             G_MAXUINT16 /* maximum value */,
                             0  /* default value */,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   pspec);

  klass->get_label = goto_tool_get_label;
  klass->get_download_options = goto_tool_get_download_options;

  parent_class = g_type_class_peek_parent (klass);

  g_type_class_add_private (klass, sizeof (VikGotoToolPrivate));
}

VikGotoTool *vik_goto_tool_new ()
{
  return VIK_GOTO_TOOL ( g_object_new ( VIK_GOTO_TOOL_TYPE, NULL ) );
}

static void vik_goto_tool_init ( VikGotoTool *self )
{
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE (self);
  priv->label = NULL;
}

static void goto_tool_finalize ( GObject *gob )
{
  VikGotoToolPrivate *priv = GOTO_TOOL_GET_PRIVATE ( gob );
  g_free ( priv->label ); priv->label = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static gchar *goto_tool_get_label ( VikGotoTool *self )
{
  VikGotoToolPrivate *priv = NULL;
  priv = GOTO_TOOL_GET_PRIVATE (self);
  return g_strdup ( priv->label );
}

static DownloadFileOptions *goto_tool_get_download_options ( VikGotoTool *self )
{
  // Default: return NULL
  return NULL;
}

gchar *vik_goto_tool_get_label ( VikGotoTool *self )
{
  return VIK_GOTO_TOOL_GET_CLASS( self )->get_label( self );
}

gchar *vik_goto_tool_get_url_format ( VikGotoTool *self )
{
  return VIK_GOTO_TOOL_GET_CLASS( self )->get_url_format( self );
}

DownloadFileOptions *vik_goto_tool_get_download_options ( VikGotoTool *self )
{
  return VIK_GOTO_TOOL_GET_CLASS( self )->get_download_options( self );
}

gboolean vik_goto_tool_parse_file_for_latlon (VikGotoTool *self, gchar *filename, struct LatLon *ll)
{
  return VIK_GOTO_TOOL_GET_CLASS( self )->parse_file_for_latlon( self, filename, ll );
}

gboolean vik_goto_tool_parse_file_for_candidates (VikGotoTool *self, gchar *filename, GList **candidates)
{
  return VIK_GOTO_TOOL_GET_CLASS( self )->parse_file_for_candidates( self, filename, candidates );
}

/**
 * vik_goto_tool_get_coord:
 *
 * @self:      The #VikGotoTool
 * @vvp:       The #VikViewport
 * @srch_str:  The string to search with
 * @coord:     Returns the top match position for a successful search
 *
 * Returns: An integer value indicating:
 *  0  = search found something
 *  -1 = search place not found by the #VikGotoTool
 *  1  = search unavailable in the #VikGotoTool due to communication issue
 *
 */
int vik_goto_tool_get_coord ( VikGotoTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord )
{
  gchar *tmpname;
  gchar *uri;
  gchar *escaped_srch_str;
  int ret = 0;  /* OK */
  struct LatLon ll;

  escaped_srch_str = g_uri_escape_string(srch_str, NULL, TRUE);

  uri = g_strdup_printf(vik_goto_tool_get_url_format(self), escaped_srch_str);

  tmpname = a_download_uri_to_tmp_file ( uri, vik_goto_tool_get_download_options(self) );

  if ( !tmpname ) {
    // Some kind of download error, so no tmp file
    ret = 1;
    goto done_no_file;
  }

  if (!vik_goto_tool_parse_file_for_latlon(self, tmpname, &ll)) {
    ret = -1;
    goto done;
  }
  vik_coord_load_from_latlon ( coord, vik_viewport_get_coord_mode(vvp), &ll );

done:
  (void)util_remove(tmpname);
done_no_file:
  g_free(tmpname);
  g_free(escaped_srch_str);
  g_free(uri);
  return ret;
}

/**
 * vik_goto_tool_get_candidates
 *
 * @self:       The #VikGotoTool
 * @vvp:        The #VikViewport
 * @srch_str:   The string to search with
 * @candidates: Returns a list of matches
 *
 * Returns: An integer value indicating:
 *  0  = search found something
 *  -1 = search place not found by the #VikGotoTool
 *  1  = search unavailable in the #VikGotoTool due to communication issue
 *
 */
int vik_goto_tool_get_candidates ( VikGotoTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, GList **candidates )
{
  gchar *tmpname;
  gchar *uri;
  gchar *escaped_srch_str;
  int ret = 0;  /* OK */

  escaped_srch_str = g_uri_escape_string(srch_str, NULL, TRUE);

  uri = g_strdup_printf(vik_goto_tool_get_url_format(self), escaped_srch_str);

  tmpname = a_download_uri_to_tmp_file ( uri, vik_goto_tool_get_download_options(self) );

  if ( !tmpname ) {
    // Some kind of download error, so no tmp file
    ret = 1;
    goto done_no_file;
  }

  g_debug("%s: %s", __FILE__, tmpname);
  if (!vik_goto_tool_parse_file_for_candidates(self, tmpname, candidates)) {
    ret = -1;
    goto done;
  }

done:
  (void)util_remove(tmpname);
done_no_file:
  g_free(tmpname);
  g_free(escaped_srch_str);
  g_free(uri);
  return ret;
}

/**
 * vik_goto_tool_free_candidates
 *
 * @data: The candidate object to free
 */
void vik_goto_tool_free_candidate ( gpointer data )
{
  struct VikGotoCandidate *candidate = data;
  g_free ( candidate->description );
  g_free ( candidate );
}
