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
 */

#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <btstack.h>
#include <uni.h>
#include "btstack_run_loop_posix.h"
}

#include "posix_imgui_platform.h"

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
    ctl2.gamepad.accel[0] = -1.25f;
    ctl2.gamepad.accel[1] = UNI_STANDARD_GRAVITY;
    ctl2.gamepad.accel[2] = 0.5f;
    ctl2.gamepad.gyro[0] = -0.75f;
    ctl2.gamepad.gyro[1] = 1.5f;
    ctl2.gamepad.gyro[2] = -0.125f;
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
    void* data = nullptr;
    int data_len = 0;
    uint8_t rc = uni_circular_buffer_get(&d->outgoing_buffer, &cid, &data, &data_len);
    if (rc != UNI_CIRCULAR_BUFFER_ERROR_OK || cid != static_cast<int16_t>(expected_cid) || data == nullptr ||
        data_len != kDs5OutputReportLen) {
        return false;
    }

    const uint8_t* report = static_cast<const uint8_t*>(data);

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

    const SeatCase kCases[] = {
        {GAMEPAD_SEAT_NONE, 0x00, 0},                            // -----
        {GAMEPAD_SEAT_A, BIT(2), 1},                             // --X-- (0x04)
        {GAMEPAD_SEAT_B, BIT(1) | BIT(3), 2},                    // -X-X- (0x0a)
        {GAMEPAD_SEAT_C, BIT(0) | BIT(2) | BIT(4), 3},           // X-X-X (0x15, was 0x1b with value % 5)
        {GAMEPAD_SEAT_D, BIT(0) | BIT(1) | BIT(3) | BIT(4), 4},  // XX-XX (0x1b, was 0x15 with value % 5)
    };

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

    const uint8_t kExpectedDs5Leds[16] = {
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

/// Test 12: Verifies that dirty upper-nibble bits (`0x10..0xf0`) are masked off before
/// switching so single-seat patterns never fall into `default:` and upper bits never
/// leak into bits 5..7 of `player_leds`.
void test_ds5_set_player_leds_upper_nibble_masking() {
    const uint8_t kExpectedDs5Leds[16] = {
        0x00, 0x04, 0x0a, 0x03, 0x15, 0x09, 0x0a, 0x0b, 0x1b, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,
    };
    const uint8_t kHighNibbles[] = {0x10, 0x20, 0x50, 0xa0, 0xf0};

    for (uint8_t high : kHighNibbles) {
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
}

/// Test 13: Verifies end-to-end integration between `posix_imgui_platform` and the real
/// `uni_hid_parser_ds5_set_player_leds` across all 4 controller slots (`GAMEPAD_SEAT_A..D`)
/// plus runtime LED override via `posix_imgui_request_player_leds`.
void test_ds5_player_leds_end_to_end_via_posix_imgui_platform() {
    btstack_run_loop_deinit();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    posix_imgui_reset_for_test();

    struct uni_platform* plat = get_posix_imgui_platform();
    const uint8_t kExpectedSeatLeds[kMaxControllers] = {
        0x04,  // Slot 0 (GAMEPAD_SEAT_A): --X--
        0x0a,  // Slot 1 (GAMEPAD_SEAT_B): -X-X-
        0x15,  // Slot 2 (GAMEPAD_SEAT_C): X-X-X
        0x1b,  // Slot 3 (GAMEPAD_SEAT_D): XX-XX
    };

    uni_hid_device_t devices[kMaxControllers];
    for (int i = 0; i < kMaxControllers; ++i) {
        init_synthetic_device(&devices[i], 0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller, "DualSense");
        devices[i].conn.interrupt_cid = static_cast<uint16_t>(kTestDs5InterruptCid + i);
        devices[i].report_parser.set_player_leds = &uni_hid_parser_ds5_set_player_leds;

        plat->on_device_connected(&devices[i]);
        TEST_ASSERT(plat->on_device_ready(&devices[i]) == UNI_ERROR_SUCCESS);
        TEST_ASSERT(dequeue_and_verify_ds5_led_report(&devices[i], static_cast<uint16_t>(kTestDs5InterruptCid + i), 0,
                                                      kExpectedSeatLeds[i]));
        TEST_ASSERT(uni_circular_buffer_is_empty(&devices[i].outgoing_buffer));
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

    ControllerSnapshot snapshots[kMaxControllers];
    posix_imgui_get_snapshots(snapshots, nullptr);
    TEST_ASSERT(snapshots[0].connected && snapshots[1].connected && snapshots[2].connected);

    // Disable Mouse and Keyboard -> Slot 1 and Slot 2 must disconnect immediately; Slot 0 stays connected.
    posix_imgui_request_set_allowed_device_types(POSIX_IMGUI_DEVICE_TYPE_GAMEPAD);
    btstack_run_loop_base_execute_callbacks();

    posix_imgui_get_snapshots(snapshots, nullptr);
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
    ControllerSnapshot snapshots[kMaxControllers];
    int newly_connected = -1;
    posix_imgui_get_snapshots(snapshots, &newly_connected);
    TEST_ASSERT(newly_connected == 0);

    // Virtual child mouse must now be accepted into Slot 1 even though physical Mice are disabled,
    // and must NOT overwrite `newly_connected_slot`.
    plat->on_device_connected(&virtual_mouse);
    TEST_ASSERT(plat->on_device_ready(&virtual_mouse) == UNI_ERROR_SUCCESS);
    TEST_ASSERT(get_posix_imgui_instance(&virtual_mouse)->slot == 1);

    posix_imgui_get_snapshots(snapshots, &newly_connected);
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

    posix_imgui_get_snapshots(snapshots, nullptr);
    TEST_ASSERT(snapshots[0].connected);
    TEST_ASSERT(!snapshots[1].connected);
    TEST_ASSERT(parent_ds5.child == nullptr);
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

    std::printf("\nSummary: %d/%d tests passed.\n", g_tests_run - g_tests_failed, g_tests_run);
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
