// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BALANCE_BOARD_H
#define UNI_BALANCE_BOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief States of fire for the Balance Board.
 */
enum {
    UNI_BALANCE_BOARD_STATE_RESET,     /**< After fire */
    UNI_BALANCE_BOARD_STATE_THRESHOLD, /**< "fire threshold" detected */
    UNI_BALANCE_BOARD_STATE_IN_AIR,    /**< in the air */
    UNI_BALANCE_BOARD_STATE_FIRE,      /**< "fire pressed" */
};

/** @brief Max frames that fire can be kept pressed */
#define UNI_BALANCE_BOARD_FIRE_MAX_FRAMES 25
/** @brief Below this value, it is considered that no one is on top of the BB */
#define UNI_BALANCE_BOARD_IDLE_THRESHOLD 1600
/** @brief Diff in weight to consider a Movement */
#define UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT 1500
/** @brief Max weight before staring the "de-accel" to trigger fire. */
#define UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT 5000

/**
 * @brief Represents the Balance Board sensor values.
 */
typedef struct {
    uint16_t tr;     /**< Top right */
    uint16_t br;     /**< Bottom right */
    uint16_t tl;     /**< Top left */
    uint16_t bl;     /**< Bottom left */
    int temperature; /**< Temperature */
} uni_balance_board_t;

/**
 * @brief Represents the Balance Board state.
 * Used by Balance Board to determine joystick movements/fire.
 */
typedef struct {
    uint8_t fire_state;
    uint8_t fire_counter;
    int16_t smooth_left;
    int16_t smooth_right;
    int16_t smooth_top;
    int16_t smooth_down;
} uni_balance_board_state_t;

/**
 * @brief Represents the threshold for movement and fire.
 */
typedef struct {
    int move;
    int fire;
} uni_balance_board_threshold_t;

/** @brief Dump raw Balance Board sensor weights and temperature to the info log. */
void uni_balance_board_dump(const uni_balance_board_t* bb);

/**
 * @brief Register Balance Board threshold CLI commands (`bb_move_threshold`, `bb_fire_threshold`).
 *
 * Implemented in `arch/uni_console_esp32.c` when ESP32 USB console support is enabled so that
 * `controller/uni_balance_board.c` remains portable across non-ESP-IDF targets.
 */
void uni_balance_board_register_cmds(void);

/**
 * @brief Initialize Balance Board move and fire thresholds from persistent properties.
 *
 * Reads `UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD` and `UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD`
 * via `uni_property_get()` (falling back to `UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT` and
 * `UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT` when unset).
 */
void uni_balance_board_init(void);

/** @brief Return the current in-memory move and fire weight thresholds. */
uni_balance_board_threshold_t uni_balance_board_get_threshold(void);

/** @brief Update the Balance Board move weight threshold in memory and persist it via `uni_property_set()`. */
void uni_balance_board_set_move_threshold(int threshold);

/** @brief Return the current Balance Board move weight threshold. */
int uni_balance_board_get_move_threshold(void);

/** @brief Update the Balance Board fire weight threshold in memory and persist it via `uni_property_set()`. */
void uni_balance_board_set_fire_threshold(int threshold);

/** @brief Return the current Balance Board fire weight threshold. */
int uni_balance_board_get_fire_threshold(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BALANCE_BOARD_H
