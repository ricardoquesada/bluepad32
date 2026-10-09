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
 *   - `g_ble_service_mutex` guards `g_ble_service_name` and `g_ble_service_password` (leaf lock;
 *     never held while acquiring `g_cmd_mutex`, `g_state_mutex`, or invoking Bluepad32/BTstack APIs).
 *   - `g_cmd_mutex` and `g_state_mutex` are NEVER held simultaneously. In
 *     `process_pending_commands()`, `g_cmd_queue` is swapped into a thread-local vector
 *     and `g_cmd_mutex` is unlocked BEFORE acquiring `g_state_mutex` or invoking any
 *     Bluepad32 / BTstack APIs.
 */

#include "posix_imgui_platform.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <variant>
#include <vector>

// BTstack and Bluepad32 C headers must be wrapped inside `extern "C"` after all C++
// standard library headers so transitive headers without their own `extern "C"` guards
// link with C linkage without trapping C++ standard headers.
extern "C" {
#include <btstack.h>
#include <uni.h>
}

namespace {

/// Command payload requesting dual-motor force-feedback vibration on `slot`.
struct RumbleCmd {
    int slot;                  ///< Target controller slot index in `[0, kMaxControllers - 1]`.
    uint16_t start_delay_ms;   ///< Delay before starting vibration in milliseconds.
    uint16_t duration_ms;      ///< Vibration duration in milliseconds (`0` stops vibration).
    uint8_t weak_magnitude;    ///< High-frequency (right) motor intensity (`0..255`).
    uint8_t strong_magnitude;  ///< Low-frequency (left) motor intensity (`0..255`).
};

/// Command payload requesting a 4-bit Player Indicator LED bitmask update on `slot`.
struct PlayerLedsCmd {
    int slot;              ///< Target controller slot index in `[0, kMaxControllers - 1]`.
    uint8_t leds_bitmask;  ///< Masked 4-bit LED bitmask (`0x00..0x0f`).
};

/// Command payload requesting an RGB lightbar color update on `slot`.
struct LightbarColorCmd {
    int slot;   ///< Target controller slot index in `[0, kMaxControllers - 1]`.
    uint8_t r;  ///< Red channel intensity (`0..255`).
    uint8_t g;  ///< Green channel intensity (`0..255`).
    uint8_t b;  ///< Blue channel intensity (`0..255`).
};

/// Command payload enabling or disabling virtual child devices (e.g., DS4/DS5 touchpad mouse).
struct SetVirtualDevicesCmd {
    bool enabled;  ///< True to enable virtual child devices; false to disable and disconnect them.
};

/// Command payload updating the bitmask of auto-accepted physical Bluetooth device categories.
struct SetAllowedDeviceTypesCmd {
    uint32_t allowed_types_mask;  ///< Bitwise OR of `PosixImguiDeviceTypeFlags`.
};

/// Command payload enabling or disabling the Bluepad32 BLE configuration service.
struct SetBleServiceEnabledCmd {
    bool enabled;  ///< True to enable the BLE configuration service; false to disable it.
};

/// Command payload updating the advertised Bluepad32 BLE configuration service name.
struct SetBleServiceNameCmd {
    std::array<char, UNI_BT_SERVICE_NAME_MAX_LEN + 1> name;  ///< Clamped NUL-terminated UTF-8 name (`1..29` bytes).
};

/// Command payload updating or clearing the Bluepad32 BLE configuration service password.
struct SetBleServicePasswordCmd {
    std::array<char, UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1>
        password;  ///< Clamped NUL-terminated UTF-8 password (`0..31` bytes).
};

/// Command payload requesting an orderly BTstack HCI power-down and run-loop exit.
struct ShutdownCmd {};

/**
 * @brief Strongly-typed UI-to-BTstack command variant.
 *
 * Why `std::variant` + `std::visit`:
 *   Replaces a C-style `enum CommandType` + fat struct so that unrelated command parameters
 *   cannot be accidentally read, and guarantees at compile time that `process_pending_commands()`
 *   exhaustively handles every command alternative.
 */
using ControllerCommand = std::variant<RumbleCmd,
                                       PlayerLedsCmd,
                                       LightbarColorCmd,
                                       SetVirtualDevicesCmd,
                                       SetAllowedDeviceTypesCmd,
                                       SetBleServiceEnabledCmd,
                                       SetBleServiceNameCmd,
                                       SetBleServicePasswordCmd,
                                       ShutdownCmd>;

/// C++23 overload-set helper for dispatching `std::variant` alternatives via `std::visit`.
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

int g_enhanced_mode = 0;  ///< Non-zero when `-e` / `--enhanced` CLI flag is passed.
int g_delete_keys = 0;    ///< Non-zero when `-d` / `--delete` CLI flag is passed.
/// True after `posix_imgui_init()` runs; false in headless unit tests where `hci_init()` is omitted.
std::atomic<bool> g_hci_initialized{false};

// Guarded by `g_state_mutex`.
std::mutex g_state_mutex;
std::array<uni_hid_device_t*, kMaxControllers> g_devices{};
std::array<ControllerSnapshot, kMaxControllers> g_snapshots{};
int g_most_recent_connected_slot = -1;

// Guarded by `g_cmd_mutex`.
std::mutex g_cmd_mutex;
std::vector<ControllerCommand> g_cmd_queue;
bool g_cmd_wakeup_pending = false;

// MUST have static storage duration because `btstack_run_loop_execute_on_main_thread()`
// links this struct directly into BTstack's intrusive callback list (`btstack_linked_list_t`).
btstack_context_callback_registration_t g_cmd_callback = {};

std::atomic<bool> g_shutdown_requested{false};
std::atomic<bool> g_virtual_devices_enabled{false};
std::atomic<uint32_t> g_allowed_device_types_mask{POSIX_IMGUI_DEVICE_TYPE_DEFAULT};

std::atomic<bool> g_ble_service_enabled{true};
std::mutex g_ble_service_mutex;
std::array<char, UNI_BT_SERVICE_NAME_MAX_LEN + 1> g_ble_service_name{"Bluepad32"};
std::array<char, UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1> g_ble_service_password{""};

/// Normalizes a BLE service name (falling back to `"Bluepad32"` if null/empty and clamping to 29 bytes).
[[nodiscard]] std::array<char, UNI_BT_SERVICE_NAME_MAX_LEN + 1> normalize_ble_service_name(const char* name) {
    std::array<char, UNI_BT_SERVICE_NAME_MAX_LEN + 1> out{};
    const char* effective = (name != nullptr && name[0] != '\0') ? name : CONFIG_BLUEPAD32_BLE_SERVICE_NAME;
    if (effective == nullptr || effective[0] == '\0') {
        effective = "Bluepad32";
    }
    std::strncpy(out.data(), effective, UNI_BT_SERVICE_NAME_MAX_LEN);
    out[UNI_BT_SERVICE_NAME_MAX_LEN] = '\0';
    return out;
}

/// Normalizes a BLE service password (mapping null to `""` and clamping to 31 bytes).
[[nodiscard]] std::array<char, UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1> normalize_ble_service_password(
    const char* password) {
    std::array<char, UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1> out{};
    const char* effective = (password != nullptr) ? password : "";
    std::strncpy(out.data(), effective, UNI_BT_SERVICE_PASSWORD_MAX_LEN);
    out[UNI_BT_SERVICE_PASSWORD_MAX_LEN] = '\0';
    return out;
}

void posix_imgui_on_device_disconnected(uni_hid_device_t* d);

/**
 * @brief Maps a Bluetooth Class of Device (CoD) value to `PosixImguiDeviceTypeFlags`.
 *
 * @param cod 24-bit Bluetooth Class of Device bitfield.
 * @return Bitwise OR of matching `PosixImguiDeviceTypeFlags` (defaults to `GAMEPAD` for non-peripheral CoDs).
 */
[[nodiscard]] uint32_t classify_cod_device_types(uint32_t cod) {
    if ((cod & UNI_BT_COD_MAJOR_MASK) == UNI_BT_COD_MAJOR_PERIPHERAL) {
        const uint32_t minor_cod = cod & UNI_BT_COD_MINOR_MASK;
        uint32_t flags = POSIX_IMGUI_DEVICE_TYPE_NONE;
        if ((minor_cod & (UNI_BT_COD_MINOR_GAMEPAD | UNI_BT_COD_MINOR_JOYSTICK)) != 0) {
            flags |= POSIX_IMGUI_DEVICE_TYPE_GAMEPAD;
        }
        if ((minor_cod & UNI_BT_COD_MINOR_MICE) != 0) {
            flags |= POSIX_IMGUI_DEVICE_TYPE_MOUSE;
        }
        if ((minor_cod & UNI_BT_COD_MINOR_KEYBOARD) != 0) {
            flags |= POSIX_IMGUI_DEVICE_TYPE_KEYBOARD;
        }
        if (flags != POSIX_IMGUI_DEVICE_TYPE_NONE) {
            return flags;
        }
    }
    // Non-peripheral or zero CoD (e.g., Audio/Video remote or synthetic test gamepad):
    return POSIX_IMGUI_DEVICE_TYPE_GAMEPAD;
}

/**
 * @brief Classifies a physical `uni_hid_device_t` into one or more `PosixImguiDeviceTypeFlags`.
 *
 * @param d Pointer to the Bluepad32 HID device (may be null).
 * @return Bitwise OR of `PosixImguiDeviceTypeFlags` represented by `d`.
 */
[[nodiscard]] uint32_t classify_physical_device_types(const uni_hid_device_t* d) {
    if (d == nullptr) {
        return POSIX_IMGUI_DEVICE_TYPE_NONE;
    }

    uint32_t flags = POSIX_IMGUI_DEVICE_TYPE_NONE;
    if (uni_hid_device_is_mouse(d) || d->controller_type == CONTROLLER_TYPE_GenericMouse ||
        d->controller.klass == UNI_CONTROLLER_CLASS_MOUSE) {
        flags |= POSIX_IMGUI_DEVICE_TYPE_MOUSE;
    }
    if (uni_hid_device_is_keyboard(d) || d->controller_type == CONTROLLER_TYPE_GenericKeyboard ||
        d->controller.klass == UNI_CONTROLLER_CLASS_KEYBOARD) {
        flags |= POSIX_IMGUI_DEVICE_TYPE_KEYBOARD;
    }
    if (uni_hid_device_is_gamepad(d)) {
        flags |= POSIX_IMGUI_DEVICE_TYPE_GAMEPAD;
    }
    if (flags != POSIX_IMGUI_DEVICE_TYPE_NONE) {
        return flags;
    }
    return classify_cod_device_types(d->cod);
}

/**
 * @brief Reclaims platform slot(s), disconnects, and deletes a device (and any virtual child)
 *        on the BTstack thread without holding `g_state_mutex`.
 *
 * @param d Pointer to the device to disconnect and delete (may be null).
 */
void disconnect_and_delete_device(uni_hid_device_t* d) {
    if (d == nullptr) {
        return;
    }
    if (d->child != nullptr) {
        posix_imgui_on_device_disconnected(d->child);
    }
    posix_imgui_on_device_disconnected(d);

    // Only invoke BTstack / Bluepad32 pool teardown (`uni_hid_device_disconnect` and
    // `uni_hid_device_delete`) when `d` resides in `uni_hid_device.c`'s device pool.
    // For synthetic stack-allocated `uni_hid_device_t` structs in headless unit tests
    // (where `hci_init()` is not called and `hci_stack` is null), unlink parent/child
    // pointers directly after reclaiming the platform slot(s).
    if (uni_hid_device_get_idx_for_instance(d) >= 0) {
        uni_hid_device_disconnect(d);
        uni_hid_device_delete(d);
    } else {
        if (d->parent != nullptr && d->parent->child == d) {
            d->parent->child = nullptr;
        }
        if (d->child != nullptr) {
            d->child->parent = nullptr;
            d->child = nullptr;
        }
    }
}

/**
 * @brief Maps a Bluepad32 controller model to its physical face-button layout family.
 *
 * @param type Bluepad32 controller model identifier.
 * @return `CONTROLLER_LAYOUT_SHAPES` (PlayStation), `CONTROLLER_LAYOUT_REVERSE` (Switch),
 *         or `CONTROLLER_LAYOUT_STANDARD` (Xbox/Generic).
 */
[[nodiscard]] ControllerLayoutType classify_controller_layout(uni_controller_type_t type) {
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
        case CONTROLLER_TYPE_Switch2ProController:
        case CONTROLLER_TYPE_Switch2JoyConLeft:
        case CONTROLLER_TYPE_Switch2JoyConRight:
            return CONTROLLER_LAYOUT_REVERSE;

        case CONTROLLER_TYPE_SteamController:
        case CONTROLLER_TYPE_SteamControllerV2:
        case CONTROLLER_TYPE_SteamControllerTriton:
        default:
            return CONTROLLER_LAYOUT_STANDARD;
    }
}

