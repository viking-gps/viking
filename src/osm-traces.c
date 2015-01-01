/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "viktrwlayer.h"
#include "osm-traces.h"
#include "gpx.h"
#include "background.h"
#include "preferences.h"

/* params will be osm_traces.username, osm_traces.password */
/* we have to make sure these don't collide. */
#define VIKING_OSM_TRACES_PARAMS_GROUP_KEY "osm_traces"
#define VIKING_OSM_TRACES_PARAMS_NAMESPACE "osm_traces."

#define VIK_SETTINGS_OSM_TRACE_VIS "osm_trace_visibility"
static gint last_active = -1;

/**
 * Login to use for OSM uploading.
 */
static gchar *osm_user = NULL;

/**
 * Password to use for OSM uploading.
 */
static gchar *osm_password = NULL;

/**
 * Mutex to protect auth. token
 */
static GMutex *login_mutex = NULL;

/**
 * Different type of trace visibility.
 */
typedef struct _OsmTraceVis_t {
	const gchar *combostr;
	const gchar *apistr;
} OsmTraceVis_t;

static const OsmTraceVis_t OsmTraceVis[] = {
	{ N_("Identifiable (public w/ timestamps)"),	"identifiable" },
	{ N_("Trackable (private w/ timestamps)"),	"trackable" },
	{ N_("Public"),					"public" },
	{ N_("Private"),				"private" },
	{ NULL, NULL },
};

/**
 * Struct hosting needed info.
 */
typedef struct _OsmTracesInfo {
  gchar *name;
  gchar *description;
  gchar *tags;
  gboolean anonymize_times; // ATM only available on a single track.
  const OsmTraceVis_t *vistype;
  VikTrwLayer *vtl;
  VikTrack *trk;
} OsmTracesInfo;

static VikLayerParam prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_OSM_TRACES_PARAMS_NAMESPACE "username", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("OSM username:"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_OSM_TRACES_PARAMS_NAMESPACE "password", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("OSM password:"), VIK_LAYER_WIDGET_PASSWORD, NULL, NULL, NULL, NULL, NULL, NULL },
};

/**
 * Free an OsmTracesInfo struct.
 */
static void oti_free(OsmTracesInfo *oti)
{
  if (oti) {
    /* Fields have been g_strdup'ed */
    g_free(oti->name); oti->name = NULL;
    g_free(oti->description); oti->description = NULL;
    g_free(oti->tags); oti->tags = NULL;
    
    g_object_unref(oti->vtl); oti->vtl = NULL;
  }
  /* Main struct has been g_malloc'ed */
  g_free(oti);
}

static const gchar *get_default_user()
{
  const gchar *default_user = NULL;

  /* Retrieve "standard" EMAIL varenv */
  default_user = g_getenv("EMAIL");

  return default_user;
}

void osm_set_login(const gchar *user, const gchar *password)
{
  g_mutex_lock(login_mutex);
  g_free(osm_user); osm_user = NULL;
  g_free(osm_password); osm_password = NULL;
  osm_user        = g_strdup(user);
  osm_password    = g_strdup(password);
  g_mutex_unlock(login_mutex);
}

gchar *osm_get_login()
{
  gchar *user_pass = NULL;
  g_mutex_lock(login_mutex);
  user_pass = g_strdup_printf("%s:%s", osm_user, osm_password);
  g_mutex_unlock(login_mutex);
  return user_pass;
}

/* initialisation */
void osm_traces_init () {
  /* Preferences */
  a_preferences_register_group ( VIKING_OSM_TRACES_PARAMS_GROUP_KEY, _("OpenStreetMap Traces") );

  VikLayerParamData tmp;
  tmp.s = "";
  a_preferences_register(prefs, tmp, VIKING_OSM_TRACES_PARAMS_GROUP_KEY);
  tmp.s = "";
  a_preferences_register(prefs+1, tmp, VIKING_OSM_TRACES_PARAMS_GROUP_KEY);

  login_mutex = vik_mutex_new();
}

void osm_traces_uninit()
{
  vik_mutex_free(login_mutex);
}

/*
 * Upload a file
 * returns a basic status:
 *   < 0  : curl error
 *   == 0 : OK
 *   > 0  : HTTP error
  */
