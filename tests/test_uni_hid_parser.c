// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2
//
// Unit and regression test suite for Bluepad32 HID parsers and input converters:
// - Core HID axis, pedal, hat, and D-pad normalization (`uni_hid_parser`).
// - Division-by-zero range guards, signed pedal ranges, DS4/DS5 feature report
//   truncation bounds, DualSense adaptive trigger underflow protection, gamepad
//   remapping, and retro joystick/balance-board converters.

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <btstack.h>
#include <btstack_run_loop_posix.h>

#include "controller/uni_balance_board.h"
#include "controller/uni_gamepad.h"
#include "controller/uni_keyboard.h"
#include "hid_usage.h"
#include "parser/uni_hid_parser.h"
#include "parser/uni_hid_parser_ds4.h"
#include "parser/uni_hid_parser_ds5.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"
#include "uni_property.h"

//
// Tests
//

static void test_process_axis(void) {
    printf("Testing uni_hid_parser_process_axis...\n");
    hid_globals_t globals = {0};
    globals.logical_minimum = 0;
    globals.logical_maximum = 255;
    globals.report_size = 8;  // Used when max == -1

    int32_t val;

    // Test center
    val = uni_hid_parser_process_axis(&globals, 128);
    assert(val == 0);

    // Test min
    val = uni_hid_parser_process_axis(&globals, 0);
    assert(val == -512);

    // Test max
    val = uni_hid_parser_process_axis(&globals, 255);
    assert(val == 508);

    // Test logical_maximum == -1 case (unsigned byte treated as signed in descriptor)
    globals.logical_maximum = -1;
    globals.report_size = 8;
    // Should behave like 0..255
    val = uni_hid_parser_process_axis(&globals, 128);
    assert(val == 0);

    // Test signed 8-bit range: -128 to 127
    globals.logical_minimum = -128;
    globals.logical_maximum = 127;

    // Center (0)
    val = uni_hid_parser_process_axis(&globals, 0);
    assert(val == 0);

    // Min (-128)
    val = uni_hid_parser_process_axis(&globals, (uint32_t)-128);
    assert(val == -512);

    printf("PASS\n");
}

static void test_process_pedal(void) {
    printf("Testing uni_hid_parser_process_pedal...\n");
    hid_globals_t globals = {0};
    globals.logical_minimum = 0;
    globals.logical_maximum = 255;
    globals.report_size = 8;

    int32_t val;

    // Min
    val = uni_hid_parser_process_pedal(&globals, 0);
    assert(val == 0);

    // Max
    val = uni_hid_parser_process_pedal(&globals, 255);
    assert(val == 1020);

    // Mid
    val = uni_hid_parser_process_pedal(&globals, 128);
    assert(val == 512);

    printf("PASS\n");
}

static void test_hat_to_dpad(void) {
    printf("Testing uni_hid_parser_hat_to_dpad...\n");
    assert(uni_hid_parser_hat_to_dpad(0) == DPAD_UP);
    assert(uni_hid_parser_hat_to_dpad(1) == (DPAD_UP | DPAD_RIGHT));
    assert(uni_hid_parser_hat_to_dpad(2) == DPAD_RIGHT);
    assert(uni_hid_parser_hat_to_dpad(3) == (DPAD_RIGHT | DPAD_DOWN));
    assert(uni_hid_parser_hat_to_dpad(4) == DPAD_DOWN);
    assert(uni_hid_parser_hat_to_dpad(5) == (DPAD_DOWN | DPAD_LEFT));
    assert(uni_hid_parser_hat_to_dpad(6) == DPAD_LEFT);
    assert(uni_hid_parser_hat_to_dpad(7) == (DPAD_LEFT | DPAD_UP));
    assert(uni_hid_parser_hat_to_dpad(8) == 0);     // Center
    assert(uni_hid_parser_hat_to_dpad(0xFF) == 0);  // Null
    printf("PASS\n");
}

