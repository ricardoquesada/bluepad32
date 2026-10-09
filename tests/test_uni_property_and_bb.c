// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2
//
// Unit and regression test suite for:
// - B1: Unified BTstack TLV property backend (`arch/uni_property_btstack_tlv.c`),
//       Pico W pre-HCI_STATE_WORKING NULL TLV fallback, and POSIX TLV persistence.
// - B2 & Phase 3.1: Wii Balance Board global properties (`UNI_PROPERTY_IDX_UNI_BB_*`),
//       swapped default threshold fix (`move=1500`, `fire=5000`), portable getters/setters,
//       and `uni_joy_to_single_joy_from_balance_board()` directional smoothing + fire hysteresis.

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <btstack.h>
#include <btstack_memory.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_posix.h>
#include <btstack_tlv.h>
#include <btstack_tlv_posix.h>

#include "controller/uni_balance_board.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "test_check.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_error.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"
#include "uni_property.h"
#include "uni_version.h"

// Minimal Platform Implementation (returns NULL from get_property to simulate
// non-Unijoysticle platforms such as POSIX, Pico W, Arduino, or Custom).
static uni_error_t mock_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)name;
    (void)cod;
    (void)rssi;
    return UNI_ERROR_SUCCESS;
}

static void mock_on_device_connected(uni_hid_device_t* d) {
    (void)d;
}

static void mock_on_device_disconnected(uni_hid_device_t* d) {
    (void)d;
}

static uni_error_t mock_on_device_ready(uni_hid_device_t* d) {
    (void)d;
    return UNI_ERROR_SUCCESS;
}

static void mock_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    (void)d;
    (void)ctl;
}

static const uni_property_t* mock_get_property(uni_property_idx_t idx) {
    (void)idx;
    return NULL;
}

static void mock_on_oob_event(uni_platform_oob_event_t event, void* data) {
    (void)event;
    (void)data;
}

static struct uni_platform g_mock_platform = {
    .name = "Test Property & BB Platform",
    .init = NULL,
    .on_init_complete = NULL,
    .on_device_discovered = mock_on_device_discovered,
    .on_device_connected = mock_on_device_connected,
    .on_device_disconnected = mock_on_device_disconnected,
    .on_device_ready = mock_on_device_ready,
    .on_controller_data = mock_on_controller_data,
    .get_property = mock_get_property,
    .on_oob_event = mock_on_oob_event,
};

struct uni_platform* uni_get_platform(void) {
    return &g_mock_platform;
}

// ============================================================================
// 2.1 Defect B1: Unified BTstack TLV Property Store
// ============================================================================

TEST(property_btstack_tlv_null_instance_fallback_b1) {
    const btstack_tlv_t* saved_impl = NULL;
    void* saved_ctx = NULL;
    btstack_tlv_get_instance(&saved_impl, &saved_ctx);
    ASSERT_TRUE(saved_impl != NULL);
    ASSERT_TRUE(saved_ctx != NULL);

    // Simulate Pico W during uni_init() before HCI_STATE_WORKING registers btstack_tlv_flash_bank.
    btstack_tlv_set_instance(NULL, NULL);

    // Verify uni_property_get() returns default_value across all property types without NULL dereference.
    uni_property_value_t ble_val = uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED);
#ifdef CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT
    EXPECT_TRUE(ble_val.boolean);
#else
    EXPECT_FALSE(ble_val.boolean);
#endif

    uni_property_value_t gap_val = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
#ifdef CONFIG_BLUEPAD32_GAP_SECURITY
    EXPECT_EQ(gap_val.u8, 2);
#else
    EXPECT_EQ(gap_val.u8, 0);
