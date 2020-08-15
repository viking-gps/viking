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
 */
#include "viking.h"
#include "vikgotoxmltool.h"

static void vik_goto_xml_tool_finalize ( GObject *gob );

static gchar *vik_goto_xml_tool_get_url_format ( VikGotoTool *self );
static gboolean vik_goto_xml_tool_parse_file(VikGotoTool *self, gchar *filename);
static gboolean vik_goto_xml_tool_parse_file_for_latlon(VikGotoTool *self, gchar *filename, struct LatLon *ll);
static gboolean vik_goto_xml_tool_parse_file_for_candidates(VikGotoTool *self, gchar *filename, GList **candidates);

typedef struct _VikGotoXmlToolPrivate VikGotoXmlToolPrivate;

struct _VikGotoXmlToolPrivate
{
  gchar *url_format;
  gchar *lat_path;
  gchar *lat_attr;
  gchar *lon_path;
  gchar *lon_attr;
  gchar *desc_path;
  gchar *desc_attr;
  
  struct LatLon ll;
  gchar *description;

  // if not null, load in all candidates
  GList **candidates;
};

G_DEFINE_TYPE_WITH_PRIVATE (VikGotoXmlTool, vik_goto_xml_tool, VIK_GOTO_TOOL_TYPE)
#define GOTO_XML_TOOL_GET_PRIVATE(o) (vik_goto_xml_tool_get_instance_private (VIK_GOTO_XML_TOOL(o)))

enum
{
  PROP_0,

  PROP_URL_FORMAT,
  PROP_LAT_PATH,
  PROP_LAT_ATTR,
  PROP_LON_PATH,
  PROP_LON_ATTR,
  PROP_DESC_PATH,
  PROP_DESC_ATTR,
};

static void
vik_goto_xml_tool_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (object);
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

    case PROP_DESC_PATH:
      splitted = g_strsplit (g_value_get_string (value), "@", 2);
      g_free (priv->desc_path);
      priv->desc_path = splitted[0];
      if (splitted[1])
      {
        g_object_set (object, "desc-attr", splitted[1], NULL);
        g_free (splitted[1]);
      }
      /* only free the tab, not the strings */
      g_free (splitted);
      splitted = NULL;
      break;

    case PROP_DESC_ATTR:
      /* Avoid to overwrite XPATH value */
      /* NB: This disable future overwriting,
         but as property is CONSTRUCT_ONLY there is no matter */
      if (!priv->desc_attr || g_value_get_string (value))
      {
        g_free (priv->desc_attr);
        priv->desc_attr = g_value_dup_string (value);
      }
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_goto_xml_tool_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (object);

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

    case PROP_DESC_PATH:
      g_value_set_string (value, priv->desc_path);
      break;

    case PROP_DESC_ATTR:
      g_value_set_string (value, priv->desc_attr);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_goto_xml_tool_class_init ( VikGotoXmlToolClass *klass )
{
  GObjectClass *object_class;
  VikGotoToolClass *parent_class;
  GParamSpec *pspec;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = vik_goto_xml_tool_finalize;
  object_class->set_property = vik_goto_xml_tool_set_property;
  object_class->get_property = vik_goto_xml_tool_get_property;


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

  pspec = g_param_spec_string ("desc-path",
                               "Description path",
                               "XPath of the description",
                               "<no-set>" /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_DESC_PATH,
                                   pspec);

  pspec = g_param_spec_string ("desc-attr",
                               "Description attribute",
                               "XML attribute of the description",
                               NULL /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_DESC_ATTR,
                                   pspec);

  parent_class = VIK_GOTO_TOOL_CLASS (klass);

  parent_class->get_url_format = vik_goto_xml_tool_get_url_format;
  parent_class->parse_file_for_latlon = vik_goto_xml_tool_parse_file_for_latlon;
  parent_class->parse_file_for_candidates = vik_goto_xml_tool_parse_file_for_candidates;
}

VikGotoXmlTool *
vik_goto_xml_tool_new ()
{
  return VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "Google", NULL ) );
}

static void
vik_goto_xml_tool_init ( VikGotoXmlTool *self )
{
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  priv->url_format = NULL;
  priv->lat_path = NULL;
  priv->lat_attr = NULL;
  priv->lon_path = NULL;
  priv->lon_attr = NULL;
  priv->desc_path = NULL;
  priv->desc_attr = NULL;
  // 
  priv->ll.lat = NAN;
  priv->ll.lon = NAN;
  priv->description = NULL;
  priv->candidates = NULL;
}

static void
vik_goto_xml_tool_finalize ( GObject *gob )
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

/* If a complete entry has been found and we want to get all candidates,
 * then move it to the candidate list */
static void
vik_goto_xml_tool_process_if_complete(VikGotoXmlToolPrivate *priv)
{
    if(!isnan(priv->ll.lon) &&
       !isnan(priv->ll.lat) &&
       priv->description != NULL &&
       priv->candidates != NULL)
    {
        struct VikGotoCandidate *cand = g_malloc(sizeof(struct VikGotoCandidate));
        cand->ll.lon = priv->ll.lon;
        cand->ll.lat = priv->ll.lat;
        cand->description = priv->description;
        *priv->candidates = g_list_prepend(*priv->candidates, cand);

        priv->ll.lon = NAN;
        priv->ll.lat = NAN;
        priv->description = NULL;
    }
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
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (user_data);
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
  /* Description */
  if (priv->desc_attr != NULL && priv->description == NULL && stack_is_path (stack, priv->desc_path))
	{
		int i=0;
		while (attribute_names[i] != NULL)
		{
			if (strcmp (attribute_names[i], priv->desc_attr) == 0)
			{
				priv->description = g_strdup(attribute_values[i]);
			}
			i++;
		}
	}

  vik_goto_xml_tool_process_if_complete(priv);
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
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (user_data);
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
	if (priv->desc_attr == NULL && priv->description == NULL && stack_is_path (stack, priv->desc_path))
	{
    priv->description = g_strdup(textl);
	}

    vik_goto_xml_tool_process_if_complete(priv);

  g_free(textl);
}

static gboolean
vik_goto_xml_tool_parse_file(VikGotoTool *self, gchar *filename)
{
	g_debug("Parse %s", filename);
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context = NULL;
	GError *error = NULL;
	VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  g_return_val_if_fail(priv != NULL, FALSE);

  g_debug ("%s: %s@%s, %s@%s, %s@%s",
           __FUNCTION__,
           priv->lat_path, priv->lat_attr,
           priv->lon_path, priv->lon_attr,
           priv->desc_path, priv->desc_attr);

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
  
    return TRUE;
}

static gboolean
vik_goto_xml_tool_parse_file_for_latlon(VikGotoTool *self, gchar *filename, struct LatLon *ll)
{
  if (!vik_goto_xml_tool_parse_file(self, filename))
    return FALSE;

  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);

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

static gboolean
vik_goto_xml_tool_parse_file_for_candidates(VikGotoTool *self, gchar *filename, GList **candidates)
{
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  priv->candidates = candidates;

  return vik_goto_xml_tool_parse_file(self, filename);
}

static gchar *
vik_goto_xml_tool_get_url_format ( VikGotoTool *self )
{
  VikGotoXmlToolPrivate *priv = GOTO_XML_TOOL_GET_PRIVATE (self);
  g_return_val_if_fail(priv != NULL, NULL);
  return priv->url_format;
}
