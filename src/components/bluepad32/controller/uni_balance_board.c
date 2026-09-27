// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "controller/uni_balance_board.h"

#include "uni_log.h"
#include "uni_property.h"

// Gets initialized at platform_init time.
static uni_balance_board_threshold_t bb_threshold = {
    .move = UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT,
    .fire = UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT,
};

static int get_bb_move_threshold_from_nvs(void) {
    uni_property_value_t value;

    value = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD);
    return value.u32;
}

static int get_bb_fire_threshold_from_nvs(void) {
    uni_property_value_t value;

    value = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD);
    return value.u32;
}

static void set_bb_move_threshold_to_nvs(int threshold) {
    uni_property_value_t value;
    value.u32 = threshold;

    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, value);
}

static void set_bb_fire_threshold_to_nvs(int threshold) {
    uni_property_value_t value;
    value.u32 = threshold;

    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD, value);
}

void uni_balance_board_set_move_threshold(int threshold) {
    bb_threshold.move = threshold;
    set_bb_move_threshold_to_nvs(threshold);
}

int uni_balance_board_get_move_threshold(void) {
    return bb_threshold.move;
}

void uni_balance_board_set_fire_threshold(int threshold) {
    bb_threshold.fire = threshold;
    set_bb_fire_threshold_to_nvs(threshold);
}

int uni_balance_board_get_fire_threshold(void) {
    return bb_threshold.fire;
}

void uni_balance_board_init(void) {
    // Load persisted Balance Board thresholds unconditionally on all platforms via uni_property
    // (decoupled from CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE so persisted values take effect even
    // when the interactive ESP32 USB console is disabled).
    bb_threshold.move = get_bb_move_threshold_from_nvs();
    bb_threshold.fire = get_bb_fire_threshold_from_nvs();
}

void uni_balance_board_dump(const uni_balance_board_t* bb) {
    // Don't add "\n"
    logi("tl=%d, tr=%d, bl=%d, br=%d, temperature=%d", bb->tl, bb->tr, bb->bl, bb->br, bb->temperature);
}

uni_balance_board_threshold_t uni_balance_board_get_threshold(void) {
    return bb_threshold;
}
