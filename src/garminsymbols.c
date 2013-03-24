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
  /* "sym" are in 'Title Case' like in gpsbabel. This is needed for
   * devices like Garmin Oregon 450. Old exports with lower case
   * identifiers will be automatically converted to the version defined
   * inside the table by vikwaypoint.c:vik_waypoint_set_symbol().
   * The hash itself tries to keep all operations case independent
   * using str_equal_casefold() and str_hash_casefold(). This is
   * necessary to allow a_get_hashed_sym() to match the lower case
   * version with the identifier stored in "sym".
   */
  /*---------------------------------------------------------------
    Marine symbols
    ---------------------------------------------------------------*/
  { "Marina",                "anchor",         0,     "white anchor symbol",               &wp_anchor_pixbuf,          &wp_anchor_large_pixbuf,          NULL },
  { "Bell",                  "bell",           1,     "white bell symbol",                 &wp_bell_pixbuf,            &wp_bell_large_pixbuf,            NULL },
  { "Green Diamon",          "diamond_grn",    2,     "green diamond symbol",              &wp_diamond_grn_pixbuf,     NULL,            NULL },
  { "Red Diamon",            "diamond_red",    3,     "red diamond symbol",                &wp_diamond_red_pixbuf,     NULL,            NULL },
  { "Diver Down Flag 1",     "dive1",          4,     "diver down flag 1",                 &wp_dive1_pixbuf,           &wp_dive1_large_pixbuf,           NULL },
  { "Diver Down Flag 2",     "dive2",          5,     "diver down flag 2",                 &wp_dive2_pixbuf,           &wp_dive2_large_pixbuf,           NULL },
  { "Bank",                  "dollar",         6,     "white dollar symbol",               &wp_dollar_pixbuf,          &wp_dollar_large_pixbuf,          NULL },
  { "Fishing Area",          "fish",           7,     "white fish symbol",                 &wp_fish_pixbuf,            &wp_fish_large_pixbuf,            NULL },
  { "Gas Station",           "fuel",           8,     "white fuel symbol",                 &wp_fuel_pixbuf,            &wp_fuel_large_pixbuf,            NULL },
  { "Horn",                  "horn",           9,     "white horn symbol",                 &wp_horn_pixbuf,            &wp_horn_large_pixbuf,         NULL },
  { "Residence",             "house",          10,    "white house symbol",                &wp_house_pixbuf,           &wp_house_large_pixbuf,           NULL },
  { "Restaurant",            "knife",          11,    "white knife & fork symbol",         &wp_knife_pixbuf,           &wp_knife_large_pixbuf,           NULL },
  { "Light",                 "light",          12,    "white light symbol",                &wp_light_pixbuf,           &wp_light_large_pixbuf,           NULL },
  { "Bar",                   "mug",            13,    "white mug symbol",                  &wp_mug_pixbuf,             &wp_mug_large_pixbuf,            NULL },
  { "Skull and Crossbones",  "skull",          14,    "white skull and crossbones symbol", &wp_skull_pixbuf,           &wp_skull_large_pixbuf,          NULL },
  { "Green Square",          "square_grn",     15,    "green square symbol",               &wp_square_grn_pixbuf,      NULL,            NULL },
  { "Red Square",            "square_red",     16,    "red square symbol",                 &wp_square_red_pixbuf,      NULL,            NULL },
  { "Buoy, White",           "wbuoy",          17,    "white buoy waypoint symbol",        &wp_wbuoy_pixbuf,           &wp_wbuoy_large_pixbuf,           NULL },
  { "Waypoint",              "wpt_dot",        18,    "waypoint dot",                      &wp_wpt_dot_pixbuf,         NULL,            NULL },
  { "Shipwreck",             "wreck",          19,    "white wreck symbol",                &wp_wreck_pixbuf,           &wp_wreck_large_pixbuf,           NULL },
  { "None",                  "null",           20,    "null symbol (transparent)",         &wp_null_pixbuf,            NULL,            NULL },
  { "Man Overboard",         "mob",            21,    "man overboard symbol",              &wp_mob_pixbuf,             &wp_mob_large_pixbuf,            NULL },
  { "Navaid, Amber",         "buoy_ambr",      22,    "amber map buoy symbol",             &wp_buoy_ambr_pixbuf,       &wp_buoy_ambr_large_pixbuf,       NULL },
  { "Navaid, Black",         "buoy_blck",      23,    "black map buoy symbol",             &wp_buoy_blck_pixbuf,       &wp_buoy_blck_large_pixbuf,       NULL },
  { "Navaid, Blue",          "buoy_blue",      24,    "blue map buoy symbol",              &wp_buoy_blue_pixbuf,       &wp_buoy_blue_large_pixbuf,       NULL },
  { "Navaid, Green",         "buoy_grn",       25,    "green map buoy symbol",             &wp_buoy_grn_pixbuf,        &wp_buoy_grn_large_pixbuf,        NULL },
  { "Navaid, Green/Red",     "buoy_grn_red",   26,    "green/red map buoy symbol",         &wp_buoy_grn_red_pixbuf,    &wp_buoy_grn_red_large_pixbuf,    NULL },
  { "Navaid, Green/White",   "buoy_grn_wht",   27,    "green/white map buoy symbol",       &wp_buoy_grn_wht_pixbuf,    &wp_buoy_grn_wht_large_pixbuf,    NULL },
  { "Navaid, Orange",        "buoy_orng",      28,    "orange map buoy symbol",            &wp_buoy_orng_pixbuf,       &wp_buoy_orng_large_pixbuf,       NULL },
  { "Navaid, Red",           "buoy_red",       29,    "red map buoy symbol",               &wp_buoy_red_pixbuf,        &wp_buoy_red_large_pixbuf,        NULL },
  { "Navaid, Red/Green",     "buoy_red_grn",   30,    "red/green map buoy symbol",         &wp_buoy_red_grn_pixbuf,    &wp_buoy_red_grn_large_pixbuf,    NULL },
  { "Navaid, Red/White",     "buoy_red_wht",   31,    "red/white map buoy symbol",         &wp_buoy_red_wht_pixbuf,    &wp_buoy_red_wht_large_pixbuf,    NULL },
  { "Navaid, Violet",        "buoy_violet",    32,    "violet map buoy symbol",            &wp_buoy_violet_pixbuf,     &wp_buoy_violet_large_pixbuf,     NULL },
  { "Navaid, White",         "buoy_wht",       33,    "white map buoy symbol",             &wp_buoy_wht_pixbuf,        &wp_buoy_wht_large_pixbuf,        NULL },
  { "Navaid, White/Green",    "buoy_wht_grn",   34,    "white/green map buoy symbol",       &wp_buoy_wht_grn_pixbuf,    &wp_buoy_wht_grn_large_pixbuf,    NULL },
  { "Navaid, White/Red",     "buoy_wht_red",   35,    "white/red map buoy symbol",         &wp_buoy_wht_red_pixbuf,    &wp_buoy_wht_red_large_pixbuf,    NULL },
  { "White Dot",             "dot",            36,    "white dot symbol",                  &wp_dot_pixbuf,             NULL,            NULL },
  { "Radio Beacon",          "rbcn",           37,    "radio beacon symbol",               &wp_rbcn_pixbuf,            &wp_rbcn_large_pixbuf,            NULL },
  { "Boat Ramp",             "boat_ramp",      150,   "boat ramp symbol",                  &wp_boat_ramp_pixbuf,       &wp_boat_ramp_large_pixbuf,       NULL },
  { "Campground",            "camp",           151,   "campground symbol",                 &wp_camp_pixbuf,            &wp_camp_large_pixbuf,            NULL },
  { "Restroom",              "restrooms",      152,   "restrooms symbol",                  &wp_restroom_pixbuf,        &wp_restroom_large_pixbuf,        NULL },
  { "Shower",                "showers",        153,   "shower symbol",                     &wp_shower_pixbuf,          &wp_shower_large_pixbuf,          NULL },
  { "Drinking Water",        "drinking_wtr",   154,   "drinking water symbol",             &wp_drinking_wtr_pixbuf,    &wp_drinking_wtr_large_pixbuf,    NULL },
  { "Telephone",             "phone",          155,   "telephone symbol",                  &wp_phone_pixbuf,           &wp_phone_large_pixbuf,           NULL },
  { "Medical Facility",      "1st_aid",        156,   "first aid symbol",                  &wp_1st_aid_pixbuf,         &wp_1st_aid_large_pixbuf,         NULL },
  { "Information",           "info",           157,   "information symbol",                &wp_info_pixbuf,            &wp_info_large_pixbuf,            NULL },
  { "Parking Area",          "parking",        158,   "parking symbol",                    &wp_parking_pixbuf,         &wp_parking_large_pixbuf,         NULL },
  { "Park",                  "park",           159,   "park symbol",                       &wp_park_pixbuf,            &wp_park_large_pixbuf,            NULL },
  { "Picnic Area",           "picnic",         160,   "picnic symbol",                     &wp_picnic_pixbuf,          &wp_picnic_large_pixbuf,          NULL },
  { "Scenic Area",           "scenic",         161,   "scenic area symbol",                &wp_scenic_pixbuf,          &wp_scenic_large_pixbuf,          NULL },
  { "Skiing Area",           "skiing",         162,   "skiing symbol",                     &wp_skiing_pixbuf,          &wp_skiing_large_pixbuf,          NULL },
  { "Swimming Area",         "swimming",       163,   "swimming symbol",                   &wp_swimming_pixbuf,        &wp_swimming_large_pixbuf,        NULL },
  { "Dam",                   "dam",            164,   "dam symbol",                        &wp_dam_pixbuf,             &wp_dam_large_pixbuf,            NULL },
  { "Controlled Area",       "controlled",     165,   "controlled area symbol",            &wp_controlled_pixbuf,      &wp_controlled_large_pixbuf,      NULL },
  { "Danger Area",           "danger",         166,   "danger symbol",                     &wp_danger_pixbuf,          &wp_danger_large_pixbuf,          NULL },
  { "Restricted Area",       "restricted",     167,   "restricted area symbol",            &wp_restricted_pixbuf,      &wp_restricted_large_pixbuf,            NULL },
  { "Null 2",                "null_2",         168,   "null symbol",                       NULL,                NULL,            NULL },  /* not exist */
  { "Ball Park",             "ball",           169,   "ball symbol",                       &wp_ball_pixbuf,            &wp_ball_large_pixbuf,            NULL },
  { "Car",                   "car",            170,   "car symbol",                        &wp_car_pixbuf,             &wp_car_large_pixbuf,            NULL },
  { "Hunting Area",          "deer",           171,   "deer symbol",                       &wp_deer_pixbuf,            &wp_deer_large_pixbuf,            NULL },
  { "Shopping Center",       "shopping",     172,   "shopping cart symbol",              NULL,                &wp_shopping_large_pixbuf,            NULL },
  { "Lodging",               "lodging",        173,   "lodging symbol",                    NULL,                &wp_lodging_large_pixbuf,            NULL },
  { "Mine",                  "mine",           174,   "mine symbol",                       &wp_mine_pixbuf,            &wp_mine_large_pixbuf,            NULL },
  { "Trail Head",            "trail_head",     175,   "trail head symbol",                 NULL,                &wp_trail_head_large_pixbuf,            NULL },
  { "Truck Stop",            "truck_stop",     176,   "truck stop symbol",                 NULL,                &wp_truck_stop_large_pixbuf,            NULL },
  { "Exit",                  "user_exit",      177,   "user exit symbol",                  NULL,                       &wp_exit_large_pixbuf,            NULL },
  { "Flag",                  "flag",           178,   "flag symbol",                       &wp_flag_pixbuf,            NULL,            NULL },
  { "Circle with X",         "circle_x",       179,   "circle with x in the center",       NULL,                NULL,            NULL },
  { "Open 24 Hours",          "open_24hr",      180,   "open 24 hours symbol",              NULL,                NULL,            NULL },
  { "Fishing Hot Spot Facility",      "fhs_facility",   181,   "U Fishing Hot SpotsTM Facility",    NULL,                &wp_fhs_facility_large_pixbuf,  NULL },
  { "Bottom Conditions",      "bot_cond",       182,   "Bottom Conditions",                 NULL,                NULL,            NULL },
  { "Tide/Current PRediction Station", "tide_pred_stn",  183,   "Tide/Current Prediction Station",   NULL,                NULL,            NULL },
  { "Anchor Prohibited",     "anchor_prohib",  184,   "U anchor prohibited symbol",        NULL,                NULL,            NULL },
  { "Beacon",              "beacon",         185,   "U beacon symbol",                   NULL,                NULL,            NULL },
  { "Coast Guard",         "coast_guard",    186,   "U coast guard symbol",              NULL,                NULL,            NULL },
  { "Reef",                "reef",           187,   "U reef symbol",                     NULL,                NULL,            NULL },
  { "Weed Bed",             "weedbed",        188,   "U weedbed symbol",                  NULL,                NULL,            NULL },
  { "Dropoff",              "dropoff",        189,   "U dropoff symbol",                  NULL,                NULL,            NULL },
  { "Dock",                "dock",           190,   "U dock symbol",                     NULL,                NULL,            NULL },
  { "U Marina",              "marina",         191,   "U marina symbol",                   NULL,                NULL,            NULL },
  { "Bait and Tackle",     "bait_tackle",    192,   "U bait and tackle symbol",          NULL,                NULL,            NULL },
  { "Stump",               "stump",          193,   "U stump symbol",                    NULL,                NULL,            NULL },
  { "Ground Transportation", "grnd_trans",   229,   "ground transportation",                    NULL,                &wp_grnd_trans_large_pixbuf,        NULL },
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
  { "Interstate Highway",    "is_hwy",         8192,  "interstate hwy symbol",             NULL,                NULL,            NULL },   /* TODO: check symbol name */
  { "US hwy",            "us_hwy",         8193,  "us hwy symbol",                     NULL,                NULL,            NULL },
  { "State Hwy",         "st_hwy",         8194,  "state hwy symbol",                  NULL,                NULL,            NULL },
  { "Mile Marker",           "mi_mrkr",        8195,  "mile marker symbol",                NULL,                NULL,            NULL },
  { "TracBack Point",        "trcbck",         8196,  "TracBack (feet) symbol",            NULL,                NULL,            NULL },
  { "Golf Course",           "golf",           8197,  "golf symbol",                       &wp_golf_pixbuf,            &wp_golf_large_pixbuf,            NULL },
  { "City (Small)",          "sml_cty",        8198,  "small city symbol",                 &wp_sml_cty_pixbuf,         &wp_sml_cty_large_pixbuf,            NULL },
  { "City (Medium)",         "med_cty",        8199,  "medium city symbol",                &wp_med_cty_pixbuf,         &wp_med_cty_large_pixbuf,            NULL },
  { "City (Large)",          "lrg_cty",        8200,  "large city symbol",                 &wp_lrg_cty_pixbuf,         &wp_lrg_cty_large_pixbuf,            NULL },
  { "Intl freeway hwy",               "freeway",        8201,  "intl freeway hwy symbol",           NULL,                NULL,            NULL },
  { "Intl national hwy",      "ntl_hwy",        8202,  "intl national hwy symbol",          NULL,                NULL,            NULL },
  { "City (Capitol)",          "cap_cty",        8203,  "capitol city symbol (star)",        &wp_cap_cty_pixbuf,         NULL,            NULL },
  { "Amusement Park",        "amuse_pk",       8204,  "amusement park symbol",             NULL,                &wp_amuse_pk_large_pixbuf,            NULL },
  { "Bowling",                "bowling",        8205,  "bowling symbol",                    NULL,                &wp_bowling_large_pixbuf,            NULL },
  { "Car Rental",            "car_rental",     8206,  "car rental symbol",                 NULL,                &wp_car_rental_large_pixbuf,            NULL },
  { "Car Repair",            "car_repair",     8207,  "car repair symbol",                 NULL,                &wp_car_repair_large_pixbuf,            NULL },
  { "Fast Food",             "fastfood",       8208,  "fast food symbol",                  NULL,                &wp_fastfood_large_pixbuf,            NULL },
  { "Fitness Center",        "fitness",        8209,  "fitness symbol",                    NULL,                &wp_fitness_large_pixbuf,            NULL },
  { "Movie Theater",         "movie",          8210,  "movie symbol",                      NULL,                &wp_movie_large_pixbuf,            NULL },
  { "Museum",                "museum",         8211,  "museum symbol",                     NULL,                &wp_museum_large_pixbuf,            NULL },
  { "Pharmacy",              "pharmacy",       8212,  "pharmacy symbol",                   NULL,                &wp_pharmacy_large_pixbuf,            NULL },
  { "Pizza",                 "pizza",          8213,  "pizza symbol",                      NULL,                &wp_pizza_large_pixbuf,            NULL },
  { "Post Office",           "post_ofc",       8214,  "post office symbol",                NULL,                &wp_post_ofc_large_pixbuf,            NULL },
  { "RV Park",               "rv_park",        8215,  "RV park symbol",                    &wp_rv_park_pixbuf,  &wp_rv_park_large_pixbuf,            NULL },
  { "School",                "school",         8216,  "school symbol",                     &wp_school_pixbuf,   &wp_school_large_pixbuf,            NULL },
  { "Stadium",               "stadium",        8217,  "stadium symbol",                    NULL,                &wp_stadium_large_pixbuf,            NULL },
  { "Department Store",      "store",          8218,  "dept. store symbol",                NULL,                &wp_store_large_pixbuf,            NULL },
  { "Zoo",                   "zoo",            8219,  "zoo symbol",                        NULL,                &wp_zoo_large_pixbuf,            NULL },
  { "Convenience Store",     "conv_store",       8220,  "convenience store symbol",          NULL,                &wp_conv_store_large_pixbuf,        NULL },
  { "Live Theater",          "theater",          8221,  "live theater symbol",               NULL,                &wp_theater_large_pixbuf,            NULL },
  { "Ramp intersection",     "ramp_int",       8222,  "ramp intersection symbol",          NULL,                NULL,            NULL },
  { "Street Intersection",   "st_int",         8223,  "street intersection symbol",        NULL,                NULL,            NULL },
  { "Scales",                "weigh_station",     8226,  "inspection/weigh station symbol",   NULL,               &wp_weigh_station_large_pixbuf,       NULL },
  { "Toll Booth",            "toll_booth",     8227,  "toll booth symbol",                 NULL,                &wp_toll_booth_large_pixbuf,            NULL },
  { "Elevation point",       "elev_pt",        8228,  "elevation point symbol",            NULL,                NULL,            NULL },
  { "Exit without services", "ex_no_srvc",     8229,  "exit without services symbol",      NULL,                NULL,            NULL },
  { "Geographic place name, Man-made", "geo_place_mm",   8230,  "Geographic place name, man-made",   NULL,                NULL,            NULL },
  { "Geographic place name, water","geo_place_wtr",  8231,  "Geographic place name, water",      NULL,                NULL,            NULL },
  { "Geographic place name, Land", "geo_place_lnd",  8232,  "Geographic place name, land",       NULL,                NULL,            NULL },
  { "Bridge",                "bridge",         8233,  "bridge symbol",                     &wp_bridge_pixbuf,          &wp_bridge_large_pixbuf,            NULL },
  { "Building",              "building",       8234,  "building symbol",                   &wp_building_pixbuf,        &wp_building_large_pixbuf,        NULL },
  { "Cemetery",              "cemetery",       8235,  "cemetery symbol",                   &wp_cemetery_pixbuf,        &wp_cemetery_large_pixbuf,            NULL },
  { "Church",                "church",         8236,  "church symbol",                     &wp_church_pixbuf,          &wp_church_large_pixbuf,          NULL },
  { "Civil",                 "civil",          8237,  "civil location symbol",             NULL,                &wp_civil_large_pixbuf,            NULL },
  { "Crossing",              "crossing",       8238,  "crossing symbol",                   NULL,                &wp_crossing_large_pixbuf,            NULL },
  { "Ghost Town",            "hist_town",      8239,  "historical town symbol",            NULL,                NULL,            NULL },
  { "Levee",                 "levee",          8240,  "levee symbol",                      NULL,                NULL,            NULL },
  { "Military",              "military",       8241,  "military location symbol",          &wp_military_pixbuf,        NULL,            NULL },
  { "Oil Field",             "oil_field",      8242,  "oil field symbol",                  NULL,                &wp_oil_field_large_pixbuf,          NULL },
  { "Tunnel",                "tunnel",         8243,  "tunnel symbol",                     &wp_tunnel_pixbuf,          &wp_tunnel_large_pixbuf,          NULL },
  { "Beach",                 "beach",          8244,  "beach symbol",                      &wp_beach_pixbuf,           &wp_beach_large_pixbuf,           NULL },
  { "Forest",                "forest",         8245,  "forest symbol",                     &wp_forest_pixbuf,          &wp_forest_large_pixbuf,          NULL },
  { "Summit",                "summit",         8246,  "summit symbol",                     &wp_summit_pixbuf,          &wp_summit_large_pixbuf,          NULL },
  { "Large Ramp intersection", "lrg_ramp_int",   8247,  "large ramp intersection symbol",    NULL,                NULL,            NULL },
  { "Large exit without services", "lrg_ex_no_srvc", 8248,  "large exit without services smbl",  NULL,                NULL,            NULL },
  { "Police Station",        "police",          8249,  "police/official badge symbol",      NULL,                &wp_police_large_pixbuf,            NULL },
  { "Gambling/casino",                "cards",          8250,  "gambling/casino symbol",            NULL,                NULL,            NULL },
  { "Ski Resort",            "ski_resort",        8251,  "snow skiing symbol",                NULL,                &wp_ski_resort_large_pixbuf,          NULL },
  { "Ice Skating",           "ice_skating",       8252,  "ice skating symbol",                &wp_ice_skating_pixbuf,     &wp_ice_skating_large_pixbuf,  NULL },
  { "Wrecker",               "wrecker",        8253,  "tow truck (wrecker) symbol",        NULL,                &wp_wrecker_large_pixbuf,            NULL },
  { "Border Crossing (Port Of Entry)", "border",         8254,  "border crossing (port of entry)",   NULL,                NULL,            NULL },
  { "Geocache",              "geocache",       8255,  "geocache location",                 &wp_geocache_pixbuf,        &wp_geocache_large_pixbuf,        NULL },
  { "Geocache Found",        "geocache_fnd",   8256,  "found geocache",                    &wp_geocache_fnd_pixbuf,    &wp_geocache_fnd_large_pixbuf,    NULL },
  { "Contact, Smiley",       "cntct_smiley",   8257,  "Rino contact symbol, ""smiley""",   NULL,                NULL,            NULL },
  { "Contact, Ball Cap",     "cntct_ball_cap", 8258,  "Rino contact symbol, ""ball cap""", NULL,                NULL,            NULL },
  { "Contact, Big Ears",      "cntct_big_ears", 8259,  "Rino contact symbol, ""big ear""",  NULL,                NULL,            NULL },
  { "Contact, Spike",         "cntct_spike",    8260,  "Rino contact symbol, ""spike""",    NULL,                NULL,            NULL },
  { "Contact, Goatee",        "cntct_goatee",   8261,  "Rino contact symbol, ""goatee""",   NULL,                NULL,            NULL },
  { "Contact, Afro",          "cntct_afro",     8262,  "Rino contact symbol, ""afro""",     NULL,                NULL,            NULL },
  { "Contact, Dreadlocks",    "cntct_dreads",   8263,  "Rino contact symbol, ""dreads""",   NULL,                NULL,            NULL },
  { "Contact, Female1",       "cntct_female1",  8264,  "Rino contact symbol, ""female 1""", NULL,                NULL,            NULL },
  { "Contact, Female2",       "cntct_female2",  8265,  "Rino contact symbol, ""female 2""", NULL,                NULL,            NULL },
  { "Contact, Female3",       "cntct_female3",  8266,  "Rino contact symbol, ""female 3""", NULL,                NULL,            NULL },
  { "Contact, Ranger",        "cntct_ranger",   8267,  "Rino contact symbol, ""ranger""",   NULL,                NULL,            NULL },
  { "Contact, Kung-Fu",       "cntct_kung_fu",  8268,  "Rino contact symbol, ""kung fu""",  NULL,                NULL,            NULL },
  { "Contact, Sumo",          "cntct_sumo",     8269,  "Rino contact symbol, ""sumo""",     NULL,                NULL,            NULL },
  { "Contact, Pirate",        "cntct_pirate",   8270,  "Rino contact symbol, ""pirate""",   NULL,                NULL,            NULL },
  { "Contact, Biker",         "cntct_biker",    8271,  "Rino contact symbol, ""biker""",    NULL,                NULL,            NULL },
  { "Contact, Alien",         "cntct_alien",    8272,  "Rino contact symbol, ""alien""",    NULL,                NULL,            NULL },
  { "Contact, Bug",           "cntct_bug",      8273,  "Rino contact symbol, ""bug""",      NULL,                NULL,            NULL },
  { "Contact, Cat",           "cntct_cat",      8274,  "Rino contact symbol, ""cat""",      NULL,                NULL,            NULL },
  { "Contact, Dog",           "cntct_dog",      8275,  "Rino contact symbol, ""dog""",      NULL,                NULL,            NULL },
  { "Contact, Pig",           "cntct_pig",      8276,  "Rino contact symbol, ""pig""",      NULL,                NULL,            NULL },
  { "Water Hydrant",          "hydrant",        8282,  "water hydrant symbol",              NULL,                NULL,            NULL },
  { "Flag, Blue",             "flag_blue",      8284,  "blue flag symbol",                  NULL,                &wp_flag_blue_large_pixbuf,            NULL },
  { "Flag, Green",            "flag_green",     8285,  "green flag symbol",                 NULL,                &wp_flag_green_large_pixbuf,            NULL },
  { "Flag, Red",              "flag_red",       8286,  "red flag symbol",                   NULL,                &wp_flag_red_large_pixbuf,            NULL },
  { "Pin, Blue",              "pin_blue",       8287,  "blue pin symbol",                   NULL,                &wp_pin_blue_large_pixbuf,            NULL },
  { "Pin, Green",             "pin_green",      8288,  "green pin symbol",                  NULL,                &wp_pin_green_large_pixbuf,            NULL },
  { "Pin, Red",               "pin_red",        8289,  "red pin symbol",                    NULL,                &wp_pin_red_large_pixbuf,            NULL },
  { "Block, Blue",            "block_blue",     8290,  "blue block symbol",                 NULL,                &wp_block_blue_large_pixbuf,            NULL },
  { "Block, Green",           "block_green",    8291,  "green block symbol",                NULL,                &wp_block_green_large_pixbuf,           NULL },
  { "Block, Red",             "block_red",      8292,  "red block symbol",                  NULL,                &wp_block_red_large_pixbuf,            NULL },
  { "Bike Trail",             "bike_trail",     8293,  "bike trail symbol",                 NULL,                &wp_bike_trail_large_pixbuf,            NULL },
  { "Circle, Red",            "circle_red",     8294,  "red circle symbol",                 NULL,                NULL,            NULL },
  { "Circle, Green",          "circle_green",   8295,  "green circle symbol",               NULL,                NULL,            NULL },
  { "Circle, Blue",           "circle_blue",    8296,  "blue circle symbol",                NULL,                NULL,            NULL },
  { "Diamond, Blue",          "diamond_blue",   8299,  "blue diamond symbol",               NULL,                NULL,            NULL },
  { "Oval, Red",              "oval_red",       8300,  "red oval symbol",                   NULL,                NULL,            NULL },
  { "Oval, Green",            "oval_green",     8301,  "green oval symbol",                 NULL,                NULL,            NULL },
  { "Oval, Blue",             "oval_blue",      8302,  "blue oval symbol",                  NULL,                NULL,            NULL },
  { "Rectangle, Red",         "rect_red",       8303,  "red rectangle symbol",              NULL,                NULL,            NULL },
  { "Rectangle, Green",       "rect_green",     8304,  "green rectangle symbol",            NULL,                NULL,            NULL },
  { "Rectangle, Blue",        "rect_blue",      8305,  "blue rectangle symbol",             NULL,                NULL,            NULL },
  { "Square, Blue",           "square_blue",    8308,  "blue square symbol",                NULL,                NULL,            NULL },
  { "Letter A, Red",          "letter_a_red",   8309,  "red letter 'A' symbol",             NULL,                NULL,            NULL },
  { "Letter B, Red",          "letter_b_red",   8310,  "red letter 'B' symbol",             NULL,                NULL,            NULL },
  { "Letter C, Red",          "letter_c_red",   8311,  "red letter 'C' symbol",             NULL,                NULL,            NULL },
  { "Letter D, Red",          "letter_d_red",   8312,  "red letter 'D' symbol",             NULL,                NULL,            NULL },
  { "Letter A, Green",        "letter_a_green", 8313,  "green letter 'A' symbol",           NULL,                NULL,            NULL },
  { "Letter C, Green",        "letter_c_green", 8314,  "green letter 'C' symbol",           NULL,                NULL,            NULL },
  { "Letter B, Green",        "letter_b_green", 8315,  "green letter 'B' symbol",           NULL,                NULL,            NULL },
  { "Letter D, Green",        "letter_d_green", 8316,  "green letter 'D' symbol",           NULL,                NULL,            NULL },
  { "Letter A, Blue",         "letter_a_blue",  8317,  "blue letter 'A' symbol",            NULL,                NULL,            NULL },
  { "Letter B, Blue",         "letter_b_blue",  8318,  "blue letter 'B' symbol",            NULL,                NULL,            NULL },
  { "Letter C, Blue",         "letter_c_blue",  8319,  "blue letter 'C' symbol",            NULL,                NULL,            NULL },
  { "Letter D, Blue",         "letter_d_blue",  8320,  "blue letter 'D' symbol",            NULL,                NULL,            NULL },
  { "Number 0, Red",          "number_0_red",   8321,  "red number '0' symbol",             NULL,                NULL,            NULL },
  { "Number 1, Red",          "number_1_red",   8322,  "red number '1' symbol",             NULL,                NULL,            NULL },
  { "Number 2, Red",          "number_2_red",   8323,  "red number '2' symbol",             NULL,                NULL,            NULL },
  { "Number 3, Red",          "number_3_red",   8324,  "red number '3' symbol",             NULL,                NULL,            NULL },
  { "Number 4, Red",          "number_4_red",   8325,  "red number '4' symbol",             NULL,                NULL,            NULL },
  { "Number 5, Red",          "number_5_red",   8326,  "red number '5' symbol",             NULL,                NULL,            NULL },
  { "Number 6, Red",          "number_6_red",   8327,  "red number '6' symbol",             NULL,                NULL,            NULL },
  { "Number 7, Red",          "number_7_red",   8328,  "red number '7' symbol",             NULL,                NULL,            NULL },
  { "Number 8, Red",          "number_8_red",   8329,  "red number '8' symbol",             NULL,                NULL,            NULL },
  { "Number 9, Red",          "number_9_red",   8330,  "red number '9' symbol",             NULL,                NULL,            NULL },
  { "Number 0, Green",        "number_0_green", 8331,  "green number '0' symbol",           NULL,                NULL,            NULL },
  { "Number 1, Green",        "number_1_green", 8332,  "green number '1' symbol",           NULL,                NULL,            NULL },
  { "Number 2, Green",        "number_2_green", 8333,  "green number '2' symbol",           NULL,                NULL,            NULL },
  { "Number 3, Green",        "number_3_green", 8334,  "green number '3' symbol",           NULL,                NULL,            NULL },
  { "Number 4, Green",        "number_4_green", 8335,  "green number '4' symbol",           NULL,                NULL,            NULL },
  { "Number 5, Green",        "number_5_green", 8336,  "green number '5' symbol",           NULL,                NULL,            NULL },
  { "Number 6, Green",        "number_6_green", 8337,  "green number '6' symbol",           NULL,                NULL,            NULL },
  { "Number 7, Green",        "number_7_green", 8338,  "green number '7' symbol",           NULL,                NULL,            NULL },
  { "Number 8, Green",        "number_8_green", 8339,  "green number '8' symbol",           NULL,                NULL,            NULL },
  { "Number 9, Green",        "number_9_green", 8340,  "green number '9' symbol",           NULL,                NULL,            NULL },
  { "Number 0, Blue",         "number_0_blue",  8341,  "blue number '0' symbol",            NULL,                NULL,            NULL },
  { "Number 1, Blue",         "number_1_blue",  8342,  "blue number '1' symbol",            NULL,                NULL,            NULL },
  { "Number 2, Blue",         "number_2_blue",  8343,  "blue number '2' symbol",            NULL,                NULL,            NULL },
  { "Number 3, Blue",         "number_3_blue",  8344,  "blue number '3' symbol",            NULL,                NULL,            NULL },
  { "Number 4, Blue",         "number_4_blue",  8345,  "blue number '4' symbol",            NULL,                NULL,            NULL },
  { "Number 5, Blue",         "number_5_blue",  8346,  "blue number '5' symbol",            NULL,                NULL,            NULL },
  { "Number 6, Blue",         "number_6_blue",  8347,  "blue number '6' symbol",            NULL,                NULL,            NULL },
  { "Number 7, Blue",         "number_7_blue",  8348,  "blue number '7' symbol",            NULL,                NULL,            NULL },
  { "Number 8, Blue",         "number_8_blue",  8349,  "blue number '8' symbol",            NULL,                NULL,            NULL },
  { "Number 9, Blue",         "number_9_blue",  8350,  "blue number '9' symbol",            NULL,                NULL,            NULL },
  { "Triangle, Blue",         "triangle_blue",  8351,  "blue triangle symbol",              NULL,                NULL,            NULL },
  { "Triangle, Green",        "triangle_green", 8352,  "green triangle symbol",             NULL,                NULL,            NULL },
  { "Triangle, Red",          "triangle_red",   8353,  "red triangle symbol",               NULL,                NULL,            NULL },
  /*---------------------------------------------------------------
    Aviation symbols
    ---------------------------------------------------------------*/
  { "Airport",                "airport",        16384, "airport symbol",                    &wp_airplane_pixbuf,        &wp_airplane_large_pixbuf,        NULL },
  { "Intersection",           "int",            16385, "intersection symbol",               NULL,                NULL,            NULL },
  { "Non-directional beacon", "ndb",            16386, "non-directional beacon symbol",     NULL,                NULL,            NULL },
  { "VHF Omni-range",         "vor",            16387, "VHF omni-range symbol",             NULL,                NULL,            NULL },
  { "Heliport",               "heliport",       16388, "heliport symbol",                   NULL,                       &wp_helipad_large_pixbuf,         NULL },
  { "Private Field",          "private",        16389, "private field symbol",              NULL,                NULL,            NULL },
  { "Soft Field",             "soft_fld",       16390, "soft field symbol",                 NULL,                NULL,            NULL },
  { "Tall Tower",             "tall_tower",     16391, "tall tower symbol",                 NULL,                &wp_tall_tower_large_pixbuf,           NULL },
  { "Short Tower",             "short_tower",    16392, "short tower symbol",                NULL,                &wp_short_tower_large_pixbuf,          NULL },
  { "Glider Area",            "glider",         16393, "glider symbol",                     NULL,                &wp_glider_large_pixbuf,            NULL },
  { "Ultralight Area",        "ultralight",     16394, "ultralight symbol",                 NULL,                &wp_ultralight_large_pixbuf,            NULL },
  { "Parachute Area",         "parachute",      16395, "parachute symbol",                  NULL,                &wp_parachute_large_pixbuf,            NULL },
  { "VOR/TACAN",              "vortac",         16396, "VOR/TACAN symbol",                  NULL,                NULL,            NULL },
  { "VOR-DME",                "vordme",         16397, "VOR-DME symbol",                    NULL,                NULL,            NULL },
  { "First approach fix",     "faf",            16398, "first approach fix",                NULL,                NULL,            NULL },
  { "Localizer Outer Marker", "lom",            16399, "localizer outer marker",            NULL,                NULL,            NULL },
  { "Missed Approach Point",  "map",            16400, "missed approach point",             NULL,                NULL,            NULL },
  { "TACAN",                  "tacan",          16401, "TACAN symbol",                      NULL,                NULL,            NULL },
  { "Seaplane Base",          "seaplane",       16402, "Seaplane Base",                     NULL,                NULL,            NULL }
};