static void test_process_dpad(void) {
    printf("Testing uni_hid_parser_process_dpad...\n");
    uint8_t dpad = 0;

    uni_hid_parser_process_dpad(HID_USAGE_DPAD_UP, 1, &dpad);
    assert(dpad == DPAD_UP);

    uni_hid_parser_process_dpad(HID_USAGE_DPAD_DOWN, 1, &dpad);
    assert(dpad == (DPAD_UP | DPAD_DOWN));  // Both set now

    uni_hid_parser_process_dpad(HID_USAGE_DPAD_UP, 0, &dpad);
    assert(dpad == DPAD_DOWN);

    // Unsupported usage
    uni_hid_parser_process_dpad(0x9999, 1, &dpad);
    assert(dpad == DPAD_DOWN);  // Should be unchanged

    printf("PASS\n");
}

static void test_div_by_zero_guard(void) {
    printf("Testing range <= 0 division-by-zero guards...\n");
    hid_globals_t globals = {0};

    // range == 0 (min = 10, max = 9 -> range = (9 - 10) + 1 = 0)
    globals.logical_minimum = 10;
    globals.logical_maximum = 9;
    globals.report_size = 8;
    assert(uni_hid_parser_process_axis(&globals, 10) == 0);
    assert(uni_hid_parser_process_pedal(&globals, 10) == 0);

    // range < 0 (min = 100, max = 0 -> range = -99)
    globals.logical_minimum = 100;
    globals.logical_maximum = 0;
    assert(uni_hid_parser_process_axis(&globals, 50) == 0);
    assert(uni_hid_parser_process_pedal(&globals, 50) == 0);

    printf("PASS\n");
}

static void test_signed_pedal_normalization(void) {
    printf("Testing signed pedal normalization (min = -128, max = 127)...\n");
    hid_globals_t globals = {0};
    globals.logical_minimum = -128;
    globals.logical_maximum = 127;
    globals.report_size = 8;

    // Min (-128) -> 0
    assert(uni_hid_parser_process_pedal(&globals, (uint32_t)-128) == 0);
    // Mid (0) -> 512
    assert(uni_hid_parser_process_pedal(&globals, 0) == 512);
    // Max (127) -> 1020
    assert(uni_hid_parser_process_pedal(&globals, 127) == 1020);

    // Verify unsigned range (0..255) remains unchanged
    globals.logical_minimum = 0;
    globals.logical_maximum = 255;
    assert(uni_hid_parser_process_pedal(&globals, 0) == 0);
    assert(uni_hid_parser_process_pedal(&globals, 128) == 512);
    assert(uni_hid_parser_process_pedal(&globals, 255) == 1020);

    printf("PASS\n");
}

static void test_ds4_ds5_feature_report_bounds(void) {
    printf("Testing DS4 & DS5 feature report bounds protection...\n");
    uni_hid_device_t d = {0};
    uint8_t baseline_parser_data[HID_DEVICE_MAX_PARSER_DATA];
    memset(baseline_parser_data, 0xA5, sizeof(baseline_parser_data));
    memcpy(d.parser_data, baseline_parser_data, sizeof(baseline_parser_data));

    // Null & zero-length guards
    uni_hid_parser_ds4_parse_feature_report(NULL, baseline_parser_data, 10);
    uni_hid_parser_ds4_parse_feature_report(&d, NULL, 10);
    uni_hid_parser_ds4_parse_feature_report(&d, baseline_parser_data, 0);
    uni_hid_parser_ds5_parse_feature_report(NULL, baseline_parser_data, 10);
    uni_hid_parser_ds5_parse_feature_report(&d, NULL, 10);
    uni_hid_parser_ds5_parse_feature_report(&d, baseline_parser_data, 0);

    // Truncated feature reports (len = 5, smaller than required struct sizes)
    uint8_t short_ds4_calib[5] = {0x02, 1, 2, 3, 4};
    uint8_t short_ds4_fw[5] = {0xa3, 1, 2, 3, 4};
    uint8_t short_ds5_calib[5] = {0x05, 1, 2, 3, 4};
    uint8_t short_ds5_pairing[5] = {0x09, 1, 2, 3, 4};
    uint8_t short_ds5_fw[5] = {0x20, 1, 2, 3, 4};

    uni_hid_parser_ds4_parse_feature_report(&d, short_ds4_calib, sizeof(short_ds4_calib));
    uni_hid_parser_ds4_parse_feature_report(&d, short_ds4_fw, sizeof(short_ds4_fw));
    uni_hid_parser_ds5_parse_feature_report(&d, short_ds5_calib, sizeof(short_ds5_calib));
    uni_hid_parser_ds5_parse_feature_report(&d, short_ds5_pairing, sizeof(short_ds5_pairing));
    uni_hid_parser_ds5_parse_feature_report(&d, short_ds5_fw, sizeof(short_ds5_fw));

    // Verify none of the truncated reports modified d.parser_data
    assert(memcmp(d.parser_data, baseline_parser_data, sizeof(baseline_parser_data)) == 0);

    // Valid-length DS4 firmware version report (49 bytes) with non-null-terminated string_date/string_time
    uint8_t valid_ds4_fw[49];
    memset(valid_ds4_fw, 0xFF, sizeof(valid_ds4_fw));
    valid_ds4_fw[0] = 0xa3;
    memset(&valid_ds4_fw[1], 'D', 11);   // string_date[11] without null terminator
    memset(&valid_ds4_fw[17], 'T', 8);   // string_time[8] without null terminator
    memset(d.parser_data, 0, sizeof(d.parser_data));
    uni_hid_parser_ds4_parse_feature_report(&d, valid_ds4_fw, sizeof(valid_ds4_fw));

    // Valid-length DS4 calibration report (37 bytes) & DS5 calibration report (41 bytes) with zeroed denom
    d.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    uint8_t valid_ds4_calib[37] = {0};
    valid_ds4_calib[0] = 0x02;
    uni_hid_parser_ds4_parse_feature_report(&d, valid_ds4_calib, sizeof(valid_ds4_calib));

    memset(d.parser_data, 0, sizeof(d.parser_data));
    uint8_t valid_ds5_calib[41] = {0};
    valid_ds5_calib[0] = 0x05;
    uni_hid_parser_ds5_parse_feature_report(&d, valid_ds5_calib, sizeof(valid_ds5_calib));

    printf("PASS\n");
}

