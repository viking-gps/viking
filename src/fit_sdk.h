/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
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

// Flexible and Interoperable Data Transfer (FIT) Protocol
// https://developer.garmin.com/fit/protocol/

// defines/constants extracted from the FIT SDK
// Seems fairly redundant to 'rewrite' such API parts
//  so copy the values we use/need as is

// c.f. SDK fit.h

// Base types all converted to glib equivalents
#include <glib.h>

typedef guint8 FIT_ENUM;
#define FIT_ENUM_INVALID            ((FIT_ENUM)0xFF)
#define FIT_BASE_TYPE_ENUM          ((FIT_UINT8)0x00)

typedef gint8 FIT_SINT8;
#define FIT_SINT8_INVALID           ((FIT_SINT8)0x7F)
#define FIT_BASE_TYPE_SINT8         ((FIT_UINT8)0x01)

typedef guint8 FIT_UINT8;
#define FIT_UINT8_INVALID           ((FIT_UINT8)0xFF)
#define FIT_BASE_TYPE_UINT8         ((FIT_UINT8)0x02)

typedef gint16 FIT_SINT16;
#define FIT_SINT16_INVALID          ((FIT_SINT16)0x7FFF)
#define FIT_BASE_TYPE_SINT16        ((FIT_UINT8)0x83)

typedef guint16 FIT_UINT16;
#define FIT_UINT16_INVALID   ((FIT_UINT16)0xFFFF)
#define FIT_BASE_TYPE_UINT16 ((FIT_UINT8)0x84)

typedef gint32 FIT_SINT32;
#define FIT_SINT32_INVALID   ((FIT_SINT32)0x7FFFFFFF)
#define FIT_BASE_TYPE_SINT32 ((FIT_UINT8)0x85)

typedef guint32 FIT_UINT32;
#define FIT_UINT32_INVALID   ((FIT_UINT32)0xFFFFFFFF)
#define FIT_BASE_TYPE_UINT32 ((FIT_UINT8)0x86)

typedef gchar FIT_STRING; // UTF-8 null terminated string
#define FIT_STRING_INVALID   ((FIT_STRING)0x00)
#define FIT_BASE_TYPE_STRING ((FIT_UINT8)0x07)

typedef gfloat FIT_FLOAT32;
#define FIT_BASE_TYPE_FLOAT32 ((FIT_UINT8)0x88)

typedef gdouble FIT_FLOAT64;
#define FIT_BASE_TYPE_FLOAT64 ((FIT_UINT8)0x89)

typedef guint8 FIT_UINT8Z;
#define FIT_UINT8Z_INVALID   ((FIT_UINT8Z)0x00)
#define FIT_BASE_TYPE_UINT8Z ((FIT_UINT8)0x0A)

typedef guint16 FIT_UINT16Z;
#define FIT_UINT16Z_INVALID   ((FIT_UINT16Z)0x0000)
#define FIT_BASE_TYPE_UINT16Z ((FIT_UINT8)0x8B)

typedef guint32 FIT_UINT32Z;
#define FIT_UINT32Z_INVALID   ((FIT_UINT32Z)0x00000000)
#define FIT_BASE_TYPE_UINT32Z ((FIT_UINT8)0x8C)

typedef guint8 FIT_BYTE;
#define FIT_BYTE_INVALID   ((FIT_BYTE)0xFF) // Field is invalid if all bytes are invalid.
#define FIT_BASE_TYPE_BYTE ((FIT_UINT8)0x0D)

typedef gint64 FIT_SINT64;
#define FIT_SINT64_INVALID   ((FIT_SINT64)0x7FFFFFFFFFFFFFFFL)
#define FIT_BASE_TYPE_SINT64 ((FIT_UINT8)0x8E)

typedef guint64 FIT_UINT64;
#define FIT_UINT64_INVALID   ((FIT_UINT64)0xFFFFFFFFFFFFFFFFL)
#define FIT_BASE_TYPE_UINT64 ((FIT_UINT8)0x8F)

typedef guint64 FIT_UINT64Z;
#define FIT_UINT64Z_INVALID   ((FIT_UINT64Z)0x0000000000000000L)
#define FIT_BASE_TYPE_UINT64Z ((FIT_UINT8)0x90)

#define FIT_HDR_TIME_REC_BIT      ((FIT_UINT8) 0x80)
#define FIT_HDR_TIME_TYPE_MASK    ((FIT_UINT8) 0x60)
#define FIT_HDR_TIME_TYPE_SHIFT   5
#define FIT_HDR_TIME_OFFSET_MASK  ((FIT_UINT8) 0x1F)
#define FIT_HDR_TYPE_DEF_BIT      ((FIT_UINT8) 0x40)
#define FIT_HDR_DEV_DATA_BIT      ((FIT_UINT8) 0x20)
#define FIT_HDR_TYPE_MASK         ((FIT_UINT8) 0x0F)
#define FIT_MAX_LOCAL_MESGS       (FIT_HDR_TYPE_MASK + 1)

