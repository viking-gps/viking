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

#include "vikgotoxmltool.h"


static void _goto_xml_tool_class_init ( VikGotoXmlToolClass *klass );
static void _goto_xml_tool_init ( VikGotoXmlTool *self );

static void _goto_xml_tool_finalize ( GObject *gob );

static gchar *_goto_xml_tool_get_url_format ( VikGotoTool *self );
static gboolean _goto_xml_tool_parse_file_for_latlon(VikGotoTool *self, gchar *filename, struct LatLon *ll);

typedef struct _VikGotoXmlToolPrivate VikGotoXmlToolPrivate;

struct _VikGotoXmlToolPrivate
{
  gchar *url_format;
  gchar *lat_path;
  gchar *lat_attr;
  gchar *lon_path;
  gchar *lon_attr;
  
  struct LatLon ll;
};

#define GOTO_XML_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                        VIK_GOTO_XML_TOOL_TYPE,          \
                                        VikGotoXmlToolPrivate))

GType vik_goto_xml_tool_get_type()
{
  static GType w_type = 0;

  if (!w_type)
  {
    static const GTypeInfo w_info = 
    {
      sizeof (VikGotoXmlToolClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) _goto_xml_tool_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikGotoXmlTool),
      0,
      (GInstanceInitFunc) _goto_xml_tool_init,
    };
    w_type = g_type_register_static ( VIK_GOTO_TOOL_TYPE, "VikGotoXmlTool", &w_info, 0 );
  }

  return w_type;
}

enum
{
  PROP_0,

  PROP_URL_FORMAT,
  PROP_LAT_PATH,
  PROP_LAT_ATTR,
  PROP_LON_PATH,
  PROP_LON_ATTR,
};

static void
_goto_xml_tool_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  VikGotoXmlTool *self = VIK_GOTO_XML_TOOL (object);
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  gchar **splitted = NULL;

  switch (property_id)
    {
    case PROP_URL_FORMAT:
      g_free (priv->url_format);
      priv->url_format = g_value_dup_string (value);
      break;

    case PROP_LAT_PATH:
      splitted = g_strsplit (g_value_get_string (value), "@", 2);
      g_free (priv->lat_path);
      priv->lat_path = splitted[0];
      if (splitted[1])
      {
        g_object_set (object, "lat-attr", splitted[1], NULL);
        g_free (splitted[1]);
      }
      /* only free the tab, not the strings */
      g_free (splitted);
      splitted = NULL;
      break;

    case PROP_LAT_ATTR:
      /* Avoid to overwrite XPATH value */
      /* NB: This disable future overwriting,
         but as property is CONSTRUCT_ONLY there is no matter */
      if (!priv->lat_attr || g_value_get_string (value))
      {
        g_free (priv->lat_attr);
        priv->lat_attr = g_value_dup_string (value);
      }
      break;

    case PROP_LON_PATH:
      splitted = g_strsplit (g_value_get_string (value), "@", 2);
      g_free (priv->lon_path);
      priv->lon_path = splitted[0];
      if (splitted[1])
      {
        g_object_set (object, "lon-attr", splitted[1], NULL);
        g_free (splitted[1]);
      }
      /* only free the tab, not the strings */
      g_free (splitted);
      splitted = NULL;
      break;

    case PROP_LON_ATTR:
      /* Avoid to overwrite XPATH value */
      /* NB: This disable future overwriting,
         but as property is CONSTRUCT_ONLY there is no matter */
      if (!priv->lon_attr || g_value_get_string (value))
      {
        g_free (priv->lon_attr);
        priv->lon_attr = g_value_dup_string (value);
      }
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
_goto_xml_tool_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  VikGotoXmlTool *self = VIK_GOTO_XML_TOOL (object);
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_URL_FORMAT:
      g_value_set_string (value, priv->url_format);
      break;

    case PROP_LAT_PATH:
      g_value_set_string (value, priv->lat_path);
      break;

    case PROP_LAT_ATTR:
      g_value_set_string (value, priv->lat_attr);
      break;

    case PROP_LON_PATH:
      g_value_set_string (value, priv->lon_path);
      break;

    case PROP_LON_ATTR:
      g_value_set_string (value, priv->lon_attr);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void _goto_xml_tool_class_init ( VikGotoXmlToolClass *klass )
{
  GObjectClass *object_class;
  VikGotoToolClass *parent_class;
  GParamSpec *pspec;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _goto_xml_tool_finalize;
  object_class->set_property = _goto_xml_tool_set_property;
  object_class->get_property = _goto_xml_tool_get_property;


  pspec = g_param_spec_string ("url-format",
                               "URL format",
                               "The format of the URL",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_URL_FORMAT,
                                   pspec);

  pspec = g_param_spec_string ("lat-path",
                               "Latitude path",
                               "XPath of the latitude",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_LAT_PATH,
                                   pspec);

  pspec = g_param_spec_string ("lat-attr",
                               "Latitude attribute",
                               "XML attribute of the latitude",
                               NULL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_LAT_ATTR,
                                   pspec);

  pspec = g_param_spec_string ("lon-path",
                               "Longitude path",
                               "XPath of the longitude",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_LON_PATH,
                                   pspec);

  pspec = g_param_spec_string ("lon-attr",
                               "Longitude attribute",
                               "XML attribute of the longitude",
                               NULL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_LON_ATTR,
                                   pspec);

  parent_class = VIK_GOTO_TOOL_CLASS (klass);

  parent_class->get_url_format = _goto_xml_tool_get_url_format;
  parent_class->parse_file_for_latlon = _goto_xml_tool_parse_file_for_latlon;

  g_type_class_add_private (klass, sizeof (VikGotoXmlToolPrivate));
}

VikGotoXmlTool *vik_goto_xml_tool_new ()
{
  return VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "Google", NULL ) );
}

