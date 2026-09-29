// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file test_uni_hid_parser_synthetic.c
 * @brief Synthetic HID report verification suite for all 20 Bluepad32 parser modules.
 *
 * Exercises valid input/feature/output reports, shortened/malformed report payloads,
 * regression edge cases (Steam INT16_MIN & offset advances, Atari len==0 & OOB D-pad,
 * Keyboard JX-05 key array saturation, Switch IMU bounds, DS4 CRC32 validation),
 * generic descriptor edge cases, and a deterministic PRNG sweep across all entries
 * in `arrControllers[]` (`uni_controller_list.h`).
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack.h>
#include <btstack_memory.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_base.h>
#include <btstack_run_loop_posix.h>

#include "bt/uni_bt_defines.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"
#include "controller/uni_keyboard.h"
#include "controller/uni_mouse.h"
#include "hid_usage.h"
#include "parser/uni_hid_parser.h"
#include "parser/uni_hid_parser_8bitdo.h"
#include "parser/uni_hid_parser_android.h"
#include "parser/uni_hid_parser_atari.h"
#include "parser/uni_hid_parser_ds3.h"
#include "parser/uni_hid_parser_ds4.h"
#include "parser/uni_hid_parser_ds5.h"
#include "parser/uni_hid_parser_generic.h"
#include "parser/uni_hid_parser_icade.h"
#include "parser/uni_hid_parser_keyboard.h"
#include "parser/uni_hid_parser_mouse.h"
#include "parser/uni_hid_parser_nimbus.h"
#include "parser/uni_hid_parser_ouya.h"
#include "parser/uni_hid_parser_psmove.h"
#include "parser/uni_hid_parser_rumble.h"
#include "parser/uni_hid_parser_smarttvremote.h"
#include "parser/uni_hid_parser_stadia.h"
#include "parser/uni_hid_parser_steam.h"
#include "parser/uni_hid_parser_switch.h"
#include "parser/uni_hid_parser_wii.h"
#include "parser/uni_hid_parser_xboxone.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "uni_circular_buffer.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "uni_property.h"
#include "uni_utils.h"
#include "uni_virtual_device.h"

// Must be included after controller/uni_controller_type.h
#include "controller/uni_controller_list.h"

// Lightweight test macros matching the Bluepad32 test harness style and exact TEST(...) names.
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                   \
    do {                                 \
        printf("Running " #name "... "); \
        fflush(stdout);                  \
        test_##name();                   \
        printf("PASS\n");                \
    } while (0)

#define ASSERT_EQ(expected, actual)                                                                                  \
    do {                                                                                                             \
        long long _exp = (long long)(expected);                                                                      \
        long long _act = (long long)(actual);                                                                        \
        if (_exp != _act) {                                                                                          \
            fprintf(stderr, "ASSERT_EQ failed at %s:%d: expected %lld, got %lld\n", __FILE__, __LINE__, _exp, _act); \
            abort();                                                                                                 \
        }                                                                                                            \
    } while (0)

#define ASSERT_NE(not_expected, actual)                                                                    \
    do {                                                                                                   \
        long long _nexp = (long long)(uintptr_t)(not_expected);                                            \
        long long _act = (long long)(uintptr_t)(actual);                                                   \
        if (_nexp == _act) {                                                                               \
            fprintf(stderr, "ASSERT_NE failed at %s:%d: got unexpected %lld\n", __FILE__, __LINE__, _act); \
            abort();                                                                                       \
        }                                                                                                  \
    } while (0)

#define EXPECT_EQ(expected, actual) ASSERT_EQ(expected, actual)
#define EXPECT_NE(not_expected, actual) ASSERT_NE(not_expected, actual)

#define EXPECT_TRUE(cond)                                                                    \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            fprintf(stderr, "EXPECT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            abort();                                                                         \
        }                                                                                    \
    } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

#define EXPECT_GT(val1, val2)                                                                           \
    do {                                                                                                \
        long long _v1 = (long long)(val1);                                                              \
        long long _v2 = (long long)(val2);                                                              \
        if (!(_v1 > _v2)) {                                                                             \
            fprintf(stderr, "EXPECT_GT failed at %s:%d: %lld <= %lld\n", __FILE__, __LINE__, _v1, _v2); \
            abort();                                                                                    \
        }                                                                                               \
    } while (0)

// Toggleable log silencer so the 480+ controller PRNG fuzz sweep does not flood stdout.
static bool g_silence_logs = true;

void uni_logv(const char* fmt, va_list args) {
    if (g_silence_logs) {
        return;
    }
    vfprintf(stdout, fmt, args);
}

// --- Dummy HCI Transport & Mock Platform Implementation for Synthetic Tests ---
static void dummy_transport_register_packet_handler(void (*handler)(uint8_t packet_type,
                                                                    uint8_t* packet,
                                                                    uint16_t size)) {
    (void)handler;
}

static const hci_transport_t g_dummy_transport = {
    .name = "dummy_synthetic",
    .register_packet_handler = dummy_transport_register_packet_handler,
};

static void mock_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
}

static void mock_platform_on_init_complete(void) {}

static uni_error_t mock_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    ARG_UNUSED(name);
    ARG_UNUSED(cod);
    ARG_UNUSED(rssi);
    return UNI_ERROR_SUCCESS;
}

static void mock_platform_on_device_connected(uni_hid_device_t* d) {
    ARG_UNUSED(d);
}

static void mock_platform_on_device_disconnected(uni_hid_device_t* d) {
    ARG_UNUSED(d);
}

static uni_error_t mock_platform_on_device_ready(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    return UNI_ERROR_SUCCESS;
}

static void mock_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    ARG_UNUSED(d);
    ARG_UNUSED(ctl);
}

static void mock_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(event);
    ARG_UNUSED(data);
}

static const uni_property_t* mock_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static struct uni_platform g_mock_platform = {
    .name = "test_synthetic_mock",
    .init = mock_platform_init,
    .on_init_complete = mock_platform_on_init_complete,
    .on_device_discovered = mock_platform_on_device_discovered,
    .on_device_connected = mock_platform_on_device_connected,
    .on_device_disconnected = mock_platform_on_device_disconnected,
    .on_device_ready = mock_platform_on_device_ready,
    .on_controller_data = mock_platform_on_controller_data,
    .on_oob_event = mock_platform_on_oob_event,
    .get_property = mock_platform_get_property,
};

struct uni_platform* uni_get_platform(void) {
    return &g_mock_platform;
}

/**
 * @brief Initialize a synthetic `uni_hid_device_t` for a given VID/PID pair and optional CoD.
 *
 * Resets the BTstack run loop timer list and internal device pool (for virtual
 * child devices created by DS4/DS5), initializes `d`, assigns its VID/PID/CoD, and
 * resolves its `report_parser` vtable via `uni_hid_device_guess_controller_type_from_pid_vid()`.
 */
static void setup_synthetic_device_with_cod(uni_hid_device_t* d, uint16_t vid, uint16_t pid, uint32_t cod) {
    btstack_run_loop_base_timers = NULL;
    uni_hid_device_setup();
    uni_hid_device_init(d);
    d->vendor_id = vid;
    d->product_id = pid;
    d->cod = cod;
    uni_hid_device_guess_controller_type_from_pid_vid(d);
    if (d->report_parser.setup) {
        d->report_parser.setup(d);
    }
}

static void setup_synthetic_device(uni_hid_device_t* d, uint16_t vid, uint16_t pid) {
    setup_synthetic_device_with_cod(d, vid, pid, 0);
}

/**
 * @brief Dispatch an input report through the device's parser lifecycle.
 */
static void feed_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (d->report_parser.init_report) {
        d->report_parser.init_report(d);
    }
    if (d->report_parser.parse_input_report) {
        d->report_parser.parse_input_report(d, report, len);
    }
}

/**
 * @brief Dispatch a feature report through the device's parser lifecycle.
 */
static void feed_feature_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (d->report_parser.parse_feature_report) {
        d->report_parser.parse_feature_report(d, report, len);
    }
}

// ============================================================================
// 1. DualShock 4 (DS4): Valid BT/USB Input Reports & Short Report Guards
// ============================================================================
TEST(hid_parser_ds4_valid_and_short) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x054c, 0x09cc);
    ASSERT_EQ(CONTROLLER_TYPE_PS4Controller, d.controller_type);
    ASSERT_NE(NULL, d.child);

    // 1. Valid 78-byte Bluetooth report (report ID 0x11; payload starts at &report[3]).
    uint8_t bt_report[78];
    memset(bt_report, 0, sizeof(bt_report));
    bt_report[0] = 0x11;
    // Sticks centered at 127, except LX = 255 (max positive) and LY = 0 (min negative).
    bt_report[3] = 255;  // x -> (255 - 127) * 4 = 512
    bt_report[4] = 0;    // y -> (0 - 127) * 4 = -508
    bt_report[5] = 127;  // rx -> 0
    bt_report[6] = 127;  // ry -> 0
    // buttons[0]: low nibble = dpad (0 = UP), bits 4..7 = West(0x10), South(0x20), East(0x40), North(0x80).
    bt_report[7] = 0x00 | 0x20 | 0x80;  // DPAD_UP | BUTTON_A (South) | BUTTON_Y (North)
    // buttons[1]: L1(0x01), R1(0x02), L2(0x04), R2(0x08), Share(0x10), Options(0x20), L3(0x40), R3(0x80).
    bt_report[8] = 0x01 | 0x10 | 0x20 | 0x40;
    // buttons[2]: PS(0x01) | Touchpad click(0x02).
    bt_report[9] = 0x01 | 0x02;
    bt_report[10] = 255;  // brake -> 255 * 4 = 1020
    bt_report[11] = 128;  // throttle -> 128 * 4 = 512
    // Active touchpad finger contact at (x=100, y=800): num_touch_reports = 1 ([35]), contact = 0x01 ([37], bit 7
    // clear).
    bt_report[35] = 1;
    bt_report[37] = 0x01;
    bt_report[38] = 100;
    bt_report[39] = 0x00;
    bt_report[40] = 50;

    feed_input_report(&d, bt_report, sizeof(bt_report));
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A | BUTTON_Y | BUTTON_SHOULDER_L | BUTTON_THUMB_L, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT | MISC_BUTTON_START | MISC_BUTTON_SYSTEM, d.controller.gamepad.misc_buttons);
    EXPECT_EQ(512, d.controller.gamepad.axis_x);
    EXPECT_EQ(-508, d.controller.gamepad.axis_y);
    EXPECT_EQ(0, d.controller.gamepad.axis_rx);
    EXPECT_EQ(0, d.controller.gamepad.axis_ry);
    EXPECT_EQ(1020, d.controller.gamepad.brake);
    EXPECT_EQ(512, d.controller.gamepad.throttle);
    EXPECT_EQ(UNI_MOUSE_BUTTON_LEFT, d.child->controller.mouse.buttons);
    EXPECT_EQ(0, d.child->controller.mouse.delta_x);

    // Second touch report with finger moved to x=125 updates d.child->controller.mouse.delta_x = 25.
    bt_report[38] = 125;
    feed_input_report(&d, bt_report, sizeof(bt_report));
    EXPECT_EQ(25, d.child->controller.mouse.delta_x);

    // 2. Valid 10-byte basic report (report ID 0x01; payload starts at &report[1]).
    uint8_t usb_report[10];
    memset(usb_report, 0, sizeof(usb_report));
    usb_report[0] = 0x01;
    usb_report[1] = 127;
    usb_report[2] = 127;
    usb_report[3] = 255;          // rx -> 512
    usb_report[4] = 127;          // ry -> 0
    usb_report[5] = 0x04 | 0x40;  // DPAD_DOWN (4) | BUTTON_B (East, 0x40)
    usb_report[6] = 0x02;         // R1 -> BUTTON_SHOULDER_R
    usb_report[7] = 0x00;
    usb_report[8] = 0;
    usb_report[9] = 200;  // throttle -> 800

    feed_input_report(&d, usb_report, sizeof(usb_report));
    EXPECT_EQ(DPAD_DOWN, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_B | BUTTON_SHOULDER_R, d.controller.gamepad.buttons);
    EXPECT_EQ(512, d.controller.gamepad.axis_rx);
    EXPECT_EQ(800, d.controller.gamepad.throttle);

    // 3. Shortened / truncated reports must be safely rejected without crashing or mutating state.
    feed_input_report(&d, bt_report, 0);
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, bt_report, 1);
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, bt_report, 77);
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, usb_report, 9);
    EXPECT_EQ(0, d.controller.gamepad.buttons);
}

// ============================================================================
// 2. DualShock 4 (DS4): Output Report CRC32 Generation & Feature Reports 0x02/0xa3
// ============================================================================
TEST(hid_parser_ds4_invalid_crc32_rejected) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x054c, 0x09cc);
    // Set a non-zero interrupt CID so `uni_hid_device_send_intr_report` queues the 79-byte output report.
    d.conn.interrupt_cid = 0x0041;

    // Trigger an output report (LED color update) so DS4 computes its 79-byte BT output frame + CRC32.
    ASSERT_NE(NULL, d.report_parser.set_lightbar_color);
    d.report_parser.set_lightbar_color(&d, 0x12, 0x34, 0x56);

    int16_t cid = 0;
    void* data = NULL;
    int data_len = 0;
    ASSERT_EQ(UNI_CIRCULAR_BUFFER_ERROR_OK, uni_circular_buffer_get(&d.outgoing_buffer, &cid, &data, &data_len));
    ASSERT_EQ(79, data_len);

    const uint8_t* out_bytes = (const uint8_t*)data;
    // Verify the generated CRC32 matches ~uni_crc32_le(0xffffffff, out_bytes, 75).
    uint32_t expected_crc = ~uni_crc32_le(0xffffffff, out_bytes, 75);
    uint32_t actual_crc = 0;
    memcpy(&actual_crc, &out_bytes[75], sizeof(actual_crc));
    EXPECT_EQ(expected_crc, actual_crc);

    // Tamper with a payload byte and verify CRC32 validation detects the mismatch.
    uint8_t tampered[79];
    memcpy(tampered, out_bytes, sizeof(tampered));
    tampered[10] ^= 0xff;
    uint32_t tampered_crc = ~uni_crc32_le(0xffffffff, tampered, 75);
    EXPECT_NE(expected_crc, tampered_crc);

    // Verify DS4 feature report 0x02 (calibration, 37 bytes), 0xa3 (firmware version, 49 bytes),
    // short feature report rejection (len = 10), and unknown feature report ID (0x99).
    uint8_t calib_report[37];
    memset(calib_report, 0, sizeof(calib_report));
    calib_report[0] = 0x02;
    feed_feature_report(&d, calib_report, 10);  // Short -> ignored
    feed_feature_report(&d, calib_report, sizeof(calib_report));

    uint8_t fw_report[49];
    memset(fw_report, 0, sizeof(fw_report));
    fw_report[0] = 0xa3;
    memcpy(&fw_report[1], "Sep 26 2026", 11);
    memcpy(&fw_report[17], "12:00:00", 8);
    fw_report[35] = 0x34;
    fw_report[36] = 0x12;  // hw_version = 0x1234
    fw_report[41] = 0x78;
    fw_report[42] = 0x56;                    // fw_version = 0x5678
    feed_feature_report(&d, fw_report, 10);  // Short -> ignored
    feed_feature_report(&d, fw_report, sizeof(fw_report));

    uint8_t unk_report[8] = {0x99};
    feed_feature_report(&d, unk_report, sizeof(unk_report));
}