/**
 * @brief Returns true if the controller family reports 6-axis IMU (accelerometer/gyroscope) data.
 *
 * @param type Bluepad32 controller model identifier.
 * @return True if 6-axis IMU telemetry is supported; false otherwise.
 */
[[nodiscard]] bool has_imu_support(uni_controller_type_t type) {
    switch (type) {
        case CONTROLLER_TYPE_PS3Controller:
        case CONTROLLER_TYPE_PS4Controller:
        case CONTROLLER_TYPE_PS5Controller:
        case CONTROLLER_TYPE_PSMoveController:
        case CONTROLLER_TYPE_SwitchProController:
        case CONTROLLER_TYPE_SwitchJoyConLeft:
        case CONTROLLER_TYPE_SwitchJoyConRight:
        case CONTROLLER_TYPE_SwitchJoyConPair:
        case CONTROLLER_TYPE_Switch2ProController:
        case CONTROLLER_TYPE_Switch2JoyConLeft:
        case CONTROLLER_TYPE_Switch2JoyConRight:
        case CONTROLLER_TYPE_SteamController:
        case CONTROLLER_TYPE_SteamControllerTriton:
        case CONTROLLER_TYPE_WiiController:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Looks up the active `uni_hid_device_t*` for `slot`, verifying bounds and `DEVICE_READY` state.
 *
 * @param slot Controller slot index in `[0, kMaxControllers - 1]`.
 * @return Pointer to the ready `uni_hid_device_t` in `slot`, or `nullptr` if out-of-bounds or disconnected.
 */
[[nodiscard]] uni_hid_device_t* lookup_ready_device_for_slot(int slot) {
    if (slot < 0 || slot >= kMaxControllers) {
        return nullptr;
    }

    uni_hid_device_t* d = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        d = g_devices[static_cast<size_t>(slot)];
    }

    // Dangling-Pointer Guard: Both `posix_imgui_on_device_disconnected()` and
    // `process_pending_commands()` run on the single BTstack thread. Checking
    // `d != nullptr` and `UNI_BT_CONN_STATE_DEVICE_READY` ensures that if a device
    // disconnected while a UI command was in flight in `g_cmd_queue`, the stale
    // command is safely discarded.
    if (d == nullptr || uni_bt_conn_get_state(&d->conn) != UNI_BT_CONN_STATE_DEVICE_READY) {
        return nullptr;
    }
    return d;
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
        std::visit(Overloaded{
                       [](const RumbleCmd& c) {
                           uni_hid_device_t* d = lookup_ready_device_for_slot(c.slot);
                           if (d != nullptr && d->report_parser.play_dual_rumble != nullptr) {
                               d->report_parser.play_dual_rumble(d, c.start_delay_ms, c.duration_ms, c.weak_magnitude,
                                                                 c.strong_magnitude);
                           }
                       },
                       [](const PlayerLedsCmd& c) {
                           uni_hid_device_t* d = lookup_ready_device_for_slot(c.slot);
                           if (d != nullptr && d->report_parser.set_player_leds != nullptr) {
                               d->report_parser.set_player_leds(d, c.leds_bitmask);
                           }
                       },
                       [](const LightbarColorCmd& c) {
                           uni_hid_device_t* d = lookup_ready_device_for_slot(c.slot);
                           if (d != nullptr && d->report_parser.set_lightbar_color != nullptr) {
                               d->report_parser.set_lightbar_color(d, c.r, c.g, c.b);
                           }
                       },
                       [](const SetVirtualDevicesCmd& c) {
                           g_virtual_devices_enabled.store(c.enabled, std::memory_order_release);
                           uni_virtual_device_set_enabled(c.enabled);

                           if (!c.enabled) {
                               std::vector<uni_hid_device_t*> virtual_to_disconnect;
                               {
                                   std::lock_guard<std::mutex> lock(g_state_mutex);
                                   for (uni_hid_device_t* dev : g_devices) {
                                       if (dev != nullptr && uni_hid_device_is_virtual_device(dev)) {
                                           virtual_to_disconnect.push_back(dev);
                                       }
                                   }
                               }
                               for (uni_hid_device_t* dev : virtual_to_disconnect) {
                                   disconnect_and_delete_device(dev);
                               }
                           } else {
                               // If a DualShock 4 or DualSense is already connected in Bluepad32's device pool
                               // without a virtual child, spawn its virtual touchpad mouse immediately.
                               std::vector<uni_hid_device_t*> parents_needing_virtual;
                               {
                                   std::lock_guard<std::mutex> lock(g_state_mutex);
                                   for (uni_hid_device_t* dev : g_devices) {
                                       if (dev != nullptr && !uni_hid_device_is_virtual_device(dev) &&
                                           dev->child == nullptr && uni_hid_device_get_idx_for_instance(dev) >= 0 &&
                                           (dev->controller_type == CONTROLLER_TYPE_PS4Controller ||
                                            dev->controller_type == CONTROLLER_TYPE_PS5Controller)) {
                                           parents_needing_virtual.push_back(dev);
                                       }
                                   }
                               }
                               for (uni_hid_device_t* parent : parents_needing_virtual) {
                                   uni_hid_device_t* child = uni_hid_device_create_virtual(parent);
                                   if (child != nullptr) {
                                       uni_hid_device_set_cod(child,
                                                              UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
                                       uni_hid_device_connect(child);
                                       if (!uni_hid_device_set_ready_complete(child)) {
                                           parent->child = nullptr;
                                       }
                                   }
                               }
                           }
                       },
                       [](const SetAllowedDeviceTypesCmd& c) {
                           g_allowed_device_types_mask.store(c.allowed_types_mask, std::memory_order_release);

                           std::vector<uni_hid_device_t*> disallowed_to_disconnect;
                           {
                               std::lock_guard<std::mutex> lock(g_state_mutex);
                               for (uni_hid_device_t* dev : g_devices) {
                                   if (dev != nullptr && !uni_hid_device_is_virtual_device(dev)) {
                                       const uint32_t dev_types = classify_physical_device_types(dev);
                                       if ((dev_types & c.allowed_types_mask) == 0) {
                                           disallowed_to_disconnect.push_back(dev);
                                       }
                                   }
                               }
                           }
                           for (uni_hid_device_t* dev : disallowed_to_disconnect) {
                               disconnect_and_delete_device(dev);
                           }
                       },
                       [](const SetBleServiceEnabledCmd& c) {
                           g_ble_service_enabled.store(c.enabled, std::memory_order_release);
                           uni_bt_service_set_enabled(c.enabled);
                       },
                       [](const SetBleServiceNameCmd& c) {
                           {
                               std::lock_guard<std::mutex> lock(g_ble_service_mutex);
                               g_ble_service_name = c.name;
                           }
                           uni_bt_service_set_name(c.name.data());
                       },
                       [](const SetBleServicePasswordCmd& c) {
                           {
                               std::lock_guard<std::mutex> lock(g_ble_service_mutex);
                               g_ble_service_password = c.password;
                           }
                           uni_bt_service_set_password(c.password.data());
                       },
                       [](const ShutdownCmd&) {
                           g_shutdown_requested.store(true, std::memory_order_release);
                           // Guard against headless unit tests where `hci_init()` was not called
                           // (`hci_stack` is NULL inside BTstack).
                           if (!g_hci_initialized.load(std::memory_order_acquire)) {
                               btstack_run_loop_trigger_exit();
                               return;
                           }
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
                       },
                   },
                   cmd);
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

/// Initializes Bluepad32 platform options (`--enhanced`, `--delete`, `--ble`, `-N`, `-P`, `-S`) during `uni_init()`.
void posix_imgui_init(int argc, const char** argv) {
    logi("posix_imgui: init()\n");
    g_hci_initialized.store(true, std::memory_order_release);
    bool ble_enabled = true;
    bool ble_service_enabled = true;
    const char* ble_service_name = CONFIG_BLUEPAD32_BLE_SERVICE_NAME;
    const char* ble_service_password = CONFIG_BLUEPAD32_BLE_SERVICE_PASSWORD;

    for (int i = 1; i < argc && argv != nullptr; i++) {
        if (argv[i] == nullptr) {
            continue;
        }
        if (std::strcmp(argv[i], "--enhanced") == 0 || std::strcmp(argv[i], "-e") == 0) {
            g_enhanced_mode = 1;
            logi("Enhanced mode enabled\n");
        } else if (std::strcmp(argv[i], "--delete") == 0 || std::strcmp(argv[i], "-d") == 0) {
            g_delete_keys = 1;
            logi("Stored keys will be deleted\n");
        } else if ((std::strcmp(argv[i], "--ble") == 0 || std::strcmp(argv[i], "-b") == 0) && i + 1 < argc &&
                   argv[i + 1] != nullptr) {
            ble_enabled = std::atoi(argv[++i]) != 0;
        } else if ((std::strcmp(argv[i], "--ble-service-name") == 0 || std::strcmp(argv[i], "--service-name") == 0 ||
                    std::strcmp(argv[i], "-N") == 0) &&
                   i + 1 < argc && argv[i + 1] != nullptr) {
            ble_service_name = argv[++i];
        } else if ((std::strcmp(argv[i], "--ble-service-password") == 0 ||
                    std::strcmp(argv[i], "--service-password") == 0 || std::strcmp(argv[i], "-P") == 0) &&
                   i + 1 < argc && argv[i + 1] != nullptr) {
            ble_service_password = argv[++i];
        } else if (std::strcmp(argv[i], "--no-ble-service") == 0 || std::strcmp(argv[i], "-S") == 0) {
            ble_service_enabled = false;
        } else if ((std::strcmp(argv[i], "--ble-service") == 0 || std::strcmp(argv[i], "--service") == 0) &&
                   i + 1 < argc && argv[i + 1] != nullptr) {
            ble_service_enabled = std::atoi(argv[++i]) != 0;
        }
    }

    const auto norm_name = normalize_ble_service_name(ble_service_name);
    const auto norm_pass = normalize_ble_service_password(ble_service_password);
    {
        std::lock_guard<std::mutex> lock(g_ble_service_mutex);
        g_ble_service_name = norm_name;
        g_ble_service_password = norm_pass;
    }
    g_ble_service_enabled.store(ble_service_enabled, std::memory_order_release);

    uni_bt_service_set_name(norm_name.data());
    uni_bt_service_set_password(norm_pass.data());
    uni_bt_service_set_enabled(ble_service_enabled);

    uni_bt_le_set_enabled(ble_enabled);
    logi("BLE enabled: %d, BLE service enabled: %d, name: '%s'\n", ble_enabled, ble_service_enabled, norm_name.data());
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
}

/// Invoked by Bluepad32 once Bluetooth initialization completes; starts scanning and auto-connect.
void posix_imgui_on_init_complete(void) {
    logi("posix_imgui: on_init_complete()\n");

    if (g_delete_keys) {
        uni_bt_del_keys_unsafe();
    } else {
        uni_bt_list_keys_unsafe();
    }

    // Enforce the platform's virtual-device state (disabled by default) regardless of
    // any stale property persisted in `/tmp/bp32_property.tlv`.
    uni_virtual_device_set_enabled(g_virtual_devices_enabled.load(std::memory_order_acquire));

    uni_property_dump_all();

    uni_bt_start_scanning_and_autoconnect_unsafe();
    uni_bt_allow_incoming_connections(true);
}

/// Filters newly discovered Bluetooth devices against `g_allowed_device_types_mask`.
uni_error_t posix_imgui_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)rssi;
    const uint32_t device_types = classify_cod_device_types(cod);
    const uint32_t allowed_mask = g_allowed_device_types_mask.load(std::memory_order_acquire);
    if ((device_types & allowed_mask) == 0) {
        logi("posix_imgui: ignoring discovered device %s ('%s', cod=%#x, types=%#x, allowed=%#x)\n",
             bd_addr_to_str(addr), name ? name : "", cod, device_types, allowed_mask);
        return UNI_ERROR_IGNORE_DEVICE;
    }
    return UNI_ERROR_SUCCESS;
}

/// Initializes per-device `platform_data` (`slot = -1`, `gamepad_seat = GAMEPAD_SEAT_NONE`) upon connection.
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

/// Reclaims the controller slot and clears its `ControllerSnapshot` when a device disconnects.
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
        if (ins->slot >= 0 && ins->slot < kMaxControllers && g_devices[static_cast<size_t>(ins->slot)] == d) {
            const auto slot_idx = static_cast<size_t>(ins->slot);
            g_devices[slot_idx] = nullptr;
            g_snapshots[slot_idx] = ControllerSnapshot{};
        }
        ins->slot = -1;
        ins->gamepad_seat = GAMEPAD_SEAT_NONE;
    }
}

