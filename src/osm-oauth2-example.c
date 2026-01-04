/* gitlab-oauth2-example.c
 *
 * Copyright 2021 Günther Wagner <info@gunibert.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * This example shows the recommended PKCE authorization flow for Gitlab.
 */

#include <glib.h>
#include <rest/rest.h>
#include <stdio.h>
#include <libsoup/soup.h>



GMainLoop *loop;

static void
fetch_logged_in_user(RestProxy *proxy)
{
  g_autoptr(GError) error = NULL;
  RestProxyCall *call;

  call = rest_proxy_new_call (proxy);
  rest_proxy_call_set_method (call, "GET");
  rest_proxy_call_set_function (call, "/user/details");
  rest_proxy_call_sync (call, &error);

  g_print ("%s\n", rest_proxy_call_get_payload (call));
  g_main_loop_quit (loop);
}

static void
osm_oauth2_example_refreshed_access_token (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  RestOAuth2Proxy *oauth2_proxy = (RestOAuth2Proxy *)object;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_OBJECT (object));
  g_assert (G_IS_ASYNC_RESULT (result));

  rest_oauth2_proxy_refresh_access_token_finish (oauth2_proxy, result, &error);
  if (error)
    g_error ("%s", error->message);

  g_print ("Access Token: %s\n", rest_oauth2_proxy_get_access_token (oauth2_proxy));
  g_print ("Refresh Token: %s\n", rest_oauth2_proxy_get_refresh_token (oauth2_proxy));
  GDateTime *expiration_date = rest_oauth2_proxy_get_expiration_date (oauth2_proxy);
  if (expiration_date)
    g_print ("Expires in: %s\n", g_date_time_format (expiration_date, "%X %x"));

  fetch_logged_in_user (REST_PROXY (oauth2_proxy));
}

static void
osm_oauth2_example_fetched_access_token (GObject      *object,
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

  g_print ("Access Token: %s\n", rest_oauth2_proxy_get_access_token (oauth2_proxy));
  g_print ("Refresh Token: %s\n", rest_oauth2_proxy_get_refresh_token (oauth2_proxy));
  GDateTime *expiration_date = rest_oauth2_proxy_get_expiration_date (oauth2_proxy);
  if (expiration_date)
    g_print ("Expires in: %s\n", g_date_time_format (expiration_date, "%X %x"));

  fetch_logged_in_user (REST_PROXY (oauth2_proxy));

  /* refresh token */
  rest_oauth2_proxy_refresh_access_token_async (oauth2_proxy, NULL, osm_oauth2_example_refreshed_access_token, user_data);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autofree gchar *line = NULL;
  size_t len = 0;

  gchar **environment = g_get_environ ();
  gchar *authurl = "https://www.openstreetmap.org/oauth2/authorize";
  gchar *tokenurl = "https://www.openstreetmap.org/oauth2/token";
  gchar *redirecturl = "urn:ietf:wg:oauth:2.0:oob";
  gchar *baseurl = "https://api.openstreetmap.org/api/0.6/";


  // ID 2ovOGzjSEA1W7ebG7D8aqyVvFPfs6RJW_TLweVcsPJk
  // x1_mDnLQbSgOvw2fFSde5WIKrTJH1v4tV4FFwCJMQc4
  const gchar *clientid = g_environ_getenv (environment, "REST_OAUTH2_CLIENT_ID");
  if (!clientid)
    {
      g_print ("You have to define your OpenStreetMap Client ID as REST_OAUTH2_CLIENT_ID environment variable\n");
      return EXIT_SUCCESS;
    }
// Secret annkUVUxacqmfqzOYMG3Bs8mNtjUH7SddoO4Nt7GRas 
// DKop7jo1FbXBW9J9B-wk85pOyQCQxrt8hQ7s9Srgz0A 
  const gchar *clientsecret = g_environ_getenv (environment, "REST_OAUTH2_CLIENT_SECRET");
  if (!clientsecret)
    {
      g_print ("You have to define your OpenStreetMap Client Secret as REST_OAUTH2_CLIENT_SECRET environment variable\n");
      return EXIT_SUCCESS;
    }
  RestPkceCodeChallenge *pkce = rest_pkce_code_challenge_new_random ();
  gchar *state = NULL;

#ifdef WITH_SOUP_2
  SoupLogger *logger = soup_logger_new (SOUP_LOGGER_LOG_HEADERS, -1);
#else
  SoupLogger *logger = soup_logger_new (SOUP_LOGGER_LOG_HEADERS);
#endif

  RestOAuth2Proxy *oauth2_proxy = rest_oauth2_proxy_new (authurl, tokenurl, redirecturl, clientid, clientsecret, baseurl);
  rest_proxy_add_soup_feature (REST_PROXY (oauth2_proxy), SOUP_SESSION_FEATURE (logger));
  const gchar *authorize_url = rest_oauth2_proxy_build_authorization_url (oauth2_proxy, rest_pkce_code_challenge_get_challenge (pkce), "openid", &state);

  g_print ("URL to authorize: %s\n", authorize_url);

  ssize_t chars = getline (&line, &len, stdin);
  if (line[chars - 1] == '\n') {
    line[chars - 1] = '\0';
  }

  g_print ("Got Authorization Grant: %s\n", line);

  /* fetch access token */
  rest_oauth2_proxy_fetch_access_token_async (oauth2_proxy, line, rest_pkce_code_challenge_get_verifier (pkce), NULL, osm_oauth2_example_fetched_access_token, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
