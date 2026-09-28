// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file posix_imgui_platform.cpp
 * @brief Implementation of the Bluepad32 POSIX Dear ImGui custom platform and cross-thread bridge.
 *
 * Threading & Synchronization Architecture:
 *   - All `uni_platform` callbacks (`posix_imgui_init`, `posix_imgui_on_init_complete`,
 *     `posix_imgui_on_device_connected`, `posix_imgui_on_device_ready`,
 *     `posix_imgui_on_device_disconnected`, `posix_imgui_on_controller_data`) and
 *     `process_pending_commands()` execute exclusively on Thread 2 (the BTstack run-loop thread).
 *   - `posix_imgui_get_snapshots()` and `posix_imgui_request_*()` are called from Thread 1
 *     (the GLFW/ImGui UI render thread).
 *
 * Lock-Ordering Invariant (Deadlock Prevention):
 *   - `g_state_mutex` guards `g_devices[]`, `g_snapshots[]`, and `g_most_recent_connected_slot`.
 *   - `g_cmd_mutex` guards `g_cmd_queue` and `g_cmd_wakeup_pending`.
 *   - `g_cmd_mutex` and `g_state_mutex` are NEVER held simultaneously. In
 *     `process_pending_commands()`, `g_cmd_queue` is swapped into a thread-local vector
 *     and `g_cmd_mutex` is unlocked BEFORE acquiring `g_state_mutex` or invoking any
 *     Bluepad32 / BTstack APIs.
 */

#include "posix_imgui_platform.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <vector>

// BTstack and Bluepad32 C headers must be wrapped inside `extern "C"` after all C++
// standard library headers so transitive headers without their own `extern "C"` guards
// link with C linkage without trapping C++ standard headers.
extern "C" {
#include <btstack.h>
#include <uni.h>
}

