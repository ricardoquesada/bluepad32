// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_joystick.h"

#include <string.h>

#include "hid_usage.h"
#include "uni_log.h"

// When accelerometer mode is enabled, it will use it as if it were
// in the Nintendo Wii Wheel.
#define ENABLE_ACCEL_WHEEL_MODE 1

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

    // 2nd & 3rd buttons
    out_joy->button2 = (gp->brake >> 2);     // convert from 1024 to 256
    out_joy->button3 = (gp->throttle >> 2);  // convert from 1024 to 256
}

// Basic Mode: One gamepad controls one joystick
void uni_joy_to_single_joy_from_gamepad(const uni_gamepad_t* gp, uni_joystick_t* out_joy, int use_two_buttons) {
    to_single_joy(gp, out_joy);

    if (!use_two_buttons) {
        // Buttom B is "jump". Good for C64 games
        out_joy->up |= ((gp->buttons & BUTTON_B) != 0);
    } else {
        // Buttom B is second joystick button, as in MSX
        out_joy->button2 = ((gp->buttons & BUTTON_B) != 0);
    }

    // 2nd & 3rd buttons
    out_joy->button2 |= ((gp->buttons & BUTTON_X) != 0);
    out_joy->button3 |= ((gp->buttons & BUTTON_Y) != 0);
}

// Twin Stick mode: One gamepad controls two joysticks
void uni_joy_to_twinstick_from_gamepad(const uni_gamepad_t* gp, uni_joystick_t* out_joy1, uni_joystick_t* out_joy2) {
    to_single_joy(gp, out_joy2);

    out_joy2->button2 |= ((gp->buttons & BUTTON_X) != 0);

    // Button B is "fire"
    out_joy1->fire |= ((gp->buttons & BUTTON_B) != 0);
    // Thumb right is "fire"
    out_joy1->fire |= ((gp->buttons & BUTTON_THUMB_R) != 0);

    out_joy1->button2 |= ((gp->buttons & BUTTON_Y) != 0);

    // Swap "auto fire" in Twin Stick
    // "left" belongs to joy1 while "right" to joy2.
    out_joy2->auto_fire = ((gp->buttons & BUTTON_SHOULDER_L) != 0);
    out_joy1->auto_fire = ((gp->buttons & BUTTON_SHOULDER_R) != 0);

    // Axis: RX and RY
    out_joy1->left |= (gp->axis_rx < -AXIS_THRESHOLD);
    out_joy1->right |= (gp->axis_rx > AXIS_THRESHOLD);
    out_joy1->up |= (gp->axis_ry < -AXIS_THRESHOLD);
    out_joy1->down |= (gp->axis_ry > AXIS_THRESHOLD);
}

void uni_joy_to_single_from_wii_accel(const uni_gamepad_t* gp, uni_joystick_t* out_joy) {
    // Button "1" is Brake (down), and button "2" is Throttle (up)
    // Buttons "1" and "2" can override values from Dpad.

    const int16_t accel_threshold = 26;

    int sx = gp->accel[0];
    int sy = gp->accel[1];

    memset(out_joy, 0, sizeof(*out_joy));

#ifdef ENABLE_ACCEL_WHEEL_MODE
    // Is the wheel in resting position, don't read accelerometer
    if (sx > -accel_threshold && sx < accel_threshold) {
        // Accelerometer reading disabled.
        // logd("Wii: Wheel in resting position, do nothing");
    } else {
        // Preserve Dpad values... they are used to navigate menus.
        out_joy->up |= (gp->dpad & DPAD_UP) ? 1 : 0;
        out_joy->down |= (gp->dpad & DPAD_DOWN) ? 1 : 0;
        out_joy->left |= (gp->dpad & DPAD_LEFT) ? 1 : 0;
        out_joy->right |= (gp->dpad & DPAD_RIGHT) ? 1 : 0;

        // Button "1" is Brake (down), and button "2" is Throttle (up)
        // Buttons "1" and "2" can override values from Dpad.
        out_joy->down |= (gp->buttons & BUTTON_A) ? DPAD_DOWN : 0;
        out_joy->up |= (gp->buttons & BUTTON_B) ? DPAD_UP : 0;

        // Either "A" or "trigger" is used as fire
        out_joy->fire = (gp->buttons & BUTTON_X) ? 1 : 0;
        out_joy->fire |= (gp->buttons & BUTTON_Y) ? 1 : 0;

        // Accelerometer overrides Dpad values.
        if (sy > accel_threshold) {
            out_joy->left = 1;
            out_joy->right = 0;
        } else if (sy < -accel_threshold) {
            out_joy->right = 1;
            out_joy->left = 0;
        }
    }

#else   // !ENABLE_ACCEL_WHEEL_MODE
    if (sx < -accel_threshold) {
        out_joy->left = 1;
    } else if (sx > accel_threshold) {
        out_joy->right = 1;
    }
    if (sy < -accel_threshold) {
        out_joy->up = 1;
    } else if (sy > (accel_threshold / 2)) {
        // Threshold for down is 50% because it is not as easy to tilt the
        // device down as it is it to tilt it up.
        out_joy->down = 1;
    }
#endif  // ! ENABLE_ACCEL_WHEEL_MODE
}

