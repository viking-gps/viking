/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2009-2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * 
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

 /**
  * SECTION:vikmapsourcedefault
  * @short_description: the base class implementing most of generic features
  * 
  * The #VikMapSourceDefault class is the base class implementing most of
  * generic feature, using properties or reducing complexity of some
  * functions.
  */

#include "vikmapsourcedefault.h"
#include "vikenumtypes.h"
#include "download.h"
#include <string.h>

static void map_source_get_copyright (VikMapSource *self, LatLonBBox bbox, gdouble zoom, void (*fct)(VikViewport*,const gchar*), void *data);
static const gchar *map_source_get_license (VikMapSource *self);
static const gchar *map_source_get_license_url (VikMapSource *self);
static const GdkPixbuf *map_source_get_logo (VikMapSource *self);

static const gchar *map_source_get_name (VikMapSource *self);
static guint16 map_source_get_uniq_id (VikMapSource *self);
static const gchar *map_source_get_label (VikMapSource *self);
static guint16 map_source_get_tilesize_x (VikMapSource *self);
static guint16 map_source_get_tilesize_y (VikMapSource *self);
static gdouble map_source_get_scale (VikMapSource *self);
static VikViewportDrawMode map_source_get_drawmode (VikMapSource *self);
static const gchar *map_source_get_file_extension (VikMapSource *self);
static gdouble map_source_get_offset_x (VikMapSource *self);
static gdouble map_source_get_offset_y (VikMapSource *self);

static DownloadResult_t _download ( VikMapSource *self, MapCoord *src, const gchar *dest_fn, void *handle );
static void * _download_handle_init ( VikMapSource *self );
static void _download_handle_cleanup ( VikMapSource *self, void *handle );

typedef struct _VikMapSourceDefaultPrivate VikMapSourceDefaultPrivate;
struct _VikMapSourceDefaultPrivate
{
	/* legal stuff */
	gchar *copyright;
	gchar *license;
	gchar *license_url;
	GdkPixbuf *logo;

	gchar *name;
	guint16 uniq_id;
	gchar *label;
	guint16 tilesize_x;
	guint16 tilesize_y;
	gdouble scale;
	VikViewportDrawMode drawmode;
	gchar *file_extension;
	gdouble offset_x;
	gdouble offset_y;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (VikMapSourceDefault, vik_map_source_default, VIK_TYPE_MAP_SOURCE);
#define VIK_MAP_SOURCE_DEFAULT_PRIVATE(o)  (vik_map_source_default_get_instance_private (VIK_MAP_SOURCE_DEFAULT(o)))

/* properties */
enum
{
  PROP_0,

  PROP_NAME,
  PROP_ID,
  PROP_LABEL,
  PROP_TILESIZE_X,
  PROP_TILESIZE_Y,
  PROP_SCALE,
  PROP_DRAWMODE,
  PROP_COPYRIGHT,
  PROP_LICENSE,
  PROP_LICENSE_URL,
  PROP_FILE_EXTENSION,
  PROP_OFFSET_X,
  PROP_OFFSET_Y,
};

static void
vik_map_source_default_init (VikMapSourceDefault *object)
{
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (object);

  priv->label = NULL;
  priv->copyright = NULL;
  priv->license = NULL;
  priv->license_url = NULL;
  priv->logo = NULL;
  priv->name = NULL;
  priv->file_extension = NULL;
}

static void
vik_map_source_default_finalize (GObject *object)
{
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (object);

  g_free (priv->label);
  priv->label = NULL;
  g_free (priv->copyright);
  priv->copyright = NULL;
  g_free (priv->license);
  priv->license = NULL;
  g_free (priv->license_url);
  priv->license_url = NULL;
  g_free (priv->logo);
  priv->license_url = NULL;
  g_free (priv->name);
  priv->name = NULL;
  g_free (priv->file_extension);
  priv->file_extension = NULL;

  G_OBJECT_CLASS (vik_map_source_default_parent_class)->finalize (object);
}

static void
vik_map_source_default_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (object);

