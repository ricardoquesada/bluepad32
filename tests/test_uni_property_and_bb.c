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

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack.h>
#include <btstack_memory.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_posix.h>
#include <btstack_tlv.h>

#include "controller/uni_balance_board.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "uni_common.h"
#include "uni_error.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"
#include "uni_property.h"
#include "uni_version.h"

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                   \
    do {                                 \
        printf("Running " #name "... "); \
        fflush(stdout);                  \
        test_##name();                   \
        printf("PASS\n");                \
    } while (0)

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
    assert(saved_impl != NULL);
    assert(saved_ctx != NULL);

    // Simulate Pico W during uni_init() before HCI_STATE_WORKING registers btstack_tlv_flash_bank.
    btstack_tlv_set_instance(NULL, NULL);

    // Verify uni_property_get() returns default_value across all property types without NULL dereference.
    uni_property_value_t ble_val = uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED);
#ifdef CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT
    assert(ble_val.boolean == true);
#else
    assert(ble_val.boolean == false);
#endif

    uni_property_value_t gap_val = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
#ifdef CONFIG_BLUEPAD32_GAP_SECURITY
    assert(gap_val.u8 == 2);
#else
    assert(gap_val.u8 == 0);
#endif

    uni_property_value_t move_val = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD);
    assert(move_val.u32 == UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT);

    uni_property_value_t fire_val = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD);
    assert(fire_val.u32 == UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT);

    uni_property_value_t allow_str = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    assert(allow_str.str == NULL);

    uni_property_value_t ver_str = uni_property_get(UNI_PROPERTY_IDX_VERSION);
    assert(ver_str.str != NULL);
    assert(strcmp(ver_str.str, UNI_VERSION_STRING) == 0);

    // Synthetic FLOAT property descriptor tested via uni_property_get_with_property().
    const uni_property_t float_prop = {
        .idx = UNI_PROPERTY_IDX_MOUSE_SCALE,
        .name = UNI_PROPERTY_NAME_MOUSE_SCALE,
        .type = UNI_PROPERTY_TYPE_FLOAT,
        .default_value.f32 = 1.5f,
        .flags = 0,
    };
    uni_property_value_t f_val = uni_property_get_with_property(&float_prop);
    assert(fabsf(f_val.f32 - 1.5f) < 1e-6f);

    // Verify uni_property_set() safely no-ops across all types when TLV is NULL.
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = !ble_val.boolean});
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, (uni_property_value_t){.u8 = 99});
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, (uni_property_value_t){.u32 = 9999});
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = "11:22:33:44:55:66"});
    uni_property_set_with_property(&float_prop, (uni_property_value_t){.f32 = 4.25f});

    // Verify NULL property descriptor guards.
    uni_property_value_t null_prop_val = uni_property_get_with_property(NULL);
    assert(null_prop_val.u32 == 0);
    assert(null_prop_val.str == NULL);
    uni_property_set_with_property(NULL, (uni_property_value_t){.u32 = 123});

    // Critical State Isolation: Restore POSIX TLV instance without calling btstack_tlv_posix_deinit().
    btstack_tlv_set_instance(saved_impl, saved_ctx);
}

