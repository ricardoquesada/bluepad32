// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_ICADE_H
#define UNI_HID_PARSER_ICADE_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// ION iCade setup.
void uni_hid_parser_icade_setup(struct uni_hid_device_s* d);

// ION iCade parser.
void uni_hid_parser_icade_parse_usage(struct uni_hid_device_s* d,
                                      const hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_ICADE_H