static void test_ds5_adaptive_triggers(void) {
    printf("Testing DS5 adaptive trigger strength == 0 & amplitude == 0 guards...\n");
    ds5_adaptive_trigger_effect_t off_ref = ds5_new_adaptive_trigger_effect_off();
    assert(off_ref.effect == 0x05);

    // strength == 0 or amplitude == 0 should return OFF (0x05), NOT underflow (0 - 1)
    ds5_adaptive_trigger_effect_t fb_zero = ds5_new_adaptive_trigger_effect_feedback(3, 0);
    assert(memcmp(&fb_zero, &off_ref, sizeof(off_ref)) == 0);

    ds5_adaptive_trigger_effect_t wp_zero = ds5_new_adaptive_trigger_effect_weapon(2, 5, 0);
    assert(memcmp(&wp_zero, &off_ref, sizeof(off_ref)) == 0);

    ds5_adaptive_trigger_effect_t vb_zero = ds5_new_adaptive_trigger_effect_vibration(3, 0, 60);
    assert(memcmp(&vb_zero, &off_ref, sizeof(off_ref)) == 0);

    // Valid non-zero parameters
    ds5_adaptive_trigger_effect_t fb_valid = ds5_new_adaptive_trigger_effect_feedback(2, 4);
    assert(fb_valid.effect == 0x21);
    assert(fb_valid.data[0] == 0xFC);  // zones 2..7 set in low byte
    assert(fb_valid.data[1] == 0x03);  // zones 8..9 set in high byte

    ds5_adaptive_trigger_effect_t wp_valid = ds5_new_adaptive_trigger_effect_weapon(2, 5, 6);
    assert(wp_valid.effect == 0x25);
    assert(wp_valid.data[0] == ((1 << 2) | (1 << 5)));
    assert(wp_valid.data[2] == 5);  // strength - 1

    ds5_adaptive_trigger_effect_t vb_valid = ds5_new_adaptive_trigger_effect_vibration(1, 5, 40);
    assert(vb_valid.effect == 0x26);
    assert(vb_valid.data[8] == 40);

    // Out-of-bounds parameters should return INVALID (0x00)
    assert(ds5_new_adaptive_trigger_effect_feedback(10, 4).effect == 0x00);
    assert(ds5_new_adaptive_trigger_effect_feedback(2, 9).effect == 0x00);
    assert(ds5_new_adaptive_trigger_effect_weapon(1, 5, 4).effect == 0x00);
    assert(ds5_new_adaptive_trigger_effect_weapon(4, 4, 4).effect == 0x00);
    assert(ds5_new_adaptive_trigger_effect_weapon(2, 5, 9).effect == 0x00);
    assert(ds5_new_adaptive_trigger_effect_vibration(10, 4, 30).effect == 0x00);
    assert(ds5_new_adaptive_trigger_effect_vibration(2, 9, 30).effect == 0x00);

    printf("PASS\n");
}