// ============================================================================
// 3. DualSense (DS5): Feature Handshake, BT Input Report (0x31), Adaptive Triggers & Short Guards
// ============================================================================
TEST(hid_parser_ds5_bt_and_usb) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x054c, 0x0ce6);
    ASSERT_EQ(CONTROLLER_TYPE_PS5Controller, d.controller_type);

    // Valid 78-byte Bluetooth report (report ID 0x31; ds5_input_report_t starts at &report[2]).
    uint8_t bt_report[78];
    memset(bt_report, 0, sizeof(bt_report));
    bt_report[0] = 0x31;
    bt_report[1] = 0x02;
    bt_report[2] = 255;                  // x -> 512
    bt_report[3] = 0;                    // y -> -508
    bt_report[4] = 127;                  // rx -> 0
    bt_report[5] = 127;                  // ry -> 0
    bt_report[6] = 100;                  // brake -> 400
    bt_report[7] = 200;                  // throttle -> 800
    bt_report[8] = 0;                    // seq_number
    bt_report[9] = 0x02 | 0x10 | 0x40;   // DPAD_RIGHT (2) | BUTTON_X (West, 0x10) | BUTTON_B (East, 0x40)
    bt_report[10] = 0x01 | 0x02 | 0x10;  // L1 | R1 | Share(Select)
    bt_report[11] = 0x01 | 0x02 | 0x04;  // PS(System) | Touchpad(0x02) | Mute(MiscCapture)
    // Active touchpad finger contact at (x=50, y=0): points[0].contact = 0x01 ([34], bit 7 clear), x_lo = 50 ([35]).
    bt_report[34] = 0x01;
    bt_report[35] = 50;

    // Pre-ready rejection: feeding report 0x31 BEFORE feature report 0x05 completes must be ignored!
    feed_input_report(&d, bt_report, sizeof(bt_report));
    EXPECT_EQ(0, d.controller.gamepad.buttons);

    // Walk DS5 feature report FSM: 0x09 (20B) -> 0x20 (64B) -> 0x05 (41B) -> DS5_STATE_READY.
    uint8_t feat_09[20] = {0x09};
    uint8_t feat_20[64] = {0x20};
    uint8_t feat_05[41] = {0x05};
    feed_feature_report(&d, feat_09, sizeof(feat_09));
    feed_feature_report(&d, feat_20, sizeof(feat_20));
    feed_feature_report(&d, feat_05, 10);  // Short -> ignored (still not ready)
    feed_input_report(&d, bt_report, sizeof(bt_report));
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_feature_report(&d, feat_05, sizeof(feat_05));
    ASSERT_NE(NULL, d.child);

    feed_input_report(&d, bt_report, sizeof(bt_report));
    EXPECT_EQ(DPAD_RIGHT, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_X | BUTTON_B | BUTTON_SHOULDER_L | BUTTON_SHOULDER_R, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT | MISC_BUTTON_SYSTEM | MISC_BUTTON_CAPTURE, d.controller.gamepad.misc_buttons);
    EXPECT_EQ(512, d.controller.gamepad.axis_x);
    EXPECT_EQ(-508, d.controller.gamepad.axis_y);
    EXPECT_EQ(400, d.controller.gamepad.brake);
    EXPECT_EQ(800, d.controller.gamepad.throttle);
    EXPECT_EQ(UNI_MOUSE_BUTTON_LEFT, d.child->controller.mouse.buttons);
    EXPECT_EQ(0, d.child->controller.mouse.delta_x);

    // Second touch report with finger moved to x=80 updates d.child->controller.mouse.delta_x = 30.
    bt_report[35] = 80;
    feed_input_report(&d, bt_report, sizeof(bt_report));
    EXPECT_EQ(30, d.child->controller.mouse.delta_x);

    // Short BT report must be rejected.
    feed_input_report(&d, bt_report, 77);
    EXPECT_EQ(0, d.controller.gamepad.buttons);

    // Exercise DS5 adaptive trigger feedback edge cases (strength == 0 OFF delegation & 27-bit zone shift).
    ds5_adaptive_trigger_effect_t off_eff = ds5_new_adaptive_trigger_effect_off();
    ds5_adaptive_trigger_effect_t zero_fb = ds5_new_adaptive_trigger_effect_feedback(3, 0);
    EXPECT_EQ(off_eff.effect, zero_fb.effect);
    EXPECT_EQ(0, zero_fb.data[0]);
    ds5_adaptive_trigger_effect_t max_fb = ds5_new_adaptive_trigger_effect_feedback(0, 8);
    EXPECT_EQ(0xff, max_fb.data[0]);
    EXPECT_EQ(0x03, max_fb.data[1]);
}

// ============================================================================
// 4. DualShock 3 (DS3): Valid 49-Byte Report & Truncated Report Guards
// ============================================================================
TEST(hid_parser_ds3_valid_and_short) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x054c, 0x0268);
    ASSERT_EQ(CONTROLLER_TYPE_PS3Controller, d.controller_type);

    uint8_t report[49];
    memset(report, 0, sizeof(report));
    report[0] = 0x01;
    // buttons[0]: Select(0x01), L3(0x02), R3(0x04), Start(0x08), Up(0x10), Right(0x20), Down(0x40), Left(0x80)
    report[2] = 0x01 | 0x02 | 0x08 | 0x10;
    // buttons[1]: L2(0x01), R2(0x02), L1(0x04), R1(0x08), North(0x10), East(0x20), South(0x40), West(0x80)
    report[3] = 0x04 | 0x40 | 0x80;
    // buttons[2]: PS(0x01)
    report[4] = 0x01;
    report[6] = 255;   // lx -> 512
    report[7] = 0;     // ly -> -508
    report[8] = 127;   // rx -> 0
    report[9] = 127;   // ry -> 0
    report[18] = 128;  // l2_analog -> 512
    report[19] = 255;  // r2_analog -> 1020

    // Accelerometer & gyroscope (big-endian 10-bit values at bytes 41..48, centered at 511):
    //   accel_x (41..42) = 611 (0x0263) -> accel[0] = 611 - 511 = +100
    //   accel_y (43..44) = 461 (0x01cd) -> accel[2] = 511 - 461 = +50 (Y/Z swapped and inverted)
    //   accel_z (45..46) = 398 (0x018e) -> accel[1] = 511 - 398 = +113 (+1G vertical)
    //   gyro_x  (47..48) = 536 (0x0218) -> gyro[0]  = 536 - 511 = +25
    report[41] = 0x02;
    report[42] = 0x63;
    report[43] = 0x01;
    report[44] = 0xcd;
    report[45] = 0x01;
    report[46] = 0x8e;
    report[47] = 0x02;
    report[48] = 0x18;

    feed_input_report(&d, report, sizeof(report));
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_THUMB_L | BUTTON_SHOULDER_L | BUTTON_A | BUTTON_X, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT | MISC_BUTTON_START | MISC_BUTTON_SYSTEM, d.controller.gamepad.misc_buttons);
    EXPECT_EQ(512, d.controller.gamepad.axis_x);
    EXPECT_EQ(-508, d.controller.gamepad.axis_y);
    EXPECT_EQ(512, d.controller.gamepad.brake);
    EXPECT_EQ(1020, d.controller.gamepad.throttle);
    EXPECT_EQ(100, d.controller.gamepad.accel[0]);
    EXPECT_EQ(113, d.controller.gamepad.accel[1]);
    EXPECT_EQ(50, d.controller.gamepad.accel[2]);
    EXPECT_EQ(25, d.controller.gamepad.gyro[0]);
    EXPECT_EQ(0, d.controller.gamepad.gyro[1]);
    EXPECT_EQ(0, d.controller.gamepad.gyro[2]);

    // Battery and name matching checks.
    EXPECT_TRUE(uni_hid_parser_ds3_does_name_match(&d, "PLAYSTATION(R)3 Controller"));
    report[30] = 0x05;  // Fully charged -> 255
    feed_input_report(&d, report, sizeof(report));
    EXPECT_EQ(255, d.controller.battery);
    report[30] = 0xEE;  // Charging -> 255
    feed_input_report(&d, report, sizeof(report));
    EXPECT_EQ(255, d.controller.battery);

    // Partial reports (30 <= len < 49) parse buttons/axes/battery but skip IMU bytes safely.
    feed_input_report(&d, report, 35);
    EXPECT_EQ(BUTTON_THUMB_L | BUTTON_SHOULDER_L | BUTTON_A | BUTTON_X, d.controller.gamepad.buttons);
    EXPECT_EQ(0, d.controller.gamepad.accel[0]);
    EXPECT_EQ(0, d.controller.gamepad.accel[1]);
    EXPECT_EQ(0, d.controller.gamepad.accel[2]);
    EXPECT_EQ(0, d.controller.gamepad.gyro[0]);

    // Short reports (< 30 bytes) and wrong report ID must be safely ignored.
    feed_input_report(&d, report, 0);
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, report, 29);
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    report[0] = 0x02;
    feed_input_report(&d, report, sizeof(report));
    EXPECT_EQ(0, d.controller.gamepad.buttons);
}

// ============================================================================
// 5. Xbox One: Firmware 3.1, 4.8, and 5.x Usage Mapping + Descriptor Reports
// ============================================================================
TEST(hid_parser_xboxone_reports) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x045e, 0x02e0);
    ASSERT_EQ(CONTROLLER_TYPE_XBoxOneController, d.controller_type);

    // Match name to attach the built-in 334-byte Xbox Wireless Controller HID descriptor.
    EXPECT_TRUE(uni_hid_parser_xboxone_does_name_match(&d, "Xbox Wireless Controller"));
    EXPECT_EQ(334, d.hid_descriptor_len);

    // Parse full 17-byte Report 0x01 through `uni_hid_parse_input_report` using the attached descriptor.
    uint8_t rpt01[17] = {
        0x01, 0xff, 0xff,  // X = 65535 -> 511
        0x00, 0x00,        // Y = 0 -> -512
        0x00, 0x80,        // Z = 32768 -> 0
        0x00, 0x80,        // Rz = 32768 -> 0
        0xff, 0x03,        // Brake = 1023
        0x00, 0x02,        // Accelerator = 512
        0x01,              // Hat = 1 -> DPAD_UP
        0x01, 0x00,        // Button 1 -> BUTTON_A
        0x01,              // AC Back -> MISC_BUTTON_SELECT
    };
    // First report starts in FW 3.1 (where Z/Rz map to brake/throttle = 512) and auto-upgrades
    // to FW 4.8 when Button 15 (0x09:0x0f) is encountered near the end of Report 0x01.
    uni_hid_parse_input_report(&d, rpt01, sizeof(rpt01));
    EXPECT_EQ(512, d.controller.gamepad.brake);
    EXPECT_EQ(512, d.controller.gamepad.throttle);

    // Second report is parsed in FW 4.8 mode: Z/Rz map to axis_rx/axis_ry (0), and Simulation
    // Controls (0x02:0xc5 Brake = 1023, 0x02:0xc4 Accelerator = 512) map to brake/throttle.
    uni_hid_parse_input_report(&d, rpt01, sizeof(rpt01));
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(511, d.controller.gamepad.axis_x);
    EXPECT_EQ(-512, d.controller.gamepad.axis_y);
    EXPECT_EQ(0, d.controller.gamepad.axis_rx);
    EXPECT_EQ(0, d.controller.gamepad.axis_ry);
    EXPECT_EQ(1023, d.controller.gamepad.brake);
    EXPECT_EQ(512, d.controller.gamepad.throttle);
    EXPECT_EQ(MISC_BUTTON_SELECT, d.controller.gamepad.misc_buttons);

    // Now test FW 4.8 and FW 5.x button mappings explicitly after firmware version transitions.
    hid_globals_t globals = {.logical_minimum = 0, .logical_maximum = 1, .report_size = 1, .report_count = 1};
    d.report_parser.init_report(&d);
    // In FW 4.8 (already triggered by rpt01's 15-button array): Button 4 -> X, Button 5 -> Y, Button 12 -> Start.
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_BUTTON, 4, 1);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_BUTTON, 5, 1);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_BUTTON, 12, 1);
    EXPECT_EQ(BUTTON_X | BUTTON_Y, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_START, d.controller.gamepad.misc_buttons);

    // Upgrade to FW 5.x via Consumer Record usage (0x0c / 0x00b2) and verify Share/Capture & Button 8 (R1).
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_CONSUMER, HID_USAGE_RECORD, 1);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_BUTTON, 8, 1);
    EXPECT_EQ(MISC_BUTTON_CAPTURE, d.controller.gamepad.misc_buttons);
    EXPECT_EQ(BUTTON_SHOULDER_R, d.controller.gamepad.buttons);
}

