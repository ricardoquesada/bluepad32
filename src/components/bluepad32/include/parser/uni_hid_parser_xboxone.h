// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_XBOXONE_H
#define UNI_HID_PARSER_XBOXONE_H

#include <stdbool.h>
#include <stdint.h>

#include "parser/uni_hid_parser.h"

// For Xbox Wireless Controllers
bool uni_hid_parser_xboxone_does_name_match(struct uni_hid_device_s* d, const char* name);
void uni_hid_parser_xboxone_setup(struct uni_hid_device_s* d);
void uni_hid_parser_xboxone_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_xboxone_parse_usage(struct uni_hid_device_s* d,
                                        const hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value);
void uni_hid_parser_xboxone_play_dual_rumble(struct uni_hid_device_s* d,
                                             uint16_t start_delay_ms,
                                             uint16_t duration_ms,
                                             uint8_t weak_magnitude,
                                             uint8_t strong_magnitude);
void uni_hid_parser_xboxone_device_dump(struct uni_hid_device_s* d);

/** BLE (Series / Xbox One w/ LE): periodic output write so the pad does not sleep the session (~1 min idle). */
void uni_hid_parser_xboxone_ble_keepalive(struct uni_hid_device_s* d);

/** Microsoft BLE gamepad on standard HIDS (not Switch 2 custom GATT). */
bool uni_hid_parser_xboxone_is_ble_hids(const struct uni_hid_device_s* d);

/** After HIDS connect: wake link, tune LE interval, poll input until first report. */
void uni_hid_parser_xboxone_ble_on_hid_connected(struct uni_hid_device_s* d);
/** Stop input polling once a report arrives (notify or GET). */
void uni_hid_parser_xboxone_ble_on_input(struct uni_hid_device_s* d);
void uni_hid_parser_xboxone_ble_teardown(struct uni_hid_device_s* d);

/** Built-in Series/One BLE HID descriptor — used when Report Map long-read hangs on CYW43. */
const uint8_t* uni_hid_parser_xboxone_ble_hid_descriptor(uint16_t* out_len);

// Unique to Xbox. Not part of the "hid_parser" interface
void xboxone_play_quad_rumble(struct uni_hid_device_s* d,
                              uint16_t start_delay_ms,
                              uint16_t duration_ms,
                              uint8_t trigger_left,
                              uint8_t trigger_right,
                              uint8_t weak_magnitude,
                              uint8_t strong_magnitude);

#endif  // UNI_HID_PARSER_XBOXONE_H