static void _goto_xml_tool_init ( VikGotoXmlTool *self )
{
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  priv->url_format = NULL;
  priv->lat_path = NULL;
  priv->lat_attr = NULL;
  priv->lon_path = NULL;
  priv->lon_attr = NULL;
  // 
  priv->ll.lat = NAN;
  priv->ll.lon = NAN;
}

static void _goto_xml_tool_finalize ( GObject *gob )
{
  G_OBJECT_GET_CLASS(gob)->finalize(gob);
}

static gboolean
stack_is_path (const GSList *stack,
               const gchar  *path)
{
  gboolean equal = TRUE;
  int stack_len = g_list_length((GList *)stack);
  int i = 0;
  i = stack_len - 1;
  while (equal == TRUE && i >= 0)
  {
    if (*path != '/')
      equal = FALSE;
    else
      path++;
    const gchar *current = g_list_nth_data((GList *)stack, i);
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

/* Called for open tags <foo bar="baz"> */
static void
_start_element (GMarkupParseContext *context,
                const gchar         *element_name,
                const gchar        **attribute_names,
                const gchar        **attribute_values,
                gpointer             user_data,
                GError             **error)
{
  VikGotoXmlTool *self = VIK_GOTO_XML_TOOL (user_data);
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  /* Longitude */
  if (priv->lon_attr != NULL && isnan(priv->ll.lon) && stack_is_path (stack, priv->lon_path))
	{
		int i=0;
		while (attribute_names[i] != NULL)
		{
			if (strcmp (attribute_names[i], priv->lon_attr) == 0)
			{
				priv->ll.lon = g_ascii_strtod(attribute_values[i], NULL);
			}
			i++;
		}
	}
  /* Latitude */
  if (priv->lat_attr != NULL && isnan(priv->ll.lat) && stack_is_path (stack, priv->lat_path))
	{
		int i=0;
		while (attribute_names[i] != NULL)
		{
			if (strcmp (attribute_names[i], priv->lat_attr) == 0)
			{
				priv->ll.lat = g_ascii_strtod(attribute_values[i], NULL);
			}
			i++;
		}
	}
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
  VikGotoXmlTool *self = VIK_GOTO_XML_TOOL (user_data);
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  gchar *textl = g_strndup(text, text_len);
  /* Store only first result */
	if (priv->lat_attr == NULL && isnan(priv->ll.lat) && stack_is_path (stack, priv->lat_path))
	{
    priv->ll.lat = g_ascii_strtod(textl, NULL);
	}
	if (priv->lon_attr == NULL && isnan(priv->ll.lon) && stack_is_path (stack, priv->lon_path))
	{
    priv->ll.lon = g_ascii_strtod(textl, NULL);
	}
  g_free(textl);
}

static gboolean
_goto_xml_tool_parse_file_for_latlon(VikGotoTool *self, gchar *filename, struct LatLon *ll)
{
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context = NULL;
	GError *error = NULL;
	VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  g_return_val_if_fail(priv != NULL, FALSE);

  g_debug ("%s: %s@%s, %s@%s",
           __FUNCTION__,
           priv->lat_path, priv->lat_attr,
           priv->lon_path, priv->lon_attr);

	FILE *file = g_fopen (filename, "r");
	if (file == NULL)
		/* TODO emit warning */
		return FALSE;
	
	/* setup context parse (ie callbacks) */
	if (priv->lat_attr != NULL || priv->lon_attr != NULL)
    // At least one coordinate uses an attribute
    xml_parser.start_element = &_start_element;
  else
    xml_parser.start_element = NULL;
	xml_parser.end_element = NULL;
	if (priv->lat_attr == NULL || priv->lon_attr == NULL)
    // At least one coordinate uses a raw element
    xml_parser.text = &_text;
  else
    xml_parser.text = NULL;
	xml_parser.passthrough = NULL;
	xml_parser.error = NULL;
	
	xml_context = g_markup_parse_context_new(&xml_parser, 0, self, NULL);

	/* setup result */
	priv->ll.lat = NAN;
	priv->ll.lon = NAN;
	
	gchar buff[BUFSIZ];
	size_t nb;
	while (xml_context &&
	       (nb = fread (buff, sizeof(gchar), BUFSIZ, file)) > 0)
	{
		if (!g_markup_parse_context_parse(xml_context, buff, nb, &error))
		{
			fprintf(stderr, "%s: parsing error: %s.\n",
				__FUNCTION__, error->message);
			g_markup_parse_context_free(xml_context);
			xml_context = NULL;
		}
		g_clear_error (&error);
	}
	/* cleanup */
	if (xml_context &&
	    !g_markup_parse_context_end_parse(xml_context, &error))
		fprintf(stderr, "%s: errors occurred while reading file: %s.\n",
			__FUNCTION__, error->message);
	g_clear_error (&error);
	
	if (xml_context)
		g_markup_parse_context_free(xml_context);
	xml_context = NULL;
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

static gchar *
_goto_xml_tool_get_url_format ( VikGotoTool *self )
{
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  g_return_val_if_fail(priv != NULL, NULL);
  return priv->url_format;
}