/**
 * @brief Formats parser-specific hardware/firmware extra info into `snap.device_extra_info`.
 *
 * Why this helper is called in BOTH `posix_imgui_on_device_ready()` and `posix_imgui_on_controller_data()`:
 *   - Synchronous parsers (DualSense, Switch, Mouse) populate their firmware/scale metadata
 *     before `uni_hid_device_set_ready_complete(d)` is invoked.
 *   - Asynchronous and runtime-mutating parsers populate or upgrade their metadata *after*
 *     `on_device_ready()` (e.g., DualShock 4 receives feature report `0xa3` asynchronously
 *     after setup; Xbox Wireless upgrades `ins->version` from `v3.1` to `v4.8`/`v5.x` during
 *     `parse_usage()`; SInput receives feature response `0x02` asynchronously; Wii hot-plugs
 *     Nunchuk/Classic Controller expansions at runtime).
 *   Ensuring `<= 0` writes `snap.device_extra_info[0] = '\0'` guarantees the UI thread always
 *   observes a valid NUL-terminated string.
 */
static void update_snapshot_device_extra_info(const uni_hid_device_t* d, ControllerSnapshot& snap) {
    if (d != nullptr && d->report_parser.device_extra_info != nullptr) {
        if (d->report_parser.device_extra_info(d, snap.device_extra_info, sizeof(snap.device_extra_info)) <= 0) {
            snap.device_extra_info[0] = '\0';
        }
    } else {
        snap.device_extra_info[0] = '\0';
    }
}