#define FIT_ARCH_ENDIAN_LITTLE    0
#define FIT_ARCH_ENDIAN_BIG       1

// c.f. SDK fit_example.h

typedef FIT_UINT16 FIT_MESG_NUM;
#define FIT_MESG_NUM_INVALID                                                     FIT_UINT16_INVALID
#define FIT_MESG_NUM_FILE_ID                                                     ((FIT_MESG_NUM)0)
#define FIT_MESG_NUM_CAPABILITIES                                                ((FIT_MESG_NUM)1)
#define FIT_MESG_NUM_DEVICE_SETTINGS                                             ((FIT_MESG_NUM)2)
#define FIT_MESG_NUM_USER_PROFILE                                                ((FIT_MESG_NUM)3)
#define FIT_MESG_NUM_HRM_PROFILE                                                 ((FIT_MESG_NUM)4)
#define FIT_MESG_NUM_SDM_PROFILE                                                 ((FIT_MESG_NUM)5)
#define FIT_MESG_NUM_BIKE_PROFILE                                                ((FIT_MESG_NUM)6)
#define FIT_MESG_NUM_ZONES_TARGET                                                ((FIT_MESG_NUM)7)
#define FIT_MESG_NUM_HR_ZONE                                                     ((FIT_MESG_NUM)8)
#define FIT_MESG_NUM_POWER_ZONE                                                  ((FIT_MESG_NUM)9)
#define FIT_MESG_NUM_MET_ZONE                                                    ((FIT_MESG_NUM)10)
#define FIT_MESG_NUM_SPORT                                                       ((FIT_MESG_NUM)12)
#define FIT_MESG_NUM_GOAL                                                        ((FIT_MESG_NUM)15)
#define FIT_MESG_NUM_SESSION                                                     ((FIT_MESG_NUM)18)
#define FIT_MESG_NUM_LAP                                                         ((FIT_MESG_NUM)19)
#define FIT_MESG_NUM_RECORD                                                      ((FIT_MESG_NUM)20)
#define FIT_MESG_NUM_EVENT                                                       ((FIT_MESG_NUM)21)
#define FIT_MESG_NUM_DEVICE_INFO                                                 ((FIT_MESG_NUM)23)
#define FIT_MESG_NUM_WORKOUT                                                     ((FIT_MESG_NUM)26)
#define FIT_MESG_NUM_WORKOUT_STEP                                                ((FIT_MESG_NUM)27)
#define FIT_MESG_NUM_SCHEDULE                                                    ((FIT_MESG_NUM)28)
#define FIT_MESG_NUM_WEIGHT_SCALE                                                ((FIT_MESG_NUM)30)
#define FIT_MESG_NUM_COURSE                                                      ((FIT_MESG_NUM)31)
#define FIT_MESG_NUM_COURSE_POINT                                                ((FIT_MESG_NUM)32)
#define FIT_MESG_NUM_TOTALS                                                      ((FIT_MESG_NUM)33)
#define FIT_MESG_NUM_ACTIVITY                                                    ((FIT_MESG_NUM)34)
#define FIT_MESG_NUM_SOFTWARE                                                    ((FIT_MESG_NUM)35)
#define FIT_MESG_NUM_FILE_CAPABILITIES                                           ((FIT_MESG_NUM)37)
#define FIT_MESG_NUM_MESG_CAPABILITIES                                           ((FIT_MESG_NUM)38)
#define FIT_MESG_NUM_FIELD_CAPABILITIES                                          ((FIT_MESG_NUM)39)
#define FIT_MESG_NUM_FILE_CREATOR                                                ((FIT_MESG_NUM)49)
#define FIT_MESG_NUM_BLOOD_PRESSURE                                              ((FIT_MESG_NUM)51)
#define FIT_MESG_NUM_SPEED_ZONE                                                  ((FIT_MESG_NUM)53)
#define FIT_MESG_NUM_MONITORING                                                  ((FIT_MESG_NUM)55)
#define FIT_MESG_NUM_TRAINING_FILE                                               ((FIT_MESG_NUM)72)
#define FIT_MESG_NUM_HRV                                                         ((FIT_MESG_NUM)78)
#define FIT_MESG_NUM_ANT_RX                                                      ((FIT_MESG_NUM)80)
#define FIT_MESG_NUM_ANT_TX                                                      ((FIT_MESG_NUM)81)
#define FIT_MESG_NUM_ANT_CHANNEL_ID                                              ((FIT_MESG_NUM)82)
#define FIT_MESG_NUM_LENGTH                                                      ((FIT_MESG_NUM)101)
#define FIT_MESG_NUM_MONITORING_INFO                                             ((FIT_MESG_NUM)103)
#define FIT_MESG_NUM_PAD                                                         ((FIT_MESG_NUM)105)
#define FIT_MESG_NUM_SLAVE_DEVICE                                                ((FIT_MESG_NUM)106)
#define FIT_MESG_NUM_CONNECTIVITY                                                ((FIT_MESG_NUM)127)
#define FIT_MESG_NUM_WEATHER_CONDITIONS                                          ((FIT_MESG_NUM)128)
#define FIT_MESG_NUM_WEATHER_ALERT                                               ((FIT_MESG_NUM)129)
#define FIT_MESG_NUM_CADENCE_ZONE                                                ((FIT_MESG_NUM)131)
#define FIT_MESG_NUM_HR                                                          ((FIT_MESG_NUM)132)
#define FIT_MESG_NUM_SEGMENT_LAP                                                 ((FIT_MESG_NUM)142)
#define FIT_MESG_NUM_MEMO_GLOB                                                   ((FIT_MESG_NUM)145)
#define FIT_MESG_NUM_SEGMENT_ID                                                  ((FIT_MESG_NUM)148)
#define FIT_MESG_NUM_SEGMENT_LEADERBOARD_ENTRY                                   ((FIT_MESG_NUM)149)
#define FIT_MESG_NUM_SEGMENT_POINT                                               ((FIT_MESG_NUM)150)
#define FIT_MESG_NUM_SEGMENT_FILE                                                ((FIT_MESG_NUM)151)
#define FIT_MESG_NUM_WORKOUT_SESSION                                             ((FIT_MESG_NUM)158)
#define FIT_MESG_NUM_WATCHFACE_SETTINGS                                          ((FIT_MESG_NUM)159)
#define FIT_MESG_NUM_GPS_METADATA                                                ((FIT_MESG_NUM)160)
#define FIT_MESG_NUM_CAMERA_EVENT                                                ((FIT_MESG_NUM)161)
#define FIT_MESG_NUM_TIMESTAMP_CORRELATION                                       ((FIT_MESG_NUM)162)
#define FIT_MESG_NUM_GYROSCOPE_DATA                                              ((FIT_MESG_NUM)164)
#define FIT_MESG_NUM_ACCELEROMETER_DATA                                          ((FIT_MESG_NUM)165)
#define FIT_MESG_NUM_THREE_D_SENSOR_CALIBRATION                                  ((FIT_MESG_NUM)167)
#define FIT_MESG_NUM_VIDEO_FRAME                                                 ((FIT_MESG_NUM)169)
#define FIT_MESG_NUM_OBDII_DATA                                                  ((FIT_MESG_NUM)174)
#define FIT_MESG_NUM_NMEA_SENTENCE                                               ((FIT_MESG_NUM)177)
#define FIT_MESG_NUM_AVIATION_ATTITUDE                                           ((FIT_MESG_NUM)178)
#define FIT_MESG_NUM_VIDEO                                                       ((FIT_MESG_NUM)184)
#define FIT_MESG_NUM_VIDEO_TITLE                                                 ((FIT_MESG_NUM)185)
#define FIT_MESG_NUM_VIDEO_DESCRIPTION                                           ((FIT_MESG_NUM)186)
#define FIT_MESG_NUM_VIDEO_CLIP                                                  ((FIT_MESG_NUM)187)
#define FIT_MESG_NUM_OHR_SETTINGS                                                ((FIT_MESG_NUM)188)
#define FIT_MESG_NUM_EXD_SCREEN_CONFIGURATION                                    ((FIT_MESG_NUM)200)
#define FIT_MESG_NUM_EXD_DATA_FIELD_CONFIGURATION                                ((FIT_MESG_NUM)201)
#define FIT_MESG_NUM_EXD_DATA_CONCEPT_CONFIGURATION                              ((FIT_MESG_NUM)202)
#define FIT_MESG_NUM_FIELD_DESCRIPTION                                           ((FIT_MESG_NUM)206)
#define FIT_MESG_NUM_DEVELOPER_DATA_ID                                           ((FIT_MESG_NUM)207)
#define FIT_MESG_NUM_MAGNETOMETER_DATA                                           ((FIT_MESG_NUM)208)
#define FIT_MESG_NUM_BAROMETER_DATA                                              ((FIT_MESG_NUM)209)
#define FIT_MESG_NUM_ONE_D_SENSOR_CALIBRATION                                    ((FIT_MESG_NUM)210)
#define FIT_MESG_NUM_TIME_IN_ZONE                                                ((FIT_MESG_NUM)216)
#define FIT_MESG_NUM_SET                                                         ((FIT_MESG_NUM)225)
#define FIT_MESG_NUM_STRESS_LEVEL                                                ((FIT_MESG_NUM)227)
#define FIT_MESG_NUM_DIVE_SETTINGS                                               ((FIT_MESG_NUM)258)
#define FIT_MESG_NUM_DIVE_GAS                                                    ((FIT_MESG_NUM)259)
#define FIT_MESG_NUM_DIVE_ALARM                                                  ((FIT_MESG_NUM)262)
#define FIT_MESG_NUM_EXERCISE_TITLE                                              ((FIT_MESG_NUM)264)
#define FIT_MESG_NUM_DIVE_SUMMARY                                                ((FIT_MESG_NUM)268)
#define FIT_MESG_NUM_JUMP                                                        ((FIT_MESG_NUM)285)
#define FIT_MESG_NUM_SPLIT                                                       ((FIT_MESG_NUM)312)
#define FIT_MESG_NUM_CLIMB_PRO                                                   ((FIT_MESG_NUM)317)
#define FIT_MESG_NUM_TANK_UPDATE                                                 ((FIT_MESG_NUM)319)
#define FIT_MESG_NUM_TANK_SUMMARY                                                ((FIT_MESG_NUM)323)
#define FIT_MESG_NUM_DEVICE_AUX_BATTERY_INFO                                     ((FIT_MESG_NUM)375)
#define FIT_MESG_NUM_DIVE_APNEA_ALARM                                            ((FIT_MESG_NUM)393)