TEST(property_btstack_tlv_roundtrip_all_types_and_guards) {
    const btstack_tlv_t* tlv_impl = NULL;
    void* tlv_ctx = NULL;
    btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
    assert(tlv_impl != NULL);
    assert(tlv_ctx != NULL);

    // 1. BOOL & U8 round-trip and union zero-initialization check:
    uni_property_value_t orig_ble = uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = false});
    assert(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean == false);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = true});
    assert(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean == true);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, orig_ble);

    uni_property_value_t orig_gap = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, (uni_property_value_t){.u8 = 0});
    uni_property_value_t gap0 = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
    assert(gap0.u8 == 0);
    assert(gap0.u32 == 0);  // Upper bytes must be zeroed, not uninitialized stack garbage
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, (uni_property_value_t){.u8 = 2});
    uni_property_value_t gap2 = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
    assert(gap2.u8 == 2);
    assert(gap2.u32 == 2);
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, orig_gap);

    // 2. U32 round-trip:
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, (uni_property_value_t){.u32 = 2200});
    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD, (uni_property_value_t){.u32 = 6500});
    assert(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32 == 2200);
    assert(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD).u32 == 6500);
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
    assert(fabsf(uni_property_get_with_property(&float_prop).f32 - 1.5f) < 1e-6f);
    uni_property_set_with_property(&float_prop, (uni_property_value_t){.f32 = 3.25f});
    assert(fabsf(uni_property_get_with_property(&float_prop).f32 - 3.25f) < 1e-6f);
    tlv_impl->delete_tag(tlv_ctx, float_tag);

    // 4. STRING round-trip, NULL rejection, > 128 byte rejection, and non-null-terminated raw tag clamp:
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = "01:02:03:04:05:06,"});
    uni_property_value_t s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    assert(s_val.str != NULL);
    assert(strcmp(s_val.str, "01:02:03:04:05:06,") == 0);

    // NULL string must be rejected, preserving previous value.
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = NULL});
    s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    assert(s_val.str != NULL);
    assert(strcmp(s_val.str, "01:02:03:04:05:06,") == 0);

    // Oversized string (strlen == 135 >= 128) must be rejected without buffer overflow.
    char oversized[136];
    memset(oversized, 'A', 135);
    oversized[135] = '\0';
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, (uni_property_value_t){.str = oversized});
    s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    assert(s_val.str != NULL);
    assert(strcmp(s_val.str, "01:02:03:04:05:06,") == 0);

    // Simulate a non-null-terminated 128-byte raw TLV tag in storage and verify clamping at index 127.
    const uint32_t allowlist_tag =
        ((uint32_t)'B' << 24) | ((uint32_t)'P' << 16) | ((uint32_t)'3' << 8) | UNI_PROPERTY_IDX_ALLOWLIST_LIST;
    uint8_t raw_unterminated[128];
    memset(raw_unterminated, 'Z', sizeof(raw_unterminated));
    assert(tlv_impl->store_tag(tlv_ctx, allowlist_tag, raw_unterminated, sizeof(raw_unterminated)) == 0);
    s_val = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    assert(s_val.str != NULL);
    assert(strlen(s_val.str) == 127);
    assert(s_val.str[127] == '\0');
    for (int i = 0; i < 127; i++) {
        assert(s_val.str[i] == 'Z');
    }
    tlv_impl->delete_tag(tlv_ctx, allowlist_tag);

    // 5. Read-Only Guard (UNI_PROPERTY_FLAG_READ_ONLY):
    uni_property_set(UNI_PROPERTY_IDX_VERSION, (uni_property_value_t){.str = "tampered-version"});
    uni_property_value_t ver_after = uni_property_get(UNI_PROPERTY_IDX_VERSION);
    assert(ver_after.str != NULL);
    assert(strcmp(ver_after.str, UNI_VERSION_STRING) == 0);
}

// ============================================================================
// 2.2 Defect B2 & Phase 3.1: Balance Board Global Properties & Hysteresis
// ============================================================================

TEST(balance_board_global_properties_and_defaults_b2) {
    const btstack_tlv_t* tlv_impl = NULL;
    void* tlv_ctx = NULL;
    btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
    assert(tlv_impl != NULL);
    assert(tlv_ctx != NULL);

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
    assert(uni_property_get_property_by_name(UNI_PROPERTY_NAME_UNI_BB_MOVE_THRESHOLD) != NULL);
    assert(uni_property_get_property_by_name(UNI_PROPERTY_NAME_UNI_BB_FIRE_THRESHOLD) != NULL);

    uint32_t prop_move = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32;
    uint32_t prop_fire = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD).u32;
    assert(prop_move == UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT);
    assert(prop_move == 1500);
    assert(prop_fire == UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT);
    assert(prop_fire == 5000);

    // Call uni_balance_board_init() (without CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE) and verify
    // thresholds are loaded as 1500 / 5000 instead of being zeroed to 0.
    uni_balance_board_init();
    uni_balance_board_threshold_t thr = uni_balance_board_get_threshold();
    assert(thr.move == 1500);
    assert(thr.fire == 5000);
    assert(uni_balance_board_get_move_threshold() == 1500);
    assert(uni_balance_board_get_fire_threshold() == 5000);

    // Exercise Phase 3.1 portable setters/getters and verify TLV persistence across uni_balance_board_init().
    uni_balance_board_set_move_threshold(1800);
    uni_balance_board_set_fire_threshold(6200);
    assert(uni_balance_board_get_move_threshold() == 1800);
    assert(uni_balance_board_get_fire_threshold() == 6200);
    assert(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD).u32 == 1800);
    assert(uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD).u32 == 6200);

    // Reload from TLV via uni_balance_board_init() and confirm persisted values.
    uni_balance_board_init();
    assert(uni_balance_board_get_move_threshold() == 1800);
    assert(uni_balance_board_get_fire_threshold() == 6200);

    // Restore defaults.
    uni_balance_board_set_move_threshold(UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT);
    uni_balance_board_set_fire_threshold(UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT);
    tlv_impl->delete_tag(tlv_ctx, move_tag);
    tlv_impl->delete_tag(tlv_ctx, fire_tag);
    uni_balance_board_init();
    assert(uni_balance_board_get_move_threshold() == 1500);
    assert(uni_balance_board_get_fire_threshold() == 5000);
}

