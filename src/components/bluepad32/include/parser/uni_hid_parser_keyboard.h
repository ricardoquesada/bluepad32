// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_KEYBOARD_H
#define UNI_HID_PARSER_KEYBOARD_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// Keyboard devices
void uni_hid_parser_keyboard_setup(struct uni_hid_device_s* d);
void uni_hid_parser_keyboard_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_keyboard_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_keyboard_parse_usage(struct uni_hid_device_s* d,
                                         const hid_globals_t* globals,
                                         uint16_t usage_page,
                                         uint16_t usage,
                                         int32_t value);
/**
 * @brief Keyboard parser `device_extra_info` callback; writes an empty string (`buf[0] = '\0'`)
 *        and returns `0` since generic HID keyboards do not report extra firmware metadata.
 */
int uni_hid_parser_keyboard_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len);

// Unique to Keyboard. Not part of the "hid_parser" interface
void uni_hid_parser_keyboard_set_leds(struct uni_hid_device_s* d, uint8_t led_bitmask);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_KEYBOARD_H