/// Assigns the lowest free slot (`0..3`), populates capability metadata, and sets initial player LEDs.
uni_error_t posix_imgui_on_device_ready(uni_hid_device_t* d) {
    logi("posix_imgui: device ready: %p\n", static_cast<void*>(d));
    if (d == nullptr) {
        return UNI_ERROR_INVALID_DEVICE;
    }

    posix_imgui_instance_t* ins = get_posix_imgui_instance(d);
    const bool is_virtual = uni_hid_device_is_virtual_device(d);

    if (is_virtual) {
        if (!g_virtual_devices_enabled.load(std::memory_order_acquire)) {
            ins->slot = -1;
            ins->gamepad_seat = GAMEPAD_SEAT_NONE;
            logi("posix_imgui: rejecting virtual device %p (virtual devices disabled)\n", static_cast<void*>(d));
            return UNI_ERROR_IGNORE_DEVICE;
        }
    } else {
        const uint32_t device_types = classify_physical_device_types(d);
        const uint32_t allowed_mask = g_allowed_device_types_mask.load(std::memory_order_acquire);
        if ((device_types & allowed_mask) == 0) {
            ins->slot = -1;
            ins->gamepad_seat = GAMEPAD_SEAT_NONE;
            logi("posix_imgui: rejecting physical device %p (types=%#x, allowed=%#x)\n", static_cast<void*>(d),
                 device_types, allowed_mask);
            return UNI_ERROR_IGNORE_DEVICE;
        }
    }

    int assigned_slot = -1;

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        for (int i = 0; i < kMaxControllers; ++i) {
            if (g_devices[static_cast<size_t>(i)] == nullptr) {
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

        const auto slot_idx = static_cast<size_t>(assigned_slot);
        ins->slot = assigned_slot;
        ins->gamepad_seat = static_cast<uni_gamepad_seat_t>(BIT(assigned_slot));
        g_devices[slot_idx] = d;

        ControllerSnapshot& snap = g_snapshots[slot_idx];
        snap = ControllerSnapshot{};
        snap.connected = true;
        snap.vendor_id = d->vendor_id;
        snap.product_id = d->product_id;
        snap.controller_type = d->controller_type;
        snap.controller_subtype = d->controller_subtype;
        std::snprintf(snap.name, sizeof(snap.name), "%s", d->name);

        const char* model = uni_gamepad_get_model_name(d->controller_type);
        std::snprintf(snap.model_name, sizeof(snap.model_name), "%s", model ? model : "Unknown");
        update_snapshot_device_extra_info(d, snap);

        std::memcpy(snap.btaddr, d->conn.btaddr, sizeof(bd_addr_t));
        snap.rssi = d->conn.rssi;

        snap.layout = classify_controller_layout(d->controller_type);
        snap.is_virtual_device = is_virtual;
        snap.has_rumble = (d->report_parser.play_dual_rumble != nullptr);
        snap.has_player_leds = (d->report_parser.set_player_leds != nullptr);
        snap.has_rgb_led = (d->report_parser.set_lightbar_color != nullptr);
        snap.has_brightness_led = (d->controller_type == CONTROLLER_TYPE_SwitchProController ||
                                   d->controller_type == CONTROLLER_TYPE_SwitchJoyConRight);
        snap.has_imu = has_imu_support(d->controller_type);

        snap.controller = d->controller;
        if (is_virtual && uni_hid_device_is_mouse(d) && snap.controller.klass == UNI_CONTROLLER_CLASS_NONE) {
            snap.controller.klass = UNI_CONTROLLER_CLASS_MOUSE;
        }
        snap.last_report_timestamp_us = 0;
        snap.report_delta_ms = 0;

        // Avoid stealing tab focus away from the parent gamepad when a virtual child mouse attaches.
        if (!is_virtual) {
            g_most_recent_connected_slot = assigned_slot;
        }
    }

    if (d->report_parser.set_player_leds != nullptr) {
        d->report_parser.set_player_leds(d, static_cast<uint8_t>(BIT(assigned_slot)));
    }

    return UNI_ERROR_SUCCESS;
}

/// Copies the latest parsed `uni_controller_t` report and monotonic timestamp into `g_snapshots[slot]`.
void posix_imgui_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    if (d == nullptr || ctl == nullptr) {
        return;
    }

    posix_imgui_instance_t* ins = get_posix_imgui_instance(d);

    const auto now_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());

    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (ins->slot >= 0 && ins->slot < kMaxControllers && g_devices[static_cast<size_t>(ins->slot)] == d) {
        ControllerSnapshot& snap = g_snapshots[static_cast<size_t>(ins->slot)];
        if (snap.last_report_timestamp_us != 0 && now_us >= snap.last_report_timestamp_us) {
            snap.report_delta_ms = static_cast<uint32_t>((now_us - snap.last_report_timestamp_us) / 1000ULL);
        }
        snap.last_report_timestamp_us = now_us;
        snap.controller = *ctl;
        snap.rssi = d->conn.rssi;
        snap.controller_subtype = d->controller_subtype;
        update_snapshot_device_extra_info(d, snap);
    }
}

