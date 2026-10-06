// SPDX-License-Identifier: Apache-2.0
// MegaCadeDev
// https://github.com/MegaCadeDev/OGX-Mini-2026

/**
 * @file uni_hid_parser_steam_triton.h
 * @brief Valve Steam Controller 2026 ("Triton") BLE GATT driver and input report parser interface.
 *
 * Supports the 2nd-generation Valve Steam Controller ("Triton") family:
 * - `28de:1302`: Steam Controller 2026 USB
 * - `28de:1303`: Steam Controller 2026 BLE
 * - `28de:1304`: Steam Controller 2026 Wireless Puck
 * - `28de:1305`: Steam Controller 2026 Nereid
 *
 * On BLE (`28de:1303`), standard HOGP (`hids_host_connect()`) stalls during HID Report Map
 * discovery after Device Information Service (DIS) queries complete. Instead, this driver
 * discovers Valve's proprietary 128-bit GATT service (`100F6C32-1735-4313-B402-38567131E5F3`),
 * subscribes to the timestamped (`0x7c` -> report `0x47`) or standard (`0x7a` -> report `0x45`)
 * input characteristic, writes the Enter Valve Mode payload (`{0xc0, 0x87, 0x03, 0x08, 0x07, 0x00}`)
 * to control characteristic `0x34`, and decodes state/battery/IMU telemetry and haptic rumble.
 */

#ifndef UNI_HID_PARSER_STEAM_TRITON_H
#define UNI_HID_PARSER_STEAM_TRITON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uni_hid_device_s;

#define UNI_TRITON_VALVE_VID 0x28de   ///< Valve Corporation Vendor ID.
#define UNI_TRITON_PID_USB 0x1302     ///< Steam Controller 2026 ("Triton") USB Product ID.
#define UNI_TRITON_PID_BLE 0x1303     ///< Steam Controller 2026 ("Triton") BLE Product ID.
#define UNI_TRITON_PID_PUCK 0x1304    ///< Steam Controller 2026 ("Triton") Wireless Puck Product ID.
#define UNI_TRITON_PID_NEREID 0x1305  ///< Steam Controller 2026 ("Triton") Nereid Product ID.
#define STEAM_TRITON_BLE_PID \
    UNI_TRITON_PID_BLE  ///< Alias for the BLE Product ID (`0x1303`) used in BLE advertising probes.

// Steam Controller 2026 ("Triton") Input & Output Report IDs
#define ID_TRITON_CONTROLLER_STATE 0x42      ///< Full controller state report (USB / wired).
#define ID_TRITON_BATTERY_STATUS 0x43        ///< Battery percentage report (`report[2]` = `0..100`).
#define ID_TRITON_CONTROLLER_STATE_BLE 0x45  ///< BLE controller state report (characteristic suffix `0x7a`).
#define ID_TRITON_CONTROLLER_STATE_TIMESTAMP \
    0x47  ///< Timestamped BLE controller state report (characteristic suffix `0x7c`).
#define ID_TRITON_OUT_REPORT_HAPTIC_RUMBLE \
    0x80  ///< Haptic rumble output report command opcode (characteristic suffix `0xb5`).

// Button bitmasks in 32-bit little-endian button word at p[1..4] (report[2..5])
#define TRITON_BTN_A 0x00000001u
#define TRITON_BTN_B 0x00000002u
#define TRITON_BTN_X 0x00000004u
#define TRITON_BTN_Y 0x00000008u
#define TRITON_BTN_QAM 0x00000010u  ///< Quick Access Menu (`...`) button -> mapped to `MISC_BUTTON_CAPTURE`.
#define TRITON_BTN_R3 0x00000020u
#define TRITON_BTN_VIEW 0x00000040u  ///< Left View/Select button -> mapped to `MISC_BUTTON_SELECT`.
#define TRITON_BTN_R4 0x00000080u
#define TRITON_BTN_R5 0x00000100u
#define TRITON_BTN_RB 0x00000200u
#define TRITON_BTN_DPAD_DOWN 0x00000400u
#define TRITON_BTN_DPAD_RIGHT 0x00000800u
#define TRITON_BTN_DPAD_LEFT 0x00001000u
#define TRITON_BTN_DPAD_UP 0x00002000u
#define TRITON_BTN_MENU 0x00004000u  ///< Right Menu/Start button -> mapped to `MISC_BUTTON_START`.
#define TRITON_BTN_L3 0x00008000u
#define TRITON_BTN_STEAM 0x00010000u  ///< Steam Home button -> mapped to `MISC_BUTTON_SYSTEM`.
#define TRITON_BTN_L4 0x00020000u
#define TRITON_BTN_L5 0x00040000u
#define TRITON_BTN_LB 0x00080000u
#define TRITON_BTN_LT_FULL 0x00100000u
#define TRITON_BTN_RT_FULL 0x00200000u
#define TRITON_BTN_RPAD_CLICK 0x00400000u
#define TRITON_BTN_LPAD_CLICK 0x04000000u

