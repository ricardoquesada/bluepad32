// SPDX-License-Identifier: Apache-2.0
// MegaCadeDev
// https://github.com/MegaCadeDev/OGX-Mini-2026

/**
 * @file uni_hid_parser_switch2.h
 * @brief Nintendo Switch 2 BLE GATT driver and input report parser interface.
 *
 * Supports the Nintendo Switch 2 controller family over Bluetooth Low Energy (BLE):
 * - Nintendo Switch 2 Pro Controller (`057e:2069`)
 * - Nintendo Switch 2 Joy-Con Left (`057e:2067`, standalone horizontal orientation)
 * - Nintendo Switch 2 Joy-Con Right (`057e:2066`, standalone horizontal orientation)
 *
 * Unlike standard BLE gamepads that use the HID Over GATT Profile (HOGP, UUID `0x1812`),
 * Switch 2 controllers advertise Nintendo manufacturer data (`0xFF`) and communicate over
 * a proprietary 128-bit BLE GATT service. This module manages BLE advertisement matching,
 * GATT service/characteristic/descriptor discovery, the `0x2b29` bootstrap gate write,
 * SPI flash stick calibration reads (`0x001fc042`), the 4-step `0x15` pairing handshake,
 * the 13-step command initialization sequence, the 5 ms HD Rumble 2 keepalive/rumble loop,
 * player LED control, and 63-byte input report decoding with SI-unit IMU output.
 */

#ifndef UNI_HID_PARSER_SWITCH2_H
#define UNI_HID_PARSER_SWITCH2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uni_hid_device_s;

#define UNI_SW2_NINTENDO_VID 0x057e    ///< Nintendo Co., Ltd. Vendor ID.
#define UNI_SW2_JOYCON_R_PID 0x2066    ///< Nintendo Switch 2 Joy-Con (R) Product ID.
#define UNI_SW2_JOYCON_L_PID 0x2067    ///< Nintendo Switch 2 Joy-Con (L) Product ID.
#define UNI_SW2_PRO_PID 0x2069         ///< Nintendo Switch 2 Pro Controller Product ID.
#define UNI_SW2_MFG_COMPANY_ID 0x0553  ///< Company ID prefix in 18-byte Switch 2 BLE manufacturer data.

/**
 * @brief Initializes the Switch 2 per-device parser instance and starts BLE GATT discovery.
 *
 * Resets per-device state in `d->parser_data`, initializes default stick calibration,
 * and—when `d->conn.handle != UNI_BT_CONN_HANDLE_INVALID`—arms the setup watchdog timer
 * and starts primary service discovery. Guards `UNI_BT_CONN_HANDLE_INVALID` so synthetic
 * unit test devices allocated on the stack do not link dangling timers into BTstack.
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_switch2_setup(struct uni_hid_device_s* d);

/**
 * @brief Stops all Switch 2 BTstack timers and unregisters GATT notification listeners.
 *
 * Must be called before disconnecting or zeroing `uni_hid_device_t` so intrusive
 * `btstack_timer_source_t` and `gatt_client_notification_t` nodes embedded in
 * `d->parser_data` are unlinked from BTstack's global run-loop lists.
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_switch2_deinit(struct uni_hid_device_s* d);

/**
 * @brief Per-frame report initialization hook (no-op; each notification carries full state).
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_switch2_init_report(struct uni_hid_device_s* d);

/**
 * @brief Parses a Switch 2 BLE input report or SPI calibration response.
 *
 * Decodes 63-byte input notifications (buttons, D-pad, 12-bit packed sticks, Pro 2 analog
 * triggers with digital `ZL`/`ZR` fallback, battery percentage, board temperature, and
 * 6-axis IMU in SI `m/s^2` and `rad/s`), applying standalone horizontal 90-degree CCW/CW
 * rotations for Joy-Con 2 Left and Joy-Con 2 Right.
 *
 * @param d      Target HID device instance.
 * @param report Raw notification payload bytes.
 * @param len    Payload length in bytes.
 */
