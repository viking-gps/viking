/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
#include "garminsymbols.h"
#include "icons.h"

#include <string.h>
#include <stdlib.h>

struct {
  gchar *sym;
  gint num;
  gchar *desc;
  const GdkPixdata *data;
  GdkPixbuf *icon;
} garmin_syms[] = {
  /*---------------------------------------------------------------
    Marine symbols
    ---------------------------------------------------------------*/
  { "anchor",         0,     "white anchor symbol",               NULL,                NULL },
  { "bell",           1,     "white bell symbol",                 NULL,                NULL },
  { "diamond_grn",    2,     "green diamond symbol",              NULL,                NULL },
  { "diamond_red",    3,     "red diamond symbol",                NULL,                NULL },
  { "dive1",          4,     "diver down flag 1",                 NULL,                NULL },
  { "dive2",          5,     "diver down flag 2",                 NULL,                NULL },
  { "dollar",         6,     "white dollar symbol",               NULL,                NULL },
  { "fish",           7,     "white fish symbol",                 NULL,                NULL },
  { "fuel",           8,     "white fuel symbol",                 NULL,                NULL },
  { "horn",           9,     "white horn symbol",                 NULL,                NULL },
  { "house",          10,    "white house symbol",                &wp_house,           NULL },
  { "knife",          11,    "white knife & fork symbol",         NULL,                NULL },
  { "light",          12,    "white light symbol",                NULL,                NULL },
  { "mug",            13,    "white mug symbol",                  NULL,                NULL },
  { "skull",          14,    "white skull and crossbones symbol", NULL,                NULL },
  { "square_grn",     15,    "green square symbol",               NULL,                NULL },
  { "square_red",     16,    "red square symbol",                 NULL,                NULL },
  { "wbuoy",          17,    "white buoy waypoint symbol",        NULL,                NULL },
  { "wpt_dot",        18,    "waypoint dot",                      &wp_wpt_dot,         NULL },
  { "wreck",          19,    "white wreck symbol",                NULL,                NULL },
  { "null",           20,    "null symbol (transparent)",         NULL,                NULL },
  { "mob",            21,    "man overboard symbol",              NULL,                NULL },
  { "buoy_ambr",      22,    "amber map buoy symbol",             NULL,                NULL },
  { "buoy_blck",      23,    "black map buoy symbol",             NULL,                NULL },
  { "buoy_blue",      24,    "blue map buoy symbol",              NULL,                NULL },
  { "buoy_grn",       25,    "green map buoy symbol",             NULL,                NULL },
  { "buoy_grn_red",   26,    "green/red map buoy symbol",         NULL,                NULL },
  { "buoy_grn_wht",   27,    "green/white map buoy symbol",       NULL,                NULL },
  { "buoy_orng",      28,    "orange map buoy symbol",            NULL,                NULL },
  { "buoy_red",       29,    "red map buoy symbol",               NULL,                NULL },
  { "buoy_red_grn",   30,    "red/green map buoy symbol",         NULL,                NULL },
  { "buoy_red_wht",   31,    "red/white map buoy symbol",         NULL,                NULL },
  { "buoy_violet",    32,    "violet map buoy symbol",            NULL,                NULL },
  { "buoy_wht",       33,    "white map buoy symbol",             NULL,                NULL },
  { "buoy_wht_grn",   34,    "white/green map buoy symbol",       NULL,                NULL },
  { "buoy_wht_red",   35,    "white/red map buoy symbol",         NULL,                NULL },
  { "dot",            36,    "white dot symbol",                  NULL,                NULL },
  { "rbcn",           37,    "radio beacon symbol",               NULL,                NULL },
  { "boat_ramp",      150,   "boat ramp symbol",                  NULL,                NULL },
  { "camp",           151,   "campground symbol",                 &wp_camp,            NULL },
  { "restrooms",      152,   "restrooms symbol",                  NULL,                NULL },
  { "showers",        153,   "shower symbol",                     NULL,                NULL },
  { "drinking_wtr",   154,   "drinking water symbol",             NULL,                NULL },
  { "phone",          155,   "telephone symbol",                  NULL,                NULL },
  { "1st_aid",        156,   "first aid symbol",                  NULL,                NULL },
  { "info",           157,   "information symbol",                NULL,                NULL },
  { "parking",        158,   "parking symbol",                    NULL,                NULL },
  { "park",           159,   "park symbol",                       NULL,                NULL },
  { "picnic",         160,   "picnic symbol",                     NULL,                NULL },
  { "scenic",         161,   "scenic area symbol",                NULL,                NULL },
  { "skiing",         162,   "skiing symbol",                     NULL,                NULL },
  { "swimming",       163,   "swimming symbol",                   NULL,                NULL },
  { "dam",            164,   "dam symbol",                        NULL,                NULL },
  { "controlled",     165,   "controlled area symbol",            NULL,                NULL },
  { "danger",         166,   "danger symbol",                     NULL,                NULL },
  { "restricted",     167,   "restricted area symbol",            NULL,                NULL },
  { "null_2",         168,   "null symbol",                       NULL,                NULL },
  { "ball",           169,   "ball symbol",                       NULL,                NULL },
  { "car",            170,   "car symbol",                        &wp_car,             NULL },
  { "deer",           171,   "deer symbol",                       &wp_deer,            NULL },
  { "shpng_cart",     172,   "shopping cart symbol",              NULL,                NULL },
  { "lodging",        173,   "lodging symbol",                    NULL,                NULL },
  { "mine",           174,   "mine symbol",                       NULL,                NULL },
  { "trail_head",     175,   "trail head symbol",                 NULL,                NULL },
  { "truck_stop",     176,   "truck stop symbol",                 NULL,                NULL },
  { "user_exit",      177,   "user exit symbol",                  NULL,                NULL },
  { "flag",           178,   "flag symbol",                       &wp_flag,            NULL },
  { "circle_x",       179,   "circle with x in the center",       NULL,                NULL },
  { "open_24hr",      180,   "open 24 hours symbol",              NULL,                NULL },
  { "fhs_facility",   181,   "U Fishing Hot SpotsTM Facility",    NULL,                NULL },
  { "bot_cond",       182,   "Bottom Conditions",                 NULL,                NULL },
  { "tide_pred_stn",  183,   "Tide/Current Prediction Station",   NULL,                NULL },
  { "anchor_prohib",  184,   "U anchor prohibited symbol",        NULL,                NULL },
  { "beacon",         185,   "U beacon symbol",                   NULL,                NULL },
  { "coast_guard",    186,   "U coast guard symbol",              NULL,                NULL },
  { "reef",           187,   "U reef symbol",                     NULL,                NULL },
  { "weedbed",        188,   "U weedbed symbol",                  NULL,                NULL },
  { "dropoff",        189,   "U dropoff symbol",                  NULL,                NULL },
  { "dock",           190,   "U dock symbol",                     NULL,                NULL },
  { "marina",         191,   "U marina symbol",                   NULL,                NULL },
  { "bait_tackle",    192,   "U bait and tackle symbol",          NULL,                NULL },
  { "stump",          193,   "U stump symbol",                    NULL,                NULL },
  /*---------------------------------------------------------------
    User customizable symbols
    The values from begin_custom to end_custom inclusive are
    reserved for the identification of user customizable symbols.
    ---------------------------------------------------------------*/
  { "begin_custom",   7680,  "first user customizable symbol",    NULL,                NULL },
  { "end_custom",     8191,  "last user customizable symbol",     NULL,                NULL },
  /*---------------------------------------------------------------
    Land symbols
    ---------------------------------------------------------------*/
  { "is_hwy",         8192,  "interstate hwy symbol",             NULL,                NULL },
  { "us_hwy",         8193,  "us hwy symbol",                     NULL,                NULL },
  { "st_hwy",         8194,  "state hwy symbol",                  NULL,                NULL },
  { "mi_mrkr",        8195,  "mile marker symbol",                NULL,                NULL },
  { "trcbck",         8196,  "TracBack (feet) symbol",            NULL,                NULL },
  { "golf",           8197,  "golf symbol",                       NULL,                NULL },
  { "sml_cty",        8198,  "small city symbol",                 NULL,                NULL },
  { "med_cty",        8199,  "medium city symbol",                NULL,                NULL },
  { "lrg_cty",        8200,  "large city symbol",                 NULL,                NULL },
  { "freeway",        8201,  "intl freeway hwy symbol",           NULL,                NULL },
  { "ntl_hwy",        8202,  "intl national hwy symbol",          NULL,                NULL },
  { "cap_cty",        8203,  "capitol city symbol (star)",        NULL,                NULL },
  { "amuse_pk",       8204,  "amusement park symbol",             NULL,                NULL },
  { "bowling",        8205,  "bowling symbol",                    NULL,                NULL },
  { "car_rental",     8206,  "car rental symbol",                 NULL,                NULL },
  { "car_repair",     8207,  "car repair symbol",                 NULL,                NULL },
  { "fastfood",       8208,  "fast food symbol",                  NULL,                NULL },
  { "fitness",        8209,  "fitness symbol",                    NULL,                NULL },
  { "movie",          8210,  "movie symbol",                      NULL,                NULL },
  { "museum",         8211,  "museum symbol",                     NULL,                NULL },
  { "pharmacy",       8212,  "pharmacy symbol",                   NULL,                NULL },
  { "pizza",          8213,  "pizza symbol",                      NULL,                NULL },
  { "post_ofc",       8214,  "post office symbol",                NULL,                NULL },
  { "rv_park",        8215,  "RV park symbol",                    NULL,                NULL },
  { "school",         8216,  "school symbol",                     NULL,                NULL },
  { "stadium",        8217,  "stadium symbol",                    NULL,                NULL },
  { "store",          8218,  "dept. store symbol",                NULL,                NULL },
  { "zoo",            8219,  "zoo symbol",                        NULL,                NULL },
  { "gas_plus",       8220,  "convenience store symbol",          NULL,                NULL },
  { "faces",          8221,  "live theater symbol",               NULL,                NULL },
  { "ramp_int",       8222,  "ramp intersection symbol",          NULL,                NULL },
  { "st_int",         8223,  "street intersection symbol",        NULL,                NULL },
  { "weigh_sttn",     8226,  "inspection/weigh station symbol",   NULL,                NULL },
  { "toll_booth",     8227,  "toll booth symbol",                 NULL,                NULL },
  { "elev_pt",        8228,  "elevation point symbol",            NULL,                NULL },
  { "ex_no_srvc",     8229,  "exit without services symbol",      NULL,                NULL },
  { "geo_place_mm",   8230,  "Geographic place name, man-made",   NULL,                NULL },
  { "geo_place_wtr",  8231,  "Geographic place name, water",      NULL,                NULL },
  { "geo_place_lnd",  8232,  "Geographic place name, land",       NULL,                NULL },
  { "bridge",         8233,  "bridge symbol",                     NULL,                NULL },
  { "building",       8234,  "building symbol",                   NULL,                NULL },
  { "cemetery",       8235,  "cemetery symbol",                   NULL,                NULL },
  { "church",         8236,  "church symbol",                     NULL,                NULL },
  { "civil",          8237,  "civil location symbol",             NULL,                NULL },
  { "crossing",       8238,  "crossing symbol",                   NULL,                NULL },
  { "hist_town",      8239,  "historical town symbol",            NULL,                NULL },
  { "levee",          8240,  "levee symbol",                      NULL,                NULL },
  { "military",       8241,  "military location symbol",          NULL,                NULL },
  { "oil_field",      8242,  "oil field symbol",                  NULL,                NULL },
  { "tunnel",         8243,  "tunnel symbol",                     NULL,                NULL },
  { "beach",          8244,  "beach symbol",                      NULL,                NULL },
  { "forest",         8245,  "forest symbol",                     NULL,                NULL },
  { "summit",         8246,  "summit symbol",                     NULL,                NULL },
  { "lrg_ramp_int",   8247,  "large ramp intersection symbol",    NULL,                NULL },
  { "lrg_ex_no_srvc", 8248,  "large exit without services smbl",  NULL,                NULL },
  { "badge",          8249,  "police/official badge symbol",      NULL,                NULL },
  { "cards",          8250,  "gambling/casino symbol",            NULL,                NULL },
  { "snowski",        8251,  "snow skiing symbol",                NULL,                NULL },
  { "iceskate",       8252,  "ice skating symbol",                NULL,                NULL },
  { "wrecker",        8253,  "tow truck (wrecker) symbol",        NULL,                NULL },
  { "border",         8254,  "border crossing (port of entry)",   NULL,                NULL },
  { "geocache",       8255,  "geocache location",                 &wp_geocache,        NULL },
  { "geocache_fnd",   8256,  "found geocache",                    &wp_geocache_fnd,    NULL },
  { "cntct_smiley",   8257,  "Rino contact symbol, ""smiley""",   NULL,                NULL },
  { "cntct_ball_cap", 8258,  "Rino contact symbol, ""ball cap""", NULL,                NULL },
  { "cntct_big_ears", 8259,  "Rino contact symbol, ""big ear""",  NULL,                NULL },
  { "cntct_spike",    8260,  "Rino contact symbol, ""spike""",    NULL,                NULL },
  { "cntct_goatee",   8261,  "Rino contact symbol, ""goatee""",   NULL,                NULL },
  { "cntct_afro",     8262,  "Rino contact symbol, ""afro""",     NULL,                NULL },
  { "cntct_dreads",   8263,  "Rino contact symbol, ""dreads""",   NULL,                NULL },
  { "cntct_female1",  8264,  "Rino contact symbol, ""female 1""", NULL,                NULL },
  { "cntct_female2",  8265,  "Rino contact symbol, ""female 2""", NULL,                NULL },
  { "cntct_female3",  8266,  "Rino contact symbol, ""female 3""", NULL,                NULL },
  { "cntct_ranger",   8267,  "Rino contact symbol, ""ranger""",   NULL,                NULL },
  { "cntct_kung_fu",  8268,  "Rino contact symbol, ""kung fu""",  NULL,                NULL },
  { "cntct_sumo",     8269,  "Rino contact symbol, ""sumo""",     NULL,                NULL },
  { "cntct_pirate",   8270,  "Rino contact symbol, ""pirate""",   NULL,                NULL },
  { "cntct_biker",    8271,  "Rino contact symbol, ""biker""",    NULL,                NULL },
  { "cntct_alien",    8272,  "Rino contact symbol, ""alien""",    NULL,                NULL },
  { "cntct_bug",      8273,  "Rino contact symbol, ""bug""",      NULL,                NULL },
  { "cntct_cat",      8274,  "Rino contact symbol, ""cat""",      NULL,                NULL },
  { "cntct_dog",      8275,  "Rino contact symbol, ""dog""",      NULL,                NULL },
  { "cntct_pig",      8276,  "Rino contact symbol, ""pig""",      NULL,                NULL },
  { "hydrant",        8282,  "water hydrant symbol",              NULL,                NULL },
  { "flag_blue",      8284,  "blue flag symbol",                  NULL,                NULL },
  { "flag_green",     8285,  "green flag symbol",                 NULL,                NULL },
  { "flag_red",       8286,  "red flag symbol",                   NULL,                NULL },
  { "pin_blue",       8287,  "blue pin symbol",                   NULL,                NULL },
  { "pin_green",      8288,  "green pin symbol",                  NULL,                NULL },
  { "pin_red",        8289,  "red pin symbol",                    NULL,                NULL },
  { "block_blue",     8290,  "blue block symbol",                 NULL,                NULL },
  { "block_green",    8291,  "green block symbol",                NULL,                NULL },
  { "block_red",      8292,  "red block symbol",                  NULL,                NULL },
  { "bike_trail",     8293,  "bike trail symbol",                 NULL,                NULL },
  { "circle_red",     8294,  "red circle symbol",                 NULL,                NULL },
  { "circle_green",   8295,  "green circle symbol",               NULL,                NULL },
  { "circle_blue",    8296,  "blue circle symbol",                NULL,                NULL },
  { "diamond_blue",   8299,  "blue diamond symbol",               NULL,                NULL },
  { "oval_red",       8300,  "red oval symbol",                   NULL,                NULL },
  { "oval_green",     8301,  "green oval symbol",                 NULL,                NULL },
  { "oval_blue",      8302,  "blue oval symbol",                  NULL,                NULL },
  { "rect_red",       8303,  "red rectangle symbol",              NULL,                NULL },
  { "rect_green",     8304,  "green rectangle symbol",            NULL,                NULL },
  { "rect_blue",      8305,  "blue rectangle symbol",             NULL,                NULL },
  { "square_blue",    8308,  "blue square symbol",                NULL,                NULL },
  { "letter_a_red",   8309,  "red letter 'A' symbol",             NULL,                NULL },
  { "letter_b_red",   8310,  "red letter 'B' symbol",             NULL,                NULL },
  { "letter_c_red",   8311,  "red letter 'C' symbol",             NULL,                NULL },
  { "letter_d_red",   8312,  "red letter 'D' symbol",             NULL,                NULL },
  { "letter_a_green", 8313,  "green letter 'A' symbol",           NULL,                NULL },
  { "letter_c_green", 8314,  "green letter 'C' symbol",           NULL,                NULL },
  { "letter_b_green", 8315,  "green letter 'B' symbol",           NULL,                NULL },
  { "letter_d_green", 8316,  "green letter 'D' symbol",           NULL,                NULL },
  { "letter_a_blue",  8317,  "blue letter 'A' symbol",            NULL,                NULL },
  { "letter_b_blue",  8318,  "blue letter 'B' symbol",            NULL,                NULL },
  { "letter_c_blue",  8319,  "blue letter 'C' symbol",            NULL,                NULL },
  { "letter_d_blue",  8320,  "blue letter 'D' symbol",            NULL,                NULL },
  { "number_0_red",   8321,  "red number '0' symbol",             NULL,                NULL },
  { "number_1_red",   8322,  "red number '1' symbol",             NULL,                NULL },
  { "number_2_red",   8323,  "red number '2' symbol",             NULL,                NULL },
  { "number_3_red",   8324,  "red number '3' symbol",             NULL,                NULL },
  { "number_4_red",   8325,  "red number '4' symbol",             NULL,                NULL },
  { "number_5_red",   8326,  "red number '5' symbol",             NULL,                NULL },
  { "number_6_red",   8327,  "red number '6' symbol",             NULL,                NULL },
  { "number_7_red",   8328,  "red number '7' symbol",             NULL,                NULL },
  { "number_8_red",   8329,  "red number '8' symbol",             NULL,                NULL },
  { "number_9_red",   8330,  "red number '9' symbol",             NULL,                NULL },
  { "number_0_green", 8331,  "green number '0' symbol",           NULL,                NULL },
  { "number_1_green", 8332,  "green number '1' symbol",           NULL,                NULL },
  { "number_2_green", 8333,  "green number '2' symbol",           NULL,                NULL },
  { "number_3_green", 8334,  "green number '3' symbol",           NULL,                NULL },
  { "number_4_green", 8335,  "green number '4' symbol",           NULL,                NULL },
  { "number_5_green", 8336,  "green number '5' symbol",           NULL,                NULL },
  { "number_6_green", 8337,  "green number '6' symbol",           NULL,                NULL },
  { "number_7_green", 8338,  "green number '7' symbol",           NULL,                NULL },
  { "number_8_green", 8339,  "green number '8' symbol",           NULL,                NULL },
  { "number_9_green", 8340,  "green number '9' symbol",           NULL,                NULL },
  { "number_0_blue",  8341,  "blue number '0' symbol",            NULL,                NULL },
  { "number_1_blue",  8342,  "blue number '1' symbol",            NULL,                NULL },
  { "number_2_blue",  8343,  "blue number '2' symbol",            NULL,                NULL },
  { "number_3_blue",  8344,  "blue number '3' symbol",            NULL,                NULL },
  { "number_4_blue",  8345,  "blue number '4' symbol",            NULL,                NULL },
  { "number_5_blue",  8346,  "blue number '5' symbol",            NULL,                NULL },
  { "number_6_blue",  8347,  "blue number '6' symbol",            NULL,                NULL },
  { "number_7_blue",  8348,  "blue number '7' symbol",            NULL,                NULL },
  { "number_8_blue",  8349,  "blue number '8' symbol",            NULL,                NULL },
  { "number_9_blue",  8350,  "blue number '9' symbol",            NULL,                NULL },
  { "triangle_blue",  8351,  "blue triangle symbol",              NULL,                NULL },
  { "triangle_green", 8352,  "green triangle symbol",             NULL,                NULL },
  { "triangle_red",   8353,  "red triangle symbol",               NULL,                NULL },
  /*---------------------------------------------------------------
    Aviation symbols
    ---------------------------------------------------------------*/
  { "airport",        16384, "airport symbol",                    NULL,                NULL },
  { "int",            16385, "intersection symbol",               NULL,                NULL },
  { "ndb",            16386, "non-directional beacon symbol",     NULL,                NULL },
  { "vor",            16387, "VHF omni-range symbol",             NULL,                NULL },
  { "heliport",       16388, "heliport symbol",                   NULL,                NULL },
  { "private",        16389, "private field symbol",              NULL,                NULL },
  { "soft_fld",       16390, "soft field symbol",                 NULL,                NULL },
  { "tall_tower",     16391, "tall tower symbol",                 NULL,                NULL },
  { "short_tower",    16392, "short tower symbol",                NULL,                NULL },
  { "glider",         16393, "glider symbol",                     NULL,                NULL },
  { "ultralight",     16394, "ultralight symbol",                 NULL,                NULL },
  { "parachute",      16395, "parachute symbol",                  NULL,                NULL },
  { "vortac",         16396, "VOR/TACAN symbol",                  NULL,                NULL },
  { "vordme",         16397, "VOR-DME symbol",                    NULL,                NULL },
  { "faf",            16398, "first approach fix",                NULL,                NULL },
  { "lom",            16399, "localizer outer marker",            NULL,                NULL },
  { "map",            16400, "missed approach point",             NULL,                NULL },
  { "tacan",          16401, "TACAN symbol",                      NULL,                NULL },
  { "seaplane",       16402, "Seaplane Base",                     NULL,                NULL }
};

