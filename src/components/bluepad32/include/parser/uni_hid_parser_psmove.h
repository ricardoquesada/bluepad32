// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_PSMOVE_H
#define UNI_HID_PARSER_PSMOVE_H

#include <stdbool.h>
#include <stdint.h>

#include "parser/uni_hid_parser.h"

// For PS Move controller
void uni_hid_parser_psmove_setup(struct uni_hid_device_s* d);
void uni_hid_parser_psmove_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_psmove_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_psmove_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
void uni_hid_parser_psmove_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude);

#endif  // UNI_HID_PARSER_PSMOVE_H
