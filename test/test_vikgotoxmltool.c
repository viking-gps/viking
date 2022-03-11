/**
 * This test program doesn't perform any downloading from search providers
 *  - it is expected that this is performed separately
 * The aim of this program is to test processing of the response
 *  as stored in a file (typically a temporary file during real usage)
 */
#include <math.h>
#include "vikgotoxmltool.h"
#include "vikgoto.h"
#include "vikgototool.h"

static gboolean parse(VikGotoTool *tool, gchar *filename)
{
    gboolean answer = FALSE;
    struct LatLon ll;
    ll.lat = NAN;
    ll.lon = NAN;
    if (vik_goto_tool_parse_file_for_latlon(tool, filename, &ll)) {
      printf("Found via for_latlon method %g %g in %s for tool %s\n", ll.lat, ll.lon, filename, vik_goto_tool_get_label(tool));
      answer = TRUE;
    }
    else
      printf("Unable to find latlon in file %s by tool %s\n", filename, vik_goto_tool_get_label(tool));

    GList *candidates = NULL;
    if ( vik_goto_tool_parse_file_for_candidates(tool, filename, &candidates) ) {
      guint len = g_list_length(candidates);
      if ( len > 0 ) {
        printf ("Found %d candidates via tool %s\n", len, vik_goto_tool_get_label(tool) );
        /*
        for ( GList *cnd = candidates; cnd != NULL; cnd = cnd->next ) {
          struct VikGotoCandidate *vgc = (struct VikGotoCandidate *) cnd->data;
          printf("Found %s @ %.3f %.3f\n", vgc->description, vgc->ll.lat, vgc->ll.lon);
        }
        */
      }
      g_list_free_full ( candidates, vik_goto_tool_free_candidate );
    }
    else
      printf("Failed to parse file %s\n", filename);
    return answer;
}

int main(int argc, char *argv[])
{
  // NB For candidate lookup method to succeed "desc-path" needs to be defined

  VikGotoXmlTool *with_element = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM1",
    "url-format", "http://ws.geonames.org/search?q=%s&maxRows=1&lang=es&style=short",
    "lat-path", "/geonames/geoname/lat",
    "lon-path", "/geonames/geoname/lng",
    "desc-path", "/geonames/geoname/toponymName",
    NULL ) );

  VikGotoXmlTool *with_attr = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM2",
    "url-format", "http://ws.geonames.org/search?q=%s&maxRows=1&lang=es&style=short",
    "lat-path", "/geonames/geoname",
    "lat-attr", "lat",
    "lon-path", "/geonames/geoname",
    "lon-attr", "lng",
    NULL ) );

  VikGotoXmlTool *with_xpath = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM3",
    "url-format", "http://ws.geonames.org/search?q=%s&maxRows=1&lang=es&style=short",
    "lat-path", "/geonames/geoname@lat",
    "lon-path", "/geonames/geoname@lng",
    NULL ) );

  VikGotoXmlTool *nominatim = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "OSM Nominatim",
    "url-format", "https://nominatim.openstreetmap.org/search?q=%s&format=xml",
    "lat-path", "/searchresults/place",
    "lat-attr", "lat",
    "lon-path", "/searchresults/place",
    "lon-attr", "lon",
    "desc-path", "/searchresults/place",
    "desc-attr", "display_name",
    NULL ) );

  // At least one parsing effort should produce a positive result
  gboolean result = FALSE;
  for (int i = 1; i<argc ; i++) {
    result = result | parse(VIK_GOTO_TOOL(with_element), argv[i]);
    result = result | parse(VIK_GOTO_TOOL(with_attr), argv[i]);
    result = result | parse(VIK_GOTO_TOOL(with_xpath), argv[i]);
    result = result | parse(VIK_GOTO_TOOL(nominatim), argv[i]);
  }

  // Convert to exit status
  return result ? 0 : 1;
}