GHashTable *icons = NULL;

/* found via Google, from Sylpheed/GPL and/or gstring.c in Glib */
static gint g_str_case_equal(gconstpointer v, gconstpointer v2)
{
	return strcasecmp((const gchar *)v, (const gchar *)v2) == 0;
}

static guint g_str_case_hash(gconstpointer key)
{
	const gchar *p = key;
	guint h = *p;

	if (h) {
		h = tolower(h);
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + tolower(*p);
	}

	return h;
}

static void init_icons() {
  icons = g_hash_table_new_full ( g_str_case_hash, g_str_case_equal, NULL, NULL);
  gint i;
  for (i=0; i<G_N_ELEMENTS(garmin_syms); i++) {
    g_hash_table_insert(icons, garmin_syms[i].sym, (gpointer)i);
  }
}

static GdkPixbuf *get_wp_sym_from_index ( gint i ) {
  if ( !garmin_syms[i].icon && garmin_syms[i].data ) {
    garmin_syms[i].icon = gdk_pixbuf_from_pixdata ( garmin_syms[i].data, FALSE, NULL );
  }
  return garmin_syms[i].icon;
}

GdkPixbuf *a_get_wp_sym ( const gchar *sym ) {
  if (!sym) {
    return NULL;
  }
  if (!icons) {
    init_icons();
  }
  return get_wp_sym_from_index((gint)g_hash_table_lookup(icons, sym));
}

void a_populate_sym_list ( GtkListStore *list ) {
  gint i;
  for (i=0; i<G_N_ELEMENTS(garmin_syms); i++) {
    if (garmin_syms[i].data) {
      GtkTreeIter iter;
      gtk_list_store_append(list, &iter);
      gtk_list_store_set(list, &iter, 0, garmin_syms[i].sym, 1, get_wp_sym_from_index(i), -1);
    }
  }
}