  switch (property_id)
    {
    case PROP_NAME:
      // Sanitize the name here for file usage
      // A simple check just to prevent containing slashes ATM
      g_free (priv->name);
      priv->name = g_strdup(g_value_get_string (value));
      if ( priv->name )
        g_strdelimit (priv->name, "\\/", '_' );
      break;

    case PROP_ID:
      priv->uniq_id = g_value_get_uint (value);
      break;

    case PROP_LABEL:
      g_free (priv->label);
      priv->label = g_strdup(g_value_get_string (value));
      break;

    case PROP_TILESIZE_X:
      priv->tilesize_x = g_value_get_uint (value);
      break;

    case PROP_TILESIZE_Y:
      priv->tilesize_y = g_value_get_uint (value);
      break;

    case PROP_SCALE:
      priv->scale = g_value_get_double (value);
      break;

    case PROP_DRAWMODE:
      priv->drawmode = g_value_get_enum(value);
      break;

    case PROP_COPYRIGHT:
      g_free (priv->copyright);
      priv->copyright = g_strdup(g_value_get_string (value));
      break;

    case PROP_LICENSE:
      g_free (priv->license);
      priv->license = g_strdup(g_value_get_string (value));
      break;

    case PROP_LICENSE_URL:
      g_free (priv->license_url);
      priv->license_url = g_strdup(g_value_get_string (value));
      break;

    case PROP_FILE_EXTENSION:
      g_free (priv->file_extension);
      priv->file_extension = g_strdup(g_value_get_string(value));
      break;

    case PROP_OFFSET_X:
      priv->offset_x = g_value_get_double (value);
      break;

    case PROP_OFFSET_Y:
      priv->offset_y = g_value_get_double (value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_map_source_default_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_ID:
      g_value_set_uint (value, priv->uniq_id);
      break;

    case PROP_LABEL:
      g_value_set_string (value, priv->label);
      break;

    case PROP_TILESIZE_X:
      g_value_set_uint (value, priv->tilesize_x);
      break;

    case PROP_TILESIZE_Y:
      g_value_set_uint (value, priv->tilesize_y);
      break;

    case PROP_SCALE:
      g_value_set_double (value, priv->scale);
      break;

    case PROP_DRAWMODE:
      g_value_set_enum (value, priv->drawmode);
      break;

    case PROP_COPYRIGHT:
      g_value_set_string (value, priv->copyright);
      break;

    case PROP_LICENSE:
      g_value_set_string (value, priv->license);
      break;

    case PROP_LICENSE_URL:
      g_value_set_string (value, priv->license_url);
      break;

    case PROP_FILE_EXTENSION:
      g_value_set_string (value, priv->file_extension);
      break;

    case PROP_OFFSET_X:
      g_value_set_double (value, priv->offset_x);
      break;

    case PROP_OFFSET_Y:
      g_value_set_double (value, priv->offset_y);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_map_source_default_class_init (VikMapSourceDefaultClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* parent_class = VIK_MAP_SOURCE_CLASS (klass);
    GParamSpec *pspec = NULL;
	
	object_class->set_property = vik_map_source_default_set_property;
    object_class->get_property = vik_map_source_default_get_property;
	
	/* Overiding methods */
	parent_class->get_copyright =   map_source_get_copyright;
	parent_class->get_license =     map_source_get_license;
	parent_class->get_license_url = map_source_get_license_url;
	parent_class->get_logo =        map_source_get_logo;
	parent_class->get_name =        map_source_get_name;
	parent_class->get_uniq_id =    map_source_get_uniq_id;
	parent_class->get_label =      map_source_get_label;
	parent_class->get_tilesize_x = map_source_get_tilesize_x;
	parent_class->get_tilesize_y = map_source_get_tilesize_y;
	parent_class->get_scale =      map_source_get_scale;
	parent_class->get_drawmode =   map_source_get_drawmode;
	parent_class->get_file_extension = map_source_get_file_extension;
	parent_class->get_offset_x = map_source_get_offset_x;
	parent_class->get_offset_y = map_source_get_offset_y;
	parent_class->download =                 _download;
	parent_class->download_handle_init =     _download_handle_init;
	parent_class->download_handle_cleanup =  _download_handle_cleanup;

	/* Default implementation of methods */
	klass->get_uri = NULL;
	klass->get_hostname = NULL;
	klass->get_download_options = NULL;

	pspec = g_param_spec_string ("name",
	                             "Name",
	                             "The name of the map that may be used as the file cache directory",
	                             NULL, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NAME, pspec);

	pspec = g_param_spec_uint ("id",
	                           "Id of the tool",
                               "Set the id",
                               0  /* minimum value */,
                               G_MAXUINT /* maximum value */,
                               0  /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	pspec = g_param_spec_string ("label",
	                             "Label",
	                             "The label of the map source",
	                             "Unknown", // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LABEL, pspec);

	pspec = g_param_spec_uint ("tilesize-x",
	                           "TileSizeX",
                               "Set the size of the tile (x)",
                               0  /* minimum value */,
                               G_MAXUINT16 /* maximum value */,
                               0  /* default value */,
                               G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TILESIZE_X, pspec);

	pspec = g_param_spec_uint ("tilesize-y",
	                           "TileSizeY",
                               "Set the size of the tile (y)",
                               0  /* minimum value */,
                               G_MAXUINT16 /* maximum value */,
                               0  /* default value */,
                               G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TILESIZE_Y, pspec);

	pspec = g_param_spec_double ("scale",
	                             "Scale",
                                 "Scale of a tile",
                                 0.0, // minimum value
                                 99999.0, // maximum value
                                 1.0, // default value
                                 G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SCALE, pspec);

	pspec = g_param_spec_enum("drawmode",
                              "Drawmode",
                              "The mode used to draw map",
                              VIK_TYPE_VIEWPORT_DRAW_MODE,
                              VIK_VIEWPORT_DRAWMODE_UTM,
                              G_PARAM_READWRITE);
    g_object_class_install_property(object_class, PROP_DRAWMODE, pspec);                                    

	pspec = g_param_spec_string ("copyright",
	                             "Copyright",
	                             "The copyright of the map source",
	                             NULL,
	                             G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COPYRIGHT, pspec);

	pspec = g_param_spec_string ("license",
	                             "License",
	                             "The license of the map source",
	                             NULL,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LICENSE, pspec);

	pspec = g_param_spec_string ("license-url",
	                             "License URL",
	                             "The URL of the license of the map source",
	                             NULL,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LICENSE_URL, pspec);

	pspec = g_param_spec_string ("file-extension",
	                             "File Extension",
	                             "The file extension of tile files on disk",
	                             ".png" /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILE_EXTENSION, pspec);

	pspec = g_param_spec_double ("offset-x",
	                             "Offset X",
	                             "The offset of a tile (x) in metres",
	                             -G_MAXDOUBLE, // minimum value
	                             G_MAXDOUBLE, // maximum value
	                             0.0, // default value
	                             G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OFFSET_X, pspec);

	pspec = g_param_spec_double ("offset-y",
	                             "Offset Y",
	                             "The offset of a tile (y) in metres",
	                             -G_MAXDOUBLE, // minimum value
	                             G_MAXDOUBLE, // maximum value
	                             0.0, // default value
	                             G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OFFSET_Y, pspec);

	object_class->finalize = vik_map_source_default_finalize;
}

static void
map_source_get_copyright (VikMapSource *self, LatLonBBox bbox, gdouble zoom, void (*fct)(VikViewport*,const gchar*), void *data)
{
	/* Just ignore bbox and zoom level */
	g_return_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self));

	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	(*fct) (data, priv->copyright);
}

static const gchar *
map_source_get_license (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), NULL);
	
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->license;
}

static const gchar *
map_source_get_license_url (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), NULL);
	
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->license_url;
}

