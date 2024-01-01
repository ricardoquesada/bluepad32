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
void uni_hid_parser_psmove_set_rumble(struct uni_hid_device_s* d, uint8_t value, uint8_t duration);
void uni_hid_parser_psmove_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);

#endif  // UNI_HID_PARSER_PSMOVE_H
