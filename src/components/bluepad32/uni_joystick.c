
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

#include "uni_joystick.h"

#include "uni_gamepad.h"

static void to_single_joy(const uni_gamepad_t* gp, uni_joystick_t* out_joy) {
    // Button A is "fire"
    out_joy->fire |= ((gp->buttons & BUTTON_A) != 0);
    // Thumb left is "fire"
    out_joy->fire |= ((gp->buttons & BUTTON_THUMB_L) != 0);

    // Shoulder right is "auto fire"
    out_joy->auto_fire |= ((gp->buttons & BUTTON_SHOULDER_R) != 0);

    // Dpad
    if (gp->dpad & DPAD_UP)
        out_joy->up |= 1;
    if (gp->dpad & DPAD_DOWN)
        out_joy->down |= 1;
    if (gp->dpad & DPAD_RIGHT)
        out_joy->right |= 1;
    if (gp->dpad & DPAD_LEFT)
        out_joy->left |= 1;

    // Axis: X and Y
    out_joy->left |= (gp->axis_x < -AXIS_THRESHOLD);
    out_joy->right |= (gp->axis_x > AXIS_THRESHOLD);
    out_joy->up |= (gp->axis_y < -AXIS_THRESHOLD);
    out_joy->down |= (gp->axis_y > AXIS_THRESHOLD);

    // Pots
    out_joy->button2 = (gp->brake >> 2);     // convert from 1024 to 256
    out_joy->button3 = (gp->throttle >> 2);  // convert from 1024 to 256
}

// Basic Mode: One gamepad controls one joystick
void uni_joy_to_single_joy_from_gamepad(const uni_gamepad_t* gp, uni_joystick_t* out_joy) {
    to_single_joy(gp, out_joy);

    // Buttom B is "jump". Good for C64 games
    out_joy->up |= ((gp->buttons & BUTTON_B) != 0);

    // 2nd Button for Atari ST / Amiga
    out_joy->button2 |= ((gp->buttons & BUTTON_X) != 0);

    // 3rd Button for Amiga
    out_joy->button3 |= ((gp->buttons & BUTTON_Y) != 0);
}

// Enhanced mode: One gamepad controls two joysticks
void uni_joy_to_combo_joy_joy_from_gamepad(const uni_gamepad_t* gp,
                                           uni_joystick_t* out_joy1,
                                           uni_joystick_t* out_joy2) {
    to_single_joy(gp, out_joy2);

    // Buttom B is "fire"
    out_joy1->fire |= ((gp->buttons & BUTTON_B) != 0);
    // Thumb right is "fire"
    out_joy1->fire |= ((gp->buttons & BUTTON_THUMB_R) != 0);

    // Swap "auto fire" in Combo Joy Joy.
    // "left" belongs to joy1 while "right" to joy2.
    out_joy2->auto_fire = ((gp->buttons & BUTTON_SHOULDER_L) != 0);
    out_joy1->auto_fire = ((gp->buttons & BUTTON_SHOULDER_R) != 0);

    // Axis: RX and RY
    out_joy1->left |= (gp->axis_rx < -AXIS_THRESHOLD);
    out_joy1->right |= (gp->axis_rx > AXIS_THRESHOLD);
    out_joy1->up |= (gp->axis_ry < -AXIS_THRESHOLD);
    out_joy1->down |= (gp->axis_ry > AXIS_THRESHOLD);
}