typedef FIT_UINT8 FIT_FILE_ID_FIELD_NUM;
#define FIT_FILE_ID_FIELD_NUM_SERIAL_NUMBER ((FIT_FILE_ID_FIELD_NUM)3)
#define FIT_FILE_ID_FIELD_NUM_TIME_CREATED ((FIT_FILE_ID_FIELD_NUM)4)
#define FIT_FILE_ID_FIELD_NUM_PRODUCT_NAME ((FIT_FILE_ID_FIELD_NUM)8)
#define FIT_FILE_ID_FIELD_NUM_MANUFACTURER ((FIT_FILE_ID_FIELD_NUM)1)
#define FIT_FILE_ID_FIELD_NUM_PRODUCT ((FIT_FILE_ID_FIELD_NUM)2)
#define FIT_FILE_ID_FIELD_NUM_NUMBER ((FIT_FILE_ID_FIELD_NUM)5)
#define FIT_FILE_ID_FIELD_NUM_TYPE ((FIT_FILE_ID_FIELD_NUM)0)

typedef FIT_ENUM FIT_FILE;
#define FIT_FILE_INVALID                                                         FIT_ENUM_INVALID
#define FIT_FILE_DEVICE                                                          ((FIT_FILE)1) // Read only, single file. Must be in root directory.
#define FIT_FILE_SETTINGS                                                        ((FIT_FILE)2) // Read/write, single file. Directory=Settings
#define FIT_FILE_SPORT                                                           ((FIT_FILE)3) // Read/write, multiple files, file number = sport type. Directory=Sports
#define FIT_FILE_ACTIVITY                                                        ((FIT_FILE)4) // Read/erase, multiple files. Directory=Activities
#define FIT_FILE_WORKOUT                                                         ((FIT_FILE)5) // Read/write/erase, multiple files. Directory=Workouts
#define FIT_FILE_COURSE                                                          ((FIT_FILE)6) // Read/write/erase, multiple files. Directory=Courses
#define FIT_FILE_SCHEDULES                                                       ((FIT_FILE)7) // Read/write, single file. Directory=Schedules
#define FIT_FILE_WEIGHT                                                          ((FIT_FILE)9) // Read only, single file. Circular buffer. All message definitions at start of file. Directory=Weight
#define FIT_FILE_TOTALS                                                          ((FIT_FILE)10) // Read only, single file. Directory=Totals
#define FIT_FILE_GOALS                                                           ((FIT_FILE)11) // Read/write, single file. Directory=Goals
#define FIT_FILE_BLOOD_PRESSURE                                                  ((FIT_FILE)14) // Read only. Directory=Blood Pressure
#define FIT_FILE_MONITORING_A                                                    ((FIT_FILE)15) // Read only. Directory=Monitoring. File number=sub type.
#define FIT_FILE_ACTIVITY_SUMMARY                                                ((FIT_FILE)20) // Read/erase, multiple files. Directory=Activities
#define FIT_FILE_MONITORING_DAILY                                                ((FIT_FILE)28)
#define FIT_FILE_MONITORING_B                                                    ((FIT_FILE)32) // Read only. Directory=Monitoring. File number=identifier
#define FIT_FILE_SEGMENT                                                         ((FIT_FILE)34) // Read/write/erase. Multiple Files. Directory=Segments
#define FIT_FILE_SEGMENT_LIST                                                    ((FIT_FILE)35) // Read/write/erase. Single File. Directory=Segments
#define FIT_FILE_EXD_CONFIGURATION                                               ((FIT_FILE)40) // Read/write/erase. Single File. Directory=Settings
#define FIT_FILE_MFG_RANGE_MIN                                                   ((FIT_FILE)0xF7) // 0xF7 - 0xFE reserved for manufacturer specific file types
#define FIT_FILE_MFG_RANGE_MAX                                                   ((FIT_FILE)0xFE) // 0xF7 - 0xFE reserved for manufacturer specific file types

