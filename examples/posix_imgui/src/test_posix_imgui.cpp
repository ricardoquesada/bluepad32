// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file test_posix_imgui.cpp
 * @brief Headless automated unit test suite (`test_posix_imgui`) for `examples/posix_imgui`.
 *
 * Architectural Role & Test Strategy:
 *   Exercises all non-GPU subsystems of the Bluepad32 POSIX Dear ImGui Controller Tester
 *   deterministically without requiring a physical USB Bluetooth dongle or an active
 *   X11/Wayland display server:
 *
 *   - Suite A (`posix_imgui_platform` Lifecycle & Slot Management):
 *     Verifies that `on_device_connected()` initializes `ins->slot = -1` (preventing
 *     zero-initialized `d->platform_data` from clobbering Slot 0 on pre-ready disconnect),
 *     assigns up to 4 seats (`GAMEPAD_SEAT_A`..`D`), rejects a 5th controller with
 *     `UNI_ERROR_NO_SLOTS`, and reclaims slots cleanly on disconnect/reconnection.
 *   - Suite B (`ControllerLayoutType` & Capability Detection):
 *     Verifies classification of Xbox (`STANDARD`), PlayStation (`SHAPES`), and Nintendo
 *     Switch (`REVERSE`) controller models and capability flags (`has_rumble`,
 *     `has_player_leds`, `has_rgb_led`, `has_brightness_led`, `has_imu`).
 *   - Suite C (Live Telemetry & Pipe-Woken Command Queue Bridge):
 *     Verifies `on_controller_data()` snapshot updates, `report_delta_ms` calculation,
 *     single-frame `newly_connected_slot` latching, command queue draining via
 *     `btstack_run_loop_base_execute_callbacks()`, and dangling-pointer protection when
 *     a device disconnects before its queued command executes.
 *   - Suite D (DualSense Player LEDs 4-Bit Bitmask & Output Report Verification):
 *     Verifies `uni_hid_parser_ds5_set_player_leds()` NULL-device guard clause, Sony PS5
 *     symmetric 5-LED patterns for single seats (`GAMEPAD_SEAT_NONE` and `GAMEPAD_SEAT_A..D`,
 *     including the Player 3 / Player 4 inversion regression fix), all 16 4-bit masks
 *     (`0x00..0x0f`) with sequence counter wrap and CRC32 validation, upper-nibble masking
 *     (`0xf0 | mask`), and end-to-end `posix_imgui_platform` integration.
 *   - Suite E (Device Auto-Accept Filtering & Virtual Device Toggle):
 *     Verifies Class-of-Device discovery filtering, runtime physical device disconnection
 *     when device categories are disabled, and virtual child device lifecycle.
 *   - Suite F (C++23 `std::span` Snapshot Overloads & `std::variant` Command Queue Verification):
 *     Verifies single-argument `std::span` and legacy pointer/`nullptr` overloads of
 *     `posix_imgui_get_snapshots()`, FIFO batch draining of `std::variant` commands via
 *     `std::visit`, out-of-bounds slot rejection, and headless `ShutdownCmd` safety.
 *   - Suite G (`DemoScene` Headless ImGui Frame & `ControllerSlotUiState` Lifecycle):
 *     Verifies headless `DemoScene::DoFrame()` execution across all 4 controller classes,
 *     all 6 category sub-tabs, Preferences tab, and 4 DPI/font scales (`0.50f..4.00f`) with
 *     zero `IM_ASSERT` failures, and verifies `ControllerSlotUiState::ResetForSlot()` on
 *     controller disconnect/reconnect transitions.
 *   - Suite H (`device_extra_info` Parser Callbacks, Cross-Thread Snapshot Propagation & Info Tab Rendering):
 *     Verifies NULL/zero-length guard clauses and small-buffer `snprintf` truncation across all
 *     8 parser `uni_hid_parser_*_device_extra_info` callbacks, exact prefix-free/newline-free
 *     formatting and out-of-bounds enum resilience, synchronous (`on_device_ready`) and
 *     asynchronous (`on_controller_data`) propagation into `ControllerSnapshot::device_extra_info`,
 *     and headless `DemoScene` Info tab rendering for both populated and empty `device_extra_info`.
 */

// Standard C++23 headers MUST be included outside and before `extern "C"`.
#include <unistd.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

extern "C" {
#include <btstack.h>
#include <uni.h>
#include "btstack_run_loop_posix.h"
#include "parser/uni_hid_parser_ds4.h"
#include "parser/uni_hid_parser_ds5.h"
#include "parser/uni_hid_parser_keyboard.h"
#include "parser/uni_hid_parser_mouse.h"
#include "parser/uni_hid_parser_sinput.h"
#include "parser/uni_hid_parser_steam_triton.h"
#include "parser/uni_hid_parser_switch.h"
#include "parser/uni_hid_parser_switch2.h"
#include "parser/uni_hid_parser_wii.h"
#include "parser/uni_hid_parser_xboxone.h"
}

#include "demo_scene.h"
#include "imgui.h"
#include "posix_imgui_platform.h"

static_assert(std::is_trivially_copyable_v<ControllerSnapshot>, "ControllerSnapshot must remain trivially copyable.");