#endif

    uni_property_value_t move_val = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD);
    EXPECT_EQ(move_val.u32, UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT);

    uni_property_value_t fire_val = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD);
    EXPECT_EQ(fire_val.u32, UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT);

    uni_property_value_t allow_str = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    EXPECT_TRUE(allow_str.str == NULL);

    uni_property_value_t ver_str = uni_property_get(UNI_PROPERTY_IDX_VERSION);
    ASSERT_TRUE(ver_str.str != NULL);
    EXPECT_EQ(strcmp(ver_str.str, UNI_VERSION_STRING), 0);

    uni_property_value_t svc_en_val = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED);
    EXPECT_EQ(svc_en_val.u8, CONFIG_BLUEPAD32_BLE_SERVICE_ENABLED);

    uni_property_value_t svc_name_val = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_NAME);
    ASSERT_TRUE(svc_name_val.str != NULL);
    EXPECT_EQ(strcmp(svc_name_val.str, CONFIG_BLUEPAD32_BLE_SERVICE_NAME), 0);

    uni_property_value_t svc_pass_val = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD);
    ASSERT_TRUE(svc_pass_val.str != NULL);
    EXPECT_EQ(strcmp(svc_pass_val.str, CONFIG_BLUEPAD32_BLE_SERVICE_PASSWORD), 0);

    // Synthetic FLOAT property descriptor tested via uni_property_get_with_property().
    const uni_property_t float_prop = {
        .idx = UNI_PROPERTY_IDX_MOUSE_SCALE,
        .name = UNI_PROPERTY_NAME_MOUSE_SCALE,
        .type = UNI_PROPERTY_TYPE_FLOAT,
        .default_value.f32 = 1.5f,
        .flags = 0,
    };
    uni_property_value_t f_val = uni_property_get_with_property(&float_prop);
    EXPECT_FLOAT_NEAR(f_val.f32, 1.5f, 1e-6f);

    // Verify uni_property_set() safely no-ops across all types when TLV is NULL.
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = !ble_val.boolean});
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, (uni_property_value_t){.u8 = 99});
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, (uni_property_value_t){.u32 = 9999});
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = "11:22:33:44:55:66"});
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED, (uni_property_value_t){.u8 = 0});
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_NAME, (uni_property_value_t){.str = "Bluepad32 rc car"});
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD, (uni_property_value_t){.str = "secret"});
    uni_property_set_with_property(&float_prop, (uni_property_value_t){.f32 = 4.25f});

    // Verify NULL property descriptor guards.
    uni_property_value_t null_prop_val = uni_property_get_with_property(NULL);
    EXPECT_EQ(null_prop_val.u32, 0);
    EXPECT_TRUE(null_prop_val.str == NULL);
    uni_property_set_with_property(NULL, (uni_property_value_t){.u32 = 123});

    // Critical State Isolation: Restore POSIX TLV instance without calling btstack_tlv_posix_deinit().
    btstack_tlv_set_instance(saved_impl, saved_ctx);
}