void uni_hid_parser_switch2_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);

/**
 * @brief Sets the 4-bit player LED bitmask on a Switch 2 controller.
 *
 * Saves `leds & 0x0f` in the parser instance (so LEDs requested prior to `SW2_STATE_READY`
 * are applied when setup finishes) and sends command `0x09` / subcommand `0x07`.
 *
 * @param d    Target HID device instance.
 * @param leds 4-bit player LED bitmask (`leds & 0x0f`, e.g., `0x01` for LED 1).
 */
void uni_hid_parser_switch2_set_player_leds(struct uni_hid_device_s* d, uint8_t leds);

/**
 * @brief Schedules or stops dual-actuator HD Rumble 2 vibration on a Switch 2 controller.
 *
 * Because Switch 2 requires a periodic 5 ms packet on the vibration characteristic to
 * prevent connection timeouts, rumble timing (`start_delay_ms` and `duration_ms`) is
 * integrated directly into the 5 ms keepalive loop so keepalive ticks never clobber
 * active vibration.
 *
 * @param d                Target HID device instance.
 * @param start_delay_ms   Delay in milliseconds before vibration starts (`0` for immediate).
 * @param duration_ms      Vibration duration in milliseconds (`0` to stop).
 * @param weak_magnitude   High-frequency / weak actuator amplitude (`0..255`).
 * @param strong_magnitude Low-frequency / strong actuator amplitude (`0..255`).
 */
void uni_hid_parser_switch2_play_dual_rumble(struct uni_hid_device_s* d,
                                             uint16_t start_delay_ms,
                                             uint16_t duration_ms,
                                             uint8_t weak_magnitude,
                                             uint8_t strong_magnitude);

/**
 * @brief Formats single-line Switch 2 diagnostic metadata (`pid`, GATT state, calibration,
 *        temperature, and discovered ATT handles) into `buf`.
 *
 * @param d   Target HID device instance.
 * @param buf Destination character buffer.
 * @param len Capacity of `buf` in bytes.
 * @return Number of characters that would have been written (excluding NUL), or `-1` on invalid args.
 */
int uni_hid_parser_switch2_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len);

/**
 * @brief Returns true if `(vid, pid)` identifies a supported Nintendo Switch 2 controller.
 *
 * @param vid Vendor ID.
 * @param pid Product ID.
 * @return `true` if `vid == 0x057e` and `pid` is `0x2066`, `0x2067`, or `0x2069`.
 */
bool uni_hid_parser_switch2_is_device(uint16_t vid, uint16_t pid);

/**
 * @brief Alias of `uni_hid_parser_switch2_is_device()` for `(vid, pid)` checks.
 *
 * @param vid Vendor ID.
 * @param pid Product ID.
 * @return `true` if `(vid, pid)` is a Nintendo Switch 2 controller.
 */
bool uni_hid_parser_switch2_is_switch2_device(uint16_t vid, uint16_t pid);

/**
 * @brief Returns true if `d` is a Nintendo Switch 2 BLE device.
 *
 * @param d Target HID device instance (may be NULL).
 * @return `true` if `d` is non-NULL and has Switch 2 VID/PID.
 */
bool uni_hid_parser_switch2_is_ble_device(const struct uni_hid_device_s* d);

/**
 * @brief Returns whether the Switch 2 device advertised SYNC pairing mode (`needs_pair`).
 *
 * @param d Target HID device instance.
 * @return `true` if the controller advertised an all-zero reconnect MAC (`needs_pair == true`).
 */
bool uni_hid_parser_switch2_needs_pair(const struct uni_hid_device_s* d);

/**
 * @brief Records whether the Switch 2 device requires the 4-step `0x15` pairing sequence.
 *
 * @param d          Target HID device instance.
 * @param needs_pair `true` if the device is in SYNC pairing mode, `false` on reconnect.
 */