namespace {

int g_tests_run = 0;
int g_tests_failed = 0;

#define TEST_ASSERT(cond)                                                                              \
    do {                                                                                               \
        if (!(cond)) {                                                                                 \
            std::fprintf(stderr, "  [FAIL] %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #cond); \
            g_tests_failed++;                                                                          \
            return;                                                                                    \
        }                                                                                              \
    } while (0)

#define TEST_ASSERT_FLOAT_NEAR(expected, actual, tol)                                                                  \
    do {                                                                                                               \
        double _exp = static_cast<double>(expected);                                                                   \
        double _act = static_cast<double>(actual);                                                                     \
        double _tol = static_cast<double>(tol);                                                                        \
        if (std::fabs(_exp - _act) > _tol) {                                                                           \
            std::fprintf(stderr, "  [FAIL] %s:%d: Expected %.6f +/- %.6f, got %.6f\n", __FILE__, __LINE__, _exp, _tol, \
                         _act);                                                                                        \
            g_tests_failed++;                                                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define RUN_TEST(fn)                           \
    do {                                       \
        int _before = g_tests_failed;          \
        g_tests_run++;                         \
        fn();                                  \
        if (g_tests_failed == _before) {       \
            std::printf("  [PASS] %s\n", #fn); \
        }                                      \
    } while (0)

/// Captures invocation counts and arguments for mock `uni_report_parser_t` output hooks.
struct MockParserCalls {
    int rumble_calls;
    uint16_t last_start_delay_ms;
    uint16_t last_duration_ms;
    uint8_t last_weak;
    uint8_t last_strong;

    int player_leds_calls;
    uint8_t last_leds;

    int lightbar_calls;
    uint8_t last_r;
    uint8_t last_g;
    uint8_t last_b;
};

MockParserCalls g_mock_calls = {};
std::vector<std::string> g_command_trace;

/// Resets `g_mock_calls` counters and clears the FIFO `g_command_trace` log.
void reset_mock_calls() {
    g_mock_calls = MockParserCalls{};
    g_command_trace.clear();
}

/// Mock `play_dual_rumble` callback recording parameters and appending to `g_command_trace`.
void mock_play_dual_rumble(struct uni_hid_device_s* d,
                           uint16_t start_delay_ms,
                           uint16_t duration_ms,
                           uint8_t weak_magnitude,
                           uint8_t strong_magnitude) {
    g_mock_calls.rumble_calls++;
    g_mock_calls.last_start_delay_ms = start_delay_ms;
    g_mock_calls.last_duration_ms = duration_ms;
    g_mock_calls.last_weak = weak_magnitude;
    g_mock_calls.last_strong = strong_magnitude;

    const int slot = (d != nullptr) ? get_posix_imgui_instance(d)->slot : -1;
    char buf[96];
    std::snprintf(buf, sizeof(buf), "rumble:slot=%d:delay=%u:dur=%u:weak=%u:strong=%u", slot,
                  static_cast<unsigned>(start_delay_ms), static_cast<unsigned>(duration_ms),
                  static_cast<unsigned>(weak_magnitude), static_cast<unsigned>(strong_magnitude));
    g_command_trace.emplace_back(buf);
}

/// Mock `set_player_leds` callback recording the 4-bit LED bitmask and appending to `g_command_trace`.
void mock_set_player_leds(struct uni_hid_device_s* d, uint8_t leds) {
    g_mock_calls.player_leds_calls++;
    g_mock_calls.last_leds = leds;

    const int slot = (d != nullptr) ? get_posix_imgui_instance(d)->slot : -1;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "leds:slot=%d:mask=0x%02x", slot, static_cast<unsigned>(leds));
    g_command_trace.emplace_back(buf);
}

/// Mock `set_lightbar_color` callback recording RGB channels and appending to `g_command_trace`.
void mock_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b) {
    g_mock_calls.lightbar_calls++;
    g_mock_calls.last_r = r;
    g_mock_calls.last_g = g;
    g_mock_calls.last_b = b;

    const int slot = (d != nullptr) ? get_posix_imgui_instance(d)->slot : -1;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "lightbar:slot=%d:r=%u:g=%u:b=%u", slot, static_cast<unsigned>(r),
                  static_cast<unsigned>(g), static_cast<unsigned>(b));
    g_command_trace.emplace_back(buf);
}

/// Zero-initializes a synthetic `uni_hid_device_t` and populates VID/PID, model type, name, and READY state.
void init_synthetic_device(uni_hid_device_t* d,
                           uint16_t vid,
                           uint16_t pid,
                           uni_controller_type_t type,
                           const char* name) {
    std::memset(d, 0, sizeof(*d));
    d->vendor_id = vid;
    d->product_id = pid;
    d->controller_type = type;
    if (name != nullptr) {
        std::snprintf(d->name, sizeof(d->name), "%s", name);
    }
    d->conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    d->conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    d->conn.rssi = 200;
}

// ============================================================================
// Suite A: posix_imgui_platform Lifecycle, Slot Management & Pre-Ready Guard
// ============================================================================

/// Test 1: Verifies `on_device_connected` overwrites `memset`-zeroed `platform_data`
/// with `slot = -1` and `gamepad_seat = GAMEPAD_SEAT_NONE`.
void test_on_device_connected_initializes_invalid_slot() {
    posix_imgui_reset_for_test();
    struct uni_platform* plat = get_posix_imgui_platform();
    TEST_ASSERT(plat != nullptr);

    uni_hid_device_t d;
    std::memset(&d, 0, sizeof(d));
    // Before on_device_connected, memset leaves platform_data bytes at 0.
    TEST_ASSERT(get_posix_imgui_instance(&d)->slot == 0);

    plat->on_device_connected(&d);
    TEST_ASSERT(get_posix_imgui_instance(&d)->slot == -1);
    TEST_ASSERT(get_posix_imgui_instance(&d)->gamepad_seat == GAMEPAD_SEAT_NONE);

    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    int newly_connected = 99;
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(newly_connected == -1);
    for (const ControllerSnapshot& snap : snapshots) {
        TEST_ASSERT(!snap.connected);
    }
}

/// Test 2: Verifies that a second device disconnecting before `on_device_ready`
/// (e.g., SDP/L2CAP timeout) never clobbers an active controller in Slot 0.
void test_pre_ready_disconnect_does_not_clobber_slot_0() {
    posix_imgui_reset_for_test();
    struct uni_platform* plat = get_posix_imgui_platform();

    uni_hid_device_t d0;
    init_synthetic_device(&d0, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense #1");
    plat->on_device_connected(&d0);
    TEST_ASSERT(plat->on_device_ready(&d0) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(get_posix_imgui_instance(&d0)->slot == 0);

    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    int newly_connected = -1;
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(newly_connected == 0);
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(snapshots[0].vendor_id == 0x054c);

    // Connect a second device d1 that fails SDP/L2CAP before reaching on_device_ready().
    uni_hid_device_t d1;
    std::memset(&d1, 0, sizeof(d1));
    plat->on_device_connected(&d1);
    TEST_ASSERT(get_posix_imgui_instance(&d1)->slot == -1);
    plat->on_device_disconnected(&d1);

    // Verify Slot 0 (Device d0) is completely untouched.
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(snapshots[0].vendor_id == 0x054c);
    TEST_ASSERT(std::strcmp(snapshots[0].name, "DualSense #1") == 0);
}

/// Test 3: Verifies seat assignment across all 4 slots (`0..3`) and confirms a 5th
/// controller is rejected with `UNI_ERROR_NO_SLOTS` without clobbering slots `0..3`.
void test_seat_assignment_up_to_4_and_5th_controller_rejection() {
    posix_imgui_reset_for_test();
    reset_mock_calls();
    struct uni_platform* plat = get_posix_imgui_platform();

    std::array<uni_hid_device_t, kMaxControllers> devices{};
    for (int i = 0; i < kMaxControllers; ++i) {
        const auto idx = static_cast<size_t>(i);
        init_synthetic_device(&devices[idx], static_cast<uint16_t>(0x1000 + i), static_cast<uint16_t>(0x2000 + i),
                              CONTROLLER_TYPE_XBoxOneController, "Controller");
        devices[idx].report_parser.set_player_leds = &mock_set_player_leds;

        plat->on_device_connected(&devices[idx]);
        TEST_ASSERT(plat->on_device_ready(&devices[idx]) == UNI_ERROR_SUCCESS);
        TEST_ASSERT(get_posix_imgui_instance(&devices[idx])->slot == i);
        TEST_ASSERT(get_posix_imgui_instance(&devices[idx])->gamepad_seat == static_cast<uni_gamepad_seat_t>(BIT(i)));
        TEST_ASSERT(g_mock_calls.last_leds == static_cast<uint8_t>(BIT(i)));

        std::array<ControllerSnapshot, kMaxControllers> snapshots{};
        int newly_connected = -1;
        posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
        TEST_ASSERT(newly_connected == i);
        TEST_ASSERT(snapshots[idx].connected);
    }

    // Connect a 5th device and verify rejection with UNI_ERROR_NO_SLOTS.
    uni_hid_device_t d4;
    init_synthetic_device(&d4, 0x9999, 0x8888, CONTROLLER_TYPE_XBoxOneController, "5th Pad");
    plat->on_device_connected(&d4);
    TEST_ASSERT(plat->on_device_ready(&d4) == UNI_ERROR_NO_SLOTS);
    TEST_ASSERT(get_posix_imgui_instance(&d4)->slot == -1);

    // Disconnecting the rejected 5th device must not clobber any of slots 0..3.
    plat->on_device_disconnected(&d4);
    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    int newly_connected = -1;
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    for (int i = 0; i < kMaxControllers; ++i) {
        const auto idx = static_cast<size_t>(i);
        TEST_ASSERT(snapshots[idx].connected);
        TEST_ASSERT(snapshots[idx].vendor_id == static_cast<uint16_t>(0x1000 + i));
    }
}

/// Test 4: Verifies that disconnecting an intermediate slot (`Slot 1`) reclaims that
/// exact lowest-numbered slot for the next controller to connect.
void test_slot_reclamation_on_disconnect_and_reconnection() {
    posix_imgui_reset_for_test();
    struct uni_platform* plat = get_posix_imgui_platform();

    std::array<uni_hid_device_t, kMaxControllers> devices{};
    for (int i = 0; i < kMaxControllers; ++i) {
        const auto idx = static_cast<size_t>(i);
        init_synthetic_device(&devices[idx], static_cast<uint16_t>(0x10 + i), 0x20, CONTROLLER_TYPE_PS4Controller,
                              "DS4");
        plat->on_device_connected(&devices[idx]);
        TEST_ASSERT(plat->on_device_ready(&devices[idx]) == UNI_ERROR_SUCCESS);
    }

    // Disconnect slot 1.
    plat->on_device_disconnected(&devices[1]);
    TEST_ASSERT(get_posix_imgui_instance(&devices[1])->slot == -1);

    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    int newly_connected = -1;
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(!snapshots[1].connected);
    TEST_ASSERT(snapshots[2].connected);
    TEST_ASSERT(snapshots[3].connected);

    // Connect a new device and verify it reclaims Slot 1.
    uni_hid_device_t d_new;
    init_synthetic_device(&d_new, 0x057e, 0x2009, CONTROLLER_TYPE_SwitchProController, "Switch Pro");
    plat->on_device_connected(&d_new);
    TEST_ASSERT(plat->on_device_ready(&d_new) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(get_posix_imgui_instance(&d_new)->slot == 1);

    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(newly_connected == 1);
    TEST_ASSERT(snapshots[1].connected);
    TEST_ASSERT(snapshots[1].vendor_id == 0x057e);
}

// ============================================================================
// Suite B: ControllerLayoutType & Capability Detection
// ============================================================================

/// Test 5: Verifies `uni_gamepad_get_model_name()` and `ControllerLayoutType` classification
/// across Xbox/Generic/Steam, PlayStation, and Nintendo Switch 1 & 2 controller types.
void test_controller_layout_classification() {
    // 1. Verify exact model names for Switch 2 and Steam Triton controller types.
    struct ModelNameCase {
        uni_controller_type_t type;
        const char* expected_name;
    };
    constexpr std::array<ModelNameCase, 4> kNewModelCases = {{
        {CONTROLLER_TYPE_Switch2ProController, "Switch 2 Pro"},
        {CONTROLLER_TYPE_Switch2JoyConLeft, "Switch 2 JoyCon Left"},
        {CONTROLLER_TYPE_Switch2JoyConRight, "Switch 2 JoyCon Right"},
        {CONTROLLER_TYPE_SteamControllerTriton, "Steam Triton"},
    }};
    for (const ModelNameCase& mc : kNewModelCases) {
        const char* actual = uni_gamepad_get_model_name(mc.type);
        TEST_ASSERT(actual != nullptr);
        TEST_ASSERT(actual[0] != '\0');
        TEST_ASSERT(std::strstr(actual, "Unknown") == nullptr);
        TEST_ASSERT(std::strcmp(actual, mc.expected_name) == 0);
    }

    // 2. Verify layout classification across Xbox/Generic/Steam, PlayStation, and Switch 1 & 2.
    struct LayoutCase {
        uni_controller_type_t type;
        ControllerLayoutType expected_layout;
    };

    constexpr std::array<LayoutCase, 20> kCases = {{
        {CONTROLLER_TYPE_XBoxOneController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_XBox360Controller, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_AndroidController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_GenericController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_8BitdoController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_SteamController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_SteamControllerTriton, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_PS3Controller, CONTROLLER_LAYOUT_SHAPES},
        {CONTROLLER_TYPE_PS4Controller, CONTROLLER_LAYOUT_SHAPES},
        {CONTROLLER_TYPE_PS5Controller, CONTROLLER_LAYOUT_SHAPES},
        {CONTROLLER_TYPE_PSMoveController, CONTROLLER_LAYOUT_SHAPES},
        {CONTROLLER_TYPE_SwitchProController, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_SwitchJoyConLeft, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_SwitchJoyConRight, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_SwitchJoyConPair, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_SwitchInputOnlyController, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_XInputSwitchController, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_Switch2ProController, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_Switch2JoyConLeft, CONTROLLER_LAYOUT_REVERSE},
        {CONTROLLER_TYPE_Switch2JoyConRight, CONTROLLER_LAYOUT_REVERSE},
    }};

    struct uni_platform* plat = get_posix_imgui_platform();
    for (const LayoutCase& tc : kCases) {
        posix_imgui_reset_for_test();
        uni_hid_device_t d;
        init_synthetic_device(&d, 0x1234, 0x5678, tc.type, "TestPad");
        plat->on_device_connected(&d);
        TEST_ASSERT(plat->on_device_ready(&d) == UNI_ERROR_SUCCESS);

        std::array<ControllerSnapshot, kMaxControllers> snapshots{};
        posix_imgui_get_snapshots(std::span{snapshots});
        TEST_ASSERT(snapshots[0].layout == tc.expected_layout);
        TEST_ASSERT(snapshots[0].model_name[0] != '\0');
        TEST_ASSERT(std::strstr(snapshots[0].model_name, "Unknown") == nullptr);
    }
}

/// Test 6: Verifies hardware capability detection (`has_rumble`, `has_player_leds`,
/// `has_rgb_led`, `has_brightness_led`, `has_imu`) across DualSense, Switch Pro, Xbox One,
/// Switch 2 (Pro, Joy-Con L, Joy-Con R), Steam Triton, and Steam Controller 2015.
void test_controller_capability_flags() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    struct uni_platform* plat = get_posix_imgui_platform();

    // Case 1: DualSense (rumble, player LEDs, RGB lightbar, IMU, no brightness LED)
    posix_imgui_reset_for_test();
    uni_hid_device_t ds5;
    init_synthetic_device(&ds5, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
    ds5.report_parser.play_dual_rumble = &mock_play_dual_rumble;
    ds5.report_parser.set_player_leds = &mock_set_player_leds;
    ds5.report_parser.set_lightbar_color = &mock_set_lightbar_color;
    plat->on_device_connected(&ds5);
    TEST_ASSERT(plat->on_device_ready(&ds5) == UNI_ERROR_SUCCESS);

    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].has_rumble);
    TEST_ASSERT(snapshots[0].has_player_leds);
    TEST_ASSERT(snapshots[0].has_rgb_led);
    TEST_ASSERT(!snapshots[0].has_brightness_led);
    TEST_ASSERT(snapshots[0].has_imu);

    // Case 2: Switch Pro Controller (rumble, player LEDs, brightness LED, IMU, no RGB LED)
    posix_imgui_reset_for_test();
    uni_hid_device_t sw_pro;
    init_synthetic_device(&sw_pro, 0x057e, 0x2009, CONTROLLER_TYPE_SwitchProController, "Switch Pro");
    sw_pro.report_parser.play_dual_rumble = &mock_play_dual_rumble;
    sw_pro.report_parser.set_player_leds = &mock_set_player_leds;
    plat->on_device_connected(&sw_pro);
    TEST_ASSERT(plat->on_device_ready(&sw_pro) == UNI_ERROR_SUCCESS);

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].has_rumble);
    TEST_ASSERT(snapshots[0].has_player_leds);
    TEST_ASSERT(!snapshots[0].has_rgb_led);
    TEST_ASSERT(snapshots[0].has_brightness_led);
    TEST_ASSERT(snapshots[0].has_imu);

    // Case 3: Xbox One Controller (rumble only)
    posix_imgui_reset_for_test();
    uni_hid_device_t xbox;
    init_synthetic_device(&xbox, 0x045e, 0x02fd, CONTROLLER_TYPE_XBoxOneController, "Xbox One");
    xbox.report_parser.play_dual_rumble = &mock_play_dual_rumble;
    plat->on_device_connected(&xbox);
    TEST_ASSERT(plat->on_device_ready(&xbox) == UNI_ERROR_SUCCESS);

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].has_rumble);
    TEST_ASSERT(!snapshots[0].has_player_leds);
    TEST_ASSERT(!snapshots[0].has_rgb_led);
    TEST_ASSERT(!snapshots[0].has_brightness_led);
    TEST_ASSERT(!snapshots[0].has_imu);

    // Case 4: Nintendo Switch 2 Pro (057e:2069), Joy-Con L (057e:2067), Joy-Con R (057e:2066)
    // Resolved via `uni_hid_device_guess_controller_type_from_pid_vid`:
    //   has_rumble = true, has_player_leds = true, has_rgb_led = false, has_brightness_led = false, has_imu = true
    constexpr std::array<uint16_t, 3> kSwitch2Pids = {0x2069, 0x2067, 0x2066};
    for (uint16_t pid : kSwitch2Pids) {
        posix_imgui_reset_for_test();
        uni_hid_device_t sw2{};
        sw2.vendor_id = 0x057e;
        sw2.product_id = pid;
        std::snprintf(sw2.name, sizeof(sw2.name), "Switch 2 %04x", static_cast<unsigned>(pid));
        sw2.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
        sw2.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
        uni_hid_device_guess_controller_type_from_pid_vid(&sw2);

        plat->on_device_connected(&sw2);
        TEST_ASSERT(plat->on_device_ready(&sw2) == UNI_ERROR_SUCCESS);

        posix_imgui_get_snapshots(std::span{snapshots});
        TEST_ASSERT(snapshots[0].layout == CONTROLLER_LAYOUT_REVERSE);
        TEST_ASSERT(snapshots[0].has_rumble);
        TEST_ASSERT(snapshots[0].has_player_leds);
        TEST_ASSERT(!snapshots[0].has_rgb_led);
        TEST_ASSERT(!snapshots[0].has_brightness_led);
        TEST_ASSERT(snapshots[0].has_imu);
    }

    // Case 5: Valve Steam Controller Triton (28de:1303)
    //   has_rumble = true, has_player_leds = false, has_rgb_led = false, has_brightness_led = false, has_imu = true
    posix_imgui_reset_for_test();
    uni_hid_device_t triton{};
    triton.vendor_id = 0x28de;
    triton.product_id = 0x1303;
    std::snprintf(triton.name, sizeof(triton.name), "Steam Controller");
    triton.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    triton.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    uni_hid_device_guess_controller_type_from_pid_vid(&triton);

    plat->on_device_connected(&triton);
    TEST_ASSERT(plat->on_device_ready(&triton) == UNI_ERROR_SUCCESS);

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].controller_type == CONTROLLER_TYPE_SteamControllerTriton);
    TEST_ASSERT(std::strcmp(snapshots[0].model_name, "Steam Triton") == 0);
    TEST_ASSERT(snapshots[0].layout == CONTROLLER_LAYOUT_STANDARD);
    TEST_ASSERT(snapshots[0].has_rumble);
    TEST_ASSERT(!snapshots[0].has_player_leds);
    TEST_ASSERT(!snapshots[0].has_rgb_led);
    TEST_ASSERT(!snapshots[0].has_brightness_led);
    TEST_ASSERT(snapshots[0].has_imu);

    // Case 6: Valve Steam Controller 2015 (28de:1106) -> has_imu = true
    posix_imgui_reset_for_test();
    uni_hid_device_t steam_2015{};
    steam_2015.vendor_id = 0x28de;
    steam_2015.product_id = 0x1106;
    std::snprintf(steam_2015.name, sizeof(steam_2015.name), "SteamController");
    steam_2015.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    steam_2015.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    uni_hid_device_guess_controller_type_from_pid_vid(&steam_2015);

    plat->on_device_connected(&steam_2015);
    TEST_ASSERT(plat->on_device_ready(&steam_2015) == UNI_ERROR_SUCCESS);

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].controller_type == CONTROLLER_TYPE_SteamController);
    TEST_ASSERT(snapshots[0].layout == CONTROLLER_LAYOUT_STANDARD);
    TEST_ASSERT(snapshots[0].has_imu);
}

// ============================================================================
// Suite C: Live Telemetry & Command Queue Bridge
// ============================================================================