TEST(property_btstack_tlv_roundtrip_all_types_and_guards) {
    const btstack_tlv_t* tlv_impl = NULL;
    void* tlv_ctx = NULL;
    btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
    ASSERT_TRUE(tlv_impl != NULL);
    ASSERT_TRUE(tlv_ctx != NULL);

    // 1. BOOL & U8 round-trip and union zero-initialization check:
    uni_property_value_t orig_ble = uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = false});
    EXPECT_FALSE(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = true});
    EXPECT_TRUE(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, orig_ble);

    uni_property_value_t orig_gap = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, (uni_property_value_t){.u8 = 0});
    uni_property_value_t gap0 = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
    EXPECT_EQ(gap0.u8, 0);
    EXPECT_EQ(gap0.u32, 0);  // Upper bytes must be zeroed, not uninitialized stack garbage
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, (uni_property_value_t){.u8 = 2});
    uni_property_value_t gap2 = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
    EXPECT_EQ(gap2.u8, 2);
    EXPECT_EQ(gap2.u32, 2);
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, orig_gap);

    // 2. U32 round-trip:
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, (uni_property_value_t){.u32 = 2200});
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD, (uni_property_value_t){.u32 = 6500});
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32, 2200);
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD).u32, 6500);
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD,
                     (uni_property_value_t){.u32 = UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT});
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD,
                     (uni_property_value_t){.u32 = UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT});

    // 3. FLOAT default fallback & round-trip persistence:
    const uint32_t float_tag =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_MOUSE_SCALE;
    tlv_impl->delete_tag(tlv_ctx, float_tag);

    const uni_property_t float_prop = {
        .idx = UNI_PROPERTY_IDX_MOUSE_SCALE,
        .name = UNI_PROPERTY_NAME_MOUSE_SCALE,
        .type = UNI_PROPERTY_TYPE_FLOAT,
        .default_value.f32 = 1.5f,
        .flags = 0,
    };
    EXPECT_FLOAT_NEAR(uni_property_get_with_property(&float_prop).f32, 1.5f, 1e-6f);
    uni_property_set_with_property(&float_prop, (uni_property_value_t){.f32 = 3.25f});
    EXPECT_FLOAT_NEAR(uni_property_get_with_property(&float_prop).f32, 3.25f, 1e-6f);
    tlv_impl->delete_tag(tlv_ctx, float_tag);

    // 4. STRING round-trip, NULL rejection, > 128 byte rejection, and non-null-terminated raw tag clamp:
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = "01:02:03:04:05:06,"});
    uni_property_value_t s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    ASSERT_TRUE(s_val.str != NULL);
    EXPECT_EQ(strcmp(s_val.str, "01:02:03:04:05:06,"), 0);

    // NULL string must be rejected, preserving previous value.
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = NULL});
    s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    ASSERT_TRUE(s_val.str != NULL);
    EXPECT_EQ(strcmp(s_val.str, "01:02:03:04:05:06,"), 0);

    // Oversized string (strlen == 135 >= 128) must be rejected without buffer overflow.
    char oversized[136];
    memset(oversized, 'A', 135);
    oversized[135] = '\0';
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = oversized});
    s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    ASSERT_TRUE(s_val.str != NULL);
    EXPECT_EQ(strcmp(s_val.str, "01:02:03:04:05:06,"), 0);

    // Simulate a non-null-terminated 128-byte raw TLV tag in storage and verify clamping at index 127.
    const uint32_t allowlist_tag =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_ALLOWLIST_LIST;
    uint8_t raw_unterminated[128];
    memset(raw_unterminated, 'Z', sizeof(raw_unterminated));
    EXPECT_EQ(tlv_impl->store_tag(tlv_ctx, allowlist_tag, raw_unterminated, sizeof(raw_unterminated)), 0);
    s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    ASSERT_TRUE(s_val.str != NULL);
    EXPECT_EQ(strlen(s_val.str), 127);
    EXPECT_EQ(s_val.str[127], '\0');
    for (int i = 0; i < 127; i++) {
        EXPECT_EQ(s_val.str[i], 'Z');
    }
    tlv_impl->delete_tag(tlv_ctx, allowlist_tag);

    // 5. Read-Only Guard (UNI_PROPERTY_FLAG_READ_ONLY):
    uni_property_set(UNI_PROPERTY_IDX_VERSION, (uni_property_value_t){.str = "tampered-version"});
    uni_property_value_t ver_after = uni_property_get(UNI_PROPERTY_IDX_VERSION);
    ASSERT_TRUE(ver_after.str != NULL);
    EXPECT_EQ(strcmp(ver_after.str, UNI_VERSION_STRING), 0);
}

// ============================================================================
// 2.2 Defect B2 & Phase 3.1: Balance Board Global Properties & Hysteresis
// ============================================================================

TEST(balance_board_global_properties_and_defaults_b2) {
    const btstack_tlv_t* tlv_impl = NULL;
    void* tlv_ctx = NULL;
    btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
    ASSERT_TRUE(tlv_impl != NULL);
    ASSERT_TRUE(tlv_ctx != NULL);

    // Delete any persisted BB tags so we test the compiled-in defaults in uni_property.c.
    const uint32_t move_tag =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD;
    const uint32_t fire_tag =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD;
    tlv_impl->delete_tag(tlv_ctx, move_tag);
    tlv_impl->delete_tag(tlv_ctx, fire_tag);

    // Verify UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD and UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD
    // are registered globally in core uni_property.c (even when platform->get_property returns NULL)
    // and that their defaults are NOT swapped (move=1500, fire=5000).
    EXPECT_TRUE(uni_property_get_property_by_name(UNI_PROPERTY_NAME_UNI_BB_MOVE_THRESHOLD) != NULL);
    EXPECT_TRUE(uni_property_get_property_by_name(UNI_PROPERTY_NAME_UNI_BB_FIRE_THRESHOLD) != NULL);

    uint32_t prop_move = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32;
    uint32_t prop_fire = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD).u32;
    EXPECT_EQ(prop_move, UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT);
    EXPECT_EQ(prop_move, 1500);
    EXPECT_EQ(prop_fire, UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT);
    EXPECT_EQ(prop_fire, 5000);

    // Call uni_balance_board_init() (without CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE) and verify
    // thresholds are loaded as 1500 / 5000 instead of being zeroed to 0.
    uni_balance_board_init();
    uni_balance_board_threshold_t thr = uni_balance_board_get_threshold();
    EXPECT_EQ(thr.move, 1500);
    EXPECT_EQ(thr.fire, 5000);
    EXPECT_EQ(uni_balance_board_get_move_threshold(), 1500);
    EXPECT_EQ(uni_balance_board_get_fire_threshold(), 5000);

    // Exercise Phase 3.1 portable setters/getters and verify TLV persistence across uni_balance_board_init().
    uni_balance_board_set_move_threshold(1800);
    uni_balance_board_set_fire_threshold(6200);
    EXPECT_EQ(uni_balance_board_get_move_threshold(), 1800);
    EXPECT_EQ(uni_balance_board_get_fire_threshold(), 6200);
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32, 1800);
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD).u32, 6200);

    // Reload from TLV via uni_balance_board_init() and confirm persisted values.
    uni_balance_board_init();
    EXPECT_EQ(uni_balance_board_get_move_threshold(), 1800);
    EXPECT_EQ(uni_balance_board_get_fire_threshold(), 6200);

    // Restore defaults.
    uni_balance_board_set_move_threshold(UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT);
    uni_balance_board_set_fire_threshold(UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT);
    tlv_impl->delete_tag(tlv_ctx, move_tag);
    tlv_impl->delete_tag(tlv_ctx, fire_tag);
    uni_balance_board_init();
    EXPECT_EQ(uni_balance_board_get_move_threshold(), 1500);
    EXPECT_EQ(uni_balance_board_get_fire_threshold(), 5000);
}