static void to_joy_from_keyboard(const uni_keyboard_t* kb, uni_joystick_t* out_joy1, uni_joystick_t* out_joy2) {
    // Sanity check. Joy1 must be valid, joy2 can be null
    if (!out_joy1) {
        loge("Joystick: Invalid joy1 for keyboard\n");
        return;
    }

    // Keys
    for (int i = 0; i < UNI_KEYBOARD_PRESSED_KEYS_MAX; i++) {
        // Stop on values from 0-3, they invalid codes.
        const uint8_t key = kb->pressed_keys[i];
        if (key <= HID_USAGE_KB_ERROR_UNDEFINED)
            break;
        switch (key) {
            // Valid for both "single" and "twin stick" modes
            // 1st joystick: Arrow keys
            case HID_USAGE_KB_LEFT_ARROW:
                out_joy1->left = 1;
                break;
            case HID_USAGE_KB_RIGHT_ARROW:
                out_joy1->right = 1;
                break;
            case HID_USAGE_KB_UP_ARROW:
                out_joy1->up = 1;
                break;
            case HID_USAGE_KB_DOWN_ARROW:
                out_joy1->down = 1;
                break;

            // Only valid in "single" mode
            // 1st joystick: Buttons
            case HID_USAGE_KB_SPACEBAR:
            case HID_USAGE_KB_Z:
                if (out_joy2 == NULL)
                    out_joy1->fire = 1;
                break;
            case HID_USAGE_KB_X:
                if (out_joy2 == NULL)
                    out_joy1->button2 = 1;
                break;
            case HID_USAGE_KB_C:
                if (out_joy2 == NULL)
                    out_joy1->button3 = 1;
                break;

            // Only valid in "twin stick" mode
            // 2nd joystick: WASD and buttons (Q, E, R)
            case HID_USAGE_KB_W:
                if (out_joy2)
                    out_joy2->up = 1;
                break;

            case HID_USAGE_KB_A:
                if (out_joy2)
                    out_joy2->left = 1;
                break;

            case HID_USAGE_KB_S:
                if (out_joy2)
                    out_joy2->down = 1;
                break;

            case HID_USAGE_KB_D:
                if (out_joy2)
                    out_joy2->right = 1;
                break;

            case HID_USAGE_KB_Q:
                if (out_joy2)
                    out_joy2->button2 = 1;
                break;

            case HID_USAGE_KB_E:
                if (out_joy2)
                    out_joy2->fire = 1;
                break;

            case HID_USAGE_KB_R:
                if (out_joy2)
                    out_joy2->button3 = 1;
                break;

            default:
                break;
        }
    }

    // Joystick 1 buttons from Modifiers
    if (out_joy2 == NULL) {
        // Left modifiers only valid when in "single" mode
        out_joy1->fire |= (kb->modifiers & UNI_KEYBOARD_MODIFIER_LEFT_CONTROL) ? 1 : 0;
        out_joy1->button2 |= (kb->modifiers & UNI_KEYBOARD_MODIFIER_LEFT_ALT) ? 1 : 0;
        out_joy1->button3 |= (kb->modifiers & UNI_KEYBOARD_MODIFIER_LEFT_SHIFT) ? 1 : 0;
    } else {
        // Right modifiers only valid when in "twin stick" mode
        out_joy1->fire |= (kb->modifiers & UNI_KEYBOARD_MODIFIER_RIGHT_ALT) ? 1 : 0;
        out_joy1->button2 |= (kb->modifiers & UNI_KEYBOARD_MODIFIER_RIGHT_CONTROL) ? 1 : 0;
        out_joy1->button3 |= (kb->modifiers & UNI_KEYBOARD_MODIFIER_RIGHT_SHIFT) ? 1 : 0;
    }
}
void uni_joy_to_single_joy_from_keyboard(const uni_keyboard_t* kb, uni_joystick_t* out_joy) {
    to_joy_from_keyboard(kb, out_joy, NULL);
}

// Twin Stick: One keyboard controls two joysticks
void uni_joy_to_twinstick_from_keyboard(const uni_keyboard_t* kb, uni_joystick_t* out_joy1, uni_joystick_t* out_joy2) {
    to_joy_from_keyboard(kb, out_joy2, out_joy1);
}

