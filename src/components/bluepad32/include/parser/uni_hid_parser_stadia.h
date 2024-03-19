// SPDX-License-Identifier: Apache-2.0, MIT
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_STADIA_H
#define UNI_HID_PARSER_STADIA_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

#define UNI_HID_PARSER_STADIA_VID 0x18d1
#define UNI_HID_PARSER_STADIA_PID 0x9400

void uni_hid_parser_stadia_setup(struct uni_hid_device_s* d);
void uni_hid_parser_stadia_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude);

#endif  // UNI_HID_PARSER_STADIA_H