TEST(balance_board_directional_smoothing_and_fire_hysteresis_b2) {
    uni_balance_board_init();
    EXPECT_EQ(uni_balance_board_get_move_threshold(), 1500);
    EXPECT_EQ(uni_balance_board_get_fire_threshold(), 5000);

    // 1. Directional Low-Pass Filter (smooth_top, smooth_down, smooth_left, smooth_right):
    //    mult_frac((15000 + 15000) - 0, 6, 100) = 1800 > 1500 (move threshold).
    {
        uni_balance_board_t bb = {.tl = 15000, .tr = 15000, .bl = 0, .br = 0};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(joy.up, 1);
        EXPECT_EQ(joy.down, 0);
        EXPECT_EQ(joy.left, 0);
        EXPECT_EQ(joy.right, 0);
    }
    {
        uni_balance_board_t bb = {.tl = 0, .tr = 0, .bl = 15000, .br = 15000};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(joy.down, 1);
        EXPECT_EQ(joy.up, 0);
        EXPECT_EQ(joy.left, 0);
        EXPECT_EQ(joy.right, 0);
    }
    {
        uni_balance_board_t bb = {.tl = 15000, .bl = 15000, .tr = 0, .br = 0};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(joy.left, 1);
        EXPECT_EQ(joy.right, 0);
        EXPECT_EQ(joy.up, 0);
        EXPECT_EQ(joy.down, 0);
    }
    {
        uni_balance_board_t bb = {.tr = 15000, .br = 15000, .tl = 0, .bl = 0};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(joy.right, 1);
        EXPECT_EQ(joy.left, 0);
        EXPECT_EQ(joy.up, 0);
        EXPECT_EQ(joy.down, 0);
    }
    {
        uni_balance_board_t bb = {.tl = 2000, .tr = 2000, .bl = 2000, .br = 2000};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(joy.up, 0);
        EXPECT_EQ(joy.down, 0);
        EXPECT_EQ(joy.left, 0);
        EXPECT_EQ(joy.right, 0);
    }

    // 2. Fire State Machine Hysteresis: Happy Path (RESET -> THRESHOLD -> IN_AIR -> FIRE -> RESET)
    {
        uni_balance_board_t bb = {0};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};

        // Step 1: RESET -> THRESHOLD (sum = 6000 >= 5000)
        bb.tl = 1500;
        bb.tr = 1500;
        bb.bl = 1500;
        bb.br = 1500;
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_THRESHOLD);
        EXPECT_EQ(state.fire_counter, 0);
        EXPECT_EQ(joy.fire, 0);

        // Step 1b (Section 3.7.2): Verify bl == UNI_BALANCE_BOARD_IDLE_THRESHOLD (1600) does NOT
        // transition to UNI_BALANCE_BOARD_STATE_IN_AIR even when tl, tr, br < 1600.
        bb.tl = 100;
        bb.tr = 100;
        bb.bl = UNI_BALANCE_BOARD_IDLE_THRESHOLD;
        bb.br = 100;
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_THRESHOLD);
        EXPECT_EQ(state.fire_counter, 1);
        EXPECT_EQ(joy.fire, 0);

        // Step 2: THRESHOLD -> IN_AIR (all sensors < 1600)
        bb.tl = 100;
        bb.tr = 100;
        bb.bl = 100;
        bb.br = 100;
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_IN_AIR);
        EXPECT_EQ(state.fire_counter, 0);
        EXPECT_EQ(joy.fire, 0);

        // Step 3: 2 frames in air keep state in IN_AIR (fire_counter = 1, 2); 3rd frame triggers FIRE
        for (int i = 1; i <= 2; i++) {
            memset(&joy, 0, sizeof(joy));
            uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
            EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_IN_AIR);
            EXPECT_EQ(state.fire_counter, i);
            EXPECT_EQ(joy.fire, 0);
        }
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_FIRE);
        EXPECT_EQ(state.fire_counter, 0);
        EXPECT_EQ(joy.fire, 1);

        // Step 4: Maintain FIRE for 10 frames (fire_counter 1..10); 11th frame resets to RESET
        for (int i = 1; i <= 10; i++) {
            memset(&joy, 0, sizeof(joy));
            uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
            EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_FIRE);
            EXPECT_EQ(state.fire_counter, i);
            EXPECT_EQ(joy.fire, 1);
        }
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_RESET);
        EXPECT_EQ(state.fire_counter, 0);
    }

    // 3. Abort Branch 1: THRESHOLD 10-Frame Timeout Without Jumping
    {
        uni_balance_board_t bb = {.tl = 1500, .tr = 1500, .bl = 1500, .br = 1500};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};

        // Enter THRESHOLD (sum = 6000 >= 5000)
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_THRESHOLD);

        // Feed intermediate weight (sum = 4000 < 5000, but tl = 2000 >= 1600 so not IN_AIR)
        bb.tl = 2000;
        bb.tr = 1000;
        bb.bl = 500;
        bb.br = 500;
        for (int i = 1; i <= 10; i++) {
            memset(&joy, 0, sizeof(joy));
            uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
            EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_THRESHOLD);
            EXPECT_EQ(joy.fire, 0);
        }
        // 11th frame (fire_counter == 11 > 10) aborts back to RESET without firing
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_RESET);
        EXPECT_EQ(state.fire_counter, 0);
        EXPECT_EQ(joy.fire, 0);
    }

    // 4. Abort Branch 2: IN_AIR Premature Landing Before > 2 Frames
    {
        uni_balance_board_t bb = {.tl = 1500, .tr = 1500, .bl = 1500, .br = 1500};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};

        // Enter THRESHOLD -> IN_AIR
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_THRESHOLD);

        bb.tl = 100;
        bb.tr = 100;
        bb.bl = 100;
        bb.br = 100;
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_IN_AIR);

        // 1 frame in air (fire_counter = 1)
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_IN_AIR);
        EXPECT_EQ(state.fire_counter, 1);

        // Premature landing (tl = 2000 >= 1600) on frame 2 -> immediately resets to RESET
        bb.tl = 2000;
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        EXPECT_EQ(state.fire_state, UNI_BALANCE_BOARD_STATE_RESET);
        EXPECT_EQ(state.fire_counter, 0);
        EXPECT_EQ(joy.fire, 0);
    }
}

