/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2025, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "viking.h"

#include "osm_api_client.h"

#include <glib.h>
#include <rest/rest.h>
#include <stdio.h>
#include <libsoup/soup.h>

#define OSM_API_AUTH_URL "https://www.openstreetmap.org/oauth2/authorize"
#define OSM_API_TOKEN_URL "https://www.openstreetmap.org/oauth2/token"
#define OSM_API_REDIRECT_URL "urn:ietf:wg:oauth:2.0:oob"
#define OSM_API_BASE_URL "https://api.openstreetmap.org/api/0.6/"

#define OSM_API_OAUTH2_CLIENT_ID "x1_mDnLQbSgOvw2fFSde5WIKrTJH1v4tV4FFwCJMQc4"
#define OSM_API_OAUTH2_CLIENT_SECRET "DKop7jo1FbXBW9J9B-wk85pOyQCQxrt8hQ7s9Srgz0A"

typedef struct _OsmApiClientPrivate OsmApiClientPrivate;
struct _OsmApiClientPrivate
{
    gchar *token; // Property to hold the token
    RestOAuth2Proxy *oauth2_proxy;
    RestPkceCodeChallenge *pkce; // For challenge
};

G_DEFINE_TYPE_WITH_PRIVATE(OsmApiClient, osm_api_client, G_TYPE_OBJECT)
#define OSM_API_CLIENT_GET_PRIVATE(o)  (osm_api_client_get_instance_private (OSM_API_CLIENT(o)))

/* properties */
enum
{
	PROP_0,

	PROP_TOKEN,
};

static void osm_api_client_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    OsmApiClient *self = (OsmApiClient *)object;
	OsmApiClientPrivate *priv = osm_api_client_get_instance_private (self);

    switch (prop_id) {
        case PROP_TOKEN: // Assuming 1 is the ID for the token property
            g_free(priv->token);
            priv->token = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void osm_api_client_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    OsmApiClient *self = (OsmApiClient *)object;
	OsmApiClientPrivate *priv = osm_api_client_get_instance_private (self);

    switch (prop_id) {
        case PROP_TOKEN: // Assuming 1 is the ID for the token property
            g_value_set_string(value, priv->token);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void osm_api_client_class_init(OsmApiClientClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = osm_api_client_set_property;
    object_class->get_property = osm_api_client_get_property;

    klass->challenge_start = osm_api_client_challenge_start;
    klass->challenge_finish = osm_api_client_challenge_finish;
    klass->create_request = osm_api_client_create_request;

    g_object_class_install_property(object_class, PROP_TOKEN,
                                    g_param_spec_string("token", "Token", "Authentication token", NULL,
                                                        G_PARAM_READWRITE));
}

static void
osm_api_client_init(OsmApiClient *self) {
    self->token = NULL;
  	/* initialize the object here */
	OsmApiClientPrivate *priv = osm_api_client_get_instance_private (self);
}

static void
osm_api_client_finalize (GObject *object)
{
	OsmApiClientPrivate *priv = OSM_API_CLIENT_GET_PRIVATE (object);

    g_unref(priv->oauth2_proxy)
	g_free (priv->token);
	priv->token = NULL;

	G_OBJECT_CLASS (bing_map_source_parent_class)->finalize (object);
}

gchar *osm_api_client_challenge_start(OsmApiClient *self) {
    OsmApiClientPrivate *priv = osm_api_client_get_instance_private (self);

    priv->pkce = rest_pkce_code_challenge_new_random ();
    gchar *state = NULL;

    #ifdef WITH_SOUP_2
    SoupLogger *logger = soup_logger_new (SOUP_LOGGER_LOG_HEADERS, -1);
    #else
    SoupLogger *logger = soup_logger_new (SOUP_LOGGER_LOG_HEADERS);
    #endif

    priv->oauth2_proxy = rest_oauth2_proxy_new (OSM_API_AUTH_URL, OSM_API_TOKEN_URL, OSM_API_REDIRECT_URL, OSM_API_OAUTH2_CLIENT_ID, OSM_API_OAUTH2_CLIENT_SECRET, OSM_API_BASE_URL);
    rest_proxy_add_soup_feature (REST_PROXY (oauth2_proxy), SOUP_SESSION_FEATURE (logger));
    const gchar *authorize_url = rest_oauth2_proxy_build_authorization_url (oauth2_proxy, rest_pkce_code_challenge_get_challenge (pkce), "openid", &state);

    return authorize_url;
}


static void
osm_api_client_fetched_access_token (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
    RestOAuth2Proxy *oauth2_proxy = (RestOAuth2Proxy *)object;
    g_autoptr(GError) error = NULL;

    g_assert (G_IS_OBJECT (object));
    g_assert (G_IS_ASYNC_RESULT (result));

    rest_oauth2_proxy_fetch_access_token_finish (oauth2_proxy, result, &error);
    if (error)
        g_error ("%s", error->message);

    OsmApiClientPrivate *priv = OSM_API_CLIENT_GET_PRIVATE (object);

    // FIXME duplicate?
    priv->token = rest_oauth2_proxy_get_access_token (oauth2_proxy);
}

void
osm_api_client_challenge_finish(OsmApiClient *self, gchar *code) {
    OsmApiClientPrivate *priv = osm_api_client_get_instance_private (self);

    rest_oauth2_proxy_fetch_access_token_async (priv->oauth2_proxy, code, rest_pkce_code_challenge_get_verifier (priv->pkce), NULL, osm_api_client_fetched_access_token, self);
}

gchar *osm_api_client_create_request(OsmApiClient *self, const gchar *function) {
	OsmApiClientPrivate *priv = osm_api_client_get_instance_private (self);
    RestProxy *proxy = REST_PROXY (priv->oauth2_proxy);
    RestProxyCall *call;

    call = rest_proxy_new_call (proxy);
    rest_proxy_call_set_method (call, "GET");
    rest_proxy_call_set_function (call, function);
    rest_proxy_call_sync (call, &error);

    return rest_proxy_call_get_payload (call);
}