/**
 * @brief Initializes the Steam Triton parser instance and starts Valve GATT service discovery.
 *
 * Resets per-device state in `d->parser_data` and—when `d->conn.handle != UNI_BT_CONN_HANDLE_INVALID`—
 * arms the connection timeout watchdog and discovers primary service `100F6C32-1735-4313-B402-38567131E5F3`.
 * Guards `UNI_BT_CONN_HANDLE_INVALID` so stack-allocated synthetic test devices do not register
 * dangling timers in BTstack's run loop.
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_steam_triton_setup(struct uni_hid_device_s* d);

/**
 * @brief Stops all Steam Triton BTstack timers (`conn_timer`, `rumble_timer`) and unregisters
 *        the GATT notification listener.
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_steam_triton_deinit(struct uni_hid_device_s* d);

/**
 * @brief Per-frame report initialization hook (no-op; each state report carries full state).
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_steam_triton_init_report(struct uni_hid_device_s* d);

/**
 * @brief Parses a Steam Triton input report (`0x42`, `0x45`, `0x47`, or battery `0x43`).
 *
 * Decodes buttons, D-pad, misc buttons (`VIEW`/`MENU`/`STEAM`/`QAM`), 16-bit triggers
 * (`0..32767` -> `0..1023`), `INT16_MIN`-safe 16-bit signed thumbsticks (`[-512, 511]`),
 * 6-axis IMU (`m/s^2` and `rad/s` in Bluepad32's right-handed Y-up coordinate frame when
 * `len >= 46`), and battery status (`0x43`).
 *
 * @param d      Target HID device instance.
 * @param report Raw report buffer (starting with the report ID byte).
 * @param len    Report length in bytes.
 */
void uni_hid_parser_steam_triton_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);

/**
 * @brief Plays or stops dual-actuator haptic rumble on a Steam Triton controller.
 *
 * Transmits an 11-byte `{0xc0, 0x80, 9, ...}` haptic rumble packet to `rumble_value_handle`
 * (`0xb5`, falling back to `report_value_handle` `0x34`) and arms a delayed-off timer for
 * `start_delay_ms + duration_ms`.
 *
 * @param d                Target HID device instance.
 * @param start_delay_ms   Delay in milliseconds before rumble starts.
 * @param duration_ms      Rumble duration in milliseconds (`0` to stop immediately).
 * @param weak_magnitude   Right / weak actuator amplitude (`0..255`).
 * @param strong_magnitude Left / strong actuator amplitude (`0..255`).
 */
void uni_hid_parser_steam_triton_play_dual_rumble(struct uni_hid_device_s* d,
                                                  uint16_t start_delay_ms,
                                                  uint16_t duration_ms,
                                                  uint8_t weak_magnitude,
                                                  uint8_t strong_magnitude);

/**
 * @brief Formats single-line Steam Triton diagnostic metadata (`pid`, GATT state, stream report ID,
 *        and discovered ATT handles) into `buf`.
 *
 * @param d   Target HID device instance.
 * @param buf Destination character buffer.
 * @param len Capacity of `buf` in bytes.
 * @return Number of characters that would have been written (excluding NUL), or `-1` on invalid args.
 */
int uni_hid_parser_steam_triton_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len);

/**
 * @brief Returns true if `(vid, pid)` identifies a Valve Steam Controller 2026 ("Triton") model.
 *
 * @param vid Vendor ID.
 * @param pid Product ID.
 * @return `true` if `vid == 0x28de` and `pid` is in `{0x1302, 0x1303, 0x1304, 0x1305}`.
 */
bool uni_hid_parser_steam_triton_is_triton_device(uint16_t vid, uint16_t pid);

/**
 * @brief Returns true if `d` is a Valve Steam Controller 2026 ("Triton") device.
 *
 * @param d Target HID device instance (may be NULL).
 * @return `true` if `d` is non-NULL and has a Triton VID/PID.
 */
bool uni_hid_parser_steam_triton_is_device(const struct uni_hid_device_s* d);

/**
 * @brief BTstack GATT event callback driving the Steam Triton service discovery, characteristic
 *        selection (`0x7c` > `0x7a`, `0x34`, `0xb5`), CCCD subscription, and input notification
 *        forwarding state machine.
 *
 * @param packet_type HCI packet type (`HCI_EVENT_PACKET`).
 * @param channel     BTstack channel (unused).
 * @param packet      Raw GATT event packet buffer.
 * @param size        Packet buffer size in bytes.
 */
void uni_hid_parser_steam_triton_handle_gatt_event(uint8_t packet_type,
                                                   uint16_t channel,
                                                   uint8_t* packet,
                                                   uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_STEAM_TRITON_H
