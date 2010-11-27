#include <math.h>
#include <vikwmscmapsource.h>
#include <vikslippymapsource.h>

void test_coord_to_mapcoord(VikMapSource *source, gdouble lat, gdouble lon, gdouble zoom)
{
  VikCoord vikCoord;
  MapCoord mapCoord;
  vikCoord.mode = VIK_COORD_LATLON;
  vikCoord.east_west = lon;
  vikCoord.north_south = lat;
  printf("%s: %f %f %f => ", g_type_name(G_OBJECT_TYPE(source)), vikCoord.east_west, vikCoord.north_south, zoom);
  vik_map_source_coord_to_mapcoord (source, &vikCoord, zoom, zoom, &mapCoord);
  printf("x=%d y=%d\n", mapCoord.x, mapCoord.y);
}

void test_mapcoord_to_center_coord (VikMapSource *source, int x, int y, int scale)
{
  VikCoord vikCoord;
  MapCoord mapCoord;
  mapCoord.x = x;
  mapCoord.y = y;
  mapCoord.scale = scale;
  printf("%s: %d %d %d => ", g_type_name(G_OBJECT_TYPE(source)), mapCoord.x, mapCoord.y, scale);
  vik_map_source_mapcoord_to_center_coord (source, &mapCoord, &vikCoord);
  printf("lon=%f lat=%f\n", vikCoord.east_west, vikCoord.north_south);
}

int main(int argc, char *argv[])
{
  g_type_init();
  
  VikMapSource *spotmaps4osm_wmsc_type = 
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_WMSC_MAP_SOURCE,
                                "id", 202,
                                "label", "Spotmaps (WMS-C)",
                                "hostname", "spotmaps.youmapps.org",
                                "url", "/spotmaps4osm/?LAYERS=spotmaps4osm&SERVICE=SPOTMAPS4OSM&SRS=EPSG:4326&bbox=%s,%s,%s,%s&width=256&height=256",
                                NULL));

  VikMapSource *osmarender_type = 
    VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
                                "id", 12,
                                "label", "OpenStreetMap (Osmarender)",
                                "hostname", "tah.openstreetmap.org",
                                "url", "/Tiles/tile/%d/%d/%d.png",
                                "check-file-server-time", TRUE,
                                NULL));
    
  gdouble lats[] = { 0, 90, 45, -45, -90 };
  gdouble lons[] = { 0, 180, 90, 45, -45, -90, -180 };
  int scale;
  for (scale = 0 ; scale < 18 ; scale++)
  {
    int i;
    for (i=0 ; i<sizeof(lats)/sizeof(lats[0]) ; i++)
    {
      int j;
      for (j=0 ; j<sizeof(lons)/sizeof(lons[0]) ; j++)
      {
        test_coord_to_mapcoord (spotmaps4osm_wmsc_type, lats[i], lons[j], 2<<scale);
        test_coord_to_mapcoord (osmarender_type, lats[i], lons[j], 2<<scale);
      }
    }
  }

  for (scale = 0 ; scale < 18 ; scale++)
  {
    test_mapcoord_to_center_coord (spotmaps4osm_wmsc_type, 0, 0, scale);
    test_mapcoord_to_center_coord (osmarender_type, 0, 0, scale);
  }

  return 0;
}
