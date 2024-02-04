// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_GAMEPAD_H
#define UNI_GAMEPAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_common.h"

extern const int AXIS_NORMALIZE_RANGE;
extern const int AXIS_THRESHOLD;

typedef enum {
    UNI_GAMEPAD_MAPPINGS_TYPE_XBOX,    // A (south), B (east), X (west), Y (north). Default
    UNI_GAMEPAD_MAPPINGS_TYPE_SWITCH,  // A (east), B (south), X (north), Y (west)
    UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM,  // User provided its own mappings

    UNI_GAMEPAD_MAPPINGS_TYPE_COUNT,
} uni_gamepad_mappings_type_t;

typedef enum {
    UNI_GAMEPAD_MAPPINGS_DPAD_UP,
    UNI_GAMEPAD_MAPPINGS_DPAD_DOWN,
    UNI_GAMEPAD_MAPPINGS_DPAD_RIGHT,
    UNI_GAMEPAD_MAPPINGS_DPAD_LEFT,
} uni_gamepad_mappings_dpad_t;

typedef enum {
    UNI_GAMEPAD_MAPPINGS_BUTTON_A,
    UNI_GAMEPAD_MAPPINGS_BUTTON_B,
    UNI_GAMEPAD_MAPPINGS_BUTTON_X,
    UNI_GAMEPAD_MAPPINGS_BUTTON_Y,
    UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_L,
    UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_R,
    UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_L,
    UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_R,
    UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_L,
    UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_R,
} uni_gamepad_mappings_button_t;

typedef enum {
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SYSTEM,
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SELECT,
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_START,
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_CAPTURE,
} uni_gamepad_mappings_misc_button_t;

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

// DPAD constants.
enum {
    DPAD_UP = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_UP),
    DPAD_DOWN = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_DOWN),
    DPAD_RIGHT = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_RIGHT),
    DPAD_LEFT = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_LEFT),
};

// BUTTON_ are the main gamepad buttons, like X, Y, A, B, etc.
enum {
    BUTTON_A = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_A),
    BUTTON_B = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_B),
    BUTTON_X = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_X),
    BUTTON_Y = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_Y),
    BUTTON_SHOULDER_L = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_L),
    BUTTON_SHOULDER_R = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_R),
    BUTTON_TRIGGER_L = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_L),
    BUTTON_TRIGGER_R = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_R),
    BUTTON_THUMB_L = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_L),
    BUTTON_THUMB_R = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_R),
};

// MISC_BUTTONS_ are buttons that are usually not used in the game, but are
// helpers like "select", "start", "system", "mute", etc.
enum {
    MISC_BUTTON_SYSTEM = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SYSTEM),    // AKA: PS, Xbox, etc.
    MISC_BUTTON_SELECT = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SELECT),    // AKA: Select, Share, Create, -
    MISC_BUTTON_START = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_START),      // AKA: Start, Options, +
    MISC_BUTTON_CAPTURE = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_CAPTURE),  // AKA: Mute, Capture, Share

    // Deprecated
    MISC_BUTTON_BACK = MISC_BUTTON_SELECT,
    MISC_BUTTON_HOME = MISC_BUTTON_START,
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
// The virtual gamepad will then be processed by the different "platforms".
//
// Virtual Gamepad layout:
//
//  Left             Center            Right
//
//  brake: 0-1023    Menu button       throttle: 0-1023
//  L-shoulder button                  R-shoulder button
//  L-trigger button                   R-trigger button
//  d-pad                              buttons: A,B,X,Y,
//  L-joypad (axis: -512, 511)         R-joypad (axis: -512, 511)
//  axis-L button                      axis-R button
//  Gyro: is measured in degress/second
//  Accelerometer: is measured in "G"s

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

    // Usage Page: 0x09 (Button)
    uint16_t buttons;

    // Misc buttons (from 0x0c (Consumer) and others)
    uint8_t misc_buttons;

    int32_t gyro[3];
    int32_t accel[3];
} uni_gamepad_t;

// Represents the mapping. Each entry contains the new button to be used,
// and not the value of the buttons.
typedef struct {
    // Remaps for Dpad
    uint8_t dpad_up;
    uint8_t dpad_down;
    uint8_t dpad_left;
    uint8_t dpad_right;

    // Remaps for the buttons
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

    uint8_t misc_button_select;
    uint8_t misc_button_start;
    uint8_t misc_button_system;
    uint8_t misc_button_capture;

    // Remaps for axis
    uint8_t axis_x;
    uint8_t axis_y;
    uint8_t axis_rx;
    uint8_t axis_ry;

    // Whether the axis should be inverted
    uint8_t axis_x_inverted;
    uint8_t axis_y_inverted;
    uint8_t axis_rx_inverted;
    uint8_t axis_ry_inverted;

    // Remaps for brake / throttle
    uint8_t brake;
    uint8_t throttle;
} uni_gamepad_mappings_t;

extern const uni_gamepad_mappings_t GAMEPAD_DEFAULT_MAPPINGS;

void uni_gamepad_dump(const uni_gamepad_t* gp);

uni_gamepad_t uni_gamepad_remap(const uni_gamepad_t* gp);
void uni_gamepad_set_mappings(const uni_gamepad_mappings_t* mapping);
void uni_gamepad_set_mappings_type(uni_gamepad_mappings_type_t type);
uni_gamepad_mappings_type_t uni_gamepad_get_mappings_type(void);
const char* uni_gamepad_get_model_name(int type);

#ifdef __cplusplus
}
#endif

#endif  // UNI_GAMEPAD_H
