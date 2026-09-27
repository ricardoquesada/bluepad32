// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_RUMBLE_H
#define UNI_HID_PARSER_RUMBLE_H

/**
 * @file uni_hid_parser_rumble.h
 * @brief Shared haptic rumble timer and state machine for HID controller parsers.
 *
 * Centralizes delayed start (`timer_delayed_start`), duration expiration (`timer_duration`),
 * 4-actuator magnitude storage (`weak_magnitude`, `strong_magnitude`, `trigger_left`,
 * `trigger_right`), and BLE GATT retry backoff (`UNI_RUMBLE_RETRY_BLE`) across all 8 rumble-capable
 * parsers (Wii, Switch, DS3, DS4, DS5, PSMove, Stadia, Xbox One).
 *
 * `uni_rumble_t` is embedded directly on `uni_hid_device_t` (outside `parser_data[]`) so that
 * parser `setup()` calls that zero `parser_data` via `memset(ins, 0, sizeof(*ins))` cannot
 * corrupt active BTstack intrusive timer list nodes.
 */

#include <btstack.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Delay in milliseconds before retrying a busy BLE GATT rumble write. */
#define UNI_RUMBLE_BLE_RETRY_MS 50

// Forward declarations
struct uni_hid_device_s;

/**
 * @brief Lifecycle state of the per-device rumble state machine.
 */
typedef enum {
    UNI_RUMBLE_STATE_DISABLED = 0, /**< No rumble active or scheduled. */
    UNI_RUMBLE_STATE_DELAYED,      /**< Waiting on `timer_delayed_start` (delayed start or BLE start retry). */
    UNI_RUMBLE_STATE_IN_PROGRESS,  /**< Rumble active on controller; waiting on `timer_duration`. */
} uni_rumble_state_t;

/**
 * @brief Return code from parser-specific rumble start/stop callbacks.
 */
typedef enum {
    UNI_RUMBLE_OK = 0,    /**< Output report sent (or queued on L2CAP) successfully. */
    UNI_RUMBLE_RETRY_BLE, /**< BLE GATT client busy (`ERROR_CODE_COMMAND_DISALLOWED`); retry in 50ms. */
    UNI_RUMBLE_ERR,       /**< Unrecoverable error; transition state machine to `UNI_RUMBLE_STATE_DISABLED`. */
} uni_rumble_result_t;

/**
 * @brief Parser callback invoked to send a hardware-specific rumble start report.
 *
 * Dual-motor controllers use `weak_magnitude` and `strong_magnitude` (ignoring trigger parameters).
 * Single-motor controllers typically take the maximum of `weak_magnitude` and `strong_magnitude`.
 * Quad-actuator controllers (such as Xbox One) use all four magnitude parameters.
 *
 * @param d                Target HID device.
 * @param weak_magnitude   Right / high-frequency motor magnitude (0..255).
 * @param strong_magnitude Left / low-frequency motor magnitude (0..255).
 * @param trigger_left     Left impulse trigger motor magnitude (0..255).
 * @param trigger_right    Right impulse trigger motor magnitude (0..255).
 * @return `UNI_RUMBLE_OK` on success, `UNI_RUMBLE_RETRY_BLE` if the BLE GATT client is busy,
 *         or `UNI_RUMBLE_ERR` on failure.
 */
typedef uni_rumble_result_t (*uni_rumble_start_fn_t)(struct uni_hid_device_s* d,
                                                     uint8_t weak_magnitude,
                                                     uint8_t strong_magnitude,
                                                     uint8_t trigger_left,
                                                     uint8_t trigger_right);

/**
 * @brief Parser callback invoked to send a hardware-specific rumble stop report.
 *
 * @param d Target HID device.
 * @return `UNI_RUMBLE_OK` on success, `UNI_RUMBLE_RETRY_BLE` if the BLE GATT client is busy,
 *         or `UNI_RUMBLE_ERR` on failure.
 */
typedef uni_rumble_result_t (*uni_rumble_stop_fn_t)(struct uni_hid_device_s* d);

/**
 * @brief Per-device rumble state and BTstack timer nodes.
 *
 * Embedded directly in `uni_hid_device_t` so its lifecycle is managed by the device
 * and rumble helpers rather than individual parser instance memory.
 */
typedef struct {
    btstack_timer_source_t timer_duration;      /**< Timer tracking active rumble duration or BLE stop retry. */
    btstack_timer_source_t timer_delayed_start; /**< Timer tracking delayed start or BLE start retry. */
    uni_rumble_state_t state;                   /**< Current rumble state machine state. */
    uint16_t duration_ms;                       /**< Requested rumble duration in milliseconds. */
    uint8_t weak_magnitude;                     /**< Cached weak motor magnitude for delayed start / BLE retry. */
    uint8_t strong_magnitude;                   /**< Cached strong motor magnitude for delayed start / BLE retry. */
    uint8_t trigger_left;                       /**< Cached left trigger magnitude for delayed start / BLE retry. */
    uint8_t trigger_right;                      /**< Cached right trigger magnitude for delayed start / BLE retry. */
    uni_rumble_start_fn_t start_fn;             /**< Parser callback to start rumble on the device. */
    uni_rumble_stop_fn_t stop_fn;               /**< Parser callback to stop rumble on the device. */
} uni_rumble_t;

