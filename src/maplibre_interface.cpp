/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <mbgl/map/map.hpp>
#include <mbgl/gfx/headless_frontend.hpp>
#include <mbgl/style/style.hpp>

#include "maplibre_interface.h"
#include "vik_compat.h"
#include "globals.h"
#include "settings.h"

#define MAPLIBRE_INTERFACE_TYPE            (maplibre_interface_get_type ())
#define MAPLIBRE_INTERFACE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MAPLIBRE_INTERFACE_TYPE, MaplibreInterface))
#define MAPLIBRE_INTERFACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MAPLIBRE_INTERFACE_TYPE, MaplibreInterfaceClass))
#define IS_MAPLIBRE_INTERFACE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MAPLIBRE_INTERFACE_TYPE))
#define IS_MAPLIBRE_INTERFACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MAPLIBRE_INTERFACE_TYPE))

typedef struct _MaplibreInterfaceClass MaplibreInterfaceClass;
typedef struct _MaplibreInterface MaplibreInterface;

GType maplibre_interface_get_type ();
struct _MaplibreInterfaceClass
{
	GObjectClass object_class;
};

static void maplibre_interface_class_init ( MaplibreInterfaceClass *mic )
{
}

static void maplibre_interface_init ( MaplibreInterface *mi )
{
}

struct _MaplibreInterface {
	GObject obj;
	mbgl::HeadlessFrontend *frontend;
	mbgl::Map *map;
	gchar *copyright; // Cached Mapnik parameter to save looking it up each time
    GMutex *render_mutex;
};

G_DEFINE_TYPE (MaplibreInterface, maplibre_interface, G_TYPE_OBJECT)

MaplibreInterface* maplibre_interface_new ( guint width, guint height, const gchar *cache_file, const gchar *api_key )
{
	MaplibreInterface* mi = MAPLIBRE_INTERFACE ( g_object_new ( MAPLIBRE_INTERFACE_TYPE, NULL ) );
	mi->frontend = new mbgl::HeadlessFrontend( { width, height }, 1.0 );
	mi->map= new mbgl::Map(
		*mi->frontend,
		mbgl::MapObserver::nullObserver(),
		mbgl::MapOptions().withMapMode( mbgl::MapMode::Static )
			.withSize( mi->frontend->getSize() )
			.withPixelRatio( 1.0 ),
		mbgl::ResourceOptions()
			.withCachePath( cache_file )
			.withApiKey(api_key)
			//.withAssetPath(asset_root)
	);
	mi->copyright = NULL;
	mi->render_mutex = vik_mutex_new();
	return mi;
}

void maplibre_interface_free ( MaplibreInterface* mi )
{
	if ( mi ) {
		g_free ( mi->copyright );
		delete mi->map;
		delete mi->frontend;
		vik_mutex_free ( mi->render_mutex );
	}
	g_object_unref ( G_OBJECT(mi) );
}

void maplibre_interface_initialize ()
{
	// setup plugins and font dir originally
}

/**
 *  caching this answer instead of looking it up each time
 */
static void set_copyright ( MaplibreInterface* mi )
{
	g_free ( mi->copyright );
	mi->copyright = NULL;
	// set copyright from map style
}

/**
 * maplibre_interface_load_style_file:
 *
 * Returns NULL on success otherwise a string about what went wrong.
 *  This string should be freed once it has been used.
 */
gchar* maplibre_interface_load_style_file ( MaplibreInterface* mi,
                                            const gchar *filename )
{
	//TODO: width/height wrong time set size above
	gchar *msg = NULL;
	if ( !mi ) return g_strdup ("Internal Error");
	try {
		std::string style_url = std::string(filename);
		if (style_url.find("://") == std::string::npos) {
			style_url = std::string("file://") + style_url;
		}
//		mi->myMap->remove_all(); // Support reloading
		mi->map->getStyle().loadURL(style_url);
		set_copyright ( mi );
	} catch (std::exception const& ex) {
		msg = g_strdup ( ex.what() );
	} catch (...) {
		msg = g_strdup ("unknown error");
	}
	return msg;
}

// GdkPixbufDestroyNotify
static void destroy_fn ( guchar *pixels, gpointer data )
{
	g_free ( pixels );
}

/**
 * maplibre_interface_render:
 *
 * Returns a #GdkPixbuf of the specified area. #GdkPixbuf may be NULL
 */
GdkPixbuf* maplibre_interface_render ( MaplibreInterface* mi, double lat_tl, double lon_tl, double lat_br, double lon_br )
{
	if ( !mi ) return NULL;

	g_mutex_lock(mi->render_mutex);

	// Copy main object to local map variable
	//  This enables rendering to work when this function is called from different threads
//	mapnik::Map myMap(*mi->myMap);
	// TODO: projection of coords?

	GdkPixbuf *pixbuf = NULL;

	try {
		mbgl::LatLngBounds boundingBox = mbgl::LatLngBounds::hull(
			mbgl::LatLng(lat_tl, lon_tl),
			mbgl::LatLng(lat_br, lon_br)
		);
		mi->map->jumpTo(
			mi->map->cameraForLatLngBounds(
				boundingBox,
				mbgl::EdgeInsets()
			)
		);

		guint width = mi->frontend->getSize().width;
		guint height = mi->frontend->getSize().height;

		mbgl::HeadlessFrontend::RenderResult res
			= mi->frontend->render( *mi->map );
		unsigned char *ImageRawDataPtr
			= (unsigned char *) g_malloc( 4 * width * height );
		if ( !ImageRawDataPtr )
			return NULL;
		memcpy( ImageRawDataPtr, res.image.data.get(), 4 * width * height );
		pixbuf = gdk_pixbuf_new_from_data(
			ImageRawDataPtr,
			GDK_COLORSPACE_RGB,
			TRUE,
			8,
			width,
			height,
			width * 4,
			destroy_fn,
			NULL
		);
	}
	catch (const std::exception & ex) {
		g_warning ("An error occurred while rendering: %s", ex.what());
	} catch (...) {
		g_warning ("An unknown error occurred while rendering");
	}

	g_mutex_unlock(mi->render_mutex);

	return pixbuf;
}

/**
 * Copyright/Attribution information about the Map - string maybe NULL
 *
 * Free returned string  after use
 */
gchar* maplibre_interface_get_copyright ( MaplibreInterface* mi )
{
	if ( !mi ) return NULL;
	return mi->copyright;
}

/**
 * 'Parameter' information about the Map configuration
 *
 * Free every string element and the returned GArray itself after use
 */
GArray* maplibre_interface_get_parameters ( MaplibreInterface* mi )
{
	GArray *array = g_array_new (FALSE, TRUE, sizeof(gchar*));
	// TODO: add json file to array i think
	return array;
}

/**
 * General information about Maplibre
 *
 * Free the returned string after use
 */
gchar * maplibre_interface_about ( void )
{
	return g_strdup("Maplibre version number and things");
}
