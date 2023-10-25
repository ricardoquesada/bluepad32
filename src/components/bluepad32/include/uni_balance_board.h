/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

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

#define UNI_BALANCE_BOARD_FIRE_MAX_FRAMES 25   // Max frames that fire can be kept pressed
#define UNI_BALANCE_BOARD_IDLE_THRESHOLD 1600  // Below this value, it is considered that noone is on top of the BB

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
void uni_balance_board_on_init_complete(void);
uni_balance_board_threshold_t uni_balance_board_get_threshold(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BALANCE_BOARD_H
