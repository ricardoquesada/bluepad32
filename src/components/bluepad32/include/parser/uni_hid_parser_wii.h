// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_WII_H
#define UNI_HID_PARSER_WII_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

void uni_hid_parser_wii_setup(struct uni_hid_device_s* d);
void uni_hid_parser_wii_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_wii_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_wii_set_player_leds(struct uni_hid_device_s* d, uint8_t leds);
void uni_hid_parser_wii_set_rumble(struct uni_hid_device_s* d, uint8_t value, uint8_t duration);
void uni_hid_parser_wii_device_dump(struct uni_hid_device_s* d);

#endif  // UNI_HID_PARSER_WII_H
