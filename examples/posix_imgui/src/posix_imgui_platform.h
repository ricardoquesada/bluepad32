// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file posix_imgui_platform.h
 * @brief Thread-safe Bluepad32 custom platform and cross-thread synchronization bridge.
 *
 * Architectural Role & Threading Model:
 *   Bluepad32 and BTstack are strictly single-threaded and execute on a dedicated
 *   background worker thread (`Thread 2`: `btstack_run_loop_execute()`), whereas
 *   GLFW, OpenGL3, and Dear ImGui must execute on the main thread (`Thread 1`).
 *
 *   This header defines the lock-protected telemetry snapshot table (Thread 2 -> Thread 1)
 *   and the asynchronous, pipe-woken command queue API (Thread 1 -> Thread 2) that
 *   decouple 60 Hz UI rendering from sub-millisecond Bluetooth packet processing:
 *
 *   - Telemetry Path (BTstack Thread -> UI Thread):
 *     `uni_platform` callbacks (`on_device_ready`, `on_controller_data`,
 *     `on_device_disconnected`) update an internal `ControllerSnapshot` table under
 *     `g_state_mutex`. The UI thread copies all 4 slots atomically once per frame via
 *     `posix_imgui_get_snapshots()`.
 *
 *   - Command Path (UI Thread -> BTstack Thread):
 *     UI button handlers invoke `posix_imgui_request_*()`, which append a command
 *     under `g_cmd_mutex` and wake the BTstack POSIX `select()` loop via
 *     `btstack_run_loop_execute_on_main_thread()`.
 */

#pragma once

// Standard C/C++ headers MUST be included outside and before `extern "C"` so C++
// standard library declarations are never trapped inside C linkage.
#include <stdbool.h>
#include <cstdint>

extern "C" {
#include <uni.h>
}

/// Maximum number of concurrent Bluetooth controllers supported (4 seats: #1..#4).
constexpr int kMaxControllers = CONFIG_BLUEPAD32_MAX_DEVICES;  // 4

/**
 * @brief Visual face-button layout classification for the 2D gamepad canvas.
 *
 * Determines which face-button textures are enabled and how physical button bitmasks
 * map to the North/South/East/West quad positions in `ControllerUIData::ConfigureButtonLayout()`.
 */
enum ControllerLayoutType {
    /// Xbox / Generic layout: A (South), B (East), X (West), Y (North).
    CONTROLLER_LAYOUT_STANDARD = 0,
    /// PlayStation layout: Cross (South), Circle (East), Square (West), Triangle (North).
    CONTROLLER_LAYOUT_SHAPES,
    /// Nintendo Switch layout: B (South), A (East), Y (West), X (North).
    CONTROLLER_LAYOUT_REVERSE,
};

/**
 * @brief Per-device platform instance data stored inside `uni_hid_device_t::platform_data`.
 *
 * Note: `uni_hid_device_init()` zero-initializes the entire `uni_hid_device_t` struct via
 * `memset`. To prevent a pre-ready disconnect (or a rejected 5th controller) from falsely
 * matching Slot 0, `on_device_connected()` explicitly initializes `slot = -1` and
 * `gamepad_seat = GAMEPAD_SEAT_NONE`.
 */
struct posix_imgui_instance_t {
    int slot;  ///< Assigned controller slot in `[0, kMaxControllers - 1]`, or `-1` if unassigned.
    uni_gamepad_seat_t gamepad_seat;  ///< Assigned Bluepad32 seat mask (`GAMEPAD_SEAT_A`..`D`), or `GAMEPAD_SEAT_NONE`.
};

/**
 * @brief Point-in-time snapshot of a single controller slot's metadata, capabilities, and input state.
 *
 * Copied by value from the BTstack thread's state table into the Dear ImGui UI thread
 * once per frame via `posix_imgui_get_snapshots()`.
 */
struct ControllerSnapshot {
    bool connected;                               ///< True if a ready controller currently occupies this slot.
    uint16_t vendor_id;                           ///< USB/Bluetooth Vendor ID (e.g., `0x054c` for Sony).
    uint16_t product_id;                          ///< USB/Bluetooth Product ID (e.g., `0x0ce6` for DualSense).
    uni_controller_type_t controller_type;        ///< Bluepad32 controller model identifier.
    uni_controller_subtype_t controller_subtype;  ///< Bluepad32 controller subtype (e.g., Wiimote extension type).
    char name[HID_MAX_NAME_LEN];                  ///< Bluetooth device name reported during discovery/SDP.
    char model_name[64];                          ///< Human-readable model string from `uni_gamepad_get_model_name()`.
    bd_addr_t btaddr;                             ///< 6-byte Bluetooth MAC address of the controller.
    uint8_t rssi;                                 ///< Latest Bluetooth RSSI reading (`int8_t` cast to `uint8_t`).

    // Capabilities populated in on_device_ready()
    ControllerLayoutType layout;  ///< Face button layout family (`STANDARD`, `SHAPES`, or `REVERSE`).
    bool has_rumble;              ///< True if `d->report_parser.play_dual_rumble != nullptr`.
    bool has_player_leds;         ///< True if `d->report_parser.set_player_leds != nullptr`.
    bool has_rgb_led;             ///< True if `d->report_parser.set_lightbar_color != nullptr`.
    bool has_brightness_led;      ///< True for Switch Pro Controller and Switch Joy-Con Right.
    bool has_imu;                 ///< True if the controller family provides 6-axis IMU telemetry.