/// Test 7: Verifies `on_controller_data` updates `ControllerSnapshot` fields (`buttons`,
/// `dpad`, `misc_buttons`, axes, triggers, IMU, battery, RSSI) and computes `report_delta_ms`.
void test_on_controller_data_updates_snapshot_and_delta_ms() {
    posix_imgui_reset_for_test();
    struct uni_platform* plat = get_posix_imgui_platform();

    uni_hid_device_t d0;
    init_synthetic_device(&d0, 0x054c, 0x09cc, CONTROLLER_TYPE_PS4Controller, "DualShock 4");
    plat->on_device_connected(&d0);
    TEST_ASSERT(plat->on_device_ready(&d0) == UNI_ERROR_SUCCESS);

    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    int newly_connected = -1;
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(newly_connected == 0);

    // Second call must reset newly_connected_slot to -1.
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(newly_connected == -1);

    uni_controller_t ctl1{};
    ctl1.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    ctl1.battery = 128;
    ctl1.gamepad.buttons = BUTTON_A;
    ctl1.gamepad.axis_x = -256;
    ctl1.gamepad.brake = 300;
    plat->on_controller_data(&d0, &ctl1);

    ::usleep(5000);  // 5 ms

    uni_controller_t ctl2{};
    ctl2.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    ctl2.battery = 250;
    ctl2.gamepad.buttons = BUTTON_B | BUTTON_SHOULDER_R;
    ctl2.gamepad.dpad = DPAD_UP;
    ctl2.gamepad.misc_buttons = MISC_BUTTON_SYSTEM;
    ctl2.gamepad.axis_x = 400;
    ctl2.gamepad.axis_ry = -512;
    ctl2.gamepad.brake = 900;
    ctl2.gamepad.throttle = 1023;
    ctl2.gamepad.accel[0] = -1.25f;
    ctl2.gamepad.accel[1] = UNI_STANDARD_GRAVITY;
    ctl2.gamepad.accel[2] = 0.5f;
    ctl2.gamepad.gyro[0] = -0.75f;
    ctl2.gamepad.gyro[1] = 1.5f;
    ctl2.gamepad.gyro[2] = -0.125f;
    d0.conn.rssi = 215;
    plat->on_controller_data(&d0, &ctl2);

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(snapshots[0].rssi == 215);
    TEST_ASSERT(snapshots[0].controller.battery == 250);
    TEST_ASSERT(snapshots[0].controller.gamepad.buttons == (BUTTON_B | BUTTON_SHOULDER_R));
    TEST_ASSERT(snapshots[0].controller.gamepad.dpad == DPAD_UP);
    TEST_ASSERT(snapshots[0].controller.gamepad.misc_buttons == MISC_BUTTON_SYSTEM);
    TEST_ASSERT(snapshots[0].controller.gamepad.axis_x == 400);
    TEST_ASSERT(snapshots[0].controller.gamepad.axis_ry == -512);
    TEST_ASSERT(snapshots[0].controller.gamepad.brake == 900);
    TEST_ASSERT(snapshots[0].controller.gamepad.throttle == 1023);
    TEST_ASSERT_FLOAT_NEAR(-1.25f, snapshots[0].controller.gamepad.accel[0], 1e-5f);
    TEST_ASSERT_FLOAT_NEAR(UNI_STANDARD_GRAVITY, snapshots[0].controller.gamepad.accel[1], 1e-5f);
    TEST_ASSERT_FLOAT_NEAR(0.5f, snapshots[0].controller.gamepad.accel[2], 1e-5f);
    TEST_ASSERT_FLOAT_NEAR(-0.75f, snapshots[0].controller.gamepad.gyro[0], 1e-5f);
    TEST_ASSERT_FLOAT_NEAR(1.5f, snapshots[0].controller.gamepad.gyro[1], 1e-5f);
    TEST_ASSERT_FLOAT_NEAR(-0.125f, snapshots[0].controller.gamepad.gyro[2], 1e-5f);
    TEST_ASSERT(snapshots[0].report_delta_ms >= 4);
}

/// Test 8: Verifies that `posix_imgui_request_rumble`, `posix_imgui_request_player_leds`,
/// and `posix_imgui_request_lightbar_color` drain through BTstack's callback queue and
/// safely discard commands if the target controller disconnects before draining.
void test_command_queue_draining_and_dangling_pointer_safety() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());

    posix_imgui_reset_for_test();
    reset_mock_calls();
    struct uni_platform* plat = get_posix_imgui_platform();

    uni_hid_device_t d0;
    init_synthetic_device(&d0, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
    d0.report_parser.play_dual_rumble = &mock_play_dual_rumble;
    d0.report_parser.set_player_leds = &mock_set_player_leds;
    d0.report_parser.set_lightbar_color = &mock_set_lightbar_color;

    plat->on_device_connected(&d0);
    TEST_ASSERT(plat->on_device_ready(&d0) == UNI_ERROR_SUCCESS);
    reset_mock_calls();  // Clear the initial set_player_leds call from on_device_ready.

    posix_imgui_request_rumble(0, 50, 500, 128, 255);
    posix_imgui_request_player_leds(0, 0x0A);
    posix_imgui_request_lightbar_color(0, 10, 20, 30);

    btstack_run_loop_base_execute_callbacks();

    TEST_ASSERT(g_mock_calls.rumble_calls == 1);
    TEST_ASSERT(g_mock_calls.last_start_delay_ms == 50);
    TEST_ASSERT(g_mock_calls.last_duration_ms == 500);
    TEST_ASSERT(g_mock_calls.last_weak == 128);
    TEST_ASSERT(g_mock_calls.last_strong == 255);

    TEST_ASSERT(g_mock_calls.player_leds_calls == 1);
    TEST_ASSERT(g_mock_calls.last_leds == 0x0A);

    TEST_ASSERT(g_mock_calls.lightbar_calls == 1);
    TEST_ASSERT(g_mock_calls.last_r == 10);
    TEST_ASSERT(g_mock_calls.last_g == 20);
    TEST_ASSERT(g_mock_calls.last_b == 30);

    // Enqueue another rumble command, disconnect d0 BEFORE draining, then drain callbacks.
    reset_mock_calls();
    posix_imgui_request_rumble(0, 0, 300, 200, 200);
    plat->on_device_disconnected(&d0);
    btstack_run_loop_base_execute_callbacks();

    TEST_ASSERT(g_mock_calls.rumble_calls == 0);
}

// ============================================================================
// Suite D: DualSense Player LEDs 4-Bit Bitmask & Output Report Verification
// ============================================================================

constexpr uint16_t kTestDs5InterruptCid = 0x0040;
constexpr uint16_t kDs5OutputReportLen = 79;

/// Dequeues a single 79-byte `ds5_output_report_t` from `d->outgoing_buffer` and validates
/// all Bluetooth/DualSense framing invariants, sequence tag, player LEDs byte, and CRC32.
bool dequeue_and_verify_ds5_led_report(uni_hid_device_t* d,
                                       uint16_t expected_cid,
                                       uint8_t expected_seq,
                                       uint8_t expected_player_leds,
                                       uint8_t* out_player_leds = nullptr) {
    int16_t cid = 0;
    uint8_t report[128]{};
    int data_len = 0;
    uint8_t rc = uni_circular_buffer_get(&d->outgoing_buffer, &cid, report, &data_len);
    if (rc != UNI_CIRCULAR_BUFFER_ERROR_OK || cid != static_cast<int16_t>(expected_cid) ||
        data_len != kDs5OutputReportLen) {
        return false;
    }

    // Bluetooth HID Output header & DualSense protocol invariants:
    //   Byte 0: transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT = 0xa2
    //   Byte 1: report_id = 0x31
    //   Byte 2: seq_tag = (expected_seq << 4)
    //   Byte 3: tag = 0x10
    //   Byte 4: valid_flag0 = 0x00
    //   Byte 5: valid_flag1 = DS5_FLAG1_PLAYER_LED_CONTROL_ENABLE (BIT(4) = 0x10)
    //   Byte 42: valid_flag2 = 0x00
    //   Byte 47: player_leds (5-LED bitmask in bits 0..4; bits 5..7 must be 0)
    //   Bytes 48..50: lightbar RGB = 0x00
    if (report[0] != 0xa2 || report[1] != 0x31 || report[2] != static_cast<uint8_t>(expected_seq << 4) ||
        report[3] != 0x10 || report[4] != 0x00 || report[5] != 0x10 || report[42] != 0x00) {
        return false;
    }

    const uint8_t actual_player_leds = report[47];
    if (out_player_leds != nullptr) {
        *out_player_leds = actual_player_leds;
    }
    if ((actual_player_leds & 0xe0) != 0x00 || actual_player_leds != expected_player_leds) {
        return false;
    }

    if (report[48] != 0x00 || report[49] != 0x00 || report[50] != 0x00) {
        return false;
    }

    // Verify trailing little-endian CRC32 over the first 75 bytes.
    uint32_t actual_crc32 = 0;
    std::memcpy(&actual_crc32, &report[75], sizeof(actual_crc32));
    const uint32_t expected_crc32 = ~uni_crc32_le(0xffffffff, report, kDs5OutputReportLen - 4);
    return actual_crc32 == expected_crc32;
}

/// Test 9: Verifies `uni_hid_parser_ds5_set_player_leds(nullptr, ...)` returns safely
/// via the NULL-device guard clause without dereferencing `d->parser_data`.
void test_ds5_set_player_leds_null_device_safety() {
    uni_hid_parser_ds5_set_player_leds(nullptr, GAMEPAD_SEAT_NONE);
    uni_hid_parser_ds5_set_player_leds(nullptr, GAMEPAD_SEAT_A);
    uni_hid_parser_ds5_set_player_leds(nullptr, 0xff);
}