static void test_gamepad_remap(void) {
    printf("Testing uni_gamepad_remap (XBOX, SWITCH, CUSTOM)...\n");
    uni_gamepad_t gp = {0};
    gp.dpad = DPAD_UP | DPAD_LEFT;
    gp.buttons = BUTTON_A | BUTTON_X | BUTTON_SHOULDER_L;
    gp.misc_buttons = MISC_BUTTON_START;
    gp.axis_x = 200;
    gp.axis_y = -300;
    gp.axis_rx = 100;
    gp.axis_ry = -100;
    gp.brake = 400;
    gp.throttle = 800;

    // 1. XBOX (identity mapping)
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
    assert(uni_gamepad_get_mappings_type() == UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
    uni_gamepad_t xbox_gp = uni_gamepad_remap(&gp);
    assert(memcmp(&xbox_gp, &gp, sizeof(gp)) == 0);

    // 2. SWITCH (swaps A <-> B and X <-> Y)
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_SWITCH);
    uni_gamepad_t switch_gp = uni_gamepad_remap(&gp);
    assert(switch_gp.buttons == (BUTTON_B | BUTTON_Y | BUTTON_SHOULDER_L));
    assert(switch_gp.axis_x == 200);
    assert(switch_gp.axis_y == -300);

    // 3. CUSTOM (invert axes, swap pedals, remap buttons/dpad)
    uni_gamepad_mappings_t custom = GAMEPAD_DEFAULT_MAPPINGS;
    custom.dpad_up = UNI_GAMEPAD_MAPPINGS_DPAD_DOWN;
    custom.button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_Y;
    custom.axis_x_inverted = 1;
    custom.axis_y_inverted = 1;
    custom.brake = UNI_GAMEPAD_MAPPINGS_PEDAL_THROTTLE;
    custom.throttle = UNI_GAMEPAD_MAPPINGS_PEDAL_BRAKE;
    uni_gamepad_set_mappings(&custom);
    assert(uni_gamepad_get_mappings_type() == UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM);

    uni_gamepad_t custom_gp = uni_gamepad_remap(&gp);
    assert((custom_gp.dpad & DPAD_DOWN) != 0);
    assert((custom_gp.buttons & BUTTON_Y) != 0);
    assert(custom_gp.axis_x == -200);
    assert(custom_gp.axis_y == 300);
    assert(custom_gp.brake == 800);
    assert(custom_gp.throttle == 400);

    // Restore default
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
    printf("PASS\n");
}

