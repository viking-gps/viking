#include "config.h"
#ifndef __VIKING_TOOBAR_XML_H
#define __VIKING_TOOBAR_XML_H

static const char *toolbar_xml =
"<ui>"
"  <toolbar name='MainToolbar'>"
"      <toolitem name='New' action='New'/>"
"      <toolitem name='Open' action='Open'/>"
"      <toolitem name='Save' action='Save'/>"
"      <toolitem name='Print' action='Print'/>"
"      <separator/>"
"      <toolitem action='FullScreen'/>"
"      <toolitem action='ViewSidePanel'/>"
"      <separator/>"
"      <toolitem action='GotoDefaultLocation'/>"
"      <toolitem action='GoBack'/>"
"      <toolitem action='GoForward'/>"
"      <toolitem action='GotoSearch'/>"
"      <separator/>"
"      <toolitem action='Pan'/>"
"      <toolitem action='Zoom'/>"
"      <toolitem action='Ruler'/>"
"      <toolitem action='Select'/>"
"      <separator/>"
"      <toolitem action='CreateWaypoint'/>"
"      <toolitem action='CreateTrack'/>"
"      <toolitem action='CreateRoute'/>"
"      <toolitem action='ExtendedRouteFinder'/>"
"      <toolitem action='Splitter'/>"
"      <toolitem action='EditWaypoint'/>"
"      <toolitem action='EditTrackpoint'/>"
"      <toolitem action='ShowPicture'/>"
"      <separator/>"
"      <toolitem action='GeorefMoveMap'/>"
"      <toolitem action='GeorefZoomTool'/>"
"      <separator/>"
"      <toolitem action='MapsDownload'/>"
"      <separator/>"
"      <toolitem action='DEMDownload'/>"
"      <separator/>"
#ifdef HAVE_LIBMAPNIK
"      <toolitem action='MapnikFeatures'/>"
"      <separator/>"
#endif
"  </toolbar>"
"</ui>"
;

#endif
