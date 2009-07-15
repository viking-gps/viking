#ifndef __VIK_GEONAMESSEARCH_H
#define __VIK_GEONAMESSEARCH_H

/* Finding a named place */
extern void a_geonames_search(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp);
gchar * a_geonamessearch_get_search_string_for_this_place(VikWindow *vw);

/* Finding Wikipedia entries within a certain box */
extern void a_geonames_wikipedia_box(VikWindow *vw, VikTrwLayer *vtl, VikLayersPanel *vlp, struct LatLon maxmin[2]);

#endif