namespace {

enum CommandType {
    CMD_RUMBLE = 0,
    CMD_PLAYER_LEDS,
    CMD_LIGHTBAR_COLOR,
    CMD_SHUTDOWN,
};

struct ControllerCommand {
    CommandType type;
    int slot;
    uint16_t start_delay_ms;
    uint16_t duration_ms;
    uint8_t weak_magnitude;
    uint8_t strong_magnitude;
    uint8_t leds_bitmask;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

int g_enhanced_mode = 0;
int g_delete_keys = 0;

// Guarded by `g_state_mutex`.
std::mutex g_state_mutex;
uni_hid_device_t* g_devices[kMaxControllers] = {nullptr};
ControllerSnapshot g_snapshots[kMaxControllers] = {};
int g_most_recent_connected_slot = -1;

// Guarded by `g_cmd_mutex`.
std::mutex g_cmd_mutex;
std::vector<ControllerCommand> g_cmd_queue;
bool g_cmd_wakeup_pending = false;

// MUST have static storage duration because `btstack_run_loop_execute_on_main_thread()`
// links this struct directly into BTstack's intrusive callback list (`btstack_linked_list_t`).
btstack_context_callback_registration_t g_cmd_callback = {};

std::atomic<bool> g_shutdown_requested{false};

// Maps a Bluepad32 controller model to its physical face-button layout family.
ControllerLayoutType classify_controller_layout(uni_controller_type_t type) {
    switch (type) {
        case CONTROLLER_TYPE_PS3Controller:
        case CONTROLLER_TYPE_PS4Controller:
        case CONTROLLER_TYPE_PS5Controller:
        case CONTROLLER_TYPE_PSMoveController:
        case CONTROLLER_TYPE_XInputPS4Controller:
            return CONTROLLER_LAYOUT_SHAPES;

        case CONTROLLER_TYPE_SwitchProController:
        case CONTROLLER_TYPE_SwitchJoyConLeft:
        case CONTROLLER_TYPE_SwitchJoyConRight:
        case CONTROLLER_TYPE_SwitchJoyConPair:
        case CONTROLLER_TYPE_SwitchInputOnlyController:
        case CONTROLLER_TYPE_XInputSwitchController:
            return CONTROLLER_LAYOUT_REVERSE;

        default:
            return CONTROLLER_LAYOUT_STANDARD;
    }
}

// Returns true if the controller family reports 6-axis IMU (accelerometer/gyroscope) data.
bool has_imu_support(uni_controller_type_t type) {
    switch (type) {
        case CONTROLLER_TYPE_PS3Controller:
        case CONTROLLER_TYPE_PS4Controller:
        case CONTROLLER_TYPE_PS5Controller:
        case CONTROLLER_TYPE_PSMoveController:
        case CONTROLLER_TYPE_SwitchProController:
        case CONTROLLER_TYPE_SwitchJoyConLeft:
        case CONTROLLER_TYPE_SwitchJoyConRight:
        case CONTROLLER_TYPE_SwitchJoyConPair:
        case CONTROLLER_TYPE_WiiController:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Drains and executes all pending UI-to-BTstack commands on the BTstack thread (Thread 2).
 *
 * Woken via POSIX pipe by `btstack_run_loop_execute_on_main_thread(&g_cmd_callback)`.
 */
void process_pending_commands(void* context) {
    ARG_UNUSED(context);

    // Swap queued commands into a local vector under `g_cmd_mutex` and immediately release
    // `g_cmd_mutex` before acquiring `g_state_mutex` or calling into BTstack/Bluepad32.
    std::vector<ControllerCommand> local_cmds;
    {
        std::lock_guard<std::mutex> lock(g_cmd_mutex);
        local_cmds.swap(g_cmd_queue);
        g_cmd_wakeup_pending = false;
    }

    for (const ControllerCommand& cmd : local_cmds) {
        if (cmd.type == CMD_SHUTDOWN) {
            g_shutdown_requested.store(true);
            // Why: If no USB Bluetooth dongle is attached, BTstack remains in
            // `HCI_STATE_INITIALIZING` polling libusb timers and never transitions
            // through `HCI_STATE_HALTING -> HCI_STATE_OFF` on its own. Triggering
            // run-loop exit directly when in `HCI_STATE_OFF` or `HCI_STATE_INITIALIZING`
            // guarantees `bt_thread.join()` never hangs on window close.
            HCI_STATE state = hci_get_state();
            if (state == HCI_STATE_OFF || state == HCI_STATE_INITIALIZING) {
                if (state == HCI_STATE_INITIALIZING) {
                    hci_power_control(HCI_POWER_OFF);
                }
                btstack_run_loop_trigger_exit();
            } else {
                hci_power_control(HCI_POWER_OFF);
            }
            continue;
        }

        if (cmd.slot < 0 || cmd.slot >= kMaxControllers) {
            continue;
        }

        uni_hid_device_t* d = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            d = g_devices[cmd.slot];
        }

        // Dangling-Pointer Guard: Both `posix_imgui_on_device_disconnected()` and
        // `process_pending_commands()` run on the single BTstack thread. Checking
        // `d != nullptr` and `UNI_BT_CONN_STATE_DEVICE_READY` ensures that if a device
        // disconnected while a UI command was in flight in `g_cmd_queue`, the stale
        // command is safely discarded.
        if (d == nullptr || uni_bt_conn_get_state(&d->conn) != UNI_BT_CONN_STATE_DEVICE_READY) {
            continue;
        }

        switch (cmd.type) {
            case CMD_RUMBLE:
                if (d->report_parser.play_dual_rumble != nullptr) {
                    d->report_parser.play_dual_rumble(d, cmd.start_delay_ms, cmd.duration_ms, cmd.weak_magnitude,
                                                      cmd.strong_magnitude);
                }
                break;
            case CMD_PLAYER_LEDS:
                if (d->report_parser.set_player_leds != nullptr) {
                    d->report_parser.set_player_leds(d, cmd.leds_bitmask);
                }
                break;
            case CMD_LIGHTBAR_COLOR:
                if (d->report_parser.set_lightbar_color != nullptr) {
                    d->report_parser.set_lightbar_color(d, cmd.r, cmd.g, cmd.b);
                }
                break;
            case CMD_SHUTDOWN:
                break;
        }
    }
}

/**
 * @brief Appends a command to `g_cmd_queue` and signals the BTstack run loop pipe if not already pending.
 *
 * Deduplicating wakeups via `g_cmd_wakeup_pending` prevents corrupting BTstack's
 * intrusive linked list (`g_cmd_callback`) or flooding the POSIX wakeup pipe when
 * the user drags an RGB color picker at 60 Hz.
 */
void enqueue_command(const ControllerCommand& cmd) {
    bool should_wake = false;
    {
        std::lock_guard<std::mutex> lock(g_cmd_mutex);
        g_cmd_queue.push_back(cmd);
        if (!g_cmd_wakeup_pending) {
            g_cmd_wakeup_pending = true;
            g_cmd_callback.callback = &process_pending_commands;
            g_cmd_callback.context = nullptr;
            should_wake = true;
        }
    }
    if (should_wake) {
        btstack_run_loop_execute_on_main_thread(&g_cmd_callback);
    }
}

// ============================================================================
// uni_platform Callbacks (Executed on Thread 2: BTstack Run-Loop Thread)
// ============================================================================
void posix_imgui_init(int argc, const char** argv) {
    logi("posix_imgui: init()\n");
    bool ble_enabled = true;

    for (int i = 1; i < argc && argv != nullptr; i++) {
        if (std::strcmp(argv[i], "--enhanced") == 0 || std::strcmp(argv[i], "-e") == 0) {
            g_enhanced_mode = 1;
            logi("Enhanced mode enabled\n");
        }
        if (std::strcmp(argv[i], "--delete") == 0 || std::strcmp(argv[i], "-d") == 0) {
            g_delete_keys = 1;
            logi("Stored keys will be deleted\n");
        }
        if ((std::strcmp(argv[i], "--ble") == 0 || std::strcmp(argv[i], "-b") == 0) && i + 1 < argc) {
            ble_enabled = std::atoi(argv[++i]) != 0;
        }
    }

    uni_bt_le_set_enabled(ble_enabled);
    logi("BLE enabled: %d\n", ble_enabled);
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
}

void posix_imgui_on_init_complete(void) {
    logi("posix_imgui: on_init_complete()\n");

    if (g_delete_keys) {
        uni_bt_del_keys_unsafe();
    } else {
        uni_bt_list_keys_unsafe();
    }

    uni_property_dump_all();

    uni_bt_start_scanning_and_autoconnect_unsafe();
    uni_bt_allow_incoming_connections(true);
}

uni_error_t posix_imgui_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)name;
    (void)cod;
    (void)rssi;
    return UNI_ERROR_SUCCESS;
}

void posix_imgui_on_device_connected(uni_hid_device_t* d) {
    logi("posix_imgui: device connected: %p\n", static_cast<void*>(d));
    if (d == nullptr) {
        return;
    }
    // Why: `uni_hid_device_init(d)` zero-initializes `d` (`memset(d, 0, sizeof(*d))`),
    // leaving all bytes of `d->platform_data` at `0`. If this device disconnects before
    // reaching `on_device_ready(d)` (e.g., L2CAP/SDP timeout) or is rejected as a 5th
    // controller (`UNI_ERROR_NO_SLOTS`), an uninitialized `ins->slot == 0` would falsely
    // match Slot 0 (Controller #1) during `on_device_disconnected(d)`.
    posix_imgui_instance_t* ins = get_posix_imgui_instance(d);
    ins->slot = -1;
    ins->gamepad_seat = GAMEPAD_SEAT_NONE;
}

void posix_imgui_on_device_disconnected(uni_hid_device_t* d) {
    logi("posix_imgui: device disconnected: %p\n", static_cast<void*>(d));
    if (d == nullptr) {
        return;
    }
    posix_imgui_instance_t* ins = get_posix_imgui_instance(d);
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        // Verify BOTH valid slot bounds AND pointer identity (`g_devices[ins->slot] == d`)
        // before reclaiming the slot so a pre-ready or rejected device never clobbers an
        // active controller's slot.
        if (ins->slot >= 0 && ins->slot < kMaxControllers && g_devices[ins->slot] == d) {
            int slot = ins->slot;
            g_devices[slot] = nullptr;
            std::memset(&g_snapshots[slot], 0, sizeof(g_snapshots[slot]));
            g_snapshots[slot].connected = false;
        }
        ins->slot = -1;
        ins->gamepad_seat = GAMEPAD_SEAT_NONE;
    }
}