typedef FIT_UINT8 FIT_RECORD_FIELD_NUM;
#define FIT_RECORD_FIELD_NUM_TIMESTAMP ((FIT_RECORD_FIELD_NUM)253)
#define FIT_RECORD_FIELD_NUM_POSITION_LAT ((FIT_RECORD_FIELD_NUM)0)
#define FIT_RECORD_FIELD_NUM_POSITION_LONG ((FIT_RECORD_FIELD_NUM)1)
#define FIT_RECORD_FIELD_NUM_DISTANCE ((FIT_RECORD_FIELD_NUM)5)
#define FIT_RECORD_FIELD_NUM_TIME_FROM_COURSE ((FIT_RECORD_FIELD_NUM)11)
#define FIT_RECORD_FIELD_NUM_TOTAL_CYCLES ((FIT_RECORD_FIELD_NUM)19)
#define FIT_RECORD_FIELD_NUM_ACCUMULATED_POWER ((FIT_RECORD_FIELD_NUM)29)
#define FIT_RECORD_FIELD_NUM_ENHANCED_SPEED ((FIT_RECORD_FIELD_NUM)73)
#define FIT_RECORD_FIELD_NUM_ENHANCED_ALTITUDE ((FIT_RECORD_FIELD_NUM)78)
#define FIT_RECORD_FIELD_NUM_ALTITUDE ((FIT_RECORD_FIELD_NUM)2)
#define FIT_RECORD_FIELD_NUM_SPEED ((FIT_RECORD_FIELD_NUM)6)
#define FIT_RECORD_FIELD_NUM_POWER ((FIT_RECORD_FIELD_NUM)7)
#define FIT_RECORD_FIELD_NUM_GRADE ((FIT_RECORD_FIELD_NUM)9)
#define FIT_RECORD_FIELD_NUM_COMPRESSED_ACCUMULATED_POWER ((FIT_RECORD_FIELD_NUM)28)
#define FIT_RECORD_FIELD_NUM_VERTICAL_SPEED ((FIT_RECORD_FIELD_NUM)32)
#define FIT_RECORD_FIELD_NUM_CALORIES ((FIT_RECORD_FIELD_NUM)33)
#define FIT_RECORD_FIELD_NUM_VERTICAL_OSCILLATION ((FIT_RECORD_FIELD_NUM)39)
#define FIT_RECORD_FIELD_NUM_STANCE_TIME_PERCENT ((FIT_RECORD_FIELD_NUM)40)
#define FIT_RECORD_FIELD_NUM_STANCE_TIME ((FIT_RECORD_FIELD_NUM)41)
#define FIT_RECORD_FIELD_NUM_BALL_SPEED ((FIT_RECORD_FIELD_NUM)51)
#define FIT_RECORD_FIELD_NUM_CADENCE256 ((FIT_RECORD_FIELD_NUM)52)
#define FIT_RECORD_FIELD_NUM_TOTAL_HEMOGLOBIN_CONC ((FIT_RECORD_FIELD_NUM)54)
#define FIT_RECORD_FIELD_NUM_TOTAL_HEMOGLOBIN_CONC_MIN ((FIT_RECORD_FIELD_NUM)55)
#define FIT_RECORD_FIELD_NUM_TOTAL_HEMOGLOBIN_CONC_MAX ((FIT_RECORD_FIELD_NUM)56)
#define FIT_RECORD_FIELD_NUM_SATURATED_HEMOGLOBIN_PERCENT ((FIT_RECORD_FIELD_NUM)57)
#define FIT_RECORD_FIELD_NUM_SATURATED_HEMOGLOBIN_PERCENT_MIN ((FIT_RECORD_FIELD_NUM)58)
#define FIT_RECORD_FIELD_NUM_SATURATED_HEMOGLOBIN_PERCENT_MAX ((FIT_RECORD_FIELD_NUM)59)
#define FIT_RECORD_FIELD_NUM_HEART_RATE ((FIT_RECORD_FIELD_NUM)3)
#define FIT_RECORD_FIELD_NUM_CADENCE ((FIT_RECORD_FIELD_NUM)4)
#define FIT_RECORD_FIELD_NUM_COMPRESSED_SPEED_DISTANCE ((FIT_RECORD_FIELD_NUM)8)
#define FIT_RECORD_FIELD_NUM_RESISTANCE ((FIT_RECORD_FIELD_NUM)10)
#define FIT_RECORD_FIELD_NUM_CYCLE_LENGTH ((FIT_RECORD_FIELD_NUM)12)
#define FIT_RECORD_FIELD_NUM_TEMPERATURE ((FIT_RECORD_FIELD_NUM)13)
#define FIT_RECORD_FIELD_NUM_SPEED_1S ((FIT_RECORD_FIELD_NUM)17)
#define FIT_RECORD_FIELD_NUM_CYCLES ((FIT_RECORD_FIELD_NUM)18)
#define FIT_RECORD_FIELD_NUM_LEFT_RIGHT_BALANCE ((FIT_RECORD_FIELD_NUM)30)
#define FIT_RECORD_FIELD_NUM_GPS_ACCURACY ((FIT_RECORD_FIELD_NUM)31)
#define FIT_RECORD_FIELD_NUM_ACTIVITY_TYPE ((FIT_RECORD_FIELD_NUM)42)
#define FIT_RECORD_FIELD_NUM_LEFT_TORQUE_EFFECTIVENESS ((FIT_RECORD_FIELD_NUM)43)
#define FIT_RECORD_FIELD_NUM_RIGHT_TORQUE_EFFECTIVENESS ((FIT_RECORD_FIELD_NUM)44)
#define FIT_RECORD_FIELD_NUM_LEFT_PEDAL_SMOOTHNESS ((FIT_RECORD_FIELD_NUM)45)
#define FIT_RECORD_FIELD_NUM_RIGHT_PEDAL_SMOOTHNESS ((FIT_RECORD_FIELD_NUM)46)
#define FIT_RECORD_FIELD_NUM_COMBINED_PEDAL_SMOOTHNESS ((FIT_RECORD_FIELD_NUM)47)
#define FIT_RECORD_FIELD_NUM_TIME128 ((FIT_RECORD_FIELD_NUM)48)
#define FIT_RECORD_FIELD_NUM_STROKE_TYPE ((FIT_RECORD_FIELD_NUM)49)
#define FIT_RECORD_FIELD_NUM_ZONE ((FIT_RECORD_FIELD_NUM)50)
#define FIT_RECORD_FIELD_NUM_FRACTIONAL_CADENCE ((FIT_RECORD_FIELD_NUM)53)
#define FIT_RECORD_FIELD_NUM_DEVICE_INDEX ((FIT_RECORD_FIELD_NUM)62)

