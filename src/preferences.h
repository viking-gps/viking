#ifndef __VIKING_PREFERENCES_H
#define __VIKING_PREFERENCES_H

#include "uibuilder.h"

// TODO IMPORTANT!!!! add REGISTER_GROUP !!! OR SOMETHING!!! CURRENTLY GROUPLESS!!!

void a_preferences_init();
void a_preferences_uninit();

/* pref should be persistent thruout the life of the preference. */


/* must call FIRST */
void a_preferences_register_group ( const gchar *key, const gchar *name );

/* nothing in pref is copied neither but pref itself is copied. (TODO: COPY EVERYTHING IN PREF WE NEED, IF ANYTHING),
   so pref key is not copied. default param data IS copied. */
/* group field (integer) will be overwritten */
void a_preferences_register( VikLayerParam *pref, VikLayerParamData defaultval, const gchar *group_key );


void a_preferences_show_window(GtkWindow *parent);

VikLayerParamData *a_preferences_get(const gchar *key);



#endif