/// Returns custom platform properties (none overridden; returns `nullptr`).
const uni_property_t* posix_imgui_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return nullptr;
}

/// Logs out-of-band Bluepad32 platform events (e.g., system button press or scanning toggle).
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
        std::copy(g_snapshots.begin(), g_snapshots.end(), out_snapshots);
    }
    if (newly_connected_slot != nullptr) {
        *newly_connected_slot = g_most_recent_connected_slot;
        g_most_recent_connected_slot = -1;
    }
}

void posix_imgui_get_snapshots(std::span<ControllerSnapshot, kMaxControllers> out_snapshots,
                               int* newly_connected_slot) {
    posix_imgui_get_snapshots(out_snapshots.data(), newly_connected_slot);
}

void posix_imgui_request_rumble(int slot,
                                uint16_t start_delay_ms,
                                uint16_t duration_ms,
                                uint8_t weak_magnitude,
                                uint8_t strong_magnitude) {
    enqueue_command(RumbleCmd{
        .slot = slot,
        .start_delay_ms = start_delay_ms,
        .duration_ms = duration_ms,
        .weak_magnitude = weak_magnitude,
        .strong_magnitude = strong_magnitude,
    });
}

void posix_imgui_request_player_leds(int slot, uint8_t leds_bitmask) {
    enqueue_command(PlayerLedsCmd{
        .slot = slot,
        .leds_bitmask = static_cast<uint8_t>(leds_bitmask & 0x0f),
    });
}