typedef FIT_UINT8 FIT_COURSE_POINT_FIELD_NUM;
#define FIT_COURSE_POINT_FIELD_NUM_TIMESTAMP ((FIT_COURSE_POINT_FIELD_NUM)1)
#define FIT_COURSE_POINT_FIELD_NUM_POSITION_LAT ((FIT_COURSE_POINT_FIELD_NUM)2)
#define FIT_COURSE_POINT_FIELD_NUM_POSITION_LONG ((FIT_COURSE_POINT_FIELD_NUM)3)
#define FIT_COURSE_POINT_FIELD_NUM_DISTANCE ((FIT_COURSE_POINT_FIELD_NUM)4)
#define FIT_COURSE_POINT_FIELD_NUM_NAME ((FIT_COURSE_POINT_FIELD_NUM)6)
#define FIT_COURSE_POINT_FIELD_NUM_MESSAGE_INDEX ((FIT_COURSE_POINT_FIELD_NUM)254)
#define FIT_COURSE_POINT_FIELD_NUM_TYPE ((FIT_COURSE_POINT_FIELD_NUM)5)
#define FIT_COURSE_POINT_FIELD_NUM_FAVORITE ((FIT_COURSE_POINT_FIELD_NUM)8)

typedef FIT_ENUM FIT_COURSE_POINT;
#define FIT_COURSE_POINT_INVALID                                                 FIT_ENUM_INVALID
#define FIT_COURSE_POINT_GENERIC                                                 ((FIT_COURSE_POINT)0)
#define FIT_COURSE_POINT_SUMMIT                                                  ((FIT_COURSE_POINT)1)
#define FIT_COURSE_POINT_VALLEY                                                  ((FIT_COURSE_POINT)2)
#define FIT_COURSE_POINT_WATER                                                   ((FIT_COURSE_POINT)3)
#define FIT_COURSE_POINT_FOOD                                                    ((FIT_COURSE_POINT)4)
#define FIT_COURSE_POINT_DANGER                                                  ((FIT_COURSE_POINT)5)
#define FIT_COURSE_POINT_LEFT                                                    ((FIT_COURSE_POINT)6)
#define FIT_COURSE_POINT_RIGHT                                                   ((FIT_COURSE_POINT)7)
#define FIT_COURSE_POINT_STRAIGHT                                                ((FIT_COURSE_POINT)8)
#define FIT_COURSE_POINT_FIRST_AID                                               ((FIT_COURSE_POINT)9)
#define FIT_COURSE_POINT_FOURTH_CATEGORY                                         ((FIT_COURSE_POINT)10)
#define FIT_COURSE_POINT_THIRD_CATEGORY                                          ((FIT_COURSE_POINT)11)
#define FIT_COURSE_POINT_SECOND_CATEGORY                                         ((FIT_COURSE_POINT)12)
#define FIT_COURSE_POINT_FIRST_CATEGORY                                          ((FIT_COURSE_POINT)13)
#define FIT_COURSE_POINT_HORS_CATEGORY                                           ((FIT_COURSE_POINT)14)
#define FIT_COURSE_POINT_SPRINT                                                  ((FIT_COURSE_POINT)15)
#define FIT_COURSE_POINT_LEFT_FORK                                               ((FIT_COURSE_POINT)16)
#define FIT_COURSE_POINT_RIGHT_FORK                                              ((FIT_COURSE_POINT)17)
#define FIT_COURSE_POINT_MIDDLE_FORK                                             ((FIT_COURSE_POINT)18)
#define FIT_COURSE_POINT_SLIGHT_LEFT                                             ((FIT_COURSE_POINT)19)
#define FIT_COURSE_POINT_SHARP_LEFT                                              ((FIT_COURSE_POINT)20)
#define FIT_COURSE_POINT_SLIGHT_RIGHT                                            ((FIT_COURSE_POINT)21)
#define FIT_COURSE_POINT_SHARP_RIGHT                                             ((FIT_COURSE_POINT)22)
#define FIT_COURSE_POINT_U_TURN                                                  ((FIT_COURSE_POINT)23)
#define FIT_COURSE_POINT_SEGMENT_START                                           ((FIT_COURSE_POINT)24)
#define FIT_COURSE_POINT_SEGMENT_END                                             ((FIT_COURSE_POINT)25)
#define FIT_COURSE_POINT_CAMPSITE                                                ((FIT_COURSE_POINT)27)
#define FIT_COURSE_POINT_AID_STATION                                             ((FIT_COURSE_POINT)28)
#define FIT_COURSE_POINT_REST_AREA                                               ((FIT_COURSE_POINT)29)
#define FIT_COURSE_POINT_GENERAL_DISTANCE                                        ((FIT_COURSE_POINT)30) // Used with UpAhead
#define FIT_COURSE_POINT_SERVICE                                                 ((FIT_COURSE_POINT)31)
#define FIT_COURSE_POINT_ENERGY_GEL                                              ((FIT_COURSE_POINT)32)
#define FIT_COURSE_POINT_SPORTS_DRINK                                            ((FIT_COURSE_POINT)33)
#define FIT_COURSE_POINT_MILE_MARKER                                             ((FIT_COURSE_POINT)34)
#define FIT_COURSE_POINT_CHECKPOINT                                              ((FIT_COURSE_POINT)35)
#define FIT_COURSE_POINT_SHELTER                                                 ((FIT_COURSE_POINT)36)
#define FIT_COURSE_POINT_MEETING_SPOT                                            ((FIT_COURSE_POINT)37)
#define FIT_COURSE_POINT_OVERLOOK                                                ((FIT_COURSE_POINT)38)
#define FIT_COURSE_POINT_TOILET                                                  ((FIT_COURSE_POINT)39)
#define FIT_COURSE_POINT_SHOWER                                                  ((FIT_COURSE_POINT)40)
#define FIT_COURSE_POINT_GEAR                                                    ((FIT_COURSE_POINT)41)
#define FIT_COURSE_POINT_SHARP_CURVE                                             ((FIT_COURSE_POINT)42)
#define FIT_COURSE_POINT_STEEP_INCLINE                                           ((FIT_COURSE_POINT)43)
#define FIT_COURSE_POINT_TUNNEL                                                  ((FIT_COURSE_POINT)44)
#define FIT_COURSE_POINT_BRIDGE                                                  ((FIT_COURSE_POINT)45)
#define FIT_COURSE_POINT_OBSTACLE                                                ((FIT_COURSE_POINT)46)
#define FIT_COURSE_POINT_CROSSING                                                ((FIT_COURSE_POINT)47)
#define FIT_COURSE_POINT_STORE                                                   ((FIT_COURSE_POINT)48)
#define FIT_COURSE_POINT_TRANSITION                                              ((FIT_COURSE_POINT)49)
#define FIT_COURSE_POINT_NAVAID                                                  ((FIT_COURSE_POINT)50)
#define FIT_COURSE_POINT_TRANSPORT                                               ((FIT_COURSE_POINT)51)
#define FIT_COURSE_POINT_ALERT                                                   ((FIT_COURSE_POINT)52)
#define FIT_COURSE_POINT_INFO                                                    ((FIT_COURSE_POINT)53)