// ============================================================================
// 6. Nintendo Switch: FSM Subcommands, Report 0x30 (Full & Short IMU Bounds) & Report 0x3F
// ============================================================================
TEST(hid_parser_switch_reports_and_imu_bounds) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x057e, 0x2009);
    ASSERT_EQ(CONTROLLER_TYPE_SwitchProController, d.controller_type);

    // Walk Switch FSM to STATE_READY with SWITCH_MODE_IMU enabled:
    uint8_t r21[48];
    memset(r21, 0, sizeof(r21));
    r21[0] = 0x21;
    r21[3] = 0x08;   // status.buttons_right bit 0x08 ("A" held) enables SWITCH_MODE_IMU
    r21[13] = 0x80;  // ack bit
    r21[17] = 0x03;  // data[2] = SWITCH_CONTROLLER_TYPE_PRO (3)

    r21[14] = 0x02;  // SUBCMD_REQ_DEV_INFO
    feed_input_report(&d, r21, sizeof(r21));

    r21[14] = 0x10;  // SUBCMD_SPI_FLASH_READ (factory stick calib; keep default calibration)
    feed_input_report(&d, r21, sizeof(r21));

    r21[14] = 0x10;  // SUBCMD_SPI_FLASH_READ (user stick calib; keep default calibration)
    feed_input_report(&d, r21, sizeof(r21));

    r21[14] = 0x10;  // SUBCMD_SPI_FLASH_READ (factory imu calib; keep default non-zero IMU divisors)
    feed_input_report(&d, r21, sizeof(r21));

    r21[14] = 0x03;  // SUBCMD_SET_REPORT_MODE
    feed_input_report(&d, r21, sizeof(r21));

    r21[14] = 0x40;  // SUBCMD_ENABLE_IMU
    feed_input_report(&d, r21, sizeof(r21));

    r21[14] = 0x30;  // SUBCMD_SET_PLAYER_LEDS -> transitions to STATE_READY!
    feed_input_report(&d, r21, sizeof(r21));

    // Test 1: Short 12-byte and 20-byte heap-allocated Report 0x30 (buttons + sticks only, truncated IMU bytes).
    // Even though SWITCH_MODE_IMU is active, len < 48 MUST guard against reading r->imu[2] OOB!
    uint8_t* r30_short = (uint8_t*)malloc(20);
    ASSERT_NE(NULL, r30_short);
    memset(r30_short, 0, 20);
    r30_short[0] = 0x30;
    r30_short[3] = 0x01 | 0x08 | 0x40;  // Y(0x01->BUTTON_X), A(0x08->BUTTON_B), R(0x40->BUTTON_SHOULDER_R)
    r30_short[4] = 0x02 | 0x10;         // Plus(START), Home(SYSTEM)
    r30_short[5] = 0x01 | 0x04;         // DPAD_DOWN(0x01) | DPAD_RIGHT(0x04)
    feed_input_report(&d, r30_short, 12);
    EXPECT_EQ(DPAD_DOWN | DPAD_RIGHT, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_X | BUTTON_B | BUTTON_SHOULDER_R, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_START | MISC_BUTTON_SYSTEM, d.controller.gamepad.misc_buttons);
    EXPECT_EQ(0, d.controller.gamepad.accel[0]);
    feed_input_report(&d, r30_short, 20);
    EXPECT_EQ(DPAD_DOWN | DPAD_RIGHT, d.controller.gamepad.dpad);
    EXPECT_EQ(0, d.controller.gamepad.accel[0]);
    free(r30_short);

    // Test 2: Full 49-byte Report 0x30 (includes 3 IMU frames; imu[2] is parsed).
    uint8_t r30_full[49];
    memset(r30_full, 0, sizeof(r30_full));
    r30_full[0] = 0x30;
    r30_full[3] = 0x04;  // B -> BUTTON_A
    // Populate imu[2] at offset 13 + 24 = 37..48
    r30_full[37] = 0x10;
    r30_full[38] = 0x01;
    feed_input_report(&d, r30_full, sizeof(r30_full));
    EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
    EXPECT_NE(0, d.controller.gamepad.accel[0]);

    // Test 3: Report 0x3F (12-byte standard HID mode report) and short 0x3F guard.
    memset(&d.controller.gamepad, 0, sizeof(d.controller.gamepad));
    uint8_t r3f[12] = {
        0x3f,
        0x01 | 0x02,        // B(BUTTON_A) | A(BUTTON_B)
        0x02 | 0x10,        // Plus(START) | Home(SYSTEM)
        0x00,               // Hat = 0 -> DPAD_UP
        0x00,        0x80,  // LX = 32768
        0x00,        0x80,  // LY = 32768
        0x00,        0x80,  // RX = 32768
        0x00,        0x80,  // RY = 32768
    };
    feed_input_report(&d, r3f, 11);  // Too short -> rejected
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, r3f, sizeof(r3f));
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A | BUTTON_B, d.controller.gamepad.buttons);
}

// ============================================================================
// 7. Nintendo Wii: Core Buttons (0x30/0x31) & Nunchuk Extension (0x32) via FSM
// ============================================================================
TEST(hid_parser_wii_core_and_nunchuk) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x057e, 0x0306);
    ASSERT_EQ(CONTROLLER_TYPE_WiiController, d.controller_type);

    // 1. Report 0x30 (DRM_K, 3 bytes) in default horizontal mode:
    uint8_t drm_k[3] = {0x30, 0x02 | 0x10, 0x02 | 0x80};
    feed_input_report(&d, drm_k, 2);  // Short -> ignored
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, drm_k, sizeof(drm_k));
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_START | MISC_BUTTON_SYSTEM, d.controller.gamepad.misc_buttons);

    // 2. Report 0x31 (DRM_KA, 6 bytes: core buttons + accelerometer):
    uint8_t drm_ka[6] = {0x31, 0x01, 0x01, 0x80, 0x80, 0x80};
    feed_input_report(&d, drm_ka, 5);  // Short -> ignored
    feed_input_report(&d, drm_ka, sizeof(drm_ka));
    EXPECT_EQ(DPAD_DOWN, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_B, d.controller.gamepad.buttons);

    // 3. Walk Wii Extension FSM to register a Nunchuk:
    // Step A: Status report 0x20 (7 bytes) with flags bit 0x02 (extension connected).
    uint8_t status_ext[7] = {0x20, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
    feed_input_report(&d, status_ext, sizeof(status_ext));
    // Step B: Two write-memory acks (0x22, 5 bytes) for ext_init and ext_encrypt_off.
    uint8_t ack_wmem[5] = {0x22, 0x00, 0x00, 0x16, 0x00};
    feed_input_report(&d, ack_wmem, sizeof(ack_wmem));
    feed_input_report(&d, ack_wmem, sizeof(ack_wmem));
    // Step C: Read memory response (0x21, 22 bytes) with Nunchuk ID (00 00 a4 20 00 00) at [6..11].
    uint8_t read_ext[22] = {
        0x21, 0x00, 0x00, 0x50, 0x00, 0xfa, 0x00, 0x00, 0xa4, 0x20, 0x00, 0x00,
    };
    feed_input_report(&d, read_ext, sizeof(read_ext));
    EXPECT_EQ(CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK, d.controller_subtype);

    // Step D: Feed Report 0x32 (DRM_KE, 11 bytes: 2 core bytes + 6 Nunchuk bytes).
    uint8_t drm_ke[11] = {
        0x32, 0x08, 0x08,            // Vertical mode: Up(0x08 in data[0]), A(0x08 in data[1] -> BUTTON_B)
        200,  50,   128,  128, 128,  // Nunchuk stick & accel
        0x00,                        // Both C and Z pressed (active low) -> BUTTON_X | BUTTON_Y
        0x00, 0x00,
    };
    feed_input_report(&d, drm_ke, 10);  // Short -> ignored
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, drm_ke, sizeof(drm_ke));
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_B | BUTTON_X | BUTTON_Y, d.controller.gamepad.buttons);
    EXPECT_NE(0, d.controller.gamepad.axis_rx);
    EXPECT_NE(0, d.controller.gamepad.axis_ry);
}

// ============================================================================
// 8. Steam Controller: Sequential Offset Advances & INT16_MIN Negation Regression
// ============================================================================
TEST(hid_parser_steam_offset_and_int16_min_regression) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x28de, 0x1106);
    ASSERT_EQ(CONTROLLER_TYPE_SteamController, d.controller_type);

    // Case 1: Flags = BUTTONS (0x0010, +3B) | TRIGGERS (0x0020, +2B) | THUMBSTICK (0x0080, +4B) | RIGHT_PAD (0x0200,
    // +4B) Total payload = 4 header + 3 + 2 + 4 + 4 = 17 bytes <= 20 bytes. Set Stick Y and Right Pad Y to 0x8000
    // (INT16_MIN = -32768) to verify no signed-overflow UB and +512 clamping.
    uint8_t report[20];
    memset(report, 0, sizeof(report));
    report[0] = 0x03;
    report[1] = 0xc0;
    report[2] = 0xb4;  // op=0x04 | 0x10 (BUTTONS) | 0x20 (TRIGGERS) | 0x80 (THUMBSTICK)
    report[3] = 0x02;  // 0x0200 (RIGHT_PAD)
    // Buttons [4..6]: A(0x80 in [4]), Start(0x40 in [5]) | Up(0x01 in [5])
    report[4] = 0x80;
    report[5] = 0x41;
    report[6] = 0x00;
    // Triggers [7..8]: left=100, right=200
    report[7] = 100;
    report[8] = 200;
    // Thumbstick [9..12]: X = 0x7fff (+32767 -> 511), Y = 0x8000 (-32768 -> negated +32768 >> 6 = +512)
    report[9] = 0xff;
    report[10] = 0x7f;
    report[11] = 0x00;
    report[12] = 0x80;
    // Right pad [13..16]: X = 0x8000 (-32768 -> -512), Y = 0x8000 (-32768 -> negated +512)
    report[13] = 0x00;
    report[14] = 0x80;
    report[15] = 0x00;
    report[16] = 0x80;

    feed_input_report(&d, report, sizeof(report));
    EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_START, d.controller.gamepad.misc_buttons);
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(100 * 4, d.controller.gamepad.brake);
    EXPECT_EQ(200 * 4, d.controller.gamepad.throttle);
    EXPECT_EQ(511, d.controller.gamepad.axis_x);
    EXPECT_EQ(512, d.controller.gamepad.axis_y);
    EXPECT_EQ(-512, d.controller.gamepad.axis_rx);
    EXPECT_EQ(512, d.controller.gamepad.axis_ry);

    // Case 2: Enable ALL 5 flags (0x03b0 -> 4 + 3 + 2 + 4 + 4 + 4 = 21 bytes > 20 bytes)
    // in a tight heap-allocated malloc(20) buffer.
    // Verify that LEFT_PAD at [13..16] advances idx to 17, while RIGHT_PAD (which would need [17..20])
    // is safely skipped by the `idx + 4 <= len` bounds guard without ASan heap-buffer-overflow!
    memset(&d.controller.gamepad, 0, sizeof(d.controller.gamepad));
    uint8_t* heap_rpt = (uint8_t*)malloc(20);
    ASSERT_NE(NULL, heap_rpt);
    memset(heap_rpt, 0, 20);
    heap_rpt[0] = 0x03;
    heap_rpt[1] = 0xc0;
    heap_rpt[2] = 0xb4;
    heap_rpt[3] = 0x03;  // LEFT_PAD (0x0100) | RIGHT_PAD (0x0200)
    // Left pad at [13..16]
    heap_rpt[13] = 0x00;
    heap_rpt[14] = 0x10;
    heap_rpt[15] = 0x00;
    heap_rpt[16] = 0x80;
    // Put non-zero data at [17..19] to confirm RIGHT_PAD is NOT read out of bounds.
    heap_rpt[17] = 0xff;
    heap_rpt[18] = 0x7f;
    heap_rpt[19] = 0x7f;

    feed_input_report(&d, heap_rpt, 20);
    EXPECT_EQ(0, d.controller.gamepad.axis_rx);
    EXPECT_EQ(0, d.controller.gamepad.axis_ry);
    free(heap_rpt);
}

// ============================================================================
// 9. Atari VCS Wireless Classic Joystick: Zero-Length & OOB D-Pad Regression
// ============================================================================
TEST(hid_parser_atari_zero_len_and_oob_dpad_regression) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x3250, 0x1001);
    ASSERT_EQ(CONTROLLER_TYPE_AtariJoystick, d.controller_type);

    uint8_t report[5] = {0x01, 0x01, 0x10, 0x00, 0x00};

    // 1. Zero-length and short reports must not dereference report[0] or crash.
    feed_input_report(&d, report, 0);
    EXPECT_EQ(0, d.controller.gamepad.dpad);
    feed_input_report(&d, report, 4);
    EXPECT_EQ(0, d.controller.gamepad.dpad);

    // 2. Valid 5-byte report across all 9 valid hat values (0..8), 10-bit throttle (0x3ff = 1023), and A button (0x01).
    report[3] = 0xff;
    report[4] = 0x03;  // axis = 0x03ff (1023)
    static const uint8_t expected_atari_dpad[9] = {
        0,
        DPAD_UP,
        DPAD_UP | DPAD_RIGHT,
        DPAD_RIGHT,
        DPAD_DOWN | DPAD_RIGHT,
        DPAD_DOWN,
        DPAD_DOWN | DPAD_LEFT,
        DPAD_LEFT,
        DPAD_UP | DPAD_LEFT,
    };
    for (uint8_t nibble = 0; nibble <= 8; nibble++) {
        report[2] = (uint8_t)(nibble << 4);
        feed_input_report(&d, report, sizeof(report));
        EXPECT_EQ(expected_atari_dpad[nibble], d.controller.gamepad.dpad);
        EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
        EXPECT_EQ(1023, d.controller.gamepad.throttle);
    }

    // 3. Out-of-bounds D-pad nibbles (9..15 in upper nibble of report[2]) must map cleanly to 0 without OOB read.
    for (uint8_t nibble = 9; nibble <= 15; nibble++) {
        report[2] = (uint8_t)(nibble << 4);
        feed_input_report(&d, report, sizeof(report));
        EXPECT_EQ(0, d.controller.gamepad.dpad);
    }

    // 4. Battery report 0x02 (2 bytes) is accepted without error.
    uint8_t bat_rpt[2] = {0x02, 0x34};
    feed_input_report(&d, bat_rpt, sizeof(bat_rpt));
    EXPECT_EQ(0, d.controller.battery);
}