    // Live telemetry updated in on_controller_data()
    uni_controller_t controller;        ///< Latest parsed Bluepad32 controller state (buttons, axes, IMU, battery).
    uint64_t last_report_timestamp_us;  ///< Monotonic timestamp (`CLOCK_MONOTONIC`, microseconds) of latest report.
    uint32_t report_delta_ms;           ///< Elapsed milliseconds between the two most recent input reports.
};

/**
 * @brief Returns the singleton custom `uni_platform` descriptor to register via
 *        `uni_platform_set_custom()` before calling `uni_init()`.
 *
 * @return Non-null pointer to the static `uni_platform` struct.
 */
struct uni_platform* get_posix_imgui_platform(void);

/**
 * @brief Retrieves the `posix_imgui_instance_t` embedded in `d->platform_data`.
 *
 * @param d Pointer to a Bluepad32 `uni_hid_device_t` instance (must not be null).
 * @return Pointer to the per-device `posix_imgui_instance_t`.
 */
posix_imgui_instance_t* get_posix_imgui_instance(uni_hid_device_t* d);

/**
 * @brief Thread-safe snapshot reader called by the Dear ImGui UI thread (Thread 1) each frame.
 *
 * Copies all `kMaxControllers` slots under `g_state_mutex`. If a controller completed
 * `on_device_ready()` since the previous call to `posix_imgui_get_snapshots()`, writes its
 * slot index (`0..3`) to `*newly_connected_slot` and resets the internal latch to `-1`
 * so the UI tab bar can auto-select the newly connected controller for one frame.
 *
 * Example:
 * @code
 * ControllerSnapshot snapshots[kMaxControllers];
 * int new_slot = -1;
 * posix_imgui_get_snapshots(snapshots, &new_slot);
 * // If new_slot == 2, Controller #3 just connected prior to this frame.
 * @endcode
 *
 * @param[out] out_snapshots        Destination array of size `kMaxControllers` (may be null).
 * @param[out] newly_connected_slot Receives the newly connected slot (`0..3`) or `-1` (may be null).
 */
void posix_imgui_get_snapshots(ControllerSnapshot out_snapshots[kMaxControllers], int* newly_connected_slot);

/**
 * @brief Enqueues an asynchronous dual-motor rumble command from the UI thread (Thread 1).
 *
 * Thread-safe and non-blocking. Appends a `CMD_RUMBLE` entry under `g_cmd_mutex` and
 * wakes the BTstack run loop via `btstack_run_loop_execute_on_main_thread()`. Pass
 * `duration_ms = 0`, `weak_magnitude = 0`, `strong_magnitude = 0` to stop active vibration.
 *
 * @param slot             Controller slot index in `[0, kMaxControllers - 1]`.
 * @param start_delay_ms   Delay before starting vibration in milliseconds (`0..1000`).
 * @param duration_ms      Vibration duration in milliseconds (`0..2000`).
 * @param weak_magnitude   High-frequency (weak) motor intensity (`0..255`).
 * @param strong_magnitude Low-frequency (strong) motor intensity (`0..255`).
 */
void posix_imgui_request_rumble(int slot,
                                uint16_t start_delay_ms,
                                uint16_t duration_ms,
                                uint8_t weak_magnitude,
                                uint8_t strong_magnitude);

/**
 * @brief Enqueues an asynchronous Player Indicator LED bitmask update from the UI thread (Thread 1).
 *
 * Thread-safe and non-blocking. Only the lower 4 bits (`leds_bitmask & 0x0f`) are forwarded
 * to `d->report_parser.set_player_leds(d, mask)` on the BTstack thread.
 *
 * @param slot         Controller slot index in `[0, kMaxControllers - 1]`.
 * @param leds_bitmask 4-bit LED bitmask (e.g., `0x01` for LED 1, `0x0f` for all 4 LEDs).
 */
void posix_imgui_request_player_leds(int slot, uint8_t leds_bitmask);

/**
 * @brief Enqueues an asynchronous RGB lightbar color update from the UI thread (Thread 1).
 *
 * Thread-safe and non-blocking. Forwarded to `d->report_parser.set_lightbar_color(d, r, g, b)`
 * on the BTstack thread for supporting controllers (DualShock 4, DualSense, PS Move).
 *
 * @param slot Controller slot index in `[0, kMaxControllers - 1]`.
 * @param r    Red channel intensity (`0..255`).
 * @param g    Green channel intensity (`0..255`).
 * @param b    Blue channel intensity (`0..255`).
 */
void posix_imgui_request_lightbar_color(int slot, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Requests a clean asynchronous shutdown of the BTstack run loop from any thread.
 *
 * Sets an atomic shutdown flag and enqueues a `CMD_SHUTDOWN` command on the BTstack thread
 * that powers down HCI (`hci_power_control(HCI_POWER_OFF)`) or exits the run loop immediately
 * if HCI is not active.
 */
void posix_imgui_request_shutdown(void);

/**
 * @brief Returns true if `posix_imgui_request_shutdown()` has been invoked.
 *
 * Thread-safe (reads an `std::atomic<bool>`).
 */
bool posix_imgui_is_shutdown_requested(void);

/**
 * @brief Resets all internal slot tables, snapshots, and command queues for headless unit tests.
 *
 * Must only be called from `test_posix_imgui` between test cases when no concurrent
 * BTstack worker thread is running.
 */
void posix_imgui_reset_for_test(void);

/**
 * @brief Synchronously drains queued `ControllerCommand` entries on the calling thread for tests.
 */
void posix_imgui_process_pending_commands(void);
