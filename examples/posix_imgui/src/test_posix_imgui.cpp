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
 *   - Suite D (`TextureAssetLoader` CPU-Side PNG Resolution & `libpng` Decoding):
 *     Verifies that all 44 sprite PNGs in `assets/gamecontroller/` resolve and decode into
 *     8-bit RGBA buffers, and that missing or corrupt/truncated PNG streams trigger the
 *     isolated `setjmp` error handler cleanly without aborting.
 *   - Suite E (Button Layout Reconfiguration, Deadzone & Trigger Normalization):
 *     Verifies `ConfigureButtonLayout()` across `STANDARD`, `SHAPES`, and `REVERSE`
 *     (confirming both quad positions and `buttonMask` fields swap for Switch and restore
 *     when returning to `STANDARD`), `MapGamepadToUIButtonMask()`, `NormalizeStickAxis()`,
 *     and `NormalizeTriggerAxis()`.
 */

#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include <btstack.h>
#include <uni.h>
#include "btstack_run_loop_posix.h"
}

#include "controllerui_data.h"
#include "controllerui_util.h"
#include "posix_imgui_platform.h"
#include "texture_asset_loader.h"

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

#define TEST_ASSERT_NEAR(actual, expected, tol)                                                                 \
    do {                                                                                                        \
        float _a = static_cast<float>(actual);                                                                  \
        float _e = static_cast<float>(expected);                                                                \
        if (std::fabs(_a - _e) > static_cast<float>(tol)) {                                                     \
            std::fprintf(stderr, "  [FAIL] %s:%d: Expected %f +/- %f, got %f (%s vs %s)\n", __FILE__, __LINE__, \
                         static_cast<double>(_e), static_cast<double>(tol), static_cast<double>(_a), #actual,   \
                         #expected);                                                                            \
            g_tests_failed++;                                                                                   \
            return;                                                                                             \
        }                                                                                                       \
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

// Mock callback recording state
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

void reset_mock_calls() {
    std::memset(&g_mock_calls, 0, sizeof(g_mock_calls));
}

void mock_play_dual_rumble(struct uni_hid_device_s* d,
                           uint16_t start_delay_ms,
                           uint16_t duration_ms,
                           uint8_t weak_magnitude,
                           uint8_t strong_magnitude) {
    ARG_UNUSED(d);
    g_mock_calls.rumble_calls++;
    g_mock_calls.last_start_delay_ms = start_delay_ms;
    g_mock_calls.last_duration_ms = duration_ms;
    g_mock_calls.last_weak = weak_magnitude;
    g_mock_calls.last_strong = strong_magnitude;
}

void mock_set_player_leds(struct uni_hid_device_s* d, uint8_t leds) {
    ARG_UNUSED(d);
    g_mock_calls.player_leds_calls++;
    g_mock_calls.last_leds = leds;
}

void mock_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b) {
    ARG_UNUSED(d);
    g_mock_calls.lightbar_calls++;
    g_mock_calls.last_r = r;
    g_mock_calls.last_g = g;
    g_mock_calls.last_b = b;
}

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

    ControllerSnapshot snapshots[kMaxControllers];
    int newly_connected = 99;
    posix_imgui_get_snapshots(snapshots, &newly_connected);
    TEST_ASSERT(newly_connected == -1);
    for (int i = 0; i < kMaxControllers; ++i) {
        TEST_ASSERT(!snapshots[i].connected);
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

    ControllerSnapshot snapshots[kMaxControllers];
    int newly_connected = -1;
    posix_imgui_get_snapshots(snapshots, &newly_connected);
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
    posix_imgui_get_snapshots(snapshots, &newly_connected);
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

    uni_hid_device_t devices[kMaxControllers];
    for (int i = 0; i < kMaxControllers; ++i) {
        init_synthetic_device(&devices[i], static_cast<uint16_t>(0x1000 + i), static_cast<uint16_t>(0x2000 + i),
                              CONTROLLER_TYPE_XBoxOneController, "Controller");
        devices[i].report_parser.set_player_leds = &mock_set_player_leds;

        plat->on_device_connected(&devices[i]);
        TEST_ASSERT(plat->on_device_ready(&devices[i]) == UNI_ERROR_SUCCESS);
        TEST_ASSERT(get_posix_imgui_instance(&devices[i])->slot == i);
        TEST_ASSERT(get_posix_imgui_instance(&devices[i])->gamepad_seat == static_cast<uni_gamepad_seat_t>(BIT(i)));
        TEST_ASSERT(g_mock_calls.last_leds == static_cast<uint8_t>(BIT(i)));

        ControllerSnapshot snapshots[kMaxControllers];
        int newly_connected = -1;
        posix_imgui_get_snapshots(snapshots, &newly_connected);
        TEST_ASSERT(newly_connected == i);
        TEST_ASSERT(snapshots[i].connected);
    }

    // Connect a 5th device and verify rejection with UNI_ERROR_NO_SLOTS.
    uni_hid_device_t d4;
    init_synthetic_device(&d4, 0x9999, 0x8888, CONTROLLER_TYPE_XBoxOneController, "5th Pad");
    plat->on_device_connected(&d4);
    TEST_ASSERT(plat->on_device_ready(&d4) == UNI_ERROR_NO_SLOTS);
    TEST_ASSERT(get_posix_imgui_instance(&d4)->slot == -1);

    // Disconnecting the rejected 5th device must not clobber any of slots 0..3.
    plat->on_device_disconnected(&d4);
    ControllerSnapshot snapshots[kMaxControllers];
    int newly_connected = -1;
    posix_imgui_get_snapshots(snapshots, &newly_connected);
    for (int i = 0; i < kMaxControllers; ++i) {
        TEST_ASSERT(snapshots[i].connected);
        TEST_ASSERT(snapshots[i].vendor_id == static_cast<uint16_t>(0x1000 + i));
    }
}

/// Test 4: Verifies that disconnecting an intermediate slot (`Slot 1`) reclaims that
/// exact lowest-numbered slot for the next controller to connect.
void test_slot_reclamation_on_disconnect_and_reconnection() {
    posix_imgui_reset_for_test();
    struct uni_platform* plat = get_posix_imgui_platform();

    uni_hid_device_t devices[kMaxControllers];
    for (int i = 0; i < kMaxControllers; ++i) {
        init_synthetic_device(&devices[i], static_cast<uint16_t>(0x10 + i), 0x20, CONTROLLER_TYPE_PS4Controller, "DS4");
        plat->on_device_connected(&devices[i]);
        TEST_ASSERT(plat->on_device_ready(&devices[i]) == UNI_ERROR_SUCCESS);
    }

    // Disconnect slot 1.
    plat->on_device_disconnected(&devices[1]);
    TEST_ASSERT(get_posix_imgui_instance(&devices[1])->slot == -1);

    ControllerSnapshot snapshots[kMaxControllers];
    int newly_connected = -1;
    posix_imgui_get_snapshots(snapshots, &newly_connected);
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

    posix_imgui_get_snapshots(snapshots, &newly_connected);
    TEST_ASSERT(newly_connected == 1);
    TEST_ASSERT(snapshots[1].connected);
    TEST_ASSERT(snapshots[1].vendor_id == 0x057e);
}

// ============================================================================
// Suite B: ControllerLayoutType & Capability Detection
// ============================================================================

/// Test 5: Verifies `ControllerLayoutType` classification across Xbox/Generic,
/// PlayStation, and Nintendo Switch controller types.
void test_controller_layout_classification() {
    struct LayoutCase {
        uni_controller_type_t type;
        ControllerLayoutType expected_layout;
    };

    const LayoutCase kCases[] = {
        {CONTROLLER_TYPE_XBoxOneController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_XBox360Controller, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_AndroidController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_GenericController, CONTROLLER_LAYOUT_STANDARD},
        {CONTROLLER_TYPE_8BitdoController, CONTROLLER_LAYOUT_STANDARD},
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
    };

    struct uni_platform* plat = get_posix_imgui_platform();
    for (const LayoutCase& tc : kCases) {
        posix_imgui_reset_for_test();
        uni_hid_device_t d;
        init_synthetic_device(&d, 0x1234, 0x5678, tc.type, "TestPad");
        plat->on_device_connected(&d);
        TEST_ASSERT(plat->on_device_ready(&d) == UNI_ERROR_SUCCESS);

        ControllerSnapshot snapshots[kMaxControllers];
        posix_imgui_get_snapshots(snapshots, nullptr);
        TEST_ASSERT(snapshots[0].layout == tc.expected_layout);
    }
}

/// Test 6: Verifies hardware capability detection (`has_rumble`, `has_player_leds`,
/// `has_rgb_led`, `has_brightness_led`, `has_imu`) across DualSense, Switch Pro, and Xbox One.
void test_controller_capability_flags() {
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

    ControllerSnapshot snapshots[kMaxControllers];
    posix_imgui_get_snapshots(snapshots, nullptr);
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

    posix_imgui_get_snapshots(snapshots, nullptr);
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

    posix_imgui_get_snapshots(snapshots, nullptr);
    TEST_ASSERT(snapshots[0].has_rumble);
    TEST_ASSERT(!snapshots[0].has_player_leds);
    TEST_ASSERT(!snapshots[0].has_rgb_led);
    TEST_ASSERT(!snapshots[0].has_brightness_led);
    TEST_ASSERT(!snapshots[0].has_imu);
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

    ControllerSnapshot snapshots[kMaxControllers];
    int newly_connected = -1;
    posix_imgui_get_snapshots(snapshots, &newly_connected);
    TEST_ASSERT(newly_connected == 0);

    // Second call must reset newly_connected_slot to -1.
    posix_imgui_get_snapshots(snapshots, &newly_connected);
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
    ctl2.gamepad.accel[0] = 100;
    ctl2.gamepad.accel[1] = -200;
    ctl2.gamepad.accel[2] = 980;
    ctl2.gamepad.gyro[0] = 15;
    ctl2.gamepad.gyro[1] = -25;
    ctl2.gamepad.gyro[2] = 35;
    d0.conn.rssi = 215;
    plat->on_controller_data(&d0, &ctl2);

    posix_imgui_get_snapshots(snapshots, nullptr);
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
    TEST_ASSERT(snapshots[0].controller.gamepad.accel[0] == 100);
    TEST_ASSERT(snapshots[0].controller.gamepad.accel[1] == -200);
    TEST_ASSERT(snapshots[0].controller.gamepad.accel[2] == 980);
    TEST_ASSERT(snapshots[0].controller.gamepad.gyro[0] == 15);
    TEST_ASSERT(snapshots[0].controller.gamepad.gyro[1] == -25);
    TEST_ASSERT(snapshots[0].controller.gamepad.gyro[2] == 35);
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
// Suite D: TextureAssetLoader PNG Resolution & libpng Decoding
// ============================================================================

/// Test 9: Verifies CPU-side path resolution and `libpng` RGBA8 decoding across all 44
/// controller sprite PNG assets in `assets/gamecontroller/`.
void test_texture_asset_loader_decodes_all_44_png_assets() {
    size_t count = 0;
    const char* const* assets = ControllerUIData::getAllSpriteAssetNames(&count);
    TEST_ASSERT(assets != nullptr);
    TEST_ASSERT(count == 44);

    for (size_t i = 0; i < count; ++i) {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> rgba;
        bool ok = TextureAssetLoader::DecodePNGToRGBA(assets[i], &width, &height, &rgba);
        TEST_ASSERT(ok);
        TEST_ASSERT(width > 0);
        TEST_ASSERT(height > 0);
        TEST_ASSERT(rgba.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    }
}

/// Test 10: Verifies that `DecodePNGToRGBA` returns `false` cleanly on missing paths,
/// null/empty filenames, and truncated/corrupt PNG streams (exercising the `setjmp` recovery).
void test_texture_asset_loader_handles_missing_and_corrupt_files() {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;

    TEST_ASSERT(!TextureAssetLoader::DecodePNGToRGBA("gamecontroller/Does_Not_Exist.png", &width, &height, &rgba));
    TEST_ASSERT(!TextureAssetLoader::DecodePNGToRGBA(nullptr, &width, &height, &rgba));
    TEST_ASSERT(!TextureAssetLoader::DecodePNGToRGBA("", &width, &height, &rgba));

    // Create a temporary corrupt file with a valid 8-byte PNG signature but truncated IHDR chunk
    // to exercise the libpng setjmp error handler cleanly without aborting.
    const char* tmp_corrupt_path = "/tmp/bluepad32_test_corrupt.png";
    {
        FILE* fp = std::fopen(tmp_corrupt_path, "wb");
        TEST_ASSERT(fp != nullptr);
        const uint8_t corrupt_bytes[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0x00, 0x00, 0x00, 0x0d, 'I', 'H'};
        TEST_ASSERT(std::fwrite(corrupt_bytes, 1, sizeof(corrupt_bytes), fp) == sizeof(corrupt_bytes));
        std::fclose(fp);
    }
    TEST_ASSERT(!TextureAssetLoader::DecodePNGToRGBA(tmp_corrupt_path, &width, &height, &rgba));
    std::remove(tmp_corrupt_path);
}

// ============================================================================
// Suite E: Button Layout (STANDARD, SHAPES, REVERSE), Deadzone & Trigger Mapping
// ============================================================================

/// Test 11: Verifies `ConfigureButtonLayout()` across `STANDARD` (Xbox), `SHAPES`
/// (PlayStation), and `REVERSE` (Nintendo Switch), confirming that `REVERSE` swaps both
/// quad positions and `buttonMask` bindings and that returning to `STANDARD` restores both.
void test_configure_button_layout_standard_shapes_and_switch_reverse() {
    const ImVec2 pos_south = ControllerUIData::getButtonQuadPosition(UIBUTTON_DPAD_DOWN);
    const ImVec2 pos_east = ControllerUIData::getButtonQuadPosition(UIBUTTON_DPAD_RIGHT);
    const ImVec2 pos_west = ControllerUIData::getButtonQuadPosition(UIBUTTON_DPAD_LEFT);
    const ImVec2 pos_north = ControllerUIData::getButtonQuadPosition(UIBUTTON_DPAD_UP);

    // 1. CONTROLLER_LAYOUT_STANDARD (Xbox)
    ControllerUIData::ConfigureButtonLayout(CONTROLLER_LAYOUT_STANDARD);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_CROSS).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_CIRCLE).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_SQUARE).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_TRIANGLE).enabled);

    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).basePosition.x, pos_south.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).basePosition.y, pos_south.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).buttonMask == UI_BUTTON_MASK_A);

    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).basePosition.x, pos_east.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).basePosition.y, pos_east.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).buttonMask == UI_BUTTON_MASK_B);

    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).basePosition.x, pos_west.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).basePosition.y, pos_west.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).buttonMask == UI_BUTTON_MASK_X);

    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).basePosition.x, pos_north.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).basePosition.y, pos_north.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).buttonMask == UI_BUTTON_MASK_Y);

    // 2. CONTROLLER_LAYOUT_SHAPES (PlayStation)
    ControllerUIData::ConfigureButtonLayout(CONTROLLER_LAYOUT_SHAPES);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_A).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_B).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_X).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_CROSS).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_CIRCLE).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_SQUARE).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_TRIANGLE).enabled);

    // 3. CONTROLLER_LAYOUT_REVERSE (Nintendo Switch: B at South, A at East, Y at West, X at North)
    ControllerUIData::ConfigureButtonLayout(CONTROLLER_LAYOUT_REVERSE);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).enabled);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).enabled);
    TEST_ASSERT(!ControllerUIData::getControllerButtonInfo(UIBUTTON_CROSS).enabled);

    // UIBUTTON_B is at South and triggered by UI_BUTTON_MASK_A (physical South in Bluepad32).
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).basePosition.x, pos_south.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).basePosition.y, pos_south.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).buttonMask == UI_BUTTON_MASK_A);

    // UIBUTTON_A is at East and triggered by UI_BUTTON_MASK_B (physical East in Bluepad32).
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).basePosition.x, pos_east.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).basePosition.y, pos_east.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).buttonMask == UI_BUTTON_MASK_B);

    // UIBUTTON_Y is at West and triggered by UI_BUTTON_MASK_X (physical West in Bluepad32).
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).basePosition.x, pos_west.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).basePosition.y, pos_west.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).buttonMask == UI_BUTTON_MASK_X);

    // UIBUTTON_X is at North and triggered by UI_BUTTON_MASK_Y (physical North in Bluepad32).
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).basePosition.x, pos_north.x, 0.01f);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).basePosition.y, pos_north.y, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).buttonMask == UI_BUTTON_MASK_Y);

    // 4. Transition back from REVERSE -> STANDARD restores both positions and masks.
    ControllerUIData::ConfigureButtonLayout(CONTROLLER_LAYOUT_STANDARD);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).basePosition.x, pos_south.x, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_A).buttonMask == UI_BUTTON_MASK_A);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).basePosition.x, pos_east.x, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_B).buttonMask == UI_BUTTON_MASK_B);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).basePosition.x, pos_west.x, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_X).buttonMask == UI_BUTTON_MASK_X);
    TEST_ASSERT_NEAR(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).basePosition.x, pos_north.x, 0.01f);
    TEST_ASSERT(ControllerUIData::getControllerButtonInfo(UIBUTTON_Y).buttonMask == UI_BUTTON_MASK_Y);

    // Verify MapGamepadToUIButtonMask translation
    uni_gamepad_t gp{};
    gp.dpad = DPAD_DOWN | DPAD_LEFT;
    gp.buttons = BUTTON_A | BUTTON_SHOULDER_L | BUTTON_THUMB_R;
    gp.brake = 600;
    gp.misc_buttons = MISC_BUTTON_START | MISC_BUTTON_CAPTURE;
    uint32_t mask = MapGamepadToUIButtonMask(gp);
    TEST_ASSERT((mask & UI_BUTTON_MASK_DPAD_DOWN) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_DPAD_LEFT) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_A) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_L1) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_L2) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_R3) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_START) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_CAPTURE) != 0);
    TEST_ASSERT((mask & UI_BUTTON_MASK_B) == 0);
}

