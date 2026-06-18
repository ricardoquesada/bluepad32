// SPDX-License-Identifier: Apache-2.0
// Switch 2 (Joy-Con 2 / Pro Controller 2) BLE GATT support for OGX-Mini.
// Protocol reference: Nadeflore/switch2-controllers, TommyWabg/switch2-controllers-windows10-gyro

#ifndef UNI_HID_PARSER_SWITCH2_H
#define UNI_HID_PARSER_SWITCH2_H

#include <stdbool.h>
#include <stdint.h>

#include "parser/uni_hid_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UNI_SW2_NINTENDO_VID 0x057e
#define UNI_SW2_JOYCON_R_PID 0x2066
#define UNI_SW2_JOYCON_L_PID 0x2067
#define UNI_SW2_PRO_PID 0x2069

/** Returns true if the GAP advertising report was a Switch 2 controller and connect was started. */
bool uni_bt_le_switch2_handle_advertisement(const uint8_t* packet, uint16_t size);

/** Called after LE connection is up; begins GATT before BLE SMP (Switch 2 SYNC flow). */
void uni_hid_parser_switch2_on_le_connected(struct uni_hid_device_s* d);

/** Called from uni_bt_le_on_hci_event_encryption_change for Switch 2 BLE devices. */
void uni_hid_parser_switch2_on_encrypted(struct uni_hid_device_s* d);

bool uni_hid_parser_switch2_is_ble_device(const struct uni_hid_device_s* d);

/** True when the controller is advertising for SYNC / first-time pairing. */
bool uni_hid_parser_switch2_needs_pair(const struct uni_hid_device_s* d);

void uni_hid_parser_switch2_setup(struct uni_hid_device_s* d);
void uni_hid_parser_switch2_teardown(struct uni_hid_device_s* d);
void uni_hid_parser_switch2_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_switch2_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_switch2_set_player_leds(struct uni_hid_device_s* d, uint8_t player);
void uni_hid_parser_switch2_play_dual_rumble(struct uni_hid_device_s* d, uint16_t start_delay_ms, uint16_t duration_ms,
                                             uint8_t weak_magnitude, uint8_t strong_magnitude);

/** Idle HD-rumble packet; must be sent often (~5 ms) or the link drops (HCI 0x08). */
void uni_hid_parser_switch2_send_keepalive(struct uni_hid_device_s* d);

bool uni_hid_parser_switch2_is_ready(const struct uni_hid_device_s* d);

/** True when this Joy-Con is the right half of a merged pair (no separate player slot). */
bool uni_hid_parser_switch2_is_joycon_pair_secondary(const struct uni_hid_device_s* d);

/** True when the per-device 5 ms keepalive timer is running (primary Joy-Con when paired). */
bool uni_hid_parser_switch2_keepalive_timer_active(const struct uni_hid_device_s* d);

/** Gamepad slot index for OGX output (merged pairs share the lower slot = player 1). */
int uni_hid_parser_switch2_get_gamepad_output_idx(const struct uni_hid_device_s* d);

/** Partner device index when Joy-Cons are paired, else -1. */
int uni_hid_parser_switch2_get_pair_partner_idx(const struct uni_hid_device_s* d);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_SWITCH2_H
