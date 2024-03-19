// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_SWITCH_H
#define UNI_HID_PARSER_SWITCH_H

#include <stdbool.h>
#include <stdint.h>

#include "parser/uni_hid_parser.h"

// Nintendo Switch devices
void uni_hid_parser_switch_setup(struct uni_hid_device_s* d);
void uni_hid_parser_switch_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_switch_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_switch_set_player_leds(struct uni_hid_device_s* d, uint8_t leds);
void uni_hid_parser_switch_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude);
bool uni_hid_parser_switch_does_name_match(struct uni_hid_device_s* d, const char* name);
void uni_hid_parser_switch_device_dump(struct uni_hid_device_s* d);

#endif  // UNI_HID_PARSER_SWITCH_H
