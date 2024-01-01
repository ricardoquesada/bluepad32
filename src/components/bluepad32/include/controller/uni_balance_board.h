// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BALANCE_BOARD_H
#define UNI_BALANCE_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// States of fire
enum {
    UNI_BALANCE_BOARD_STATE_RESET,      // After fire
    UNI_BALANCE_BOARD_STATE_THRESHOLD,  // "fire threshold" detected
    UNI_BALANCE_BOARD_STATE_IN_AIR,     // in the air
    UNI_BALANCE_BOARD_STATE_FIRE,       // "fire pressed"
};

// Balance Board defaults
#define UNI_BALANCE_BOARD_FIRE_MAX_FRAMES 25   // Max frames that fire can be kept pressed
#define UNI_BALANCE_BOARD_IDLE_THRESHOLD 1600  // Below this value, it is considered that no one is on top of the BB
#define UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT 1500  // Diff in weight to consider a Movement
#define UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT 5000  // Max weight before staring the "de-accel" to trigger fire.

// Represents the Balance Board sensor values.
typedef struct {
    uint16_t tr;      // Top right
    uint16_t br;      // Bottom right
    uint16_t tl;      // Top left
    uint16_t bl;      // Bottom left
    int temperature;  // Temperature
} uni_balance_board_t;

// Represents the Balance Board state.
// Used by Balance Board to determine joystick movements/fire
typedef struct {
    uint8_t fire_state;
    uint8_t fire_counter;
    int16_t smooth_left;
    int16_t smooth_right;
    int16_t smooth_top;
    int16_t smooth_down;
} uni_balance_board_state_t;

// Represents the threshold for movement and fire.
typedef struct {
    int move;
    int fire;
} uni_balance_board_threshold_t;

void uni_balance_board_dump(const uni_balance_board_t* bb);

void uni_balance_board_register_cmds(void);

void uni_balance_board_init(void);

uni_balance_board_threshold_t uni_balance_board_get_threshold(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BALANCE_BOARD_H