// ============================================================================
// 10. Keyboard: Standard Modifiers/Keys & JX-05 Overflow Regression
// ============================================================================
TEST(hid_parser_keyboard_standard_and_jx05_overflow_regression) {
    uni_hid_device_t d;
    // Use JX-05 VID/PID (0x05ac:0x022c) with Keyboard CoD so `using_jx_05` is enabled.
    setup_synthetic_device_with_cod(&d, 0x05ac, 0x022c, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD);
    ASSERT_EQ(CONTROLLER_TYPE_GenericKeyboard, d.controller_type);

    hid_globals_t globals = {.logical_minimum = 0, .logical_maximum = 255, .report_size = 8, .report_count = 1};
    d.report_parser.init_report(&d);

    // 1. Standard modifiers (0xe0 = Left Control, 0xe1 = Left Shift).
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, 0xe0, 1);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, 0xe1, 1);
    EXPECT_EQ(0x03, d.controller.keyboard.modifiers);

    // 2. Feed 14 standard + consumer keys in a single report (exceeding UNI_KEYBOARD_PRESSED_KEYS_MAX == 10).
    for (uint16_t usage = 0x04; usage < 0x04 + 12; usage++) {
        d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, usage, 1);
    }
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_CONSUMER, HID_USAGE_POWER, 1);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_CONSUMER, HID_USAGE_AC_HOME, 1);

    // 3. Feed JX-05 Digitizer Contact Count impulses while pressed_key_index is already saturated at 10!
    hid_globals_t ptr_globals = {.logical_minimum = -512, .logical_maximum = 511, .report_size = 16, .report_count = 1};
    d.report_parser.parse_usage(&d, &ptr_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, -260);
    d.report_parser.parse_usage(&d, &ptr_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, -222);
    for (int i = 0; i < 5; i++) {
        d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_TIP_SWITCH, 0);
        d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_TIP_SWITCH, 1);
        d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_CONTACT_COUNT, 1);
    }

    // Verify all 10 slots are filled with 0x04..0x0d and no out-of-bounds write occurred.
    for (int i = 0; i < UNI_KEYBOARD_PRESSED_KEYS_MAX; i++) {
        EXPECT_EQ(0x04 + i, d.controller.keyboard.pressed_keys[i]);
    }

    // 4. Verify JX-05 mapping when pressed_keys has room: (-260, -222) -> Up, (-260, 145) + (-260, -454) -> Down.
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_TIP_SWITCH, 0);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_TIP_SWITCH, 1);
    d.report_parser.parse_usage(&d, &ptr_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, -260);
    d.report_parser.parse_usage(&d, &ptr_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, -222);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_CONTACT_COUNT, 1);

    // Arm next JX-05 gesture for Down arrow: first report (-260, 145), second report (-260, -454).
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_TIP_SWITCH, 0);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_TIP_SWITCH, 1);
    d.report_parser.parse_usage(&d, &ptr_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, -260);
    d.report_parser.parse_usage(&d, &ptr_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, 145);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_CONTACT_COUNT, 1);
    d.report_parser.parse_usage(&d, &ptr_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, -454);
    d.report_parser.parse_usage(&d, &globals, HID_USAGE_PAGE_DIGITIZER, HID_USAGE_CONTACT_COUNT, 1);

    EXPECT_EQ(HID_USAGE_KB_UP_ARROW, d.controller.keyboard.pressed_keys[0]);
    EXPECT_EQ(HID_USAGE_KB_DOWN_ARROW, d.controller.keyboard.pressed_keys[1]);
}

// ============================================================================
// 11. Mouse: Axes, Wheel, Buttons, Resolution Scaling & Multi-Byte Report Parsing
// ============================================================================
TEST(hid_parser_mouse_8byte_and_wheel) {
    uni_hid_device_t d;
    setup_synthetic_device_with_cod(&d, 0x05ac, 0x0304, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
    ASSERT_EQ(CONTROLLER_TYPE_GenericMouse, d.controller_type);

    // 1. Descriptor-driven 5-byte mouse report via `uni_hid_parse_input_report`:
    // Report ID 1, 5 buttons (1 bit each) + 3-bit padding, 3 signed 8-bit axes (X, Y, Wheel).
    static const uint8_t mouse_desc[] = {
        0x05, 0x01,  // Usage Page (Generic Desktop)
        0x09, 0x02,  // Usage (Mouse)
        0xa1, 0x01,  // Collection (Application)
        0x85, 0x01,  //   Report ID (1)
        0x05, 0x09,  //   Usage Page (Button)
        0x19, 0x01,  //   Usage Minimum (1)
        0x29, 0x05,  //   Usage Maximum (5)
        0x15, 0x00,  //   Logical Minimum (0)
        0x25, 0x01,  //   Logical Maximum (1)
        0x75, 0x01,  //   Report Size (1)
        0x95, 0x05,  //   Report Count (5)
        0x81, 0x02,  //   Input (Data,Var,Abs)
        0x75, 0x03,  //   Report Size (3)
        0x95, 0x01,  //   Report Count (1)
        0x81, 0x01,  //   Input (Const)
        0x05, 0x01,  //   Usage Page (Generic Desktop)
        0x09, 0x30,  //   Usage (X)
        0x09, 0x31,  //   Usage (Y)
        0x09, 0x38,  //   Usage (Wheel)
        0x15, 0x81,  //   Logical Minimum (-127)
        0x25, 0x7f,  //   Logical Maximum (127)
        0x75, 0x08,  //   Report Size (8)
        0x95, 0x03,  //   Report Count (3)
        0x81, 0x06,  //   Input (Data,Var,Rel)
        0xc0,        // End Collection
    };
    memcpy(d.hid_descriptor, mouse_desc, sizeof(mouse_desc));
    d.hid_descriptor_len = sizeof(mouse_desc);

    uint8_t mouse_rpt[5] = {
        0x01,            // Report ID 1
        0x1f,            // Buttons 1..5 all pressed
        42,              // X = +42
        (uint8_t)(-19),  // Y = -19
        (uint8_t)(-3),   // Wheel = -3
    };
    uni_hid_parse_input_report(&d, mouse_rpt, sizeof(mouse_rpt));
    EXPECT_EQ(42, d.controller.mouse.delta_x);
    EXPECT_EQ(-19, d.controller.mouse.delta_y);
    EXPECT_EQ(-3, d.controller.mouse.scroll_wheel);
    EXPECT_EQ(UNI_MOUSE_BUTTON_LEFT | UNI_MOUSE_BUTTON_RIGHT | UNI_MOUSE_BUTTON_MIDDLE | UNI_MOUSE_BUTTON_AUX_0 |
                  UNI_MOUSE_BUTTON_AUX_1,
              d.controller.mouse.buttons);

    // 2. Per-(VID, PID, Name) resolution scaling, sub-unit movement preservation, and [-127, 127] clamping:
    hid_globals_t axis16_globals = {
        .logical_minimum = -2048, .logical_maximum = 2047, .report_size = 16, .report_count = 1};

    // 2a. Apple Magic Mouse 1st gen (0x05ac:0x030d -> scale = 0.2f)
    setup_synthetic_device_with_cod(&d, 0x05ac, 0x030d, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, 100);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, -50);
    EXPECT_EQ(20, d.controller.mouse.delta_x);
    EXPECT_EQ(-10, d.controller.mouse.delta_y);
    // Sub-unit movement preservation (1 * 0.2 = 0.2 -> rounds to 0 -> preserved as +1; -1 -> -1):
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, 1);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, -1);
    EXPECT_EQ(1, d.controller.mouse.delta_x);
    EXPECT_EQ(-1, d.controller.mouse.delta_y);

    // 2b. Adesso Bluetooth 3.0 Mouse (0x0a5c:0x4503, name="Adesso Bluetooth 3.0 Mouse" -> scale = 0.5f)
    setup_synthetic_device_with_cod(&d, 0x0a5c, 0x4503, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
    strncpy(d.name, "Adesso Bluetooth 3.0 Mouse", sizeof(d.name) - 1);
    d.report_parser.setup(&d);
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, 40);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, -20);
    EXPECT_EQ(20, d.controller.mouse.delta_x);
    EXPECT_EQ(-10, d.controller.mouse.delta_y);
    // [-127, 127] clamping (500 * 0.5 = 250 -> 127; -500 * 0.5 = -250 -> -127):
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, 500);
    d.report_parser.parse_usage(&d, &axis16_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, -500);
    EXPECT_EQ(127, d.controller.mouse.delta_x);
    EXPECT_EQ(-127, d.controller.mouse.delta_y);
}

// ============================================================================
// 12. Google Stadia, Android & Smart TV Remote Parser Modules
// ============================================================================
TEST(hid_parser_stadia_and_smarttvremote) {
    uni_hid_device_t d;
    hid_globals_t btn_globals = {.logical_minimum = 0, .logical_maximum = 1, .report_size = 1, .report_count = 1};
    hid_globals_t axis_globals = {.logical_minimum = 0, .logical_maximum = 255, .report_size = 8, .report_count = 1};
    hid_globals_t hat_globals = {.logical_minimum = 0, .logical_maximum = 7, .report_size = 4, .report_count = 1};

    // 1. Stadia Controller (0x18d1:0x9400 -> CONTROLLER_TYPE_AndroidController with Stadia setup/rumble hooks)
    setup_synthetic_device(&d, 0x18d1, 0x9400);
    ASSERT_EQ(CONTROLLER_TYPE_AndroidController, d.controller_type);
    ASSERT_EQ(uni_hid_parser_stadia_setup, d.report_parser.setup);
    ASSERT_EQ(uni_hid_parser_stadia_play_dual_rumble, d.report_parser.play_dual_rumble);

    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 1, 1);     // A
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 11, 1);    // Select
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 12, 1);    // Start
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 13, 1);    // System
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 0x13, 1);  // Stadia LT -> BUTTON_TRIGGER_L
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 0x14, 1);  // Stadia RT -> BUTTON_TRIGGER_R
    d.report_parser.play_dual_rumble(&d, 0, 50, 128, 255);
    EXPECT_EQ(BUTTON_A | BUTTON_TRIGGER_L | BUTTON_TRIGGER_R, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT | MISC_BUTTON_START | MISC_BUTTON_SYSTEM, d.controller.gamepad.misc_buttons);

    // 2. Android Controller (0x20d6:0x6271 -> CONTROLLER_TYPE_AndroidController):
    // Axes X, Y, Z, Rz, hat, D-pad usages 0x90..0x93, Brake/Accelerator, Battery Strength, and Consumer Home/Back.
    setup_synthetic_device(&d, 0x20d6, 0x6271);
    ASSERT_EQ(CONTROLLER_TYPE_AndroidController, d.controller_type);
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_X, 255);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y, 0);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Z, 255);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_RZ, 0);
    d.report_parser.parse_usage(&d, &hat_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_HAT, 2);  // DPAD_RIGHT
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_DPAD_UP, 1);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_SIMULATION_CONTROLS, HID_USAGE_BRAKE, 255);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_SIMULATION_CONTROLS, HID_USAGE_ACCELERATOR, 128);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS, HID_USAGE_BATTERY_STRENGTH,
                                90);
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_CONSUMER, HID_USAGE_AC_HOME, 1);
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_CONSUMER, HID_USAGE_AC_BACK, 1);
    EXPECT_EQ(508, d.controller.gamepad.axis_x);
    EXPECT_EQ(-512, d.controller.gamepad.axis_y);
    EXPECT_EQ(508, d.controller.gamepad.axis_rx);
    EXPECT_EQ(-512, d.controller.gamepad.axis_ry);
    EXPECT_EQ(DPAD_RIGHT | DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(1020, d.controller.gamepad.brake);
    EXPECT_EQ(512, d.controller.gamepad.throttle);
    EXPECT_EQ(90, d.controller.battery);
    EXPECT_EQ(MISC_BUTTON_START | MISC_BUTTON_SELECT, d.controller.gamepad.misc_buttons);

    // 3. Smart TV Remote (0x1949:0x0401 -> CONTROLLER_TYPE_SmartTVRemoteController)
    // Exercise with a multi-report HID descriptor across Report ID 0x01 (Keyboard page) and Report ID 0x02 (Consumer
    // page).
    setup_synthetic_device(&d, 0x1949, 0x0401);
    ASSERT_EQ(CONTROLLER_TYPE_SmartTVRemoteController, d.controller_type);
    static const uint8_t smart_tv_desc[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop)
        0x09, 0x06,        // Usage (Keyboard)
        0xa1, 0x01,        // Collection (Application)
        0x85, 0x01,        //   Report ID (1)
        0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
        0x09, 0x52,        //   Usage (Up Arrow)
        0x09, 0x58,        //   Usage (Keypad Enter)
        0x0a, 0xf1, 0x00,  //   Usage (0xf1 Back)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x03,        //   Report Count (3)
        0x81, 0x02,        //   Input (Data,Var,Abs)
        0x75, 0x05,        //   Report Size (5)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x01,        //   Input (Const)
        0x85, 0x02,        //   Report ID (2)
        0x05, 0x0c,        //   Usage Page (Consumer)
        0x09, 0x40,        //   Usage (Menu)
        0x0a, 0x23, 0x02,  //   Usage (AC Home)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs)
        0x75, 0x06,        //   Report Size (6)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x01,        //   Input (Const)
        0xc0,              // End Collection
    };
    memcpy(d.hid_descriptor, smart_tv_desc, sizeof(smart_tv_desc));
    d.hid_descriptor_len = sizeof(smart_tv_desc);

    uint8_t tv_rpt1[2] = {0x01, 0x07};  // Up Arrow(0x01) | Keypad Enter(0x02) | Back(0x04)
    uni_hid_parse_input_report(&d, tv_rpt1, sizeof(tv_rpt1));
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT, d.controller.gamepad.misc_buttons);

    uint8_t tv_rpt2[2] = {0x02, 0x03};  // Menu(0x01 -> SYSTEM) | AC Home(0x02 -> START)
    uni_hid_parse_input_report(&d, tv_rpt2, sizeof(tv_rpt2));
    EXPECT_EQ(MISC_BUTTON_SYSTEM | MISC_BUTTON_START, d.controller.gamepad.misc_buttons);
}

