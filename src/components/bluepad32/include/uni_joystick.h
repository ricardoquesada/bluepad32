// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_JOYSTICK_H
#define UNI_JOYSTICK_H

#include <stdint.h>

#include "controller/uni_balance_board.h"
#include "controller/uni_gamepad.h"
#include "controller/uni_keyboard.h"

// Valid for Amiga, Atari 8-bit, Atari St, C64 and others...
typedef struct {
    uint8_t up;         // line 1 - Y2 for quad mouse
    uint8_t down;       // line 2 - X1 for quad mouse
    uint8_t left;       // line 3 - Y1 for quad mouse
    uint8_t right;      // line 4 - X2 for quad mouse
    uint8_t button3;    // line 5 - Middle button for mouse, Pot Y (C64), Pot X (Amiga)
    uint8_t fire;       // line 6 - Left button for mouse
    uint8_t _power;     // line 7 - +5v. added as ref only
    uint8_t _ground;    // line 8 - ground. added as ref only
    uint8_t button2;    // line 9 - Right button for mouse, Pot X (C64), Pot Y (Amiga)
    uint8_t auto_fire;  // virtual button
} uni_joystick_t;

// Gamepad related
void uni_joy_to_single_joy_from_gamepad(const uni_gamepad_t* gp, uni_joystick_t* out_joy, int use_two_buttons);
void uni_joy_to_twinstick_from_gamepad(const uni_gamepad_t* gp, uni_joystick_t* out_joy1, uni_joystick_t* out_joy2);

// Wii related
void uni_joy_to_single_from_wii_accel(const uni_gamepad_t* gp, uni_joystick_t* out_joy);

// Keyboard related
void uni_joy_to_single_joy_from_keyboard(const uni_keyboard_t* kb, uni_joystick_t* out_joy);
void uni_joy_to_twinstick_from_keyboard(const uni_keyboard_t* kb, uni_joystick_t* out_joy1, uni_joystick_t* out_joy2);

// Balance Board
void uni_joy_to_single_joy_from_balance_board(const uni_balance_board_t* bb,
                                              uni_balance_board_state_t* bb_state,
                                              uni_joystick_t* out_joy);

#endif  // UNI_JOYSTICK_H