static GHashTable *icons = NULL;
static GHashTable *old_icons = NULL;

static gboolean str_equal_casefold ( gconstpointer v1, gconstpointer v2 ) {
  gboolean equal;
  gchar *v1_lower;
  gchar *v2_lower;

  v1_lower = g_utf8_casefold ( v1, -1 );
  if (!v1_lower)
    return FALSE;
  v2_lower = g_utf8_casefold ( v2, -1 );
  if (!v2_lower) {
    g_free ( v1_lower );
    return FALSE;
  }

  equal = g_str_equal( v1_lower, v2_lower );

  g_free ( v1_lower );
  g_free ( v2_lower );

  return equal;
}

static guint str_hash_casefold ( gconstpointer key ) {
  guint h;
  gchar *key_lower;

  key_lower = g_utf8_casefold ( key, -1 );
  if (!key_lower)
    return 0;

  h = g_str_hash ( key_lower );

  g_free ( key_lower );

  return h;
}

static void init_icons() {
  icons = g_hash_table_new_full ( str_hash_casefold, str_equal_casefold, NULL, NULL);
  old_icons = g_hash_table_new_full ( str_hash_casefold, str_equal_casefold, NULL, NULL);
  gint i;
  for (i=0; i<G_N_ELEMENTS(garmin_syms); i++) {
    g_hash_table_insert(icons, garmin_syms[i].sym, GINT_TO_POINTER (i));
    g_hash_table_insert(old_icons, garmin_syms[i].old_sym, GINT_TO_POINTER (i));
  }
}

