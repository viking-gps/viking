/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
 * Created by Quy Tonthat <qtonthat@gmail.com>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_MATH_H
#include "math.h"
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "util.h"
#include "curl_download.h"

#include "vikxmlsearchtool.h"


static void vik_xml_search_tool_class_init ( VikXmlSearchToolClass *klass );
static void vik_xml_search_tool_init ( VikXmlSearchTool *vwd );

static void vik_xml_search_tool_finalize ( GObject *gob );

static int vik_xml_search_tool_get_coord ( VikSearchTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord );

typedef struct _VikXmlSearchToolPrivate VikXmlSearchToolPrivate;

struct _VikXmlSearchToolPrivate
{
  gchar *url_format;
  gchar *lat_path;
  gchar *lon_path;
  
  struct LatLon ll;
};

#define XML_SEARCH_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                        VIK_XML_SEARCH_TOOL_TYPE,          \
                                        VikXmlSearchToolPrivate))

GType vik_xml_search_tool_get_type()
{
  static GType w_type = 0;

  if (!w_type)
  {
    static const GTypeInfo w_info = 
    {
      sizeof (VikXmlSearchToolClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) vik_xml_search_tool_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikXmlSearchTool),
      0,
      (GInstanceInitFunc) vik_xml_search_tool_init,
    };
    w_type = g_type_register_static ( VIK_SEARCH_TOOL_TYPE, "VikXmlSearchTool", &w_info, 0 );
  }

  return w_type;
}

enum
{
  PROP_0,

  PROP_URL_FORMAT,
  PROP_LAT_PATH,
  PROP_LON_PATH,
};