void posix_imgui_request_lightbar_color(int slot, uint8_t r, uint8_t g, uint8_t b) {
    enqueue_command(LightbarColorCmd{
        .slot = slot,
        .r = r,
        .g = g,
        .b = b,
    });
}

void posix_imgui_request_set_virtual_devices_enabled(bool enabled) {
    g_virtual_devices_enabled.store(enabled, std::memory_order_release);
    enqueue_command(SetVirtualDevicesCmd{
        .enabled = enabled,
    });
}

bool posix_imgui_is_virtual_devices_enabled(void) {
    return g_virtual_devices_enabled.load(std::memory_order_acquire);
}

void posix_imgui_request_set_allowed_device_types(uint32_t allowed_types_mask) {
    g_allowed_device_types_mask.store(allowed_types_mask, std::memory_order_release);
    enqueue_command(SetAllowedDeviceTypesCmd{
        .allowed_types_mask = allowed_types_mask,
    });
}

uint32_t posix_imgui_get_allowed_device_types(void) {
    return g_allowed_device_types_mask.load(std::memory_order_acquire);
}

void posix_imgui_request_set_ble_service_enabled(bool enabled) {
    g_ble_service_enabled.store(enabled, std::memory_order_release);
    // In single-threaded headless unit tests (before `posix_imgui_init()` sets `g_hci_initialized`),
    // also update `uni_bt_service` synchronously. Once the multi-threaded runtime is active,
    // all `uni_bt_service_*` calls are dispatched exclusively on Thread 2 via `g_cmd_queue`.
    if (!g_hci_initialized.load(std::memory_order_acquire)) {
        uni_bt_service_set_enabled(enabled);
    }
    enqueue_command(SetBleServiceEnabledCmd{
        .enabled = enabled,
    });
}