void uni_hid_parser_switch2_set_needs_pair(struct uni_hid_device_s* d, bool needs_pair);

/**
 * @brief Parses a BLE Manufacturer Specific Data (`0xFF`) payload for Switch 2 metadata.
 *
 * Supports both the full 18-byte Switch 2 advertisement format (Company ID `0x0553` at
 * `[0..1]`, VID `0x057e` at `[5..6]`, PID at `[7..8]`, reconnect MAC at `[12..17]`) and
 * compact Nintendo manufacturer data (`0x057e` at `[0..1]`, PID at `[2..3]` or `[5..6]`).
 *
 * @param mfg               Pointer to the AD `0xFF` payload bytes (after the length and `0xFF` type bytes).
 * @param mfg_len           Length of `mfg` in bytes.
 * @param out_pid           Optional output for the matched Switch 2 Product ID.
 * @param out_reconnect_mac Optional 6-byte buffer receiving the advertised reconnect MAC.
 * @param out_needs_pair    Optional output set to `true` when `reconnect_mac` is all zeros.
 * @return `true` if `mfg` is a valid Switch 2 manufacturer data payload, `false` otherwise.
 */
bool uni_hid_parser_switch2_parse_mfg_data(const uint8_t* mfg,
                                           uint8_t mfg_len,
                                           uint16_t* out_pid,
                                           uint8_t out_reconnect_mac[6],
                                           bool* out_needs_pair);

/**
 * @brief Bounds-checked probe testing whether a `GAP_EVENT_ADVERTISING_REPORT` packet
 *        originates from a Nintendo Switch 2 controller.
 *
 * Enforces `size >= 12` and clamps `ad_len` to `size - 12` before iterating AD structures.
 *
 * @param packet Raw HCI `GAP_EVENT_ADVERTISING_REPORT` packet buffer.
 * @param size   Total packet buffer size in bytes.
 * @return `true` if the advertisement matches a Switch 2 controller, `false` otherwise.
 */
bool uni_hid_parser_switch2_does_packet_match(const uint8_t* packet, uint16_t size);

/**
 * @brief Handles a Switch 2 BLE advertisement by creating a `uni_hid_device_t` and initiating
 *        a GAP LE connection.
 *
 * @param packet Raw HCI `GAP_EVENT_ADVERTISING_REPORT` packet buffer.
 * @param size   Total packet buffer size in bytes.
 * @return `true` if the advertisement was a Switch 2 packet and was handled, `false` otherwise.
 */
bool uni_bt_le_switch2_handle_advertisement(const uint8_t* packet, uint16_t size);

/**
 * @brief Invoked by `uni_bt_le.c` when an unbonded or pairing Switch 2 LE connection completes.
 *
 * Resolves the controller type, marks the device connected (idempotently), and transitions
 * to `uni_hid_device_set_ready(d)` to start custom GATT discovery.
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_switch2_on_le_connected(struct uni_hid_device_s* d);

/**
 * @brief Invoked by `uni_bt_le.c` when link encryption completes on a Switch 2 connection.
 *
 * Marks `paired_from_bond = !needs_pair` and either starts GATT discovery (if still in
 * `SW2_STATE_IDLE` on a bonded reconnect) or retries the command-response CCCD write
 * (if encryption was requested on-demand during `SW2_STATE_ENABLE_CMD_NOTIFY`).
 *
 * @param d Target HID device instance.
 */
void uni_hid_parser_switch2_on_encrypted(struct uni_hid_device_s* d);

/**
 * @brief BTstack GATT event callback driving the Switch 2 discovery, calibration, pairing,
 *        initialization, and input notification state machine.
 *
 * @param packet_type HCI packet type (`HCI_EVENT_PACKET`).
 * @param channel     BTstack channel (unused).
 * @param packet      Raw GATT event packet buffer.
 * @param size        Packet buffer size in bytes.
 */
void uni_hid_parser_switch2_handle_gatt_event(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_SWITCH2_H
