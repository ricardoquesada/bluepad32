// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_WII_H
#define UNI_HID_PARSER_WII_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"
#include "uni_common.h"

typedef enum wii_flags {
    WII_MODE_HORIZONTAL = 0,
    WII_MODE_VERTICAL = 1,
    WII_MODE_ACCEL = 2,
} wii_mode_t;

typedef enum wii_report_type {
    WII_REPORT_TYPE_K = 0x30,      // Core Buttons
    WII_REPORT_TYPE_KA = 0x31,     // Core Buttons + Accelerometer
    WII_REPORT_TYPE_KE = 0x32,     // Core Buttons + 8 Extension Bytes
    WII_REPORT_TYPE_KAI = 0x33,    // Core Buttons + Accelerometer with 12 IR bytes
    WII_REPORT_TYPE_KEE = 0x34,    // Core Buttons + 19 Extension bytes
    WII_REPORT_TYPE_KAE = 0x35,    // Core Buttons + Accelerometer with 16 Extension Bytes
    WII_REPORT_TYPE_KIE = 0x36,    // Core Buttons + 10 IR bytes and 9 Extension Bytes
    WII_REPORT_TYPE_KAIE = 0x37,   // Core Buttons + Accelerometer with 10 IR bytes and 6 Extension Bytes
    WII_REPORT_TYPE_E = 0x3d,      // 21 extension bytes (no core buttons)
    WII_REPORT_TYPE_SKAI1 = 0x3e,  // Interleaved Core Buttons + Accelerometer with 36 IR bytes
    WII_REPORT_TYPE_SKAI2 = 0x3f,
} wii_report_type_t;

void uni_hid_parser_wii_setup(struct uni_hid_device_s* d);
void uni_hid_parser_wii_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_wii_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_wii_set_player_leds(struct uni_hid_device_s* d, uint8_t leds);
void uni_hid_parser_wii_play_dual_rumble(struct uni_hid_device_s* d,
                                         uint16_t start_delay_ms,
                                         uint16_t duration_ms,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude);
void uni_hid_parser_wii_device_dump(struct uni_hid_device_s* d);

// Unique to Wii. Not part of the "hid_parser" interface
void uni_hid_parser_wii_set_mode(struct uni_hid_device_s* d, wii_mode_t mode);
void uni_hid_parser_wii_request_report_type(struct uni_hid_device_s* d, wii_report_type_t report_type);

#endif  // UNI_HID_PARSER_WII_H
