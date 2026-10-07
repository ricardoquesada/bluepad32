// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_STEAM_H
#define UNI_HID_PARSER_STEAM_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// Steam devices
void uni_hid_parser_steam_setup(struct uni_hid_device_s* d);
void uni_hid_parser_steam_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_steam_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_steam_play_dual_rumble(struct uni_hid_device_s* d,
                                           uint16_t start_delay_ms,
                                           uint16_t duration_ms,
                                           uint8_t weak_magnitude,
                                           uint8_t strong_magnitude);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_STEAM_H