void uni_joy_to_single_joy_from_balance_board(const uni_balance_board_t* bb,
                                              uni_balance_board_state_t* bb_state,
                                              uni_joystick_t* out_joy) {
    uni_balance_board_threshold_t bb_threshold = uni_balance_board_get_threshold();

    // Low pass filter, assuming values arrive at the same framespeed
    bb_state->smooth_down = mult_frac((bb->bl + bb->br) - bb_state->smooth_down, 6, 100);
    bb_state->smooth_top = mult_frac((bb->tl + bb->tr) - bb_state->smooth_top, 6, 100);
    bb_state->smooth_left = mult_frac((bb->tl + bb->bl) - bb_state->smooth_left, 6, 100);
    bb_state->smooth_right = mult_frac((bb->tr + bb->br) - bb_state->smooth_right, 6, 100);

    logd("l=%d, r=%d, t=%d, d=%d\n", bb_state->smooth_left, bb_state->smooth_right, bb_state->smooth_top,
         bb_state->smooth_down);

    if ((bb_state->smooth_top - bb_state->smooth_down) > bb_threshold.move)
        out_joy->up = 1;
    else if ((bb_state->smooth_down - bb_state->smooth_top) > bb_threshold.move)
        out_joy->down = 1;

    if ((bb_state->smooth_right - bb_state->smooth_left) > bb_threshold.move)
        out_joy->right = 1;
    else if ((bb_state->smooth_left - bb_state->smooth_right) > bb_threshold.move)
        out_joy->left = 1;

    // State machine to detect whether we can trigger fire
    int sum = bb->tl + bb->tr + bb->bl + bb->br;
    bb_state->fire_counter++;
    logd("SUM=%d, counter=%d, state=%d (%d,%d,%d,%d)\n", sum, bb_state->fire_counter, bb_state->fire_state, bb->tl,
         bb->tr, bb->bl, bb->br);
    switch (bb_state->fire_state) {
        case UNI_BALANCE_BOARD_STATE_RESET:
            if (sum >= bb_threshold.fire) {
                bb_state->fire_state = UNI_BALANCE_BOARD_STATE_THRESHOLD;
                bb_state->fire_counter = 0;
            }
            break;
        case UNI_BALANCE_BOARD_STATE_THRESHOLD:
            if (bb->tl < UNI_BALANCE_BOARD_IDLE_THRESHOLD && bb->tr < UNI_BALANCE_BOARD_IDLE_THRESHOLD &&
                bb->bl < UNI_BALANCE_BOARD_IDLE_THRESHOLD && bb->br < UNI_BALANCE_BOARD_IDLE_THRESHOLD) {
                bb_state->fire_state = UNI_BALANCE_BOARD_STATE_IN_AIR;
                bb_state->fire_counter = 0;
                break;
            }
            // Since threshold was triggered, it has 10 frames to get to "in_air", otherwise we reset the state.
            if (bb_state->fire_counter > 10) {
                bb_state->fire_state = UNI_BALANCE_BOARD_STATE_RESET;
                bb_state->fire_counter = 0;
                break;
            }
            // Reset counter in case we are still above threshold
            if (sum >= bb_threshold.fire) {
                bb_state->fire_counter = 0;
                break;
            }
            break;
        case UNI_BALANCE_BOARD_STATE_IN_AIR:
            // Once in Air, it must be at least 2 frames in the air
            if (bb_state->fire_counter > 2) {
                out_joy->fire = 1;
                bb_state->fire_state = UNI_BALANCE_BOARD_STATE_FIRE;
                bb_state->fire_counter = 0;
                break;
            }
            if (bb->tl >= UNI_BALANCE_BOARD_IDLE_THRESHOLD || bb->tr >= UNI_BALANCE_BOARD_IDLE_THRESHOLD ||
                bb->bl > UNI_BALANCE_BOARD_IDLE_THRESHOLD || bb->br >= UNI_BALANCE_BOARD_IDLE_THRESHOLD) {
                bb_state->fire_state = UNI_BALANCE_BOARD_STATE_RESET;
                bb_state->fire_counter = 0;
            }
            break;
        case UNI_BALANCE_BOARD_STATE_FIRE:
            out_joy->fire = 1;
            // Maintain "fire" pressed for 10 frames
            if (bb_state->fire_counter > 10) {
                bb_state->fire_state = UNI_BALANCE_BOARD_STATE_RESET;
                bb_state->fire_counter = 0;
            }
            break;
        default:
            loge("Joystick: Unexpected balance board state: %d\n", bb_state->fire_state);
            break;
    }
}
