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

#ifndef UNI_CONTROLLER_H
#define UNI_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_balance_board.h"
#include "uni_common.h"
#include "uni_gamepad.h"
#include "uni_mouse.h"

typedef enum {
    UNI_CONTROLLER_CLASS_NONE,
    UNI_CONTROLLER_CLASS_GAMEPAD,
    UNI_CONTROLLER_CLASS_MOUSE,
    UNI_CONTROLLER_CLASS_KEYBOARD,
    UNI_CONTROLLER_CLASS_BALANCE_BOARD,

    UNI_CONTROLLER_CLASS_COUNT,
} uni_controller_class_t;

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
    };
    uint8_t battery;  // 0=emtpy, 254=full, 255=battery report not available
} uni_controller_t;

void uni_controller_dump(const uni_controller_t* ctl);

#ifdef __cplusplus
}
#endif

#endif  // UNI_CONTROLLER_H
