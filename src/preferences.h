#ifndef __VIKING_PREFERENCES_H
#define __VIKING_PREFERENCES_H

#include "uibuilder.h"

// TODO IMPORTANT!!!! add REGISTER_GROUP !!! OR SOMETHING!!! CURRENTLY GROUPLESS!!!

void a_preferences_init();
void a_preferences_uninit();

/* pref should be persistent thruout the life of the preference. */

  /* nothing in pref is copied neither is pref key copied, default param data yes */
void a_preferences_register( VikLayerParam *pref, VikLayerParamData defaultval);

void a_preferences_show_window(GtkWindow *parent);

VikLayerParamData *a_preferences_get(const gchar *key);

#endif