// ============================================================================
// 2.3 Parallel CTest Isolation: BLUEPAD32_TLV_PATH Override & Default Fallback
// ============================================================================

// Helper to cleanly release in-memory TLV nodes and close the open file handle
// without calling btstack_tlv_posix_deinit() (which permanently sets
// btstack_tlv_posix_read_only = true for the rest of the process).
static void reset_posix_tlv_singleton(void) {
    const btstack_tlv_t* tlv_impl = NULL;
    btstack_tlv_posix_t* tlv_ctx = NULL;
    btstack_tlv_get_instance(&tlv_impl, (void**)&tlv_ctx);
    if (tlv_ctx != NULL) {
        while (tlv_ctx->entry_list != NULL) {
            btstack_linked_item_t* item = btstack_linked_list_pop(&tlv_ctx->entry_list);
            free(item);
        }
        if (tlv_ctx->file != NULL) {
            fclose(tlv_ctx->file);
            tlv_ctx->file = NULL;
        }
    }
    btstack_tlv_set_instance(NULL, NULL);
}

TEST(property_btstack_tlv_posix_env_path_override_and_default_fallback) {
    const char* orig_env = getenv("BLUEPAD32_TLV_PATH");
    char saved_env[256] = {0};
    bool had_orig_env = (orig_env != NULL);
    if (had_orig_env) {
        strncpy(saved_env, orig_env, sizeof(saved_env) - 1);
    }

    const char* custom_path = "/tmp/bp32_test_custom_env_override.tlv";
    unlink(custom_path);

    // 1. Set BLUEPAD32_TLV_PATH to a custom temporary path and re-run uni_property_init().
    reset_posix_tlv_singleton();
    EXPECT_EQ(setenv("BLUEPAD32_TLV_PATH", custom_path, 1), 0);
    uni_property_init();

    const btstack_tlv_t* tlv_impl = NULL;
    btstack_tlv_posix_t* tlv_ctx = NULL;
    btstack_tlv_get_instance(&tlv_impl, (void**)&tlv_ctx);
    ASSERT_TRUE(tlv_impl != NULL);
    ASSERT_TRUE(tlv_ctx != NULL);
    ASSERT_TRUE(tlv_ctx->db_path != NULL);
    EXPECT_EQ(strcmp(tlv_ctx->db_path, custom_path), 0);
    EXPECT_EQ(access(custom_path, F_OK), 0);

    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, (uni_property_value_t){.u32 = 2345});
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32, 2345);

    // Re-open the custom path via uni_property_init() to confirm on-disk persistence at custom_path.
    reset_posix_tlv_singleton();
    uni_property_init();
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32, 2345);

    // 2. Empty BLUEPAD32_TLV_PATH ("") must fall back to "/tmp/bp32_property.tlv".
    reset_posix_tlv_singleton();
    EXPECT_EQ(setenv("BLUEPAD32_TLV_PATH", "", 1), 0);
    uni_property_init();
    btstack_tlv_get_instance(&tlv_impl, (void**)&tlv_ctx);
    ASSERT_TRUE(tlv_ctx != NULL);
    ASSERT_TRUE(tlv_ctx->db_path != NULL);
    EXPECT_EQ(strcmp(tlv_ctx->db_path, "/tmp/bp32_property.tlv"), 0);

    // 3. Unset BLUEPAD32_TLV_PATH must also fall back to "/tmp/bp32_property.tlv".
    reset_posix_tlv_singleton();
    EXPECT_EQ(unsetenv("BLUEPAD32_TLV_PATH"), 0);
    uni_property_init();
    btstack_tlv_get_instance(&tlv_impl, (void**)&tlv_ctx);
    ASSERT_TRUE(tlv_ctx != NULL);
    ASSERT_TRUE(tlv_ctx->db_path != NULL);
    EXPECT_EQ(strcmp(tlv_ctx->db_path, "/tmp/bp32_property.tlv"), 0);

    // Restore original environment variable and TLV singleton instance.
    reset_posix_tlv_singleton();
    if (had_orig_env) {
        setenv("BLUEPAD32_TLV_PATH", saved_env, 1);
    } else {
        unsetenv("BLUEPAD32_TLV_PATH");
    }
    uni_property_init();
    unlink(custom_path);
}