void posix_imgui_set_ble_service_enabled(bool enabled) {
    posix_imgui_request_set_ble_service_enabled(enabled);
}

bool posix_imgui_is_ble_service_enabled(void) {
    return g_ble_service_enabled.load(std::memory_order_acquire);
}

bool posix_imgui_get_ble_service_enabled(void) {
    return posix_imgui_is_ble_service_enabled();
}

void posix_imgui_request_set_ble_service_name(const char* name) {
    const auto norm = normalize_ble_service_name(name);
    {
        std::lock_guard<std::mutex> lock(g_ble_service_mutex);
        g_ble_service_name = norm;
    }
    if (!g_hci_initialized.load(std::memory_order_acquire)) {
        uni_bt_service_set_name(norm.data());
    }
    enqueue_command(SetBleServiceNameCmd{
        .name = norm,
    });
}

void posix_imgui_set_ble_service_name(const char* name) {
    posix_imgui_request_set_ble_service_name(name);
}

void posix_imgui_get_ble_service_name(char* out_buf, size_t out_len) {
    if (out_buf == nullptr || out_len == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_ble_service_mutex);
    std::snprintf(out_buf, out_len, "%s", g_ble_service_name.data());
}

const char* posix_imgui_get_ble_service_name(void) {
    // Copy under `g_ble_service_mutex` into a thread-local buffer so returning a `const char*`
    // after releasing the mutex never races with concurrent updates on another thread.
    thread_local std::array<char, UNI_BT_SERVICE_NAME_MAX_LEN + 1> tls_buf{};
    {
        std::lock_guard<std::mutex> lock(g_ble_service_mutex);
        tls_buf = g_ble_service_name;
    }
    return tls_buf.data();
}