static const GdkPixbuf *
map_source_get_logo (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), NULL);

	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->logo;
}

static const gchar *
map_source_get_name (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), NULL);
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
	if ( priv->name )
		return priv->name;
	else {
		// Fallback if not set
		priv->name = g_strdup ( priv->label );
		g_strdelimit ( priv->name, "\\/", '_' );
		return priv->name;
	}
}

static guint16
map_source_get_uniq_id (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (guint16)0);
	
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->uniq_id;
}

static const gchar *
map_source_get_label (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), NULL);
	
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->label;
}

static guint16
map_source_get_tilesize_x (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (guint16)0);

    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->tilesize_x;
}

static guint16
map_source_get_tilesize_y (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (guint16)0);

    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->tilesize_y;
}

static double
map_source_get_scale (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (gdouble)1.0);

    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->scale;
}

static VikViewportDrawMode
map_source_get_drawmode (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (VikViewportDrawMode)0);

    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);

	return priv->drawmode;
}

static DownloadResult_t
_download ( VikMapSource *self, MapCoord *src, const gchar *dest_fn, void *handle )
{
   gchar *uri = vik_map_source_default_get_uri(VIK_MAP_SOURCE_DEFAULT(self), src);
   gchar *host = vik_map_source_default_get_hostname(VIK_MAP_SOURCE_DEFAULT(self));
   DownloadFileOptions *options = vik_map_source_default_get_download_options(VIK_MAP_SOURCE_DEFAULT(self), src);
   DownloadResult_t res = a_http_download_get_url ( host, uri, dest_fn, options, handle );
   g_free ( uri );
   g_free ( host );
   return res;
}