static gint osm_traces_upload_file(const char *user,
				   const char *password,
				   const char *file,
				   const char *filename,
				   const char *description,
				   const char *tags,
				   const OsmTraceVis_t *vistype)
{
  CURL *curl;
  CURLcode res;
  char curl_error_buffer[CURL_ERROR_SIZE];
  struct curl_slist *headers = NULL;
  struct curl_httppost *post=NULL;
  struct curl_httppost *last=NULL;

  char *base_url = "http://www.openstreetmap.org/api/0.6/gpx/create";

  gchar *user_pass = osm_get_login();

  gint result = 0; // Default to it worked!

  g_debug("%s: %s %s %s %s %s %s", __FUNCTION__,
  	  user, password, file, filename, description, tags);

  /* Init CURL */
  curl = curl_easy_init();

  /* Filling the form */
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "description",
               CURLFORM_COPYCONTENTS, description, CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "tags",
               CURLFORM_COPYCONTENTS, tags, CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "visibility",
               CURLFORM_COPYCONTENTS, vistype->apistr, CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "file",
               CURLFORM_FILE, file,
               CURLFORM_FILENAME, filename,
	       CURLFORM_CONTENTTYPE, "text/xml", CURLFORM_END);

  /* Prepare request */
  /* As explained in http://wiki.openstreetmap.org/index.php/User:LA2 */
  /* Expect: header seems to produce incompatibilites between curl and httpd */
  headers = curl_slist_append(headers, "Expect: ");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_URL, base_url);
  curl_easy_setopt(curl, CURLOPT_USERPWD, user_pass);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_buffer);
  if (vik_verbose)
    curl_easy_setopt ( curl, CURLOPT_VERBOSE, 1 );

  /* Execute request */
  res = curl_easy_perform(curl);
  if (res == CURLE_OK)
  {
    long code;
    res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (res == CURLE_OK)
    {
      g_debug("received valid curl response: %ld", code);
      if (code != 200) {
        g_warning(_("failed to upload data: HTTP response is %ld"), code);
	result = code;
      }
    }
    else {
      g_critical(_("curl_easy_getinfo failed: %d"), res);
      result = -1;
    }
  }
  else {
    g_warning(_("curl request failed: %s"), curl_error_buffer);
    result = -2;
  }

  /* Memory */
  g_free(user_pass); user_pass = NULL;
  
  curl_formfree(post);
  curl_easy_cleanup(curl);
  return result;
}

/**
 * uploading function executed by the background" thread
 */
static void osm_traces_upload_thread ( OsmTracesInfo *oti, gpointer threaddata )
{
  /* Due to OSM limits, we have to enforce ele and time fields
   also don't upload invisible tracks */
  static GpxWritingOptions options = { TRUE, TRUE, FALSE, FALSE };

  if (!oti)
    return;

  gchar *filename = NULL;

  /* writing gpx file */
  if (oti->trk != NULL)
  {
    /* Upload only the selected track */
    if ( oti->anonymize_times )
    {
      VikTrack *trk = vik_track_copy(oti->trk, TRUE);
      vik_track_anonymize_times(trk);
      filename = a_gpx_write_track_tmp_file(trk, &options);
      vik_track_free(trk);
    }
    else
      filename = a_gpx_write_track_tmp_file (oti->trk, &options);
  }
  else
  {
    /* Upload the whole VikTrwLayer */
    filename = a_gpx_write_tmp_file (oti->vtl, &options);
  }
  
  if ( !filename )
    return;

  /* finally, upload it */
  gint ans = osm_traces_upload_file(osm_user, osm_password, filename,
                   oti->name, oti->description, oti->tags, oti->vistype);

  //
  // Show result in statusbar or failure in dialog for user feedback
  //

  // Get current time to put into message to show when result was generated
  //  since need to show difference between operations (when displayed on statusbar)
  // NB If on dialog then don't need time.
  time_t timenow;
  struct tm* timeinfo;
  time ( &timenow );
  timeinfo = localtime ( &timenow );
  gchar timestr[80];
  // Compact time only - as days/date isn't very useful here
  strftime ( timestr, sizeof(timestr), "%X)", timeinfo );

  //
  // Test to see if window it was invoked on is still valid
  // Not sure if this test really works! (i.e. if the window was closed in the mean time)
  //
  if ( IS_VIK_WINDOW ((VikWindow *)VIK_GTK_WINDOW_FROM_LAYER(oti->vtl)) ) {
    gchar* msg;
    if ( ans == 0 ) {
      // Success
      msg = g_strdup_printf ( "%s (@%s)", _("Uploaded to OSM"), timestr );
    }
    // Use UPPER CASE for bad news :(
    else if ( ans < 0 ) {
      msg = g_strdup_printf ( "%s (@%s)", _("FAILED TO UPLOAD DATA TO OSM - CURL PROBLEM"), timestr );
    }
    else {
      msg = g_strdup_printf ( "%s : %s %d (@%s)", _("FAILED TO UPLOAD DATA TO OSM"), _("HTTP response code"), ans, timestr );
    }
    vik_window_statusbar_update ( (VikWindow*)VIK_GTK_WINDOW_FROM_LAYER(oti->vtl), msg, VIK_STATUSBAR_INFO );
    g_free (msg);
  }
  /* Removing temporary file */
  int ret = g_unlink(filename);
  if (ret != 0) {
    g_critical(_("failed to unlink temporary file: %s"), strerror(errno));
  }
}

