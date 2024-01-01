// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_DS5_H
#define UNI_HID_PARSER_DS5_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

// For DualSense gamepads
void uni_hid_parser_ds5_setup(struct uni_hid_device_s* d);
void uni_hid_parser_ds5_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_ds5_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds5_parse_feature_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds5_set_player_leds(struct uni_hid_device_s* d, uint8_t value);
void uni_hid_parser_ds5_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
void uni_hid_parser_ds5_set_rumble(struct uni_hid_device_s* d, uint8_t value, uint8_t duration);
void uni_hid_parser_ds5_device_dump(struct uni_hid_device_s* d);
#endif  // UNI_HID_PARSER_DS5_H
