#ifndef __VIK_DATASOURCES_H
#define __VIK_DATASOURCES_H

#include "acquire.h"

extern VikDataSourceInterface vik_datasource_gps_interface;
extern VikDataSourceInterface vik_datasource_google_interface;
#ifdef VIK_CONFIG_GEOCACHES
extern VikDataSourceInterface vik_datasource_gc_interface;
#endif
#endif