/**
 *
 */
void osm_login_widgets (GtkWidget *user_entry, GtkWidget *password_entry)
{
  if (!user_entry || !password_entry)
    return;

  const gchar *default_user = get_default_user();
  const gchar *pref_user = a_preferences_get(VIKING_OSM_TRACES_PARAMS_NAMESPACE "username")->s;
  const gchar *pref_password = a_preferences_get(VIKING_OSM_TRACES_PARAMS_NAMESPACE "password")->s;

  if (osm_user != NULL && osm_user[0] != '\0')
    gtk_entry_set_text(GTK_ENTRY(user_entry), osm_user);
  else if (pref_user != NULL && pref_user[0] != '\0')
    gtk_entry_set_text(GTK_ENTRY(user_entry), pref_user);
  else if (default_user != NULL)
    gtk_entry_set_text(GTK_ENTRY(user_entry), default_user);

  if (osm_password != NULL && osm_password[0] != '\0')
    gtk_entry_set_text(GTK_ENTRY(password_entry), osm_password);
  else if (pref_password != NULL)
    gtk_entry_set_text(GTK_ENTRY(password_entry), pref_password);
  /* This is a password -> invisible */
  gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
}

/**
 * Uploading a VikTrwLayer
 *
 * @param vtl VikTrwLayer
 * @param trk if not null, the track to upload
 */