static const gchar *
map_source_get_file_extension (VikMapSource *self)
{
    g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), NULL);
    VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
    return priv->file_extension;
}

static gdouble
map_source_get_offset_x (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (gdouble)0.0);
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
	return priv->offset_x;
}

static gdouble
map_source_get_offset_y (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT(self), (gdouble)0.0);
	VikMapSourceDefaultPrivate *priv = VIK_MAP_SOURCE_DEFAULT_PRIVATE(self);
	return priv->offset_y;
}

static void *
_download_handle_init ( VikMapSource *self )
{
   return a_download_handle_init ();
}


static void
_download_handle_cleanup ( VikMapSource *self, void *handle )
{
   a_download_handle_cleanup ( handle );
}

gchar *
vik_map_source_default_get_uri( VikMapSourceDefault *self, MapCoord *src )
{
	VikMapSourceDefaultClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT (self), 0);
	klass = VIK_MAP_SOURCE_DEFAULT_GET_CLASS(self);

	g_return_val_if_fail (klass->get_uri != NULL, 0);

	return (*klass->get_uri)(self, src);
}

gchar *
vik_map_source_default_get_hostname( VikMapSourceDefault *self )
{
	VikMapSourceDefaultClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT (self), 0);
	klass = VIK_MAP_SOURCE_DEFAULT_GET_CLASS(self);

	g_return_val_if_fail (klass->get_hostname != NULL, 0);

	return (*klass->get_hostname)(self);
}

DownloadFileOptions *
vik_map_source_default_get_download_options( VikMapSourceDefault *self, MapCoord *src )
{
	VikMapSourceDefaultClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT (self), 0);
	klass = VIK_MAP_SOURCE_DEFAULT_GET_CLASS(self);

	g_return_val_if_fail (klass->get_download_options != NULL, 0);

	return (*klass->get_download_options)(self, src);
}

gchar *
vik_map_source_default_get_url_display( VikMapSourceDefault *self, MapCoord *src )
{
	VikMapSourceDefaultClass *klass;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (VIK_IS_MAP_SOURCE_DEFAULT (self), 0);
	klass = VIK_MAP_SOURCE_DEFAULT_GET_CLASS(self);

	g_return_val_if_fail (klass->get_uri != NULL, 0);
	g_return_val_if_fail (klass->get_hostname != NULL, 0);

	gchar *newstr = NULL;
	gchar *hostname = (*klass->get_hostname)(self);
	gchar *url = (*klass->get_uri)(self, src);
	if ( hostname && strlen(hostname)>1 ) {
		if ( strstr (hostname, "://") == NULL ) {
			//prepend http
			newstr = g_strdup_printf ( "http://%s", hostname );
		}
		else {
			newstr = g_strdup ( hostname );
		}
	}
	if ( url && strlen(url)>1 ) {
		if ( newstr ) {
			gchar *tmp = g_strdup ( newstr );
			newstr = g_strdup_printf ( "%s%s", newstr, url );
			g_free ( tmp );
		}
		else {
			newstr = g_strdup ( url );
		}
	}

	return newstr;
}