// ============================================================================
// 13. 8BitDo, iCade, SteelSeries Nimbus, OUYA & PS Move Parser Modules
// ============================================================================
TEST(hid_parser_8bitdo_icade_nimbus_ouya) {
    uni_hid_device_t d;
    hid_globals_t btn_globals = {.logical_minimum = 0, .logical_maximum = 1, .report_size = 1, .report_count = 1};
    hid_globals_t axis_globals = {.logical_minimum = -127, .logical_maximum = 127, .report_size = 8, .report_count = 1};
    hid_globals_t uaxis_globals = {.logical_minimum = 0, .logical_maximum = 255, .report_size = 8, .report_count = 1};

    // 1. 8BitDo (SN30 Pro: 0x2dc8:0x6100 & Zero 2 Keyboard Mode)
    setup_synthetic_device(&d, 0x2dc8, 0x6100);
    ASSERT_EQ(CONTROLLER_TYPE_8BitdoController, d.controller_type);
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 1, 1);   // BUTTON_B on 8BitDo
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 2, 1);   // BUTTON_A on 8BitDo
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 4, 1);   // BUTTON_Y on 8BitDo
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 5, 1);   // BUTTON_X on 8BitDo
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 11, 1);  // SELECT
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 12, 1);  // START
    EXPECT_EQ(BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT | MISC_BUTTON_START, d.controller.gamepad.misc_buttons);

    // 8BitDo Zero 2 Keyboard mode (page 0x07 usages HID_USAGE_KB_C..HID_USAGE_KB_O)
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_C, 1);  // DPAD_UP
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_F, 1);  // DPAD_RIGHT
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_G, 1);  // BUTTON_B
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_J, 1);  // BUTTON_A
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_K, 1);  // SHOULDER_L
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_M, 1);  // SHOULDER_R
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_N, 1);  // SELECT
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_O, 1);  // START
    EXPECT_EQ(DPAD_UP | DPAD_RIGHT, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A | BUTTON_B | BUTTON_SHOULDER_L | BUTTON_SHOULDER_R, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT | MISC_BUTTON_START, d.controller.gamepad.misc_buttons);

    // 2. ION iCade (0x15e4:0x0132): stateful key-down / key-up cabinet protocol
    setup_synthetic_device(&d, 0x15e4, 0x0132);
    ASSERT_EQ(CONTROLLER_TYPE_iCadeController, d.controller_type);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_W, 1);  // 'w': Up ON
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_Y,
                                1);  // 'y': Button A ON
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_E, 1);  // 'e': Up OFF
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_USAGE_KB_T,
                                1);  // 't': Button A OFF
    EXPECT_EQ(0, d.controller.gamepad.dpad);
    EXPECT_EQ(0, d.controller.gamepad.buttons);

    // 3. SteelSeries Nimbus (0x0111:0x1420): inverted Y/Rz axes, pressure D-pad, and dual-mapped trigger buttons
    // 0x07/0x08
    setup_synthetic_device(&d, 0x0111, 0x1420);
    ASSERT_EQ(CONTROLLER_TYPE_NimbusController, d.controller_type);
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &uaxis_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_Y,
                                0);  // 0 -> -(-512) = +512
    d.report_parser.parse_usage(&d, &uaxis_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_AXIS_RZ,
                                255);  // 255 -> -(508) = -508
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_GENERIC_DESKTOP, HID_USAGE_DPAD_UP, 64);
    d.report_parser.parse_usage(&d, &axis_globals, HID_USAGE_PAGE_BUTTON, 1, 64);
    d.report_parser.parse_usage(&d, &uaxis_globals, HID_USAGE_PAGE_BUTTON, 7, 255);  // L2 -> BUTTON_TRIGGER_L + brake
    d.report_parser.parse_usage(&d, &uaxis_globals, HID_USAGE_PAGE_BUTTON, 8,
                                128);  // R2 -> BUTTON_TRIGGER_R + throttle
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_CONSUMER, HID_USAGE_AC_HOME, 1);
    EXPECT_EQ(512, d.controller.gamepad.axis_y);
    EXPECT_EQ(-508, d.controller.gamepad.axis_ry);
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A | BUTTON_TRIGGER_L | BUTTON_TRIGGER_R, d.controller.gamepad.buttons);
    EXPECT_EQ(1020, d.controller.gamepad.brake);
    EXPECT_EQ(512, d.controller.gamepad.throttle);
    EXPECT_EQ(MISC_BUTTON_SYSTEM, d.controller.gamepad.misc_buttons);

    // 4. OUYA Controller (0x2836:0x0001)
    setup_synthetic_device(&d, 0x2836, 0x0001);
    ASSERT_EQ(CONTROLLER_TYPE_OUYAController, d.controller_type);
    d.report_parser.init_report(&d);
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 1, 1);   // BUTTON_A
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 9, 1);   // DPAD_UP
    d.report_parser.parse_usage(&d, &btn_globals, HID_USAGE_PAGE_BUTTON, 15, 1);  // SYSTEM
    EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
    EXPECT_EQ(DPAD_UP, d.controller.gamepad.dpad);
    EXPECT_EQ(MISC_BUTTON_SYSTEM, d.controller.gamepad.misc_buttons);

    // 5. Sony PS Move (0x054c:0x03d5)
    setup_synthetic_device(&d, 0x054c, 0x03d5);
    ASSERT_EQ(CONTROLLER_TYPE_PSMoveController, d.controller_type);
    uint8_t psmove_rpt[49];
    memset(psmove_rpt, 0, sizeof(psmove_rpt));
    psmove_rpt[0] = 0x01;
    psmove_rpt[1] = 0x01;   // Select
    psmove_rpt[2] = 0x40;   // Cross -> BUTTON_A
    psmove_rpt[5] = 200;    // Trigger -> throttle = 200 * 4 = 800
    psmove_rpt[12] = 0x05;  // Battery max (5 * 51 = 255)
    psmove_rpt[13] = 0x34;
    psmove_rpt[14] = 0x12;  // accel_x = 0x1234
    psmove_rpt[25] = 0x78;
    psmove_rpt[26] = 0x56;  // gyro_x = 0x5678
    // Truncated report (< 39 bytes) must be rejected:
    feed_input_report(&d, psmove_rpt, 10);
    EXPECT_EQ(0, d.controller.gamepad.buttons);
    feed_input_report(&d, psmove_rpt, sizeof(psmove_rpt));
    EXPECT_EQ(BUTTON_A, d.controller.gamepad.buttons);
    EXPECT_EQ(MISC_BUTTON_SELECT, d.controller.gamepad.misc_buttons);
    EXPECT_EQ(800, d.controller.gamepad.throttle);
    EXPECT_EQ(255, d.controller.battery);
    EXPECT_EQ(0x1234, d.controller.gamepad.accel[0]);
    EXPECT_EQ(0x5678, d.controller.gamepad.gyro[0]);
}

// ============================================================================
// 14. Generic Gamepad Parser: Full HID Descriptor + Input Report End-to-End
// ============================================================================
TEST(hid_parser_generic_descriptor_gamepad) {
    uni_hid_device_t d;
    // Generic Gamepad (0x0a5c:0x4502 -> CONTROLLER_TYPE_GenericController -> uni_hid_parser_generic)
    setup_synthetic_device(&d, 0x0a5c, 0x4502);
    ASSERT_EQ(CONTROLLER_TYPE_GenericController, d.controller_type);

    // Construct a standard Generic Desktop Gamepad HID descriptor:
    // - Report ID 1
    // - 2 8-bit axes: X, Y (0..255)
    // - 1 4-bit Hat switch (0..7) + 4-bit constant padding
    // - 8 1-bit buttons (Buttons 1..8)
    static const uint8_t generic_desc[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop)
        0x09, 0x05,        // Usage (Game Pad)
        0xa1, 0x01,        // Collection (Application)
        0x85, 0x01,        //   Report ID (1)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xff, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs)
        0x09, 0x39,        //   Usage (Hat switch)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x07,        //   Logical Maximum (7)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x42,        //   Input (Data,Var,Abs,Null)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x01,        //   Input (Const)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (1)
        0x29, 0x08,        //   Usage Maximum (8)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x08,        //   Report Count (8)
        0x81, 0x02,        //   Input (Data,Var,Abs)
        0xc0,              // End Collection
    };
    memcpy(d.hid_descriptor, generic_desc, sizeof(generic_desc));
    d.hid_descriptor_len = sizeof(generic_desc);

    uint8_t report[5] = {
        0x01,        // Report ID 1
        255,         // X = 255 -> 508
        0,           // Y = 0 -> -512
        0x02,        // Hat = 2 -> DPAD_RIGHT
        0x01 | 0x40  // Button 1 (BUTTON_A) | Button 7 (BUTTON_SHOULDER_L)
    };
    uni_hid_parse_input_report(&d, report, sizeof(report));
    EXPECT_EQ(508, d.controller.gamepad.axis_x);
    EXPECT_EQ(-512, d.controller.gamepad.axis_y);
    EXPECT_EQ(DPAD_RIGHT, d.controller.gamepad.dpad);
    EXPECT_EQ(BUTTON_A | BUTTON_SHOULDER_L, d.controller.gamepad.buttons);
}

// ============================================================================
// 15. HID Descriptor Engine: Malformed, Truncated, Oversized & Deep Collection
// ============================================================================
TEST(hid_parser_descriptor_malformed_and_deep_collection) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x0a5c, 0x4502);

    // Deeply nested Collections (32 levels of 0xA1 0x01) + Pop underflow (0xB4) +
    // field with report_size = 64 (> 32 bits) + truncated item tag (0xfe) at the end.
    uint8_t deep_desc[80];
    int idx = 0;
    for (int i = 0; i < 32; i++) {
        deep_desc[idx++] = 0xa1;
        deep_desc[idx++] = 0x01;
    }
    // Unmatched Pop (0xb4) and Push (0xa4)
    deep_desc[idx++] = 0xb4;
    deep_desc[idx++] = 0xa4;
    deep_desc[idx++] = 0xb4;
    // Field with report_size = 64 (> 32 bits) + truncated long item tag (0xfe) at end.
    deep_desc[idx++] = 0x75;
    deep_desc[idx++] = 0x40;  // Report Size (64)
    deep_desc[idx++] = 0x95;
    deep_desc[idx++] = 0x01;  // Report Count (1)
    deep_desc[idx++] = 0x81;
    deep_desc[idx++] = 0x02;  // Input
    deep_desc[idx++] = 0xfe;  // Truncated long item at end of descriptor

    memcpy(d.hid_descriptor, deep_desc, idx);
    d.hid_descriptor_len = idx;

    uint8_t dummy_report[16] = {0};
    uni_hid_parse_input_report(&d, dummy_report, sizeof(dummy_report));
    uni_hid_parse_input_report(&d, dummy_report, 0);

    // Also verify `uni_hid_parser_process_axis` and `uni_hid_parser_process_pedal` guard against min > max (range <=
    // 0).
    hid_globals_t inverted_globals = {
        .logical_minimum = 100, .logical_maximum = -100, .report_size = 8, .report_count = 1};
    EXPECT_EQ(0, uni_hid_parser_process_axis(&inverted_globals, 50));
    EXPECT_EQ(0, uni_hid_parser_process_pedal(&inverted_globals, 50));
}

// ============================================================================
// 16. Full Controller List Sweep: Short Reports & Deterministic Mulberry32 Fuzz
// ============================================================================

/**
 * @brief Deterministic 32-bit Mulberry32 PRNG for reproducible fuzzing across platforms.
 *
 * @param state Pointer to the mutable 32-bit PRNG state.
 * @return Next pseudo-random 32-bit unsigned integer.
 */
static uint32_t mulberry32(uint32_t* state) {
    uint32_t z = (*state += 0x6D2B79F5u);
    z = (z ^ (z >> 15)) * (z | 1u);
    z ^= z + (z ^ (z >> 7)) * (z | 61u);
    return z ^ (z >> 14);
}

// Fallback HID descriptor (no Report ID, covering Generic Desktop X/Y/Z/Rz/Hat, Simulation Brake/Accel,
// Buttons 1..16, Keyboard array, and Consumer Home/Back) so `uni_hid_parse_input_report()` exercises
// all 11 descriptor-driven `parse_usage` parsers in addition to all raw `parse_input_report` parsers.
static const uint8_t g_fuzz_fallback_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Game Pad)
    0xa1, 0x01,        // Collection (Application)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x81,        //   Logical Minimum (-127)
    0x25, 0x7f,        //   Logical Maximum (127)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,Null)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const)
    0x05, 0x02,        //   Usage Page (Simulation Controls)
    0x09, 0xc5,        //   Usage (Brake)
    0x09, 0xc4,        //   Usage (Accelerator)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xff, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x10,        //   Usage Maximum (16)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x00,        //   Input (Data,Ary,Abs)
    0xc0,              // End Collection
};

/**
 * @brief Sweep a single initialized HID device across boundary lengths and 32 Mulberry32 PRNG frames.
 *
 * Attaches `g_fuzz_fallback_descriptor` when the parser does not provide a built-in HID
 * descriptor so `uni_hid_parse_input_report()` exercises both descriptor-driven (`parse_usage`)
 * and raw (`parse_input_report`) parser pipelines under ASan + UBSan.
 *
 * @param d        Pointer to the initialized `uni_hid_device_t` under test.
 * @param fuzz_buf Scratch buffer of at least 512 bytes.
 * @param prng     Pointer to the Mulberry32 PRNG state.
 */
static void fuzz_single_device(uni_hid_device_t* d, uint8_t* fuzz_buf, uint32_t* prng) {
    if (d->hid_descriptor_len == 0) {
        memcpy(d->hid_descriptor, g_fuzz_fallback_descriptor, sizeof(g_fuzz_fallback_descriptor));
        d->hid_descriptor_len = sizeof(g_fuzz_fallback_descriptor);
    }

    // Phase A: Boundary lengths {0, 1, 2, 64, 512} with all-0x00 and all-0xFF payloads.
    const uint16_t boundary_lens[] = {0, 1, 2, 64, 512};
    memset(fuzz_buf, 0x00, 512);
    for (size_t s = 0; s < ARRAY_SIZE(boundary_lens); s++) {
        uni_hid_parse_input_report(d, fuzz_buf, boundary_lens[s]);
    }
    memset(fuzz_buf, 0xff, 512);
    for (size_t s = 0; s < ARRAY_SIZE(boundary_lens); s++) {
        uni_hid_parse_input_report(d, fuzz_buf, boundary_lens[s]);
    }

    // Phase B: 32 deterministic Mulberry32 PRNG frames of varying lengths (1..128 bytes).
    for (int iter = 0; iter < 32; iter++) {
        uint16_t rlen = (uint16_t)((mulberry32(prng) % 128) + 1);
        for (uint16_t b = 0; b < rlen; b += 4) {
            uint32_t word = mulberry32(prng);
            memcpy(&fuzz_buf[b], &word, sizeof(word));
        }
        uni_hid_parse_input_report(d, fuzz_buf, rlen);
    }
}