typedef FIT_ENUM FIT_EVENT;
#define FIT_EVENT_INVALID                                                        FIT_ENUM_INVALID
#define FIT_EVENT_TIMER                                                          ((FIT_EVENT)0) // Group 0. Start / stop_all
#define FIT_EVENT_WORKOUT                                                        ((FIT_EVENT)3) // start / stop
#define FIT_EVENT_WORKOUT_STEP                                                   ((FIT_EVENT)4) // Start at beginning of workout. Stop at end of each step.
#define FIT_EVENT_POWER_DOWN                                                     ((FIT_EVENT)5) // stop_all group 0
#define FIT_EVENT_POWER_UP                                                       ((FIT_EVENT)6) // stop_all group 0
#define FIT_EVENT_OFF_COURSE                                                     ((FIT_EVENT)7) // start / stop group 0
#define FIT_EVENT_SESSION                                                        ((FIT_EVENT)8) // Stop at end of each session.
#define FIT_EVENT_LAP                                                            ((FIT_EVENT)9) // Stop at end of each lap.
#define FIT_EVENT_COURSE_POINT                                                   ((FIT_EVENT)10) // marker
#define FIT_EVENT_BATTERY                                                        ((FIT_EVENT)11) // marker
#define FIT_EVENT_VIRTUAL_PARTNER_PACE                                           ((FIT_EVENT)12) // Group 1. Start at beginning of activity if VP enabled, when VP pace is changed during activity or VP enabled mid activity. stop_disable when VP disabled.
#define FIT_EVENT_HR_HIGH_ALERT                                                  ((FIT_EVENT)13) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_HR_LOW_ALERT                                                   ((FIT_EVENT)14) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_SPEED_HIGH_ALERT                                               ((FIT_EVENT)15) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_SPEED_LOW_ALERT                                                ((FIT_EVENT)16) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_CAD_HIGH_ALERT                                                 ((FIT_EVENT)17) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_CAD_LOW_ALERT                                                  ((FIT_EVENT)18) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_POWER_HIGH_ALERT                                               ((FIT_EVENT)19) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_POWER_LOW_ALERT                                                ((FIT_EVENT)20) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_RECOVERY_HR                                                    ((FIT_EVENT)21) // marker
#define FIT_EVENT_BATTERY_LOW                                                    ((FIT_EVENT)22) // marker
#define FIT_EVENT_TIME_DURATION_ALERT                                            ((FIT_EVENT)23) // Group 1. Start if enabled mid activity (not required at start of activity). Stop when duration is reached. stop_disable if disabled.
#define FIT_EVENT_DISTANCE_DURATION_ALERT                                        ((FIT_EVENT)24) // Group 1. Start if enabled mid activity (not required at start of activity). Stop when duration is reached. stop_disable if disabled.
#define FIT_EVENT_CALORIE_DURATION_ALERT                                         ((FIT_EVENT)25) // Group 1. Start if enabled mid activity (not required at start of activity). Stop when duration is reached. stop_disable if disabled.
#define FIT_EVENT_ACTIVITY                                                       ((FIT_EVENT)26) // Group 1.. Stop at end of activity.
#define FIT_EVENT_FITNESS_EQUIPMENT                                              ((FIT_EVENT)27) // marker
#define FIT_EVENT_LENGTH                                                         ((FIT_EVENT)28) // Stop at end of each length.
#define FIT_EVENT_USER_MARKER                                                    ((FIT_EVENT)32) // marker
#define FIT_EVENT_SPORT_POINT                                                    ((FIT_EVENT)33) // marker
#define FIT_EVENT_CALIBRATION                                                    ((FIT_EVENT)36) // start/stop/marker
#define FIT_EVENT_FRONT_GEAR_CHANGE                                              ((FIT_EVENT)42) // marker
#define FIT_EVENT_REAR_GEAR_CHANGE                                               ((FIT_EVENT)43) // marker
#define FIT_EVENT_RIDER_POSITION_CHANGE                                          ((FIT_EVENT)44) // marker
#define FIT_EVENT_ELEV_HIGH_ALERT                                                ((FIT_EVENT)45) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_ELEV_LOW_ALERT                                                 ((FIT_EVENT)46) // Group 0. Start / stop when in alert condition.
#define FIT_EVENT_COMM_TIMEOUT                                                   ((FIT_EVENT)47) // marker
#define FIT_EVENT_DIVE_ALERT                                                     ((FIT_EVENT)56) // marker
#define FIT_EVENT_DIVE_GAS_SWITCHED                                              ((FIT_EVENT)57) // marker
#define FIT_EVENT_TANK_PRESSURE_RESERVE                                          ((FIT_EVENT)71) // marker
#define FIT_EVENT_TANK_PRESSURE_CRITICAL                                         ((FIT_EVENT)72) // marker
#define FIT_EVENT_TANK_LOST                                                      ((FIT_EVENT)73) // marker
#define FIT_EVENT_RADAR_THREAT_ALERT                                             ((FIT_EVENT)75) // start/stop/marker
#define FIT_EVENT_TANK_BATTERY_LOW                                               ((FIT_EVENT)76) // marker
#define FIT_EVENT_TANK_POD_CONNECTED                                             ((FIT_EVENT)81) // marker - tank pod has connected
#define FIT_EVENT_TANK_POD_DISCONNECTED                                          ((FIT_EVENT)82) // marker - tank pod has lost connection

