/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2008, Quy Tonthat <qtonthat@gmail.com>
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
#include "icons/icons.h"

#include <string.h>
#include <stdlib.h>

static struct {
  gchar *sym;     /* icon names used by gpsbabel, garmin */
  gchar *old_sym; /* keep backward compatible */
  gint num;
  gchar *desc;
  const GdkPixdata *data;
  const GdkPixdata *data_large;
  GdkPixbuf *icon;
} garmin_syms[] = {
  /* "sym" are in all lower case. This is because viking convert symbol
   * names to lowercase when reading gpx files (see gpx.c:gpx_end()).
   * This method works with gpsbable (when viking upload back to GPS
   * device), but I don't know it works with other application.
   * The method (lower case for sym, that is) is more eficient but if
   * it is not compatible to all application, change sym to the right
   * case (see http://www.gpsbabel.org/) change gpx.c not to convert
   * symbol to lower case and use g_utf8_casefold() in key_equal_func
   * for g_hash_table_new_full() (see init_icons() in this file).
   * Quy Tonthat <qtonthat@gmail.com>
   */
  /*---------------------------------------------------------------
    Marine symbols
    ---------------------------------------------------------------*/
  { "marina",                "anchor",         0,     "white anchor symbol",               &wp_anchor_pixbuf,          &wp_anchor_large_pixbuf,          NULL },
  { "bell",                  "bell",           1,     "white bell symbol",                 &wp_bell_pixbuf,            &wp_bell_large_pixbuf,            NULL },
  { "green diamon",          "diamond_grn",    2,     "green diamond symbol",              &wp_diamond_grn_pixbuf,     NULL,            NULL },
  { "red diamon",            "diamond_red",    3,     "red diamond symbol",                &wp_diamond_red_pixbuf,     NULL,            NULL },
  { "diver down flag 1",     "dive1",          4,     "diver down flag 1",                 &wp_dive1_pixbuf,           &wp_dive1_large_pixbuf,           NULL },
  { "diver down flag 2",     "dive2",          5,     "diver down flag 2",                 &wp_dive2_pixbuf,           &wp_dive2_large_pixbuf,           NULL },
  { "bank",                  "dollar",         6,     "white dollar symbol",               &wp_dollar_pixbuf,          &wp_dollar_large_pixbuf,          NULL },
  { "fishing area",          "fish",           7,     "white fish symbol",                 &wp_fish_pixbuf,            &wp_fish_large_pixbuf,            NULL },
  { "gas station",           "fuel",           8,     "white fuel symbol",                 &wp_fuel_pixbuf,            &wp_fuel_large_pixbuf,            NULL },
  { "horn",                  "horn",           9,     "white horn symbol",                 &wp_horn_pixbuf,            &wp_horn_large_pixbuf,         NULL },
  { "residence",             "house",          10,    "white house symbol",                &wp_house_pixbuf,           &wp_house_large_pixbuf,           NULL },
  { "restaurant",            "knife",          11,    "white knife & fork symbol",         &wp_knife_pixbuf,           &wp_knife_large_pixbuf,           NULL },
  { "light",                 "light",          12,    "white light symbol",                &wp_light_pixbuf,           &wp_light_large_pixbuf,           NULL },
  { "bar",                   "mug",            13,    "white mug symbol",                  &wp_mug_pixbuf,             &wp_mug_large_pixbuf,            NULL },
  { "skull and crossbones",  "skull",          14,    "white skull and crossbones symbol", &wp_skull_pixbuf,           &wp_skull_large_pixbuf,          NULL },
  { "green square",          "square_grn",     15,    "green square symbol",               &wp_square_grn_pixbuf,      NULL,            NULL },
  { "red square",            "square_red",     16,    "red square symbol",                 &wp_square_red_pixbuf,      NULL,            NULL },
  { "white buoy",            "wbuoy",          17,    "white buoy waypoint symbol",        &wp_wbuoy_pixbuf,           &wp_wbuoy_large_pixbuf,           NULL },
  { "waypoint",              "wpt_dot",        18,    "waypoint dot",                      &wp_wpt_dot_pixbuf,         NULL,            NULL },
  { "shipwreck",             "wreck",          19,    "white wreck symbol",                &wp_wreck_pixbuf,           &wp_wreck_large_pixbuf,           NULL },
  { "none",                  "null",           20,    "null symbol (transparent)",         &wp_null_pixbuf,            NULL,            NULL },
  { "man overboard",         "mob",            21,    "man overboard symbol",              &wp_mob_pixbuf,             &wp_mob_large_pixbuf,            NULL },
  { "navaid, amber",         "buoy_ambr",      22,    "amber map buoy symbol",             &wp_buoy_ambr_pixbuf,       &wp_buoy_ambr_large_pixbuf,       NULL },
  { "navaid, black",         "buoy_blck",      23,    "black map buoy symbol",             &wp_buoy_blck_pixbuf,       &wp_buoy_blck_large_pixbuf,       NULL },
  { "navaid, blue",          "buoy_blue",      24,    "blue map buoy symbol",              &wp_buoy_blue_pixbuf,       &wp_buoy_blue_large_pixbuf,       NULL },
  { "navaid, green",         "buoy_grn",       25,    "green map buoy symbol",             &wp_buoy_grn_pixbuf,        &wp_buoy_grn_large_pixbuf,        NULL },
  { "navaid, green/Red",     "buoy_grn_red",   26,    "green/red map buoy symbol",         &wp_buoy_grn_red_pixbuf,    &wp_buoy_grn_red_large_pixbuf,    NULL },
  { "navaid, green/White",   "buoy_grn_wht",   27,    "green/white map buoy symbol",       &wp_buoy_grn_wht_pixbuf,    &wp_buoy_grn_wht_large_pixbuf,    NULL },
  { "navaid, orange",        "buoy_orng",      28,    "orange map buoy symbol",            &wp_buoy_orng_pixbuf,       &wp_buoy_orng_large_pixbuf,       NULL },
  { "navaid, red",           "buoy_red",       29,    "red map buoy symbol",               &wp_buoy_red_pixbuf,        &wp_buoy_red_large_pixbuf,        NULL },
  { "navaid, red/green",     "buoy_red_grn",   30,    "red/green map buoy symbol",         &wp_buoy_red_grn_pixbuf,    &wp_buoy_red_grn_large_pixbuf,    NULL },
  { "navaid, red/white",     "buoy_red_wht",   31,    "red/white map buoy symbol",         &wp_buoy_red_wht_pixbuf,    &wp_buoy_red_wht_large_pixbuf,    NULL },
  { "navaid, violet",        "buoy_violet",    32,    "violet map buoy symbol",            &wp_buoy_violet_pixbuf,     &wp_buoy_violet_large_pixbuf,     NULL },
  { "navaid, white",         "buoy_wht",       33,    "white map buoy symbol",             &wp_buoy_wht_pixbuf,        &wp_buoy_wht_large_pixbuf,        NULL },
  { "navaid, whit/green",    "buoy_wht_grn",   34,    "white/green map buoy symbol",       &wp_buoy_wht_grn_pixbuf,    &wp_buoy_wht_grn_large_pixbuf,    NULL },
  { "navaid, white/red",     "buoy_wht_red",   35,    "white/red map buoy symbol",         &wp_buoy_wht_red_pixbuf,    &wp_buoy_wht_red_large_pixbuf,    NULL },
  { "white dot",             "dot",            36,    "white dot symbol",                  &wp_dot_pixbuf,             NULL,            NULL },
  { "radio beacon",          "rbcn",           37,    "radio beacon symbol",               &wp_rbcn_pixbuf,            &wp_rbcn_large_pixbuf,            NULL },
  { "boat ramp",             "boat_ramp",      150,   "boat ramp symbol",                  &wp_boat_ramp_pixbuf,       &wp_boat_ramp_large_pixbuf,       NULL },
  { "campground",            "camp",           151,   "campground symbol",                 &wp_camp_pixbuf,            &wp_camp_large_pixbuf,            NULL },
  { "restroom",              "restrooms",      152,   "restrooms symbol",                  &wp_restroom_pixbuf,        &wp_restroom_large_pixbuf,        NULL },
  { "shower",                "showers",        153,   "shower symbol",                     &wp_shower_pixbuf,          &wp_shower_large_pixbuf,          NULL },
  { "drinking water",        "drinking_wtr",   154,   "drinking water symbol",             &wp_drinking_wtr_pixbuf,    &wp_drinking_wtr_large_pixbuf,    NULL },
  { "telephone",             "phone",          155,   "telephone symbol",                  &wp_phone_pixbuf,           &wp_phone_large_pixbuf,           NULL },
  { "medical facility",      "1st_aid",        156,   "first aid symbol",                  &wp_1st_aid_pixbuf,         &wp_1st_aid_large_pixbuf,         NULL },
  { "information",           "info",           157,   "information symbol",                &wp_info_pixbuf,            &wp_info_large_pixbuf,            NULL },
  { "parking area",          "parking",        158,   "parking symbol",                    &wp_parking_pixbuf,         &wp_parking_large_pixbuf,         NULL },
  { "park",                  "park",           159,   "park symbol",                       &wp_park_pixbuf,            &wp_park_large_pixbuf,            NULL },
  { "picnic area",           "picnic",         160,   "picnic symbol",                     &wp_picnic_pixbuf,          &wp_picnic_large_pixbuf,          NULL },
  { "scenic",                "scenic",         161,   "scenic area symbol",                &wp_scenic_pixbuf,          &wp_scenic_large_pixbuf,          NULL },
  { "skiing area",           "skiing",         162,   "skiing symbol",                     &wp_skiing_pixbuf,          &wp_skiing_large_pixbuf,          NULL },
  { "swimming area",         "swimming",       163,   "swimming symbol",                   &wp_swimming_pixbuf,        &wp_swimming_large_pixbuf,        NULL },
  { "dam",                   "dam",            164,   "dam symbol",                        &wp_dam_pixbuf,             &wp_dam_large_pixbuf,            NULL },
  { "controlled area",       "controlled",     165,   "controlled area symbol",            &wp_controlled_pixbuf,      &wp_controlled_large_pixbuf,      NULL },
  { "danger area",           "danger",         166,   "danger symbol",                     &wp_danger_pixbuf,          &wp_danger_large_pixbuf,          NULL },
  { "restricted area",       "restricted",     167,   "restricted area symbol",            &wp_restricted_pixbuf,      &wp_restricted_large_pixbuf,            NULL },
  { "null 2",                "null_2",         168,   "null symbol",                       NULL,                NULL,            NULL },  /* not exist */
  { "ball park",             "ball",           169,   "ball symbol",                       &wp_ball_pixbuf,            &wp_ball_large_pixbuf,            NULL },
  { "car",                   "car",            170,   "car symbol",                        &wp_car_pixbuf,             &wp_car_large_pixbuf,            NULL },
  { "hunting area",          "deer",           171,   "deer symbol",                       &wp_deer_pixbuf,            &wp_deer_large_pixbuf,            NULL },
  { "shopping center",       "shopping",     172,   "shopping cart symbol",              NULL,                &wp_shopping_large_pixbuf,            NULL },
  { "lodging",               "lodging",        173,   "lodging symbol",                    NULL,                &wp_lodging_large_pixbuf,            NULL },
  { "mine",                  "mine",           174,   "mine symbol",                       &wp_mine_pixbuf,            &wp_mine_large_pixbuf,            NULL },
  { "trail head",            "trail_head",     175,   "trail head symbol",                 NULL,                &wp_trail_head_large_pixbuf,            NULL },
  { "truck stop",            "truck_stop",     176,   "truck stop symbol",                 NULL,                &wp_truck_stop_large_pixbuf,            NULL },
  { "exit",                  "user_exit",      177,   "user exit symbol",                  NULL,                NULL,            NULL },
  { "flag",                  "flag",           178,   "flag symbol",                       &wp_flag_pixbuf,            NULL,            NULL },
  { "circle with x",         "circle_x",       179,   "circle with x in the center",       NULL,                NULL,            NULL },
  { "open 24 hours",          "open_24hr",      180,   "open 24 hours symbol",              NULL,                NULL,            NULL },
  { "fishing hot spot facility",      "fhs_facility",   181,   "U Fishing Hot SpotsTM Facility",    NULL,                &wp_fhs_facility_large_pixbuf,  NULL },
  { "bottom conditions",      "bot_cond",       182,   "Bottom Conditions",                 NULL,                NULL,            NULL },
  { "tide/current prediction station", "tide_pred_stn",  183,   "Tide/Current Prediction Station",   NULL,                NULL,            NULL },
  { "anchor prohibited",     "anchor_prohib",  184,   "U anchor prohibited symbol",        NULL,                NULL,            NULL },
  { "beacon",              "beacon",         185,   "U beacon symbol",                   NULL,                NULL,            NULL },
  { "coast Guard",         "coast_guard",    186,   "U coast guard symbol",              NULL,                NULL,            NULL },
  { "reef",                "reef",           187,   "U reef symbol",                     NULL,                NULL,            NULL },
  { "weed bed",             "weedbed",        188,   "U weedbed symbol",                  NULL,                NULL,            NULL },
  { "dropoff",              "dropoff",        189,   "U dropoff symbol",                  NULL,                NULL,            NULL },
  { "dock",                "dock",           190,   "U dock symbol",                     NULL,                NULL,            NULL },
  { "u marina",              "marina",         191,   "U marina symbol",                   NULL,                NULL,            NULL },
  { "bait and tackle",     "bait_tackle",    192,   "U bait and tackle symbol",          NULL,                NULL,            NULL },
  { "stump",               "stump",          193,   "U stump symbol",                    NULL,                NULL,            NULL },
  { "ground transportation", "grnd_trans",   229,   "ground transportation",                    NULL,                &wp_grnd_trans_large_pixbuf,        NULL },
  /*---------------------------------------------------------------
    User customizable symbols
    The values from begin_custom to end_custom inclusive are
    reserved for the identification of user customizable symbols.
    ---------------------------------------------------------------*/
  { "custom begin placeholder",   "begin_custom",   7680,  "first user customizable symbol",    NULL,                NULL,            NULL },
  { "custom end placeholder","end_custom",     8191,  "last user customizable symbol",     NULL,                NULL,            NULL },
  /*---------------------------------------------------------------
    Land symbols
    ---------------------------------------------------------------*/
  { "interstate highway",    "is_hwy",         8192,  "interstate hwy symbol",             NULL,                NULL,            NULL },   /* TODO: check symbol name */
  { "us hwy",            "us_hwy",         8193,  "us hwy symbol",                     NULL,                NULL,            NULL },
  { "state hwy",         "st_hwy",         8194,  "state hwy symbol",                  NULL,                NULL,            NULL },
  { "mile marker",           "mi_mrkr",        8195,  "mile marker symbol",                NULL,                NULL,            NULL },
  { "tracBack point",        "trcbck",         8196,  "TracBack (feet) symbol",            NULL,                NULL,            NULL },
  { "golf course",           "golf",           8197,  "golf symbol",                       &wp_golf_pixbuf,            &wp_golf_large_pixbuf,            NULL },
  { "city (small)",          "sml_cty",        8198,  "small city symbol",                 &wp_sml_cty_pixbuf,         &wp_sml_cty_large_pixbuf,            NULL },
  { "city (medium)",         "med_cty",        8199,  "medium city symbol",                &wp_med_cty_pixbuf,         &wp_med_cty_large_pixbuf,            NULL },
  { "city (large)",          "lrg_cty",        8200,  "large city symbol",                 &wp_lrg_cty_pixbuf,         &wp_lrg_cty_large_pixbuf,            NULL },
  { "intl freeway hwy",               "freeway",        8201,  "intl freeway hwy symbol",           NULL,                NULL,            NULL },
  { "intl national hwy",      "ntl_hwy",        8202,  "intl national hwy symbol",          NULL,                NULL,            NULL },
  { "city (capitol)",          "cap_cty",        8203,  "capitol city symbol (star)",        &wp_cap_cty_pixbuf,         NULL,            NULL },
  { "amusement park",        "amuse_pk",       8204,  "amusement park symbol",             NULL,                &wp_amuse_pk_large_pixbuf,            NULL },
  { "bowling",                "bowling",        8205,  "bowling symbol",                    NULL,                &wp_bowling_large_pixbuf,            NULL },
  { "car rental",            "car_rental",     8206,  "car rental symbol",                 NULL,                &wp_car_rental_large_pixbuf,            NULL },
  { "car repair",            "car_repair",     8207,  "car repair symbol",                 NULL,                &wp_car_repair_large_pixbuf,            NULL },
  { "fast food",             "fastfood",       8208,  "fast food symbol",                  NULL,                &wp_fastfood_large_pixbuf,            NULL },
  { "fitness center",        "fitness",        8209,  "fitness symbol",                    NULL,                &wp_fitness_large_pixbuf,            NULL },
  { "movie theater",         "movie",          8210,  "movie symbol",                      NULL,                &wp_movie_large_pixbuf,            NULL },
  { "museum",                "museum",         8211,  "museum symbol",                     NULL,                &wp_museum_large_pixbuf,            NULL },
  { "pharmacy",              "pharmacy",       8212,  "pharmacy symbol",                   NULL,                &wp_pharmacy_large_pixbuf,            NULL },
  { "pizza",                 "pizza",          8213,  "pizza symbol",                      NULL,                &wp_pizza_large_pixbuf,            NULL },
  { "post office",           "post_ofc",       8214,  "post office symbol",                NULL,                &wp_post_ofc_large_pixbuf,            NULL },
  { "rv park",               "rv_park",        8215,  "RV park symbol",                    &wp_rv_park_pixbuf,  &wp_rv_park_large_pixbuf,            NULL },
  { "school",                "school",         8216,  "school symbol",                     &wp_school_pixbuf,   &wp_school_large_pixbuf,            NULL },
  { "stadium",               "stadium",        8217,  "stadium symbol",                    NULL,                &wp_stadium_large_pixbuf,            NULL },
  { "department store",      "store",          8218,  "dept. store symbol",                NULL,                &wp_store_large_pixbuf,            NULL },
  { "zoo",                   "zoo",            8219,  "zoo symbol",                        NULL,                &wp_zoo_large_pixbuf,            NULL },
  { "convenience store",     "conv_store",       8220,  "convenience store symbol",          NULL,                &wp_conv_store_large_pixbuf,        NULL },
  { "live theater",          "theater",          8221,  "live theater symbol",               NULL,                &wp_theater_large_pixbuf,            NULL },
  { "ramp intersection",     "ramp_int",       8222,  "ramp intersection symbol",          NULL,                NULL,            NULL },
  { "street intersection",   "st_int",         8223,  "street intersection symbol",        NULL,                NULL,            NULL },
  { "scales",                "weigh_station",     8226,  "inspection/weigh station symbol",   NULL,               &wp_weigh_station_large_pixbuf,       NULL },
  { "toll booth",            "toll_booth",     8227,  "toll booth symbol",                 NULL,                &wp_toll_booth_large_pixbuf,            NULL },
  { "elevation point",       "elev_pt",        8228,  "elevation point symbol",            NULL,                NULL,            NULL },
  { "exit without services", "ex_no_srvc",     8229,  "exit without services symbol",      NULL,                NULL,            NULL },
  { "geographic place name, man-made", "geo_place_mm",   8230,  "Geographic place name, man-made",   NULL,                NULL,            NULL },
  { "geographic place name, water","geo_place_wtr",  8231,  "Geographic place name, water",      NULL,                NULL,            NULL },
  { "geographic place name, land", "geo_place_lnd",  8232,  "Geographic place name, land",       NULL,                NULL,            NULL },
  { "bridge",                "bridge",         8233,  "bridge symbol",                     &wp_bridge_pixbuf,          &wp_bridge_large_pixbuf,            NULL },
  { "building",              "building",       8234,  "building symbol",                   &wp_building_pixbuf,        &wp_building_large_pixbuf,        NULL },
  { "cemetery",              "cemetery",       8235,  "cemetery symbol",                   &wp_cemetery_pixbuf,        &wp_cemetery_large_pixbuf,            NULL },
  { "church",                "church",         8236,  "church symbol",                     &wp_church_pixbuf,          &wp_church_large_pixbuf,          NULL },
  { "civil",                 "civil",          8237,  "civil location symbol",             NULL,                &wp_civil_large_pixbuf,            NULL },
  { "crossing",              "crossing",       8238,  "crossing symbol",                   NULL,                &wp_crossing_large_pixbuf,            NULL },
  { "ghost town",            "hist_town",      8239,  "historical town symbol",            NULL,                NULL,            NULL },
  { "levee",                 "levee",          8240,  "levee symbol",                      NULL,                NULL,            NULL },
  { "military",              "military",       8241,  "military location symbol",          &wp_military_pixbuf,        NULL,            NULL },
  { "oil field",             "oil_field",      8242,  "oil field symbol",                  NULL,                &wp_oil_field_large_pixbuf,          NULL },
  { "tunnel",                "tunnel",         8243,  "tunnel symbol",                     &wp_tunnel_pixbuf,          &wp_tunnel_large_pixbuf,          NULL },
  { "beach",                 "beach",          8244,  "beach symbol",                      &wp_beach_pixbuf,           &wp_beach_large_pixbuf,           NULL },
  { "forest",                "forest",         8245,  "forest symbol",                     &wp_forest_pixbuf,          &wp_forest_large_pixbuf,          NULL },
  { "summit",                "summit",         8246,  "summit symbol",                     &wp_summit_pixbuf,          &wp_summit_large_pixbuf,          NULL },
  { "large ramp intersection", "lrg_ramp_int",   8247,  "large ramp intersection symbol",    NULL,                NULL,            NULL },
  { "large exit without services", "lrg_ex_no_srvc", 8248,  "large exit without services smbl",  NULL,                NULL,            NULL },
  { "police station",        "police",          8249,  "police/official badge symbol",      NULL,                &wp_police_large_pixbuf,            NULL },
  { "gambling/casino",                "cards",          8250,  "gambling/casino symbol",            NULL,                NULL,            NULL },
  { "ski resort",            "ski_resort",        8251,  "snow skiing symbol",                NULL,                &wp_ski_resort_large_pixbuf,          NULL },
  { "ice skating",           "ice_skating",       8252,  "ice skating symbol",                &wp_ice_skating_pixbuf,     &wp_ice_skating_large_pixbuf,  NULL },
  { "wrecker",               "wrecker",        8253,  "tow truck (wrecker) symbol",        NULL,                &wp_wrecker_large_pixbuf,            NULL },
  { "border crossing (port of entry)", "border",         8254,  "border crossing (port of entry)",   NULL,                NULL,            NULL },
  { "geocache",              "geocache",       8255,  "geocache location",                 &wp_geocache_pixbuf,        &wp_geocache_large_pixbuf,        NULL },
  { "geocache found",        "geocache_fnd",   8256,  "found geocache",                    &wp_geocache_fnd_pixbuf,    &wp_geocache_fnd_large_pixbuf,    NULL },
  { "contact, smiley",       "cntct_smiley",   8257,  "Rino contact symbol, ""smiley""",   NULL,                NULL,            NULL },
  { "contact, ball cap",     "cntct_ball_cap", 8258,  "Rino contact symbol, ""ball cap""", NULL,                NULL,            NULL },
  { "contact, big ears",      "cntct_big_ears", 8259,  "Rino contact symbol, ""big ear""",  NULL,                NULL,            NULL },
  { "contact, spike",         "cntct_spike",    8260,  "Rino contact symbol, ""spike""",    NULL,                NULL,            NULL },
  { "contact, goatee",        "cntct_goatee",   8261,  "Rino contact symbol, ""goatee""",   NULL,                NULL,            NULL },
  { "contact, afro",          "cntct_afro",     8262,  "Rino contact symbol, ""afro""",     NULL,                NULL,            NULL },
  { "contact, dreadlocks",    "cntct_dreads",   8263,  "Rino contact symbol, ""dreads""",   NULL,                NULL,            NULL },
  { "contact, female1",       "cntct_female1",  8264,  "Rino contact symbol, ""female 1""", NULL,                NULL,            NULL },
  { "contact, female2",       "cntct_female2",  8265,  "Rino contact symbol, ""female 2""", NULL,                NULL,            NULL },
  { "contact, female3",       "cntct_female3",  8266,  "Rino contact symbol, ""female 3""", NULL,                NULL,            NULL },
  { "contact, ranger",        "cntct_ranger",   8267,  "Rino contact symbol, ""ranger""",   NULL,                NULL,            NULL },
  { "contact, kung-Fu",       "cntct_kung_fu",  8268,  "Rino contact symbol, ""kung fu""",  NULL,                NULL,            NULL },
  { "contact, sumo",          "cntct_sumo",     8269,  "Rino contact symbol, ""sumo""",     NULL,                NULL,            NULL },
  { "contact, pirate",        "cntct_pirate",   8270,  "Rino contact symbol, ""pirate""",   NULL,                NULL,            NULL },
  { "contact, biker",         "cntct_biker",    8271,  "Rino contact symbol, ""biker""",    NULL,                NULL,            NULL },
  { "contact, alien",         "cntct_alien",    8272,  "Rino contact symbol, ""alien""",    NULL,                NULL,            NULL },
  { "contact, bug",           "cntct_bug",      8273,  "Rino contact symbol, ""bug""",      NULL,                NULL,            NULL },
  { "contact, cat",           "cntct_cat",      8274,  "Rino contact symbol, ""cat""",      NULL,                NULL,            NULL },
  { "contact, dog",           "cntct_dog",      8275,  "Rino contact symbol, ""dog""",      NULL,                NULL,            NULL },
  { "contact, pig",           "cntct_pig",      8276,  "Rino contact symbol, ""pig""",      NULL,                NULL,            NULL },
  { "water hydrant",          "hydrant",        8282,  "water hydrant symbol",              NULL,                NULL,            NULL },
  { "flag, blue",             "flag_blue",      8284,  "blue flag symbol",                  NULL,                &wp_flag_blue_large_pixbuf,            NULL },
  { "flag, green",            "flag_green",     8285,  "green flag symbol",                 NULL,                &wp_flag_green_large_pixbuf,            NULL },
  { "flag, red",              "flag_red",       8286,  "red flag symbol",                   NULL,                &wp_flag_red_large_pixbuf,            NULL },
  { "pin, blue",              "pin_blue",       8287,  "blue pin symbol",                   NULL,                &wp_pin_blue_large_pixbuf,            NULL },
  { "pin, green",             "pin_green",      8288,  "green pin symbol",                  NULL,                &wp_pin_green_large_pixbuf,            NULL },
  { "pin, red",               "pin_red",        8289,  "red pin symbol",                    NULL,                &wp_pin_red_large_pixbuf,            NULL },
  { "block, blue",            "block_blue",     8290,  "blue block symbol",                 NULL,                &wp_block_blue_large_pixbuf,            NULL },
  { "block, green",           "block_green",    8291,  "green block symbol",                NULL,                &wp_block_green_large_pixbuf,           NULL },
  { "block, red",             "block_red",      8292,  "red block symbol",                  NULL,                &wp_block_red_large_pixbuf,            NULL },
  { "bike trail",             "bike_trail",     8293,  "bike trail symbol",                 NULL,                &wp_bike_trail_large_pixbuf,            NULL },
  { "circle, red",            "circle_red",     8294,  "red circle symbol",                 NULL,                NULL,            NULL },
  { "circle, green",          "circle_green",   8295,  "green circle symbol",               NULL,                NULL,            NULL },
  { "circle, blue",           "circle_blue",    8296,  "blue circle symbol",                NULL,                NULL,            NULL },
  { "diamond, blue",          "diamond_blue",   8299,  "blue diamond symbol",               NULL,                NULL,            NULL },
  { "oval, red",              "oval_red",       8300,  "red oval symbol",                   NULL,                NULL,            NULL },
  { "oval, green",            "oval_green",     8301,  "green oval symbol",                 NULL,                NULL,            NULL },
  { "oval, blue",             "oval_blue",      8302,  "blue oval symbol",                  NULL,                NULL,            NULL },
  { "rectangle, red",         "rect_red",       8303,  "red rectangle symbol",              NULL,                NULL,            NULL },
  { "rectangle, green",       "rect_green",     8304,  "green rectangle symbol",            NULL,                NULL,            NULL },
  { "rectangle, blue",        "rect_blue",      8305,  "blue rectangle symbol",             NULL,                NULL,            NULL },
  { "square, blue",           "square_blue",    8308,  "blue square symbol",                NULL,                NULL,            NULL },
  { "letter a, red",          "letter_a_red",   8309,  "red letter 'A' symbol",             NULL,                NULL,            NULL },
  { "letter b, red",          "letter_b_red",   8310,  "red letter 'B' symbol",             NULL,                NULL,            NULL },
  { "letter c, red",          "letter_c_red",   8311,  "red letter 'C' symbol",             NULL,                NULL,            NULL },
  { "letter d, red",          "letter_d_red",   8312,  "red letter 'D' symbol",             NULL,                NULL,            NULL },
  { "letter a, green",        "letter_a_green", 8313,  "green letter 'A' symbol",           NULL,                NULL,            NULL },
  { "letter c, green",        "letter_c_green", 8314,  "green letter 'C' symbol",           NULL,                NULL,            NULL },
  { "letter b, green",        "letter_b_green", 8315,  "green letter 'B' symbol",           NULL,                NULL,            NULL },
  { "letter d, green",        "letter_d_green", 8316,  "green letter 'D' symbol",           NULL,                NULL,            NULL },
  { "letter a, blue",         "letter_a_blue",  8317,  "blue letter 'A' symbol",            NULL,                NULL,            NULL },
  { "letter b, blue",         "letter_b_blue",  8318,  "blue letter 'B' symbol",            NULL,                NULL,            NULL },
  { "letter c, blue",         "letter_c_blue",  8319,  "blue letter 'C' symbol",            NULL,                NULL,            NULL },
  { "letter d, blue",         "letter_d_blue",  8320,  "blue letter 'D' symbol",            NULL,                NULL,            NULL },
  { "number 0, red",          "number_0_red",   8321,  "red number '0' symbol",             NULL,                NULL,            NULL },
  { "number 1, red",          "number_1_red",   8322,  "red number '1' symbol",             NULL,                NULL,            NULL },
  { "number 2, red",          "number_2_red",   8323,  "red number '2' symbol",             NULL,                NULL,            NULL },
  { "number 3, red",          "number_3_red",   8324,  "red number '3' symbol",             NULL,                NULL,            NULL },
  { "number 4, red",          "number_4_red",   8325,  "red number '4' symbol",             NULL,                NULL,            NULL },
  { "number 5, red",          "number_5_red",   8326,  "red number '5' symbol",             NULL,                NULL,            NULL },
  { "number 6, red",          "number_6_red",   8327,  "red number '6' symbol",             NULL,                NULL,            NULL },
  { "number 7, red",          "number_7_red",   8328,  "red number '7' symbol",             NULL,                NULL,            NULL },
  { "number 8, red",          "number_8_red",   8329,  "red number '8' symbol",             NULL,                NULL,            NULL },
  { "number 9, red",          "number_9_red",   8330,  "red number '9' symbol",             NULL,                NULL,            NULL },
  { "number 0, green",        "number_0_green", 8331,  "green number '0' symbol",           NULL,                NULL,            NULL },
  { "number 1, green",        "number_1_green", 8332,  "green number '1' symbol",           NULL,                NULL,            NULL },
  { "number 2, green",        "number_2_green", 8333,  "green number '2' symbol",           NULL,                NULL,            NULL },
  { "number 3, green",        "number_3_green", 8334,  "green number '3' symbol",           NULL,                NULL,            NULL },
  { "number 4, green",        "number_4_green", 8335,  "green number '4' symbol",           NULL,                NULL,            NULL },
  { "number 5, green",        "number_5_green", 8336,  "green number '5' symbol",           NULL,                NULL,            NULL },
  { "number 6, green",        "number_6_green", 8337,  "green number '6' symbol",           NULL,                NULL,            NULL },
  { "number 7, green",        "number_7_green", 8338,  "green number '7' symbol",           NULL,                NULL,            NULL },
  { "number 8, green",        "number_8_green", 8339,  "green number '8' symbol",           NULL,                NULL,            NULL },
  { "number 9, green",        "number_9_green", 8340,  "green number '9' symbol",           NULL,                NULL,            NULL },
  { "number 0, blue",         "number_0_blue",  8341,  "blue number '0' symbol",            NULL,                NULL,            NULL },
  { "number 1, blue",         "number_1_blue",  8342,  "blue number '1' symbol",            NULL,                NULL,            NULL },
  { "number 2, blue",         "number_2_blue",  8343,  "blue number '2' symbol",            NULL,                NULL,            NULL },
  { "number 3, blue",         "number_3_blue",  8344,  "blue number '3' symbol",            NULL,                NULL,            NULL },
  { "number 4, blue",         "number_4_blue",  8345,  "blue number '4' symbol",            NULL,                NULL,            NULL },
  { "number 5, blue",         "number_5_blue",  8346,  "blue number '5' symbol",            NULL,                NULL,            NULL },
  { "number 6, blue",         "number_6_blue",  8347,  "blue number '6' symbol",            NULL,                NULL,            NULL },
  { "number 7, blue",         "number_7_blue",  8348,  "blue number '7' symbol",            NULL,                NULL,            NULL },
  { "number 8, blue",         "number_8_blue",  8349,  "blue number '8' symbol",            NULL,                NULL,            NULL },
  { "number 9, blue",         "number_9_blue",  8350,  "blue number '9' symbol",            NULL,                NULL,            NULL },
  { "triangle, blue",         "triangle_blue",  8351,  "blue triangle symbol",              NULL,                NULL,            NULL },
  { "triangle, green",        "triangle_green", 8352,  "green triangle symbol",             NULL,                NULL,            NULL },
  { "triangle, red",          "triangle_red",   8353,  "red triangle symbol",               NULL,                NULL,            NULL },
  /*---------------------------------------------------------------
    Aviation symbols
    ---------------------------------------------------------------*/
  { "airport",                "airport",        16384, "airport symbol",                    &wp_airplane_pixbuf,        &wp_airplane_large_pixbuf,        NULL },
  { "intersection",           "int",            16385, "intersection symbol",               NULL,                NULL,            NULL },
  { "non-directional beacon", "ndb",            16386, "non-directional beacon symbol",     NULL,                NULL,            NULL },
  { "vhf omni-range",         "vor",            16387, "VHF omni-range symbol",             NULL,                NULL,            NULL },
  { "heliport",               "heliport",       16388, "heliport symbol",                   NULL,                NULL,            NULL },
  { "private field",          "private",        16389, "private field symbol",              NULL,                NULL,            NULL },
  { "soft field",             "soft_fld",       16390, "soft field symbol",                 NULL,                NULL,            NULL },
  { "tall tower",             "tall_tower",     16391, "tall tower symbol",                 NULL,                &wp_tall_tower_large_pixbuf,           NULL },
  { "short tower",             "short_tower",    16392, "short tower symbol",                NULL,                &wp_short_tower_large_pixbuf,          NULL },
  { "glider area",            "glider",         16393, "glider symbol",                     NULL,                &wp_glider_large_pixbuf,            NULL },
  { "ultralight area",        "ultralight",     16394, "ultralight symbol",                 NULL,                &wp_ultralight_large_pixbuf,            NULL },
  { "parachute area",         "parachute",      16395, "parachute symbol",                  NULL,                &wp_parachute_large_pixbuf,            NULL },
  { "vor/tacan",              "vortac",         16396, "VOR/TACAN symbol",                  NULL,                NULL,            NULL },
  { "vor-dme",                "vordme",         16397, "VOR-DME symbol",                    NULL,                NULL,            NULL },
  { "first approach fix",     "faf",            16398, "first approach fix",                NULL,                NULL,            NULL },
  { "localizer outer marker", "lom",            16399, "localizer outer marker",            NULL,                NULL,            NULL },
  { "missed approach point",  "map",            16400, "missed approach point",             NULL,                NULL,            NULL },
  { "tacan",                  "tacan",          16401, "TACAN symbol",                      NULL,                NULL,            NULL },
  { "seaplane base",          "seaplane",       16402, "Seaplane Base",                     NULL,                NULL,            NULL }
};