uni_error_t posix_imgui_on_device_ready(uni_hid_device_t* d) {
    logi("posix_imgui: device ready: %p\n", static_cast<void*>(d));
    if (d == nullptr) {
        return UNI_ERROR_INVALID_DEVICE;
    }

    posix_imgui_instance_t* ins = get_posix_imgui_instance(d);
    int assigned_slot = -1;

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        for (int i = 0; i < kMaxControllers; ++i) {
            if (g_devices[i] == nullptr) {
                assigned_slot = i;
                break;
            }
        }

        if (assigned_slot < 0) {
            ins->slot = -1;
            ins->gamepad_seat = GAMEPAD_SEAT_NONE;
            logi("posix_imgui: rejecting device %p (all %d slots occupied)\n", static_cast<void*>(d), kMaxControllers);
            return UNI_ERROR_NO_SLOTS;
        }

        ins->slot = assigned_slot;
        ins->gamepad_seat = static_cast<uni_gamepad_seat_t>(BIT(assigned_slot));
        g_devices[assigned_slot] = d;

        ControllerSnapshot& snap = g_snapshots[assigned_slot];
        std::memset(&snap, 0, sizeof(snap));
        snap.connected = true;
        snap.vendor_id = d->vendor_id;
        snap.product_id = d->product_id;
        snap.controller_type = d->controller_type;
        snap.controller_subtype = d->controller_subtype;
        std::snprintf(snap.name, sizeof(snap.name), "%s", d->name);

        const char* model = uni_gamepad_get_model_name(d->controller_type);
        std::snprintf(snap.model_name, sizeof(snap.model_name), "%s", model ? model : "Unknown");

        std::memcpy(snap.btaddr, d->conn.btaddr, sizeof(bd_addr_t));
        snap.rssi = d->conn.rssi;

        snap.layout = classify_controller_layout(d->controller_type);
        snap.has_rumble = (d->report_parser.play_dual_rumble != nullptr);
        snap.has_player_leds = (d->report_parser.set_player_leds != nullptr);
        snap.has_rgb_led = (d->report_parser.set_lightbar_color != nullptr);
        snap.has_brightness_led = (d->controller_type == CONTROLLER_TYPE_SwitchProController ||
                                   d->controller_type == CONTROLLER_TYPE_SwitchJoyConRight);
        snap.has_imu = has_imu_support(d->controller_type);

        snap.controller = d->controller;
        snap.last_report_timestamp_us = 0;
        snap.report_delta_ms = 0;

        g_most_recent_connected_slot = assigned_slot;
    }

    if (d->report_parser.set_player_leds != nullptr) {
        d->report_parser.set_player_leds(d, static_cast<uint8_t>(BIT(assigned_slot)));
    }

    return UNI_ERROR_SUCCESS;
}