TEST(balance_board_directional_smoothing_and_fire_hysteresis_b2) {
    uni_balance_board_init();
    assert(uni_balance_board_get_move_threshold() == 1500);
    assert(uni_balance_board_get_fire_threshold() == 5000);

    // 1. Directional Low-Pass Filter (smooth_top, smooth_down, smooth_left, smooth_right):
    //    mult_frac((15000 + 15000) - 0, 6, 100) = 1800 > 1500 (move threshold).
    {
        uni_balance_board_t bb = {.tl = 15000, .tr = 15000, .bl = 0, .br = 0};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(joy.up == 1);
        assert(joy.down == 0);
        assert(joy.left == 0);
        assert(joy.right == 0);
    }
    {
        uni_balance_board_t bb = {.tl = 0, .tr = 0, .bl = 15000, .br = 15000};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(joy.down == 1);
        assert(joy.up == 0);
        assert(joy.left == 0);
        assert(joy.right == 0);
    }
    {
        uni_balance_board_t bb = {.tl = 15000, .bl = 15000, .tr = 0, .br = 0};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(joy.left == 1);
        assert(joy.right == 0);
        assert(joy.up == 0);
        assert(joy.down == 0);
    }
    {
        uni_balance_board_t bb = {.tr = 15000, .br = 15000, .tl = 0, .bl = 0};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(joy.right == 1);
        assert(joy.left == 0);
        assert(joy.up == 0);
        assert(joy.down == 0);
    }
    {
        uni_balance_board_t bb = {.tl = 2000, .tr = 2000, .bl = 2000, .br = 2000};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(joy.up == 0);
        assert(joy.down == 0);
        assert(joy.left == 0);
        assert(joy.right == 0);
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
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_THRESHOLD);
        assert(state.fire_counter == 0);
        assert(joy.fire == 0);

        // Step 2: THRESHOLD -> IN_AIR (all sensors < 1600)
        bb.tl = 100;
        bb.tr = 100;
        bb.bl = 100;
        bb.br = 100;
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_IN_AIR);
        assert(state.fire_counter == 0);
        assert(joy.fire == 0);

        // Step 3: 2 frames in air keep state in IN_AIR (fire_counter = 1, 2); 3rd frame triggers FIRE
        for (int i = 1; i <= 2; i++) {
            memset(&joy, 0, sizeof(joy));
            uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
            assert(state.fire_state == UNI_BALANCE_BOARD_STATE_IN_AIR);
            assert(state.fire_counter == i);
            assert(joy.fire == 0);
        }
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_FIRE);
        assert(state.fire_counter == 0);
        assert(joy.fire == 1);

        // Step 4: Maintain FIRE for 10 frames (fire_counter 1..10); 11th frame resets to RESET
        for (int i = 1; i <= 10; i++) {
            memset(&joy, 0, sizeof(joy));
            uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
            assert(state.fire_state == UNI_BALANCE_BOARD_STATE_FIRE);
            assert(state.fire_counter == i);
            assert(joy.fire == 1);
        }
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_RESET);
        assert(state.fire_counter == 0);
    }

    // 3. Abort Branch 1: THRESHOLD 10-Frame Timeout Without Jumping
    {
        uni_balance_board_t bb = {.tl = 1500, .tr = 1500, .bl = 1500, .br = 1500};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};

        // Enter THRESHOLD (sum = 6000 >= 5000)
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_THRESHOLD);

        // Feed intermediate weight (sum = 4000 < 5000, but tl = 2000 >= 1600 so not IN_AIR)
        bb.tl = 2000;
        bb.tr = 1000;
        bb.bl = 500;
        bb.br = 500;
        for (int i = 1; i <= 10; i++) {
            memset(&joy, 0, sizeof(joy));
            uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
            assert(state.fire_state == UNI_BALANCE_BOARD_STATE_THRESHOLD);
            assert(joy.fire == 0);
        }
        // 11th frame (fire_counter == 11 > 10) aborts back to RESET without firing
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_RESET);
        assert(state.fire_counter == 0);
        assert(joy.fire == 0);
    }

    // 4. Abort Branch 2: IN_AIR Premature Landing Before > 2 Frames
    {
        uni_balance_board_t bb = {.tl = 1500, .tr = 1500, .bl = 1500, .br = 1500};
        uni_balance_board_state_t state = {0};
        uni_joystick_t joy = {0};

        // Enter THRESHOLD -> IN_AIR
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_THRESHOLD);

        bb.tl = 100;
        bb.tr = 100;
        bb.bl = 100;
        bb.br = 100;
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_IN_AIR);

        // 1 frame in air (fire_counter = 1)
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_IN_AIR);
        assert(state.fire_counter == 1);

        // Premature landing (tl = 2000 >= 1600) on frame 2 -> immediately resets to RESET
        bb.tl = 2000;
        memset(&joy, 0, sizeof(joy));
        uni_joy_to_single_joy_from_balance_board(&bb, &state, &joy);
        assert(state.fire_state == UNI_BALANCE_BOARD_STATE_RESET);
        assert(state.fire_counter == 0);
        assert(joy.fire == 0);
    }
}

int main(void) {
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    uni_property_init();

    RUN_TEST(property_btstack_tlv_null_instance_fallback_b1);
    RUN_TEST(property_btstack_tlv_roundtrip_all_types_and_guards);
    RUN_TEST(balance_board_global_properties_and_defaults_b2);
    RUN_TEST(balance_board_directional_smoothing_and_fire_hysteresis_b2);

    printf("All uni_property_and_bb tests passed!\n");
    return 0;
}
