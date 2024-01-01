// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_ATARI_H
#define UNI_HID_PARSER_ATARI_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

// Atari VCS Modern Wireless controller / joystick.
void uni_hid_parser_atari_setup(struct uni_hid_device_s* d);
void uni_hid_parser_atari_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_atari_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);

#endif  // UNI_HID_PARSER_ATARI_H