/// Test 12: Verifies `NormalizeStickAxis` deadzone trimming (`AXIS_THRESHOLD`) and clamping,
/// plus `NormalizeTriggerAxis` `[0, 1023]` scaling and digital button fallback.
void test_axis_deadzone_and_trigger_normalization() {
    // Stick axis normalization & deadzone trimming
    TEST_ASSERT_NEAR(NormalizeStickAxis(0, false), 0.0f, 1e-6f);
    TEST_ASSERT_NEAR(NormalizeStickAxis(5, false), 0.0f, 1e-6f);
    TEST_ASSERT_NEAR(NormalizeStickAxis(-5, false), 0.0f, 1e-6f);
    TEST_ASSERT_NEAR(NormalizeStickAxis(5, true), 5.0f / 512.0f, 1e-5f);
    TEST_ASSERT_NEAR(NormalizeStickAxis(-512, false), -1.0f, 1e-5f);
    TEST_ASSERT_NEAR(NormalizeStickAxis(511, false), 511.0f / 512.0f, 1e-4f);
    TEST_ASSERT_NEAR(NormalizeStickAxis(600, false), 1.0f, 1e-5f);
    TEST_ASSERT_NEAR(NormalizeStickAxis(-600, false), -1.0f, 1e-5f);

    // Trigger axis normalization & digital fallback
    TEST_ASSERT_NEAR(NormalizeTriggerAxis(0, false), 0.0f, 1e-6f);
    TEST_ASSERT_NEAR(NormalizeTriggerAxis(0, true), 1.0f, 1e-6f);
    TEST_ASSERT_NEAR(NormalizeTriggerAxis(512, false), 512.0f / 1023.0f, 1e-5f);
    TEST_ASSERT_NEAR(NormalizeTriggerAxis(1023, false), 1.0f, 1e-5f);
    TEST_ASSERT_NEAR(NormalizeTriggerAxis(2000, false), 1.0f, 1e-5f);
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
    RUN_TEST(test_texture_asset_loader_decodes_all_44_png_assets);
    RUN_TEST(test_texture_asset_loader_handles_missing_and_corrupt_files);

    // Suite E
    RUN_TEST(test_configure_button_layout_standard_shapes_and_switch_reverse);
    RUN_TEST(test_axis_deadzone_and_trigger_normalization);

    std::printf("\nSummary: %d/%d tests passed.\n", g_tests_run - g_tests_failed, g_tests_run);
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