static GdkPixbuf *get_wp_sym_from_index ( gint i ) {
  // Ensure data exists to either directly load icon or scale from the other set
  if ( !garmin_syms[i].icon && ( garmin_syms[i].data || garmin_syms[i].data_large) ) {
    if ( a_vik_get_use_large_waypoint_icons() ) {
      if ( garmin_syms[i].data_large )
	// Directly load icon
	garmin_syms[i].icon = gdk_pixbuf_from_pixdata ( garmin_syms[i].data_large, FALSE, NULL );
      else
	// Up sample from small image
	garmin_syms[i].icon = gdk_pixbuf_scale_simple ( gdk_pixbuf_from_pixdata ( garmin_syms[i].data, FALSE, NULL ), 30, 30, GDK_INTERP_BILINEAR );
    }
    else {
      if ( garmin_syms[i].data )
	// Directly use small symbol
	garmin_syms[i].icon = gdk_pixbuf_from_pixdata ( garmin_syms[i].data, FALSE, NULL );
      else
	// Down size large image
	garmin_syms[i].icon = gdk_pixbuf_scale_simple ( gdk_pixbuf_from_pixdata ( garmin_syms[i].data_large, FALSE, NULL ), 18, 18, GDK_INTERP_BILINEAR );
    }
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

const gchar *a_get_hashed_sym ( const gchar *sym ) {
  gpointer gp;
  gpointer x;

  if (!sym) {
    return NULL;
  }
  if (!icons) {
    init_icons();
  }
  if (g_hash_table_lookup_extended(icons, sym, &x, &gp))
    return garmin_syms[GPOINTER_TO_INT(gp)].sym;
  else if (g_hash_table_lookup_extended(old_icons, sym, &x, &gp))
    return garmin_syms[GPOINTER_TO_INT(gp)].sym;
  else
    return NULL;
}

void a_populate_sym_list ( GtkListStore *list ) {
  gint i;
  for (i=0; i<G_N_ELEMENTS(garmin_syms); i++) {
    // Ensure at least one symbol available - the other can be auto generated
    if ( garmin_syms[i].data || garmin_syms[i].data_large ) {
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