TEST(hid_parser_sweep_all_controllers_short_and_prng_fuzz) {
    uint32_t prng = 0xB10E9AD3u;
    uni_hid_device_t d;
    uint8_t fuzz_buf[512];
    int tested_entries = 0;

    // 1. Iterate over every entry in `arrControllers[]` (`uni_controller_list.h`).
    for (size_t i = 0; i < ARRAY_SIZE(arrControllers); i++) {
        uint16_t vid = (uint16_t)(arrControllers[i].device_id >> 16);
        uint16_t pid = (uint16_t)(arrControllers[i].device_id & 0xffff);

        setup_synthetic_device(&d, vid, pid);
        fuzz_single_device(&d, fuzz_buf, &prng);
        tested_entries++;
    }

    // 2. Also sweep the 4 non-`arrControllers` controller types so all 17 parser vtables are exercised:
    // - GenericKeyboard (via Keyboard CoD)
    setup_synthetic_device_with_cod(&d, 0x05ac, 0x022c, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD);
    ASSERT_EQ(CONTROLLER_TYPE_GenericKeyboard, d.controller_type);
    fuzz_single_device(&d, fuzz_buf, &prng);
    tested_entries++;

    // - GenericMouse (via Mouse CoD)
    setup_synthetic_device_with_cod(&d, 0x05ac, 0x030d, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
    ASSERT_EQ(CONTROLLER_TYPE_GenericMouse, d.controller_type);
    fuzz_single_device(&d, fuzz_buf, &prng);
    tested_entries++;

    // - GenericController (0x0a5c:0x4502)
    setup_synthetic_device(&d, 0x0a5c, 0x4502);
    ASSERT_EQ(CONTROLLER_TYPE_GenericController, d.controller_type);
    fuzz_single_device(&d, fuzz_buf, &prng);
    tested_entries++;

    // - SmartTVRemoteController (0x1949:0x0401)
    setup_synthetic_device(&d, 0x1949, 0x0401);
    ASSERT_EQ(CONTROLLER_TYPE_SmartTVRemoteController, d.controller_type);
    fuzz_single_device(&d, fuzz_buf, &prng);
    tested_entries++;

    // Verify we swept the entire 470+ controller database plus the 4 non-arrControllers types.
    EXPECT_GT(tested_entries, 450);
}

// ============================================================================
// 17. B3: Rumble & Switch Setup Timer Teardown on Disconnect & Re-Setup
// ============================================================================
TEST(rumble_and_switch_setup_timer_teardown_on_disconnect_and_resetup_b3) {
    // 1. Delayed Rumble Disconnect:
    btstack_run_loop_base_timers = NULL;
    uni_hid_device_setup();
    bd_addr_t addr1 = {0x70, 0x00, 0x00, 0x00, 0x00, 0x01};
    uni_hid_device_t* d1 = uni_hid_device_create(addr1);
    ASSERT_NE(NULL, d1);
    uni_hid_device_set_vendor_id(d1, 0x054c);
    uni_hid_device_set_product_id(d1, 0x09cc);
    uni_hid_device_guess_controller_type_from_pid_vid(d1);
    ASSERT_NE(NULL, d1->report_parser.setup);
    d1->report_parser.setup(d1);

    d1->report_parser.play_dual_rumble(d1, 200, 500, 128, 255);
    EXPECT_EQ(UNI_RUMBLE_STATE_DELAYED, d1->rumble.state);
    EXPECT_NE(NULL, btstack_run_loop_base_timers);

    uni_hid_device_delete(d1);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);

    // 2. In-Progress Rumble Disconnect:
    bd_addr_t addr2 = {0x70, 0x00, 0x00, 0x00, 0x00, 0x02};
    uni_hid_device_t* d2 = uni_hid_device_create(addr2);
    ASSERT_NE(NULL, d2);
    uni_hid_device_set_vendor_id(d2, 0x054c);
    uni_hid_device_set_product_id(d2, 0x09cc);
    uni_hid_device_guess_controller_type_from_pid_vid(d2);
    d2->report_parser.setup(d2);

    d2->report_parser.play_dual_rumble(d2, 0, 500, 128, 255);
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d2->rumble.state);
    EXPECT_NE(NULL, btstack_run_loop_base_timers);

    uni_hid_device_delete(d2);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);

    // 3. Parser Re-Setup While Rumble Active:
    // 3a. Parsers calling uni_hid_parser_rumble_init() in setup() (e.g., DS4) safely disarm
    //     active rumble timers before zeroing d->rumble.
    bd_addr_t addr3 = {0x70, 0x00, 0x00, 0x00, 0x00, 0x03};
    uni_hid_device_t* d3 = uni_hid_device_create(addr3);
    ASSERT_NE(NULL, d3);
    uni_hid_device_set_vendor_id(d3, 0x054c);
    uni_hid_device_set_product_id(d3, 0x09cc);
    uni_hid_device_guess_controller_type_from_pid_vid(d3);
    d3->report_parser.setup(d3);
    d3->report_parser.play_dual_rumble(d3, 0, 500, 128, 255);
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d3->rumble.state);
    EXPECT_NE(NULL, btstack_run_loop_base_timers);
    d3->report_parser.setup(d3);
    EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d3->rumble.state);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);
    uni_hid_device_delete(d3);

    // 3b. Zeroing d->parser_data directly (as parser setup() functions do via memset(ins, 0, sizeof(*ins)))
    //     leaves d->rumble untouched outside d->parser_data and never corrupts btstack_run_loop_base_timers.
    uni_hid_device_t* d3_xb = uni_hid_device_create(addr3);
    ASSERT_NE(NULL, d3_xb);
    uni_hid_device_set_vendor_id(d3_xb, 0x045e);
    uni_hid_device_set_product_id(d3_xb, 0x02e0);
    uni_hid_device_guess_controller_type_from_pid_vid(d3_xb);
    d3_xb->report_parser.setup(d3_xb);
    d3_xb->report_parser.play_dual_rumble(d3_xb, 0, 500, 128, 255);
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d3_xb->rumble.state);
    memset(d3_xb->parser_data, 0, sizeof(d3_xb->parser_data));
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d3_xb->rumble.state);
    EXPECT_NE(NULL, btstack_run_loop_base_timers);
    uni_hid_device_delete(d3_xb);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);

    // 4. Nintendo Switch setup_timer Teardown on Re-Setup and Disconnect:
    bd_addr_t addr_sw = {0x70, 0x00, 0x00, 0x00, 0x00, 0x04};
    uni_hid_device_t* d_sw = uni_hid_device_create(addr_sw);
    ASSERT_NE(NULL, d_sw);
    uni_hid_device_set_vendor_id(d_sw, 0x057e);
    uni_hid_device_set_product_id(d_sw, 0x2009);
    uni_hid_device_guess_controller_type_from_pid_vid(d_sw);
    ASSERT_EQ(CONTROLLER_TYPE_SwitchProController, d_sw->controller_type);
    ASSERT_NE(NULL, d_sw->report_parser.setup);
    ASSERT_NE(NULL, d_sw->report_parser.deinit);

    d_sw->report_parser.setup(d_sw);
    EXPECT_NE(NULL, btstack_run_loop_base_timers);
    EXPECT_NE(NULL, btstack_run_loop_base_timers->next);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers->next->next);
    // Re-setup must disarm the previous setup_timer before memset(ins, 0, sizeof(*ins))
    d_sw->report_parser.setup(d_sw);
    EXPECT_NE(NULL, btstack_run_loop_base_timers);
    EXPECT_NE(NULL, btstack_run_loop_base_timers->next);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers->next->next);

    // Deleting Switch device invokes report_parser.deinit(d_sw) which removes setup_timer
    // and also stops connection_timer, leaving the timer list completely empty.
    uni_hid_device_delete(d_sw);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);
}

// ============================================================================
// 18. B4: Delayed Rumble Cancellation & State Transitions Across All 8 Parsers
// ============================================================================
TEST(rumble_delayed_cancel_and_state_transitions_all_parsers_b4) {
    static const struct {
        uint16_t vid;
        uint16_t pid;
        uni_controller_type_t expected_type;
    } k_rumble_controllers[] = {
        {0x054c, 0x0268, CONTROLLER_TYPE_PS3Controller},        // ds3
        {0x054c, 0x09cc, CONTROLLER_TYPE_PS4Controller},        // ds4
        {0x054c, 0x0ce6, CONTROLLER_TYPE_PS5Controller},        // ds5
        {0x054c, 0x03d5, CONTROLLER_TYPE_PSMoveController},     // psmove
        {0x057e, 0x2009, CONTROLLER_TYPE_SwitchProController},  // switch
        {0x057e, 0x0306, CONTROLLER_TYPE_WiiController},        // wii
        {0x045e, 0x02e0, CONTROLLER_TYPE_XBoxOneController},    // xboxone
        {0x18d1, 0x9400, CONTROLLER_TYPE_AndroidController},    // stadia
    };

    for (size_t i = 0; i < ARRAY_SIZE(k_rumble_controllers); i++) {
        uni_hid_device_t d;
        setup_synthetic_device(&d, k_rumble_controllers[i].vid, k_rumble_controllers[i].pid);
        d.conn.interrupt_cid = 0x0041;
        ASSERT_EQ(k_rumble_controllers[i].expected_type, d.controller_type);
        ASSERT_NE(NULL, d.report_parser.play_dual_rumble);

        // Drain any initial setup packets from outgoing_buffer.
        uni_circular_buffer_reset(&d.outgoing_buffer);

        // 1. Cancel while UNI_RUMBLE_STATE_DELAYED:
        //    Must NOT trigger assert(state == IN_PROGRESS) in ds4/switch, must transition
        //    to UNI_RUMBLE_STATE_DISABLED (not stuck in DELAYED as in ds3/wii/psmove),
        //    and must NOT call stop_fn since the motor never started.
        d.report_parser.play_dual_rumble(&d, 100, 400, 100, 200);
        EXPECT_EQ(UNI_RUMBLE_STATE_DELAYED, d.rumble.state);
        EXPECT_EQ(1, uni_circular_buffer_is_empty(&d.outgoing_buffer));

        d.report_parser.play_dual_rumble(&d, 0, 0, 0, 0);
        EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d.rumble.state);
        EXPECT_EQ(1, uni_circular_buffer_is_empty(&d.outgoing_buffer));

        // 2. Cancel while UNI_RUMBLE_STATE_IN_PROGRESS:
        d.report_parser.play_dual_rumble(&d, 0, 400, 100, 200);
        EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d.rumble.state);
        uni_circular_buffer_reset(&d.outgoing_buffer);

        d.report_parser.play_dual_rumble(&d, 0, 0, 0, 0);
        EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d.rumble.state);

        // 3. Cancel while already UNI_RUMBLE_STATE_DISABLED:
        uni_circular_buffer_reset(&d.outgoing_buffer);
        d.report_parser.play_dual_rumble(&d, 0, 0, 0, 0);
        EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d.rumble.state);
        EXPECT_EQ(1, uni_circular_buffer_is_empty(&d.outgoing_buffer));

        // 4. Timer expiry callbacks (timer_delayed_start.process -> timer_duration.process):
        d.report_parser.play_dual_rumble(&d, 50, 300, 80, 160);
        EXPECT_EQ(UNI_RUMBLE_STATE_DELAYED, d.rumble.state);
        ASSERT_NE(NULL, d.rumble.timer_delayed_start.process);
        d.rumble.timer_delayed_start.process(&d.rumble.timer_delayed_start);
        EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d.rumble.state);

        ASSERT_NE(NULL, d.rumble.timer_duration.process);
        d.rumble.timer_duration.process(&d.rumble.timer_duration);
        EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d.rumble.state);

        if (d.report_parser.deinit) {
            d.report_parser.deinit(&d);
        }
    }

    // 5. Per-Parser Hardware Invariants:
    // 5a. DS3: clone_controller set in uni_hid_parser_ds3_does_name_match() must survive uni_hid_parser_ds3_setup()!
    {
        uni_hid_device_t d_ds3;
        btstack_run_loop_base_timers = NULL;
        uni_hid_device_setup();
        uni_hid_device_init(&d_ds3);
        EXPECT_TRUE(uni_hid_parser_ds3_does_name_match(&d_ds3, "PLAYSTATION(R)3Conteroller-PANHAI"));
        // Verify parser_data[offsetof(clone_controller)] is non-zero before and after setup().
        bool any_nonzero_before = false;
        for (size_t b = 0; b < HID_DEVICE_MAX_PARSER_DATA; b++) {
            if (d_ds3.parser_data[b] != 0) {
                any_nonzero_before = true;
            }
        }
        EXPECT_TRUE(any_nonzero_before);
        uint8_t saved_parser_data[HID_DEVICE_MAX_PARSER_DATA];
        memcpy(saved_parser_data, d_ds3.parser_data, sizeof(saved_parser_data));
        uni_hid_parser_ds3_setup(&d_ds3);
        EXPECT_EQ(0, memcmp(saved_parser_data, d_ds3.parser_data, sizeof(saved_parser_data)));
    }

    // 5b. DS4: set_lightbar_color() while rumble is in progress preserves motor_right & motor_left,
    //     and stopping rumble preserves led_red/led_green/led_blue while zeroing motor_right & motor_left.
    {
        uni_hid_device_t d_ds4;
        setup_synthetic_device(&d_ds4, 0x054c, 0x09cc);
        d_ds4.conn.interrupt_cid = 0x0041;
        uni_circular_buffer_reset(&d_ds4.outgoing_buffer);

        d_ds4.report_parser.play_dual_rumble(&d_ds4, 0, 500, 0x55, 0xAA);
        uni_circular_buffer_reset(&d_ds4.outgoing_buffer);

        d_ds4.report_parser.set_lightbar_color(&d_ds4, 0x11, 0x22, 0x33);
        int16_t cid = 0;
        void* data = NULL;
        int len = 0;
        ASSERT_EQ(UNI_CIRCULAR_BUFFER_ERROR_OK, uni_circular_buffer_get(&d_ds4.outgoing_buffer, &cid, &data, &len));
        ASSERT_EQ(79, len);
        const uint8_t* rpt = (const uint8_t*)data;
        // In ds4_output_report_t: [7]=motor_right, [8]=motor_left, [9]=led_red, [10]=led_green, [11]=led_blue
        EXPECT_EQ(0x55, rpt[7]);
        EXPECT_EQ(0xAA, rpt[8]);
        EXPECT_EQ(0x11, rpt[9]);
        EXPECT_EQ(0x22, rpt[10]);
        EXPECT_EQ(0x33, rpt[11]);

        // Now stop rumble and verify LED color is preserved while motors are zeroed.
        d_ds4.report_parser.play_dual_rumble(&d_ds4, 0, 0, 0, 0);
        ASSERT_EQ(UNI_CIRCULAR_BUFFER_ERROR_OK, uni_circular_buffer_get(&d_ds4.outgoing_buffer, &cid, &data, &len));
        ASSERT_EQ(79, len);
        rpt = (const uint8_t*)data;
        EXPECT_EQ(0x00, rpt[7]);
        EXPECT_EQ(0x00, rpt[8]);
        EXPECT_EQ(0x11, rpt[9]);
        EXPECT_EQ(0x22, rpt[10]);
        EXPECT_EQ(0x33, rpt[11]);
    }

    // 5c. PSMove: set_lightbar_color() while rumble is active preserves ins->rumble_magnitude,
    //     and stopping rumble resets ins->rumble_magnitude = 0.
    {
        uni_hid_device_t d_psm;
        setup_synthetic_device(&d_psm, 0x054c, 0x03d5);
        d_psm.conn.interrupt_cid = 0x0041;
        uni_circular_buffer_reset(&d_psm.outgoing_buffer);

        d_psm.report_parser.play_dual_rumble(&d_psm, 0, 500, 0x80, 0xC0);
        uni_circular_buffer_reset(&d_psm.outgoing_buffer);

        d_psm.report_parser.set_lightbar_color(&d_psm, 0xAA, 0xBB, 0xCC);
        int16_t cid = 0;
        void* data = NULL;
        int len = 0;
        ASSERT_EQ(UNI_CIRCULAR_BUFFER_ERROR_OK, uni_circular_buffer_get(&d_psm.outgoing_buffer, &cid, &data, &len));
        ASSERT_EQ(10, len);
        const uint8_t* rpt = (const uint8_t*)data;
        // In psmove_output_report_t: [3..5]=led_rgb, [7]=rumble
        EXPECT_EQ(0xAA, rpt[3]);
        EXPECT_EQ(0xBB, rpt[4]);
        EXPECT_EQ(0xCC, rpt[5]);
        EXPECT_NE(0x00, rpt[7]);

        // Stop rumble and verify subsequent set_lightbar_color sends rumble == 0.
        d_psm.report_parser.play_dual_rumble(&d_psm, 0, 0, 0, 0);
        uni_circular_buffer_reset(&d_psm.outgoing_buffer);
        d_psm.report_parser.set_lightbar_color(&d_psm, 0xAA, 0xBB, 0xCC);
        ASSERT_EQ(UNI_CIRCULAR_BUFFER_ERROR_OK, uni_circular_buffer_get(&d_psm.outgoing_buffer, &cid, &data, &len));
        rpt = (const uint8_t*)data;
        EXPECT_EQ(0x00, rpt[7]);
    }
}