static void test_joystick_converters(void) {
    printf("Testing uni_joystick converters (gamepad, twinstick, keyboard, balance board)...\n");

    // 1. Single joy from gamepad (use_two_buttons = 0 vs 1)
    uni_gamepad_t gp = {0};
    gp.buttons = BUTTON_A | BUTTON_B | BUTTON_Y | BUTTON_SHOULDER_R;
    gp.axis_x = -200;  // left (< -AXIS_THRESHOLD)
    gp.axis_y = 200;   // down (> AXIS_THRESHOLD)

    uni_joystick_t joy_c64 = {0};
    uni_joy_to_single_joy_from_gamepad(&gp, &joy_c64, 0);
    assert(joy_c64.fire == 1);
    assert(joy_c64.up == 1);  // BUTTON_B maps to jump (up) when use_two_buttons == 0
    assert(joy_c64.left == 1);
    assert(joy_c64.down == 1);
    assert(joy_c64.button3 == 1);
    assert(joy_c64.auto_fire == 1);

    uni_joystick_t joy_msx = {0};
    uni_joy_to_single_joy_from_gamepad(&gp, &joy_msx, 1);
    assert(joy_msx.up == 0);
    assert(joy_msx.button2 == 1);  // BUTTON_B maps to button2 when use_two_buttons == 1

    // 2. TwinStick from gamepad
    uni_gamepad_t ts_gp = {0};
    ts_gp.buttons = BUTTON_A | BUTTON_B | BUTTON_SHOULDER_L | BUTTON_SHOULDER_R;
    ts_gp.axis_x = 250;    // joy2 right
    ts_gp.axis_ry = -250;  // joy1 up
    uni_joystick_t joy1 = {0};
    uni_joystick_t joy2 = {0};
    uni_joy_to_twinstick_from_gamepad(&ts_gp, &joy1, &joy2);
    assert(joy2.fire == 1);
    assert(joy2.right == 1);
    assert(joy2.auto_fire == 1);
    assert(joy1.fire == 1);
    assert(joy1.up == 1);
    assert(joy1.auto_fire == 1);

    // 3. Keyboard single joy & twinstick
    uni_keyboard_t kb = {0};
    kb.pressed_keys[0] = HID_USAGE_KB_UP_ARROW;
    kb.pressed_keys[1] = HID_USAGE_KB_SPACEBAR;
    kb.pressed_keys[2] = HID_USAGE_KB_X;
    kb.pressed_keys[3] = HID_USAGE_KB_W;
    kb.pressed_keys[4] = HID_USAGE_KB_E;
    kb.modifiers = UNI_KEYBOARD_MODIFIER_LEFT_SHIFT | UNI_KEYBOARD_MODIFIER_RIGHT_ALT;

    uni_joystick_t kb_single = {0};
    uni_joy_to_single_joy_from_keyboard(&kb, &kb_single);
    assert(kb_single.up == 1);
    assert(kb_single.fire == 1);
    assert(kb_single.button2 == 1);
    assert(kb_single.button3 == 1);  // From LEFT_SHIFT

    uni_joystick_t kb_ts1 = {0};
    uni_joystick_t kb_ts2 = {0};
    uni_joy_to_twinstick_from_keyboard(&kb, &kb_ts1, &kb_ts2);
    // In twinstick: out_joy2 receives arrows/right-modifiers, out_joy1 receives WASD/QER
    assert(kb_ts2.up == 1);
    assert(kb_ts2.fire == 1);  // From RIGHT_ALT
    assert(kb_ts1.up == 1);    // From W
    assert(kb_ts1.fire == 1);  // From E

    // 4. Balance board state machine (RESET -> THRESHOLD -> IN_AIR -> FIRE -> RESET)
    uni_balance_board_t bb = {0};
    uni_balance_board_state_t bb_state = {0};
    uni_joystick_t bb_joy = {0};

    // Trigger RESET -> THRESHOLD (sum >= 5000) and build smooth_top for up movement
    bb.tl = 3000;
    bb.tr = 3000;
    bb.bl = 0;
    bb.br = 0;
    uni_joy_to_single_joy_from_balance_board(&bb, &bb_state, &bb_joy);
    assert(bb_state.fire_state == UNI_BALANCE_BOARD_STATE_THRESHOLD);

    // Transition THRESHOLD -> IN_AIR (all sensors < 1600)
    bb.tl = 100;
    bb.tr = 100;
    bb.bl = 100;
    bb.br = 100;
    uni_joy_to_single_joy_from_balance_board(&bb, &bb_state, &bb_joy);
    assert(bb_state.fire_state == UNI_BALANCE_BOARD_STATE_IN_AIR);

    // Stay in air for 3 frames (fire_counter > 2 triggers FIRE)
    for (int i = 0; i < 3; i++) {
        memset(&bb_joy, 0, sizeof(bb_joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &bb_state, &bb_joy);
    }
    assert(bb_state.fire_state == UNI_BALANCE_BOARD_STATE_FIRE);
    assert(bb_joy.fire == 1);

    // Maintain FIRE for 11 frames until it resets to UNI_BALANCE_BOARD_STATE_RESET
    for (int i = 0; i < 11; i++) {
        memset(&bb_joy, 0, sizeof(bb_joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &bb_state, &bb_joy);
    }
    assert(bb_state.fire_state == UNI_BALANCE_BOARD_STATE_RESET);

    printf("PASS\n");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    uni_property_init();

    test_process_axis();
    test_process_pedal();
    test_hat_to_dpad();
    test_process_dpad();

    test_div_by_zero_guard();
    test_signed_pedal_normalization();
    test_ds4_ds5_feature_report_bounds();
    test_ds5_adaptive_triggers();
    test_gamepad_remap();
    test_joystick_converters();

    printf("All uni_hid_parser tests passed!\n");
    return 0;
}