/**
 * @brief Initialize (or re-initialize) the rumble state machine for a HID device.
 *
 * Safely removes both rumble timers from the BTstack run loop before zeroing `d->rumble`
 * and binding the parser's `start_fn` and `stop_fn` callbacks.
 *
 * @param d        Target HID device (no-op if `NULL`).
 * @param start_fn Parser callback to start rumble, or `NULL` if bound lazily on play.
 * @param stop_fn  Parser callback to stop rumble, or `NULL` if bound lazily on play.
 */
void uni_hid_parser_rumble_init(struct uni_hid_device_s* d,
                                uni_rumble_start_fn_t start_fn,
                                uni_rumble_stop_fn_t stop_fn);

/**
 * @brief Disarm both rumble timers (`timer_delayed_start` and `timer_duration`) and reset state.
 *
 * Must be called before zeroing or deleting a `uni_hid_device_t` so active intrusive timer
 * nodes are unlinked from BTstack's global run-loop timer list first.
 *
 * @param d Target HID device (no-op if `NULL`).
 */
void uni_hid_parser_rumble_stop_timers(struct uni_hid_device_s* d);

/**
 * @brief Check whether rumble is currently active (`UNI_RUMBLE_STATE_IN_PROGRESS`) on a device.
 *
 * Used by parsers whose output reports multiplex LED and rumble state (e.g., Wii LED report bit `0x01`).
 *
 * @param d Target HID device.
 * @return `true` if `d != NULL` and `d->rumble.state == UNI_RUMBLE_STATE_IN_PROGRESS`, `false` otherwise.
 */
bool uni_hid_parser_rumble_is_in_progress(const struct uni_hid_device_s* d);

/**
 * @brief Schedule or immediately play a dual-actuator rumble effect.
 *
 * Delegates to `uni_hid_parser_rumble_play_quad()` with `trigger_left = 0` and `trigger_right = 0`.
 * If `start_delay_ms == 0` and `duration_ms == 0`, cancels any pending delayed start or stops
 * an in-progress rumble.
 *
 * @param d                Target HID device.
 * @param start_delay_ms   Delay before starting rumble in milliseconds (`0` for immediate start).
 * @param duration_ms      Active rumble duration in milliseconds (`0` to cancel/stop rumble).
 * @param weak_magnitude   Weak / high-frequency motor magnitude (0..255).
 * @param strong_magnitude Strong / low-frequency motor magnitude (0..255).
 * @param default_start_fn Fallback start callback bound if `d->rumble.start_fn` is `NULL`.
 * @param default_stop_fn  Fallback stop callback bound if `d->rumble.stop_fn` is `NULL`.
 */
void uni_hid_parser_rumble_play_dual(struct uni_hid_device_s* d,
                                     uint16_t start_delay_ms,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude,
                                     uni_rumble_start_fn_t default_start_fn,
                                     uni_rumble_stop_fn_t default_stop_fn);

/**
 * @brief Schedule or immediately play a 4-actuator rumble effect (dual main motors + impulse triggers).
 *
 * Cancels any previously armed timer for the current state, persists the requested duration and
 * all four actuator magnitudes on `d->rumble` (preserving them across `UNI_RUMBLE_RETRY_BLE`
 * 50ms retries), and either starts/stops immediately (`start_delay_ms == 0`) or arms
 * `timer_delayed_start`.
 *
 * @param d                Target HID device.
 * @param start_delay_ms   Delay before starting rumble in milliseconds (`0` for immediate start).
 * @param duration_ms      Active rumble duration in milliseconds (`0` to cancel/stop rumble).
 * @param weak_magnitude   Weak / high-frequency motor magnitude (0..255).
 * @param strong_magnitude Strong / low-frequency motor magnitude (0..255).
 * @param trigger_left     Left impulse trigger magnitude (0..255).
 * @param trigger_right    Right impulse trigger magnitude (0..255).
 * @param default_start_fn Fallback start callback bound if `d->rumble.start_fn` is `NULL`.
 * @param default_stop_fn  Fallback stop callback bound if `d->rumble.stop_fn` is `NULL`.
 */
void uni_hid_parser_rumble_play_quad(struct uni_hid_device_s* d,
                                     uint16_t start_delay_ms,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude,
                                     uint8_t trigger_left,
                                     uint8_t trigger_right,
                                     uni_rumble_start_fn_t default_start_fn,
                                     uni_rumble_stop_fn_t default_stop_fn);

#ifdef __cplusplus
}
#endif

#endif  // UNI_HID_PARSER_RUMBLE_H