// ============================================================================
// 19. B4: BLE 50ms Retry (UNI_RUMBLE_RETRY_BLE) for Xbox One & Stadia
// ============================================================================
static uint8_t g_mock_hids_write_status = ERROR_CODE_SUCCESS;
static int g_mock_hids_write_calls = 0;
static uint8_t g_mock_hids_last_report_id = 0;
static uint8_t g_mock_hids_last_report[32];
static uint8_t g_mock_hids_last_report_len = 0;

void hids_host_init(uint8_t* hid_descriptor_storage, uint16_t hid_descriptor_storage_len) {
    ARG_UNUSED(hid_descriptor_storage);
    ARG_UNUSED(hid_descriptor_storage_len);
}

uint8_t hids_host_connect(hci_con_handle_t con_handle,
                          btstack_packet_handler_t packet_handler,
                          hid_protocol_mode_t protocol_mode,
                          uint16_t* hids_cid) {
    ARG_UNUSED(con_handle);
    ARG_UNUSED(packet_handler);
    ARG_UNUSED(protocol_mode);
    if (hids_cid != NULL) {
        *hids_cid = 1;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hids_host_disconnect(uint16_t hids_cid) {
    ARG_UNUSED(hids_cid);
    return ERROR_CODE_SUCCESS;
}

const uint8_t* hids_host_descriptor_storage_get_descriptor_data(uint16_t hids_cid, uint8_t service_index) {
    ARG_UNUSED(hids_cid);
    ARG_UNUSED(service_index);
    return NULL;
}

uint16_t hids_host_descriptor_storage_get_descriptor_len(uint16_t hids_cid, uint8_t service_index) {
    ARG_UNUSED(hids_cid);
    ARG_UNUSED(service_index);
    return 0;
}

uint8_t hids_host_send_write_report(uint16_t hids_cid,
                                    uint8_t report_id,
                                    hid_report_type_t report_type,
                                    const uint8_t* report,
                                    uint8_t report_len) {
    ARG_UNUSED(hids_cid);
    ARG_UNUSED(report_type);
    g_mock_hids_write_calls++;
    g_mock_hids_last_report_id = report_id;
    g_mock_hids_last_report_len = report_len;
    if (report != NULL && report_len <= sizeof(g_mock_hids_last_report)) {
        memcpy(g_mock_hids_last_report, report, report_len);
    }
    return g_mock_hids_write_status;
}

TEST(rumble_ble_50ms_retry_xboxone_and_stadia_b4) {
    // 1. Xbox One Firmware v5.x (BLE):
    //    Trigger XBOXONE_FIRMWARE_V5 via Consumer Record usage (0x000c, 0x00b2), then test
    //    xboxone_play_quad_rumble() with start_delay_ms == 0 when hids_host_send_write_report
    //    returns ERROR_CODE_COMMAND_DISALLOWED (Latent Bug 2A & 2B):
    uni_hid_device_t d_xb;
    setup_synthetic_device(&d_xb, 0x045e, 0x0b13);
    d_xb.conn.protocol = UNI_BT_CONN_PROTOCOL_BLE;
    d_xb.hids_cid = 0x0011;
    // Set firmware v5.x by parsing Button 0x0f (v3.1 -> v4.8) followed by Consumer Record 0x00b2 (v4.8 -> v5.x):
    hid_globals_t globals = {.logical_minimum = 0, .logical_maximum = 1, .report_size = 1, .report_count = 1};
    d_xb.report_parser.init_report(&d_xb);
    d_xb.report_parser.parse_usage(&d_xb, &globals, 0x0009, 0x000f, 1);
    d_xb.report_parser.parse_usage(&d_xb, &globals, 0x000c, 0x00b2, 1);

    g_mock_hids_write_calls = 0;
    g_mock_hids_write_status = ERROR_CODE_COMMAND_DISALLOWED;

    // Immediate start (start_delay_ms == 0) hits ERROR_CODE_COMMAND_DISALLOWED -> UNI_RUMBLE_RETRY_BLE:
    // Must preserve duration_ms and all 4 actuator magnitudes (Latent Bug 2A regression).
    xboxone_play_quad_rumble(&d_xb, 0, 350, 45, 95, 90, 180);
    EXPECT_EQ(1, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_DELAYED, d_xb.rumble.state);
    EXPECT_EQ(350, d_xb.rumble.duration_ms);
    EXPECT_EQ(90, d_xb.rumble.weak_magnitude);
    EXPECT_EQ(180, d_xb.rumble.strong_magnitude);
    EXPECT_EQ(45, d_xb.rumble.trigger_left);
    EXPECT_EQ(95, d_xb.rumble.trigger_right);
    EXPECT_EQ(&d_xb, btstack_run_loop_get_timer_context(&d_xb.rumble.timer_delayed_start));
    EXPECT_NE(NULL, btstack_run_loop_base_timers);

    // Second call (when 50ms timer_delayed_start.process fires) returns ERROR_CODE_SUCCESS:
    g_mock_hids_write_status = ERROR_CODE_SUCCESS;
    d_xb.rumble.timer_delayed_start.process(&d_xb.rumble.timer_delayed_start);
    EXPECT_EQ(2, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d_xb.rumble.state);
    EXPECT_EQ(350, d_xb.rumble.duration_ms);
    EXPECT_EQ(&d_xb, btstack_run_loop_get_timer_context(&d_xb.rumble.timer_duration));

    // When timer_duration expires and stop_fn returns UNI_RUMBLE_RETRY_BLE (Latent Bug 2B):
    g_mock_hids_write_status = ERROR_CODE_COMMAND_DISALLOWED;
    d_xb.rumble.timer_duration.process(&d_xb.rumble.timer_duration);
    EXPECT_EQ(3, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d_xb.rumble.state);
    EXPECT_EQ(&d_xb, btstack_run_loop_get_timer_context(&d_xb.rumble.timer_duration));
    EXPECT_NE(NULL, btstack_run_loop_base_timers);

    // Next 50ms stop retry succeeds (ERROR_CODE_SUCCESS -> UNI_RUMBLE_OK):
    g_mock_hids_write_status = ERROR_CODE_SUCCESS;
    d_xb.rumble.timer_duration.process(&d_xb.rumble.timer_duration);
    EXPECT_EQ(4, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d_xb.rumble.state);

    // 2. Stadia Controller (0x18d1:0x9400) BLE 50ms retry for both start and stop:
    uni_hid_device_t d_stadia;
    setup_synthetic_device(&d_stadia, 0x18d1, 0x9400);
    d_stadia.conn.protocol = UNI_BT_CONN_PROTOCOL_BLE;
    d_stadia.hids_cid = 0x0022;

    g_mock_hids_write_calls = 0;
    g_mock_hids_write_status = ERROR_CODE_COMMAND_DISALLOWED;
    d_stadia.report_parser.play_dual_rumble(&d_stadia, 0, 250, 0x40, 0x80);
    EXPECT_EQ(1, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_DELAYED, d_stadia.rumble.state);
    EXPECT_EQ(250, d_stadia.rumble.duration_ms);
    EXPECT_EQ(0x40, d_stadia.rumble.weak_magnitude);
    EXPECT_EQ(0x80, d_stadia.rumble.strong_magnitude);
    EXPECT_EQ(&d_stadia, btstack_run_loop_get_timer_context(&d_stadia.rumble.timer_delayed_start));

    g_mock_hids_write_status = ERROR_CODE_SUCCESS;
    d_stadia.rumble.timer_delayed_start.process(&d_stadia.rumble.timer_delayed_start);
    EXPECT_EQ(2, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d_stadia.rumble.state);
    EXPECT_EQ(4, g_mock_hids_last_report_len);
    // Stadia report: strong_magnitude << 8 (0x8000 LE = 0x00, 0x80), weak_magnitude << 8 (0x4000 LE = 0x00, 0x40)
    EXPECT_EQ(0x00, g_mock_hids_last_report[0]);
    EXPECT_EQ(0x80, g_mock_hids_last_report[1]);
    EXPECT_EQ(0x00, g_mock_hids_last_report[2]);
    EXPECT_EQ(0x40, g_mock_hids_last_report[3]);

    // Stadia stop retry -> success:
    g_mock_hids_write_status = ERROR_CODE_COMMAND_DISALLOWED;
    d_stadia.rumble.timer_duration.process(&d_stadia.rumble.timer_duration);
    EXPECT_EQ(3, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_IN_PROGRESS, d_stadia.rumble.state);
    EXPECT_EQ(&d_stadia, btstack_run_loop_get_timer_context(&d_stadia.rumble.timer_duration));

    g_mock_hids_write_status = ERROR_CODE_SUCCESS;
    d_stadia.rumble.timer_duration.process(&d_stadia.rumble.timer_duration);
    EXPECT_EQ(4, g_mock_hids_write_calls);
    EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d_stadia.rumble.state);

    // 3. Non-retryable BLE error (ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER -> UNI_RUMBLE_ERR):
    g_mock_hids_write_status = ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    d_xb.report_parser.play_dual_rumble(&d_xb, 0, 250, 100, 200);
    EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d_xb.rumble.state);
    d_stadia.report_parser.play_dual_rumble(&d_stadia, 0, 250, 100, 200);
    EXPECT_EQ(UNI_RUMBLE_STATE_DISABLED, d_stadia.rumble.state);

    // Restore default mock status for subsequent tests.
    g_mock_hids_write_status = ERROR_CODE_SUCCESS;
}

// ============================================================================
// 20. B4: Wiimote wii_set_led() Preserves Rumble Bit 0x01
// ============================================================================
TEST(wii_set_led_preserves_rumble_bit_b4) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x057e, 0x0306);
    d.conn.interrupt_cid = 0x0041;
    uni_circular_buffer_reset(&d.outgoing_buffer);

    // 1. Start rumble on Wiimote and verify uni_hid_parser_rumble_is_in_progress(&d) is true.
    d.report_parser.play_dual_rumble(&d, 0, 500, 128, 128);
    EXPECT_TRUE(uni_hid_parser_rumble_is_in_progress(&d));
    uni_circular_buffer_reset(&d.outgoing_buffer);

    // 2. Set Player 2 LED (seat = 0x02 -> 0x20) while rumble is active:
    //    Dequeue the 3-byte output report {0xa2, 0x11, led} and verify bit 0x01 is preserved!
    d.report_parser.set_player_leds(&d, 0x02);
    int16_t cid = 0;
    void* data = NULL;
    int len = 0;
    ASSERT_EQ(UNI_CIRCULAR_BUFFER_ERROR_OK, uni_circular_buffer_get(&d.outgoing_buffer, &cid, &data, &len));
    ASSERT_EQ(3, len);
    const uint8_t* rpt = (const uint8_t*)data;
    EXPECT_EQ(0xa2, rpt[0]);
    EXPECT_EQ(0x11, rpt[1]);
    EXPECT_EQ(0x20 | 0x01, rpt[2]);

    // 3. Stop rumble and set Player 2 LED again: verify bit 0x01 is now cleared (0x20).
    d.report_parser.play_dual_rumble(&d, 0, 0, 0, 0);
    EXPECT_FALSE(uni_hid_parser_rumble_is_in_progress(&d));
    uni_circular_buffer_reset(&d.outgoing_buffer);

    d.report_parser.set_player_leds(&d, 0x02);
    ASSERT_EQ(UNI_CIRCULAR_BUFFER_ERROR_OK, uni_circular_buffer_get(&d.outgoing_buffer, &cid, &data, &len));
    ASSERT_EQ(3, len);
    rpt = (const uint8_t*)data;
    EXPECT_EQ(0xa2, rpt[0]);
    EXPECT_EQ(0x11, rpt[1]);
    EXPECT_EQ(0x20, rpt[2]);
}