typedef FIT_ENUM FIT_EVENT_TYPE;
#define FIT_EVENT_TYPE_INVALID                                                   FIT_ENUM_INVALID
#define FIT_EVENT_TYPE_START                                                     ((FIT_EVENT_TYPE)0)
#define FIT_EVENT_TYPE_STOP                                                      ((FIT_EVENT_TYPE)1)
#define FIT_EVENT_TYPE_CONSECUTIVE_DEPRECIATED                                   ((FIT_EVENT_TYPE)2)
#define FIT_EVENT_TYPE_MARKER                                                    ((FIT_EVENT_TYPE)3)
#define FIT_EVENT_TYPE_STOP_ALL                                                  ((FIT_EVENT_TYPE)4)
#define FIT_EVENT_TYPE_BEGIN_DEPRECIATED                                         ((FIT_EVENT_TYPE)5)
#define FIT_EVENT_TYPE_END_DEPRECIATED                                           ((FIT_EVENT_TYPE)6)
#define FIT_EVENT_TYPE_END_ALL_DEPRECIATED                                       ((FIT_EVENT_TYPE)7)
#define FIT_EVENT_TYPE_STOP_DISABLE                                              ((FIT_EVENT_TYPE)8)
#define FIT_EVENT_TYPE_STOP_DISABLE_ALL                                          ((FIT_EVENT_TYPE)9)
