// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_GENERIC_H
#define UNI_HID_PARSER_GENERIC_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

// Android devices
void uni_hid_parser_generic_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_generic_parse_usage(struct uni_hid_device_s* d,
                                        hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value);

#endif  // UNI_HID_PARSER_GENERIC_H