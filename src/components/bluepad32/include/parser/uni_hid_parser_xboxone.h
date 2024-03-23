// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_XBOXONE_H
#define UNI_HID_PARSER_XBOXONE_H

#include <stdbool.h>
#include <stdint.h>

#include "parser/uni_hid_parser.h"

// For Xbox Wireless Controllers
bool uni_hid_parser_xboxone_does_name_match(struct uni_hid_device_s* d, const char* name);
void uni_hid_parser_xboxone_setup(struct uni_hid_device_s* d);
void uni_hid_parser_xboxone_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_xboxone_parse_usage(struct uni_hid_device_s* d,
                                        hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value);
void uni_hid_parser_xboxone_play_dual_rumble(struct uni_hid_device_s* d,
                                             uint16_t start_delay_ms,
                                             uint16_t duration_ms,
                                             uint8_t weak_magnitude,
                                             uint8_t strong_magnitude);
void uni_hid_parser_xboxone_device_dump(struct uni_hid_device_s* d);

// Unique to Xbox. Not part of the "hid_parser" interface
void xboxone_play_quad_rumble(struct uni_hid_device_s* d,
                              uint16_t start_delay_ms,
                              uint16_t duration_ms,
                              uint8_t left_trigger,
                              uint8_t right_trigger,
                              uint8_t weak_magnitude,
                              uint8_t strong_magnitude);

#endif  // UNI_HID_PARSER_XBOXONE_H