void posix_imgui_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    if (d == nullptr || ctl == nullptr) {
        return;
    }

    posix_imgui_instance_t* ins = get_posix_imgui_instance(d);

    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_us = static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;

    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (ins->slot >= 0 && ins->slot < kMaxControllers && g_devices[ins->slot] == d) {
        ControllerSnapshot& snap = g_snapshots[ins->slot];
        if (snap.last_report_timestamp_us != 0 && now_us >= snap.last_report_timestamp_us) {
            snap.report_delta_ms = static_cast<uint32_t>((now_us - snap.last_report_timestamp_us) / 1000ULL);
        }
        snap.last_report_timestamp_us = now_us;
        snap.controller = *ctl;
        snap.rssi = d->conn.rssi;
        snap.controller_subtype = d->controller_subtype;
    }
}

const uni_property_t* posix_imgui_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return nullptr;
}

void posix_imgui_on_oob_event(uni_platform_oob_event_t event, void* data) {
    switch (event) {
        case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON:
            logi("posix_imgui_on_oob_event: system button pressed on device %p\n", data);
            break;
        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
            logi("posix_imgui_on_oob_event: Bluetooth scanning enabled: %d\n", data != nullptr ? 1 : 0);
            break;
        default:
            logi("posix_imgui_on_oob_event: unsupported event: 0x%04x\n", event);
            break;
    }
}

}  // namespace

