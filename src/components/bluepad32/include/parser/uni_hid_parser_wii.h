// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_WII_H
#define UNI_HID_PARSER_WII_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"
#include "uni_common.h"

enum wii_flags {
    WII_FLAGS_NONE = 0,
    WII_FLAGS_VERTICAL = BIT(0),
    WII_FLAGS_ACCEL = BIT(1),
    WII_FLAGS_HORIZONTAL = BIT(2),
    WII_FLAGS_NATIVE = BIT(3),
};
typedef enum wii_flags wii_flags_t;

void wii_force_flags(wii_flags_t flags);

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

#endif  // UNI_HID_PARSER_WII_H
