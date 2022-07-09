/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2019 Ricardo Quesada

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

#ifndef UNI_JOYSTICK_H
#define UNI_JOYSTICK_H

#include <stdint.h>

#include "uni_gamepad.h"

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

void uni_joy_to_single_joy_from_gamepad(const uni_gamepad_t* gp, uni_joystick_t* out_joy);
void uni_joy_to_combo_joy_joy_from_gamepad(const uni_gamepad_t* gp, uni_joystick_t* out_joy1, uni_joystick_t* out_joy2);

#endif  // UNI_JOYSTICK_H