static GHashTable *icons = NULL;
static GHashTable *old_icons = NULL;

static void init_icons() {
  icons = g_hash_table_new_full ( g_str_hash, g_str_equal, NULL, NULL);
  old_icons = g_hash_table_new_full ( g_str_hash, g_str_equal, NULL, NULL);
  gint i;
  for (i=0; i<G_N_ELEMENTS(garmin_syms); i++) {
    g_hash_table_insert(icons, garmin_syms[i].sym, GINT_TO_POINTER (i));
    g_hash_table_insert(old_icons, garmin_syms[i].old_sym, GINT_TO_POINTER (i));
  }
}

static GdkPixbuf *get_wp_sym_from_index ( gint i ) {
  if ( !garmin_syms[i].icon &&
      ((!a_vik_get_use_large_waypoint_icons() && garmin_syms[i].data) ||
       (a_vik_get_use_large_waypoint_icons() && garmin_syms[i].data_large))) {
    garmin_syms[i].icon = gdk_pixbuf_from_pixdata (
       a_vik_get_use_large_waypoint_icons() ? garmin_syms[i].data_large : garmin_syms[i].data,
       FALSE, NULL );
  }
  return garmin_syms[i].icon;
}

GdkPixbuf *a_get_wp_sym ( const gchar *sym ) {
  gpointer gp;
  gpointer x;

  if (!sym) {
    return NULL;
  }
  if (!icons) {
    init_icons();
  }
  if (g_hash_table_lookup_extended(icons, sym, &x, &gp))
    return get_wp_sym_from_index(GPOINTER_TO_INT(gp));
  else if (g_hash_table_lookup_extended(old_icons, sym, &x, &gp))
    return get_wp_sym_from_index(GPOINTER_TO_INT(gp));
  else
    return NULL;
}

void a_populate_sym_list ( GtkListStore *list ) {
  gint i;
  for (i=0; i<G_N_ELEMENTS(garmin_syms); i++) {
    if ((!a_vik_get_use_large_waypoint_icons() && garmin_syms[i].data) ||
        (a_vik_get_use_large_waypoint_icons() && garmin_syms[i].data_large)) {
      GtkTreeIter iter;
      gtk_list_store_append(list, &iter);
      gtk_list_store_set(list, &iter, 0, garmin_syms[i].sym, 1, get_wp_sym_from_index(i), -1);
    }
  }
}


/* Use when preferences have changed to reset icons*/
void clear_garmin_icon_syms () {
  g_debug("garminsymbols: clear_garmin_icon_syms");
  gint i;
  for (i=0; i<G_N_ELEMENTS(garmin_syms); i++) {
    if (garmin_syms[i].icon) {
      g_object_unref (garmin_syms[i].icon);
      garmin_syms[i].icon = NULL;
    }
  }
}
