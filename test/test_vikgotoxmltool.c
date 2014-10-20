#include <math.h>
#include <vikgotoxmltool.h>

void parse(VikGotoTool *tool, gchar *filename)
{
    struct LatLon ll;
    ll.lat = NAN;
    ll.lon = NAN;
    if (vik_goto_tool_parse_file_for_latlon(tool, filename, &ll))
      printf("Found %g %g in %s\n", ll.lat, ll.lon, filename);
    else
      printf("Failed to parse file %s\n", filename);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION (2, 36, 0)
  g_type_init();
#endif

  VikGotoXmlTool *with_element = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM",
    "url-format", "http://ws.geonames.org/search?q=%s&maxRows=1&lang=es&style=short",
    "lat-path", "/geonames/geoname/lat",
    "lon-path", "/geonames/geoname/lng",
    NULL ) );

  VikGotoXmlTool *with_attr = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM",
    "url-format", "http://ws.geonames.org/search?q=%s&maxRows=1&lang=es&style=short",
    "lat-path", "/geonames/geoname",
    "lat-attr", "lat",
    "lon-path", "/geonames/geoname",
    "lon-attr", "lng",
    NULL ) );

  VikGotoXmlTool *with_xpath = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM",
    "url-format", "http://ws.geonames.org/search?q=%s&maxRows=1&lang=es&style=short",
    "lat-path", "/geonames/geoname@lat",
    "lon-path", "/geonames/geoname@lng",
    NULL ) );
    
  int i;
  for (i = 1; i<argc ; i++)
  {
    parse(VIK_GOTO_TOOL(with_element), argv[i]);
    parse(VIK_GOTO_TOOL(with_attr), argv[i]);
    parse(VIK_GOTO_TOOL(with_xpath), argv[i]);
  }
  return 0;
}
