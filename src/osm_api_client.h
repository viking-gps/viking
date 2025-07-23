/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2025, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
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

#ifndef _OSM_API_CLIENT_H
#define _OSM_API_CLIENT_H

#include <glib.h>

G_BEGIN_DECLS

#define OSM_TYPE_API_CLIENT (osm_api_client_get_type())
#define OSM_API_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), OSM_TYPE_API_CLIENT, OsmApiClient))
#define OSM_IS_API_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), OSM_TYPE_API_CLIENT))

typedef struct _OsmApiClientClass OsmApiClientClass;
typedef struct _OsmApiClient OsmApiClient;

struct _OsmApiClient {
    GObject parent_instance;
};

struct _OsmApiClientClass {
    GObjectClass parent_class;
    gchar *(*challenge_start)(OsmApiClient *self);
    gchar *(*challenge_finish)(OsmApiClient *self, gchar *code);
    gchar *(*create_request)(OsmApiClient *self, const gchar *url);
};


// Function prototypes
GType osm_api_client_get_type(void);
OsmApiClient *osm_api_client_new(void);
gchar *osm_api_client_challenge_start(OsmApiClient *self);
gchar *osm_api_client_challenge_finish(OsmApiClient *self, gchar *code);
gchar *osm_api_client_create_request(OsmApiClient *self, const gchar *function);

// Property accessors
void osm_api_client_set_token(OsmApiClient *self, const gchar *token);
const gchar *osm_api_client_get_token(OsmApiClient *self);

G_END_DECLS

#endif /* OSM_API_CLIENT_H */
