#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkbuilder.h>

#include "vikwebtoolcenter.h"

static gchar *class_name = NULL;
GType gtype = 0;
GParameter *parameters = NULL;
gint nb_parameters = 0;

/* Called for open tags <foo bar="baz"> */
static void
_start_element (GMarkupParseContext *context,
                const gchar         *element_name,
                const gchar        **attribute_names,
                const gchar        **attribute_values,
                gpointer             user_data,
                GError             **error)
{
	g_debug(__FUNCTION__);
	if (strcmp(element_name, "object") == 0)
	{
		class_name = g_strdup(attribute_values[0]);
		gtype = g_type_from_name (class_name);
		g_debug ("%s -> %d", class_name, gtype);
	}
	if (strcmp(element_name, "property") == 0)
	{
		if (attribute_values[0] && attribute_values[1] && gtype != 0)
		{
			g_debug("%s=%s", attribute_values[0], attribute_values[1]);
			nb_parameters++;
			parameters = g_realloc(parameters, sizeof(GParameter)*nb_parameters);
			/* parameter name */
			parameters[nb_parameters-1].name = g_strdup(attribute_values[0]);
			/* parameter value */
			GObjectClass *oclass;
			oclass = g_type_class_ref (gtype);
			g_assert (oclass != NULL);
			GParamSpec *pspec;
			pspec = g_object_class_find_property (G_OBJECT_CLASS (oclass),
                                            parameters[nb_parameters-1].name);
			if (!pspec)
			{
				g_warning ("Unknown property: %s.%s",
				g_type_name (gtype), parameters[nb_parameters-1].name);
				/* FIXME free unused array item */
				return;
			}
			g_debug("Expected type: %s", g_type_name (pspec->value_type));
			memset (&(parameters[nb_parameters-1].value), 0, sizeof(GValue));
			gtk_builder_value_from_string_type(NULL, pspec->value_type, attribute_values[1], &(parameters[nb_parameters-1].value), NULL);
		}
	}
}

/* Called for close tags </foo> */
static void
_end_element (GMarkupParseContext *context,
              const gchar         *element_name,
              gpointer             user_data,
              GError             **error)
{
	g_debug(__FUNCTION__);
	gpointer object = NULL;
	if (strcmp(element_name, "object") == 0 && gtype != 0)
	{
		object = g_object_newv(gtype, nb_parameters, parameters);
		g_free (parameters);
		parameters = NULL;
		nb_parameters = 0;
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
	g_debug(__FUNCTION__);
}

int parse(gchar *text)
{
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context;
	GError *error;
	
	/* setup context parse (ie callbacks) */
	xml_parser.start_element = &_start_element;
	xml_parser.end_element = &_end_element;
	xml_parser.text = &_text;
	xml_parser.passthrough = NULL;
	xml_parser.error = NULL;
	
	xml_context = g_markup_parse_context_new(&xml_parser, 0, NULL, NULL);
	
	if (!g_markup_parse_context_parse(xml_context, text, strlen(text), &error))
		printf("read_xml() : parsing error.\n");
	/* cleanup */
	if (!g_markup_parse_context_end_parse(xml_context, &error))
		printf("read_xml() : errors occurred reading file.\n");
	
	g_markup_parse_context_free(xml_context);
	
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	g_type_init ();

	/* Load some specific types */
	VIK_WEBTOOL_CENTER_TYPE;

	/* Do some tests */
	parse ("<objects><object class=\"toto\"><property name=\"titi\" value=\"tutu\"/></object></objects>");
	parse ("<objects><object class=\"GObject\"><property name=\"titi\" value=\"tutu\"/></object></objects>");
	parse ("<objects><object class=\"VikWebtoolCenter\">"
	         "<property name=\"label\" value=\"Le label\"/>"
	         "<property name=\"url\" value=\"http://url/\"/>"
	         "<property name=\"id\" value=\"42\"/>"
	       "</object></objects>");

	return EXIT_SUCCESS;
}