/// Test 10: Verifies Sony PS5 symmetric 5-LED patterns for `GAMEPAD_SEAT_NONE` and
/// single-player seats `GAMEPAD_SEAT_A..D`, explicitly guarding against the prior
/// `value % 5` inversion of Player 3 (`GAMEPAD_SEAT_C = 0x04`) and Player 4 (`GAMEPAD_SEAT_D = 0x08`).
void test_ds5_set_player_leds_single_seat_patterns() {
    uni_hid_device_t d;
    init_synthetic_device(&d, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
    d.conn.interrupt_cid = kTestDs5InterruptCid;

    struct SeatCase {
        uint8_t seat_mask;
        uint8_t expected_ds5_leds;
        uint8_t expected_seq;
    };

    constexpr std::array<SeatCase, 5> kCases = {{
        {GAMEPAD_SEAT_NONE, 0x00, 0},                            // -----
        {GAMEPAD_SEAT_A, BIT(2), 1},                             // --X-- (0x04)
        {GAMEPAD_SEAT_B, BIT(1) | BIT(3), 2},                    // -X-X- (0x0a)
        {GAMEPAD_SEAT_C, BIT(0) | BIT(2) | BIT(4), 3},           // X-X-X (0x15, was 0x1b with value % 5)
        {GAMEPAD_SEAT_D, BIT(0) | BIT(1) | BIT(3) | BIT(4), 4},  // XX-XX (0x1b, was 0x15 with value % 5)
    }};

    for (const SeatCase& tc : kCases) {
        uni_hid_parser_ds5_set_player_leds(&d, tc.seat_mask);
        uint8_t actual_leds = 0xff;
        TEST_ASSERT(dequeue_and_verify_ds5_led_report(&d, kTestDs5InterruptCid, tc.expected_seq, tc.expected_ds5_leds,
                                                      &actual_leds));
        if (tc.seat_mask == GAMEPAD_SEAT_C) {
            // Regression assertion: Player 3 (0x04) must NOT produce Player 4's 0x1b pattern.
            TEST_ASSERT(actual_leds == 0x15);
            TEST_ASSERT(actual_leds != 0x1b);
        } else if (tc.seat_mask == GAMEPAD_SEAT_D) {
            // Regression assertion: Player 4 (0x08) must NOT produce Player 3's 0x15 pattern or raw 0x10.
            TEST_ASSERT(actual_leds == 0x1b);
            TEST_ASSERT(actual_leds != 0x15);
            TEST_ASSERT(actual_leds != 0x10);
        }
    }
    TEST_ASSERT(uni_circular_buffer_is_empty(&d.outgoing_buffer));
}

/// Test 11: Exhaustively tests all 16 4-bit bitmasks (`0x00..0x0f`) and verifies that
/// `output_seq` increments across `0..14`, wraps to `0` on the 16th report (`0x0f`),
/// and advances to `1` on the 17th report.
void test_ds5_set_player_leds_all_16_bitmasks_and_sequence_wrap() {
    uni_hid_device_t d;
    init_synthetic_device(&d, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
    d.conn.interrupt_cid = kTestDs5InterruptCid;

    constexpr std::array<uint8_t, 16> kExpectedDs5Leds = {
        0x00,  // 0x00: GAMEPAD_SEAT_NONE  -> -----
        0x04,  // 0x01: GAMEPAD_SEAT_A     -> --X--
        0x0a,  // 0x02: GAMEPAD_SEAT_B     -> -X-X-
        0x03,  // 0x03: SEAT_A | SEAT_B    -> ---XX
        0x15,  // 0x04: GAMEPAD_SEAT_C     -> X-X-X
        0x09,  // 0x05: SEAT_A | SEAT_C    -> -X--X
        0x0a,  // 0x06: SEAT_B | SEAT_C    -> -X-X-
        0x0b,  // 0x07: SEAT_A | B | C     -> -X-XX
        0x1b,  // 0x08: GAMEPAD_SEAT_D     -> XX-XX
        0x11,  // 0x09: SEAT_A | SEAT_D    -> X---X
        0x12,  // 0x0a: SEAT_B | SEAT_D    -> X--X-
        0x13,  // 0x0b: SEAT_A | B | D     -> X--XX
        0x18,  // 0x0c: SEAT_C | SEAT_D    -> XX---
        0x19,  // 0x0d: SEAT_A | C | D     -> XX--X
        0x1a,  // 0x0e: SEAT_B | C | D     -> XX-X-
        0x1b,  // 0x0f: SEAT_A | B | C | D -> XX-XX
    };

    for (uint8_t mask = 0; mask < 16; ++mask) {
        const uint8_t expected_seq = static_cast<uint8_t>(mask % 15);
        uni_hid_parser_ds5_set_player_leds(&d, mask);
        TEST_ASSERT(dequeue_and_verify_ds5_led_report(&d, kTestDs5InterruptCid, expected_seq, kExpectedDs5Leds[mask]));
    }

    // 17th call: after wrapping to 0 on the 16th call (index 15), sequence number must be 1.
    uni_hid_parser_ds5_set_player_leds(&d, GAMEPAD_SEAT_A);
    TEST_ASSERT(dequeue_and_verify_ds5_led_report(&d, kTestDs5InterruptCid, 1, 0x04));
    TEST_ASSERT(uni_circular_buffer_is_empty(&d.outgoing_buffer));
}

/// Test 12: Verifies that when `BIT(7)` is clear (`0x00..0x7f`), dirty bits 4..6 (`0x10..0x70`)
/// are masked off before switching so single-seat patterns never fall into `default:`, and when
/// `BIT(7)` is set (`0x80..0xff`), bits 0..4 (`leds & 0x1f`) directly represent each of the 5
/// physical DualSense LEDs while bits 5..7 never leak into `player_leds`.
void test_ds5_set_player_leds_upper_nibble_masking() {
    constexpr std::array<uint8_t, 16> kExpectedDs5Leds = {
        0x00, 0x04, 0x0a, 0x03, 0x15, 0x09, 0x0a, 0x0b, 0x1b, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,
    };
    constexpr std::array<uint8_t, 5> kHighNibblesBit7Off = {0x10, 0x20, 0x30, 0x50, 0x70};

    for (uint8_t high : kHighNibblesBit7Off) {
        uni_hid_device_t d;
        init_synthetic_device(&d, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
        d.conn.interrupt_cid = kTestDs5InterruptCid;

        for (uint8_t mask = 0; mask < 16; ++mask) {
            const uint8_t dirty_input = static_cast<uint8_t>(high | mask);
            const uint8_t expected_seq = static_cast<uint8_t>(mask % 15);
            uni_hid_parser_ds5_set_player_leds(&d, dirty_input);
            TEST_ASSERT(
                dequeue_and_verify_ds5_led_report(&d, kTestDs5InterruptCid, expected_seq, kExpectedDs5Leds[mask]));
        }
        TEST_ASSERT(uni_circular_buffer_is_empty(&d.outgoing_buffer));
    }

    // When BIT(7) (0x80) is set, bits 0..4 (0x00..0x1f) directly represent each of the 5 LEDs,
    // and bits 5..7 (0xe0) must be masked off before populating `out.player_leds`.
    constexpr std::array<uint8_t, 4> kBit7Prefixes = {0x80, 0xa0, 0xc0, 0xe0};
    for (uint8_t prefix : kBit7Prefixes) {
        uni_hid_device_t d;
        init_synthetic_device(&d, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
        d.conn.interrupt_cid = kTestDs5InterruptCid;

        for (uint8_t raw_5bit = 0; raw_5bit < 32; ++raw_5bit) {
            const uint8_t input = static_cast<uint8_t>(prefix | raw_5bit);
            const uint8_t expected_seq = static_cast<uint8_t>(raw_5bit % 15);
            uni_hid_parser_ds5_set_player_leds(&d, input);
            TEST_ASSERT(dequeue_and_verify_ds5_led_report(&d, kTestDs5InterruptCid, expected_seq, raw_5bit));
        }
        TEST_ASSERT(uni_circular_buffer_is_empty(&d.outgoing_buffer));
    }
}

/// Test 13: Verifies end-to-end integration between `posix_imgui_platform` and the real
/// `uni_hid_parser_ds5_set_player_leds` across all 4 controller slots (`GAMEPAD_SEAT_A..D`)
/// plus runtime LED override via `posix_imgui_request_player_leds`.
void test_ds5_player_leds_end_to_end_via_posix_imgui_platform() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    struct uni_platform* plat = get_posix_imgui_platform();
    constexpr std::array<uint8_t, kMaxControllers> kExpectedSeatLeds = {
        0x04,  // Slot 0 (GAMEPAD_SEAT_A): --X--
        0x0a,  // Slot 1 (GAMEPAD_SEAT_B): -X-X-
        0x15,  // Slot 2 (GAMEPAD_SEAT_C): X-X-X
        0x1b,  // Slot 3 (GAMEPAD_SEAT_D): XX-XX
    };

    std::array<uni_hid_device_t, kMaxControllers> devices{};
    for (int i = 0; i < kMaxControllers; ++i) {
        const auto idx = static_cast<size_t>(i);
        init_synthetic_device(&devices[idx], 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
        devices[idx].conn.interrupt_cid = static_cast<uint16_t>(kTestDs5InterruptCid + i);
        devices[idx].report_parser.set_player_leds = &uni_hid_parser_ds5_set_player_leds;

        plat->on_device_connected(&devices[idx]);
        TEST_ASSERT(plat->on_device_ready(&devices[idx]) == UNI_ERROR_SUCCESS);
        TEST_ASSERT(dequeue_and_verify_ds5_led_report(&devices[idx], static_cast<uint16_t>(kTestDs5InterruptCid + i), 0,
                                                      kExpectedSeatLeds[idx]));
        TEST_ASSERT(uni_circular_buffer_is_empty(&devices[idx].outgoing_buffer));
    }

    // Override Slot 2 (Player 3) LEDs at runtime with multi-bit mask GAMEPAD_SEAT_AB_MASK (0x03).
    posix_imgui_request_player_leds(2, GAMEPAD_SEAT_AB_MASK);
    btstack_run_loop_base_execute_callbacks();

    TEST_ASSERT(
        dequeue_and_verify_ds5_led_report(&devices[2], static_cast<uint16_t>(kTestDs5InterruptCid + 2), 1, 0x03));
    TEST_ASSERT(uni_circular_buffer_is_empty(&devices[2].outgoing_buffer));
}

// ============================================================================
// Suite E: Device Auto-Accept Filtering & Virtual Device Toggle
// ============================================================================

/// Test 14: Verifies default allowed physical device types (`POSIX_IMGUI_DEVICE_TYPE_GAMEPAD` only),
/// `on_device_discovered` CoD filtering, and `on_device_ready` physical device filtering.
void test_device_discovery_and_ready_filtering_by_device_type() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    struct uni_platform* plat = get_posix_imgui_platform();
    TEST_ASSERT(posix_imgui_get_allowed_device_types() == POSIX_IMGUI_DEVICE_TYPE_GAMEPAD);

    bd_addr_t addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const uint16_t kCodGamepad = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD;
    const uint16_t kCodJoystick = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_JOYSTICK;
    const uint16_t kCodMouse = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE;
    const uint16_t kCodKeyboard = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD;

    // Default filter: Gamepads and Joysticks accepted, Mice and Keyboards rejected.
    TEST_ASSERT(plat->on_device_discovered(addr, "Gamepad", kCodGamepad, 200) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(plat->on_device_discovered(addr, "Joystick", kCodJoystick, 200) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(plat->on_device_discovered(addr, "Mouse", kCodMouse, 200) == UNI_ERROR_IGNORE_DEVICE);
    TEST_ASSERT(plat->on_device_discovered(addr, "Keyboard", kCodKeyboard, 200) == UNI_ERROR_IGNORE_DEVICE);

    // Physical mouse and keyboard must also be rejected in on_device_ready() under default filter.
    uni_hid_device_t mouse_dev;
    init_synthetic_device(&mouse_dev, 0x046d, 0xb019, CONTROLLER_TYPE_GenericMouse, "BT Mouse");
    uni_hid_device_set_cod(&mouse_dev, kCodMouse);
    plat->on_device_connected(&mouse_dev);
    TEST_ASSERT(plat->on_device_ready(&mouse_dev) == UNI_ERROR_IGNORE_DEVICE);
    TEST_ASSERT(get_posix_imgui_instance(&mouse_dev)->slot == -1);

    uni_hid_device_t kb_dev;
    init_synthetic_device(&kb_dev, 0x046d, 0xb342, CONTROLLER_TYPE_GenericKeyboard, "BT Keyboard");
    uni_hid_device_set_cod(&kb_dev, kCodKeyboard);
    plat->on_device_connected(&kb_dev);
    TEST_ASSERT(plat->on_device_ready(&kb_dev) == UNI_ERROR_IGNORE_DEVICE);
    TEST_ASSERT(get_posix_imgui_instance(&kb_dev)->slot == -1);

    // Enable Mice and Keyboards, disable Gamepads.
    posix_imgui_request_set_allowed_device_types(POSIX_IMGUI_DEVICE_TYPE_MOUSE | POSIX_IMGUI_DEVICE_TYPE_KEYBOARD);
    btstack_run_loop_base_execute_callbacks();
    TEST_ASSERT(posix_imgui_get_allowed_device_types() ==
                (POSIX_IMGUI_DEVICE_TYPE_MOUSE | POSIX_IMGUI_DEVICE_TYPE_KEYBOARD));

    TEST_ASSERT(plat->on_device_discovered(addr, "Gamepad", kCodGamepad, 200) == UNI_ERROR_IGNORE_DEVICE);
    TEST_ASSERT(plat->on_device_discovered(addr, "Mouse", kCodMouse, 200) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(plat->on_device_discovered(addr, "Keyboard", kCodKeyboard, 200) == UNI_ERROR_SUCCESS);

    plat->on_device_connected(&mouse_dev);
    TEST_ASSERT(plat->on_device_ready(&mouse_dev) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(get_posix_imgui_instance(&mouse_dev)->slot == 0);

    plat->on_device_connected(&kb_dev);
    TEST_ASSERT(plat->on_device_ready(&kb_dev) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(get_posix_imgui_instance(&kb_dev)->slot == 1);
}

/// Test 15: Verifies that unchecking an input device category at runtime immediately
/// disconnects any currently connected physical devices of that disabled category while
/// preserving connected devices of still-enabled categories.
void test_runtime_disconnection_when_device_category_disabled() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    struct uni_platform* plat = get_posix_imgui_platform();
    posix_imgui_request_set_allowed_device_types(POSIX_IMGUI_DEVICE_TYPE_GAMEPAD | POSIX_IMGUI_DEVICE_TYPE_MOUSE |
                                                 POSIX_IMGUI_DEVICE_TYPE_KEYBOARD);
    btstack_run_loop_base_execute_callbacks();

    uni_hid_device_t pad;
    init_synthetic_device(&pad, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
    uni_hid_device_set_cod(&pad, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD);
    plat->on_device_connected(&pad);
    TEST_ASSERT(plat->on_device_ready(&pad) == UNI_ERROR_SUCCESS);

    uni_hid_device_t mouse;
    init_synthetic_device(&mouse, 0x046d, 0xb019, CONTROLLER_TYPE_GenericMouse, "Mouse");
    uni_hid_device_set_cod(&mouse, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
    plat->on_device_connected(&mouse);
    TEST_ASSERT(plat->on_device_ready(&mouse) == UNI_ERROR_SUCCESS);

    uni_hid_device_t kb;
    init_synthetic_device(&kb, 0x046d, 0xb342, CONTROLLER_TYPE_GenericKeyboard, "Keyboard");
    uni_hid_device_set_cod(&kb, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD);
    plat->on_device_connected(&kb);
    TEST_ASSERT(plat->on_device_ready(&kb) == UNI_ERROR_SUCCESS);

    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].connected && snapshots[1].connected && snapshots[2].connected);

    // Disable Mouse and Keyboard -> Slot 1 and Slot 2 must disconnect immediately; Slot 0 stays connected.
    posix_imgui_request_set_allowed_device_types(POSIX_IMGUI_DEVICE_TYPE_GAMEPAD);
    btstack_run_loop_base_execute_callbacks();

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(!snapshots[1].connected);
    TEST_ASSERT(!snapshots[2].connected);
}

/// Test 16: Verifies that virtual child devices (e.g., DualSense / DualShock 4 touchpad mouse)
/// are disabled by default, can be enabled independently of physical mice, do not steal tab
/// focus from the parent gamepad, and are immediately disconnected when toggled off at runtime.
void test_virtual_device_disabled_by_default_and_runtime_toggle() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    struct uni_platform* plat = get_posix_imgui_platform();
    TEST_ASSERT(!posix_imgui_is_virtual_devices_enabled());
    TEST_ASSERT(!uni_virtual_device_is_enabled());
    TEST_ASSERT(posix_imgui_get_allowed_device_types() == POSIX_IMGUI_DEVICE_TYPE_GAMEPAD);

    // Connect parent DualSense in Slot 0.
    uni_hid_device_t parent_ds5;
    init_synthetic_device(&parent_ds5, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
    uni_hid_device_set_cod(&parent_ds5, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD);
    plat->on_device_connected(&parent_ds5);
    TEST_ASSERT(plat->on_device_ready(&parent_ds5) == UNI_ERROR_SUCCESS);

    // Virtual child mouse must be rejected while Virtual Devices are disabled.
    uni_hid_device_t virtual_mouse;
    init_synthetic_device(&virtual_mouse, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "virtual-1");
    uni_hid_device_set_cod(&virtual_mouse, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
    virtual_mouse.parent = &parent_ds5;
    parent_ds5.child = &virtual_mouse;

    plat->on_device_connected(&virtual_mouse);
    TEST_ASSERT(plat->on_device_ready(&virtual_mouse) == UNI_ERROR_IGNORE_DEVICE);
    TEST_ASSERT(get_posix_imgui_instance(&virtual_mouse)->slot == -1);

    // Enable Virtual Devices (while physical Mice remain disabled).
    posix_imgui_request_set_virtual_devices_enabled(true);
    btstack_run_loop_base_execute_callbacks();
    TEST_ASSERT(posix_imgui_is_virtual_devices_enabled());
    TEST_ASSERT(uni_virtual_device_is_enabled());

    // Clear the newly_connected_slot latch from parent_ds5 before connecting virtual_mouse.
    std::array<ControllerSnapshot, kMaxControllers> snapshots{};
    int newly_connected = -1;
    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(newly_connected == 0);

    // Virtual child mouse must now be accepted into Slot 1 even though physical Mice are disabled,
    // and must NOT overwrite `newly_connected_slot`.
    plat->on_device_connected(&virtual_mouse);
    TEST_ASSERT(plat->on_device_ready(&virtual_mouse) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(get_posix_imgui_instance(&virtual_mouse)->slot == 1);

    posix_imgui_get_snapshots(std::span{snapshots}, &newly_connected);
    TEST_ASSERT(newly_connected == -1);
    TEST_ASSERT(snapshots[0].connected && !snapshots[0].is_virtual_device);
    TEST_ASSERT(snapshots[1].connected && snapshots[1].is_virtual_device);
    TEST_ASSERT(snapshots[1].controller.klass == UNI_CONTROLLER_CLASS_MOUSE);

    // Disable Virtual Devices at runtime -> Slot 1 (virtual_mouse) must immediately disconnect
    // and unlink from parent_ds5, while Slot 0 (parent_ds5) stays connected!
    posix_imgui_request_set_virtual_devices_enabled(false);
    btstack_run_loop_base_execute_callbacks();
    TEST_ASSERT(!posix_imgui_is_virtual_devices_enabled());
    TEST_ASSERT(!uni_virtual_device_is_enabled());

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(!snapshots[1].connected);
    TEST_ASSERT(parent_ds5.child == nullptr);
}

// ============================================================================
// Suite F: C++23 std::span Overloads & std::variant Command Queue Verification
// ============================================================================

/// Test 17: Verifies both the C++23 `std::span<ControllerSnapshot, kMaxControllers>` overload
/// (including single-argument default `newly_connected_slot = nullptr`) and the legacy pointer
/// overload (`nullptr` destination array and raw C array).
void test_get_snapshots_span_and_pointer_overloads() {
    posix_imgui_reset_for_test();
    struct uni_platform* plat = get_posix_imgui_platform();

    uni_hid_device_t d0;
    init_synthetic_device(&d0, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense Slot 0");
    plat->on_device_connected(&d0);
    TEST_ASSERT(plat->on_device_ready(&d0) == UNI_ERROR_SUCCESS);

    uni_hid_device_t d1;
    init_synthetic_device(&d1, 0x057e, 0x2009, CONTROLLER_TYPE_SwitchProController, "Switch Pro Slot 1");
    plat->on_device_connected(&d1);
    TEST_ASSERT(plat->on_device_ready(&d1) == UNI_ERROR_SUCCESS);

    // 1. Legacy pointer overload with out_snapshots == nullptr must latch and clear newly_connected_slot.
    int newly_connected = -1;
    posix_imgui_get_snapshots(nullptr, &newly_connected);
    TEST_ASSERT(newly_connected == 1);

    posix_imgui_get_snapshots(nullptr, &newly_connected);
    TEST_ASSERT(newly_connected == -1);

    // 2. Single-argument std::span overload (using default newly_connected_slot = nullptr) vs. raw C array overload.
    std::array<ControllerSnapshot, kMaxControllers> snapshots_span{};
    posix_imgui_get_snapshots(std::span<ControllerSnapshot, kMaxControllers>{snapshots_span});

    ControllerSnapshot snapshots_c_array[kMaxControllers] = {};
    posix_imgui_get_snapshots(snapshots_c_array, &newly_connected);
    TEST_ASSERT(newly_connected == -1);

    TEST_ASSERT(snapshots_span[0].connected);
    TEST_ASSERT(snapshots_span[1].connected);
    TEST_ASSERT(!snapshots_span[2].connected);
    TEST_ASSERT(!snapshots_span[3].connected);

    for (int i = 0; i < kMaxControllers; ++i) {
        const auto idx = static_cast<size_t>(i);
        TEST_ASSERT(snapshots_span[idx].connected == snapshots_c_array[i].connected);
        TEST_ASSERT(snapshots_span[idx].vendor_id == snapshots_c_array[i].vendor_id);
        TEST_ASSERT(snapshots_span[idx].product_id == snapshots_c_array[i].product_id);
        TEST_ASSERT(snapshots_span[idx].layout == snapshots_c_array[i].layout);
        TEST_ASSERT(std::strcmp(snapshots_span[idx].name, snapshots_c_array[i].name) == 0);
    }
}

/// Test 18: Verifies `std::variant` command queue FIFO batch draining via `std::visit`,
/// out-of-bounds slot rejection (`-1`, `kMaxControllers`, `99`), upper-nibble LED masking,
/// global state commands (`SetVirtualDevicesCmd`, `SetAllowedDeviceTypesCmd`), and headless
/// `ShutdownCmd` safety when `hci_init()` has not been called.
void test_variant_command_queue_ordering_batch_draining_and_bounds_rejection() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();
    reset_mock_calls();

    struct uni_platform* plat = get_posix_imgui_platform();

    uni_hid_device_t d0;
    init_synthetic_device(&d0, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense #0");
    d0.report_parser.play_dual_rumble = &mock_play_dual_rumble;
    d0.report_parser.set_player_leds = &mock_set_player_leds;
    d0.report_parser.set_lightbar_color = &mock_set_lightbar_color;
    plat->on_device_connected(&d0);
    TEST_ASSERT(plat->on_device_ready(&d0) == UNI_ERROR_SUCCESS);

    uni_hid_device_t d1;
    init_synthetic_device(&d1, 0x057e, 0x2009, CONTROLLER_TYPE_SwitchProController, "Switch Pro #1");
    d1.report_parser.play_dual_rumble = &mock_play_dual_rumble;
    d1.report_parser.set_player_leds = &mock_set_player_leds;
    d1.report_parser.set_lightbar_color = &mock_set_lightbar_color;
    plat->on_device_connected(&d1);
    TEST_ASSERT(plat->on_device_ready(&d1) == UNI_ERROR_SUCCESS);

    // Clear initial set_player_leds calls triggered during on_device_ready().
    reset_mock_calls();
    TEST_ASSERT(!posix_imgui_is_virtual_devices_enabled());
    TEST_ASSERT(posix_imgui_get_allowed_device_types() == POSIX_IMGUI_DEVICE_TYPE_GAMEPAD);
    TEST_ASSERT(!posix_imgui_is_shutdown_requested());

    // 1. Enqueue out-of-bounds slot commands (-1, kMaxControllers = 4, 99) across all per-slot command types.
    for (int invalid_slot : {-1, kMaxControllers, 99}) {
        posix_imgui_request_rumble(invalid_slot, 10, 100, 50, 50);
        posix_imgui_request_player_leds(invalid_slot, 0xff);
        posix_imgui_request_lightbar_color(invalid_slot, 255, 0, 0);
    }

    // 2. Enqueue valid per-slot and global commands in a single batch before draining.
    posix_imgui_request_rumble(0, 10, 250, 64, 192);
    posix_imgui_request_player_leds(1, 0xF5);  // Dirty upper nibble -> must mask to 0x05
    posix_imgui_request_lightbar_color(0, 12, 34, 56);
    posix_imgui_request_set_virtual_devices_enabled(true);
    posix_imgui_request_set_allowed_device_types(POSIX_IMGUI_DEVICE_TYPE_GAMEPAD | POSIX_IMGUI_DEVICE_TYPE_MOUSE);
    posix_imgui_request_shutdown();

    // 3. Drain the entire command batch in one run-loop callback execution.
    btstack_run_loop_base_execute_callbacks();

    // Verify all 9 out-of-bounds commands were rejected and the 3 valid per-slot commands ran in exact FIFO order.
    TEST_ASSERT(g_mock_calls.rumble_calls == 1);
    TEST_ASSERT(g_mock_calls.player_leds_calls == 1);
    TEST_ASSERT(g_mock_calls.lightbar_calls == 1);
    TEST_ASSERT(g_command_trace.size() == 3);
    TEST_ASSERT(g_command_trace[0] == "rumble:slot=0:delay=10:dur=250:weak=64:strong=192");
    TEST_ASSERT(g_command_trace[1] == "leds:slot=1:mask=0x05");
    TEST_ASSERT(g_command_trace[2] == "lightbar:slot=0:r=12:g=34:b=56");

    // Verify global state commands and headless ShutdownCmd completed cleanly.
    TEST_ASSERT(posix_imgui_is_virtual_devices_enabled());
    TEST_ASSERT(uni_virtual_device_is_enabled());
    TEST_ASSERT(posix_imgui_get_allowed_device_types() ==
                (POSIX_IMGUI_DEVICE_TYPE_GAMEPAD | POSIX_IMGUI_DEVICE_TYPE_MOUSE));
    TEST_ASSERT(posix_imgui_is_shutdown_requested());
}

// ============================================================================
// Suite G: DemoScene Headless ImGui Frame & ControllerSlotUiState Lifecycle
// ============================================================================

/// Test 19: Verifies headless `DemoScene::DoFrame()` across all 4 controller classes
/// (Gamepad, Balance Board, Mouse, Keyboard), all 5 category sub-tabs (`0..4`) plus an
/// out-of-range tab index (`5`), the Preferences tab, and 4 DPI/font scales
/// (`0.50f`, `1.00f`, `2.00f`, `4.00f`) with zero `IM_ASSERT` failures and valid draw lists.
void test_demo_scene_headless_all_tabs_and_font_scales() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    // Enable all physical device types so Gamepad, Balance Board, Mouse, and Keyboard can connect simultaneously.
    posix_imgui_request_set_allowed_device_types(POSIX_IMGUI_DEVICE_TYPE_GAMEPAD | POSIX_IMGUI_DEVICE_TYPE_MOUSE |
                                                 POSIX_IMGUI_DEVICE_TYPE_KEYBOARD);
    btstack_run_loop_base_execute_callbacks();

    struct uni_platform* plat = get_posix_imgui_platform();

    // Slot 0: DualSense Gamepad with IMU, sticks, triggers, buttons, D-pad, and misc buttons.
    uni_hid_device_t d_pad;
    init_synthetic_device(&d_pad, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense Wireless Controller");
    uni_hid_device_set_cod(&d_pad, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD);
    plat->on_device_connected(&d_pad);
    TEST_ASSERT(plat->on_device_ready(&d_pad) == UNI_ERROR_SUCCESS);

    uni_controller_t pad_ctl = {};
    pad_ctl.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    pad_ctl.battery = 200;
    pad_ctl.gamepad.dpad = DPAD_UP | DPAD_RIGHT;
    pad_ctl.gamepad.axis_x = 256;
    pad_ctl.gamepad.axis_y = -300;
    pad_ctl.gamepad.axis_rx = -180;
    pad_ctl.gamepad.axis_ry = 410;
    pad_ctl.gamepad.brake = 640;
    pad_ctl.gamepad.throttle = 820;
    pad_ctl.gamepad.buttons = BUTTON_A | BUTTON_X | BUTTON_SHOULDER_L | BUTTON_THUMB_R;
    pad_ctl.gamepad.misc_buttons = MISC_BUTTON_SYSTEM | MISC_BUTTON_SELECT | MISC_BUTTON_START | MISC_BUTTON_CAPTURE;
    pad_ctl.gamepad.gyro[0] = 120;
    pad_ctl.gamepad.gyro[1] = -85;
    pad_ctl.gamepad.gyro[2] = 45;
    pad_ctl.gamepad.accel[0] = 50;
    pad_ctl.gamepad.accel[1] = 980;
    pad_ctl.gamepad.accel[2] = -110;
    plat->on_controller_data(&d_pad, &pad_ctl);

    // Slot 1: Wii Balance Board.
    uni_hid_device_t d_bb;
    init_synthetic_device(&d_bb, 0x057e, 0x0306, CONTROLLER_TYPE_WiiController, "Nintendo RVL-WBC-01");
    uni_hid_device_set_cod(&d_bb, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_JOYSTICK);
    plat->on_device_connected(&d_bb);
    TEST_ASSERT(plat->on_device_ready(&d_bb) == UNI_ERROR_SUCCESS);

    uni_controller_t bb_ctl = {};
    bb_ctl.klass = UNI_CONTROLLER_CLASS_BALANCE_BOARD;
    bb_ctl.battery = 180;
    bb_ctl.balance_board.tl = 1350;
    bb_ctl.balance_board.tr = 1250;
    bb_ctl.balance_board.bl = 1400;
    bb_ctl.balance_board.br = 1100;
    bb_ctl.balance_board.temperature = 23;
    plat->on_controller_data(&d_bb, &bb_ctl);

    // Slot 2: Generic Bluetooth Mouse.
    uni_hid_device_t d_mouse;
    init_synthetic_device(&d_mouse, 0x046d, 0xb019, CONTROLLER_TYPE_GenericMouse, "MX Master 3S");
    uni_hid_device_set_cod(&d_mouse, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
    plat->on_device_connected(&d_mouse);
    TEST_ASSERT(plat->on_device_ready(&d_mouse) == UNI_ERROR_SUCCESS);

    uni_controller_t mouse_ctl = {};
    mouse_ctl.klass = UNI_CONTROLLER_CLASS_MOUSE;
    mouse_ctl.battery = 220;
    mouse_ctl.mouse.delta_x = 18;
    mouse_ctl.mouse.delta_y = -12;
    mouse_ctl.mouse.buttons = 0x05;
    mouse_ctl.mouse.scroll_wheel = -2;
    plat->on_controller_data(&d_mouse, &mouse_ctl);

    // Slot 3: Generic Bluetooth Keyboard.
    uni_hid_device_t d_kb;
    init_synthetic_device(&d_kb, 0x046d, 0xb342, CONTROLLER_TYPE_GenericKeyboard, "Keychron K2");
    uni_hid_device_set_cod(&d_kb, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD);
    plat->on_device_connected(&d_kb);
    TEST_ASSERT(plat->on_device_ready(&d_kb) == UNI_ERROR_SUCCESS);

    uni_controller_t kb_ctl = {};
    kb_ctl.klass = UNI_CONTROLLER_CLASS_KEYBOARD;
    kb_ctl.battery = 250;
    kb_ctl.keyboard.modifiers = 0x05;
    kb_ctl.keyboard.pressed_keys[0] = 0x04;  // 'A'
    kb_ctl.keyboard.pressed_keys[1] = 0x05;  // 'B'
    kb_ctl.keyboard.pressed_keys[2] = 0x28;  // Enter
    plat->on_controller_data(&d_kb, &kb_ctl);

    // Create headless Dear ImGui 1.93.0 WIP context with pre-built font atlas.
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    TEST_ASSERT(ctx != nullptr);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.0f, 800.0f);
    io.DeltaTime = 1.0f / 60.0f;
    // Why `io.Fonts->Build()`:
    //   In headless unit tests without an OpenGL renderer backend (`ImGui_ImplOpenGL3_NewFrame()`),
    //   we must explicitly build the CPU font atlas so `ImGui::NewFrame()` and `ImGui::CalcTextSize()`
    //   have valid glyph metrics and do not trigger `IM_ASSERT(io.Fonts->IsBuilt())`.
    io.Fonts->Build();

    DemoScene demo_scene;
    ImGuiStyle& style = ImGui::GetStyle();

    constexpr std::array<float, 4> kFontScales = {0.50f, 1.00f, 2.00f, 4.00f};
    for (float scale : kFontScales) {
        style.FontScaleMain = scale;

        // Exercise all 5 category sub-tabs (0..4: Controls, Rumble, IMU, Lights, Info)
        // plus an out-of-range tab index (5) to verify safe fallback to the active tab.
        for (int tab = 0; tab < 6; ++tab) {
            demo_scene.SetPreferencesActiveForTest(false);
            demo_scene.SelectCategoryTabForTest(tab);

            ImGui::NewFrame();
            demo_scene.DoFrame();
            ImGui::Render();

            ImDrawData* draw_data = ImGui::GetDrawData();
            TEST_ASSERT(draw_data != nullptr);
            TEST_ASSERT(draw_data->Valid);
            TEST_ASSERT(draw_data->CmdListsCount > 0);
        }

        // Exercise Preferences tab at each font scale.
        demo_scene.SetPreferencesActiveForTest(true);
        ImGui::NewFrame();
        demo_scene.DoFrame();
        ImGui::Render();

        ImDrawData* pref_draw_data = ImGui::GetDrawData();
        TEST_ASSERT(pref_draw_data != nullptr);
        TEST_ASSERT(pref_draw_data->Valid);
        TEST_ASSERT(pref_draw_data->CmdListsCount > 0);
    }

    ImGui::DestroyContext(ctx);
}

/// Test 20: Verifies `ControllerSlotUiState::ResetForSlot(slot)` per-slot defaults and
/// verifies that `DemoScene::DoFrame()` automatically resets a slot's UI state on a
/// `disconnected -> connected` transition so stale rumble, LEDs, brightness, or IMU history
/// never bleed into a newly connected controller.
void test_demo_scene_slot_ui_state_reset_on_disconnect_and_reconnect() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    // 1. Verify direct ResetForSlot(0..3) defaults.
    for (int slot = 0; slot < kMaxControllers; ++slot) {
        ControllerSlotUiState state;
        state.rumble_weak_intensity = 0.95f;
        state.rumble_strong_intensity = 0.90f;
        state.rumble_duration_ms = 2500.0f;
        state.trigger_rumble_enabled = true;
        state.brightness_percent = 25;
        state.ResetForSlot(slot);

        TEST_ASSERT(std::fabs(state.rumble_duration_ms - 1000.0f) < 1e-5f);
        TEST_ASSERT(std::fabs(state.rumble_weak_intensity - 0.40f) < 1e-5f);
        TEST_ASSERT(std::fabs(state.rumble_strong_intensity - 0.80f) < 1e-5f);
        TEST_ASSERT(!state.trigger_rumble_enabled);
        TEST_ASSERT(!state.trigger_rumble_active);
        TEST_ASSERT(state.player_led_index == slot + 1);
        for (int b = 0; b < 4; ++b) {
            TEST_ASSERT(state.player_led_bits[static_cast<size_t>(b)] == (b == slot));
        }
        TEST_ASSERT(std::fabs(state.rgb_color[0] - 0.0f) < 1e-5f);
        TEST_ASSERT(std::fabs(state.rgb_color[1] - 0.25f) < 1e-5f);
        TEST_ASSERT(std::fabs(state.rgb_color[2] - 1.0f) < 1e-5f);
        TEST_ASSERT(!state.rgb_live_update);
        TEST_ASSERT(state.brightness_percent == 100);
        TEST_ASSERT(std::strcmp(state.last_detected_input.data(), "None") == 0);
    }

    struct uni_platform* plat = get_posix_imgui_platform();

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    TEST_ASSERT(ctx != nullptr);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.0f, 800.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->Build();

    DemoScene demo_scene;

    // 2. Connect Controller #1 in Slot 0 and run one frame.
    uni_hid_device_t d0;
    init_synthetic_device(&d0, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense First");
    plat->on_device_connected(&d0);
    TEST_ASSERT(plat->on_device_ready(&d0) == UNI_ERROR_SUCCESS);

    uni_controller_t ctl = {};
    ctl.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    ctl.gamepad.buttons = BUTTON_A;
    ctl.gamepad.gyro[0] = 300;
    plat->on_controller_data(&d0, &ctl);

    ImGui::NewFrame();
    demo_scene.DoFrame();
    ImGui::Render();

    TEST_ASSERT(demo_scene.GetSlotStateForTest(0).prev_connected);
    TEST_ASSERT(demo_scene.GetSlotStateForTest(0).imu_history_offset == 1);
    TEST_ASSERT(std::strcmp(demo_scene.GetSlotStateForTest(0).last_detected_input.data(), "None") != 0);

    // 3. Mutate Slot 0 UI controls to non-default values while connected.
    ControllerSlotUiState& slot0 = demo_scene.MutateSlotStateForTest(0);
    slot0.rumble_weak_intensity = 0.95f;
    slot0.rumble_strong_intensity = 0.90f;
    slot0.rumble_duration_ms = 2500.0f;
    slot0.trigger_rumble_enabled = true;
    slot0.trigger_rumble_active = true;
    slot0.last_trigger_strong_u8 = 200;
    slot0.last_trigger_weak_u8 = 180;
    slot0.player_led_index = 4;
    slot0.player_led_bits = {true, true, true, true};
    slot0.rgb_color = {0.25f, 0.50f, 0.75f};
    slot0.rgb_live_update = true;
    slot0.brightness_percent = 35;

    // 4. Disconnect Slot 0 and run one frame so `prev_connected` transitions to `false`.
    plat->on_device_disconnected(&d0);
    ImGui::NewFrame();
    demo_scene.DoFrame();
    ImGui::Render();
    TEST_ASSERT(!demo_scene.GetSlotStateForTest(0).prev_connected);

    // 5. Reconnect a new controller into Slot 0 and run one frame.
    uni_hid_device_t d0_reconnected;
    init_synthetic_device(&d0_reconnected, 0x057e, 0x2009, CONTROLLER_TYPE_SwitchProController, "Switch Pro Second");
    plat->on_device_connected(&d0_reconnected);
    TEST_ASSERT(plat->on_device_ready(&d0_reconnected) == UNI_ERROR_SUCCESS);

    ImGui::NewFrame();
    demo_scene.DoFrame();
    ImGui::Render();

    // 6. Verify all stale Slot 0 UI state was automatically reset by `ResetForSlot(0)` before frame update.
    const ControllerSlotUiState& reset_slot0 = demo_scene.GetSlotStateForTest(0);
    TEST_ASSERT(reset_slot0.prev_connected);
    TEST_ASSERT(std::fabs(reset_slot0.rumble_duration_ms - 1000.0f) < 1e-5f);
    TEST_ASSERT(std::fabs(reset_slot0.rumble_weak_intensity - 0.40f) < 1e-5f);
    TEST_ASSERT(std::fabs(reset_slot0.rumble_strong_intensity - 0.80f) < 1e-5f);
    TEST_ASSERT(!reset_slot0.trigger_rumble_enabled);
    TEST_ASSERT(!reset_slot0.trigger_rumble_active);
    TEST_ASSERT(reset_slot0.last_trigger_strong_u8 == 0);
    TEST_ASSERT(reset_slot0.last_trigger_weak_u8 == 0);
    TEST_ASSERT(reset_slot0.player_led_index == 1);
    TEST_ASSERT(reset_slot0.player_led_bits[0] && !reset_slot0.player_led_bits[1] && !reset_slot0.player_led_bits[2] &&
                !reset_slot0.player_led_bits[3]);
    TEST_ASSERT(std::fabs(reset_slot0.rgb_color[0] - 0.0f) < 1e-5f);
    TEST_ASSERT(std::fabs(reset_slot0.rgb_color[1] - 0.25f) < 1e-5f);
    TEST_ASSERT(std::fabs(reset_slot0.rgb_color[2] - 1.0f) < 1e-5f);
    TEST_ASSERT(!reset_slot0.rgb_live_update);
    TEST_ASSERT(reset_slot0.brightness_percent == 100);
    TEST_ASSERT(reset_slot0.imu_history_offset == 0);
    TEST_ASSERT(std::strcmp(reset_slot0.last_detected_input.data(), "None") == 0);

    ImGui::DestroyContext(ctx);
}

// ============================================================================
// Suite H: device_extra_info Parser Callbacks, Snapshot Propagation & Info Tab
// ============================================================================

/**
 * @brief Layout-compatible prefix of `wii_instance_t` (`uni_hid_parser_wii.c`).
 *
 * Why a prefix struct is used here:
 *   `wii_instance_t` is private to `uni_hid_parser_wii.c` and mapped onto `d->parser_data[0]`.
 *   Matching the exact leading field layout (`state`, `register_address`, `mode`, `dev_type`,
 *   `ext_type`) lets unit tests inject both valid (`WII_DEVTYPE_REMOTE_MP`, `WII_EXT_NUNCHUK`)
 *   and out-of-bounds positive/negative enum values (`99`, `255`, `-1`) to verify defensive
 *   lookup-table bounds checking in `uni_hid_parser_wii_device_extra_info()`.
 */
struct TestWiiParserPrefix {
    uint8_t state;
    uint8_t register_address;
    wii_mode_t mode;
    int dev_type;
    int ext_type;
};

/**
 * @brief Layout-compatible prefix of `xboxone_instance_t` (`uni_hid_parser_xboxone.c`).
 *
 * Matches the leading `version` (`xboxone_firmware_t`) field mapped at `d->parser_data[0]`
 * so unit tests can inject out-of-bounds positive/negative enum values (`99`, `-1`) and
 * verify that `uni_hid_parser_xboxone_device_extra_info()` safely writes `buf[0] = '\0'`
 * and returns `0`.
 */
struct TestXboxOneParserPrefix {
    int version;
};

/// Test 21: Verifies NULL/zero-length guard clauses (`d == nullptr`, `buf == nullptr`, `len == 0`)
/// and small-buffer `snprintf` truncation (`len == 1` and `len == 8`) across all 10 parser
/// `uni_hid_parser_*_device_extra_info` functions.
void test_parser_device_extra_info_null_and_small_buffer_guards() {
    constexpr std::array<report_device_extra_info_fn_t, 10> kAllCallbacks = {
        &uni_hid_parser_ds4_device_extra_info,          &uni_hid_parser_ds5_device_extra_info,
        &uni_hid_parser_switch_device_extra_info,       &uni_hid_parser_switch2_device_extra_info,
        &uni_hid_parser_steam_triton_device_extra_info, &uni_hid_parser_wii_device_extra_info,
        &uni_hid_parser_xboxone_device_extra_info,      &uni_hid_parser_sinput_device_extra_info,
        &uni_hid_parser_mouse_device_extra_info,        &uni_hid_parser_keyboard_device_extra_info,
    };

    uni_hid_device_t d;
    init_synthetic_device(&d, 0x054c, 0x09cc, CONTROLLER_TYPE_PS4Controller, "GuardTest");
    char buf[64]{};

    for (report_device_extra_info_fn_t fn : kAllCallbacks) {
        TEST_ASSERT(fn(nullptr, buf, sizeof(buf)) == -1);
        TEST_ASSERT(fn(&d, nullptr, sizeof(buf)) == -1);

        std::memset(buf, 'X', sizeof(buf));
        TEST_ASSERT(fn(&d, buf, 0) == -1);
        TEST_ASSERT(buf[0] == 'X');
    }

    // Small-buffer truncation (len == 1) on callbacks that format non-empty strings on zeroed state:
    constexpr std::array<report_device_extra_info_fn_t, 8> kNonEmptyOnZeroCallbacks = {
        &uni_hid_parser_ds4_device_extra_info,          &uni_hid_parser_ds5_device_extra_info,
        &uni_hid_parser_switch_device_extra_info,       &uni_hid_parser_switch2_device_extra_info,
        &uni_hid_parser_steam_triton_device_extra_info, &uni_hid_parser_wii_device_extra_info,
        &uni_hid_parser_xboxone_device_extra_info,      &uni_hid_parser_mouse_device_extra_info,
    };
    for (report_device_extra_info_fn_t fn : kNonEmptyOnZeroCallbacks) {
        std::memset(buf, 'Z', sizeof(buf));
        const int full_len = fn(&d, buf, 1);
        TEST_ASSERT(full_len > 0);
        TEST_ASSERT(buf[0] == '\0');
        TEST_ASSERT(buf[1] == 'Z');
    }

    // Small-buffer truncation (len == 8) on DS4 ("FW version 0, HW version 0" -> 26 chars, truncated to "FW vers"):
    std::memset(buf, 'Z', sizeof(buf));
    const int ds4_len = uni_hid_parser_ds4_device_extra_info(&d, buf, 8);
    TEST_ASSERT(ds4_len == 26);
    TEST_ASSERT(std::strcmp(buf, "FW vers") == 0);
    TEST_ASSERT(buf[8] == 'Z');
}

/// Test 22: Verifies exact prefix-free, tab-free, newline-free string formatting and
/// out-of-bounds enum resilience across all 10 parser `device_extra_info` implementations.
void test_parser_device_extra_info_all_parsers_and_enum_bounds() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();
    uni_platform_set_custom(get_posix_imgui_platform());

    char buf[128]{};

    // 1. DualShock 4: initial zeroed state and after 49-byte feature report 0xa3
    {
        uni_hid_device_t d_ds4;
        init_synthetic_device(&d_ds4, 0x054c, 0x09cc, CONTROLLER_TYPE_PS4Controller, "DualShock 4");
        TEST_ASSERT(uni_hid_parser_ds4_device_extra_info(&d_ds4, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "FW version 0, HW version 0") == 0);

        std::array<uint8_t, 49> ds4_fw_report{};
        ds4_fw_report[0] = 0xa3;  // DS4_FEATURE_REPORT_FIRMWARE_VERSION
        // hw_version = 0x0100 at offset 35..36 (little-endian)
        ds4_fw_report[35] = 0x00;
        ds4_fw_report[36] = 0x01;
        // fw_version = 0x0412 at offset 41..42 (little-endian)
        ds4_fw_report[41] = 0x12;
        ds4_fw_report[42] = 0x04;
        uni_hid_parser_ds4_parse_feature_report(&d_ds4, ds4_fw_report.data(), ds4_fw_report.size());

        const int n = uni_hid_parser_ds4_device_extra_info(&d_ds4, buf, sizeof(buf));
        TEST_ASSERT(n > 0);
        TEST_ASSERT(std::strcmp(buf, "FW version 0x412, HW version 0x100") == 0);
        TEST_ASSERT(std::strchr(buf, '\t') == nullptr && std::strchr(buf, '\n') == nullptr);
    }

    // 2. DualSense: after 64-byte feature report 0x20 (hw_version=0x00010002, fw_version=0x01020304,
    //    update_version=0x0221 -> use_vibration2=1)
    {
        uni_hid_device_t d_ds5;
        init_synthetic_device(&d_ds5, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
        std::array<uint8_t, 64> ds5_fw_report{};
        ds5_fw_report[0] = 0x20;  // DS5_FEATURE_REPORT_FIRMWARE_VERSION
        // hw_version = 0x00010002 at offset 24..27 (little-endian)
        ds5_fw_report[24] = 0x02;
        ds5_fw_report[25] = 0x00;
        ds5_fw_report[26] = 0x01;
        ds5_fw_report[27] = 0x00;
        // fw_version = 0x01020304 at offset 28..31 (little-endian)
        ds5_fw_report[28] = 0x04;
        ds5_fw_report[29] = 0x03;
        ds5_fw_report[30] = 0x02;
        ds5_fw_report[31] = 0x01;
        // update_version = 0x0221 at offset 44..45 (little-endian)
        ds5_fw_report[44] = 0x21;
        ds5_fw_report[45] = 0x02;
        uni_hid_parser_ds5_parse_feature_report(&d_ds5, ds5_fw_report.data(), ds5_fw_report.size());

        const int n = uni_hid_parser_ds5_device_extra_info(&d_ds5, buf, sizeof(buf));
        TEST_ASSERT(n > 0);
        TEST_ASSERT(
            std::strcmp(buf, "FW version: 0x1020304, HW version: 0x10002, update version: 0x221, use vibration2: 1") ==
            0);
        TEST_ASSERT(std::strchr(buf, '\t') == nullptr && std::strchr(buf, '\n') == nullptr);
    }

    // 3. Nintendo Switch: synthetic 0x21 SUBCMD_REQ_DEV_INFO (0x02) reply + all 3 Switch vtable entries
    {
        uni_hid_device_t d_sw;
        init_synthetic_device(&d_sw, 0x057e, 0x2009, CONTROLLER_TYPE_SwitchProController, "Pro Controller");
        // 18-byte report 0x21 (SWITCH_INPUT_SUBCMD_REPLY):
        //   [0]=0x21, [1]=timer, [2]=bat_con (0x80), [3..12]=status, [13]=ack (0x82),
        //   [14]=subcmd_id (0x02 = SUBCMD_REQ_DEV_INFO), [15]=fw_hi (4), [16]=fw_lo (33), [17]=type (3)
        constexpr std::array<uint8_t, 18> kSwitchDevInfoReply = {
            0x21, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x02, 4, 33, 3,
        };
        uni_hid_parser_switch_parse_input_report(&d_sw, kSwitchDevInfoReply.data(), kSwitchDevInfoReply.size());

        const int n = uni_hid_parser_switch_device_extra_info(&d_sw, buf, sizeof(buf));
        TEST_ASSERT(n > 0);
        TEST_ASSERT(std::strcmp(buf, "FW version 4.33") == 0);

        // Verify all 3 Switch controller types wire uni_hid_parser_switch_device_extra_info in k_parser_entries[]
        constexpr std::array<uint16_t, 3> kSwitchPids = {0x2009, 0x2007, 0x2006};
        for (uint16_t pid : kSwitchPids) {
            uni_hid_device_t d_check{};
            d_check.vendor_id = 0x057e;
            d_check.product_id = pid;
            uni_hid_device_guess_controller_type_from_pid_vid(&d_check);
            TEST_ASSERT(d_check.report_parser.device_extra_info == &uni_hid_parser_switch_device_extra_info);
        }
    }

    // 4. Nintendo Wii: valid enums and out-of-bounds positive/negative enum resilience
    {
        uni_hid_device_t d_wii;
        init_synthetic_device(&d_wii, 0x057e, 0x0330, CONTROLLER_TYPE_WiiController, "Nintendo RVL-CNT-01-TR");
        auto* wii_ins = reinterpret_cast<TestWiiParserPrefix*>(&d_wii.parser_data[0]);
        wii_ins->dev_type = 3;  // WII_DEVTYPE_REMOTE_MP
        wii_ins->ext_type = 2;  // WII_EXT_NUNCHUK
        TEST_ASSERT(uni_hid_parser_wii_device_extra_info(&d_wii, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "device 'Wii Mote Motion Plus (2nd gen)', extension 'Nunchuk'") == 0);

        wii_ins->dev_type = 99;
        wii_ins->ext_type = 255;
        TEST_ASSERT(uni_hid_parser_wii_device_extra_info(&d_wii, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "device 'unk', extension 'unk'") == 0);

        wii_ins->dev_type = -1;
        wii_ins->ext_type = -1;
        TEST_ASSERT(uni_hid_parser_wii_device_extra_info(&d_wii, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "device 'unk', extension 'unk'") == 0);
    }

    // 5. Xbox Wireless: v3.1 initial state, runtime upgrades to v4.8 and v5.x, and out-of-bounds enum
    {
        uni_hid_device_t d_xb;
        init_synthetic_device(&d_xb, 0x045e, 0x02fd, CONTROLLER_TYPE_XBoxOneController, "Xbox Wireless Controller");
        TEST_ASSERT(uni_hid_parser_xboxone_device_extra_info(&d_xb, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "FW version v3.1") == 0);

        hid_globals_t globals{};
        // Usage 0x0f on Button page (0x09) upgrades v3.1 -> v4.8
        uni_hid_parser_xboxone_parse_usage(&d_xb, &globals, 0x09, 0x0f, 1);
        TEST_ASSERT(uni_hid_parser_xboxone_device_extra_info(&d_xb, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "FW version v4.8") == 0);

        // Usage HID_USAGE_RECORD (0x00b2) on Consumer page (0x0c) upgrades v4.8 -> v5.x
        uni_hid_parser_xboxone_parse_usage(&d_xb, &globals, 0x0c, 0x00b2, 1);
        TEST_ASSERT(uni_hid_parser_xboxone_device_extra_info(&d_xb, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "FW version v5.x") == 0);

        // Out-of-bounds positive and negative enum values return 0 and write empty string
        auto* xb_ins = reinterpret_cast<TestXboxOneParserPrefix*>(&d_xb.parser_data[0]);
        for (int invalid_ver : {99, -1}) {
            xb_ins->version = invalid_ver;
            std::memset(buf, 'A', sizeof(buf));
            TEST_ASSERT(uni_hid_parser_xboxone_device_extra_info(&d_xb, buf, sizeof(buf)) == 0);
            TEST_ASSERT(buf[0] == '\0');
        }
    }

    // 6. SInput: before feature response (returns 0, empty string) and after 14-byte feature response
    {
        uni_hid_device_t d_sinput{};
        d_sinput.vendor_id = 0x2e8a;
        d_sinput.product_id = 0x10c6;
        d_sinput.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
        uni_hid_device_guess_controller_type_from_pid_vid(&d_sinput);
        TEST_ASSERT(d_sinput.report_parser.device_extra_info == &uni_hid_parser_sinput_device_extra_info);

        std::memset(buf, 'A', sizeof(buf));
        TEST_ASSERT(uni_hid_parser_sinput_device_extra_info(&d_sinput, buf, sizeof(buf)) == 0);
        TEST_ASSERT(buf[0] == '\0');

        constexpr std::array<uint8_t, 14> kSinputFeatureReply = {
            0x02, 0x02, 0x01, 0x00, 0x0f, 0x01, 0x00, 0x00, 0xe8, 0x03, 0x08, 0x00, 0xd0, 0x07,
        };
        uni_hid_parser_sinput_parse_input_report(&d_sinput, kSinputFeatureReply.data(), kSinputFeatureReply.size());
        TEST_ASSERT(uni_hid_parser_sinput_device_extra_info(&d_sinput, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "protocol=1, caps0=0x0f, caps1=0x01, poll=1000us, accel=+/-8g, gyro=+/-2000dps") ==
                    0);
    }

    // 7. Mouse: Apple Magic Mouse 1st gen (VID=0x05ac, PID=0x030d -> scale=0.200000)
    {
        uni_hid_device_t d_mouse;
        init_synthetic_device(&d_mouse, 0x05ac, 0x030d, CONTROLLER_TYPE_GenericMouse, "Magic Mouse");
        uni_hid_parser_mouse_setup(&d_mouse);
        TEST_ASSERT(uni_hid_parser_mouse_device_extra_info(&d_mouse, buf, sizeof(buf)) > 0);
        TEST_ASSERT(std::strcmp(buf, "scale=0.200000") == 0);
    }

    // 8. Keyboard: returns 0 and sets buf[0] = '\0'
    {
        uni_hid_device_t d_kb;
        init_synthetic_device(&d_kb, 0x046d, 0xb342, CONTROLLER_TYPE_GenericKeyboard, "Keyboard");
        std::memset(buf, 'A', sizeof(buf));
        TEST_ASSERT(uni_hid_parser_keyboard_device_extra_info(&d_kb, buf, sizeof(buf)) == 0);
        TEST_ASSERT(buf[0] == '\0');
    }

    // 9. Nintendo Switch 2: Pro Controller 2 (0x2069), Joy-Con 2 Left (0x2067), Joy-Con 2 Right (0x2066)
    {
        constexpr std::array<uint16_t, 3> kSwitch2Pids = {0x2069, 0x2067, 0x2066};
        for (uint16_t pid : kSwitch2Pids) {
            uni_hid_device_t d_sw2{};
            d_sw2.vendor_id = 0x057e;
            d_sw2.product_id = pid;
            d_sw2.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
            d_sw2.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
            uni_hid_device_guess_controller_type_from_pid_vid(&d_sw2);
            TEST_ASSERT(d_sw2.report_parser.device_extra_info == &uni_hid_parser_switch2_device_extra_info);
            d_sw2.report_parser.setup(&d_sw2);

            const int n = uni_hid_parser_switch2_device_extra_info(&d_sw2, buf, sizeof(buf));
            TEST_ASSERT(n > 0);
            TEST_ASSERT(std::strstr(buf, "pid=0x206") != nullptr);
            TEST_ASSERT(std::strstr(buf, "state=idle, cal=default") != nullptr);
            TEST_ASSERT(std::strchr(buf, '\t') == nullptr && std::strchr(buf, '\n') == nullptr);
        }
    }

    // 10. Valve Steam Controller Triton: USB (0x1302), BLE (0x1303), Puck (0x1304), Nereid (0x1305)
    {
        constexpr std::array<uint16_t, 4> kTritonPids = {0x1302, 0x1303, 0x1304, 0x1305};
        for (uint16_t pid : kTritonPids) {
            uni_hid_device_t d_tri{};
            d_tri.vendor_id = 0x28de;
            d_tri.product_id = pid;
            d_tri.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
            d_tri.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
            uni_hid_device_guess_controller_type_from_pid_vid(&d_tri);
            TEST_ASSERT(d_tri.report_parser.device_extra_info == &uni_hid_parser_steam_triton_device_extra_info);
            d_tri.report_parser.setup(&d_tri);

            const int n = uni_hid_parser_steam_triton_device_extra_info(&d_tri, buf, sizeof(buf));
            TEST_ASSERT(n > 0);
            TEST_ASSERT(std::strstr(buf, "pid=0x130") != nullptr);
            TEST_ASSERT(std::strstr(buf, "state=idle, stream=0x00") != nullptr);
            TEST_ASSERT(std::strchr(buf, '\t') == nullptr && std::strchr(buf, '\n') == nullptr);
        }
    }
}

/// Test 23: Verifies `posix_imgui_on_device_ready()` and `posix_imgui_on_controller_data()`
/// propagation of `device_extra_info` into `ControllerSnapshot::device_extra_info`, including
/// `nullptr` / zero-return fallbacks, asynchronous DS4 firmware report arrival, Switch 2 &
/// Steam Triton snapshot + IMU telemetry propagation, and disconnect cleanup.
void test_snapshot_device_extra_info_ready_async_update_and_fallbacks() {
    posix_imgui_reset_for_test();
    struct uni_platform* plat = get_posix_imgui_platform();
    std::array<ControllerSnapshot, kMaxControllers> snapshots{};

    // 1. Slot 0: Controller with report_parser.device_extra_info == nullptr -> empty string
    uni_hid_device_t d_null_cb;
    init_synthetic_device(&d_null_cb, 0x1111, 0x2222, CONTROLLER_TYPE_GenericController, "Generic Pad");
    d_null_cb.report_parser.device_extra_info = nullptr;
    plat->on_device_connected(&d_null_cb);
    TEST_ASSERT(plat->on_device_ready(&d_null_cb) == UNI_ERROR_SUCCESS);

    // 2. Slot 1: Controller whose device_extra_info returns 0 -> empty string
    uni_hid_device_t d_zero_cb;
    init_synthetic_device(&d_zero_cb, 0x2e8a, 0x10c6, CONTROLLER_TYPE_SInputController, "SInput Pending");
    d_zero_cb.report_parser.device_extra_info = &uni_hid_parser_sinput_device_extra_info;
    plat->on_device_connected(&d_zero_cb);
    TEST_ASSERT(plat->on_device_ready(&d_zero_cb) == UNI_ERROR_SUCCESS);

    // 3. Slot 2: DualShock 4 lifecycle (initial 0 at on_device_ready, updated via feature report + on_controller_data)
    uni_hid_device_t d_ds4;
    init_synthetic_device(&d_ds4, 0x054c, 0x09cc, CONTROLLER_TYPE_PS4Controller, "DualShock 4");
    d_ds4.report_parser.device_extra_info = &uni_hid_parser_ds4_device_extra_info;
    d_ds4.report_parser.parse_feature_report = &uni_hid_parser_ds4_parse_feature_report;
    plat->on_device_connected(&d_ds4);
    TEST_ASSERT(plat->on_device_ready(&d_ds4) == UNI_ERROR_SUCCESS);

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(snapshots[0].connected && snapshots[0].device_extra_info[0] == '\0');
    TEST_ASSERT(snapshots[1].connected && snapshots[1].device_extra_info[0] == '\0');
    TEST_ASSERT(snapshots[2].connected &&
                std::strcmp(snapshots[2].device_extra_info, "FW version 0, HW version 0") == 0);

    // Deliver late DS4 firmware version feature report (0xa3) followed by on_controller_data()
    std::array<uint8_t, 49> ds4_fw_report{};
    ds4_fw_report[0] = 0xa3;
    ds4_fw_report[35] = 0x00;
    ds4_fw_report[36] = 0x01;  // hw_version = 0x0100
    ds4_fw_report[41] = 0x12;
    ds4_fw_report[42] = 0x04;  // fw_version = 0x0412
    d_ds4.report_parser.parse_feature_report(&d_ds4, ds4_fw_report.data(), ds4_fw_report.size());

    uni_controller_t ctl{};
    ctl.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    plat->on_controller_data(&d_ds4, &ctl);

    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(std::strcmp(snapshots[2].device_extra_info, "FW version 0x412, HW version 0x100") == 0);

    // 4. Disconnect Slot 2 and verify snapshot is cleared
    plat->on_device_disconnected(&d_ds4);
    posix_imgui_get_snapshots(std::span{snapshots});
    TEST_ASSERT(!snapshots[2].connected);
    TEST_ASSERT(snapshots[2].device_extra_info[0] == '\0');

    // 5. Switch 2 Pro (057e:2069), Joy-Con L (057e:2067), Joy-Con R (057e:2066), and Steam Triton (28de:1303)
    //    end-to-end snapshot & IMU telemetry propagation across Slots 0..3:
    posix_imgui_reset_for_test();
    uni_hid_device_t sw2_pro{};
    sw2_pro.vendor_id = 0x057e;
    sw2_pro.product_id = 0x2069;
    sw2_pro.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    sw2_pro.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    std::snprintf(sw2_pro.name, sizeof(sw2_pro.name), "Nintendo Switch 2 Pro");
    uni_hid_device_guess_controller_type_from_pid_vid(&sw2_pro);
    sw2_pro.report_parser.setup(&sw2_pro);
    plat->on_device_connected(&sw2_pro);
    TEST_ASSERT(plat->on_device_ready(&sw2_pro) == UNI_ERROR_SUCCESS);

    uni_hid_device_t sw2_jcl{};
    sw2_jcl.vendor_id = 0x057e;
    sw2_jcl.product_id = 0x2067;
    sw2_jcl.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    sw2_jcl.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    std::snprintf(sw2_jcl.name, sizeof(sw2_jcl.name), "Nintendo Switch 2 Joy-Con (L)");
    uni_hid_device_guess_controller_type_from_pid_vid(&sw2_jcl);
    sw2_jcl.report_parser.setup(&sw2_jcl);
    plat->on_device_connected(&sw2_jcl);
    TEST_ASSERT(plat->on_device_ready(&sw2_jcl) == UNI_ERROR_SUCCESS);

    uni_hid_device_t sw2_jcr{};
    sw2_jcr.vendor_id = 0x057e;
    sw2_jcr.product_id = 0x2066;
    sw2_jcr.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    sw2_jcr.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    std::snprintf(sw2_jcr.name, sizeof(sw2_jcr.name), "Nintendo Switch 2 Joy-Con (R)");
    uni_hid_device_guess_controller_type_from_pid_vid(&sw2_jcr);
    sw2_jcr.report_parser.setup(&sw2_jcr);
    plat->on_device_connected(&sw2_jcr);
    TEST_ASSERT(plat->on_device_ready(&sw2_jcr) == UNI_ERROR_SUCCESS);

    uni_hid_device_t triton{};
    triton.vendor_id = 0x28de;
    triton.product_id = 0x1303;
    triton.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    triton.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    std::snprintf(triton.name, sizeof(triton.name), "Steam Controller");
    uni_hid_device_guess_controller_type_from_pid_vid(&triton);
    triton.report_parser.setup(&triton);
    plat->on_device_connected(&triton);
    TEST_ASSERT(plat->on_device_ready(&triton) == UNI_ERROR_SUCCESS);

    // Feed a synthetic 63-byte Switch 2 Pro report with +1g (az = 4096) and BUTTON_B (physical A = 0x08)
    std::array<uint8_t, 63> sw2_report{};
    sw2_report[3] = 80;    // 80% battery
    sw2_report[4] = 0x08;  // Right A -> BUTTON_B
    // Neutral sticks (2048 = 0x800) at [10..15]
    sw2_report[10] = 0x00;
    sw2_report[11] = 0x08;
    sw2_report[12] = 0x80;
    sw2_report[13] = 0x00;
    sw2_report[14] = 0x08;
    sw2_report[15] = 0x80;
    // az = 4096 (+1g = 0x1000) at [52..53], gz = 13371 (+936 dps = 0x343b) at [58..59]
    sw2_report[52] = 0x00;
    sw2_report[53] = 0x10;
    sw2_report[58] = 0x3b;
    sw2_report[59] = 0x34;
    sw2_pro.report_parser.init_report(&sw2_pro);
    sw2_pro.report_parser.parse_input_report(&sw2_pro, sw2_report.data(), sw2_report.size());
    plat->on_controller_data(&sw2_pro, &sw2_pro.controller);

    // Feed a synthetic 46-byte Steam Triton 0x47 report with +1g (az = 16384 = 0x4000) and BUTTON_A (0x01)
    std::array<uint8_t, 46> triton_report{};
    triton_report[0] = ID_TRITON_CONTROLLER_STATE_TIMESTAMP;  // 0x47
    triton_report[2] = 0x01;                                  // TRITON_BTN_A -> BUTTON_A
    // az at p[37..38] (report[38..39]) = 16384 (0x4000), gz at p[43..44] (report[44..45]) = 16384 (+1000 dps)
    triton_report[38] = 0x00;
    triton_report[39] = 0x40;
    triton_report[44] = 0x00;
    triton_report[45] = 0x40;
    triton.report_parser.init_report(&triton);
    triton.report_parser.parse_input_report(&triton, triton_report.data(), triton_report.size());
    plat->on_controller_data(&triton, &triton.controller);

    posix_imgui_get_snapshots(std::span{snapshots});
    // Slot 0: Switch 2 Pro
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(std::strcmp(snapshots[0].model_name, "Switch 2 Pro") == 0);
    TEST_ASSERT(snapshots[0].layout == CONTROLLER_LAYOUT_REVERSE);
    TEST_ASSERT(snapshots[0].has_imu && snapshots[0].has_rumble && snapshots[0].has_player_leds);
    TEST_ASSERT(std::strstr(snapshots[0].device_extra_info, "pid=0x2069, state=idle, cal=default") != nullptr);
    TEST_ASSERT(snapshots[0].controller.gamepad.buttons == BUTTON_B);
    TEST_ASSERT_FLOAT_NEAR(UNI_STANDARD_GRAVITY, snapshots[0].controller.gamepad.accel[1], 1e-2f);
    TEST_ASSERT_FLOAT_NEAR(936.0f * UNI_DEG_TO_RAD, snapshots[0].controller.gamepad.gyro[1], 1e-2f);

    // Slot 1 & Slot 2: Switch 2 Joy-Con Left & Right
    TEST_ASSERT(snapshots[1].connected);
    TEST_ASSERT(std::strcmp(snapshots[1].model_name, "Switch 2 JoyCon Left") == 0);
    TEST_ASSERT(snapshots[1].layout == CONTROLLER_LAYOUT_REVERSE && snapshots[1].has_imu);
    TEST_ASSERT(std::strstr(snapshots[1].device_extra_info, "pid=0x2067, state=idle, cal=default") != nullptr);

    TEST_ASSERT(snapshots[2].connected);
    TEST_ASSERT(std::strcmp(snapshots[2].model_name, "Switch 2 JoyCon Right") == 0);
    TEST_ASSERT(snapshots[2].layout == CONTROLLER_LAYOUT_REVERSE && snapshots[2].has_imu);
    TEST_ASSERT(std::strstr(snapshots[2].device_extra_info, "pid=0x2066, state=idle, cal=default") != nullptr);

    // Slot 3: Steam Triton
    TEST_ASSERT(snapshots[3].connected);
    TEST_ASSERT(std::strcmp(snapshots[3].model_name, "Steam Triton") == 0);
    TEST_ASSERT(snapshots[3].layout == CONTROLLER_LAYOUT_STANDARD);
    TEST_ASSERT(snapshots[3].has_imu && snapshots[3].has_rumble && !snapshots[3].has_player_leds);
    TEST_ASSERT(std::strstr(snapshots[3].device_extra_info, "pid=0x1303, state=idle, stream=0x00") != nullptr);
    TEST_ASSERT(snapshots[3].controller.gamepad.buttons == BUTTON_A);
    TEST_ASSERT_FLOAT_NEAR(UNI_STANDARD_GRAVITY, snapshots[3].controller.gamepad.accel[1], 1e-2f);
    TEST_ASSERT_FLOAT_NEAR(1000.0f * UNI_DEG_TO_RAD, snapshots[3].controller.gamepad.gyro[1], 1e-2f);
}

/// Test 24: Verifies headless `DemoScene::DoFrame()` on the Info tab (`SelectCategoryTabForTest(4)`)
/// with both a populated `device_extra_info` string and an empty `device_extra_info` fallback (`"Not available"`).
void test_demo_scene_headless_info_tab_with_populated_and_empty_device_extra_info() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    struct uni_platform* plat = get_posix_imgui_platform();

    // Slot 0: DualSense with populated firmware version
    uni_hid_device_t d_ds5;
    init_synthetic_device(&d_ds5, 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense With FW");
    d_ds5.report_parser.device_extra_info = &uni_hid_parser_ds5_device_extra_info;
    std::array<uint8_t, 64> ds5_fw_report{};
    ds5_fw_report[0] = 0x20;
    ds5_fw_report[24] = 0x02;
    ds5_fw_report[26] = 0x01;
    ds5_fw_report[28] = 0x04;
    ds5_fw_report[29] = 0x03;
    ds5_fw_report[30] = 0x02;
    ds5_fw_report[31] = 0x01;
    ds5_fw_report[44] = 0x21;
    ds5_fw_report[45] = 0x02;
    uni_hid_parser_ds5_parse_feature_report(&d_ds5, ds5_fw_report.data(), ds5_fw_report.size());
    plat->on_device_connected(&d_ds5);
    TEST_ASSERT(plat->on_device_ready(&d_ds5) == UNI_ERROR_SUCCESS);

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    TEST_ASSERT(ctx != nullptr);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.0f, 800.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->Build();

    DemoScene demo_scene;
    demo_scene.SetPreferencesActiveForTest(false);
    demo_scene.SelectCategoryTabForTest(4);  // Info tab

    // Render frame 1 with Slot 0 (populated device_extra_info) active
    ImGui::NewFrame();
    demo_scene.DoFrame();
    ImGui::Render();
    ImDrawData* draw_data0 = ImGui::GetDrawData();
    TEST_ASSERT(draw_data0 != nullptr && draw_data0->Valid && draw_data0->CmdListsCount > 0);

    // Connect Slot 1: Generic Controller with nullptr device_extra_info ("Not available" fallback)
    uni_hid_device_t d_gen;
    init_synthetic_device(&d_gen, 0x1234, 0x5678, CONTROLLER_TYPE_GenericController, "Generic No Extra Info");
    d_gen.report_parser.device_extra_info = nullptr;
    plat->on_device_connected(&d_gen);
    TEST_ASSERT(plat->on_device_ready(&d_gen) == UNI_ERROR_SUCCESS);

    // Render frame 2 (auto-switches to newly connected Slot 1 on Info tab)
    demo_scene.SelectCategoryTabForTest(4);
    ImGui::NewFrame();
    demo_scene.DoFrame();
    ImGui::Render();
    ImDrawData* draw_data1 = ImGui::GetDrawData();
    TEST_ASSERT(draw_data1 != nullptr && draw_data1->Valid && draw_data1->CmdListsCount > 0);

    ImGui::DestroyContext(ctx);
}

}  // namespace

int main() {
    std::printf("Running test_posix_imgui headless unit test suite...\n");

    // Suite A
    RUN_TEST(test_on_device_connected_initializes_invalid_slot);
    RUN_TEST(test_pre_ready_disconnect_does_not_clobber_slot_0);
    RUN_TEST(test_seat_assignment_up_to_4_and_5th_controller_rejection);
    RUN_TEST(test_slot_reclamation_on_disconnect_and_reconnection);

    // Suite B
    RUN_TEST(test_controller_layout_classification);
    RUN_TEST(test_controller_capability_flags);

    // Suite C
    RUN_TEST(test_on_controller_data_updates_snapshot_and_delta_ms);
    RUN_TEST(test_command_queue_draining_and_dangling_pointer_safety);

    // Suite D
    RUN_TEST(test_ds5_set_player_leds_null_device_safety);
    RUN_TEST(test_ds5_set_player_leds_single_seat_patterns);
    RUN_TEST(test_ds5_set_player_leds_all_16_bitmasks_and_sequence_wrap);
    RUN_TEST(test_ds5_set_player_leds_upper_nibble_masking);
    RUN_TEST(test_ds5_player_leds_end_to_end_via_posix_imgui_platform);

    // Suite E
    RUN_TEST(test_device_discovery_and_ready_filtering_by_device_type);
    RUN_TEST(test_runtime_disconnection_when_device_category_disabled);
    RUN_TEST(test_virtual_device_disabled_by_default_and_runtime_toggle);

    // Suite F
    RUN_TEST(test_get_snapshots_span_and_pointer_overloads);
    RUN_TEST(test_variant_command_queue_ordering_batch_draining_and_bounds_rejection);

    // Suite G
    RUN_TEST(test_demo_scene_headless_all_tabs_and_font_scales);
    RUN_TEST(test_demo_scene_slot_ui_state_reset_on_disconnect_and_reconnect);

    // Suite H
    RUN_TEST(test_parser_device_extra_info_null_and_small_buffer_guards);
    RUN_TEST(test_parser_device_extra_info_all_parsers_and_enum_bounds);
    RUN_TEST(test_snapshot_device_extra_info_ready_async_update_and_fallbacks);
    RUN_TEST(test_demo_scene_headless_info_tab_with_populated_and_empty_device_extra_info);

    std::printf("\nSummary: %d/%d tests passed.\n", g_tests_run - g_tests_failed, g_tests_run);
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
