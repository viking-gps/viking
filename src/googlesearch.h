#ifndef __VIK_GOOGLESEARCH_H
#define __VIK_GOOGLESEARCH_H

extern void a_google_search(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp);
gchar * a_googlesearch_get_search_string_for_this_place(VikWindow *vw);
gchar *uri_escape(gchar *str);
#endif