posix_imgui_instance_t* get_posix_imgui_instance(uni_hid_device_t* d) {
    return reinterpret_cast<posix_imgui_instance_t*>(&d->platform_data[0]);
}

struct uni_platform* get_posix_imgui_platform(void) {
    static struct uni_platform plat = {
        .name = "Posix ImGui",
        .init = posix_imgui_init,
        .on_init_complete = posix_imgui_on_init_complete,
        .on_device_discovered = posix_imgui_on_device_discovered,
        .on_device_connected = posix_imgui_on_device_connected,
        .on_device_disconnected = posix_imgui_on_device_disconnected,
        .on_device_ready = posix_imgui_on_device_ready,
        .on_gamepad_data = nullptr,
        .on_controller_data = posix_imgui_on_controller_data,
        .get_property = posix_imgui_get_property,
        .on_oob_event = posix_imgui_on_oob_event,
        .device_dump = nullptr,
        .register_console_cmds = nullptr,
    };
    return &plat;
}

void posix_imgui_get_snapshots(ControllerSnapshot out_snapshots[kMaxControllers], int* newly_connected_slot) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (out_snapshots != nullptr) {
        std::memcpy(out_snapshots, g_snapshots, sizeof(g_snapshots));
    }
    if (newly_connected_slot != nullptr) {
        *newly_connected_slot = g_most_recent_connected_slot;
        g_most_recent_connected_slot = -1;
    }
}

void posix_imgui_request_rumble(int slot,
                                uint16_t start_delay_ms,
                                uint16_t duration_ms,
                                uint8_t weak_magnitude,
                                uint8_t strong_magnitude) {
    ControllerCommand cmd{};
    cmd.type = CMD_RUMBLE;
    cmd.slot = slot;
    cmd.start_delay_ms = start_delay_ms;
    cmd.duration_ms = duration_ms;
    cmd.weak_magnitude = weak_magnitude;
    cmd.strong_magnitude = strong_magnitude;
    enqueue_command(cmd);
}

void posix_imgui_request_player_leds(int slot, uint8_t leds_bitmask) {
    ControllerCommand cmd{};
    cmd.type = CMD_PLAYER_LEDS;
    cmd.slot = slot;
    cmd.leds_bitmask = static_cast<uint8_t>(leds_bitmask & 0x0f);
    enqueue_command(cmd);
}

void posix_imgui_request_lightbar_color(int slot, uint8_t r, uint8_t g, uint8_t b) {
    ControllerCommand cmd{};
    cmd.type = CMD_LIGHTBAR_COLOR;
    cmd.slot = slot;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    enqueue_command(cmd);
}

void posix_imgui_request_shutdown(void) {
    g_shutdown_requested.store(true);
    ControllerCommand cmd{};
    cmd.type = CMD_SHUTDOWN;
    cmd.slot = -1;
    enqueue_command(cmd);
}

bool posix_imgui_is_shutdown_requested(void) {
    return g_shutdown_requested.load();
}

void posix_imgui_reset_for_test(void) {
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        for (int i = 0; i < kMaxControllers; ++i) {
            g_devices[i] = nullptr;
        }
        std::memset(g_snapshots, 0, sizeof(g_snapshots));
        g_most_recent_connected_slot = -1;
    }
    {
        std::lock_guard<std::mutex> lock(g_cmd_mutex);
        g_cmd_queue.clear();
        g_cmd_wakeup_pending = false;
    }
    btstack_run_loop_base_execute_callbacks();
    g_shutdown_requested.store(false);
}

void posix_imgui_process_pending_commands(void) {
    process_pending_commands(nullptr);
}
