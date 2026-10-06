// SPDX-License-Identifier: Apache-2.0
// SInput HID gamepads (Handheld Legend). Spec: https://docs.handheldlegend.com/s/sinput

#ifndef UNI_HID_PARSER_SINPUT_H
#define UNI_HID_PARSER_SINPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "parser/uni_hid_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fallback VID/PID from the SInput spec. Intended for testing; real products should register their own PID.
#define UNI_HID_PARSER_SINPUT_VID 0x2e8a
#define UNI_HID_PARSER_SINPUT_PID 0x10c6

void uni_hid_parser_sinput_setup(struct uni_hid_device_s* d);
void uni_hid_parser_sinput_deinit(struct uni_hid_device_s* d);
void uni_hid_parser_sinput_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_sinput_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
// leds: Bluepad32 player-LED bitmask; the lowest set bit selects the SInput player index (bit 0 -> player 1).
void uni_hid_parser_sinput_set_player_leds(struct uni_hid_device_s* d, uint8_t leds);
void uni_hid_parser_sinput_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude);
void uni_hid_parser_sinput_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
/**
 * @brief Formats SInput protocol version, capability bitmasks, polling interval, and IMU
 *        sensor ranges into `buf` once the feature response (`0x02`) has been received
 *        (e.g., `"protocol=1, caps0=0x0f, caps1=0x01, poll=1000us, accel=+/-8g, gyro=+/-2000dps"`),
 *        or writes `buf[0] = '\0'` and returns `0` before the feature response arrives.
 */
int uni_hid_parser_sinput_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len);

// Capability bits from the feature response (command 0x02), byte 0 and byte 1.
#define UNI_SINPUT_CAPS0_RUMBLE 0x01
#define UNI_SINPUT_CAPS0_PLAYER_LEDS 0x02
#define UNI_SINPUT_CAPS0_ACCEL 0x04
#define UNI_SINPUT_CAPS0_GYRO 0x08
#define UNI_SINPUT_CAPS0_LEFT_STICK 0x10
#define UNI_SINPUT_CAPS0_RIGHT_STICK 0x20
#define UNI_SINPUT_CAPS0_LEFT_TRIGGER 0x40
#define UNI_SINPUT_CAPS0_RIGHT_TRIGGER 0x80
#define UNI_SINPUT_CAPS1_TOUCHPAD 0x01
#define UNI_SINPUT_CAPS1_RGB 0x02
#define UNI_SINPUT_CAPS1_HANDHELD 0x04

// Returns false until the device has answered the feature request.
bool uni_hid_parser_sinput_get_features(struct uni_hid_device_s* d,
                                        uint16_t* protocol_version,
                                        uint8_t* caps0,
                                        uint8_t* caps1);

// IMU settings from the feature response. A range is 0 when the device has no such sensor. Returns false until the
// feature response has arrived.
bool uni_hid_parser_sinput_get_imu_config(struct uni_hid_device_s* d,
                                          uint16_t* poll_us,
                                          uint16_t* accel_range_g,
                                          uint16_t* gyro_range_dps);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_SINPUT_H
