// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_8BITDO_H
#define UNI_HID_PARSER_8BITDO_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

// 8Bitdo controllers
void uni_hid_parser_8bitdo_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_8bitdo_parse_usage(struct uni_hid_device_s* d,
                                       hid_globals_t* globals,
                                       uint16_t usage_page,
                                       uint16_t usage,
                                       int32_t value);

#endif  // UNI_HID_PARSER_8BITDO_H