TEST(ble_service_properties_defaults_roundtrip_and_shared_buffer_safety) {
    const btstack_tlv_t* tlv_impl = NULL;
    void* tlv_ctx = NULL;
    btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
    ASSERT_TRUE(tlv_impl != NULL);
    ASSERT_TRUE(tlv_ctx != NULL);

    // 1. Property Registration & Index Ordering Invariant (Landmine #2):
    uni_property_init_debug();
    EXPECT_EQ(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD, 10);
    EXPECT_EQ(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, 11);
    EXPECT_EQ(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED, 12);
    EXPECT_EQ(UNI_PROPERTY_IDX_BLE_SERVICE_NAME, 13);
    EXPECT_EQ(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD, 14);
    EXPECT_EQ(UNI_PROPERTY_IDX_LAST, 15);

    const uni_property_t* p_en = uni_property_get_property_by_name(UNI_PROPERTY_NAME_BLE_SERVICE_ENABLED);
    const uni_property_t* p_name = uni_property_get_property_by_name(UNI_PROPERTY_NAME_BLE_SERVICE_NAME);
    const uni_property_t* p_pass = uni_property_get_property_by_name(UNI_PROPERTY_NAME_BLE_SERVICE_PASSWORD);
    ASSERT_TRUE(p_en != NULL);
    ASSERT_TRUE(p_name != NULL);
    ASSERT_TRUE(p_pass != NULL);
    EXPECT_EQ(strcmp(p_en->name, "bp.ble.svc_en"), 0);
    EXPECT_EQ(strcmp(p_name->name, "bp.ble.name"), 0);
    EXPECT_EQ(strcmp(p_pass->name, "bp.ble.pass"), 0);
    EXPECT_EQ(p_en->idx, UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED);
    EXPECT_EQ(p_name->idx, UNI_PROPERTY_IDX_BLE_SERVICE_NAME);
    EXPECT_EQ(p_pass->idx, UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD);

    // 2. Default Fallback When TLV Tags Are Absent:
    const uint32_t tag_en =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED;
    const uint32_t tag_name =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_BLE_SERVICE_NAME;
    const uint32_t tag_pass =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD;
    tlv_impl->delete_tag(tlv_ctx, tag_en);
    tlv_impl->delete_tag(tlv_ctx, tag_name);
    tlv_impl->delete_tag(tlv_ctx, tag_pass);

    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED).u8, 1);
    uni_property_value_t def_name = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_NAME);
    ASSERT_TRUE(def_name.str != NULL);
    EXPECT_EQ(strcmp(def_name.str, "Bluepad32"), 0);
    uni_property_value_t def_pass = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD);
    ASSERT_TRUE(def_pass.str != NULL);
    EXPECT_EQ(strcmp(def_pass.str, ""), 0);

    // 3. Round-Trip Persistence Across All Three Properties:
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED, (uni_property_value_t){.u8 = 0});
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED).u8, 0);
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED, (uni_property_value_t){.u8 = 1});
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED).u8, 1);

    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_NAME, (uni_property_value_t){.str = "Bluepad32 rc car"});
    uni_property_value_t rt_name = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_NAME);
    ASSERT_TRUE(rt_name.str != NULL);
    EXPECT_EQ(strcmp(rt_name.str, "Bluepad32 rc car"), 0);

    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD, (uni_property_value_t){.str = "my_secret_pass"});
    uni_property_value_t rt_pass = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD);
    ASSERT_TRUE(rt_pass.str != NULL);
    EXPECT_EQ(strcmp(rt_pass.str, "my_secret_pass"), 0);

    // 4. Shared Static str_ret[128] Buffer Regression Test (Landmine #2):
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_NAME, (uni_property_value_t){.str = "Bluepad32 on esp32"});
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD, (uni_property_value_t){.str = "hunter2"});

    char copied_name[64] = {0};
    char copied_pass[64] = {0};
    uni_property_value_t read_name = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_NAME);
    ASSERT_TRUE(read_name.str != NULL);
    strncpy(copied_name, read_name.str, sizeof(copied_name) - 1);

    uni_property_value_t read_pass = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD);
    ASSERT_TRUE(read_pass.str != NULL);
    strncpy(copied_pass, read_pass.str, sizeof(copied_pass) - 1);

    EXPECT_EQ(strcmp(copied_name, "Bluepad32 on esp32"), 0);
    EXPECT_EQ(strcmp(copied_pass, "hunter2"), 0);

    // 5. Cleanup:
    tlv_impl->delete_tag(tlv_ctx, tag_en);
    tlv_impl->delete_tag(tlv_ctx, tag_name);
    tlv_impl->delete_tag(tlv_ctx, tag_pass);
}

int main(void) {
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    uni_property_init();

    RUN_TEST(property_btstack_tlv_null_instance_fallback_b1);
    RUN_TEST(property_btstack_tlv_roundtrip_all_types_and_guards);
    RUN_TEST(balance_board_global_properties_and_defaults_b2);
    RUN_TEST(balance_board_directional_smoothing_and_fire_hysteresis_b2);
    RUN_TEST(property_btstack_tlv_posix_env_path_override_and_default_fallback);
    RUN_TEST(ble_service_properties_defaults_roundtrip_and_shared_buffer_safety);

    return test_summary();
}