static void
xml_search_tool_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  VikXmlSearchTool *self = VIK_XML_SEARCH_TOOL (object);
  VikXmlSearchToolPrivate *priv = XML_SEARCH_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_URL_FORMAT:
      g_free (priv->url_format);
      priv->url_format = g_value_dup_string (value);
      break;

    case PROP_LAT_PATH:
      g_free (priv->lat_path);
      priv->lat_path = g_value_dup_string (value);
      break;

    case PROP_LON_PATH:
      g_free (priv->lon_path);
      priv->lon_path = g_value_dup_string (value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xml_search_tool_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  VikXmlSearchTool *self = VIK_XML_SEARCH_TOOL (object);
  VikXmlSearchToolPrivate *priv = XML_SEARCH_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_URL_FORMAT:
      g_value_set_string (value, priv->url_format);
      break;

    case PROP_LAT_PATH:
      g_value_set_string (value, priv->lat_path);
      break;

    case PROP_LON_PATH:
      g_value_set_string (value, priv->lon_path);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void vik_xml_search_tool_class_init ( VikXmlSearchToolClass *klass )
{
  GObjectClass *object_class;
  VikSearchToolClass *parent_class;
  GParamSpec *pspec;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = vik_xml_search_tool_finalize;
  object_class->set_property = xml_search_tool_set_property;
  object_class->get_property = xml_search_tool_get_property;


  pspec = g_param_spec_string ("url-format",
                               "URL format",
                               "The format of the URL",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_URL_FORMAT,
                                   pspec);

  pspec = g_param_spec_string ("lat-path",
                               "Lat path",
                               "XPath of the latitude",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_LAT_PATH,
                                   pspec);

  pspec = g_param_spec_string ("lon-path",
                               "Lon path",
                               "XPath of the longitude",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_LON_PATH,
                                   pspec);

  parent_class = VIK_SEARCH_TOOL_CLASS (klass);

  parent_class->get_coord = vik_xml_search_tool_get_coord;

  g_type_class_add_private (klass, sizeof (VikXmlSearchToolPrivate));
}

VikXmlSearchTool *vik_xml_search_tool_new ()
{
  return VIK_XML_SEARCH_TOOL ( g_object_new ( VIK_XML_SEARCH_TOOL_TYPE, "label", "Google", NULL ) );
}

static void vik_xml_search_tool_init ( VikXmlSearchTool *self )
{
  VikXmlSearchToolPrivate *priv = XML_SEARCH_TOOL_GET_PRIVATE (self);
  priv->url_format = NULL;
  priv->lat_path = NULL;
  priv->lon_path = NULL;
  // 
  priv->ll.lat = NAN;
  priv->ll.lon = NAN;
}

static void vik_xml_search_tool_finalize ( GObject *gob )
{
  G_OBJECT_GET_CLASS(gob)->finalize(gob);
}

static gboolean
stack_is_path (const GSList *stack,
               const gchar  *path)
{
  gboolean equal = TRUE;
  int stack_len = g_list_length(stack);
  int i = 0;
  i = stack_len - 1;
  while (equal == TRUE && i >= 0)
  {
    if (*path != '/')
      equal = FALSE;
    else
      path++;
    const gchar *current = g_list_nth_data(stack, i);
    size_t len = strlen(current);
    if (strncmp(path, current, len) != 0 )
      equal = FALSE;
    else
    {
      path += len;
    }
    i--;
  }
  if (*path != '\0')
    equal = FALSE;
  return equal;
}

/* Called for character data */
/* text is not nul-terminated */
static void
_text (GMarkupParseContext *context,
       const gchar         *text,
       gsize                text_len,  
       gpointer             user_data,
       GError             **error)
{
  VikXmlSearchTool *self = VIK_XML_SEARCH_TOOL (user_data);
  VikXmlSearchToolPrivate *priv = XML_SEARCH_TOOL_GET_PRIVATE (self);
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  gchar *textl = g_strndup(text, text_len);
	if (stack_is_path (stack, priv->lat_path))
	{
    priv->ll.lat = g_ascii_strtod(textl, NULL);
	}
	if (stack_is_path (stack, priv->lon_path))
	{
    priv->ll.lon = g_ascii_strtod(textl, NULL);
	}
  g_free(textl);
}

static gboolean
parse_file_for_latlon(VikXmlSearchTool *self, gchar *filename, struct LatLon *ll)
{
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context;
	GError *error;
	VikXmlSearchToolPrivate *priv = XML_SEARCH_TOOL_GET_PRIVATE (self);

	FILE *file = g_fopen (filename, "r");
	if (file == NULL)
		/* TODO emit warning */
		return FALSE;
	
	/* setup context parse (ie callbacks) */
	xml_parser.start_element = NULL;
	xml_parser.end_element = NULL;
	xml_parser.text = &_text;
	xml_parser.passthrough = NULL;
	xml_parser.error = NULL;
	
	xml_context = g_markup_parse_context_new(&xml_parser, 0, self, NULL);

	/* setup result */
	priv->ll.lat = NAN;
	priv->ll.lon = NAN;
	
	gchar buff[BUFSIZ];
	size_t nb;
	while ((nb = fread (buff, sizeof(gchar), BUFSIZ, file)) > 0)
	{
		if (!g_markup_parse_context_parse(xml_context, buff, nb, &error))
			fprintf(stderr, "%s: parsing error.\n", __FUNCTION__);
	}
	/* cleanup */
	if (!g_markup_parse_context_end_parse(xml_context, &error))
		fprintf(stderr, "%s: errors occurred reading file.\n", __FUNCTION__);
	
	g_markup_parse_context_free(xml_context);
	fclose (file);
  
  if (ll != NULL)
  {
    *ll = priv->ll;
  }
  
  if (isnan(priv->ll.lat) || isnan(priv->ll.lat))
		/* At least one coordinate not found */
		return FALSE;
	else
		return TRUE;
}

static int vik_xml_search_tool_get_coord ( VikSearchTool *object, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord )
{
  FILE *tmp_file;
  int tmp_fd;
  gchar *tmpname;
  gchar *uri;
  gchar *escaped_srch_str;
  int ret = 0;  /* OK */
  struct LatLon ll;

  g_debug("%s: raw search: %s", __FUNCTION__, srch_str);

  escaped_srch_str = uri_escape(srch_str);

  g_debug("%s: escaped search: %s", __FUNCTION__, escaped_srch_str);

  if ((tmp_fd = g_file_open_tmp ("vikxmlsearch.XXXXXX", &tmpname, NULL)) == -1) {
    g_critical(_("couldn't open temp file"));
    exit(1);
  }
  
  VikXmlSearchTool *self = VIK_XML_SEARCH_TOOL (object);
  VikXmlSearchToolPrivate *priv = XML_SEARCH_TOOL_GET_PRIVATE (self);

  tmp_file = fdopen(tmp_fd, "r+");
  uri = g_strdup_printf(priv->url_format, escaped_srch_str);

  /* TODO: curl may not be available */
  if (curl_download_uri(uri, tmp_file, NULL)) {  /* error */
    fclose(tmp_file);
    tmp_file = NULL;
    ret = -1;
    goto done;
  }

  fclose(tmp_file);
  tmp_file = NULL;
  g_debug("%s: %s", __FILE__, tmpname);
  if (!parse_file_for_latlon(self, tmpname, &ll)) {
    ret = -1;
    goto done;
  }
  vik_coord_load_from_latlon ( coord, vik_viewport_get_coord_mode(vvp), &ll );

done:
  g_free(escaped_srch_str);
  g_free(uri);
  g_remove(tmpname);
  g_free(tmpname);
  return ret;
}