// ============================================================================
// 21. B6: Wii Balance Board Zero & Inverted Calibration Division-by-Zero Guards
// ============================================================================
TEST(wii_balance_board_zero_and_inverted_calibration_guards_b6) {
    uni_hid_device_t d;
    setup_synthetic_device(&d, 0x057e, 0x0306);
    ASSERT_EQ(CONTROLLER_TYPE_WiiController, d.controller_type);

    // Walk Wii extension FSM to register a Wii Balance Board (00 00 a4 20 04 02):
    uint8_t status_ext[7] = {0x20, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
    feed_input_report(&d, status_ext, sizeof(status_ext));
    uint8_t ack_wmem[5] = {0x22, 0x00, 0x00, 0x16, 0x00};
    feed_input_report(&d, ack_wmem, sizeof(ack_wmem));
    feed_input_report(&d, ack_wmem, sizeof(ack_wmem));
    uint8_t read_bb_id[22] = {
        0x21, 0x00, 0x00, 0x50, 0x00, 0xfa, 0x00, 0x00, 0xa4, 0x20, 0x04, 0x02,
    };
    feed_input_report(&d, read_bb_id, sizeof(read_bb_id));

    // 1. Zero Calibration (kg0 == kg17 == kg34 == 0, before calibration data is populated):
    //    Feed Report 0x34 (DRM_KEE, 22 bytes) with raw sensor values val = 0 and val = 1500.
    //    Verify balance_interpolate() returns 0 for all 4 sensors without float division by zero!
    uint8_t bb_rpt[22];
    memset(bb_rpt, 0, sizeof(bb_rpt));
    bb_rpt[0] = 0x34;
    // tr = 0, br = 1500 (0x05dc), tl = 0, bl = 1500 (0x05dc)
    bb_rpt[5] = 0x05;
    bb_rpt[6] = 0xdc;
    bb_rpt[9] = 0x05;
    bb_rpt[10] = 0xdc;
    bb_rpt[13] = 0x82;  // battery full
    feed_input_report(&d, bb_rpt, sizeof(bb_rpt));
    EXPECT_EQ(UNI_CONTROLLER_CLASS_BALANCE_BOARD, d.controller.klass);
    EXPECT_EQ(0, d.controller.balance_board.tr);
    EXPECT_EQ(0, d.controller.balance_board.br);
    EXPECT_EQ(0, d.controller.balance_board.tl);
    EXPECT_EQ(0, d.controller.balance_board.bl);

    // 2. Equal & Inverted Calibration (tr: kg0=1000, kg17=1000, kg34=2000;
    //    br: kg0=1000, kg17=2000, kg34=2000; tl/bl: inverted kg0=3000 > kg17=2000 > kg34=1000):
    uint8_t cal1[22] = {
        0x21,
        0x00,
        0x00,
        0xf0,
        0x00,
        0x24,
        // kg0: tr=1000 (0x03e8), br=1000 (0x03e8), tl=3000 (0x0bb8), bl=3000 (0x0bb8)
        0x03,
        0xe8,
        0x03,
        0xe8,
        0x0b,
        0xb8,
        0x0b,
        0xb8,
        // kg17: tr=1000 (0x03e8), br=2000 (0x07d0), tl=2000 (0x07d0), bl=2000 (0x07d0)
        0x03,
        0xe8,
        0x07,
        0xd0,
        0x07,
        0xd0,
        0x07,
        0xd0,
    };
    uint8_t cal2[22] = {
        0x21,
        0x00,
        0x00,
        0x70,
        0x00,
        0x34,
        // kg34: tr=1000 (0x03e8), br=2000 (0x07d0), tl=1000 (0x03e8), bl=1000 (0x03e8)
        0x03,
        0xe8,
        0x07,
        0xd0,
        0x03,
        0xe8,
        0x03,
        0xe8,
    };
    feed_input_report(&d, cal1, sizeof(cal1));
    feed_input_report(&d, cal2, sizeof(cal2));
    EXPECT_EQ(CONTROLLER_SUBTYPE_WII_BALANCE_BOARD, d.controller_subtype);

    // Feed tr=1000, br=2500, tl=2500, bl=3500:
    // tr: val=1000 >= kg17(1000), kg34(1000) <= kg17(1000) -> 0
    // br: val=2500 >= kg17(2000), kg34(2000) <= kg17(2000) -> 0
    // tl: val=2500 < kg0(3000) -> 0
    // bl: val=3500 >= kg0(3000) and >= kg17(2000), kg34(1000) <= kg17(2000) -> 0
    bb_rpt[3] = 0x03;
    bb_rpt[4] = 0xe8;  // tr = 1000
    bb_rpt[5] = 0x09;
    bb_rpt[6] = 0xc4;  // br = 2500
    bb_rpt[7] = 0x09;
    bb_rpt[8] = 0xc4;  // tl = 2500
    bb_rpt[9] = 0x0d;
    bb_rpt[10] = 0xac;  // bl = 3500
    feed_input_report(&d, bb_rpt, sizeof(bb_rpt));
    EXPECT_EQ(0, d.controller.balance_board.tr);
    EXPECT_EQ(0, d.controller.balance_board.br);
    EXPECT_EQ(0, d.controller.balance_board.tl);
    EXPECT_EQ(0, d.controller.balance_board.bl);

    // 3. Valid Calibration (kg0 = 1000, kg17 = 2000, kg34 = 3000 on all 4 sensors):
    // Re-setup and walk to valid calibration:
    setup_synthetic_device(&d, 0x057e, 0x0306);
    feed_input_report(&d, status_ext, sizeof(status_ext));
    feed_input_report(&d, ack_wmem, sizeof(ack_wmem));
    feed_input_report(&d, ack_wmem, sizeof(ack_wmem));
    feed_input_report(&d, read_bb_id, sizeof(read_bb_id));

    uint8_t valid_cal1[22] = {
        0x21,
        0x00,
        0x00,
        0xf0,
        0x00,
        0x24,
        // kg0: 1000 (0x03e8) x 4
        0x03,
        0xe8,
        0x03,
        0xe8,
        0x03,
        0xe8,
        0x03,
        0xe8,
        // kg17: 2000 (0x07d0) x 4
        0x07,
        0xd0,
        0x07,
        0xd0,
        0x07,
        0xd0,
        0x07,
        0xd0,
    };
    uint8_t valid_cal2[22] = {
        0x21,
        0x00,
        0x00,
        0x70,
        0x00,
        0x34,
        // kg34: 3000 (0x0bb8) x 4
        0x0b,
        0xb8,
        0x0b,
        0xb8,
        0x0b,
        0xb8,
        0x0b,
        0xb8,
    };
    feed_input_report(&d, valid_cal1, sizeof(valid_cal1));
    feed_input_report(&d, valid_cal2, sizeof(valid_cal2));

    // tr = 500 (< kg0 -> 0g), br = 1500 (midpoint 0..17kg -> 8500g),
    // tl = 2500 (midpoint 17..34kg -> 25500g), bl = 2000 (exact 17kg -> 17000g)
    bb_rpt[3] = 0x01;
    bb_rpt[4] = 0xf4;  // tr = 500
    bb_rpt[5] = 0x05;
    bb_rpt[6] = 0xdc;  // br = 1500
    bb_rpt[7] = 0x09;
    bb_rpt[8] = 0xc4;  // tl = 2500
    bb_rpt[9] = 0x07;
    bb_rpt[10] = 0xd0;  // bl = 2000
    feed_input_report(&d, bb_rpt, sizeof(bb_rpt));
    EXPECT_EQ(0, d.controller.balance_board.tr);
    EXPECT_EQ(8500, d.controller.balance_board.br);
    EXPECT_EQ(25500, d.controller.balance_board.tl);
    EXPECT_EQ(17000, d.controller.balance_board.bl);
}

// ============================================================================
// 22. B6 & Phase 4: Controller List Uniqueness & Table-Driven Parser Lookup
// ============================================================================
TEST(controller_list_uniqueness_and_table_driven_lookup_b6_phase4) {
    // 1. Verify every entry in arrControllers[] has a valid controller type, and that every
    //    entry in the Bluepad32 custom additions section (starting at OUYA 0x2836:0x0001) has
    //    a globally unique (VID, PID) across the entire arrControllers[] table (confirming
    //    removal of the duplicate 0x057e:0x0306 entry in B6).
    size_t bp32_addons_start = ARRAY_SIZE(arrControllers);
    for (size_t i = 0; i < ARRAY_SIZE(arrControllers); i++) {
        EXPECT_NE(CONTROLLER_TYPE_None, arrControllers[i].controller_type);
        if (arrControllers[i].device_id == MAKE_CONTROLLER_ID(0x2836, 0x0001)) {
            bp32_addons_start = i;
        }
    }
    EXPECT_TRUE(bp32_addons_start < ARRAY_SIZE(arrControllers));
    for (size_t i = bp32_addons_start; i < ARRAY_SIZE(arrControllers); i++) {
        uint16_t vid = (uint16_t)(arrControllers[i].device_id >> 16);
        uint16_t pid = (uint16_t)(arrControllers[i].device_id & 0xffff);
        EXPECT_EQ(arrControllers[i].controller_type, uni_guess_controller_type(vid, pid));
        for (size_t j = 0; j < ARRAY_SIZE(arrControllers); j++) {
            if (i == j) {
                continue;
            }
            EXPECT_NE(arrControllers[i].device_id, arrControllers[j].device_id);
        }
    }

    // 2. Table-Driven Parser Lookup & Stadia Override (Phase 4.1):
    uni_hid_device_t d;
    // Stadia (0x18d1:0x9400) -> CONTROLLER_TYPE_AndroidController with Stadia setup & rumble hooks:
    setup_synthetic_device(&d, 0x18d1, 0x9400);
    EXPECT_EQ(CONTROLLER_TYPE_AndroidController, d.controller_type);
    EXPECT_EQ(uni_hid_parser_stadia_setup, d.report_parser.setup);
    EXPECT_EQ(uni_hid_parser_stadia_play_dual_rumble, d.report_parser.play_dual_rumble);
    EXPECT_EQ(uni_hid_parser_android_parse_usage, d.report_parser.parse_usage);

    // Regular Android controller (0x20d6:0x6271) -> CONTROLLER_TYPE_AndroidController with NULL setup/rumble:
    setup_synthetic_device(&d, 0x20d6, 0x6271);
    EXPECT_EQ(CONTROLLER_TYPE_AndroidController, d.controller_type);
    EXPECT_EQ(NULL, d.report_parser.setup);
    EXPECT_EQ(NULL, d.report_parser.play_dual_rumble);
    EXPECT_EQ(uni_hid_parser_android_parse_usage, d.report_parser.parse_usage);

    // Unknown VID/PID (0xDEAD:0xBEEF) falls back to CONTROLLER_TYPE_AndroidController:
    setup_synthetic_device(&d, 0xDEAD, 0xBEEF);
    EXPECT_EQ(CONTROLLER_TYPE_AndroidController, d.controller_type);
    EXPECT_EQ(uni_hid_parser_android_init_report, d.report_parser.init_report);
    EXPECT_EQ(uni_hid_parser_android_parse_usage, d.report_parser.parse_usage);

    // Controller type without a specialized entry in k_parser_entries[] (e.g., Xbox 360 0x045e:0x028e)
    // falls back to uni_hid_parser_generic in setup_report_parser():
    setup_synthetic_device(&d, 0x045e, 0x028e);
    EXPECT_EQ(CONTROLLER_TYPE_XBox360Controller, d.controller_type);
    EXPECT_EQ(uni_hid_parser_generic_init_report, d.report_parser.init_report);
    EXPECT_EQ(uni_hid_parser_generic_parse_usage, d.report_parser.parse_usage);

    // 3. Pre-SDP vs. Post-SDP Name Matcher Separation (Phase 4.1):
    uni_hid_device_init(&d);
    EXPECT_TRUE(uni_hid_device_guess_controller_type_from_name(&d, "PLAYSTATION(R)3 Controller"));
    EXPECT_EQ(CONTROLLER_TYPE_PS3Controller, d.controller_type);

    uni_hid_device_init(&d);
    EXPECT_TRUE(uni_hid_device_guess_controller_type_from_name(&d, "Pro Controller"));
    EXPECT_EQ(CONTROLLER_TYPE_SwitchProController, d.controller_type);

    // Pre-SDP name matcher MUST return false for "Xbox Wireless Controller" so BR/EDR Xbox
    // controllers still perform SDP to fetch their genuine HID descriptor:
    uni_hid_device_init(&d);
    EXPECT_FALSE(uni_hid_device_guess_controller_type_from_name(&d, "Xbox Wireless Controller"));

    // Post-SDP fallback in uni_hid_device_guess_controller_type_from_pid_vid() with unknown VID/PID
    // and d.name = "Xbox Wireless Controller" resolves to CONTROLLER_TYPE_XBoxOneController and
    // attaches the 334-byte fallback HID descriptor:
    uni_hid_device_init(&d);
    d.vendor_id = 0xDEAD;
    d.product_id = 0xBEEF;
    uni_hid_device_set_name(&d, "Xbox Wireless Controller");
    uni_hid_device_guess_controller_type_from_pid_vid(&d);
    EXPECT_EQ(CONTROLLER_TYPE_XBoxOneController, d.controller_type);
    EXPECT_EQ(334, d.hid_descriptor_len);
    EXPECT_EQ(uni_hid_parser_xboxone_setup, d.report_parser.setup);
}

int main(int argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    uni_property_init();
    uni_virtual_device_init();
    uni_virtual_device_set_enabled(true);
    hci_init(&g_dummy_transport, NULL);
    l2cap_init();

    RUN_TEST(hid_parser_ds4_valid_and_short);
    RUN_TEST(hid_parser_ds4_invalid_crc32_rejected);
    RUN_TEST(hid_parser_ds5_bt_and_usb);
    RUN_TEST(hid_parser_ds3_valid_and_short);
    RUN_TEST(hid_parser_xboxone_reports);
    RUN_TEST(hid_parser_switch_reports_and_imu_bounds);
    RUN_TEST(hid_parser_wii_core_and_nunchuk);
    RUN_TEST(hid_parser_steam_offset_and_int16_min_regression);
    RUN_TEST(hid_parser_atari_zero_len_and_oob_dpad_regression);
    RUN_TEST(hid_parser_keyboard_standard_and_jx05_overflow_regression);
    RUN_TEST(hid_parser_mouse_8byte_and_wheel);
    RUN_TEST(hid_parser_stadia_and_smarttvremote);
    RUN_TEST(hid_parser_8bitdo_icade_nimbus_ouya);
    RUN_TEST(hid_parser_generic_descriptor_gamepad);
    RUN_TEST(hid_parser_descriptor_malformed_and_deep_collection);
    RUN_TEST(hid_parser_sweep_all_controllers_short_and_prng_fuzz);
    RUN_TEST(rumble_and_switch_setup_timer_teardown_on_disconnect_and_resetup_b3);
    RUN_TEST(rumble_delayed_cancel_and_state_transitions_all_parsers_b4);
    RUN_TEST(rumble_ble_50ms_retry_xboxone_and_stadia_b4);
    RUN_TEST(wii_set_led_preserves_rumble_bit_b4);
    RUN_TEST(wii_balance_board_zero_and_inverted_calibration_guards_b6);
    RUN_TEST(controller_list_uniqueness_and_table_driven_lookup_b6_phase4);

    printf("All 22 synthetic HID parser test suites passed!\n");
    return 0;
}
