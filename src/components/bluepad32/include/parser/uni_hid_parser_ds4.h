// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_DS4_H
#define UNI_HID_PARSER_DS4_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// For DUALSHOCK 4 gamepads
void uni_hid_parser_ds4_setup(struct uni_hid_device_s* d);
void uni_hid_parser_ds4_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_ds4_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds4_parse_feature_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds4_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
void uni_hid_parser_ds4_play_dual_rumble(struct uni_hid_device_s* d,
                                         uint16_t start_delay_ms,
                                         uint16_t duration_ms,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude);
/**
 * @brief Formats DualShock 4 firmware and hardware versions into `buf`
 *        (e.g., `"FW version 0x412, HW version 0x100"`).
 *
 * Note: `fw_version` and `hw_version` are initially `0` when `setup()` completes and are
 * populated asynchronously when feature report `0xa3` (`DS4_FEATURE_REPORT_FIRMWARE_VERSION`) arrives.
 */
int uni_hid_parser_ds4_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_DS4_H
