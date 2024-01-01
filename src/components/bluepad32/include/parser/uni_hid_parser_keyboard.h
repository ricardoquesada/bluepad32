// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_KEYBOARD_H
#define UNI_HID_PARSER_KEYBOARD_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

// Mouse devices
void uni_hid_parser_keyboard_setup(struct uni_hid_device_s* d);
void uni_hid_parser_keyboard_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_keyboard_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_keyboard_parse_usage(struct uni_hid_device_s* d,
                                         hid_globals_t* globals,
                                         uint16_t usage_page,
                                         uint16_t usage,
                                         int32_t value);
void uni_hid_parser_keyboard_device_dump(struct uni_hid_device_s* d);

#endif  // UNI_HID_PARSER_KEYBOARD_H