void osm_traces_upload_viktrwlayer ( VikTrwLayer *vtl, VikTrack *trk )
{
  GtkWidget *dia = gtk_dialog_new_with_buttons (_("OSM upload"),
                                                 VIK_GTK_WINDOW_FROM_LAYER(vtl),
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_STOCK_CANCEL,
                                                 GTK_RESPONSE_REJECT,
                                                 GTK_STOCK_OK,
                                                 GTK_RESPONSE_ACCEPT,
                                                 NULL);

  const gchar *name = NULL;
  GtkWidget *user_label, *user_entry;
  GtkWidget *password_label, *password_entry;
  GtkWidget *name_label, *name_entry;
  GtkWidget *description_label, *description_entry;
  GtkWidget *tags_label, *tags_entry;
  GtkWidget *visibility;
  GtkWidget *anonymize_checkbutton = NULL;
  const OsmTraceVis_t *vis_t;

  user_label = gtk_label_new(_("Email:"));
  user_entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), user_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), user_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_markup(GTK_WIDGET(user_entry),
                        _("The email used as login\n"
                        "<small>Enter the email you use to login into www.openstreetmap.org.</small>"));

  password_label = gtk_label_new(_("Password:"));
  password_entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), password_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), password_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_markup(GTK_WIDGET(password_entry),
                        _("The password used to login\n"
                        "<small>Enter the password you use to login into www.openstreetmap.org.</small>"));

  osm_login_widgets ( user_entry, password_entry );

  name_label = gtk_label_new(_("File's name:"));
  name_entry = gtk_entry_new();
  if (trk != NULL)
    name = trk->name;
  else
    name = vik_layer_get_name(VIK_LAYER(vtl));
  gtk_entry_set_text(GTK_ENTRY(name_entry), name);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), name_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), name_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_markup(GTK_WIDGET(name_entry),
                        _("The name of the file on OSM\n"
                        "<small>This is the name of the file created on the server."
			"This is not the name of the local file.</small>"));

  description_label = gtk_label_new(_("Description:"));
  description_entry = gtk_entry_new();
  const gchar *description = NULL;
  if (trk != NULL)
    description = trk->description;
  else {
    VikTRWMetadata *md = vik_trw_layer_get_metadata (vtl);
    description = md ? md->description : NULL;
  }
  if (description)
    gtk_entry_set_text(GTK_ENTRY(description_entry), description);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), description_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), description_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text(GTK_WIDGET(description_entry),
                        _("The description of the trace"));

  if (trk != NULL) {
    GtkWidget *label = gtk_label_new(_("Anonymize Times:"));
    anonymize_checkbutton = gtk_check_button_new ();
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), anonymize_checkbutton, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(GTK_WIDGET(anonymize_checkbutton),
                                _("Anonymize times of the trace.\n"
                                  "<small>You may choose to make the trace identifiable, yet mask the actual real time values</small>"));
  }

  tags_label = gtk_label_new(_("Tags:"));
  tags_entry = gtk_entry_new();
  VikTRWMetadata *md = vik_trw_layer_get_metadata (vtl);
  if (md->keywords)
    gtk_entry_set_text(GTK_ENTRY(tags_entry), md->keywords);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), tags_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), tags_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text(GTK_WIDGET(tags_entry),
                        _("The tags associated to the trace"));

  visibility = vik_combo_box_text_new();
  for (vis_t = OsmTraceVis; vis_t->combostr != NULL; vis_t++)
    vik_combo_box_text_append (visibility, vis_t->combostr);

  // Set identifiable by default or use the settings for the value
  if ( last_active < 0 ) {
    gint find_entry = -1;
    gint wanted_entry = -1;
    gchar *vis = NULL;
    if ( a_settings_get_string ( VIK_SETTINGS_OSM_TRACE_VIS, &vis ) ) {
      // Use setting
      if ( vis ) {
        for (vis_t = OsmTraceVis; vis_t->apistr != NULL; vis_t++) {
          find_entry++;
          if (!strcmp(vis, vis_t->apistr)) {
            wanted_entry = find_entry;
          }
        }
      }
      // If not found set it to the first entry, otherwise use the entry
      last_active = ( wanted_entry < 0 ) ? 0 : wanted_entry;
    }
    else
      last_active = 0;
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(visibility), last_active);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), GTK_WIDGET(visibility), FALSE, FALSE, 0);

  /* User should think about it first... */
  gtk_dialog_set_default_response ( GTK_DIALOG(dia), GTK_RESPONSE_REJECT );

  gtk_widget_show_all ( dia );
  gtk_widget_grab_focus ( description_entry );

  if ( gtk_dialog_run ( GTK_DIALOG(dia) ) == GTK_RESPONSE_ACCEPT )
  {
    gchar *title = NULL;

    /* overwrite authentication info */
    osm_set_login(gtk_entry_get_text(GTK_ENTRY(user_entry)),
                  gtk_entry_get_text(GTK_ENTRY(password_entry)));

    /* Storing data for the future thread */
    OsmTracesInfo *info = g_malloc(sizeof(OsmTracesInfo));
    info->name        = g_strdup(gtk_entry_get_text(GTK_ENTRY(name_entry)));
    info->description = g_strdup(gtk_entry_get_text(GTK_ENTRY(description_entry)));
    /* TODO Normalize tags: they will be used as URL part */
    info->tags        = g_strdup(gtk_entry_get_text(GTK_ENTRY(tags_entry)));
    info->vistype     = &OsmTraceVis[gtk_combo_box_get_active(GTK_COMBO_BOX(visibility))];
    info->vtl         = VIK_TRW_LAYER(g_object_ref(vtl));
    info->trk         = trk;
    if (trk != NULL && anonymize_checkbutton != NULL )
      info->anonymize_times = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(anonymize_checkbutton));
    else
      info->anonymize_times = FALSE;

    // Save visibility value for default reuse
    last_active = gtk_combo_box_get_active(GTK_COMBO_BOX(visibility));
    a_settings_set_string ( VIK_SETTINGS_OSM_TRACE_VIS, OsmTraceVis[last_active].apistr );

    title = g_strdup_printf(_("Uploading %s to OSM"), info->name);

    // launch the thread
    a_background_thread( BACKGROUND_POOL_REMOTE,
                         VIK_GTK_WINDOW_FROM_LAYER(vtl),          /* parent window */
                         title,                                   /* description string */
                         (vik_thr_func) osm_traces_upload_thread, /* function to call within thread */
                         info,                                    /* pass along data */
                         (vik_thr_free_func) oti_free,            /* function to free pass along data */
                         (vik_thr_free_func) NULL,
                         1 );
    g_free ( title ); title = NULL;
  }
  gtk_widget_destroy ( dia );
}
