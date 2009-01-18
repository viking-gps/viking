#ifndef __VIK_GEONAMESSEARCH_H
#define __VIK_GEONAMESSEARCH_H

extern void a_geonames_search(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp);
gchar * a_geonamessearch_get_search_string_for_this_place(VikWindow *vw);
gchar *uri_escape(gchar *str);
#endif
