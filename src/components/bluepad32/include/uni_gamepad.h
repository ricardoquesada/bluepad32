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

#ifndef UNI_GAMEPAD_H
#define UNI_GAMEPAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_common.h"

extern const int AXIS_NORMALIZE_RANGE;
extern const int AXIS_THRESHOLD;

// DPAD constants.
enum {
    DPAD_UP = BIT(0),
    DPAD_DOWN = BIT(1),
    DPAD_RIGHT = BIT(2),
    DPAD_LEFT = BIT(3),
};

// BUTTON_ are the main gamepad buttons, like X, Y, A, B, etc.
enum {
    BUTTON_A = BIT(0),
    BUTTON_B = BIT(1),
    BUTTON_X = BIT(2),
    BUTTON_Y = BIT(3),
    BUTTON_SHOULDER_L = BIT(4),
    BUTTON_SHOULDER_R = BIT(5),
    BUTTON_TRIGGER_L = BIT(6),
    BUTTON_TRIGGER_R = BIT(7),
    BUTTON_THUMB_L = BIT(8),
    BUTTON_THUMB_R = BIT(9),
};

// MISC_BUTTONS_ are buttons that are usually not used in the game, but are
// helpers like "back", "home", etc.
enum {
    MISC_BUTTON_SYSTEM = BIT(0),  // AKA: PS, Xbox, etc.
    MISC_BUTTON_BACK = BIT(1),    // AKA: Select, Share, -
    MISC_BUTTON_HOME = BIT(2),    // AKA: Start, Options, +
};

// GAMEPAD_STATE_ are used internally to determine which button event
// were registered in the last HID report.
// Most gamepad (if not all) report all their buttons in just one report.
// TODO: Investigate if this is legacy code, or it is actually needed for iCade.
enum {
    GAMEPAD_STATE_DPAD = BIT(0),

    GAMEPAD_STATE_AXIS_X = BIT(1),
    GAMEPAD_STATE_AXIS_Y = BIT(2),
    GAMEPAD_STATE_AXIS_RX = BIT(3),
    GAMEPAD_STATE_AXIS_RY = BIT(4),

    GAMEPAD_STATE_BRAKE = BIT(5),     // AKA L2
    GAMEPAD_STATE_THROTTLE = BIT(6),  // AKA R2

    GAMEPAD_STATE_BUTTON_A = BIT(10),
    GAMEPAD_STATE_BUTTON_B = BIT(11),
    GAMEPAD_STATE_BUTTON_X = BIT(12),
    GAMEPAD_STATE_BUTTON_Y = BIT(13),
    GAMEPAD_STATE_BUTTON_SHOULDER_L = BIT(14),  // AKA L1
    GAMEPAD_STATE_BUTTON_SHOULDER_R = BIT(15),  // AKA R1
    GAMEPAD_STATE_BUTTON_TRIGGER_L = BIT(16),
    GAMEPAD_STATE_BUTTON_TRIGGER_R = BIT(17),
    GAMEPAD_STATE_BUTTON_THUMB_L = BIT(18),
    GAMEPAD_STATE_BUTTON_THUMB_R = BIT(19),

    GAMEPAD_STATE_MISC_BUTTON_BACK = BIT(24),
    GAMEPAD_STATE_MISC_BUTTON_HOME = BIT(25),
    GAMEPAD_STATE_MISC_BUTTON_MENU = BIT(26),
    GAMEPAD_STATE_MISC_BUTTON_SYSTEM = BIT(27),
};

// Represents which "seat" the gamepad is using. Multiple gamepads can be
// connected at the same time, and the "seat" is the ID of each gamepad.
// In the Legacy firmware it was possible for a gamepad to take more than one
// seat, but since the v2.0 it might not be needed anymore.
// TODO: Investigate if this really needs to be a "bit".
typedef enum {
    GAMEPAD_SEAT_NONE = 0,
    GAMEPAD_SEAT_A = BIT(0),
    GAMEPAD_SEAT_B = BIT(1),
    GAMEPAD_SEAT_C = BIT(2),
    GAMEPAD_SEAT_D = BIT(3),

    // Masks
    GAMEPAD_SEAT_AB_MASK = (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B),
} uni_gamepad_seat_t;

// uni_gamepad_t is a virtual gamepad.
// Different parsers should populate this virtual gamepad accordingly.
// For example, the virtual gamepad doesn't have a Hat, but has a D-pad.
// If the real gamepad has a Hat, it should populate the D-pad instead.
//
// If the real device only has one joy-pad, it should populate the left side
// of the virtual gamepad.
// Example: a TV remote control should populate the left D-pad and the left
// joypad.
//
// The virtual gamepad will then be processed by the dfferent "platforms".
//
// Virtual Gamepad layout:
//
//  Left             Center            Right
//
//  brake: 0-1023    Menu button       accelerator: 0-1023
//  L-shoulder button                  R-shoulder button
//  L-trigger button                   R-trigger button
//  d-pad                              buttons: A,B,X,Y,
//  l-joypad (axis: -512, 511)         r-joypad (axis: -512, 511)
//  axis-l button                      axis-r button
//
//  trigger's buttons & accelerator are shared physically.

typedef struct {
    // Usage Page: 0x01 (Generic Desktop Controls)
    uint8_t dpad;
    int32_t axis_x;
    int32_t axis_y;
    int32_t axis_rx;
    int32_t axis_ry;

    // Usage Page: 0x02 (Sim controls)
    int32_t brake;
    int32_t throttle;

    // Usage Page: 0x06 (Generic dev controls)
    uint16_t battery;

    // Usage Page: 0x09 (Button)
    uint16_t buttons;

    // Misc buttons (from 0x0c (Consumer) and others)
    uint8_t misc_buttons;

    // FIXME: It might be OK to get rid of this variable. Or in any case, it
    // should be moved ouside uni_gamepad_t?
    // Indicates which states have been updated
    uint32_t updated_states;
} uni_gamepad_t;

// Represents the mapping. Each entry contains the new button to be used,
// and not the value of the buttons.
typedef struct {
    uint8_t dpad_up;
    uint8_t dpad_down;
    uint8_t dpad_left;
    uint8_t dpad_right;

    uint8_t button_a;
    uint8_t button_b;
    uint8_t button_x;
    uint8_t button_y;

    uint8_t button_shoulder_l;
    uint8_t button_shoulder_r;
    uint8_t button_trigger_l;
    uint8_t button_trigger_r;

    uint8_t button_thumb_l;
    uint8_t button_thumb_r;

    uint8_t misc_button_back;
    uint8_t misc_button_home;
    uint8_t misc_button_system;
} uni_gamepad_mappings_t;

typedef enum {
    UNI_GAMEPAD_MAPPINGS_AXIS_X,
    UNI_GAMEPAD_MAPPINGS_AXIS_Y,
    UNI_GAMEPAD_MAPPINGS_AXIS_RX,
    UNI_GAMEPAD_MAPPINGS_AXIS_RY,

} uni_gamepad_mappings_axis_t;

typedef enum {
    UNI_GAMEPAD_MAPPINGS_PEDAL_BRAKE,
    UNI_GAMEPAD_MAPPINGS_PEDAL_THROTTLE,
} uni_gamepad_mappings_pedal_t;

void uni_gamepad_dump(const uni_gamepad_t* gp);

uni_gamepad_t uni_gamepad_remap(const uni_gamepad_t* gp);
void uni_gamepad_set_mappings(const uni_gamepad_mappings_t* mapping);

#ifdef __cplusplus
}
#endif

#endif  // UNI_GAMEPAD_H