void posix_imgui_request_set_ble_service_password(const char* password) {
    const auto norm = normalize_ble_service_password(password);
    {
        std::lock_guard<std::mutex> lock(g_ble_service_mutex);
        g_ble_service_password = norm;
    }
    if (!g_hci_initialized.load(std::memory_order_acquire)) {
        uni_bt_service_set_password(norm.data());
    }
    enqueue_command(SetBleServicePasswordCmd{
        .password = norm,
    });
}

void posix_imgui_set_ble_service_password(const char* password) {
    posix_imgui_request_set_ble_service_password(password);
}

void posix_imgui_get_ble_service_password(char* out_buf, size_t out_len) {
    if (out_buf == nullptr || out_len == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_ble_service_mutex);
    std::snprintf(out_buf, out_len, "%s", g_ble_service_password.data());
}

const char* posix_imgui_get_ble_service_password(void) {
    thread_local std::array<char, UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1> tls_buf{};
    {
        std::lock_guard<std::mutex> lock(g_ble_service_mutex);
        tls_buf = g_ble_service_password;
    }
    return tls_buf.data();
}

void posix_imgui_request_shutdown(void) {
    g_shutdown_requested.store(true, std::memory_order_release);
    enqueue_command(ShutdownCmd{});
}

bool posix_imgui_is_shutdown_requested(void) {
    return g_shutdown_requested.load(std::memory_order_acquire);
}

void posix_imgui_reset_for_test(void) {
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        g_devices.fill(nullptr);
        g_snapshots.fill(ControllerSnapshot{});
        g_most_recent_connected_slot = -1;
    }
    {
        std::lock_guard<std::mutex> lock(g_cmd_mutex);
        g_cmd_queue.clear();
        g_cmd_wakeup_pending = false;
    }
    btstack_run_loop_base_execute_callbacks();
    g_hci_initialized.store(false, std::memory_order_release);
    g_enhanced_mode = 0;
    g_delete_keys = 0;
    g_shutdown_requested.store(false, std::memory_order_release);
    g_virtual_devices_enabled.store(false, std::memory_order_release);
    g_allowed_device_types_mask.store(POSIX_IMGUI_DEVICE_TYPE_DEFAULT, std::memory_order_release);
    uni_virtual_device_set_enabled(false);

    // Reset BLE configuration service state (Landmine #6)
    g_ble_service_enabled.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(g_ble_service_mutex);
        g_ble_service_name = normalize_ble_service_name("Bluepad32");
        g_ble_service_password = normalize_ble_service_password("");
    }
    uni_bt_service_set_name("Bluepad32");
    uni_bt_service_set_password("");
    uni_bt_service_set_enabled(true);
}

void posix_imgui_process_pending_commands(void) {
    process_pending_commands(nullptr);
}
