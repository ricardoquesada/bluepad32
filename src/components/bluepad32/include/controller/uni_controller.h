// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_CONTROLLER_H
#define UNI_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "controller/uni_balance_board.h"
#include "controller/uni_gamepad.h"
#include "controller/uni_keyboard.h"
#include "controller/uni_mouse.h"
#include "uni_common.h"

typedef enum {
    // Order should not be changed.
    UNI_CONTROLLER_CLASS_NONE = 0,
    UNI_CONTROLLER_CLASS_GAMEPAD,
    UNI_CONTROLLER_CLASS_MOUSE,
    UNI_CONTROLLER_CLASS_KEYBOARD,
    UNI_CONTROLLER_CLASS_BALANCE_BOARD,

    UNI_CONTROLLER_CLASS_COUNT,
} uni_controller_class_t;

typedef enum {
    // Order should not be changed.
    // Unused entries should not be removed.
    CONTROLLER_SUBTYPE_NONE = 0,
    CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL,
    CONTROLLER_SUBTYPE_WIIMOTE_VERTICAL,
    CONTROLLER_SUBTYPE_WIIMOTE_ACCEL,
    CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK,
    CONTROLLER_SUBTYPE_UNUSED_00,
    CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK_ACCEL,
    CONTROLLER_SUBTYPE_WII_CLASSIC,
    CONTROLLER_SUBTYPE_WIIUPRO,
    CONTROLLER_SUBTYPE_WII_BALANCE_BOARD,

    // Each gamepad should have its own range, this is to have binary compatibility
    // E.g: NINA users might depend on some hardcoded values
    CONTROLLER_SUBTYPE_EXAMPLE_OF_NEW_GAMEPAD = 20,

} uni_controller_subtype_t;

enum {
    // Matches spec which says "Null values indicate unknown battery status"
    UNI_CONTROLLER_BATTERY_NOT_AVAILABLE = 0,
    UNI_CONTROLLER_BATTERY_EMPTY = 1,
    UNI_CONTROLLER_BATTERY_FULL = 255,
};

// Type that supports all kind of controllers.
// Add a new type to the union if needed.
// Common field, like "battery", should be placed outside the union.
typedef struct {
    uni_controller_class_t klass;
    union {
        uni_gamepad_t gamepad;
        uni_mouse_t mouse;
        uni_balance_board_t balance_board;
        uni_keyboard_t keyboard;
    };
    uint8_t battery;  // 0=emtpy, 254=full, 255=battery report not available
} uni_controller_t;

void uni_controller_dump(const uni_controller_t* ctl);

#ifdef __cplusplus
}
#endif

#endif  // UNI_CONTROLLER_H
