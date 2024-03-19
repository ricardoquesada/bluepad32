// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_DS3_H
#define UNI_HID_PARSER_DS3_H

#include <stdbool.h>
#include <stdint.h>

#include "parser/uni_hid_parser.h"

// For DUALSHOCK 3 gamepads
void uni_hid_parser_ds3_setup(struct uni_hid_device_s* d);
void uni_hid_parser_ds3_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_ds3_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds3_set_player_leds(struct uni_hid_device_s* d, uint8_t leds);
void uni_hid_parser_ds3_play_dual_rumble(struct uni_hid_device_s* d,
                                         uint16_t start_delay_ms,
                                         uint16_t duration_ms,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude);
bool uni_hid_parser_ds3_does_name_match(struct uni_hid_device_s* d, const char* name);

#endif  // UNI_HID_PARSER_DS3_H
